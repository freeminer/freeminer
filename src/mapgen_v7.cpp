/*
mapgen_v7.cpp
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


#include "mapgen.h"
#include "voxel.h"
#include "noise.h"
#include "mapblock.h"
#include "mapnode.h"
#include "map.h"
#include "content_sao.h"
#include "nodedef.h"
#include "voxelalgorithms.h"
#include "profiler.h"
#include "settings.h" // For g_settings
#include "main.h" // For g_profiler
#include "emerge.h"
#include "dungeongen.h"
#include "cavegen.h"
#include "treegen.h"
#include "mg_biome.h"
#include "mg_ore.h"
#include "mg_decoration.h"
#include "mapgen_v7.h"
#include "mapgen_indev.h" //farscale
#include "environment.h"
#include "log_types.h"



FlagDesc flagdesc_mapgen_v7[] = {
	{"mountains", MGV7_MOUNTAINS},
	{"ridges",    MGV7_RIDGES},
	{NULL,        0}
};

///////////////////////////////////////////////////////////////////////////////


MapgenV7::MapgenV7(int mapgenid, MapgenParams *params, EmergeManager *emerge)
	: Mapgen(mapgenid, params, emerge)
	, Mapgen_features(mapgenid, params, emerge)
{
	this->m_emerge = emerge;
	this->bmgr     = emerge->biomemgr;

	//// amount of elements to skip for the next index
	//// for noise/height/biome maps (not vmanip)
	this->ystride = csize.X;
	this->zstride = csize.X * csize.Y;

	this->biomemap  = new u8[csize.X * csize.Z];
	this->heightmap = new s16[csize.X * csize.Z];
	this->ridge_heightmap = new s16[csize.X * csize.Z];

	MapgenV7Params *sp = (MapgenV7Params *)params->sparams;
	this->spflags = sp->spflags;

	//// Terrain noise
	noise_terrain_base    = new Noise(&sp->np_terrain_base,    seed, csize.X, csize.Z);
	noise_terrain_alt     = new Noise(&sp->np_terrain_alt,     seed, csize.X, csize.Z);
	noise_terrain_persist = new Noise(&sp->np_terrain_persist, seed, csize.X, csize.Z);
	noise_height_select   = new Noise(&sp->np_height_select,   seed, csize.X, csize.Z);
	noise_filler_depth    = new Noise(&sp->np_filler_depth,    seed, csize.X, csize.Z);
	noise_mount_height    = new Noise(&sp->np_mount_height,    seed, csize.X, csize.Z);
	noise_ridge_uwater    = new Noise(&sp->np_ridge_uwater,    seed, csize.X, csize.Z);

	//// 3d terrain noise
	noise_mountain = new Noise(&sp->np_mountain, seed, csize.X, csize.Y, csize.Z);
	noise_ridge    = new Noise(&sp->np_ridge,    seed, csize.X, csize.Y, csize.Z);

	//// Biome noise
	noise_heat     = new Noise(&params->np_biome_heat,     seed, csize.X, csize.Z);
	noise_humidity = new Noise(&params->np_biome_humidity, seed, csize.X, csize.Z);

	//// Resolve nodes to be used
	INodeDefManager *ndef = emerge->ndef;

	c_stone           = ndef->getId("mapgen_stone");
	c_dirt            = ndef->getId("mapgen_dirt");
	c_dirt_with_grass = ndef->getId("mapgen_dirt_with_grass");
	c_sand            = ndef->getId("mapgen_sand");
	c_water_source    = ndef->getId("mapgen_water_source");
	c_lava_source     = ndef->getId("mapgen_lava_source");
	c_ice             = ndef->getId("default:ice");
	if (c_ice == CONTENT_IGNORE)
		c_ice = c_water_source;

	//freeminer:
	c_dirt_with_snow  = ndef->getId("mapgen_dirt_with_snow");
	if (c_dirt_with_snow == CONTENT_IGNORE)
		c_dirt_with_snow = c_dirt;
	float_islands = sp->float_islands;
	noise_float_islands1  = new Noise(&sp->np_float_islands1, seed, csize.X, csize.Y, csize.Z);
	noise_float_islands2  = new Noise(&sp->np_float_islands2, seed, csize.X, csize.Y, csize.Z);
	noise_float_islands3  = new Noise(&sp->np_float_islands3, seed, csize.X, csize.Z);

	noise_layers          = new Noise(&sp->np_layers,         seed, csize.X, csize.Y, csize.Z);
	layers_init(emerge, sp->paramsj);
}


MapgenV7::~MapgenV7()
{
	delete noise_terrain_base;
	delete noise_terrain_persist;
	delete noise_height_select;
	delete noise_terrain_alt;
	delete noise_filler_depth;
	delete noise_mount_height;
	delete noise_ridge_uwater;
	delete noise_mountain;
	delete noise_ridge;

	delete noise_heat;
	delete noise_humidity;

	delete[] ridge_heightmap;
	delete[] heightmap;
	delete[] biomemap;
}


MapgenV7Params::MapgenV7Params()
{
	spflags = MGV7_MOUNTAINS | MGV7_RIDGES;

	np_terrain_base    = NoiseParams(4,    70,  v3f(300, 300, 300), 82341, 6, 0.7,  2.0);
	np_terrain_alt     = NoiseParams(4,    25,  v3f(600, 600, 600), 5934,  5, 0.6,  2.0);
	np_terrain_persist = NoiseParams(0.6,  0.1, v3f(500, 500, 500), 539,   3, 0.6,  2.0);
	np_height_select   = NoiseParams(-0.5, 1,   v3f(250, 250, 250), 4213,  5, 0.69, 2.0);
	np_filler_depth    = NoiseParams(0,    1.2, v3f(150, 150, 150), 261,   4, 0.7,  2.0);
	np_mount_height    = NoiseParams(100,  30,  v3f(500, 500, 500), 72449, 4, 0.6,  2.0);
	np_ridge_uwater    = NoiseParams(0,    1,   v3f(500, 500, 500), 85039, 4, 0.6,  2.0);
	np_mountain        = NoiseParams(0,    1,   v3f(250, 350, 250), 5333,  5, 0.68, 2.0);
	np_ridge           = NoiseParams(0,    1,   v3f(100, 100, 100), 6467,  4, 0.75, 2.0);

	float_islands = 500;
	np_float_islands1  = NoiseParams(0,    1,   v3f(256, 256, 256), 3683,  6, 0.6, 2.0, NOISE_FLAG_DEFAULTS, 1,   1.5);
	np_float_islands2  = NoiseParams(0,    1,   v3f(8,   8,   8  ), 9292,  2, 0.5, 2.0, NOISE_FLAG_DEFAULTS, 1,   1.5);
	np_float_islands3  = NoiseParams(0,    1,   v3f(256, 256, 256), 6412,  2, 0.5, 2.0, NOISE_FLAG_DEFAULTS, 1,   0.5);
	np_layers          = NoiseParams(500,  500, v3f(100, 50,  100), 3663,  5, 0.6, 2.0, NOISE_FLAG_DEFAULTS, 1,   5,   0.5);

}


void MapgenV7Params::readParams(Settings *settings)
{
	settings->getFlagStrNoEx("mgv7_spflags", spflags, flagdesc_mapgen_v7);

	settings->getNoiseParams("mgv7_np_terrain_base",    np_terrain_base);
	settings->getNoiseParams("mgv7_np_terrain_alt",     np_terrain_alt);
	settings->getNoiseParams("mgv7_np_terrain_persist", np_terrain_persist);
	settings->getNoiseParams("mgv7_np_height_select",   np_height_select);
	settings->getNoiseParams("mgv7_np_filler_depth",    np_filler_depth);
	settings->getNoiseParams("mgv7_np_mount_height",    np_mount_height);
	settings->getNoiseParams("mgv7_np_ridge_uwater",    np_ridge_uwater);
	settings->getNoiseParams("mgv7_np_mountain",        np_mountain);
	settings->getNoiseParams("mgv7_np_ridge",           np_ridge);

	settings->getS16NoEx("mg_float_islands", float_islands);
	settings->getNoiseParamsFromGroup("mg_np_float_islands1", np_float_islands1);
	settings->getNoiseParamsFromGroup("mg_np_float_islands2", np_float_islands2);
	settings->getNoiseParamsFromGroup("mg_np_float_islands3", np_float_islands3);
	settings->getNoiseParamsFromGroup("mg_np_layers",         np_layers);
	paramsj = settings->getJson("mg_params", paramsj);
}


void MapgenV7Params::writeParams(Settings *settings)
{
	settings->setFlagStr("mgv7_spflags", spflags, flagdesc_mapgen_v7, (u32)-1);

	settings->setNoiseParams("mgv7_np_terrain_base",    np_terrain_base);
	settings->setNoiseParams("mgv7_np_terrain_alt",     np_terrain_alt);
	settings->setNoiseParams("mgv7_np_terrain_persist", np_terrain_persist);
	settings->setNoiseParams("mgv7_np_height_select",   np_height_select);
	settings->setNoiseParams("mgv7_np_filler_depth",    np_filler_depth);
	settings->setNoiseParams("mgv7_np_mount_height",    np_mount_height);
	settings->setNoiseParams("mgv7_np_ridge_uwater",    np_ridge_uwater);
	settings->setNoiseParams("mgv7_np_mountain",        np_mountain);
	settings->setNoiseParams("mgv7_np_ridge",           np_ridge);

	settings->setS16("mg_float_islands", float_islands);
	settings->setNoiseParams("mg_np_float_islands1", np_float_islands1);
	settings->setNoiseParams("mg_np_float_islands2", np_float_islands2);
	settings->setNoiseParams("mg_np_float_islands3", np_float_islands3);
	settings->setNoiseParams("mg_np_layers",         np_layers);

	settings->setJson("mg_params", paramsj);
}


///////////////////////////////////////


int MapgenV7::getGroundLevelAtPoint(v2s16 p)
{
	// Base terrain calculation
	s16 y = baseTerrainLevelAtPoint(p.X, p.Y);

	// Ridge/river terrain calculation
	float width = 0.3;
	float uwatern = NoisePerlin2D(&noise_ridge_uwater->np, p.X, p.Y, seed) * 2;
	// actually computing the depth of the ridge is much more expensive;
	// if inside a river, simply guess
	if (uwatern >= -width && uwatern <= width)
		return water_level - 10;

	// Mountain terrain calculation
	int iters = 128; // don't even bother iterating more than 128 times..
	while (iters--) {
		//current point would have been air
		if (!getMountainTerrainAtPoint(p.X, y, p.Y))
			return y;

		y++;
	}

	return y;
}


void MapgenV7::makeChunk(BlockMakeData *data)
{
	assert(data->vmanip);
	assert(data->nodedef);
	assert(data->blockpos_requested.X >= data->blockpos_min.X &&
		   data->blockpos_requested.Y >= data->blockpos_min.Y &&
		   data->blockpos_requested.Z >= data->blockpos_min.Z);
	assert(data->blockpos_requested.X <= data->blockpos_max.X &&
		   data->blockpos_requested.Y <= data->blockpos_max.Y &&
		   data->blockpos_requested.Z <= data->blockpos_max.Z);

	this->generating = true;
	this->vm   = data->vmanip;
	this->ndef = data->nodedef;
	//TimeTaker t("makeChunk");

	v3s16 blockpos_min = data->blockpos_min;
	v3s16 blockpos_max = data->blockpos_max;
	node_min = blockpos_min * MAP_BLOCKSIZE;
	node_max = (blockpos_max + v3s16(1, 1, 1)) * MAP_BLOCKSIZE - v3s16(1, 1, 1);
	full_node_min = (blockpos_min - 1) * MAP_BLOCKSIZE;
	full_node_max = (blockpos_max + 2) * MAP_BLOCKSIZE - v3s16(1, 1, 1);

	blockseed = getBlockSeed2(full_node_min, seed);

	// Make some noise
	calculateNoise();

	if (float_islands && node_max.Y >= float_islands) {
		float_islands_prepare(node_min, node_max, float_islands);
	}

	layers_prepare(node_min, node_max);

	// Generate base terrain, mountains, and ridges with initial heightmaps
	s16 stone_surface_max_y = generateTerrain();

	updateHeightmap(node_min, node_max);

	// Calculate biomes
	bmgr->calcBiomes(csize.X, csize.Z, noise_heat->result,
		noise_humidity->result, heightmap, biomemap);

	// Actually place the biome-specific nodes and what not
	generateBiomes();

	generateExperimental();

	if (flags & MG_CAVES)
		generateCaves(stone_surface_max_y);

	if ((flags & MG_DUNGEONS) && (stone_surface_max_y >= node_min.Y)) {
		DungeonGen dgen(this, NULL);
		dgen.generate(blockseed, full_node_min, full_node_max);
	}

	// Generate the registered decorations
	m_emerge->decomgr->placeAllDecos(this, blockseed, node_min, node_max);

	// Generate the registered ores
	m_emerge->oremgr->placeAllOres(this, blockseed, node_min, node_max);

	// Sprinkle some dust on top after everything else was generated
	dustTopNodes();

	//printf("makeChunk: %dms\n", t.stop());

	updateLiquid(full_node_min, full_node_max);

	if (flags & MG_LIGHT)
		calcLighting(node_min - v3s16(1, 0, 1) * MAP_BLOCKSIZE,
					 node_max + v3s16(1, 0, 1) * MAP_BLOCKSIZE);
	//setLighting(node_min - v3s16(1, 0, 1) * MAP_BLOCKSIZE,
	//			node_max + v3s16(1, 0, 1) * MAP_BLOCKSIZE, 0xFF);

	this->generating = false;
}


void MapgenV7::calculateNoise()
{
	//TimeTaker t("calculateNoise", NULL, PRECISION_MICRO);
	int x = node_min.X;
	int y = node_min.Y;
	int z = node_min.Z;

	noise_height_select->perlinMap2D(x, z);
	noise_terrain_persist->perlinMap2D(x, z);
	float *persistmap = noise_terrain_persist->result;
	for (int i = 0; i != csize.X * csize.Z; i++)
		persistmap[i] = rangelim(persistmap[i], 0.4, 0.9);

	noise_terrain_base->perlinMap2D(x, z, persistmap);
	noise_terrain_alt->perlinMap2D(x, z, persistmap);
	noise_filler_depth->perlinMap2D(x, z);

	if (spflags & MGV7_MOUNTAINS) {
		noise_mountain->perlinMap3D(x, y, z);
		noise_mount_height->perlinMap2D(x, z);
	}

	if (spflags & MGV7_RIDGES) {
		noise_ridge->perlinMap3D(x, y, z);
		noise_ridge_uwater->perlinMap2D(x, z);
	}

	noise_heat->perlinMap2D(x, z);
	noise_humidity->perlinMap2D(x, z);

	//printf("calculateNoise: %dus\n", t.stop());
}


Biome *MapgenV7::getBiomeAtPoint(v3s16 p)
{
	float heat      = NoisePerlin2D(&noise_heat->np, p.X, p.Z, seed);
	float humidity  = NoisePerlin2D(&noise_humidity->np, p.X, p.Z, seed);
	s16 groundlevel = baseTerrainLevelAtPoint(p.X, p.Z);

	return bmgr->getBiome(heat, humidity, groundlevel);
}

//needs to be updated
float MapgenV7::baseTerrainLevelAtPoint(int x, int z)
{
	float hselect = NoisePerlin2D(&noise_height_select->np, x, z, seed);
	hselect = rangelim(hselect, 0.0, 1.0);

	float persist = NoisePerlin2D(&noise_terrain_persist->np, x, z, seed);
	persist = rangelim(persist, 0.4, 0.9);

	auto persist_save = noise_terrain_base->np.persist;
	noise_terrain_base->np.persist = persist;
	float height_base = NoisePerlin2D(&noise_terrain_base->np, x, z, seed);
	noise_terrain_base->np.persist = persist_save;

	persist_save = noise_terrain_alt->np.persist;
	noise_terrain_alt->np.persist = persist;
	float height_alt = NoisePerlin2D(&noise_terrain_alt->np, x, z, seed);
	noise_terrain_alt->np.persist = persist_save;

	if (height_alt > height_base)
		return height_alt;

	return (height_base * hselect) + (height_alt * (1.0 - hselect));
}


float MapgenV7::baseTerrainLevelFromMap(int index)
{
	float hselect     = rangelim(noise_height_select->result[index], 0.0, 1.0);
	float height_base = noise_terrain_base->result[index];
	float height_alt  = noise_terrain_alt->result[index];

	if (height_alt > height_base)
		return height_alt;

	return (height_base * hselect) + (height_alt * (1.0 - hselect));
}


bool MapgenV7::getMountainTerrainAtPoint(int x, int y, int z)
{
	float mnt_h_n = NoisePerlin2D(&noise_mount_height->np, x, z, seed);
	float height_modifier = -((float)y / mnt_h_n);
	float mnt_n = NoisePerlin3D(&noise_mountain->np, x, y, z, seed);

	return mnt_n + height_modifier >= 0.6;
}


bool MapgenV7::getMountainTerrainFromMap(int idx_xyz, int idx_xz, int y)
{
	float mounthn = noise_mount_height->result[idx_xz];
	float height_modifier = -((float)y / mounthn);
	return (noise_mountain->result[idx_xyz] + height_modifier >= 0.6);
}


#if 0
void MapgenV7::carveRivers() {
	MapNode n_air(CONTENT_AIR), n_water_source(c_water_source);
	MapNode n_stone(c_stone);
	u32 index = 0;

	int river_depth = 4;

	for (s16 z = node_min.Z; z <= node_max.Z; z++)
	for (s16 x = node_min.X; x <= node_max.X; x++, index++) {
		float terrain_mod  = noise_terrain_mod->result[index];
		NoiseParams *np = noise_terrain_river->np;
		np.persist = noise_terrain_persist->result[index];
		float terrain_river = NoisePerlin2DNoTxfm(np, x, z, seed);
		float height = terrain_river * (1 - abs(terrain_mod)) *
						noise_terrain_river->np.scale;
		height = log(height * height); //log(h^3) is pretty interesting for terrain

		s16 y = heightmap[index];
		if (height < 1.0 && y > river_depth &&
			y - river_depth >= node_min.Y && y <= node_max.Y) {

			for (s16 ry = y; ry != y - river_depth; ry--) {
				u32 vi = vm->m_area.index(x, ry, z);
				vm->m_data[vi] = n_air;
			}

			u32 vi = vm->m_area.index(x, y - river_depth, z);
			vm->m_data[vi] = n_water_source;
		}
	}
}
#endif


int MapgenV7::generateTerrain()
{
	int ymax = generateBaseTerrain();

	if (spflags & MGV7_MOUNTAINS)
		ymax = generateMountainTerrain(ymax);

	if (spflags & MGV7_RIDGES)
		generateRidgeTerrain();

	return ymax;
}


int MapgenV7::generateBaseTerrain()
{
	MapNode n_air(CONTENT_AIR);
	MapNode n_stone(c_stone);
	MapNode n_water(c_water_source);
	MapNode n_ice(c_ice);

	int stone_surface_max_y = -MAP_GENERATION_LIMIT;
	v3s16 em = vm->m_area.getExtent();
	u32 index = 0;

	for (s16 z = node_min.Z; z <= node_max.Z; z++)
	for (s16 x = node_min.X; x <= node_max.X; x++, index++) {
		float surface_height = baseTerrainLevelFromMap(index);
		s16 surface_y = (s16)surface_height;

		heightmap[index]       = surface_y;
		ridge_heightmap[index] = surface_y;

		if (surface_y > stone_surface_max_y)
			stone_surface_max_y = surface_y;

		s16 heat = m_emerge->env->m_use_weather ? m_emerge->env->getServerMap().updateBlockHeat(m_emerge->env, v3POS(x,node_max.Y,z), nullptr, &heat_cache) : 0;

		u32 i = vm->m_area.index(x, node_min.Y, z);
		for (s16 y = node_min.Y; y <= node_max.Y; y++) {

			if (vm->m_data[i].getContent() == CONTENT_IGNORE) {
				if (y <= surface_y) {

					int index3 = (z - node_min.Z) * zstride +
						(y - node_min.Y) * ystride +
						(x - node_min.X);

					vm->m_data[i] = layers_get(index3);
				}
				else if (y <= water_level)
				{
					vm->m_data[i] = (heat < 0 && y > heat/3) ? n_ice : n_water;
				}
				else
					vm->m_data[i] = n_air;
			}
			vm->m_area.add_y(em, i, 1);
		}
	}

	return stone_surface_max_y;
}


int MapgenV7::generateMountainTerrain(int ymax)
{
	if (node_max.Y <= water_level)
		return ymax;

	MapNode n_stone(c_stone);
	u32 j = 0;

	for (s16 z = node_min.Z; z <= node_max.Z; z++)
	for (s16 y = node_min.Y; y <= node_max.Y; y++) {
		u32 vi = vm->m_area.index(node_min.X, y, z);
		for (s16 x = node_min.X; x <= node_max.X; x++) {
			int index = (z - node_min.Z) * csize.X + (x - node_min.X);
			content_t c = vm->m_data[vi].getContent();

			if (getMountainTerrainFromMap(j, index, y)
					&& (c == CONTENT_AIR || c == c_water_source)) {
				vm->m_data[vi] = n_stone;
				if (y > ymax)
					ymax = y;
			}

			vi++;
			j++;
		}
	}

	return ymax;
}


void MapgenV7::generateRidgeTerrain()
{
	MapNode n_water(c_water_source);
	MapNode n_ice(c_ice);
	MapNode n_air(CONTENT_AIR);
	u32 index = 0;

	for (s16 z = node_min.Z; z <= node_max.Z; z++)
	for (s16 y = node_min.Y; y <= node_max.Y; y++) {
		u32 vi = vm->m_area.index(node_min.X, y, z);
		for (s16 x = node_min.X; x <= node_max.X; x++, index++, vi++) {
			int j = (z - node_min.Z) * csize.X + (x - node_min.X);

			if (heightmap[j] < water_level - 4)
				continue;

			float widthn = (noise_terrain_persist->result[j] - 0.6) / 0.1;
			//widthn = rangelim(widthn, -0.05, 0.5);

			float width = 0.3; // TODO: figure out acceptable perlin noise values
			float uwatern = noise_ridge_uwater->result[j] * 2;
			if (uwatern < -width || uwatern > width)
				continue;

			float height_mod = (float)(y + 17) / 2.5;
			float width_mod  = (width - fabs(uwatern));
			float nridge = noise_ridge->result[index] * (float)y / 7.0;

			if (y < water_level)
				nridge = -fabs(nridge) * 3.0 * widthn * 0.3;

			if (nridge + width_mod * height_mod < 0.6)
				continue;

			if (y < ridge_heightmap[j])
				ridge_heightmap[j] = y - 1;

			s16 heat = m_emerge->env->m_use_weather ? m_emerge->env->getServerMap().updateBlockHeat(m_emerge->env, v3POS(x,node_max.Y,z), NULL, &heat_cache) : 0;
			MapNode n_water_or_ice = (heat < 0 && y > water_level + heat/4) ? n_ice : n_water;

			vm->m_data[vi] = (y > water_level) ? n_air : n_water_or_ice;
		}
	}
}


void MapgenV7::generateBiomes()
{
	if (node_max.Y < water_level)
		return;

	MapNode n_air(CONTENT_AIR);
	MapNode n_stone(c_stone);
	MapNode n_water(c_water_source);

	v3s16 em = vm->m_area.getExtent();
	u32 index = 0;

	for (s16 z = node_min.Z; z <= node_max.Z; z++)
	for (s16 x = node_min.X; x <= node_max.X; x++, index++) {
		Biome *biome        = (Biome *)bmgr->get(biomemap[index]);
		s16 dfiller         = biome->depth_filler + noise_filler_depth->result[index];
		s16 y0_top          = biome->depth_top;
		s16 y0_filler       = biome->depth_top + dfiller;
		s16 shore_max       = water_level + biome->height_shore;
		s16 depth_water_top = biome->depth_water_top;

		s16 nplaced = 0;
		u32 i = vm->m_area.index(x, node_max.Y, z);

		content_t c_above = vm->m_data[i + em.X].getContent();
		bool have_air = c_above == CONTENT_AIR;

		s16 heat = m_emerge->env->m_use_weather ? m_emerge->env->getServerMap().updateBlockHeat(m_emerge->env, v3POS(x,node_max.Y,z), NULL, &heat_cache) : 0;

		for (s16 y = node_max.Y; y >= node_min.Y; y--) {
			content_t c = vm->m_data[i].getContent();

			// It could be the case that the elevation is equal to the chunk
			// boundary, but the chunk above has not been generated yet
			if (y == node_max.Y && c_above == CONTENT_IGNORE &&
				y == heightmap[index] && c == c_stone) {
				int j = (z - node_min.Z) * zstride +
						(y - node_min.Y) * ystride +
						(x - node_min.X);
				have_air = !getMountainTerrainFromMap(j, index, y);
			}
			if (c != CONTENT_AIR && c != c_water_source && have_air) {
				content_t c_below = vm->m_data[i - em.X].getContent();

				if (c_below != CONTENT_AIR) {
					if (nplaced < y0_top) {
						if(y < water_level)
							vm->m_data[i] = MapNode(biome->c_underwater);
						else if(y <= shore_max)
							vm->m_data[i] = MapNode(biome->c_shore_top);
						else
							vm->m_data[i] = MapNode(
								((y < water_level) && (biome->c_top == c_dirt_with_grass))
									? c_dirt
									: heat < -3
										? biome->c_top_cold
										: biome->c_top);
						nplaced++;
					} else if (nplaced < y0_filler && nplaced >= y0_top) {
						if(y < water_level)
							vm->m_data[i] = MapNode(biome->c_underwater);
						else if(y <= shore_max)
							vm->m_data[i] = MapNode(biome->c_shore_filler);
						else
							vm->m_data[i] = MapNode(biome->c_filler);
						nplaced++;
					} else if (c == c_stone) {
						have_air = false;
						nplaced  = 0;
						vm->m_data[i] = MapNode(biome->c_stone);
					} else {
						have_air = false;
						nplaced  = 0;
					}
				} else if (c == c_stone) {
					have_air = false;
					nplaced = 0;
					vm->m_data[i] = MapNode(biome->c_stone);
				}
			} else if (c == c_stone) {
				have_air = false;
				nplaced = 0;
				vm->m_data[i] = MapNode(biome->c_stone);
			} else if (c == c_water_source) {
				have_air = true;
				nplaced = 0;
				if(y > water_level - depth_water_top)
					vm->m_data[i] = MapNode(biome->c_water_top);
				else
					vm->m_data[i] = MapNode((heat < 0 && y > water_level + heat/4) ? biome->c_ice : biome->c_water);
			} else if (c == CONTENT_AIR) {
				have_air = true;
				nplaced = 0;
			}

			vm->m_area.add_y(em, i, -1);
		}
	}
}


void MapgenV7::dustTopNodes()
{
	v3s16 em = vm->m_area.getExtent();
	u32 index = 0;

	if (water_level > node_max.Y)
		return;

	for (s16 z = node_min.Z; z <= node_max.Z; z++)
	for (s16 x = node_min.X; x <= node_max.X; x++, index++) {
		Biome *biome = (Biome *)bmgr->get(biomemap[index]);

		if (biome->c_dust == CONTENT_IGNORE)
			continue;

		s16 y = node_max.Y;
		u32 vi = vm->m_area.index(x, y, z);
		for (; y >= node_min.Y; y--) {
			if (vm->m_data[vi].getContent() != CONTENT_AIR)
				break;

			vm->m_area.add_y(em, vi, -1);
		}

		content_t c = vm->m_data[vi].getContent();
		if (!ndef->get(c).buildable_to && c != CONTENT_IGNORE) {
			if (y == node_max.Y)
				continue;

			vm->m_area.add_y(em, vi, 1);
			vm->m_data[vi] = MapNode(biome->c_dust);
		}
	}
}


#if 0
void MapgenV7::addTopNodes()
{
	v3s16 em = vm->m_area.getExtent();
	s16 ntopnodes;
	u32 index = 0;

	for (s16 z = node_min.Z; z <= node_max.Z; z++)
	for (s16 x = node_min.X; x <= node_max.X; x++, index++) {
		Biome *biome = bmgr->biomes[biomemap[index]];

		//////////////////// First, add top nodes below the ridge
		s16 y = ridge_heightmap[index];

		// This cutoff is good enough, but not perfect.
		// It will cut off potentially placed top nodes at chunk boundaries
		if (y < node_min.Y)
			continue;
		if (y > node_max.Y) {
			y = node_max.Y; // Let's see if we can still go downward anyway
			u32 vi = vm->m_area.index(x, y, z);
			content_t c = vm->m_data[vi].getContent();
			if (ndef->get(c).walkable)
				continue;
		}

		// N.B.  It is necessary to search downward since ridge_heightmap[i]
		// might not be the actual height, just the lowest part in the chunk
		// where a ridge had been carved
		u32 i = vm->m_area.index(x, y, z);
		for (; y >= node_min.Y; y--) {
			content_t c = vm->m_data[i].getContent();
			if (ndef->get(c).walkable)
				break;
			vm->m_area.add_y(em, i, -1);
		}

		if (y != node_min.Y - 1 && y >= water_level) {
			ridge_heightmap[index] = y; //update ridgeheight
			ntopnodes = biome->top_depth;
			for (; y <= node_max.Y && ntopnodes; y++) {
				ntopnodes--;
				vm->m_data[i] = MapNode(biome->c_top);
				vm->m_area.add_y(em, i, 1);
			}
			// If dirt, grow grass on it.
			if (y > water_level - 10 &&
				vm->m_data[i].getContent() == CONTENT_AIR) {
				vm->m_area.add_y(em, i, -1);
				if (vm->m_data[i].getContent() == c_dirt)
					vm->m_data[i] = MapNode(c_dirt_with_grass);
			}
		}

		//////////////////// Now, add top nodes on top of the ridge
		y = heightmap[index];
		if (y > node_max.Y) {
			y = node_max.Y; // Let's see if we can still go downward anyway
			u32 vi = vm->m_area.index(x, y, z);
			content_t c = vm->m_data[vi].getContent();
			if (ndef->get(c).walkable)
				continue;
		}

		i = vm->m_area.index(x, y, z);
		for (; y >= node_min.Y; y--) {
			content_t c = vm->m_data[i].getContent();
			if (ndef->get(c).walkable)
				break;
			vm->m_area.add_y(em, i, -1);
		}

		if (y != node_min.Y - 1) {
			ntopnodes = biome->top_depth;
			// Let's see if we've already added it...
			if (y == ridge_heightmap[index] + ntopnodes - 1)
				continue;

			for (; y <= node_max.Y && ntopnodes; y++) {
				ntopnodes--;
				vm->m_data[i] = MapNode(biome->c_top);
				vm->m_area.add_y(em, i, 1);
			}
			// If dirt, grow grass on it.
			if (y > water_level - 10 &&
				vm->m_data[i].getContent() == CONTENT_AIR) {
				vm->m_area.add_y(em, i, -1);
				if (vm->m_data[i].getContent() == c_dirt)
					vm->m_data[i] = MapNode(c_dirt_with_grass);
			}
		}
	}
}
#endif


NoiseParams nparams_v7_def_cave(6, 6.0, v3f(250.0, 250.0, 250.0), 34329, 3, 0.50, 2.0);

void MapgenV7::generateCaves(int max_stone_y)
{
	PseudoRandom ps(blockseed + 21343);

	int volume_nodes = (node_max.X - node_min.X + 1) *
					   (node_max.Y - node_min.Y + 1) *
					   (node_max.Z - node_min.Z + 1);
	float cave_amount = NoisePerlin2D(&nparams_v7_def_cave,
								node_min.X, node_min.Y, seed);

	u32 caves_count = MYMAX(0.0, cave_amount) * volume_nodes / 250000;
	for (u32 i = 0; i < caves_count; i++) {
		CaveV7 cave(this, &ps, false);
		cave.makeCave(node_min, node_max, max_stone_y);
	}

	u32 bruises_count = (ps.range(1, 8) == 1) ? ps.range(0, ps.range(0, 2)) : 1;
	for (u32 i = 0; i < bruises_count; i++) {
		CaveV7 cave(this, &ps, true);
		cave.makeCave(node_min, node_max, max_stone_y);
	}
}


void MapgenV7::generateExperimental() {
	if (float_islands)
		if (float_islands_generate(node_min, node_max, float_islands, vm))
			dustTopNodes();
}
