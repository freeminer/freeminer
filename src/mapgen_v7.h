/*
mapgen_v7.h
Copyright (C) 2010-2013 kwolekr, Ryan Kwolek <kwolekr@minetest.net>
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

#ifndef MAPGEN_V7_HEADER
#define MAPGEN_V7_HEADER

#include "mapgen.h"
#include "mapgen_indev.h"
/////////////////// Mapgen V7 flags
#define MGV7_MOUNTAINS   0x01
#define MGV7_RIDGES      0x02

class BiomeManager;

extern FlagDesc flagdesc_mapgen_v7[];


struct MapgenV7Params : public MapgenSpecificParams {
	u32 spflags;
	NoiseParams np_terrain_base;
	NoiseParams np_terrain_alt;
	NoiseParams np_terrain_persist;
	NoiseParams np_height_select;
	NoiseParams np_filler_depth;
	NoiseParams np_mount_height;
	NoiseParams np_ridge_uwater;
	NoiseParams np_mountain;
	NoiseParams np_ridge;
	NoiseParams np_cave1;
	NoiseParams np_cave2;

	s16 float_islands;
	NoiseParams np_float_islands1;
	NoiseParams np_float_islands2;
	NoiseParams np_float_islands3;
	NoiseParams np_layers;
	Json::Value paramsj;

	MapgenV7Params();
	~MapgenV7Params() {}

	void readParams(Settings *settings);
	void writeParams(Settings *settings);
};

class MapgenV7 : public Mapgen, public Mapgen_features {
public:
	EmergeManager *m_emerge;
	BiomeManager *bmgr;

	int ystride;
	int zstride;
	u32 spflags;

	v3s16 node_min;
	v3s16 node_max;
	v3s16 full_node_min;
	v3s16 full_node_max;

	s16 *ridge_heightmap;

	Noise *noise_terrain_base;
	Noise *noise_terrain_alt;
	Noise *noise_terrain_persist;
	Noise *noise_height_select;
	Noise *noise_filler_depth;
	Noise *noise_mount_height;
	Noise *noise_ridge_uwater;
	Noise *noise_mountain;
	Noise *noise_ridge;
	Noise *noise_cave1;
	Noise *noise_cave2;

	Noise *noise_heat;
	Noise *noise_humidity;

	//freeminer:
	s16 float_islands;
	content_t c_dirt_with_snow;

	content_t c_stone;
	content_t c_dirt;
	content_t c_dirt_with_grass;
	content_t c_sand;
	content_t c_water_source;
	content_t c_lava_source;
	content_t c_ice;
	content_t c_gravel;
	content_t c_cobble;
	content_t c_desert_sand;
	content_t c_desert_stone;

	MapgenV7(int mapgenid, MapgenParams *params, EmergeManager *emerge);
	~MapgenV7();

	virtual void makeChunk(BlockMakeData *data);
	int getGroundLevelAtPoint(v2s16 p);
	Biome *getBiomeAtPoint(v3s16 p);

	float baseTerrainLevelAtPoint(int x, int z);
	float baseTerrainLevelFromMap(int index);
	bool getMountainTerrainAtPoint(int x, int y, int z);
	bool getMountainTerrainFromMap(int idx_xyz, int idx_xz, int y);

	virtual void calculateNoise();

	virtual int generateTerrain();
	int generateBaseTerrain();
	int generateMountainTerrain(int ymax);
	void generateRidgeTerrain();

	void generateBiomes();
	void dustTopNodes();

	//void addTopNodes();

	void generateCaves(int max_stone_y);

	virtual void generateExperimental();

};

struct MapgenFactoryV7 : public MapgenFactory {
	Mapgen *createMapgen(int mgid, MapgenParams *params, EmergeManager *emerge)
	{
		return new MapgenV7(mgid, params, emerge);
	};

	MapgenSpecificParams *createMapgenParams()
	{
		return new MapgenV7Params();
	};
};

#endif
