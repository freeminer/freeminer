/*
mapgen_v6.h
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

#ifndef MAPGENV6_HEADER
#define MAPGENV6_HEADER

#include "mapgen.h"
#include "noise.h"

#define AVERAGE_MUD_AMOUNT 4
#define DESERT_STONE_BASE -32
#define ICE_BASE 0
#define FREQ_HOT 0.4
#define FREQ_SNOW -0.4
#define FREQ_TAIGA 0.5
#define FREQ_JUNGLE 0.7

//////////// Mapgen V6 flags
#define MGV6_JUNGLES    0x01
#define MGV6_BIOMEBLEND 0x02
#define MGV6_MUDFLOW    0x04
#define MGV6_SNOWBIOMES 0x08


extern FlagDesc flagdesc_mapgen_v6[];


enum BiomeV6Type
{
	BT_NORMAL,
	BT_DESERT,
	BT_JUNGLE,
	BT_TUNDRA,
	BT_TAIGA,
};


struct MapgenV6Params : public MapgenSpecificParams {
	u32 spflags;
	float freq_desert;
	float freq_beach;
	NoiseParams np_terrain_base;
	NoiseParams np_terrain_higher;
	NoiseParams np_steepness;
	NoiseParams np_height_select;
	NoiseParams np_mud;
	NoiseParams np_beach;
	NoiseParams np_biome;
	NoiseParams np_cave;
	NoiseParams np_humidity;
	NoiseParams np_trees;
	NoiseParams np_apple_trees;

	MapgenV6Params();
	~MapgenV6Params() {}

	void readParams(Settings *settings);
	void writeParams(Settings *settings) const;
};


class MapgenV6 : public Mapgen {
public:
	EmergeManager *m_emerge;

	int ystride;
	u32 spflags;

	v3s16 node_min;
	v3s16 node_max;
	v3s16 full_node_min;
	v3s16 full_node_max;
	v3s16 central_area_size;
	int volume_nodes;

	Noise *noise_terrain_base;
	Noise *noise_terrain_higher;
	Noise *noise_steepness;
	Noise *noise_height_select;
	Noise *noise_mud;
	Noise *noise_beach;
	Noise *noise_biome;
	Noise *noise_humidity;
	NoiseParams *np_cave;
	NoiseParams *np_humidity;
	NoiseParams *np_trees;
	NoiseParams *np_apple_trees;
	float freq_desert;
	float freq_beach;

	content_t c_stone;
	content_t c_dirt;
	content_t c_dirt_with_grass;
	content_t c_sand;
	content_t c_water_source;
	content_t c_lava_source;
	content_t c_gravel;
	content_t c_desert_stone;
	content_t c_desert_sand;
	content_t c_dirt_with_snow;
	content_t c_snow;
	content_t c_snowblock;

	content_t c_ice;

	content_t c_cobble;
	content_t c_mossycobble;
	content_t c_stair_cobble;

	MapgenV6(int mapgenid, MapgenParams *params, EmergeManager *emerge);
	~MapgenV6();

	void makeChunk(BlockMakeData *data);
	int getGroundLevelAtPoint(v2s16 p);

	float baseTerrainLevel(float terrain_base, float terrain_higher,
		float steepness, float height_select);
	virtual float baseTerrainLevelFromNoise(v2s16 p);
	virtual float baseTerrainLevelFromMap(v2s16 p);
	virtual float baseTerrainLevelFromMap(int index);

	s16 find_stone_level(v2s16 p2d);
	bool block_is_underground(u64 seed, v3s16 blockpos);
	s16 find_ground_level_from_noise(u64 seed, v2s16 p2d, s16 precision);

	float getHumidity(v3POS p);
	float getTreeAmount(v2s16 p);
	bool getHaveAppleTree(v2s16 p);
	float getMudAmount(v2s16 p);
	virtual float getMudAmount(int index);
	bool getHaveBeach(v2s16 p);
	bool getHaveBeach(int index);
	BiomeV6Type getBiome(v3POS p);
	BiomeV6Type getBiome(int index, v3POS p);

	u32 get_blockseed(u64 seed, v3s16 p);

	virtual void calculateNoise();
	virtual int generateGround();
	void addMud();
	void flowMud(s16 &mudflow_minpos, s16 &mudflow_maxpos);
	void growGrass();
	void placeTreesAndJungleGrass();
	virtual void generateCaves(int max_stone_y);
};


struct MapgenFactoryV6 : public MapgenFactory {
	Mapgen *createMapgen(int mgid, MapgenParams *params, EmergeManager *emerge)
	{
		return new MapgenV6(mgid, params, emerge);
	};

	MapgenSpecificParams *createMapgenParams()
	{
		return new MapgenV6Params();
	};
};


#endif
