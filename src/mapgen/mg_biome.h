// Luanti
// SPDX-License-Identifier: LGPL-2.1-or-later
// Copyright (C) 2014-2020 paramat
// Copyright (C) 2014-2016 kwolekr, Ryan Kwolek <kwolekr@minetest.net>

#pragma once

#include "fm_weather.h"
#include "irr_v3d.h"
#include "objdef.h"
#include "nodedef.h"
#include "noise.h"
#include "debug.h" // FATAL_ERROR_IF

struct MapgenParams;

class Server;
class Settings;
class BiomeManager;

////
//// Biome
////

typedef u16 biome_t;

constexpr v3pos_t MAX_MAP_GENERATION_LIMIT_V3(
	MAX_MAP_GENERATION_LIMIT,
	MAX_MAP_GENERATION_LIMIT,
	MAX_MAP_GENERATION_LIMIT
);

#define BIOME_NONE ((biome_t)0)

enum BiomeType {
	BIOMETYPE_NORMAL,
};

class Biome : public ObjDef, public NodeResolver {
public:
	ObjDef *clone() const;

	content_t
		c_top         = CONTENT_IGNORE,
		c_filler      = CONTENT_IGNORE,
		c_stone       = CONTENT_IGNORE,
		c_water_top   = CONTENT_IGNORE,
		c_water       = CONTENT_IGNORE,
		c_river_water = CONTENT_IGNORE,
		c_riverbed    = CONTENT_IGNORE,
		c_dust        = CONTENT_IGNORE;
	std::vector<content_t> c_cave_liquid;
	content_t
		c_dungeon       = CONTENT_IGNORE,
		c_dungeon_alt   = CONTENT_IGNORE,
		c_dungeon_stair = CONTENT_IGNORE;

	pos_t depth_top       = 0;
	pos_t depth_filler    = -MAX_MAP_GENERATION_LIMIT;
	pos_t depth_water_top = 0;
	pos_t depth_riverbed  = 0;

	v3pos_t min_pos = -MAX_MAP_GENERATION_LIMIT_V3;
	v3pos_t max_pos =  MAX_MAP_GENERATION_LIMIT_V3;
	float heat_point     = 0.0f;
	float humidity_point = 0.0f;
	pos_t vertical_blend = 0;
	float weight = 1.0f;

	//freeminer:
	content_t c_ice;
	content_t c_top_cold;

	virtual void resolveNodeNames();
};


////
//// BiomeGen
////

enum BiomeGenType {
	BIOMEGEN_ORIGINAL,
};

struct BiomeParams {
	virtual void readParams(const Settings *settings) = 0;
	virtual void writeParams(Settings *settings) const = 0;
	virtual ~BiomeParams() = default;

	s32 seed;
};

// WARNING: this class is not thread-safe
class BiomeGen {
public:
	virtual ~BiomeGen() = default;

	virtual BiomeGenType getType() const = 0;

	// Clone this BiomeGen and set a the new BiomeManager to be used by the copy
	virtual BiomeGen *clone(BiomeManager *biomemgr) const = 0;

	// Check that the internal chunk size is what the mapgen expects, just to be sure.
	inline void assertChunkSize(v3pos_t expect) const
	{
		FATAL_ERROR_IF(m_csize != expect, "Chunk size mismatches");
	}

	// Calculates the biome at the exact position provided.  This function can
	// be called at any time, but may be less efficient than the latter methods,
	// depending on implementation.
	virtual Biome *calcBiomeAtPoint(v3pos_t pos) const = 0;

	// Computes any intermediate results needed for biome generation.  Must be
	// called before using any of: getBiomes, getBiomeAtPoint, or getBiomeAtIndex.
	// Calling this invalidates the previous results stored in biomemap.
	virtual void calcBiomeNoise(v3pos_t pmin) = 0;

	// Gets all biomes in current chunk using each corresponding element of
	// heightmap as the y position, then stores the results by biome index in
	// biomemap (also returned)
	virtual biome_t *getBiomes(pos_t *heightmap, v3pos_t pmin) = 0;

	// Gets a single biome at the specified position, which must be contained
	// in the region formed by m_pmin and (m_pmin + m_csize - 1).
	virtual Biome *getBiomeAtPoint(v3pos_t pos) const = 0;

	// Same as above, but uses a raw numeric index correlating to the (x,z) position.
	virtual Biome *getBiomeAtIndex(size_t index, v3pos_t pos) const = 0;

	// Returns the next lower y position at which the biome could change.
	// You can use this to optimize calls to getBiomeAtIndex().
	virtual pos_t getNextTransitionY(pos_t y) const {
		return y == S16_MIN ? y : (y - 1);
	};

	// Result of calcBiomes bulk computation.
	biome_t *biomemap = nullptr;

protected:
	BiomeManager *m_bmgr = nullptr;
	v3pos_t m_pmin;
	v3pos_t m_csize;
};


////
//// BiomeGen implementations
////

//
// Original biome algorithm (Whittaker's classification + surface height)
//

struct BiomeParamsOriginal : public BiomeParams {
	BiomeParamsOriginal() :
		np_heat(15, 30, v3f(1000.0, 1000.0, 1000.0), 5349, 3, 0.5, 2.0),
		np_humidity(50, 50, v3f(1000.0, 1000.0, 1000.0), 842, 3, 0.5, 2.0),
		np_heat_blend(0, 1.5, v3f(8.0, 8.0, 8.0), 13, 2, 1.0, 2.0),
		np_humidity_blend(0, 1.5, v3f(8.0, 8.0, 8.0), 90003, 2, 1.0, 2.0)
	{
	}

	virtual void readParams(const Settings *settings);
	virtual void writeParams(Settings *settings) const;

	NoiseParams np_heat;
	NoiseParams np_humidity;
	NoiseParams np_heat_blend;
	NoiseParams np_humidity_blend;
};

class BiomeGenOriginal final : public BiomeGen {
public:
	BiomeGenOriginal(BiomeManager *biomemgr,
		const BiomeParamsOriginal *params, v3pos_t chunksize);
	virtual ~BiomeGenOriginal();

	BiomeGenType getType() const { return BIOMEGEN_ORIGINAL; }

	BiomeGen *clone(BiomeManager *biomemgr) const;

	// Slower, meant for Script API use
	float calcHeatAtPoint(v3pos_t pos) const;
	float calcHumidityAtPoint(v3pos_t pos) const;
	Biome *calcBiomeAtPoint(v3pos_t pos) const;

	void calcBiomeNoise(v3pos_t pmin);

	biome_t *getBiomes(pos_t *heightmap, v3pos_t pmin);
	Biome *getBiomeAtPoint(v3pos_t pos) const;
	Biome *getBiomeAtIndex(size_t index, v3pos_t pos) const;

	Biome *calcBiomeFromNoise(float heat, float humidity, v3pos_t pos) const;
	pos_t getNextTransitionY(pos_t y) const;

	float *heatmap;
	float *humidmap;

private:
	const BiomeParamsOriginal *m_params;

	Noise *noise_heat;
	Noise *noise_humidity;
	Noise *noise_heat_blend;
	Noise *noise_humidity_blend;

	/// Y values at which biomes may transition.
	/// This array may only be used for downwards scanning!
	std::vector<pos_t> m_transitions_y;
};


////
//// BiomeManager
////

class BiomeManager : public ObjDefManager {
public:
	BiomeManager(Server *server);
	virtual ~BiomeManager() = default;

	BiomeManager *clone() const;

	const char *getObjectTitle() const
	{
		return "biome";
	}

	static Biome *create(BiomeType type)
	{
		return new Biome;
	}

	//freeminer:
	u32 year_days;
	s32 weather_heat_season;
	s32 weather_heat_width;
	s32 weather_heat_daily;
	s32 weather_heat_height;
	s32 weather_humidity_season;
	s32 weather_humidity_width;
	s32 weather_humidity_daily;
	s32 weather_humidity_days;
	s32 weather_humidity_height;
	s32 weather_hot_core;

	MapgenParams * mapgen_params{};
	weather::heat_t calcBlockHeat(const v3pos_t &p, uint64_t seed, float timeofday, float totaltime, bool use_weather = 1);
	weather::humidity_t calcBlockHumidity(const v3pos_t &p, uint64_t seed, float timeofday, float totaltime, bool use_weather = 1);
	//====

	BiomeGen *createBiomeGen(BiomeGenType type, BiomeParams *params, v3pos_t chunksize)
	{
		switch (type) {
		case BIOMEGEN_ORIGINAL:
			return new BiomeGenOriginal(this,
				(BiomeParamsOriginal *)params, chunksize);
		default:
			return NULL;
		}
	}

	static BiomeParams *createBiomeParams(BiomeGenType type)
	{
		switch (type) {
		case BIOMEGEN_ORIGINAL:
			return new BiomeParamsOriginal;
		default:
			return NULL;
		}
	}

	virtual void clear();

private:
	BiomeManager() {};

	Server *m_server;

};
