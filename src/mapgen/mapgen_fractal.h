// Luanti
// SPDX-License-Identifier: LGPL-2.1-or-later
// Copyright (C) 2015-2019 paramat
// Copyright (C) 2015-2016 kwolekr, Ryan Kwolek

#pragma once

#include "mapgen.h"

///////////// Mapgen Fractal flags
#define MGFRACTAL_TERRAIN     0x01

class BiomeManager;

extern FlagDesc flagdesc_mapgen_fractal[];


struct MapgenFractalParams : public MapgenParams
{
	float cave_width = 0.09f;
	pos_t large_cave_depth = -33;
	u16 small_cave_num_min = 0;
	u16 small_cave_num_max = 0;
	u16 large_cave_num_min = 0;
	u16 large_cave_num_max = 2;
	float large_cave_flooded = 0.5f;
	pos_t dungeon_ymin = -MAX_MAP_GENERATION_LIMIT;
	pos_t dungeon_ymax = MAX_MAP_GENERATION_LIMIT;
	u16 fractal = 1;
	u16 iterations = 11;
	v3f scale = v3f(4096.0, 1024.0, 4096.0);
	v3f offset = v3f(1.52, 0.0, 0.0);
	float slice_w = 0.0f;
	float julia_x = 0.267f;
	float julia_y = 0.2f;
	float julia_z = 0.133f;
	float julia_w = 0.067f;

	NoiseParams np_seabed;
	NoiseParams np_filler_depth;
	NoiseParams np_cave1;
	NoiseParams np_cave2;
	NoiseParams np_dungeons;

	MapgenFractalParams();
	~MapgenFractalParams() = default;

	void readParams(const Settings *settings);
	void writeParams(Settings *settings) const;
	void setDefaultSettings(Settings *settings);
};


class MapgenFractal : public MapgenBasic
{
public:
	MapgenFractal(MapgenFractalParams *params, EmergeParams *emerge);
	~MapgenFractal();

	virtual MapgenType getType() const { return MAPGEN_FRACTAL; }

	virtual void makeChunk(BlockMakeData *data);
	int getSpawnLevelAtPoint(v2pos_t p);
	bool getFractalAtPoint(pos_t x, pos_t y, pos_t z);
	pos_t generateTerrain();

private:
	u16 formula;
	bool julia;
	u16 fractal;
	u16 iterations;
	v3f scale;
	v3f offset;
	float slice_w;
	float julia_x;
	float julia_y;
	float julia_z;
	float julia_w;
	Noise *noise_seabed = nullptr;
};
