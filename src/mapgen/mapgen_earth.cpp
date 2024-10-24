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
#include <string>

#include "debug/iostream_debug_helpers.h"
#include "filesys.h"
#include "irr_v2d.h"
#include "irrlichttypes.h"
#include "mapgen/earth/hgt.h"
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
#include <osmium/osm/entity_bits.hpp>
#include <osmium/osm/node.hpp>
#include <osmium/osm/way.hpp>
#include <osmium/tags/tags_filter.hpp>
#endif

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
		MapgenV7((MapgenV7Params *)params_, emerge),
		hgt_reader(porting::path_cache + DIR_DELIM + "earth")
{
	ndef = emerge->ndef;
	// mg_params = (MapgenEarthParams *)params_->sparams;
	mg_params = params_;

	Json::Value &params = mg_params->params;

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

#if USE_OSMIUM
#include "earth/osmium-inl.h"
	const auto path_name =
			porting::path_cache + DIR_DELIM + "earth" + DIR_DELIM + "map.pbf";
	if (std::filesystem::exists(path_name)) {
		handler = std::make_unique<hdl>(this, path_name);
	}
#endif
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

v2pos_t MapgenEarth::ll_to_pos(const ll &l)
{
	return v2pos_t((l.lon / scale.X - center.X) * (EQUATOR_LEN / 360),
			(l.lat / scale.Z - center.Z) * (EQUATOR_LEN / 360));
}

pos_t MapgenEarth::get_height(pos_t x, pos_t z)
{
	const auto tc = pos_to_ll(x, z);
	auto y = hgt_reader.get(tc.lat, tc.lon);
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

//  https://www.roguebasin.com/index.php?title=Bresenham%27s_Line_Algorithm
void MapgenEarth::bresenham(pos_t x1, pos_t y1, const pos_t x2, const pos_t y2, pos_t y,
		pos_t h, const MapNode &n)
{
	pos_t delta_x(x2 - x1);
	// if x1 == x2, then it does not matter what we set here
	const int8_t ix((delta_x > 0) - (delta_x < 0));
	delta_x = std::abs(delta_x) << 1;

	pos_t delta_y(y2 - y1);
	// if y1 == y2, then it does not matter what we set here
	const int8_t iy((delta_y > 0) - (delta_y < 0));
	delta_y = std::abs(delta_y) << 1;

	if (vm->exists({x1, y, y1})) {
		for (pos_t yi = y; yi <= y + h; ++yi) {
			vm->setNode({x1, yi, y1}, n);
		}
	}

	if (delta_x >= delta_y) {
		// error may go below zero
		pos_t error(delta_y - (delta_x >> 1));

		while (x1 != x2) {
			// reduce error, while taking into account the corner case of error == 0
			if ((error > 0) || (!error && (ix > 0))) {
				error -= delta_x;
				y1 += iy;
			}

			error += delta_y;
			x1 += ix;

			if (vm->exists({x1, y, y1})) {
				for (pos_t yi = y; yi <= y + h; ++yi) {
					vm->setNode({x1, yi, y1}, n);
				}
			}
		}
	} else {
		// error may go below zero
		int error(delta_x - (delta_y >> 1));

		while (y1 != y2) {
			// reduce error, while taking into account the corner case of error == 0
			if ((error > 0) || (!error && (iy > 0))) {
				error -= delta_y;
				x1 += ix;
			}

			error += delta_x;
			y1 += iy;

			if (vm->exists({x1, y, y1})) {
				for (pos_t yi = y; yi <= y + h; ++yi) {
					vm->setNode({x1, yi, y1}, n);
				}
			}
		}
	}
}

int MapgenEarth::generateTerrain()
{
	MapNode n_ice(c_ice);
	u32 index = 0;
	v3pos_t em = vm->m_area.getExtent();

	for (pos_t z = node_min.Z; z <= node_max.Z; z++) {
		for (pos_t x = node_min.X; x <= node_max.X; x++, index++) {
			s16 heat =
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

void MapgenEarth::generateBuildings()
{

	if (handler)
		handler->apply();
}
