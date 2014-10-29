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

#include "biome.h"
#include "nodedef.h"
#include "map.h" //for ManualMapVoxelManipulator
#include "log_types.h"
#include "util/numeric.h"
#include "main.h"
#include "util/mathconstants.h"
#include "porting.h"
#include "settings.h"

NoiseParams nparams_biome_def_heat(15, 30, v3f(500.0, 500.0, 500.0), 5349, 2, 0.65);
NoiseParams nparams_biome_def_humidity(50, 50, v3f(500.0, 500.0, 500.0), 842, 3, 0.50);

BiomeDefManager::BiomeDefManager(NodeResolver *resolver)
{
	biome_registration_finished = false;
	np_heat     = &nparams_biome_def_heat;
	np_humidity = &nparams_biome_def_humidity;

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

	resolver->addNode("air",                 "", CONTENT_AIR, &b->c_top);
	resolver->addNode("air",                 "", CONTENT_AIR, &b->c_filler);
	resolver->addNode("mapgen_water_source", "", CONTENT_AIR, &b->c_water);
	resolver->addNode("air",                 "", CONTENT_AIR, &b->c_dust);
	resolver->addNode("mapgen_water_source", "", CONTENT_AIR, &b->c_dust_water);
	resolver->addNode("mapgen_ice",          "mapgen_water_source", b->c_water, &b->c_ice);

	biomes.push_back(b);

	g_settings->getNoiseParams("mgv7_np_heat", nparams_biome_def_heat);
	g_settings->getNoiseParams("mgv7_np_humidity", nparams_biome_def_humidity);
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
}


BiomeDefManager::~BiomeDefManager()
{
	//if (biomecache)
	//	delete[] biomecache;
	
	for (size_t i = 0; i != biomes.size(); i++)
		delete biomes[i];
}


Biome *BiomeDefManager::createBiome(BiomeTerrainType btt)
{
	/*switch (btt) {
		case BIOME_TERRAIN_NORMAL:
			return new Biome;
		case BIOME_TERRAIN_LIQUID:
			return new BiomeLiquid;
		case BIOME_TERRAIN_NETHER:
			return new BiomeHell;
		case BIOME_TERRAIN_AETHER:
			return new BiomeSky;
		case BIOME_TERRAIN_FLAT:
			return new BiomeSuperflat;
	}
	return NULL;*/
	return new Biome;
}


// just a PoC, obviously needs optimization later on (precalculate this)
void BiomeDefManager::calcBiomes(BiomeNoiseInput *input, u8 *biomeid_map)
{
	int i = 0;
	for (int y = 0; y != input->mapsize.Y; y++) {
		for (int x = 0; x != input->mapsize.X; x++, i++) {
			float heat     = (input->heat_map[i] + 1) * 50;
			float humidity = (input->humidity_map[i] + 1) * 50;
			biomeid_map[i] = getBiome(heat, humidity, input->height_map[i])->id;
		}
	}
}


bool BiomeDefManager::addBiome(Biome *b)
{
	if (biome_registration_finished) {
		errorstream << "BiomeDefManager: biome registration already "
			"finished, dropping " << b->name << std::endl;
		return false;
	}
	
	size_t nbiomes = biomes.size();
	if (nbiomes >= 0xFF) {
		errorstream << "BiomeDefManager: too many biomes, dropping "
			<< b->name << std::endl;
		return false;
	}

	b->id = (u8)nbiomes;
	biomes.push_back(b);
	verbosestream << "BiomeDefManager: added biome " << b->name << std::endl;

	return true;
}


Biome *BiomeDefManager::getBiome(float heat, float humidity, s16 y)
{
	Biome *b, *biome_closest = NULL;
	float dist_min = FLT_MAX;

	for (size_t i = 1; i < biomes.size(); i++) {
		b = biomes[i];
		if (y > b->height_max || y < b->height_min)
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
	
	return biome_closest ? biome_closest : biomes[0];
}


u8 BiomeDefManager::getBiomeIdByName(const char *name)
{
	for (size_t i = 0; i != biomes.size(); i++) {
		if (!strcasecmp(name, biomes[i]->name.c_str()))
			return i;
	}
	
	return 0;
}


///////////////////////////// Weather


s16 BiomeDefManager::calcBlockHeat(v3s16 p, u64 seed, float timeofday, float totaltime, bool use_weather) {
	//variant 1: full random
	//f32 heat = NoisePerlin3D(np_heat, p.X, env->getGameTime()/100, p.Z, seed);

	//variant 2: season change based on default heat map
	f32 heat = NoisePerlin2D(np_heat, p.X, p.Z, seed); // -30..20..70

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


s16 BiomeDefManager::calcBlockHumidity(v3s16 p, u64 seed, float timeofday, float totaltime, bool use_weather) {

	f32 humidity = NoisePerlin2D(np_humidity, p.X, p.Z, seed);

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
