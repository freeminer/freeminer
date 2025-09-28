// Luanti
// SPDX-License-Identifier: LGPL-2.1-or-later
// Copyright (C) 2014-2018 kwolekr, Ryan Kwolek <kwolekr@minetest.net>
// Copyright (C) 2014-2018 paramat

#include "mg_biome.h"
#include "constants.h"
#include "mg_decoration.h"
#include "emerge.h"
#include "server.h"
#include "nodedef.h"
#include "map.h" //for MMVManip
#include "log_types.h"
#include "util/numeric.h"
#include "porting.h"
#include "settings.h"

#include <algorithm>

///////////////////////////////////////////////////////////////////////////////


BiomeManager::BiomeManager(Server *server) :
	ObjDefManager(server, OBJDEF_BIOME)
{
	m_server = server;

	// Create default biome to be used in case none exist
	Biome *b = new Biome;
	b->name            = "default";

	b->m_nodenames.emplace_back("mapgen_stone");
	b->m_nodenames.emplace_back("mapgen_stone");
	b->m_nodenames.emplace_back("mapgen_stone");
	b->m_nodenames.emplace_back("mapgen_water_source");
	b->m_nodenames.emplace_back("mapgen_water_source");
	b->m_nodenames.emplace_back("mapgen_river_water_source");

	//freeminer
	b->m_nodenames.emplace_back("mapgen_ice");
	b->m_nodenames.emplace_back("mapgen_dirt_with_snow");
	//========


	b->m_nodenames.emplace_back("mapgen_stone");
	b->m_nodenames.emplace_back("ignore");
	b->m_nodenames.emplace_back("ignore");
	b->m_nnlistsizes.push_back(1);
	b->m_nodenames.emplace_back("ignore");
	b->m_nodenames.emplace_back("ignore");
	b->m_nodenames.emplace_back("ignore");
	m_ndef->pendNodeResolve(b);

	year_days = g_settings->getS16("year_days");
	weather_heat_season = g_settings->getS16("weather_heat_season");
	weather_heat_daily = g_settings->getS16("weather_heat_daily");
	weather_heat_width = g_settings->getPos("weather_heat_width");
	weather_heat_height = g_settings->getPos("weather_heat_height");
	weather_humidity_season = g_settings->getS16("weather_humidity_season");
	weather_humidity_daily = g_settings->getS16("weather_humidity_daily");
	weather_humidity_width = g_settings->getPos("weather_humidity_width");
	weather_humidity_days = g_settings->getS16("weather_humidity_days");
	weather_humidity_height = g_settings->getS16("weather_humidity_height");
	weather_hot_core = g_settings->getPos("weather_hot_core");
	if (add(b) == OBJDEF_INVALID_HANDLE)
		delete b;
}


void BiomeManager::clear()
{
	EmergeManager *emerge = m_server->getEmergeManager();

	if (emerge) {
		// Remove all dangling references in Decorations
		DecorationManager *decomgr = emerge->getWritableDecorationManager();
		for (size_t i = 0; i != decomgr->getNumObjects(); i++) {
			Decoration *deco = (Decoration *)decomgr->getRaw(i);
			deco->biomes.clear();
		}
	}

	// Don't delete the first biome
	for (size_t i = 1; i < m_objects.size(); i++)
		delete (Biome *)m_objects[i];

	m_objects.resize(1);
	//m_objects.clear(); // WTF 1 ????
}


BiomeManager *BiomeManager::clone() const
{
	auto mgr = new BiomeManager(m_server);
	assert(mgr);
	ObjDefManager::cloneTo(mgr);
	mgr->m_server = m_server;
	mgr->mapgen_params = mapgen_params;
	return mgr;
}

////////////////////////////////////////////////////////////////////////////////

void BiomeParamsOriginal::readParams(const Settings *settings)
{
	settings->getNoiseParams("mg_biome_np_heat",           np_heat);
	settings->getNoiseParams("mg_biome_np_heat_blend",     np_heat_blend);
	settings->getNoiseParams("mg_biome_np_humidity",       np_humidity);
	settings->getNoiseParams("mg_biome_np_humidity_blend", np_humidity_blend);
}


void BiomeParamsOriginal::writeParams(Settings *settings) const
{
	settings->setNoiseParams("mg_biome_np_heat",           np_heat);
	settings->setNoiseParams("mg_biome_np_heat_blend",     np_heat_blend);
	settings->setNoiseParams("mg_biome_np_humidity",       np_humidity);
	settings->setNoiseParams("mg_biome_np_humidity_blend", np_humidity_blend);
}


////////////////////////////////////////////////////////////////////////////////

BiomeGenOriginal::BiomeGenOriginal(BiomeManager *biomemgr,
	const BiomeParamsOriginal *params, v3pos_t chunksize)
{
	m_bmgr   = biomemgr;
	m_params = params;
	m_csize  = chunksize;

	noise_heat           = new Noise(&params->np_heat,
									params->seed, m_csize.X, m_csize.Z);
	noise_humidity       = new Noise(&params->np_humidity,
									params->seed, m_csize.X, m_csize.Z);
	noise_heat_blend     = new Noise(&params->np_heat_blend,
									params->seed, m_csize.X, m_csize.Z);
	noise_humidity_blend = new Noise(&params->np_humidity_blend,
									params->seed, m_csize.X, m_csize.Z);

	heatmap  = noise_heat->result;
	humidmap = noise_humidity->result;

	biomemap = new biome_t[m_csize.X * m_csize.Z];
	// Initialise with the ID of 'BIOME_NONE' so that cavegen can get the
	// fallback biome when biome generation (which calculates the biomemap IDs)
	// is disabled.
	memset(biomemap, 0, sizeof(biome_t) * m_csize.X * m_csize.Z);

	// Calculate cache of Y transition points
	std::vector<pos_t> values;
	values.reserve(m_bmgr->getNumObjects() * 2);
	for (size_t i = 0; i < m_bmgr->getNumObjects(); i++) {
		Biome *b = (Biome *)m_bmgr->getRaw(i);
		values.push_back(b->max_pos.Y);
		// We scan for biomes from high Y to low Y (top to bottom). Hence,
		// biomes effectively transition at (min_pos.Y - 1).
		if (b->min_pos.Y > -MAX_MAP_GENERATION_LIMIT)
			values.push_back(b->min_pos.Y - 1);
	}

	std::sort(values.begin(), values.end(), std::greater<>());
	values.erase(std::unique(values.begin(), values.end()), values.end());

	m_transitions_y = std::move(values);
}

BiomeGenOriginal::~BiomeGenOriginal()
{
	delete []biomemap;

	delete noise_heat;
	delete noise_humidity;
	delete noise_heat_blend;
	delete noise_humidity_blend;
}

pos_t BiomeGenOriginal::getNextTransitionY(pos_t y) const
{
	// Find first value that is less than y using binary search
	auto it = std::lower_bound(m_transitions_y.begin(), m_transitions_y.end(), y, std::greater_equal<>());
	return (it == m_transitions_y.end()) ? S16_MIN : *it;
}

BiomeGen *BiomeGenOriginal::clone(BiomeManager *biomemgr) const
{
	return new BiomeGenOriginal(biomemgr, m_params, m_csize);
}

float BiomeGenOriginal::calcHeatAtPoint(v3pos_t pos) const
{
	return NoiseFractal2D(&m_params->np_heat, pos.X, pos.Z, m_params->seed) +
		NoiseFractal2D(&m_params->np_heat_blend, pos.X, pos.Z, m_params->seed);
}

float BiomeGenOriginal::calcHumidityAtPoint(v3pos_t pos) const
{
	return NoiseFractal2D(&m_params->np_humidity, pos.X, pos.Z, m_params->seed) +
		NoiseFractal2D(&m_params->np_humidity_blend, pos.X, pos.Z, m_params->seed);
}

Biome *BiomeGenOriginal::calcBiomeAtPoint(v3pos_t pos) const
{
	return calcBiomeFromNoise(calcHeatAtPoint(pos), calcHumidityAtPoint(pos), pos);
}


void BiomeGenOriginal::calcBiomeNoise(v3pos_t pmin)
{
	m_pmin = pmin;

	noise_heat->noiseMap2D(pmin.X, pmin.Z);
	noise_humidity->noiseMap2D(pmin.X, pmin.Z);
	noise_heat_blend->noiseMap2D(pmin.X, pmin.Z);
	noise_humidity_blend->noiseMap2D(pmin.X, pmin.Z);

	for (s32 i = 0; i < m_csize.X * m_csize.Z; i++) {
		noise_heat->result[i]     += noise_heat_blend->result[i];
		noise_humidity->result[i] += noise_humidity_blend->result[i];
	}
}


biome_t *BiomeGenOriginal::getBiomes(pos_t *heightmap, v3pos_t pmin)
{
	for (s16 zr = 0; zr < m_csize.Z; zr++)
	for (s16 xr = 0; xr < m_csize.X; xr++) {
		s32 i = zr * m_csize.X + xr;
		Biome *biome = calcBiomeFromNoise(
			noise_heat->result[i],
			noise_humidity->result[i],
			v3pos_t(pmin.X + xr, heightmap[i], pmin.Z + zr));

		biomemap[i] = biome->index;
	}

	return biomemap;
}


Biome *BiomeGenOriginal::getBiomeAtPoint(v3pos_t pos) const
{
	return getBiomeAtIndex(
		(pos.Z - m_pmin.Z) * m_csize.X + (pos.X - m_pmin.X),
		pos);
}


Biome *BiomeGenOriginal::getBiomeAtIndex(size_t index, v3pos_t pos) const
{
	return calcBiomeFromNoise(
		noise_heat->result[index],
		noise_humidity->result[index],
		pos);
}


Biome *BiomeGenOriginal::calcBiomeFromNoise(float heat, float humidity, v3pos_t pos) const
{
	Biome *biome_closest = nullptr;
	Biome *biome_closest_blend = nullptr;
	float dist_min = FLT_MAX;
	float dist_min_blend = FLT_MAX;

	for (size_t i = 1; i < m_bmgr->getNumObjects(); i++) {
		Biome *b = (Biome *)m_bmgr->getRaw(i);
		if (!b ||
				pos.Y < b->min_pos.Y || pos.Y > b->max_pos.Y + b->vertical_blend ||
				pos.X < b->min_pos.X || pos.X > b->max_pos.X ||
				pos.Z < b->min_pos.Z || pos.Z > b->max_pos.Z)
			continue;
		float heat_point = (b->heat_point - 50) * (( m_params->np_heat.offset + m_params->np_heat.scale ) / 100)
			 + m_params->np_heat.offset;

		float d_heat = heat - heat_point;
		float d_humidity = humidity - b->humidity_point;
		float dist = ((d_heat * d_heat) + (d_humidity * d_humidity));
		if (b->weight > 0.f)
		       dist /= b->weight;

		if (pos.Y <= b->max_pos.Y) { // Within y limits of biome b
			if (dist < dist_min) {
				dist_min = dist;
				biome_closest = b;
			}
		} else if (dist < dist_min_blend) { // Blend area above biome b
			dist_min_blend = dist;
			biome_closest_blend = b;
		}
	}

	// Carefully tune pseudorandom seed variation to avoid single node dither
	// and create larger scale blending patterns similar to horizontal biome
	// blend.
	// The calculation can be a negative floating point number, which is an
	// undefined behavior if assigned to unsigned integer. Cast the result
	// into signed integer before it is casted into unsigned integer to
	// eliminate the undefined behavior.
	const u64 seed = static_cast<s64>(pos.Y + (heat + humidity) * 0.9f);
	PcgRandom rng(seed);

	if (biome_closest_blend && dist_min_blend <= dist_min &&
			rng.range(0, biome_closest_blend->vertical_blend) >=
			pos.Y - biome_closest_blend->max_pos.Y)
		return biome_closest_blend;

	return (biome_closest) ? biome_closest : (Biome *)m_bmgr->getRaw(BIOME_NONE);
}

// Freeminer Weather
weather::heat_t BiomeManager::calcBlockHeat(const v3pos_t &p, uint64_t seed, float timeofday, float totaltime, bool use_weather) {
	//variant 1: full random
	//f32 heat = NoisePerlin3D(np_heat, p.X, env->getGameTime()/100, p.Z, seed);

	//variant 2: season change based on default heat map
	auto heat = NoiseFractal2D(&(mapgen_params->bparams->np_heat), p.X, p.Z, seed); // -30..20..70
	// auto heat =calcHeatAtPoint(p);

	if (use_weather) {
		f32 seasonv = totaltime;
		seasonv /= 86400 * year_days; // season change speed
		seasonv += (f32)p.X / weather_heat_width; // you can walk to area with other season
		seasonv = sin(seasonv * M_PI);
		//heat += (weather_heat_season * (heat < offset ? 2 : 0.5)) * seasonv; // -60..0..30
		heat += (weather_heat_season) * seasonv; // -60..0..30

		// daily change, hotter at sun +4, colder at night -4
		heat += weather_heat_daily * (sin(cycle_shift(timeofday, -0.25) * M_PI) - 0.5); //-64..0..34
	}
	heat += p.Y / weather_heat_height; // upper=colder, lower=hotter, 3c per 1000

	if (weather_hot_core && p.Y < -(WEATHER_LIMIT-weather_hot_core))
		heat += 6000 * (1.0-((float)(p.Y - -WEATHER_LIMIT)/weather_hot_core)); //hot core, later via realms

	return heat;
}

weather::humidity_t BiomeManager::calcBlockHumidity(const v3pos_t &p, uint64_t seed, float timeofday, float totaltime, bool use_weather) {

	auto humidity = NoiseFractal2D(&(mapgen_params->bparams->np_humidity), p.X, p.Z, seed);
	// auto humidity = calcHumidityAtPoint(p);

	if (use_weather) {
		f32 seasonv = totaltime;
		seasonv /= 86400 * weather_humidity_days; // bad weather change speed (2 days)
		seasonv += (f32)p.Z / weather_humidity_width;
		humidity += weather_humidity_season * sin(seasonv * M_PI);
		humidity += weather_humidity_daily * (sin(cycle_shift(timeofday, -0.1) * M_PI) - 0.5);
	}
	humidity += p.Y / weather_humidity_height; // upper=dry, lower=wet, 3c per 1000

	humidity = rangelim(humidity, 0, 100);

	return humidity;
}
//===============fm end


////////////////////////////////////////////////////////////////////////////////

ObjDef *Biome::clone() const
{
	auto obj = new Biome();
	ObjDef::cloneTo(obj);
	NodeResolver::cloneTo(obj);

	// fm:
	obj->c_top_cold = c_top_cold;
	obj->c_ice = c_ice;
	// ===

	obj->c_top = c_top;
	obj->c_filler = c_filler;
	obj->c_stone = c_stone;
	obj->c_water_top = c_water_top;
	obj->c_water = c_water;
	obj->c_river_water = c_river_water;
	obj->c_riverbed = c_riverbed;
	obj->c_dust = c_dust;
	obj->c_cave_liquid = c_cave_liquid;
	obj->c_dungeon = c_dungeon;
	obj->c_dungeon_alt = c_dungeon_alt;
	obj->c_dungeon_stair = c_dungeon_stair;

	obj->depth_top = depth_top;
	obj->depth_filler = depth_filler;
	obj->depth_water_top = depth_water_top;
	obj->depth_riverbed = depth_riverbed;

	obj->min_pos = min_pos;
	obj->max_pos = max_pos;
	obj->heat_point = heat_point;
	obj->humidity_point = humidity_point;
	obj->vertical_blend = vertical_blend;
	obj->weight = weight;

	return obj;
}

void Biome::resolveNodeNames()
{
	getIdFromNrBacklog(&c_top,           "mapgen_stone",              CONTENT_AIR,    false);
	getIdFromNrBacklog(&c_filler,        "mapgen_stone",              CONTENT_AIR,    false);
	getIdFromNrBacklog(&c_stone,         "mapgen_stone",              CONTENT_AIR,    false);
	getIdFromNrBacklog(&c_water,         "mapgen_water_source",       CONTENT_AIR,    false);
	getIdFromNrBacklog(&c_water_top,     "mapgen_water_source",       c_water,    false);
	getIdFromNrBacklog(&c_river_water,   "mapgen_river_water_source", c_water,    false);

	//freeminer:
	getIdFromNrBacklog(&c_ice,           "mapgen_ice",                c_water,        false);
	getIdFromNrBacklog(&c_top_cold,      "mapgen_dirt_with_snow",     c_top,          false);
	//==========

	getIdFromNrBacklog(&c_riverbed,      "mapgen_stone",              CONTENT_AIR,    false);
	getIdFromNrBacklog(&c_dust,          "ignore",                    CONTENT_IGNORE, false);
	getIdsFromNrBacklog(&c_cave_liquid);
	getIdFromNrBacklog(&c_dungeon,       "ignore",                    CONTENT_IGNORE, false);
	getIdFromNrBacklog(&c_dungeon_alt,   "ignore",                    CONTENT_IGNORE, false);
	getIdFromNrBacklog(&c_dungeon_stair, "ignore",                    CONTENT_IGNORE, false);
}
