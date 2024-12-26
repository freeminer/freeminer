// Luanti
// SPDX-License-Identifier: LGPL-2.1-or-later
// Copyright (C) 2014-2020 paramat
// Copyright (C) 2013-2016 kwolekr, Ryan Kwolek <kwolekr@minetest.net>

#pragma once

#include "mapgen.h"
#include "mapgen_indev.h"

///////////// Mapgen V7 flags
#define MGV7_MOUNTAINS   0x01
#define MGV7_RIDGES      0x02
#define MGV7_FLOATLANDS  0x04
#define MGV7_CAVERNS     0x08
#define MGV7_BIOMEREPEAT 0x10 // Now unused

class BiomeManager;

extern FlagDesc flagdesc_mapgen_v7[];


struct MapgenV7Params : public MapgenParams {
	pos_t mount_zero_level = 0;
	pos_t floatland_ymin = 1024;
    pos_t floatland_ymax = 4096;
	pos_t floatland_taper = 256;
	float float_taper_exp = 2.0f;
	float floatland_density = -0.6f;
	pos_t floatland_ywater = -MAX_MAP_GENERATION_LIMIT;

	float cave_width = 0.09f;
	pos_t large_cave_depth = -33;
	u16 small_cave_num_min = 0;
	u16 small_cave_num_max = 0;
	u16 large_cave_num_min = 0;
	u16 large_cave_num_max = 2;
	float large_cave_flooded = 0.5f;
	pos_t cavern_limit = -256;
	pos_t cavern_taper = 256;
	float cavern_threshold = 0.7f;
	pos_t dungeon_ymin = -MAX_MAP_GENERATION_LIMIT;
	pos_t dungeon_ymax = MAX_MAP_GENERATION_LIMIT;

	NoiseParams np_terrain_base;
	NoiseParams np_terrain_alt;
	NoiseParams np_terrain_persist;
	NoiseParams np_height_select;
	NoiseParams np_filler_depth;
	NoiseParams np_mount_height;
	NoiseParams np_ridge_uwater;
	NoiseParams np_mountain;
	NoiseParams np_ridge;
	NoiseParams np_floatland;
	NoiseParams np_cavern;
	NoiseParams np_cave1;
	NoiseParams np_cave2;
	NoiseParams np_dungeons;

	//freeminer: ===
	NoiseParams np_layers;
	NoiseParams np_cave_indev;
	Json::Value paramsj;
	// =============

	MapgenV7Params();
	~MapgenV7Params() = default;

	virtual
	void readParams(const Settings *settings);
	virtual
	void writeParams(Settings *settings) const;
	virtual
	void setDefaultSettings(Settings *settings);
};


class MapgenV7 : public MapgenBasic, public Mapgen_features {
public:
	MapgenV7(MapgenV7Params *params, EmergeParams *emerge);
	~MapgenV7();

	virtual MapgenType getType() const { return MAPGEN_V7; }

	virtual void makeChunk(BlockMakeData *data);
	virtual
	int getSpawnLevelAtPoint(v2pos_t p);

	float baseTerrainLevelAtPoint(pos_t x, pos_t z);
	float baseTerrainLevelFromMap(int index);
	bool getMountainTerrainAtPoint(pos_t x, pos_t y, pos_t z);
	bool getMountainTerrainFromMap(int idx_xyz, int idx_xz, pos_t y);
	bool getRiverChannelFromMap(int idx_xyz, int idx_xz, pos_t y);
	bool getFloatlandTerrainFromMap(int idx_xyz, float float_offset);

	virtual int generateTerrain();

private:
	pos_t mount_zero_level;
	pos_t floatland_ymin;
	pos_t floatland_ymax;
	pos_t floatland_taper;
	float float_taper_exp;
	float floatland_density;
	pos_t floatland_ywater;

	float *float_offset_cache = nullptr;

	Noise *noise_terrain_base;
	Noise *noise_terrain_alt;
	Noise *noise_terrain_persist;
	Noise *noise_height_select;
	Noise *noise_mount_height;
	Noise *noise_ridge_uwater;
	Noise *noise_mountain;
	Noise *noise_ridge;

	//freeminer:
	MapgenV7Params *sp{};
	//virtual void generateExperimental();
	// freeminer:
public:
	virtual bool visible(const v3pos_t &p);

private:
	// ==

	Noise *noise_floatland;
};
