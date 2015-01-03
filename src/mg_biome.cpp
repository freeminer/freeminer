/*
biome.cpp
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

#include "mg_biome.h"
#include "gamedef.h"
#include "nodedef.h"
#include "map.h" //for ManualMapVoxelManipulator
#include "log_types.h"
#include "util/numeric.h"
#include "main.h"
#include "util/mathconstants.h"
#include "porting.h"
#include "settings.h"

const char *BiomeManager::ELEMENT_TITLE = "biome";


///////////////////////////////////////////////////////////////////////////////

BiomeManager::BiomeManager(IGameDef *gamedef) :
	GenElementManager(gamedef)
{
	// Create default biome to be used in case none exist
	Biome *b = new Biome;

	b->id             = 0;
	b->name           = "Default";
	b->flags          = 0;
	b->depth_top      = 0;
	b->depth_filler   = 0;
	b->height_min     = -MAP_GENERATION_LIMIT;
	b->height_max     = MAP_GENERATION_LIMIT;
	b->heat_point     = 0.0;
	b->humidity_point = 0.0;

	NodeResolveInfo *nri = new NodeResolveInfo(b);
	nri->nodenames.push_back("air");
	nri->nodenames.push_back("air");
	nri->nodenames.push_back("mapgen_stone");
	nri->nodenames.push_back("mapgen_water_source");
	nri->nodenames.push_back("air");
	nri->nodenames.push_back("mapgen_water_source");

	nri->nodenames.push_back("mapgen_ice");
	m_ndef->pendNodeResolve(nri);

	year_days = g_settings->getS16("year_days");
	weather_heat_season = g_settings->getS16("weather_heat_season");
	weather_heat_daily = g_settings->getS16("weather_heat_daily");
	weather_heat_width = g_settings->getS16("weather_heat_width");
	weather_heat_height = g_settings->getS16("weather_heat_height");
	weather_humidity_season = g_settings->getS16("weather_humidity_season");
	weather_humidity_daily = g_settings->getS16("weather_humidity_daily");
	weather_humidity_width = g_settings->getS16("weather_humidity_width");
	weather_humidity_days = g_settings->getS16("weather_humidity_days");
	weather_hot_core = g_settings->getS16("weather_hot_core");

	add(b);
}



BiomeManager::~BiomeManager()
{
	//if (biomecache)
	//	delete[] biomecache;
}


// just a PoC, obviously needs optimization later on (precalculate this)
void BiomeManager::calcBiomes(s16 sx, s16 sy, float *heat_map,
	float *humidity_map, s16 *height_map, u8 *biomeid_map)
{
	for (s32 i = 0; i != sx * sy; i++)
		biomeid_map[i] = getBiome(heat_map[i], humidity_map[i], height_map[i])->id;
}


Biome *BiomeManager::getBiome(float heat, float humidity, s16 y)
{
	Biome *b, *biome_closest = NULL;
	float dist_min = FLT_MAX;

	for (size_t i = 1; i < m_elements.size(); i++) {
		b = (Biome *)m_elements[i];
		if (!b || y > b->height_max || y < b->height_min)
			continue;

		float d_heat     = heat     - b->heat_point;
		float d_humidity = humidity - b->humidity_point;
		float dist = (d_heat * d_heat) +
					 (d_humidity * d_humidity);
		if (dist < dist_min) {
			dist_min = dist;
			biome_closest = b;
		}
	}

	return biome_closest ? biome_closest : (Biome *)m_elements[0];
}

// Freeminer Weather

s16 BiomeManager::calcBlockHeat(v3POS p, uint64_t seed, float timeofday, float totaltime, bool use_weather) {
	//variant 1: full random
	//f32 heat = NoisePerlin3D(np_heat, p.X, env->getGameTime()/100, p.Z, seed);

	//variant 2: season change based on default heat map
	auto heat = NoisePerlin2D(&(mapgen_params->np_biome_heat), p.X, p.Z, seed); // -30..20..70

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

	if (weather_hot_core && p.Y < -(MAP_GENERATION_LIMIT-weather_hot_core))
		heat += 6000 * (1-((float)(p.Y - -MAP_GENERATION_LIMIT)/weather_hot_core)); //hot core, later via realms

	return heat;
}


s16 BiomeManager::calcBlockHumidity(v3POS p, uint64_t seed, float timeofday, float totaltime, bool use_weather) {

	auto humidity = NoisePerlin2D(&(mapgen_params->np_biome_humidity), p.X, p.Z, seed);

	if (use_weather) {
		f32 seasonv = totaltime;
		seasonv /= 86400 * weather_humidity_days; // bad weather change speed (2 days)
		seasonv += (f32)p.Z / weather_humidity_width;
		humidity += weather_humidity_season * sin(seasonv * M_PI);
		humidity += weather_humidity_daily * (sin(cycle_shift(timeofday, -0.1) * M_PI) - 0.5);
	}

	humidity = rangelim(humidity, 0, 100);

	return humidity;
}


void BiomeManager::clear()
{

	for (size_t i = 1; i < m_elements.size(); i++) {
		Biome *b = (Biome *)m_elements[i];
		if (!b)
			continue;
		delete b;
	}

	m_elements.resize(1);
}


///////////////////////////////////////////////////////////////////////////////


void Biome::resolveNodeNames(NodeResolveInfo *nri)
{
	m_ndef->getIdFromResolveInfo(nri, "mapgen_dirt_with_grass", CONTENT_AIR,    c_top);
	m_ndef->getIdFromResolveInfo(nri, "mapgen_dirt",            CONTENT_AIR,    c_filler);
	m_ndef->getIdFromResolveInfo(nri, "mapgen_stone",           CONTENT_AIR,    c_stone);
	m_ndef->getIdFromResolveInfo(nri, "mapgen_water_source",    CONTENT_AIR,    c_water);
	m_ndef->getIdFromResolveInfo(nri, "air",                    CONTENT_IGNORE, c_dust);
	m_ndef->getIdFromResolveInfo(nri, "mapgen_water_source",    CONTENT_IGNORE, c_dust_water);

	m_ndef->getIdFromResolveInfo(nri, "ice",                    CONTENT_IGNORE, c_ice);
}

