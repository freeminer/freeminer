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
//#include "profiler.h" // For TimeTaker
#include "settings.h" // For g_settings
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
	this->zstride = csize.X * (csize.Y + 2);

	this->biomemap        = new u8[csize.X * csize.Z];
	this->heightmap       = new s16[csize.X * csize.Z];
	this->heatmap         = NULL;
	this->humidmap        = NULL;
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
	noise_mountain = new Noise(&sp->np_mountain, seed, csize.X, csize.Y + 2, csize.Z);
	noise_ridge    = new Noise(&sp->np_ridge,    seed, csize.X, csize.Y + 2, csize.Z);
	noise_cave1    = new Noise(&sp->np_cave1,    seed, csize.X, csize.Y + 2, csize.Z);
	noise_cave2    = new Noise(&sp->np_cave2,    seed, csize.X, csize.Y + 2, csize.Z);

	//// Biome noise
	noise_heat           = new Noise(&params->np_biome_heat,           seed, csize.X, csize.Z);
	noise_humidity       = new Noise(&params->np_biome_humidity,       seed, csize.X, csize.Z);
	noise_heat_blend     = new Noise(&params->np_biome_heat_blend,     seed, csize.X, csize.Z);
	noise_humidity_blend = new Noise(&params->np_biome_humidity_blend, seed, csize.X, csize.Z);

	//// Resolve nodes to be used
	INodeDefManager *ndef = emerge->ndef;

	c_stone                = ndef->getId("mapgen_stone");
	c_water_source         = ndef->getId("mapgen_water_source");
	c_lava_source          = ndef->getId("mapgen_lava_source");
	c_desert_stone         = ndef->getId("mapgen_desert_stone");
	c_ice                  = ndef->getId("mapgen_ice");
	c_sandstone            = ndef->getId("mapgen_sandstone");

	c_cobble               = ndef->getId("mapgen_cobble");
	c_stair_cobble         = ndef->getId("mapgen_stair_cobble");
	c_mossycobble          = ndef->getId("mapgen_mossycobble");
	c_sandstonebrick       = ndef->getId("mapgen_sandstonebrick");
	c_stair_sandstonebrick = ndef->getId("mapgen_stair_sandstonebrick");

	if (c_ice == CONTENT_IGNORE)
		c_ice = c_water_source;

	//freeminer:
	float_islands = sp->float_islands;
	noise_float_islands1  = new Noise(&sp->np_float_islands1, seed, csize.X, csize.Y + 2, csize.Z);
	noise_float_islands2  = new Noise(&sp->np_float_islands2, seed, csize.X, csize.Y + 2, csize.Z);
	noise_float_islands3  = new Noise(&sp->np_float_islands3, seed, csize.X, csize.Z);

	noise_layers          = new Noise(&sp->np_layers,         seed, csize.X, csize.Y + 2, csize.Z);
	layers_init(emerge, sp->paramsj);

	if (c_mossycobble == CONTENT_IGNORE)
		c_mossycobble = c_cobble;
	if (c_stair_cobble == CONTENT_IGNORE)
		c_stair_cobble = c_cobble;
	if (c_sandstonebrick == CONTENT_IGNORE)
		c_sandstonebrick = c_sandstone;
	if (c_stair_sandstonebrick == CONTENT_IGNORE)
		c_stair_sandstonebrick = c_sandstone;
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
	delete noise_cave1;
	delete noise_cave2;

	delete noise_heat;
	delete noise_humidity;
	delete noise_heat_blend;
	delete noise_humidity_blend;

	delete[] ridge_heightmap;
	delete[] heightmap;
	delete[] biomemap;
}


MapgenV7Params::MapgenV7Params()
{
	spflags = MGV7_MOUNTAINS | MGV7_RIDGES;

//freeminer:
	float_islands = 500;
	np_float_islands1  = NoiseParams(0,    1,   v3f(256, 256, 256), 3683,  6, 0.6, 2.0, NOISE_FLAG_DEFAULTS, 1,   1.5);
	np_float_islands2  = NoiseParams(0,    1,   v3f(8,   8,   8  ), 9292,  2, 0.5, 2.0, NOISE_FLAG_DEFAULTS, 1,   1.5);
	np_float_islands3  = NoiseParams(0,    1,   v3f(256, 256, 256), 6412,  2, 0.5, 2.0, NOISE_FLAG_DEFAULTS, 1,   0.5);
	np_layers          = NoiseParams(500,  500, v3f(100, 100,  100), 3663, 5, 0.6, 2.0, NOISE_FLAG_DEFAULTS, 1,   5,   0.5);
//----------

	np_terrain_base    = NoiseParams(4,    70,  v3f(600,  600,  600),  82341, 5, 0.6,  2.0);
	np_terrain_alt     = NoiseParams(4,    25,  v3f(600,  600,  600),  5934,  5, 0.6,  2.0);
	np_terrain_persist = NoiseParams(0.6,  0.1, v3f(2000, 2000, 2000), 539,   3, 0.6,  2.0);
	np_height_select   = NoiseParams(-12,  24,  v3f(500,  500,  500),  4213,  6, 0.7,  2.0);
	np_filler_depth    = NoiseParams(0,    1.2, v3f(150,  150,  150),  261,   3, 0.7,  2.0);
	np_mount_height    = NoiseParams(256,  112, v3f(1000, 1000, 1000), 72449, 3, 0.6,  2.0);
	np_ridge_uwater    = NoiseParams(0,    1,   v3f(1000, 1000, 1000), 85039, 5, 0.6,  2.0);
	np_mountain        = NoiseParams(-0.6, 1,   v3f(250,  350,  250),  5333,  5, 0.63, 2.0);
	np_ridge           = NoiseParams(0,    1,   v3f(100,  100,  100),  6467,  4, 0.75, 2.0);
	np_cave1           = NoiseParams(0,    12,  v3f(100,  100,  100),  52534, 4, 0.5,  2.0);
	np_cave2           = NoiseParams(0,    12,  v3f(100,  100,  100),  10325, 4, 0.5,  2.0);
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

//freeminer:
	settings->getS16NoEx("mg_float_islands", float_islands);
	settings->getNoiseParamsFromGroup("mg_np_float_islands1", np_float_islands1);
	settings->getNoiseParamsFromGroup("mg_np_float_islands2", np_float_islands2);
	settings->getNoiseParamsFromGroup("mg_np_float_islands3", np_float_islands3);
	settings->getNoiseParamsFromGroup("mg_np_layers",         np_layers);
	paramsj = settings->getJson("mg_params", paramsj);
//----------

	settings->getNoiseParams("mgv7_np_cave1",           np_cave1);
	settings->getNoiseParams("mgv7_np_cave2",           np_cave2);
}


void MapgenV7Params::writeParams(Settings *settings) const
{
	settings->setFlagStr("mgv7_spflags", spflags, flagdesc_mapgen_v7, U32_MAX);

	settings->setNoiseParams("mgv7_np_terrain_base",    np_terrain_base);
	settings->setNoiseParams("mgv7_np_terrain_alt",     np_terrain_alt);
	settings->setNoiseParams("mgv7_np_terrain_persist", np_terrain_persist);
	settings->setNoiseParams("mgv7_np_height_select",   np_height_select);
	settings->setNoiseParams("mgv7_np_filler_depth",    np_filler_depth);
	settings->setNoiseParams("mgv7_np_mount_height",    np_mount_height);
	settings->setNoiseParams("mgv7_np_ridge_uwater",    np_ridge_uwater);
	settings->setNoiseParams("mgv7_np_mountain",        np_mountain);
	settings->setNoiseParams("mgv7_np_ridge",           np_ridge);

//freeminer:
	settings->setS16("mg_float_islands", float_islands);
	settings->setNoiseParams("mg_np_float_islands1", np_float_islands1);
	settings->setNoiseParams("mg_np_float_islands2", np_float_islands2);
	settings->setNoiseParams("mg_np_float_islands3", np_float_islands3);
	settings->setNoiseParams("mg_np_layers",         np_layers);
	settings->setJson("mg_params", paramsj);
//----------

	settings->setNoiseParams("mgv7_np_cave1",           np_cave1);
	settings->setNoiseParams("mgv7_np_cave2",           np_cave2);
}


///////////////////////////////////////


int MapgenV7::getGroundLevelAtPoint(v2s16 p)
{
	// Base terrain calculation
	s16 y = baseTerrainLevelAtPoint(p.X, p.Y);

	// Ridge/river terrain calculation
	float width = 0.2;
	float uwatern = NoisePerlin2D(&noise_ridge_uwater->np, p.X, p.Y, seed) * 2;
	// actually computing the depth of the ridge is much more expensive;
	// if inside a river, simply guess
	if (fabs(uwatern) <= width)
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
	// Pre-conditions
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

	// Create heightmap
	updateHeightmap(node_min, node_max);

	// Create biomemap at heightmap surface
	bmgr->calcBiomes(csize.X, csize.Z, noise_heat->result,
		noise_humidity->result, heightmap, biomemap);

	// Actually place the biome-specific nodes
	MgStoneType stone_type = generateBiomes(noise_heat->result, noise_humidity->result);

	generateExperimental();

	if (flags & MG_CAVES)
		generateCaves(stone_surface_max_y);

	if ((flags & MG_DUNGEONS) && (stone_surface_max_y >= node_min.Y)) {
		DungeonParams dp;

		dp.np_rarity  = nparams_dungeon_rarity;
		dp.np_density = nparams_dungeon_density;
		dp.np_wetness = nparams_dungeon_wetness;
		dp.c_water    = c_water_source;
		if (stone_type == STONE) {
			dp.c_cobble = c_cobble;
			dp.c_moss   = c_mossycobble;
			dp.c_stair  = c_stair_cobble;

			dp.diagonal_dirs = false;
			dp.mossratio     = 3.0;
			dp.holesize      = v3s16(1, 2, 1);
			dp.roomsize      = v3s16(0, 0, 0);
			dp.notifytype    = GENNOTIFY_DUNGEON;
		} else if (stone_type == DESERT_STONE) {
			dp.c_cobble = c_desert_stone;
			dp.c_moss   = c_desert_stone;
			dp.c_stair  = c_desert_stone;

			dp.diagonal_dirs = true;
			dp.mossratio     = 0.0;
			dp.holesize      = v3s16(2, 3, 2);
			dp.roomsize      = v3s16(2, 5, 2);
			dp.notifytype    = GENNOTIFY_TEMPLE;
		} else if (stone_type == SANDSTONE) {
			dp.c_cobble = c_sandstonebrick;
			dp.c_moss   = c_sandstonebrick;
			dp.c_stair  = c_sandstonebrick;

			dp.diagonal_dirs = false;
			dp.mossratio     = 0.0;
			dp.holesize      = v3s16(2, 2, 2);
			dp.roomsize      = v3s16(2, 0, 2);
			dp.notifytype    = GENNOTIFY_DUNGEON;
		}

		DungeonGen dgen(this, &dp);
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
		calcLighting(node_min - v3s16(0, 1, 0), node_max + v3s16(0, 1, 0),
			full_node_min, full_node_max);

	//setLighting(node_min - v3s16(1, 0, 1) * MAP_BLOCKSIZE,
	//			node_max + v3s16(1, 0, 1) * MAP_BLOCKSIZE, 0xFF);

	this->generating = false;
}


void MapgenV7::calculateNoise()
{
	//TimeTaker t("calculateNoise", NULL, PRECISION_MICRO);
	int x = node_min.X;
	int y = node_min.Y - 1;
	int z = node_min.Z;

	noise_terrain_persist->perlinMap2D(x, z);
	float *persistmap = noise_terrain_persist->result;

	noise_terrain_base->perlinMap2D(x, z, persistmap);
	noise_terrain_alt->perlinMap2D(x, z, persistmap);
	noise_height_select->perlinMap2D(x, z);

	if (flags & MG_CAVES) {
		noise_cave1->perlinMap3D(x, y, z);
		noise_cave2->perlinMap3D(x, y, z);
	}

	if ((spflags & MGV7_RIDGES) && node_max.Y >= water_level) {
		noise_ridge->perlinMap3D(x, y, z);
		noise_ridge_uwater->perlinMap2D(x, z);
	}

	// Mountain noises are calculated in generateMountainTerrain()

	noise_filler_depth->perlinMap2D(x, z);
	noise_heat->perlinMap2D(x, z);
	noise_humidity->perlinMap2D(x, z);
	noise_heat_blend->perlinMap2D(x, z);
	noise_humidity_blend->perlinMap2D(x, z);

	for (s32 i = 0; i < csize.X * csize.Z; i++) {
		noise_heat->result[i] += noise_heat_blend->result[i];
		noise_humidity->result[i] += noise_humidity_blend->result[i];
	}

	heatmap = noise_heat->result;
	humidmap = noise_humidity->result;
	//printf("calculateNoise: %dus\n", t.stop());
}


Biome *MapgenV7::getBiomeAtPoint(v3s16 p)
{
	float heat = NoisePerlin2D(&noise_heat->np, p.X, p.Z, seed) +
		NoisePerlin2D(&noise_heat_blend->np, p.X, p.Z, seed);
	float humidity = NoisePerlin2D(&noise_humidity->np, p.X, p.Z, seed) +
		NoisePerlin2D(&noise_humidity_blend->np, p.X, p.Z, seed);
	s16 groundlevel = baseTerrainLevelAtPoint(p.X, p.Z);

	return bmgr->getBiome(heat, humidity, groundlevel);
}

//needs to be updated
float MapgenV7::baseTerrainLevelAtPoint(s16 x, s16 z)
{
	float hselect = NoisePerlin2D(&noise_height_select->np, x, z, seed);
	hselect = rangelim(hselect, 0.0, 1.0);

	float persist = NoisePerlin2D(&noise_terrain_persist->np, x, z, seed);

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


bool MapgenV7::getMountainTerrainAtPoint(s16 x, s16 y, s16 z)
{
	float mnt_h_n = NoisePerlin2D(&noise_mount_height->np, x, z, seed);
	float density_gradient = -((float)y / mnt_h_n);
	float mnt_n = NoisePerlin3D(&noise_mountain->np, x, y, z, seed);

	return mnt_n + density_gradient >= 0.0;
}


bool MapgenV7::getMountainTerrainFromMap(int idx_xyz, int idx_xz, s16 y)
{
	float mounthn = noise_mount_height->result[idx_xz];
	float density_gradient = -((float)y / mounthn);
	float mountn = noise_mountain->result[idx_xyz];

	return mountn + density_gradient >= 0.0;
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
	s16 stone_surface_min_y;
	s16 stone_surface_max_y;

	generateBaseTerrain(&stone_surface_min_y, &stone_surface_max_y);

	if ((spflags & MGV7_MOUNTAINS) && stone_surface_min_y < node_max.Y)
		stone_surface_max_y = generateMountainTerrain(stone_surface_max_y);

	if (spflags & MGV7_RIDGES)
		generateRidgeTerrain();

	return stone_surface_max_y;
}


void MapgenV7::generateBaseTerrain(s16 *stone_surface_min_y, s16 *stone_surface_max_y)
{
	MapNode n_air(CONTENT_AIR);
	MapNode n_stone(c_stone);
	MapNode n_water(c_water_source);
	MapNode n_ice(c_ice);

	v3s16 em = vm->m_area.getExtent();
	s16 surface_min_y = MAX_MAP_GENERATION_LIMIT;
	s16 surface_max_y = -MAX_MAP_GENERATION_LIMIT;
	u32 index = 0;

	for (s16 z = node_min.Z; z <= node_max.Z; z++)
	for (s16 x = node_min.X; x <= node_max.X; x++, index++) {
		float surface_height = baseTerrainLevelFromMap(index);
		s16 surface_y = (s16)surface_height;

		heightmap[index]       = surface_y;
		ridge_heightmap[index] = surface_y;

		if (surface_y < surface_min_y)
			surface_min_y = surface_y;

		if (surface_y > surface_max_y)
			surface_max_y = surface_y;

		s16 heat = m_emerge->env->m_use_weather ? m_emerge->env->getServerMap().updateBlockHeat(m_emerge->env, v3POS(x,node_max.Y,z), nullptr, &heat_cache) : 0;

		u32 i = vm->m_area.index(x, node_min.Y - 1, z);
		for (s16 y = node_min.Y - 1; y <= node_max.Y + 1; y++) {
			if (vm->m_data[i].getContent() == CONTENT_IGNORE) {
				if (y <= surface_y) {

					int index3 = (z - node_min.Z) * zstride +
						(y - node_min.Y + 1) * ystride +
						(x - node_min.X);

					vm->m_data[i] = layers_get(index3);
				}
				else if (y <= water_level)
				{
					vm->m_data[i] = (heat < 0 && y > heat/3) ? n_ice : n_water;
					if (liquid_pressure && y <= 0)
						vm->m_data[i].addLevel(m_emerge->ndef, water_level - y, 1);
				}
				else
					vm->m_data[i] = n_air;
			}
			vm->m_area.add_y(em, i, 1);
		}
	}

	*stone_surface_min_y = surface_min_y;
	*stone_surface_max_y = surface_max_y;
}


int MapgenV7::generateMountainTerrain(s16 ymax)
{
	noise_mountain->perlinMap3D(node_min.X, node_min.Y - 1, node_min.Z);
	noise_mount_height->perlinMap2D(node_min.X, node_min.Z);

	MapNode n_stone(c_stone);
	u32 j = 0;

	for (s16 z = node_min.Z; z <= node_max.Z; z++)
	for (s16 y = node_min.Y - 1; y <= node_max.Y + 1; y++) {
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
	if (node_max.Y < water_level)
		return;

	MapNode n_water(c_water_source);
	MapNode n_ice(c_ice);
	MapNode n_air(CONTENT_AIR);
	u32 index = 0;
	float width = 0.2; // TODO: figure out acceptable perlin noise values

	for (s16 z = node_min.Z; z <= node_max.Z; z++)
	for (s16 y = node_min.Y - 1; y <= node_max.Y + 1; y++) {
		u32 vi = vm->m_area.index(node_min.X, y, z);
		for (s16 x = node_min.X; x <= node_max.X; x++, index++, vi++) {
			int j = (z - node_min.Z) * csize.X + (x - node_min.X);

			if (heightmap[j] < water_level - 16)
				continue;

			float uwatern = noise_ridge_uwater->result[j] * 2;
			if (fabs(uwatern) > width)
				continue;

			float altitude = y - water_level;
			float height_mod = (altitude + 17) / 2.5;
			float width_mod  = width - fabs(uwatern);
			float nridge = noise_ridge->result[index] * MYMAX(altitude, 0) / 7.0;

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


MgStoneType MapgenV7::generateBiomes(float *heat_map, float *humidity_map)
{
	v3s16 em = vm->m_area.getExtent();
	u32 index = 0;
	MgStoneType stone_type = STONE;

	for (s16 z = node_min.Z; z <= node_max.Z; z++)
	for (s16 x = node_min.X; x <= node_max.X; x++, index++) {
		Biome *biome = NULL;
		u16 depth_top = 0;
		u16 base_filler = 0;
		u16 depth_water_top = 0;
		u32 vi = vm->m_area.index(x, node_max.Y, z);

		// Check node at base of mapchunk above, either a node of a previously
		// generated mapchunk or if not, a node of overgenerated base terrain.
		content_t c_above = vm->m_data[vi + em.X].getContent();
		bool air_above = c_above == CONTENT_AIR;
		bool water_above = c_above == c_water_source;

		// If there is air or water above enable top/filler placement, otherwise force
		// nplaced to stone level by setting a number exceeding any possible filler depth.
		u16 nplaced = (air_above || water_above) ? 0 : U16_MAX;

		s16 heat = m_emerge->env->m_use_weather ? m_emerge->env->getServerMap().updateBlockHeat(m_emerge->env, v3POS(x,node_max.Y,z), NULL, &heat_cache) : 0;

		for (s16 y = node_max.Y; y >= node_min.Y; y--) {
			content_t c = vm->m_data[vi].getContent();

			bool cc_stone = (c != CONTENT_AIR && c != c_water_source && c != CONTENT_IGNORE);

			// Biome is recalculated each time an upper surface is detected while
			// working down a column. The selected biome then remains in effect for
			// all nodes below until the next surface and biome recalculation.
			// Biome is recalculated:
			// 1. At the surface of stone below air or water.
			// 2. At the surface of water below air.
			// 3. When stone or water is detected but biome has not yet been calculated.
			if ((cc_stone && (air_above || water_above || !biome)) ||
					(c == c_water_source && (air_above || !biome))) {
				biome = bmgr->getBiome(heat_map[index], humidity_map[index], y);
				depth_top = biome->depth_top;
				base_filler = MYMAX(depth_top + biome->depth_filler
						+ noise_filler_depth->result[index], 0);
				depth_water_top = biome->depth_water_top;

				// Detect stone type for dungeons during every biome calculation.
				// This is more efficient than detecting per-node and will not
				// miss any desert stone or sandstone biomes.
				if (biome->c_stone == c_desert_stone)
					stone_type = DESERT_STONE;
				else if (biome->c_stone == c_sandstone)
					stone_type = SANDSTONE;
			}

			if (cc_stone && biome && (c == biome->c_ice || c == biome->c_water || c == biome->c_water_top))
				cc_stone = false;

			if (cc_stone) {
				content_t c_below = vm->m_data[vi - em.X].getContent();

				// If the node below isn't solid, make this node stone, so that
				// any top/filler nodes above are structurally supported.
				// This is done by aborting the cycle of top/filler placement
				// immediately by forcing nplaced to stone level.
				if (c_below == CONTENT_AIR || c_below == c_water_source)
					nplaced = U16_MAX;

				if (nplaced < depth_top) {
					vm->m_data[vi] = MapNode(
								((y < water_level) /* && (biome->c_top == c_dirt_with_grass)*/ )
									? biome->c_top
									: heat < -3
										? biome->c_top_cold
										: biome->c_top);

					nplaced++;
				} else if (nplaced < base_filler) {
					vm->m_data[vi] = MapNode(biome->c_filler);
					nplaced++;
				} else if (nplaced < (depth_top+base_filler+depth_water_top)){
					vm->m_data[vi] = MapNode(biome->c_stone);
				}

				air_above = false;
				water_above = false;
			} else if (c == c_water_source) {
				bool ice = heat < 0 && y > water_level + heat/4;
				vm->m_data[vi] = MapNode((y > (s32)(water_level - depth_water_top)) ?
						(ice ? biome->c_ice : biome->c_water_top) : ice ? biome->c_ice : biome->c_water);
				nplaced = 0;  // Enable top/filler placement for next surface
				air_above = false;
				water_above = true;
			} else if (c == CONTENT_AIR) {
				nplaced = 0;  // Enable top/filler placement for next surface
				air_above = true;
				water_above = false;
			} else {  // Possible various nodes overgenerated from neighbouring mapchunks
				nplaced = U16_MAX;  // Disable top/filler placement
				air_above = false;
				water_above = false;
			}

			vm->m_area.add_y(em, vi, -1);
		}
	}

	return stone_type;
}


void MapgenV7::dustTopNodes()
{
	if (node_max.Y < water_level)
		return;

	v3s16 em = vm->m_area.getExtent();
	u32 index = 0;

	for (s16 z = node_min.Z; z <= node_max.Z; z++)
	for (s16 x = node_min.X; x <= node_max.X; x++, index++) {
		Biome *biome = (Biome *)bmgr->getRaw(biomemap[index]);

		if (biome->c_dust == CONTENT_IGNORE)
			continue;

		u32 vi = vm->m_area.index(x, full_node_max.Y, z);
		content_t c_full_max = vm->m_data[vi].getContent();
		s16 y_start;

		if (c_full_max == CONTENT_AIR) {
			y_start = full_node_max.Y - 1;
		} else if (c_full_max == CONTENT_IGNORE) {
			vi = vm->m_area.index(x, node_max.Y + 1, z);
			content_t c_max = vm->m_data[vi].getContent();

			if (c_max == CONTENT_AIR)
				y_start = node_max.Y;
			else
				continue;
		} else {
			continue;
		}

		vi = vm->m_area.index(x, y_start, z);
		for (s16 y = y_start; y >= node_min.Y - 1; y--) {
			if (vm->m_data[vi].getContent() != CONTENT_AIR)
				break;

			vm->m_area.add_y(em, vi, -1);
		}

		content_t c = vm->m_data[vi].getContent();
		if (!ndef->get(c).buildable_to && c != CONTENT_IGNORE && c != biome->c_dust) {
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


void MapgenV7::generateCaves(s16 max_stone_y)
{
	if (max_stone_y >= node_min.Y) {
		u32 index   = 0;

		for (s16 z = node_min.Z; z <= node_max.Z; z++)
		for (s16 y = node_min.Y - 1; y <= node_max.Y + 1; y++) {
			u32 i = vm->m_area.index(node_min.X, y, z);
			for (s16 x = node_min.X; x <= node_max.X; x++, i++, index++) {
				float d1 = contour(noise_cave1->result[index]);
				float d2 = contour(noise_cave2->result[index]);
				if (d1 * d2 > 0.3) {
					content_t c = vm->m_data[i].getContent();
					if (!ndef->get(c).is_ground_content || c == CONTENT_AIR)
						continue;

					vm->m_data[i] = MapNode(CONTENT_AIR);
				}
			}
		}
	}

	PseudoRandom ps(blockseed + 21343);
	u32 bruises_count = (ps.range(1, 4) == 1) ? ps.range(1, 2) : 0;
	for (u32 i = 0; i < bruises_count; i++) {
		CaveV7 cave(this, &ps);
		cave.makeCave(node_min, node_max, max_stone_y);
	}
}

void MapgenV7::generateExperimental() {
	if (float_islands)
		if (float_islands_generate(node_min, node_max, float_islands, vm))
			dustTopNodes();
}

