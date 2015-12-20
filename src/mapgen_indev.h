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

#include "json/json.h"
#include "mapgen.h"
#include "mapgen_v6.h"
#include "cavegen.h"

//#define getNoiseIndevParams(x, y) getStruct((x), "f,f,v3,s32,s32,f,f,f,f", &(y), sizeof(y))
//#define setNoiseIndevParams(x, y) setStruct((x), "f,f,v3,s32,s32,f,f,f,f", &(y))


typedef struct {
	content_t content;
	MapNode node;
	int height_min;
	int height_max;
	int thickness;
	//std::string name; //dev
} layer_data;

class Mapgen_features {
public:

	Mapgen_features(int mapgenid, MapgenParams *params, EmergeManager *emerge);
	~Mapgen_features();

	int y_offset;
	MapNode n_stone;
	Noise *noise_layers;
	float noise_layers_width;
	std::vector<layer_data> layers;
	std::vector<MapNode> layers_node;
	unsigned int layers_node_size;
	void layers_init(EmergeManager *emerge, const Json::Value & layersj);
	void layers_prepare(const v3POS & node_min, const v3POS & node_max);
	MapNode layers_get(unsigned int index);

	Noise *noise_float_islands1;
	Noise *noise_float_islands2;
	Noise *noise_float_islands3;
	void float_islands_prepare(const v3POS & node_min, const v3POS & node_max, int min_y);
	int float_islands_generate(const v3POS & node_min, const v3POS & node_max, int min_y, MMVManip *vm);

	Noise *noise_cave_indev;
	int cave_noise_threshold;
	bool cave_noise_enabled;
	void cave_prepare(const v3POS & node_min, const v3POS & node_max, int max_y);

};


struct MapgenIndevParams : public MapgenV6Params {
	s16 float_islands;

	NoiseParams np_float_islands1;
	NoiseParams np_float_islands2;
	NoiseParams np_float_islands3;
	NoiseParams np_layers;
	NoiseParams np_cave_indev;

	Json::Value paramsj;

	MapgenIndevParams();
	~MapgenIndevParams() {}

	void readParams(Settings *settings);
	void writeParams(Settings *settings) const;
};

class MapgenIndev : public MapgenV6, public Mapgen_features {
public:
	MapgenIndevParams *sp;

	int xstride, ystride, zstride;

	MapgenIndev(int mapgenid, MapgenParams *params, EmergeManager *emerge);
	~MapgenIndev();

	virtual void calculateNoise();
	int generateGround();
	void generateCaves(int max_stone_y);
	void generateExperimental();
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
			v3POS node_min, bool is_large_cave);
};

#endif
