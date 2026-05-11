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

#include <cmath>
#include <cstdint>
#include <cstdlib>
#include <exception>
#include <memory>
#include <mutex>
#include <sstream>
#include <string>
#include <system_error>
#include <tuple>
#include <filesystem>

#include "content/mods.h"
#include "constants.h"
#include "emerge.h"
#include "mapgen/earth/png_holder.h"
//#include "mapgen/earth/rgb_temp.h"
#include "mapgen/mg_decoration.h"
#include "mapgen/mg_ore.h"
#include "server.h"
#include "filesys.h"
#include "irr_v2d.h"
#include "irr_v3d.h"
#include "irrlichttypes.h"
#include "log_types.h"
#include "log.h"
#include "map.h"
#include "mapblock.h"
#include "mapgen_earth.h"
#include "mapgen/earth/hgt.h"
#include "mapgen/earth/http.h"
#include "mapnode.h"
#include "mg_biome.h"
#include "nodedef.h"
#include "serverenvironment.h"
#include "servermap.h"
#include "settings.h"
#include "voxel.h"
#include "voxelalgorithms.h"
#if USE_OSMIUM
#include "earth/osmium-inl.h"
#include <osmium/area/assembler.hpp>
#include <osmium/area/multipolygon_manager.hpp>
#include <osmium/dynamic_handler.hpp>
#include <osmium/geom/tile.hpp>
#include <osmium/handler/node_locations_for_ways.hpp>
#include <osmium/index/map/sparse_mem_array.hpp>
#include <osmium/io/file.hpp>
#include <osmium/io/pbf_input.hpp>
#include <osmium/osm/entity_bits.hpp>
#include <osmium/osm/node.hpp>
#include <osmium/osm/way.hpp>
#include <osmium/tags/tags_filter.hpp>
#if USE_OSMIUM_TOOL
#include "mapgen/earth/osmium-tool/src/command_extract.hpp"
#endif
#endif
#if USE_VOXEL_EARTH
#include "mapgen/earth/luanti-earth/native/src/downloader.h"
#include "mapgen/earth/luanti-earth/native/src/voxelizer.h"
#endif

#undef stoi

#if USE_VOXEL_EARTH
#include "mapgen/earth/voxel_importer.cpp"
#endif

std::unique_ptr<maps_holder_t> MapgenEarth::maps_holder;

void MapgenEarthParams::setDefaultSettings(Settings *settings)
{
	settings->setDefault("mgearth_spflags", flagdesc_mapgen_v7, 0);
}

void MapgenEarthParams::readParams(const Settings *settings)
{
	try {
		MapgenV7Params::readParams(settings);
	} catch (...) {
	}
	auto mg_earth = settings->getJson("mg_earth");
	if (!mg_earth.isNull())
		params = mg_earth;
}

void MapgenEarthParams::writeParams(Settings *settings) const
{
	settings->setJson("mg_earth", params);
	try {
		MapgenV7Params::writeParams(settings);
	} catch (...) {
	}
}

///////////////////////////////////////////////////////////////////////////////
/*
// Utility function for converting degrees to radians
long double toRadians(const long double degree)
{
	// cmath library in C++ defines the constant M_PI as the value of pi accurate to 1e-30
	long double one_deg = (M_PI) / 180;
	return (one_deg * degree);
}

long double distance(
		long double lat1, long double long1, long double lat2, long double long2)
{
	DUMP(lat1, long1, lat2, long2);
	// Convert the latitudes and longitudes from degree to radians.
	lat1 = toRadians(lat1);
	long1 = toRadians(long1);
	lat2 = toRadians(lat2);
	long2 = toRadians(long2);

	// Haversine Formula
	long double dlong = long2 - long1;
	long double dlat = lat2 - lat1;

	long double ans =
			pow(sin(dlat / 2), 2) + cos(lat1) * cos(lat2) * pow(sin(dlong / 2), 2);

	ans = 2 * asin(sqrt(ans));

	// Radius of Earth in Kilometers, R = 6371 Use R = 3956 for miles long double R =
	// 6371;
	constexpr double R{6378137.0};

	// Calculate the result
	ans = ans * R;
	return ans;
}
*/

MapgenEarth::MapgenEarth(MapgenEarthParams *params_, EmergeParams *emerge) :
		MapgenV7((MapgenV7Params *)params_, emerge)
{
	ndef = emerge->ndef;
#if USE_VOXEL_EARTH
	std::vector<std::string> texture_dirs;
	std::string pure_colors_path;
	fs::GetRecursiveDirs(texture_dirs, g_settings->get("texture_path"));
	fs::GetRecursiveDirs(texture_dirs,
			porting::path_user + DIR_DELIM + "textures" + DIR_DELIM + "server");
	if (emerge->server) {
		if (const SubgameSpec *gamespec = emerge->server->getGameSpec())
			fs::GetRecursiveDirs(texture_dirs, gamespec->path + DIR_DELIM + "textures");
		if (const ModSpec *mod = emerge->server->getModSpec("luanti_earth"))
			pure_colors_path = mod->path + DIR_DELIM + "colors.lua";
		const auto &mods = emerge->server->getMods();
		for (auto it = mods.crbegin(); it != mods.crend(); ++it) {
			fs::GetRecursiveDirs(texture_dirs, it->path + DIR_DELIM + "textures");
			fs::GetRecursiveDirs(texture_dirs, it->path + DIR_DELIM + "media");
		}
	}
	voxel_importer::init_block_palette(ndef, texture_dirs, pure_colors_path);
#endif
	// mg_params = (MapgenEarthParams *)params_->sparams;
	mg_params = params_;

	const Json::Value &params = mg_params->params;
	flags = 0;

	if (params.get("light", 0).asBool())
		this->flags &= ~MG_LIGHT;

	n_air = MapNode(ndef->getId(params.get("air", "air").asString()), LIGHT_SUN);
	n_water = MapNode(
			ndef->getId(params.get("water_source", "mapgen_water_source").asString()),
			LIGHT_SUN);
	n_stone = MapNode(
			ndef->getId(params.get("stone", "mapgen_stone").asString()), LIGHT_SUN);

	if (params.get("center", Json::Value()).isObject())
		center = {params["center"]["x"].asDouble(), params["center"]["y"].asDouble(),
				params["center"]["z"].asDouble()};

	if (params.get("scale", Json::Value()).isObject())
		scale = {params["scale"]["x"].asDouble(), params["scale"]["y"].asDouble(),
				params["scale"]["z"].asDouble()};

	/* todomake test
	static bool shown = 0;
	if (!shown) {
		shown = true;
		std::vector<std::pair<int, int>> a{{0, 0}, {0, 1000}, {1000, 0}, {0, 30000},
				{30000, 0}, {-30000, -30000}, {-30000, 30000}, {30000, -30000},
				{30000, 30000}};

		auto p0 = pos_to_ll(0, 0);
		for (const auto &c : a) {

			// auto p1 = posToLl(30000, 0);
			// auto p2 = posToLl(0, 30000);
			// auto p3 = posToLl(0, 0);
			auto p = pos_to_ll(c.first, c.second);
			auto dist1 = distance(p0.lat, p0.lon, p.lat, p.lon);
			// auto dist2 = distance(p0.lat, p0.lat, p.lat, p.lon);
			auto h = get_height(c.first, c.second);
			DUMP(c.first, c.second, p0, p, dist1, h);
		}
	}
	*/
	/*
	hgt_reader.debug = 1;
	std::vector<std::pair<int, int>> a{
			{0, 0}, {-30000, -30000}, {-30000, 30000}, {30000, -30000}, {30000, 30000}};

	for (const auto &c : a) {

		const auto tc = posToLl(c.first, c.second);
		// DUMP(x, z, tc.lat, tc.lon, center);
		// auto y = hgt_reader.srtmGetElevation(tc.lat, tc.lon);
		// if (!(x % 100) && !(z % 100)) {
		// auto url = std::string{"https://www.google.com/maps/@"} +
		// std::to_string(tc.lat) +"," + std::to_string(tc.lon) + ",11z";
		auto url = std::string{"https://yandex.ru/maps/?ll="} + std::to_string(tc.lon) +
				   "%2C" + std::to_string(tc.lat) + "&z=11";
		auto h = getHeight(c.first, c.second);

		DUMP(h, c.first, c.second, tc.lat, tc.lon, scale, center, url);
	}
	hgt_reader.debug = 0;
*/

	if (!maps_holder) {
		maps_holder = std::make_unique<maps_holder_t>();
	}

	{
		const auto heat_img = maps_holder->data_root + "/earth_heat.png";
		if (!std::filesystem::exists(heat_img)) {
			const auto lock = std::lock_guard(maps_holder->download_lock);
			if (!std::filesystem::exists(heat_img)) {
				multi_http_to_file_cdn("earth", "earth_heat.png", {});
			}
		}

		if (std::filesystem::exists(heat_img) && std::filesystem::file_size(heat_img)) {
			const auto lock = std::lock_guard(maps_holder->download_lock);
			if (!maps_holder->heat_image) {
				maps_holder->heat_image = std::make_unique<PngImage>(heat_img);
			}
		}
	}
}

MapgenEarth::~MapgenEarth()
{
}

//////////////////////// Map generator

MapNode MapgenEarth::layers_get(float value, float max)
{
	const auto layer_index =
			rangelim((unsigned int)myround((value / max) * layers_node_size), 0,
					layers_node_size - 1);
	return layers_node[layer_index];
}

bool MapgenEarth::visible(const v3pos_t &p)
{
	return p.Y < get_height(p.X, p.Z);
}

const MapNode &MapgenEarth::visible_content(const v3pos_t &p, bool use_weather)
{
	const auto v = visible(p);
	const auto vw = visible_water_level(p);
	if (!v && !vw) {
		return visible_transparent;
	}
	if (!use_weather) {
		return visible_surface_green;
	}
	auto heat = 10;
	heat += p.Y / -100; // upper=colder, lower=hotter, 3c per 1000

	if (!v && p.Y < water_level)
		return heat < 0 ? visible_ice : visible_water;
	const auto humidity = 60;
	return heat < 0	   ? (humidity < 20 ? visible_surface : visible_surface_cold)
		   : heat < 10 ? visible_surface
		   : heat < 40 ? (humidity < 20 ? visible_surface_dry : visible_surface_green)
					   : visible_surface_hot;
}

//constexpr double EARTH_RADIUS = 6378137.0;
//constexpr double EQUATOR_LEN = EARTH_RADIUS * 3.14159265358979323846 * 2;

constexpr double EQUATOR_LEN{40075696.0};
ll MapgenEarth::pos_to_ll(const pos_t x, const pos_t z)
{
	const auto lon = ((ll_t)x * scale.X) / (EQUATOR_LEN / 360.0) + center.X;
	const auto lat = ((ll_t)z * scale.Z) / (EQUATOR_LEN / 360.0) + center.Z;
	if (lat < 90 && lat > -90 && lon < 180 && lon > -180) {
		return {(ll_t)lat, (ll_t)lon};
	} else {
		return {89.9999, 0};
	}
}
ll MapgenEarth::pos_to_ll(const v3pos_t &p)
{
	return pos_to_ll(p.X, p.Z);
}

v2pos_t MapgenEarth::ll_to_pos(const ll &l)
{
	return v2pos_t((l.lon - center.X) * (EQUATOR_LEN / 360) / scale.X,
			(l.lat - center.Z) * (EQUATOR_LEN / 360) / scale.Z);
}

pos_t MapgenEarth::get_height(pos_t x, pos_t z)
{
	const auto tc = pos_to_ll(x, z);
	const auto y = maps_holder->hgt_reader.get(tc.lat, tc.lon);
	return ceil(y / scale.Y) - center.Y;
}

pos_t MapgenEarth::getSpawnLevelAtPoint(v2pos_t p)
{
	return std::max(2, get_height(p.X, p.Y) + 2);
}

pos_t MapgenEarth::getGroundLevelAtPoint(v2pos_t p)
{
	return get_height(p.X, p.Y); // + MGV6_AVERAGE_MUD_AMOUNT;
}

int MapgenEarth::generateTerrain()
{
	const MapNode n_ice(c_ice);
	u32 index = 0;
	const auto em = vm->m_area.getExtent();

	for (pos_t z = node_min.Z; z <= node_max.Z; z++) {
		for (pos_t x = node_min.X; x <= node_max.X; x++, index++) {
			const auto heat =
					m_emerge->env->m_use_weather
							? m_emerge->env->getServerMap().updateBlockHeat(m_emerge->env,
									  v3pos_t(x, node_max.Y, z), nullptr, &heat_cache)
							: 0;
			const auto height = get_height(x, z);
			u32 i = vm->m_area.index(x, node_min.Y, z);
			for (pos_t y = node_min.Y; y <= node_max.Y; y++) {
				bool underground = height >= y;
				if (underground) {
					if (!vm->m_data[i]) {
						vm->m_data[i] = layers_get(0, 1);
					}
				} else if (y <= water_level) {
					vm->m_data[i] = (heat < 0 && y > heat / 3) ? n_ice : n_water;
				} else {
					vm->m_data[i] = n_air;
				}
				vm->m_area.add_y(em, i, 1);
			}
		}
	}
	return 0;
}

int lat_start(ll_t lat_dec)
{
	return floor(lat_dec);
}

int lon_start(ll_t lon_dec)
{
	return floor(lon_dec);
}

int long2tilex(double lon, int z)
{
	return (int)(floor((lon + 180.0) / 360.0 * (1 << z)));
}

int lat2tiley(double lat, int z)
{
	double latrad = lat * M_PI / 180.0;
	return (int)(floor((1.0 - asinh(tan(latrad)) / M_PI) / 2.0 * (1 << z)));
}

double tilex2long(int x, int z)
{
	return x / (double)(1 << z) * 360.0 - 180;
}

double tiley2lat(int y, int z)
{
	double n = M_PI - 2.0 * M_PI * y / (double)(1 << z);
	return 180.0 / M_PI * atan(0.5 * (exp(n) - exp(-n)));
}

const auto floor01 = [](const auto &v, const float &div) { return floor(v * div) / div; };

//const auto ceil01 = [](const auto &v, const float &div) { return ceil(v * div) / div; };

auto bbox_to_string(const auto &start, const auto &end)
{
	std::stringstream bboxs;
	bboxs << start.lon << "," << start.lat << "," << end.lon << "," << end.lat;
	return bboxs.str();
}

auto make_bbox(const auto &tc, auto div)
{
	const ll start{floor01(tc.lat, div), floor01(tc.lon, div)};
	const ll end{floor01(tc.lat + (1.0 / div), div), floor01(tc.lon + (1.0 / div), div)};
	const auto bbox = bbox_to_string(start, end);
	return std::make_tuple(bbox, start, end);
}

#if USE_VOXEL_EARTH
using Vec3 = v3d;

static Vec3 cartesianFromDegrees(double lat, double lon, double h = 0)
{
	const double a = 6378137.0;
	const double f = 1.0 / 298.257223563;
	const double e2 = f * (2.0 - f);
	const double radLat = lat * 3.14159265358979323846 / 180.0;
	const double radLon = lon * 3.14159265358979323846 / 180.0;
	const double sinLat = std::sin(radLat);
	const double cosLat = std::cos(radLat);
	const double N = a / std::sqrt(1.0 - e2 * sinLat * sinLat);
	const double x = (N + h) * cosLat * std::cos(radLon);
	const double y = (N + h) * cosLat * std::sin(radLon);
	const double z = (N * (1.0 - e2) + h) * sinLat;
	return {x, y, z};
}

#include "earth/geoid.h"
#include "earth/CpuVoxelizer.cpp"
#endif

void MapgenEarth::start_download_and_voxelize(double lat, double lon, double elevation,
		double radius, int resolution, const std::string &api_key)
{
#if USE_VOXEL_EARTH
	try {
		const std::string &apiKeyStr = api_key;
		const pos_t terrain_y =
				std::max(get_height(node_min.X + csize.X / 2, node_min.Z + csize.Z / 2),
						static_cast<pos_t>(water_level)) +
				1;

		TileDownloader downloader(
				apiKeyStr, maps_holder->data_root + DIR_DELIM + "voxel_earth");
		const double horizontal_radius =
				radius * std::max(std::abs(scale.X), std::abs(scale.Z));
		double vertical_padding = std::max<double>(csize.Y * std::abs(scale.Y), 64.0);
		g_settings->getFloatNoEx("voxel_earth_query_vertical_padding", vertical_padding);
		double query_radius = std::sqrt(horizontal_radius * horizontal_radius +
										vertical_padding * vertical_padding);
		g_settings->getFloatNoEx("voxel_earth_query_radius", query_radius);
		const double terrain_elevation = (terrain_y + center.Y) * scale.Y;
		const double terrain_ellipsoid_elevation =
				earth::orthometric_to_ellipsoid_height(lat, lon, terrain_elevation);
		auto tiles = downloader.downloadTiles(
				lat, lon, terrain_ellipsoid_elevation, query_radius);
		bool use_java_voxelizer = false;
		g_settings->getBoolNoEx("use_java_voxelizer", use_java_voxelizer);
		bool use_native_voxelizer = !use_java_voxelizer;
		g_settings->getBoolNoEx("use_native_voxelizer", use_native_voxelizer);

		Voxelizer voxelizer;
		CpuVoxelizer cpuvoxelizer{csize.X * 2, true, true};
		const double chunk_elevation = (elevation + center.Y) * scale.Y;
		const double chunk_ellipsoid_elevation =
				earth::orthometric_to_ellipsoid_height(lat, lon, chunk_elevation);
		Vec3 origin = cartesianFromDegrees(lat, lon, chunk_ellipsoid_elevation);
		double y_offset_nodes = 0.0;
		g_settings->getFloatNoEx("voxel_earth_y_offset", y_offset_nodes);
		// DUMP(node_min, node_max, lat, lon, origin.X, origin.Y, origin.Z, tiles.size());
		const auto mg = this;

		int set = 0, miss = 0;
		for (const auto &tile : tiles) {
			if (use_java_voxelizer) {
				const auto callback = [&, this](const int &x, const int &y, const int &z,
											  const uint8_t &r, const uint8_t &g,
											  const uint8_t &b, const uint8_t &a) {
					const v3pos_t pos_rel{static_cast<pos_t>(x), static_cast<pos_t>(y),
							//+ csize.Y/2
							static_cast<pos_t>(z)};
					const auto pos = node_min + pos_rel;

					if (mg->vm->exists(pos)) {
						const auto block_name = voxel_importer::rgb_to_block(r, g, b);
						const auto id = ndef->getId(block_name);
						MapNode node{id, LIGHT_SUN};
						mg->vm->setNode(pos, node);
						++set;
					} else {
						++miss;
					}
				};
				const auto stats = cpuvoxelizer.voxelizeSingleGLB(tile, callback);
			}

			if (use_native_voxelizer) {
				VoxelGrid grid = voxelizer.voxelize(tile, resolution, origin.X, origin.Y,
						origin.Z, y_offset_nodes, center.X, center.Y, center.Z, scale.X,
						scale.Y, scale.Z, node_min.X, node_min.Y, node_min.Z);
				int auto_y_anchor = 1;
				g_settings->getS32NoEx("voxel_earth_auto_y_anchor", auto_y_anchor);
				pos_t y_anchor_shift = 0;
				if (auto_y_anchor && !grid.voxels.empty()) {
					pos_t min_voxel_y = std::numeric_limits<pos_t>::max();
					for (const auto &v : grid.voxels) {
						if (v.x < 0 || v.x >= csize.X || v.z < 0 || v.z >= csize.Z)
							continue;
						min_voxel_y = std::min<pos_t>(min_voxel_y, v.y);
					}
					if (min_voxel_y != std::numeric_limits<pos_t>::max()) {
						const pos_t target_min_y = terrain_y - node_min.Y;
						y_anchor_shift = target_min_y - min_voxel_y;
					}
				}
				for (const auto &v : grid.voxels) {
					const pos_t shifted_y = static_cast<pos_t>(v.y + y_anchor_shift);
					const v3pos_t pos_rel{static_cast<pos_t>(v.x), shifted_y,
							//+ csize.Y/2
							static_cast<pos_t>(v.z)};
					const auto pos = node_min + pos_rel;
					if (mg->vm->exists(pos)) {
						const auto block_name =
								voxel_importer::rgb_to_block(v.r, v.g, v.b);
						const auto id = ndef->getId(block_name);
						MapNode node{id, LIGHT_SUN};
						mg->vm->setNode(pos, node);
						++set;
					} else {
						++miss;
					}
				}
			}
		}
	} catch (const std::exception &e) {
		errorstream << "Voxel earth exception: " << e.what() << "\n";
	}
#endif
}

void MapgenEarth::generateBuildings()
{
#if USE_OSMIUM
	TimeTaker timer("earth buildings", {}, PRECISION_MILLI);
	std::string use_file;
	try {

		//#define FILE_INCLUDED 1
		//#include "earth/osmium-inl.h"
		constexpr auto extra = MAP_BLOCKSIZE * 2;
		const auto coord_min = pos_to_ll(node_min.X - extra, node_min.Z - extra);
		const auto coord_max = pos_to_ll(node_max.X + extra, node_max.Z + extra);
		static const auto folder = maps_holder->data_root;
		const auto lat_dec = lat_start(coord_min.lat);
		const auto lon_dec = lon_start(coord_min.lon);

		static const auto timestamp = []() {
			std::string ts = "latest";
			g_settings->getNoEx("earth_movisda_timestamp", ts);
			return ts;
		}();
		char buff[100];
		std::snprintf(buff, sizeof(buff), "%c%02d%c%03d-%s.osm.pbf",
				lat_dec >= 0 ? 'N' : 'S', abs(lat_dec), lon_dec > 0 ? 'W' : 'E',
				abs(lon_dec), timestamp.c_str());
		const std::string filename = buff;
		const auto base_full_name = folder + DIR_DELIM + filename;
		if (!std::filesystem::exists(base_full_name)) {
			const auto lock = std::lock_guard(maps_holder->osm_http_lock);
			if (!std::filesystem::exists(base_full_name)) {
				const auto url = "https://osm.download.movisda.io/grid/" + filename;
				multi_http_to_file_cdn("movisda_grid", filename, {url});
			}
		}

		use_file = base_full_name;
		std::string bbox;
		{
			const auto try_extract = [](const auto &path_name, const auto &bbox,
											 const auto &filename) {
				if (std::filesystem::exists(filename)) {
					return true;
				}
				const auto lock = std::lock_guard(maps_holder->osm_extract_lock);
				if (std::filesystem::exists(filename)) {
					return true;
				}

#if USE_OSMIUM_TOOL
				{
					verbosestream << "Extracting " << bbox << "\n";
					CommandExtract extract{{}};
					const std::vector<std::string> arguments{"--output-format", "pbf",
							"--strategy", "smart", "--option", "types=any", "--bbox",
							bbox, "--output", filename + ".tmp", path_name};
					extract.setup(arguments);
					extract.run();
				}
#else
				{
					std::stringstream cmd;
					cmd << "osmium extract --output-format pbf --strategy smart --option types=any "
						<< "--bbox " << bbox << " --output " << filename << ".tmp" << " "
						<< path_name;
					exec_to_string(cmd.str());
				}
#endif

				if (!std::filesystem::exists(filename + ".tmp")) {
					return false;
				}

				std::error_code error_code;
				std::filesystem::rename(filename + ".tmp", filename, error_code);
				return !error_code.value();
			};

			for (auto div = 10; div <= 10000; div *= 10) {
				std::error_code ec;
				// const auto size = std::filesystem::file_size(use_file, ec);
				if (ec) {
					break;
				};

				auto [bbox_next, bb_start, bb_end] = make_bbox(coord_min, div);
				const auto bbox_to_filename = [](const auto &bbox_next, const auto div) {
					auto filename_next = folder + DIR_DELIM + "extract." +
										 std::to_string(div) + "." + bbox_next +
										 ".osm.pbf";
					return filename_next;
				};
				auto filename_next = bbox_to_filename(bbox_next, div);

				if (!(bb_start.lat <= coord_min.lat && bb_start.lon <= coord_min.lon &&
							bb_end.lat >= coord_max.lat && bb_end.lon >= coord_max.lon)) {

					const auto bbox_exact = bbox_to_string(coord_min, coord_max);
					const auto filename_exact = bbox_to_filename(bbox_exact, 100000);
					filename_next = filename_exact;
					bbox_next = bbox_exact;
				}

				if (!try_extract(use_file, bbox_next, filename_next)) {
					break;
				}

				if (div >= 1000) {
					maps_holder->files_to_delete.emplace_back(filename_next);
				}

				use_file = filename_next;
				bbox = bbox_next;
			}
		}

		if (std::filesystem::exists(use_file) && std::filesystem::file_size(use_file)) {
			const auto lock = std::lock_guard{maps_holder->osm_bbox_lock};
			if (!maps_holder->osm_bbox.contains(bbox)) {
				const auto osm = std::make_shared<hdl>(this, use_file);
				//const auto lock = maps_holder->osm_bbox.lock_unique_rec();
				if (!maps_holder->osm_bbox.contains(bbox)) {
					maps_holder->osm_bbox.emplace(bbox, osm);
				}
			}
		}

		{
			auto lock = std::unique_lock{maps_holder->osm_bbox_lock};

			if (const auto &hdlr = maps_holder->osm_bbox.get(bbox)) {
				lock.unlock();
				hdlr.value()->apply(this);
			}
		}
	} catch (const std::exception &ex) {
		warningstream << node_min << " : " << ex.what() << " file=" << use_file << "\n";
	}

	verbosestream << "Buildings stat: " << node_min << " .. " << node_max << " "
				  << " set=" << stat.set << " miss=" << stat.miss
				  << " level=" << stat.level << " check=" << stat.check
				  << " fill=" << stat.fill << " per=" << timer.getTimerTime()
				  << " mh=" << maps_holder->osm_bbox.size() << "\n";
	stat.clean();
#endif
}

/*
Heat data:
https://search.earthdata.nasa.gov/search/granules?p=C1276812859-GES_DISC&pg[0][v]=f&pg[0][gsk]=-start_date&g=G3503129918-GES_DISC&ff=Map%20Imagery&tl=1038183927.842!5!!&fsm0=Clouds&fst0=Atmosphere&long=2.3696682464454994&zoom=2.8910241901494977
https://gibs-a.earthdata.nasa.gov/wmts/epsg4326/best/wmts.cgi?TIME=2025-03-01&layer=MERRA2_2m_Air_Temperature_Monthly&style=default&tilematrixset=2km&Service=WMTS&Request=GetTile&Version=1.0.0&Format=image%2Fpng&TileMatrix=0&TileCol=0&TileRow=0

Snow data:
https://cmr.earthdata.nasa.gov/search/concepts/C3050353608-NSIDC_CPRD.html
https://gibs-a.earthdata.nasa.gov/wmts/epsg4326/best/wmts.cgi?TIME=2025-03-01&layer=MODIS_Terra_L3_Snow_Cover_Monthly_Average_Pct&style=default&tilematrixset=2km&Service=WMTS&Request=GetTile&Version=1.0.0&Format=image%2Fpng&TileMatrix=0&TileCol=0&TileRow=0
*/

weather::heat_t MapgenEarth::calcBlockHeat(const v3pos_t &p, uint64_t seed,
		float timeofday, float totaltime, bool use_weather)
{
#if USE_OSMIUM
	const auto ll = pos_to_ll(p);
	if (maps_holder->heat_image) {
		const auto x = maps_holder->heat_image->width() *
					   ((int(45 + ll.lon + 180) % 360) / 360.0);
		const auto y = (std::min<int>(maps_holder->heat_image->height(),
							   maps_holder->heat_image->width() / 1.6)) *
					   ((90 - ll.lat) / 180);
		const auto pixel = maps_holder->heat_image->get_pixel(x, y);
		auto heat = rgbToCelsiusJet(pixel->getRed(), pixel->getGreen(), pixel->getBlue());
		heat += m_emerge->biomemgr->weather_heat_daily *
				(sin(cycle_shift(timeofday, -0.25) * M_PI) - 0.5); //-64..0..34
		heat += p.Y / m_emerge->biomemgr->weather_heat_height;
		if (m_emerge->biomemgr->weather_hot_core &&
				p.Y < -(WEATHER_LIMIT - m_emerge->biomemgr->weather_hot_core))
			heat += 6000 *
					(1.0 - ((float)(p.Y - -WEATHER_LIMIT) /
								   m_emerge->biomemgr
										   ->weather_hot_core)); //hot core, later via realms
		return heat;
	}
#endif
	return m_emerge->biomemgr->calcBlockHeat(p, seed, timeofday, totaltime, use_weather);
}

// TODO: use cloud data
weather::humidity_t MapgenEarth::calcBlockHumidity(const v3pos_t &p, uint64_t seed,
		float timeofday, float totaltime, bool use_weather)
{
	return m_emerge->biomemgr->calcBlockHumidity(
			p, seed, timeofday, totaltime, use_weather);
}

maps_holder_t::~maps_holder_t()
{
	for (const auto &file : files_to_delete) {
#if NDEBUG
		std::error_code ec;
		std::filesystem::remove(file, ec);
#endif
	}
};

void MapgenEarth::makeChunk(BlockMakeData *data)
{
	// Pre-conditions
	assert(data->vmanip);
	assert(data->nodedef);

	//TimeTaker t("makeChunk");

	this->generating = true;
	this->vm = data->vmanip;
	this->ndef = data->nodedef;

	auto blockpos_min = data->blockpos_min;
	auto blockpos_max = data->blockpos_max;
	node_min = blockpos_min * MAP_BLOCKSIZE;
	node_max = (blockpos_max + v3pos_t(1, 1, 1)) * MAP_BLOCKSIZE - v3pos_t(1, 1, 1);
	full_node_min = (blockpos_min - 1) * MAP_BLOCKSIZE;
	full_node_max = (blockpos_max + 2) * MAP_BLOCKSIZE - v3pos_t(1, 1, 1);

	blockseed = getBlockSeed2(full_node_min, seed);

	//freeminer:
	layers_prepare(node_min, node_max);
	cave_prepare(node_min, node_max, sp->paramsj.get("cave_indev", -100).asInt());
	//==========

	bool voxel_earth = false;
	g_settings->getBoolNoEx("voxel_earth", voxel_earth);

#if USE_VOXEL_EARTH
	if (voxel_earth) {
		// Generate base and mountain terrain
		const auto stone_surface_max_y = generateTerrain();
		const auto tc_min = pos_to_ll(node_min.X, node_min.Z);
		const auto tc_max = pos_to_ll(node_max.X, node_max.Z);

		const auto tc = pos_to_ll(node_min + csize / 2);
		static std::string key;
		if (key.empty()) {
			g_settings->getNoEx("voxel_earth_api_key", key);
		}
		const auto origin_min = cartesianFromDegrees(tc_min.lat, tc_min.lon, node_min.Y);
		const auto origin_max = cartesianFromDegrees(tc_max.lat, tc_max.lon, node_max.Y);
		const auto origin_diff = origin_max - origin_min;
		const auto radius = csize.X / 2; // - MAP_BLOCKSIZE;
		const auto resolution = csize.X;
		const auto elevation = node_min.Y + csize.Y / 2;
		{
			start_download_and_voxelize(
					tc.lat, tc.lon, elevation, radius, resolution, key);
		}
	} else
#endif
	{
		// Generate base and mountain terrain
		pos_t stone_surface_max_y = generateTerrain();

		// Create heightmap
		updateHeightmap(node_min, node_max);

		// Init biome generator, place biome-specific nodes, and build biomemap
		if (flags & MG_BIOMES) {
			biomegen->calcBiomeNoise(node_min);
			generateBiomes();
		}

		// Generate tunnels, caverns and large randomwalk caves
		if (flags & MG_CAVES) {
			// Generate tunnels first as caverns confuse them
			generateCavesNoiseIntersection(stone_surface_max_y);

			// Generate caverns
			bool near_cavern = false;
			if (spflags & MGV7_CAVERNS)
				near_cavern = generateCavernsNoise(stone_surface_max_y);

			// Generate large randomwalk caves
			if (near_cavern)
				// Disable large randomwalk caves in this mapchunk by setting
				// 'large cave depth' to world base. Avoids excessive liquid in
				// large caverns and floating blobs of overgenerated liquid.
				generateCavesRandomWalk(stone_surface_max_y, -MAX_MAP_GENERATION_LIMIT);
			else
				generateCavesRandomWalk(stone_surface_max_y, large_cave_depth);
		}

		// Generate the registered ores
		if (flags & MG_ORES)
			m_emerge->oremgr->placeAllOres(this, blockseed, node_min, node_max);

		// Generate dungeons
		if (flags & MG_DUNGEONS)
			generateDungeons(stone_surface_max_y);

		// Generate the registered decorations
		if (flags & MG_DECORATIONS)
			m_emerge->decomgr->placeAllDecos(this, blockseed, node_min, node_max);

		// Sprinkle some dust on top after everything else was generated
		if (flags & MG_BIOMES)
			dustTopNodes();

		generateBuildings();
	}

	// Update liquids
	updateLiquid(&data->transforming_liquid, full_node_min, full_node_max);

	// Calculate lighting
	// Limit floatland shadows
	bool propagate_shadow =
			!((spflags & MGV7_FLOATLANDS) && node_max.Y >= floatland_ymin - csize.Y * 2 &&
					node_min.Y <= floatland_ymax);

	if (flags & MG_LIGHT)
		calcLighting(node_min - v3pos_t(0, 1, 0), node_max + v3pos_t(0, 1, 0),
				full_node_min, full_node_max, propagate_shadow);

	this->generating = false;
}
