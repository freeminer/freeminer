/*
mapgen_singlenode.h
Copyright (C) 2010-2013 celeron55, Perttu Ahola <celeron55@gmail.com>
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

#ifndef MAPGEN_SINGLENODE_HEADER
#define MAPGEN_SINGLENODE_HEADER

#include "mapgen.h"

struct MapgenSinglenodeParams : public MapgenSpecificParams {
	
	MapgenSinglenodeParams() {}
	~MapgenSinglenodeParams() {}
	
	void readParams(Settings *settings) {}
	void writeParams(Settings *settings) const {}
};

class MapgenSinglenode : public Mapgen {
public:
	u32 flags;
	content_t c_node;

	MapgenSinglenode(int mapgenid, MapgenParams *params, EmergeManager *emerge);
	~MapgenSinglenode();
	
	void makeChunk(BlockMakeData *data);
	int getGroundLevelAtPoint(v2s16 p);
};

struct MapgenFactorySinglenode : public MapgenFactory {
	Mapgen *createMapgen(int mgid, MapgenParams *params, EmergeManager *emerge) {
		return new MapgenSinglenode(mgid, params, emerge);
	};
	
	MapgenSpecificParams *createMapgenParams() {
		return new MapgenSinglenodeParams();
	};
};

#endif
