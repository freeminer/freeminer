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
#include <sstream>
#include <string>
#include <sys/wait.h>
#include <thread>
#include <vector>
#include "debug/iostream_debug_helpers.h"

#include "filesys.h"
#include "httpfetch.h"
#include "log.h"
#include "settings.h"
#include "threading/concurrent_set.h"
#include "util/timetaker.h"

hgts::hgts(const std::string &folder) : folder{folder}
{
	fs::CreateAllDirs(folder);
}

hgt *hgts::get(hgt::ll_t lat, hgt::ll_t lon)
{
	if (map[lat].contains(lon)) {
		return &map[lat][lon].value();
	}
	auto lock = std::unique_lock(mutex);
	if (map[lat].contains(lon)) {
		return &map[lat][lon].value();
	}
	// DUMP("insert", (long)this, lat, lon, folder, map.size(), map[lat].size());
	map[lat][lon].emplace(folder, lat, lon).get(lat, lon);
	return &map[lat][lon].value();
}

hgt::hgt(const std::string &folder, ll_t lat, ll_t lon) : folder{folder}
{
	const int lat_dec = (int)floor(lat);
	const int lon_dec = (int)floor(lon);
	if (load(lat_dec, lon_dec)) {
		static thread_local auto once = 0;
		if (!once--)
			DUMP("load failed", lat_dec, lon_dec);
	}
}

std::basic_string<uint8_t> exec_to_string(const std::string &cmd)
{
	std::array<uint8_t, 1000000> buffer;
	std::basic_stringstream<uint8_t> result;
	std::unique_ptr<FILE, decltype(&pclose)> pipe(popen(cmd.c_str(), "r"), pclose);
	if (!pipe) {
		throw std::runtime_error("popen() failed!");
	}
	size_t sz = 0;
	while ((sz = read(pipe.get()->_fileno, buffer.data(),
					static_cast<int>(buffer.size()))) > 0) {
		result << std::basic_string<uint8_t>{buffer.data(), sz};
	}
	return result.str();
}

bool hgt::load(int lat_dec, int lon_dec)
{
	//DUMP(lat_dec, lon_dec);
	if (lat_loaded == lat_dec && lon_loaded == lon_dec) {
		return false;
	}
	auto lock = std::unique_lock(mutex);
	//DUMP(lat_dec, lon_dec);
	if (lat_loaded == lat_dec && lon_loaded == lon_dec) {
		return false;
	}
	if (lat_loading == lat_dec && lon_loading == lon_dec) {
		//DUMP(lat_dec, lon_dec);
		return true;
	}
	// DUMP((long)this, lat_dec, lon_dec, lat_loading, lon_loading, lat_loaded, lon_loaded);
	TimeTaker timer("hgt load");

	lat_loading = lat_dec;
	lon_loading = lon_dec;

	const auto gen_zip_name = [](int lat_dec, int lon_dec) {
		std::string zipname;
		if (lat_dec < 0) {
			zipname += 'S';
			zipname += char('A' + abs(ceil(lat_dec / 90.0 * 23)));
		} else {
			zipname += char('A' + abs(floor(lat_dec / 90.0 * 23)));
		}
		zipname += std::to_string(int(floor((((lon_dec + 180) / 360.0)) * 60) + 1));
		return zipname;
	};

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

	const auto zipname = gen_zip_name(lat_dec, lon_dec);

	std::string zipfile = zipname + ".zip";
	std::string zipfull = folder + "/" + zipfile;

	std::string filename(255, 0);
	sprintf(filename.data(), "%c%02d%c%03d.hgt", lat_dec > 0 ? 'N' : 'S', abs(lat_dec),
			lon_dec > 0 ? 'E' : 'W', abs(lon_dec));
	std::string filefull = folder + "/" + filename;
	// DUMP(lat_dec, lon_dec, filename, zipname, zipfull);

	std::basic_string<uint8_t> srtmTile;
	size_t filesize = 0;

	auto set_ratio = [&]() {
		if (filesize == 2884802) {
			seconds_per_px = 3;
		} else if (filesize == 25934402) {
			seconds_per_px = 1;
		} else {
			throw std::logic_error("unknown file size " + std::to_string(filesize));
		}

		side_length = sqrt(filesize >> 1);
	};

	// DUMP(filefull, zipfull);
	if (!std::filesystem::exists(filefull) && !std::filesystem::exists(zipfull)) {
		const auto http_to_file = [](const std::string &url, const std::string &zipfull) {
			HTTPFetchRequest req;
			req.url = url;
			req.connect_timeout = req.timeout =
					g_settings->getS32("curl_file_download_timeout");
			actionstream << "Downloading map from " << req.url << "\n";

			HTTPFetchResult res;

			if (1) {
				// TODO: why sync does not work?
				req.caller = HTTPFETCH_SYNC;
			httpfetch_sync(req, res);
			} else {
				req.caller = httpfetch_caller_alloc();
				httpfetch_async(req);
				std::this_thread::sleep_for(std::chrono::milliseconds(50));
				HTTPFetchResult res;
				while (!httpfetch_async_get(req.caller, res)) {
					std::this_thread::sleep_for(std::chrono::milliseconds(100));
				}
				httpfetch_caller_free(req.caller);
			}

			actionstream << req.url << " " << res.succeeded << " " << res.response_code
						 << " " << res.data.size() << "\n";
			if (!res.succeeded)
				return uintmax_t{0};

			if (!res.data.size())
				return uintmax_t{0};

			std::ofstream(zipfull) << res.data;
			if (!std::filesystem::exists(zipfull))
				return uintmax_t{0};
			return std::filesystem::file_size(zipfull);
		};

		// TODO: https://viewfinderpanoramas.org/Coverage%20map%20viewfinderpanoramas_org15.htm
		static concurrent_set<std::string> http_failed;
		if (!http_failed.contains(zipfile))
			if (!http_to_file("http://build.freeminer.org/earth/" + zipfile, zipfull))
				if (!http_to_file(
							"https://viewfinderpanoramas.org/dem1/" + zipfile, zipfull))
					if (!http_to_file("https://viewfinderpanoramas.org/dem3/" + zipfile,
								zipfull)) {
				errorstream << "Not found " << zipfile << "\n"
							<< "try to download manually: \n"
							<< "curl -o " << zipfull
							<< " https://viewfinderpanoramas.org/dem1/" << zipfile
							<< " || " << "curl -o " << zipfull
							<< " https://viewfinderpanoramas.org/dem3/" << zipfile
							<< "\n";
				http_failed.insert(zipfile);
			}
	}

	if (!std::filesystem::exists(filefull) && std::filesystem::exists(zipfull)) {

		// TODO: use some available in server and client zip lib to extract files

		//auto fs = RenderingEngine::get_raw_device()->getFileSystem();
		//bool ok = fs::extractZipFile(fs, zipfile, destination);

		std::string cmd{"unzip -C -b -p " + zipfull + " " + zipname + "/" + filename};

		srtmTile = exec_to_string(cmd);
		filesize = srtmTile.size();
		if (filesize) {
			set_ratio();
		}
	}
	// TODO: first try load unpached file, then unpack zip
	if (srtmTile.empty()) {

		if (!std::filesystem::exists(filename)) {
			static thread_local auto once = 0;
			if (!once++) {
				std::cerr << "Missing file " << filename << " for " << lat_dec << ","
						  << lon_dec << std::endl;
			}
			return true;
		}

		filesize = std::filesystem::file_size(filename);

		set_ratio();

		std::ifstream istrm(filename, std::ios::binary);

		if (!istrm.good()) {
			std::cerr << "Error opening " << filename << std::endl;
			return true;
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
		return true;
	}

	heights.resize(filesize >> 1);
	for (uint32_t i = 0; i < filesize >> 1; ++i) {
		int16_t height = (srtmTile[i << 1] << 8) | (srtmTile[(i << 1) + 1]);
		if (height == -32768) {
			height = 0;
		}
		heights[i] = height;
	}
	lat_loaded = lat_dec;
	lon_loaded = lon_dec;
	// DUMP("loadok", (long)this, heights.size(), lat_loaded, lon_loaded, filesize, zipname, filename, seconds_per_px, get(lat_dec, lon_dec));
	return false;
}

/** Pixel idx from left bottom corner (0-1200) */
int16_t hgt::read(int16_t y, int16_t x)
{
	const int row = (side_length - 1) - y;
	const int col = x;
	const int pos = (row * side_length + col);
	return heights[pos];
}

float hgt::get(ll_t lat, ll_t lon)
{
	const int lat_dec = (int)floor(lat);
	const int lon_dec = (int)floor(lon);

	if (lat_loaded != lat_dec || lon_loaded != lon_dec) {
		// DUMP("notloaded", lat, lon, lat_dec, lon_dec, lat_loaded, lon_loaded, lat_loading, lon_loading, heights.size());
		return {};
	}

	const ll_t lat_seconds = (lat - (ll_t)lat_dec) * 60 * 60;
	const ll_t lon_seconds = (lon - (ll_t)lon_dec) * 60 * 60;

	const int y = lat_seconds / seconds_per_px;
	const int x = lon_seconds / seconds_per_px;

	const int16_t height[] = {
			read(y + 1, x),
			read(y + 1, x + 1),
			read(y, x),
			read(y, x + 1),
	};
	// return height[2]; // debug not interpolated

	// ratio where X lays
	const float dy = fmod(lat_seconds, seconds_per_px) / seconds_per_px;
	const float dx = fmod(lon_seconds, seconds_per_px) / seconds_per_px;

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
