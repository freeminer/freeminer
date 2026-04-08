/*
Copyright (C) 2024 proller <proler@gmail.com>
*/

/*
This file is part of Freeminer.

Freeminer is free software: you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation, either version 3 of the License, or
(at your option) any later version.

Freeminer  is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with Freeminer.  If not, see <http://www.gnu.org/licenses/>.
*/

// Some code got from https://github.com/zbycz/srtm-hgt-reader

#include "hgt.h"

#include <cmath>
#include <cstddef>
#include <cstdint>
#include <fstream>
#include <ios>
#include <filesystem>
#include <iostream>
#include <math.h>
#include <memory>
#include <optional>
#include <sstream>
#include <string>
#include <unistd.h>
#include <utility>
#include <vector>
#include "debug/dump.h"
#include "http.h"
#include "serialization.h"

#if USE_TIFF
#include "tiffio.h"
#endif

#define HGT_DEBUG 0

// bad anything but works
// todo: prepare all data from all sources in one good tiled layer

// Thread-local container cache definition
thread_local hgts::ThreadLocalContainerCache hgts::tl_container_cache;

hgts::hgts(const std::string &folder) : folder{folder}
{
	std::error_code ec;
	std::filesystem::create_directories(folder, ec);
}

// Get layer definitions
std::vector<hgts::Layer> hgts::get_layers(const height::ll_t lat, const height::ll_t lon) {
	constexpr auto layers = true;
	
	auto place_dummy_generic = [](std::map<int, std::map<int, std::shared_ptr<height>>>& map,
								 std::unordered_map<uint64_t, std::weak_ptr<height>>& cache,
								 int lat_dec, int lon_dec) {
		const static auto hgt_dummy = std::make_shared<height_dummy>();
		if (!map[lat_dec].contains(lon_dec)) {
			map[lat_dec][lon_dec] = hgt_dummy;
			cache[make_tile_key(lat_dec, lon_dec)] = hgt_dummy;
		}
	};
	
	auto identity_post_process = [](height::ll_t result) { return result; };
	
	return {
		Layer{
			.container = map1,
			.cache = tl_container_cache.map1_cache,
			.factory = [](const std::string& folder, height::ll_t lat, height::ll_t lon) {
				return std::make_shared<height_hgt>(folder, lat, lon);
			},
			.post_process = identity_post_process,
			.place_dummy = place_dummy_generic,
			.min_height = 0.0f,   // Primary layer can handle very low elevations
			.max_height = 10000.0f,   // Primary layer handles high elevations
		},
		Layer{
			.container = map1_seabed,
			.cache = tl_container_cache.map1_seabed_cache,
			.factory = [](const std::string& folder, height::ll_t lat, height::ll_t lon) {
				return std::make_shared<height_seabed_tif>(folder, lat, lon);
			},
			.post_process = identity_post_process,
			.place_dummy = place_dummy_generic,
			.min_height = -12000.0f,  // Seabed handles deep ocean depths
			.max_height = 0.0f,       // Seabed only for underwater/sea level
		}
	};
}

height::height_t hgts::get(const height_hgt::ll_t lat, const height_hgt::ll_t lon)
{
	const auto lat1 = height::lat_start(lat);
	const auto lon1 = height::lon_start(lon);
	
	// First try thread-local container cache
	const uint64_t tile_key = make_tile_key(lat1, lon1);
	
	// Get all layers
	auto layers = get_layers(lat, lon);
	
	// Try each layer in cycle
	for (auto& layer : layers) {
		// Check thread-local cache first
		auto it = layer.cache.find(tile_key);
		if (auto cached_height = it != layer.cache.end() ? it->second.lock() : std::shared_ptr<height>{};
		    cached_height && cached_height->ok(lat, lon)) {
			const auto result = cached_height->get(lat, lon);
			const auto processed_result = layer.post_process(result);
			
			// Check if result is within layer's valid range
			if (processed_result > layer.min_height && processed_result < layer.max_height) {
				return processed_result;
			}
			// If not in range, continue to next layer
		}
	}

	// Try each layer with lock
	for (auto& layer : layers) {
		{
			const auto lock = std::unique_lock(mutex);
			if (const auto it = layer.container.find(lat1); it != layer.container.end()) {
				if (const auto inner_it = it->second.find(lon1); inner_it != it->second.end()) {
					const auto result = inner_it->second->get(lat, lon);
					const auto processed_result = layer.post_process(result);
					
					// Check if result is within layer's valid range
					if (processed_result > layer.min_height && processed_result < layer.max_height) {
						// Cache the height object for future fast access
						layer.cache[tile_key] = inner_it->second;
						return processed_result;
					}
					// If not in range, continue to next layer
				}
			}
		} // Lock released
		
		// Load data outside the lock if needed and within valid range
		if (lat <= 90 && lat >= -90 && lon <= 180 && lon >= -180) {
			const auto& container_lat1 = layer.container[lat1];
			
			if (!container_lat1.contains(lon1)) {
				auto hgt = layer.factory(folder, lat, lon);
				const int lat_dec = hgt->lat_start(lat);
				const int lon_dec = hgt->lon_start(lon);
				if (hgt->load(lat, lon)) {
					// Successfully loaded, now acquire lock to store it
					{
						const auto lock = std::unique_lock(mutex);
						layer.container[lat_dec][lon_dec] = std::move(hgt);
					} // Lock released
					
					// Now get the result with a brief lock
					const auto lock = std::unique_lock(mutex);
					if (const auto it = layer.container.find(lat_dec); it != layer.container.end()) {
						if (const auto inner_it = it->second.find(lon_dec); inner_it != it->second.end()) {
							const auto result = inner_it->second->get(lat, lon);
							const auto processed_result = layer.post_process(result);
							
							// Check if result is within layer's valid range
							if (processed_result > layer.min_height && processed_result < layer.max_height) {
								layer.cache[make_tile_key(lat_dec, lon_dec)] = layer.container[lat_dec][lon_dec];
								return processed_result;
							}
						}
					}
				} else {
					// Failed to load, place dummy
					const auto lock = std::unique_lock(mutex);
					layer.place_dummy(layer.container, layer.cache, lat1, lon1);
				}
			}
			
			// Final check with lock held
			const auto lock = std::unique_lock(mutex);
			if (const auto it = layer.container.find(lat1); it != layer.container.end()) {
				if (const auto inner_it = it->second.find(lon1); inner_it != it->second.end()) {
					const auto result = inner_it->second->get(lat, lon);
					const auto processed_result = layer.post_process(result);
					
					// Check if result is within layer's valid range
					if (processed_result > layer.min_height && processed_result < layer.max_height) {
						// Cache the height object for future fast access
						layer.cache[tile_key] = inner_it->second;
						return processed_result;
					}
				}
			}
		}
	}
	
	// If all layers failed or results were out of range, return 0 as fallback
	return 0;
}

std::mutex height::mutex;

height_hgt::height_hgt(const std::string &folder, ll_t lat, ll_t lon) : folder{folder}
{
	side_length_x_extra = 1;
	tile_deg_x = 1;
	tile_deg_y = 1;

	std::error_code ec;
	std::filesystem::create_directories(folder, ec);
}

height_tif::height_tif(const std::string &folder, ll_t lat, ll_t lon) : folder{folder}
{

	side_length_x_extra = 1;
	tile_deg_x = 60;
	tile_deg_y = 45;

	std::error_code ec;
	std::filesystem::create_directories(folder, ec);
}

bool height::ok(ll_t lat, ll_t lon)
{
	const auto ok = (lat_loaded == lat_start(lat) && lon_loaded == lon_start(lon));

#if HGT_DEBUG
	if (!ok) {
		static int rare = 0;
		if (!(rare++ % 10000))
			DUMP("notloaded", lat, lon,
					//lat_dec, lon_dec,
					lat_loaded, lon_loaded, lat_start(lat), lon_start(lon),
					heights.size(), file_name(lat, lon));
	}
#endif

	return ok;
}

int height::lat_start(ll_t lat_dec)
{
	return floor(lat_dec);
}

int height::lon_start(ll_t lon_dec)
{
	return floor(lon_dec);
}

const auto gen_zip_name = [](int lat_dec, int lon_dec) {
	std::string zipname;
	if (lat_dec < 0) {
		zipname += 'S';
		zipname += char('A' + abs(ceil(lat_dec / 90.0 * 23)));
	} else {
		zipname += char('A' + abs(round(lat_dec / 90.0 * 21)));
	}
	zipname += std::to_string(int(floor((((lon_dec + 180) / 360.0)) * 60) + 1));
	return zipname;
};

bool height_hgt::load(ll_t lat, ll_t lon)
{
	auto lat_dec = lat_start(lat);
	auto lon_dec = lon_start(lon);

	//DUMP(lat_dec, lon_dec);
	if (ok(lat_dec, lon_dec)) {
		return true;
	}
	const auto lock = std::unique_lock(mutex);
	//DUMP(lat_dec, lon_dec);
	if (ok(lat_dec, lon_dec)) {
		return true;
	}
	if (lat_loading == lat_dec && lon_loading == lon_dec) {
		//DUMP(lat_dec, lon_dec);
		return false;
	}
	//DUMP((long long)this, lat_dec, lon_dec, lat_loading, lon_loading, lat_loaded, lon_loaded);
	//TimeTaker timer("hgt load");

	lat_loading = lat_dec;
	lon_loading = lon_dec;

//#define GEN_TEST 1
#if GEN_TEST
	{
		size_t fails = 0;
		for (const auto &t : {
					 std::pair{std::pair{36, 30}, "J36"},
					 std::pair{std::pair{39, 35}, "J36"},
					 std::pair{std::pair{36, 24}, "J35"},
					 std::pair{std::pair{39, 29}, "J35"},
					 std::pair{std::pair{40, 18}, "K34"},
					 std::pair{std::pair{80, 25}, "U26"},
					 std::pair{std::pair{83, 30}, "U26"},
					 std::pair{std::pair{68, 163}, "R03"}, // TODO!!!
					 std::pair{std::pair{70, 164}, "R03"},
					 std::pair{std::pair{47, 5}, "L31"},
					 std::pair{std::pair{43, 5}, "K31"},
					 std::pair{std::pair{44, 5}, "L31"},
			 }) {
			std::string r;
			if (r = gen_zip_name(t.first.first, t.first.second); t.second != r) {
				DUMP("fail", t, r);
				++fails;
			}
		}
		DUMP(fails);
	}
#endif

	auto zipname = gen_zip_name(lat_dec, lon_dec);

	std::string zipfile = zipname + ".zip";
	std::string zipfull = folder + "/" + zipfile;

	char buff[100];
	std::snprintf(buff, sizeof(buff), "%c%02d%c%03d.hgt", lat_dec >= 0 ? 'N' : 'S',
			abs(lat_dec), lon_dec >= 0 ? 'E' : 'W', abs(lon_dec));
	std::string filename = buff;

	std::string filefull = folder + "/" + filename;
	// DUMP(lat_dec, lon_dec, filename, zipname, zipfull);

	std::string srtmTile;
	size_t filesize = 0;

	auto set_ratio = [&, this](const auto filesize) {
		side_length_x = side_length_y = sqrt(filesize >> 1);

		pixel_per_deg_x = (ll_t)side_length_x / tile_deg_x;
		pixel_per_deg_y = (ll_t)side_length_y / tile_deg_y;

		seconds_per_px_x =
				tile_deg_x * 3600 / (float)(side_length_x - side_length_x_extra);
		seconds_per_px_y = ceil(tile_deg_y * 3600 / (float)(side_length_y));
		//DUMP(tile_deg_y * 3600 / (float)(side_length_y));
		//DUMP("sides", side_length_x, side_length_y, seconds_per_px_x, seconds_per_px_y);
	};

	// zst fastest
	if (srtmTile.empty()) {
		char buff[100];
		std::snprintf(
				buff, sizeof(buff), "%c%02d", lat_dec >= 0 ? 'N' : 'S', abs(lat_dec));
		std::string zipname = buff;

		const auto zstfile = zipname + "/" + filename + ".zst";
		std::string ffolder = folder + "/" + zipname;
		std::string zstdfull = folder + "/" + zstfile;
		std::error_code ec;
		std::filesystem::create_directories(folder, ec);

		multi_http_to_file(zstfile,
				{
#if defined(__EMSCRIPTEN__)
						"/"
#else
						"http://cdn.freeminer.org/"
#endif
						"earth/" +
								zstfile,
				},
				zstdfull);
		if (std::filesystem::exists(zstdfull) && std::filesystem::file_size(zstdfull)) {

			// FIXME: zero copy possible in c++26 or with custom rdbuf
			std::ifstream is(zstdfull, std::ios_base::binary);
			std::ostringstream os(std::ios_base::binary);

			decompressZstd(is, os);
			srtmTile = os.str();
			filesize = srtmTile.size();
			if (filesize) {
				set_ratio(filesize);
			}
		}
	}

	// bz2 has best compression
	/* use zstd
	if (srtmTile.empty()) {
		const auto bzipfile = zipname + ".tar.bz2";
		std::string bzipfull = folder + "/" + bzipfile;
		multi_http_to_file(bzipfile,
				{
						"http://cdn.freeminer.org/earth/" + bzipfile,
				},
				bzipfull);
		if (std::filesystem::exists(bzipfull) && std::filesystem::file_size(bzipfull)) {
			std::string cmd{"tar -jOxvf " + bzipfull + " " + zipname + "/" + filename};
			actionstream << "Unpack: " << cmd << "\n";
			srtmTile = exec_to_string(cmd);
			filesize = srtmTile.size();
			if (filesize) {
				set_ratio(filesize);
			}
		}
	}
*/
#if 0
//#if !defined(_WIN32) && !defined(__ANDROID__) && !defined(__EMSCRIPTEN__)
	if (srtmTile.empty() && !std::filesystem::exists(filefull)) {

		// TODO: https://viewfinderpanoramas.org/Coverage%20map%20viewfinderpanoramas_org15.htm

		multi_http_to_file(zipfile,
				{"http://cdn.freeminer.org/earth/" + zipfile,
						"http://viewfinderpanoramas.org/dem1/" + zipfile,
						"http://viewfinderpanoramas.org/dem3/" + zipfile},
				zipfull);
	}

	if (srtmTile.empty() && !std::filesystem::exists(filefull) &&
			std::filesystem::exists(zipfull) && std::filesystem::file_size(zipfull)) {

		// TODO: use some available in server and client zip lib to extract files

		//auto fs = RenderingEngine::get_raw_device()->getFileSystem();
		//bool ok = fs::extractZipFile(fs, zipfile, destination);

		std::string cmd{"unzip -C -b -p " + zipfull + " " + zipname + "/" + filename};
		actionstream << "Unpack: " << cmd << "\n";

		srtmTile = exec_to_string(cmd);
		filesize = srtmTile.size();
		if (filesize) {
			set_ratio(filesize);
		}
	}
#endif

	// TODO: first try load unpached file, then unpack zip
	if (srtmTile.empty()) {
		if (!std::filesystem::exists(filename)) {
			static thread_local auto once = 0;
			if (!once++) {
				std::cerr << "Missing file " << filename << " for " << lat_dec << ","
						  << lon_dec << std::endl;
			}
			return false;
		}

		filesize = std::filesystem::file_size(filename);

		set_ratio(filesize);

		std::ifstream istrm(filename, std::ios::binary);

		if (!istrm.good()) {
			std::cerr << "Error opening " << filename << std::endl;
			return false;
		}

		srtmTile.resize(filesize);
		istrm.read(reinterpret_cast<char *>(srtmTile.data()), filesize);
	}

	if (srtmTile.empty()) {
		static thread_local auto once = 0;
		if (!once++) {
			std::cerr << "Missing file " << filename << " " << zipname << " for "
					  << lat_dec << "," << lon_dec << std::endl;
		}
		return false;
	}

	heights.resize(filesize >> 1);
	for (uint32_t i = 0; i < filesize >> 1; ++i) {
		int16_t height =
				((uint8_t)srtmTile[i << 1] << 8) | ((uint8_t)srtmTile[(i << 1) + 1]);
		if (height == -32768) {
			height = 0;
		}
		heights[i] = height;
	}
	lat_loaded = lat_dec;
	lon_loaded = lon_dec;
	//DUMP("loadok", (long long)this, heights.size(), lat_loaded, lon_loaded, filesize, zipname, filename, seconds_per_px_x, get(lat_dec, lon_dec), heights[0], heights.back(), heights[side_length_x]);
	return true;
}

const auto gen_zip_name_15 = [](int lat_dec, int lon_dec) {
	int h = 0;
	if (lat_dec < 0) {
		h += 90 + abs(lat_dec);
	} else {
		h += 90 - lat_dec;
	}
	h = floor(h / 180.0 * 4);
	int w = floor((lon_dec + 180) / 360.0 * 6);
	char c = 'A' + h * 6 + w;
	return std::string{"15-"} + c;
};

bool height_tif::load(ll_t lat, ll_t lon)
{
	auto lat_dec = lat_start(lat);
	auto lon_dec = lon_start(lon);

#if USE_TIFF

	// COMPLETELY WRONG!!!

	//DUMP(lat_dec, lon_dec);
	if (ok(lat, lon)) {
		return true;
	}
	const auto lock = std::unique_lock(mutex);

	if (lat >= 90 || lat <= -90 || lon >= 180 || lon <= -180)
		return false;

	//DUMP(lat_dec, lon_dec);
	if (ok(lat, lon)) {
		return true;
	}

	if (lat_loading == lat_dec && lon_loading == lon_dec) {
		//DUMP(lat_dec, lon_dec);
		return false;
	}
	//TimeTaker timer("hgt load");

	lat_loading = lat_dec;
	lon_loading = lon_dec;

	//if (srtmTile.empty())
	{
		const auto zipname = gen_zip_name_15(lat_dec, lon_dec);
		//zipname = "15-J";
		//zipname = "15-O";
		const auto zipfile = zipname + ".zip";
		const auto zipfull = folder + "/" + zipname;
		const auto tifname = folder + "/" + zipname + ".tif";
		if (!std::filesystem::exists(tifname)) {
			multi_http_to_file(zipfile,
					{"http://cdn.freeminer.org/earth/" + zipfile,
							"http://www.viewfinderpanoramas.org/DEM/TIF15/" + zipfile},
					zipfull);
		}

		if (!std::filesystem::exists(tifname) && std::filesystem::exists(zipfull) &&
				std::filesystem::file_size(zipfull)) {
			const auto cmd = "unzip " + zipfull + " -d " + folder;
			exec_to_string(cmd); // TODO just exec
		}

		if (std::filesystem::exists(tifname)) {
			if (auto tif = TIFFOpen(tifname.c_str(), "r"); tif) {
				uint32_t w = 0, h = 0;

				TIFFGetField(tif, TIFFTAG_IMAGEWIDTH, &w);
				TIFFGetField(tif, TIFFTAG_IMAGELENGTH, &h);
				size_t npixels = w * h;
				if (!npixels)
					return false;

				if (auto raster = (uint32_t *)_TIFFmalloc(npixels * sizeof(uint32_t));
						raster) {
					if (TIFFReadRGBAImage(tif, w, h, raster, 0)) {
						/*
						DUMP(raster[0]);
						{
							uint8_t *bytes = (uint8_t *)&raster[0];
							DUMP(bytes[0], bytes[2], bytes[3], bytes[4]);
						}
						*/
						heights.resize(npixels);

						for (size_t i = 0; i < npixels; ++i) {
							uint8_t *bytes = (uint8_t *)&raster[i];

							const auto gray =
									floor(((bytes[0] * 0.299) + (bytes[1] * 0.587) +
											(bytes[3] * 0.144) + 0.5)) -
									37; // Wrong?
							//if (!(i % 100000)) DUMP(i, raster[i], bytes[0], bytes[2], bytes[3], bytes[4],gray);
							heights[i] = gray;
						}

						lat_loaded = lat_dec;
						lon_loaded = lon_dec;
					} else {
						//DUMP("read fail");
					}
					_TIFFfree(raster);
				} else {
					//DUMP("malloc fail");
				}
				TIFFClose(tif);

				//DUMP(w, h, npixels);

				if (lat_loaded == lat_dec && lon_loaded == lon_dec) {
					seconds_per_px_y = seconds_per_px_x = 15;
					side_length_x = w;
					side_length_y = h;
					//DUMP("tif ok", seconds_per_px_x, side_length_x, side_length_y);

					lat_loaded = lat_dec;
					lon_loaded = lon_dec;

					pixel_per_deg_x = (ll_t)side_length_x / tile_deg_x;
					pixel_per_deg_y = (ll_t)side_length_y / tile_deg_y;

					//DUMP("loadok", (long long)this, heights.size(), lat_loaded, lon_loaded, zipname, tifname, seconds_per_px_x, get(lat_dec, lon_dec));
					//DUMP("ppd", pixel_per_deg_x, pixel_per_deg_y);

					return true;
				}
			}
		}
	}

#endif

	//lat_loaded = lat_dec;
	//lon_loaded = lon_dec;
	// DUMP("loadok", (long)this, heights.size(), lat_loaded, lon_loaded, filesize, zipname, filename, seconds_per_px, get(lat_dec, lon_dec));
	return false;
}

#if 0
height_gebco_tif::height_gebco_tif(const std::string &folder, ll_t lat, ll_t lon) :
		folder{folder}
{
	tile_deg_x = 90;
	tile_deg_y = 90;
}

bool height_gebco_tif::ok(ll_t lat, ll_t lon)
{

	const auto ok = (lat_loaded == lat90_start(lat) && lon_loaded == lon90_start(lon));

#if HGT_DEBUG
	if (!ok) {
		static int rare = 0;
		if (!(rare++ % 10000))
			DUMP("notloaded", lat, lon,
					//lat_dec, lon_dec,
					lat_loaded, lon_loaded, lat90_start(lat), lon90_start(lon),
					heights.size(), file_name(lat, lon));
	}
#endif

	return ok;
}

int height_gebco_tif::lat90_start(ll_t lat_dec)
{
	return floor(lat_dec / 90.0 + 1) * 90;
}
int height_gebco_tif::lon90_start(ll_t lon_dec)
{
	return floor(lon_dec / 90.0) * 90;
}

std::string height_gebco_tif::file_name(ll_t lat, ll_t lon)
{

	/*
	      0
	1 | 2 | 3 | 4
0 ----|---|---|----
    5 | 6 | 7 | 8

gebco_2023_sub_ice_n90.0_s0.0_w-180.0_e-90.0.tif  1 north america
gebco_2023_sub_ice_n90.0_s0.0_w-90.0_e0.0.tif     2 atlantica 
gebco_2023_sub_ice_n90.0_s0.0_w0.0_e90.0.tif   -  3 europe    lat = 46.4085; float lon = 11.8393 
gebco_2023_sub_ice_n90.0_s0.0_w90.0_e180.0.tif    4 eurasia
gebco_2023_sub_ice_n0.0_s-90.0_w-180.0_e-90.0.tif 5 ocean
gebco_2023_sub_ice_n0.0_s-90.0_w-90.0_e0.0.tif    6 south america
gebco_2023_sub_ice_n0.0_s-90.0_w0.0_e90.0.tif     7 south africa
gebco_2023_sub_ice_n0.0_s-90.0_w90.0_e180.0.tif   8 australia
*/
	std::string name; // = "gebco_2023_sub_ice_";
	name += 'n';
	const auto h_start = abs(lat90_start(lat));
	name += std::to_string(h_start);
	name += ".0_s";
	const auto h_end = h_start - 90;
	name += std::to_string(h_end);
	name += ".0_w";
	const auto w_start = lon90_start(lon); // (lon_dec / 90) * 90;
	name += std::to_string(w_start);
	name += ".0_e";
	const auto w_end = w_start + 90;
	name += std::to_string(w_end);
	name += ".0";
	// DUMP(lat, lon, name, h_start, h_end, w_start, w_end);
	return name;
}

bool height_gebco_tif::load(ll_t lat, ll_t lon)
{
	const auto lat_dec = lat90_start(lat);
	const auto lon_dec = lon90_start(lon);
#if TEST
	static int once = 0;
	if (!once++)
		for (const auto &t : std::vector<std::tuple<int, int, std::string>>{
					 {0, 0, "n90.0_s0.0_w0.0_e90.0"}, {1, 1, "n90.0_s0.0_w0.0_e90.0"},
					 {-1, 1, "n0.0_s-90.0_w0.0_e90.0"}, {1, -1, "n90.0_s0.0_w-90.0_e0.0"},
					 {-1, -1, "n0.0_s-90.0_w-90.0_e0.0"},
					 {1, 100, "n90.0_s0.0_w90.0_e180.0"},
					 {-1, -100, "n0.0_s-90.0_w-180.0_e-90.0"}, {-300, 0, ""},
					 {0, 300, ""}}) {
			const auto fn = folder + "/" + "gebco_2023_sub_ice_" +
							file_name(get<0>(t), get<1>(t)) + ".tif";
			DUMP("testname", get<0>(t), get<1>(t), file_name(get<0>(t), get<1>(t)),
					get<2>(t), std::filesystem::exists(fn));
			if (!get<2>(t).empty() && get<2>(t) != file_name(get<0>(t), get<1>(t))) {
				DUMP("testfail");
				//exit(1);
			}
		}
#endif

#if USE_TIFF

	//DUMP(lat_dec, lon_dec);
	if (ok(lat, lon)) {
		return true;
	}
	const auto lock = std::unique_lock(mutex);
	//DUMP(lat_dec, lon_dec);
	if (ok(lat, lon)) {
		return true;
	}
	if (lat_loading == lat_dec && lon_loading == lon_dec) {
		//DUMP(lat_dec, lon_dec);
		return false;
	}
	//DUMP("loadstart", (long long)this, lat, lon, lat_dec, lon_dec, lat_loading, lon_loading, lat_loaded, lon_loaded, floor(lat / 90.0 + 1) * 90);
	//TimeTaker timer("tiff load");

	lat_loading = lat_dec;
	lon_loading = lon_dec;

	{
		const auto name = file_name(lat, lon);
		auto tifname = folder + "/" + "gebco_2023_sub_ice_" + name + ".tif";
		//DUMP(name, tifname);
		if (0) // too big zips
		{
			std::string zipfile = "gebco_2023_sub_ice_topo_geotiff.zip";
			std::string zipfull = folder + "/" + zipfile;
			if (!std::filesystem::exists(tifname)) {
				if (multi_http_to_file(zipfile,
							{"https://www.bodc.ac.uk/data/open_download/gebco/gebco_2023_sub_ice_topo/geotiff/"},
							zipfull)) {
					exec_to_string(
							"unzip " + zipfull + " -d " + folder); // TODO just exec
				}
			}

			if (!std::filesystem::exists(tifname) && !std::filesystem::exists(zipfull)) {
				zipfile = "gebco_2023_geotiff.zip";
				zipfull = folder + "/" + zipfile;
				tifname = folder + "/" + "gebco_2023_" + name + ".tif";
				if (multi_http_to_file(zipfile,
							{"https://www.bodc.ac.uk/data/open_download/gebco/gebco_2023_tid/geotiff/"},
							zipfull)) {
					exec_to_string(
							"unzip " + zipfull + " -d " + folder); // TODO just exec
				}
			}
		} else {
			std::cerr
					<< "Want " << tifname << " from "
					<< "https://www.bodc.ac.uk/data/open_download/gebco/gebco_2023_sub_ice_topo/geotiff/"
					<< " or "
					<< "https://www.bodc.ac.uk/data/open_download/gebco/gebco_2023_tid/geotiff/"
					<< " in " << porting::path_cache + "/earth" << "\n";
		}

		//DUMP(tifname, std::filesystem::exists(tifname));

		if (!std::filesystem::exists(tifname)) {
			tifname = folder + "/" + "gebco_2023_" + name + ".tif";
		}
		//DUMP("loadtifname", tifname, std::filesystem::exists(tifname));
		if (std::filesystem::exists(tifname)) {
			DUMP("open tif", tifname, std::filesystem::file_size(tifname));
			if (const auto tif = TIFFOpen(tifname.c_str(), "r"); tif) {
				uint32_t w = 0;
				uint32_t h = 0;
				TIFFGetField(tif, TIFFTAG_IMAGEWIDTH, &w);
				TIFFGetField(tif, TIFFTAG_IMAGELENGTH, &h);
				if (!w || !h) {
					return false;
				}

				const size_t npixels = (w + 1) * (h + 1);
				//DUMP("tiff size", w, h, npixels);
				heights.resize(npixels);

				//w = TIFFScanlineSize(tif) >> 1;
				const tdata_t buf = _TIFFmalloc(TIFFScanlineSize(tif));
				for (uint32_t row = 0; row < h; ++row) {
					TIFFReadScanline(tif, buf, row, 0);

#if HGT_DEBUG
					if (!(row % 10000))
						DUMP(row, TIFFNumberOfStrips(tif), TIFFStripSize(tif),
								((uint8_t *)buf)[0], ((uint8_t *)buf)[2],
								((uint8_t *)buf)[3], ((uint8_t *)buf)[4]);
#endif

					int16_t height;
					for (uint32_t i = 0; i < w; ++i) {
						height = (((uint8_t *)buf)[i << 1]) |
								 (((uint8_t *)buf)[(i << 1) + 1] << 8);
						if (height == -32768 || height == 31727) {
							height = 0;
						}
						const auto dest = i + row * (w + 1);
#if HGT_DEBUG
						if (!(i % 10000) && !(row % 10000))
							DUMP("fill", i, w, h, row, dest, height, //height2,
									((uint8_t *)buf)[(i << 1)],
									((uint8_t *)buf)[(i << 1) + 1],
									((uint8_t *)buf)[(i << 1) + 2],
									((uint8_t *)buf)[(i << 1) + 3]);
#endif

						heights[dest] = height;
					}
					const auto dest = w + row * (w + 1);
					heights[dest] = height; // hack for  interpolation x+1 get
											//DUMP("xhck", w, dest, height);
				}

				// hack for interpolation y+1 get
				for (uint32_t i = 0; i <= w; ++i) {
					const auto src = i + (h - 1) * (w + 1);
					const auto dest = i + h * (w + 1);
					//DUMP("yhck", i, src, dest, heights[src]);
					heights[dest] = heights[src];
				}

				_TIFFfree(buf);
				TIFFClose(tif);

				lat_loaded = lat_dec;
				lon_loaded = lon_dec;

				side_length_x = w;
				side_length_y = h;

				seconds_per_px_x =
						tile_deg_x * 3600 / ((float)side_length_x - side_length_x_extra);
				seconds_per_px_y = tile_deg_y * 3600 / ((float)side_length_y);

				pixel_per_deg_x = (ll_t)side_length_x / tile_deg_x;
				pixel_per_deg_y = (ll_t)side_length_y / tile_deg_y;

#if HGT_DEBUG
				DUMP("tif ok", seconds_per_px_x, side_length_x, side_length_y,
						seconds_per_px_x, seconds_per_px_y);
				DUMP("loadok", (long)this, heights.size(), lat_loaded, lon_loaded,
						tifname, seconds_per_px_x, get(lat_dec, lon_dec));
				DUMP("testread", read(0, 0), read(0, side_length_x - 1),
						read(side_length_y - 1, side_length_x - 1),
						read(side_length_y - 1, 0));
#endif

				return true;
			}
		}
	}

#endif

	// DUMP("load not ok", (long long)this, heights.size(), lat_loaded, lon_loaded, seconds_per_px_x, get(lat_dec, lon_dec));
	return false;
}

std::tuple<size_t, size_t, height::ll_t, height::ll_t> height_gebco_tif::ll_to_xy(
		ll_t lat, ll_t lon)
{
	const height::ll_t lat_seconds = (lat_loaded - lat) * 60 * 60;
	const height::ll_t lon_seconds = (lon - lon_loaded) * 60 * 60;

	const size_t y = ((lat_loaded - lat) * pixel_per_deg_y) - 1;
	const size_t x = (lon - lon_loaded) * pixel_per_deg_x;

	return {x, y, lat_seconds, lon_seconds};
}

int16_t height_gebco_tif::read(uint16_t y, uint16_t x)
{
	const int pos = (((int)side_length_x - side_length_x_extra + 1) * y) + x;

#if HGT_DEBGUG
	static int rare = 0;
	if (!(rare++ % 1000000))
		DUMP("read", x, y,
				// row, col,
				pos, side_length_x, side_length_x_extra, side_length_y, heights[pos]);
#endif

	return heights[pos];
}
#endif

std::tuple<size_t, size_t, height::ll_t, height::ll_t> height_hgt::ll_to_xy(
		height::ll_t lat, height::ll_t lon)
{

	const height::ll_t lat_seconds = (lat - (ll_t)lat_loaded) * 60 * 60;
	const height::ll_t lon_seconds = (lon - (ll_t)lon_loaded) * 60 * 60;
	const int y = lat_seconds / seconds_per_px_y;
	const int x = lon_seconds / seconds_per_px_x;
	return {x, y, lat_seconds, lon_seconds};
}

std::tuple<size_t, size_t, height::ll_t, height::ll_t> height_tif::ll_to_xy(
		height::ll_t lat, height::ll_t lon)
{
	const ll_t lat_seconds = (lat - (ll_t)lat_loaded) * 60 * 60;
	const ll_t lon_seconds = (lon - (ll_t)lon_loaded) * 60 * 60;
	const size_t y = ((lat - lat_loaded) * pixel_per_deg_y) - 1;
	const size_t x = (lon - lon_loaded) * pixel_per_deg_x;

	return {x, y, lat_seconds, lon_seconds};
}

/** Pixel idx from left bottom corner (0-1200) */
int16_t height_hgt::read(uint16_t y, uint16_t x)
{
	const int row = (side_length_x - 1) - y;
	const int col = x;
	const int pos = (row * side_length_y + col);
	return heights[pos];
}

height::height_t height::get(ll_t lat, ll_t lon)
{
	if (!ok(lat, lon)) {
		/*static int rare = 0;
		if (!(rare++ % 10000))
			DUMP("notloaded", lat, lon,
					//lat_dec, lon_dec,
					lat_loaded, lon_loaded, lat_start(lat), lon_start(lon),
					heights.size(), file_name(lat, lon));*/
		return {};
	}

	const auto [x, y, lat_seconds, lon_seconds] = ll_to_xy(lat, lon);

#if HGT_DEBUG
	const auto h = read(y, x);
	{
		static int rare = 0;
		if (h > 9000 || h < -10000 || !(rare++ % 1000000))
			DUMP("tst", h, x, y,
					//lat_seconds, lon_seconds,
					seconds_per_px_x, seconds_per_px_y, lat, lon, lat_loaded, lon_loaded,
					(lat - lat_loaded), pixel_per_deg_x, (lon - lon_loaded),
					pixel_per_deg_y);
	}
#endif
	const int16_t height[] = {
			read(y + 1, x),
			read(y + 1, x + 1),
			read(y, x),
			read(y, x + 1),
	};

#if HGT_DEBUG
	static int rare = 0;
	if (height[2] > 9000 || height[2] < -10000 || !(rare++ % 100000))
		DUMP("tst", height[2], x, y,
				//lat_seconds, lon_seconds,
				seconds_per_px_x, seconds_per_px_y, lat, lon, lat_loaded, lon_loaded,
				(lat - lat_loaded), pixel_per_deg_x, (lon - lon_loaded), pixel_per_deg_y);
		//return height[2]; // debug not interpolated
#endif

	// ratio where X lays
	const float dy = fmod(lat_seconds, seconds_per_px_x) / seconds_per_px_x;
	const float dx = fmod(lon_seconds, seconds_per_px_y) / seconds_per_px_y;

	// Bilinear interpolation
	// h0------------h1
	// |
	// |--dx-- .
	// |       |
	// |      dy
	// |       |
	// h2------------h3
	return height[0] * dy * (1 - dx) + height[1] * dy * (dx) +
		   height[2] * (1 - dy) * (1 - dx) + height[3] * (1 - dy) * dx;
}

// height_sb_tif implementation

height_seabed_tif::height_seabed_tif(
		const std::string &folder, height::ll_t lat, height::ll_t lon) : folder{folder}
{
	side_length_x_extra = 1;
	tile_deg_x = 1;
	tile_deg_y = 1;
}

std::string height_seabed_tif::file_name(height::ll_t lat, height::ll_t lon)
{
	int lat_dec = height::lat_start(lat);
	int lon_dec = height::lon_start(lon);
	char buff[100];
	std::snprintf(buff, sizeof(buff), "%c%02d%c%03d.sb.tif", lat_dec >= 0 ? 'N' : 'S',
			std::abs(lat_dec), lon_dec >= 0 ? 'E' : 'W', std::abs(lon_dec));
	return std::string(buff);
}

bool height_seabed_tif::ok(height::ll_t lat, height::ll_t lon)
{
	return (lat_loaded == height::lat_start(lat) && lon_loaded == height::lon_start(lon));
}

std::tuple<size_t, size_t, height::ll_t, height::ll_t> height_seabed_tif::ll_to_xy(
		height::ll_t lat, height::ll_t lon)
{
	const height::ll_t lat_seconds = (lat - (height::ll_t)lat_loaded) * 60 * 60;
	const height::ll_t lon_seconds = (lon - (height::ll_t)lon_loaded) * 60 * 60;
	const int y = lat_seconds / seconds_per_px_y;
	const int x = lon_seconds / seconds_per_px_x;
	return {x, y, lat_seconds, lon_seconds};
}

int16_t height_seabed_tif::read(uint16_t y, uint16_t x)
{
	const int row = (side_length_y)-y;
	const int pos = x + row * (side_length_x + 1);
	const auto ret = heights[pos];
	return ret;
}

bool height_seabed_tif::load(height::ll_t lat, height::ll_t lon)
{
	auto lat_dec = height::lat_start(lat);
	auto lon_dec = height::lon_start(lon);

	if (ok(lat, lon))
		return true;
	const auto lock = std::unique_lock(mutex);
	if (ok(lat, lon))
		return true;
	if (lat_loading == lat_dec && lon_loading == lon_dec) {
		return false;
	}

	lat_loading = lat_dec;
	lon_loading = lon_dec;

	std::string filename = file_name(lat, lon);
	char lat_dir_buff[10];
	std::snprintf(lat_dir_buff, sizeof(lat_dir_buff), "%c%02d", lat_dec >= 0 ? 'N' : 'S',
			std::abs(lat_dec));
	std::string lat_dir = lat_dir_buff;
	std::string fullpath = folder + "/" + lat_dir + "/" + filename;

	if (!std::filesystem::exists(fullpath)) {
		std::error_code ec;
		std::filesystem::create_directories(folder + "/" + lat_dir, ec);

		std::string url =
				std::string("http://cdn.freeminer.org/earth/") + lat_dir + "/" + filename;
		multi_http_to_file(filename, {url}, fullpath);
	}
	if (!std::filesystem::exists(fullpath)) {
		return false;
	}

	if (const auto tif = TIFFOpen(fullpath.c_str(), "r"); tif) {
		uint32_t w = 0, h = 0;
		TIFFGetField(tif, TIFFTAG_IMAGEWIDTH, &w);
		TIFFGetField(tif, TIFFTAG_IMAGELENGTH, &h);
		if (!w || !h) {
			TIFFClose(tif);
			return false;
		}

		const size_t npixels = (w + 1) * (h + 1);
		heights.resize(npixels);

		const tdata_t buf = _TIFFmalloc(TIFFScanlineSize(tif));
		for (uint32_t row = 0; row < h; ++row) {
			TIFFReadScanline(tif, buf, row, 0);
			uint32_t i = 0;
			int16_t height = 0;
			uint32_t dest = 0;
			for (; i < w; ++i) {
				height = (((uint8_t *)buf)[i << 1]) |
						 (((uint8_t *)buf)[(i << 1) + 1] << 8);
				dest = i + row * (w + 1);
				if (height <= -32767 || height >= 31727)
					height = 0;
				heights[dest] = height;
			}
			heights[++dest] = height; // fill last col for interpolation
		}

		// fill last row for interpolation
		for (uint32_t i = 0; i <= w; ++i) {
			const auto src = i + (h - 1) * (w + 1);
			const auto dest = i + h * (w + 1);
			heights[dest] = heights[src];
		}

		_TIFFfree(buf);
		TIFFClose(tif);

		lat_loaded = lat_dec;
		lon_loaded = lon_dec;
		side_length_x = w;
		side_length_y = h;

		seconds_per_px_x =
				tile_deg_x * 3600 / ((float)side_length_x - side_length_x_extra);
		seconds_per_px_y = tile_deg_y * 3600 / ((float)side_length_y);
		pixel_per_deg_x = (height::ll_t)side_length_x / tile_deg_x;
		pixel_per_deg_y = (height::ll_t)side_length_y / tile_deg_y;

#if HGT_DEBUG
		DUMP("tif ok", seconds_per_px_x, side_length_x, side_length_y, seconds_per_px_x,
				seconds_per_px_y);
		DUMP("loadok", (long)this, heights.size(), lat_loaded, lon_loaded, fullpath,
				seconds_per_px_x, get(lat_dec, lon_dec));
		DUMP("testread", read(0, 0), read(0, side_length_x - 1),
				read(side_length_y - 1, side_length_x - 1), read(side_length_y - 1, 0));
#endif

		return true;
	}
	return false;
}
