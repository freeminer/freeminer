/*
mapgen_indev.h
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

#ifndef MAPGENINDEV_HEADER
#define MAPGENINDEV_HEADER

#include "mapgen.h"
#include "mapgen_v6.h"
#include "cavegen.h"

#define getNoiseIndevParams(x, y) getStruct((x), "f,f,v3,s32,s32,f,f,f,f", &(y), sizeof(y))
#define setNoiseIndevParams(x, y) setStruct((x), "f,f,v3,s32,s32,f,f,f,f", &(y))

struct MapgenIndevParams : public MapgenV6Params {
	s16 float_islands;
	NoiseParams np_float_islands1;
	NoiseParams np_float_islands2;
	NoiseParams np_float_islands3;

	MapgenIndevParams();
	~MapgenIndevParams() {}

	void readParams(Settings *settings);
	void writeParams(Settings *settings);
};

class MapgenIndev : public MapgenV6 {
public:
	Noise *noise_float_islands1;
	Noise *noise_float_islands2;
	Noise *noise_float_islands3;
	s16 float_islands;

	MapgenIndev(int mapgenid, MapgenParams *params, EmergeManager *emerge);
	~MapgenIndev();
	void calculateNoise();

	void generateCaves(int max_stone_y);
	void generateExperimental();
	
	void generateFloatIslands(int min_y);
};

struct MapgenFactoryIndev : public MapgenFactoryV6 {
	Mapgen *createMapgen(int mgid, MapgenParams *params, EmergeManager *emerge) {
		return new MapgenIndev(mgid, params, emerge);
	};

	MapgenSpecificParams *createMapgenParams() {
		return new MapgenIndevParams();
	};
};

class CaveIndev : public CaveV6 {
public:
	CaveIndev(MapgenIndev *mg, PseudoRandom *ps, PseudoRandom *ps2,
			v3s16 node_min, bool is_large_cave);
};

#endif
