/*
mapgen_singlenode.h
Minetest
Copyright (C) 2013-2018 celeron55, Perttu Ahola <celeron55@gmail.com>
Copyright (C) 2013-2018 kwolekr, Ryan Kwolek <kwolekr@minetest.net>
Copyright (C) 2015-2018 paramat

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

#include "mapgen.h"

struct MapgenSinglenodeParams : public MapgenParams
{
	MapgenSinglenodeParams() = default;
	~MapgenSinglenodeParams() = default;

	void readParams(const Settings *settings) {}
	void writeParams(Settings *settings) const {}
};

class MapgenSinglenode : public Mapgen
{
public:
	content_t c_node;
	u8 set_light;

	MapgenSinglenode(MapgenParams *params, EmergeParams *emerge);
	~MapgenSinglenode() = default;

	virtual MapgenType getType() const { return MAPGEN_SINGLENODE; }

	void makeChunk(BlockMakeData *data);
	int getSpawnLevelAtPoint(v2pos_t p);
};
