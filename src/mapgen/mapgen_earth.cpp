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
#include <cstdlib>
#include <memory>
#include <mutex>
#include <osmium/geom/tile.hpp>
#include <sstream>
#include <string>
#include <system_error>

#include "debug/dump.h"
#include "filesys.h"
#include "irr_v2d.h"
#include "irrlichttypes.h"
#include "mapgen/earth/hgt.h"
#include "mapgen/earth/http.h"
#include "mapgen_earth.h"
#include "voxel.h"
#include "mapblock.h"
#include "mapnode.h"
#include "map.h"
#include "nodedef.h"
#include "voxelalgorithms.h"
#include "settings.h"
#include "emerge.h"
#include "serverenvironment.h"
#include "mg_biome.h"
#include "log_types.h"
#if USE_OSMIUM
#include <filesystem>
#include <osmium/area/assembler.hpp>
#include <osmium/area/multipolygon_manager.hpp>
#include <osmium/dynamic_handler.hpp>
#include <osmium/handler/node_locations_for_ways.hpp>
#include <osmium/index/map/sparse_mem_array.hpp>
#include <osmium/io/file.hpp>
#include <osmium/io/pbf_input.hpp>
#include <osmium/osm/entity_bits.hpp>
#include <osmium/osm/node.hpp>
#include <osmium/osm/way.hpp>
#include <osmium/tags/tags_filter.hpp>

#include "earth/osmium-inl.h"

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
	// mg_params = (MapgenEarthParams *)params_->sparams;
	mg_params = params_;

	Json::Value &params = mg_params->params;
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
}

MapgenEarth::~MapgenEarth()
{
}

//////////////////////// Map generator

MapNode MapgenEarth::layers_get(float value, float max)
{
	auto layer_index = rangelim((unsigned int)myround((value / max) * layers_node_size),
			0, layers_node_size - 1);
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

constexpr double EQUATOR_LEN{40075696.0};
ll MapgenEarth::pos_to_ll(const pos_t x, const pos_t z)
{
	const auto lon = ((ll_t)x * scale.X) / (EQUATOR_LEN / 360) + center.X;
	const auto lat = ((ll_t)z * scale.Z) / (EQUATOR_LEN / 360) + center.Z;
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
	return v2pos_t((l.lon / scale.X - center.X) * (EQUATOR_LEN / 360),
			(l.lat / scale.Z - center.Z) * (EQUATOR_LEN / 360));
}

pos_t MapgenEarth::get_height(pos_t x, pos_t z)
{
	const auto tc = pos_to_ll(x, z);
	auto y = maps_holder->hgt_reader.get(tc.lat, tc.lon);
	return ceil(y / scale.Y) - center.Y;
}

int MapgenEarth::getSpawnLevelAtPoint(v2pos_t p)
{
	return std::max(2, get_height(p.X, p.Y) + 2);
}

int MapgenEarth::getGroundLevelAtPoint(v2pos_t p)
{
	return get_height(p.X, p.Y); // + MGV6_AVERAGE_MUD_AMOUNT;
}

int MapgenEarth::generateTerrain()
{
	MapNode n_ice(c_ice);
	u32 index = 0;
	auto em = vm->m_area.getExtent();

	for (pos_t z = node_min.Z; z <= node_max.Z; z++) {
		for (pos_t x = node_min.X; x <= node_max.X; x++, index++) {
			auto heat =
					m_emerge->env->m_use_weather
							? m_emerge->env->getServerMap().updateBlockHeat(m_emerge->env,
									  v3pos_t(x, node_max.Y, z), nullptr, &heat_cache)
							: 0;
			auto height = get_height(x, z);
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

const auto make_bbox = [](auto tc, auto div) {
	const auto lat_dec01 = floor01(tc.lat, div);
	const auto lon_dec01 = floor01(tc.lon, div);
	const auto lat_end_dec01 = floor01(tc.lat + (1.0 / div), div);
	const auto lon_end_dec01 = floor01(tc.lon + (1.0 / div), div);
	std::stringstream bboxs;
	bboxs << lon_dec01 << "," << lat_dec01 << "," << lon_end_dec01 << ","
		  << lat_end_dec01;
	auto bbox = bboxs.str();
	return bbox;
};

void MapgenEarth::generateBuildings()
{

#if USE_OSMIUM

	//#define FILE_INCLUDED 1
	//#include "earth/osmium-inl.h"
	const auto tc = pos_to_ll(node_min.X, node_min.Z);
	const auto tc_max = pos_to_ll(node_max.X, node_max.Z);
	static const auto folder = porting::path_cache + DIR_DELIM + "earth";
	const auto lat_dec = lat_start(tc.lat);
	const auto lon_dec = lon_start(tc.lon);

	static const auto timestamp = []() {
		std::string ts = "latest";
		g_settings->getNoEx("earth_movisda_timestamp", ts);
		return ts;
	}();
	char buff[100];
	std::snprintf(buff, sizeof(buff), "%c%02d%c%03d-%s.osm.pbf", lat_dec >= 0 ? 'N' : 'S',
			abs(lat_dec), lon_dec >= 0 ? 'W' : 'E', abs(lon_dec), timestamp.c_str());
	std::string filename = buff;
	const auto base_full_name = folder + DIR_DELIM + filename;
	if (!std::filesystem::exists(base_full_name)) {
		const auto lock = std::lock_guard(maps_holder->osm_http_lock);
		if (!std::filesystem::exists(base_full_name)) {
			const auto url = "https://osm.download.movisda.io/grid/" + filename;
			multi_http_to_file({url}, base_full_name);
		}
	}

	std::string use_file = base_full_name;
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

			std::stringstream cmd;
			// TODO: use osmium tool as lib
			cmd << "osmium extract --output-format pbf --strategy smart " << "--bbox "
				<< bbox << " --output " << filename << ".tmp" << " " << path_name;
			exec_to_string(cmd.str());
			if (!std::filesystem::exists(filename + ".tmp")) {
				return false;
			}

			std::error_code error_code;
			std::filesystem::rename(filename + ".tmp", filename, error_code);
			return !error_code.value();
		};

		for (auto div = 10; div <= 1000; div *= 10) {
			std::error_code ec;
			const auto size = std::filesystem::file_size(use_file, ec);
			if (ec || size < 40000) {
				break;
			};

			const auto bbox_next = make_bbox(tc, div);
			auto filename_next = folder + DIR_DELIM + "extract." + std::to_string(div) +
								 "." + bbox_next + ".osm.pbf";
			if (!try_extract(use_file, bbox_next, filename_next)) {
				break;
			}
			use_file = filename_next;
			bbox = bbox_next;
		}
	}

	if (std::filesystem::exists(use_file)) {
		const auto osm = std::make_shared<hdl>(this, use_file);
		const auto lock = maps_holder->osm_bbox.lock_unique_rec();
		if (!maps_holder->osm_bbox.contains(bbox)) {
			maps_holder->osm_bbox.emplace(bbox, osm);
		}
	}

	if (const auto &hdlr = maps_holder->osm_bbox.get(bbox)) {
		hdlr->apply();
	}

	warningstream << "Buildings stat: " << node_min << " set=" << stat.set
				  << " miss=" << stat.miss << " level=" << stat.level
				  << " check=" << stat.check << " fill=" << stat.fill << "\n";
	stat = {};

#endif
}

weather::heat_t MapgenEarth::calcBlockHeat(const v3pos_t &p, uint64_t seed,
		float timeofday, float totaltime, bool use_weather)
{
	const auto ll = pos_to_ll(p);
	const auto tile = osmium::geom::Tile(3, osmium::Location(ll.lon, ll.lat));
	return m_emerge->biomemgr->calcBlockHeat(p, seed, timeofday, totaltime, use_weather);
}

weather::humidity_t MapgenEarth::calcBlockHumidity(const v3pos_t &p, uint64_t seed,
		float timeofday, float totaltime, bool use_weather)
{
	return m_emerge->biomemgr->calcBlockHumidity(
			p, seed, timeofday, totaltime, use_weather);
}
