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

#include <memory>
#include "config.h"
#include "mapgen/earth/hgt.h"
#include "mapgen/mapgen.h"
#include "mapgen/mapgen_v7.h"
#include "json/json.h"

//using ll_t = float;
using ll_t = double;
struct ll
{
	ll_t lat = 0;
	ll_t lon = 0;
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
	virtual void apply() = 0;
};

class MapgenEarth : public MapgenV7
{
public:
	MapgenEarthParams *mg_params;

	std::unique_ptr<handler_i> handler;

	virtual MapgenType getType() const override { return MAPGEN_EARTH; }
	MapgenEarth(MapgenEarthParams *mg_params, EmergeParams *emerge);
	~MapgenEarth();

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

	hgts hgt_reader;

	pos_t get_height(pos_t x, pos_t z);
	ll pos_to_ll(pos_t x, pos_t z);
	v2pos_t ll_to_pos(const ll &l);
	void bresenham(
			pos_t xa, pos_t za, pos_t xb, pos_t zb, pos_t y, pos_t h, const MapNode &n);
};
