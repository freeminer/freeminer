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

#pragma once

#include <atomic>
#include <memory>
#include <mutex>
#include <string>

#include "earth/hgt.h"
#include "mapgen/mapgen_v7.h"
#include "porting.h"
#include "filesys.h"
#include "threading/concurrent_map.h"
#include "threading/concurrent_vector.h"
#include "util/lrucache.hpp"

class PngImage;

typedef core::vector2d<double> v2d;

//using ll_t = float;
using ll_t = double;
struct ll
{
	ll_t lat{};
	ll_t lon{};
};

inline std::ostream &operator<<(std::ostream &s, const ll &p)
{
	s << "(" << p.lat << "," << p.lon << ")";
	return s;
}

struct MapgenEarthParams : public MapgenV7Params
{
	MapgenEarthParams(){};
	~MapgenEarthParams(){};

	Json::Value params;

	void readParams(const Settings *settings) override;
	void writeParams(Settings *settings) const override;
	void setDefaultSettings(Settings *settings) override;
};

class MapgenEarth;

class handler_i
{
public:
	virtual void apply(MapgenEarth *) = 0;
};

struct maps_holder_t
{
	const std::string data_root{porting::path_cache + DIR_DELIM + "earth"};
	hgts hgt_reader{data_root};
	using osm_ptr = std::shared_ptr<handler_i>;
	lru_cache<std::string, osm_ptr, 50> osm_bbox;
	std::mutex download_lock;
	std::mutex osm_bbox_lock;
	std::mutex osm_http_lock;
	std::mutex osm_extract_lock;
	std::unique_ptr<PngImage> heat_image;
	concurrent_shared_vector<std::string> files_to_delete;
	~maps_holder_t();
};

class MapgenEarth : public MapgenV7
{
public:
	MapgenEarthParams *mg_params;

	static std::unique_ptr<maps_holder_t> maps_holder;

	virtual MapgenType getType() const override { return MAPGEN_EARTH; }
	MapgenEarth(MapgenEarthParams *mg_params, EmergeParams *emerge);
	~MapgenEarth();

	void makeChunk(BlockMakeData *data) override;
	int generateTerrain() override;
	void generateBuildings() override;
	int getSpawnLevelAtPoint(v2pos_t p) override;
	int getGroundLevelAtPoint(v2pos_t p) override;

	v3d scale{1, 1, 1};
	v3d center{0, 0, 0};
	bool no_layers = false;

	MapNode n_air, n_water, n_stone;

	MapNode layers_get(float value, float max);
	bool visible(const v3pos_t &p) override;
	const MapNode &visible_content(const v3pos_t &p, bool use_weather) override;

	pos_t get_height(pos_t x, pos_t z);
	ll pos_to_ll(pos_t x, pos_t z);
	ll pos_to_ll(const v3pos_t &p);
	v2pos_t ll_to_pos(const ll &l);

	weather::heat_t calcBlockHeat(const v3pos_t &p, uint64_t seed, float timeofday,
			float totaltime, bool use_weather) override;
	weather::humidity_t calcBlockHumidity(const v3pos_t &p, uint64_t seed,
			float timeofday, float totaltime, bool use_weather) override;

	struct Stat
	{
		std::atomic_int set{};
		std::atomic_int miss{};
		std::atomic_int level{};
		std::atomic_int check{};
		std::atomic_int fill{};
		void clean() { set = miss = level = check = fill = 0; }
	} stat;

	void start_download_and_voxelize(double lat, double lon, double elevation,
			double radius, int resolution, const std::string &api_key);
};
