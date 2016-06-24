/*
mapgen_v7.cpp
Copyright (C) 2010-2015 kwolekr, Ryan Kwolek <kwolekr@minetest.net>
Copyright (C) 2010-2015 paramat, Matt Gregory
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
	: MapgenBasic(mapgenid, params, emerge)
	, Mapgen_features(mapgenid, params, emerge)
{
	MapgenV7Params *sp = (MapgenV7Params *)params->sparams;

	this->spflags    = sp->spflags;
	this->cave_width = sp->cave_width;

	//// Terrain noise
	noise_terrain_base    = new Noise(&sp->np_terrain_base,    seed, csize.X, csize.Z);
	noise_terrain_alt     = new Noise(&sp->np_terrain_alt,     seed, csize.X, csize.Z);
	noise_terrain_persist = new Noise(&sp->np_terrain_persist, seed, csize.X, csize.Z);
	noise_height_select   = new Noise(&sp->np_height_select,   seed, csize.X, csize.Z);
	noise_filler_depth    = new Noise(&sp->np_filler_depth,    seed, csize.X, csize.Z);
	noise_mount_height    = new Noise(&sp->np_mount_height,    seed, csize.X, csize.Z);
	noise_ridge_uwater    = new Noise(&sp->np_ridge_uwater,    seed, csize.X, csize.Z);

	//// 3d terrain noise
	// 1-up 1-down overgeneration
	noise_mountain = new Noise(&sp->np_mountain, seed, csize.X, csize.Y + 2, csize.Z);
	noise_ridge    = new Noise(&sp->np_ridge,    seed, csize.X, csize.Y + 2, csize.Z);

	MapgenBasic::np_cave1 = sp->np_cave1;
	MapgenBasic::np_cave2 = sp->np_cave2;

	//freeminer:
	y_offset = 1;

	float_islands = sp->float_islands;
	noise_float_islands1  = new Noise(&sp->np_float_islands1, seed, csize.X, csize.Y + y_offset * 2, csize.Z);
	noise_float_islands2  = new Noise(&sp->np_float_islands2, seed, csize.X, csize.Y + y_offset * 2, csize.Z);
	noise_float_islands3  = new Noise(&sp->np_float_islands3, seed, csize.X, csize.Z);

	noise_layers          = new Noise(&sp->np_layers,         seed, csize.X, csize.Y + y_offset * 2, csize.Z);
	layers_init(emerge, sp->paramsj);
	//noise_cave_indev      = new Noise(&sp->np_cave_indev,     seed, csize.X, csize.Y + y_offset * 2, csize.Z);
	//==========

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
}


MapgenV7Params::MapgenV7Params()
{
	spflags    = MGV7_MOUNTAINS | MGV7_RIDGES;
	cave_width = 0.2;

//freeminer:
	float_islands = 500;
	np_float_islands1  = NoiseParams(0,    1,   v3f(256, 256, 256), 3683,  6, 0.6, 2.0, NOISE_FLAG_DEFAULTS, 1,   1.5);
	np_float_islands2  = NoiseParams(0,    1,   v3f(8,   8,   8  ), 9292,  2, 0.5, 2.0, NOISE_FLAG_DEFAULTS, 1,   1.5);
	np_float_islands3  = NoiseParams(0,    1,   v3f(256, 256, 256), 6412,  2, 0.5, 2.0, NOISE_FLAG_DEFAULTS, 1,   0.5);
	np_layers          = NoiseParams(500,  500, v3f(100, 100,  100), 3663, 5, 0.6, 2.0, NOISE_FLAG_DEFAULTS, 1,   1.1,   0.5);
//----------

	np_terrain_base    = NoiseParams(4,    70,  v3f(600,  600,  600),  82341, 5, 0.6,  2.0);
	np_terrain_alt     = NoiseParams(4,    25,  v3f(600,  600,  600),  5934,  5, 0.6,  2.0);
	np_terrain_persist = NoiseParams(0.6,  0.1, v3f(2000, 2000, 2000), 539,   3, 0.6,  2.0);
	np_height_select   = NoiseParams(-8,   16,  v3f(500,  500,  500),  4213,  6, 0.7,  2.0);
	np_filler_depth    = NoiseParams(0,    1.2, v3f(150,  150,  150),  261,   3, 0.7,  2.0);
	np_mount_height    = NoiseParams(256,  112, v3f(1000, 1000, 1000), 72449, 3, 0.6,  2.0);
	np_ridge_uwater    = NoiseParams(0,    1,   v3f(1000, 1000, 1000), 85039, 5, 0.6,  2.0);
	np_mountain        = NoiseParams(-0.6, 1,   v3f(250,  350,  250),  5333,  5, 0.63, 2.0);
	np_ridge           = NoiseParams(0,    1,   v3f(100,  100,  100),  6467,  4, 0.75, 2.0);
	np_cave1           = NoiseParams(0,    12,  v3f(61,   61,   61),   52534, 3, 0.5,  2.0);
	np_cave2           = NoiseParams(0,    12,  v3f(67,   67,   67),   10325, 3, 0.5,  2.0);
}


void MapgenV7Params::readParams(Settings *settings)
{
	settings->getFlagStrNoEx("mgv7_spflags",  spflags, flagdesc_mapgen_v7);
	settings->getFloatNoEx("mgv7_cave_width", cave_width);

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
	settings->setFlagStr("mgv7_spflags",  spflags, flagdesc_mapgen_v7, U32_MAX);
	settings->setFloat("mgv7_cave_width", cave_width);

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


///////////////////////////////////////////////////////////////////////////////


int MapgenV7::getSpawnLevelAtPoint(v2s16 p)
{
	// Base terrain calculation
	s16 y = baseTerrainLevelAtPoint(p.X, p.Y);

	// Ridge/river terrain calculation
	float width = 0.2;
	float uwatern = NoisePerlin2D(&noise_ridge_uwater->np, p.X, p.Y, seed) * 2;
	// if inside a river this is an unsuitable spawn point
	if (fabs(uwatern) <= width)
		return MAX_MAP_GENERATION_LIMIT;

	// Mountain terrain calculation
	int iters = 128;
	while (iters--) {
		if (!getMountainTerrainAtPoint(p.X, y + 1, p.Y)) {  // Air, y is ground level
			if (y <= water_level || y > water_level + 16)
				return MAX_MAP_GENERATION_LIMIT;  // Unsuitable spawn point
			else
				return y;
		}
		y++;
	}

	// Unsuitable spawn point, no ground surface found
	return MAX_MAP_GENERATION_LIMIT;
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

	//freeminer:
	if (float_islands && node_max.Y >= float_islands) {
		float_islands_prepare(node_min, node_max, float_islands);
	}

	layers_prepare(node_min, node_max);
	//==========

	// Generate base and mountain terrain
	// An initial heightmap is no longer created here for use in generateRidgeTerrain()
	s16 stone_surface_max_y = generateTerrain();

	// Generate rivers
	if (spflags & MGV7_RIDGES)
		generateRidgeTerrain();

	// Create heightmap
	updateHeightmap(node_min, node_max);

	// Init biome generator, place biome-specific nodes, and build biomemap
	biomegen->calcBiomeNoise(node_min);
	MgStoneType stone_type = generateBiomes();

	generateExperimental();

	if (flags & MG_CAVES)
		generateCaves(stone_surface_max_y, water_level);

	if (flags & MG_DUNGEONS)
		generateDungeons(stone_surface_max_y, stone_type);

	// Generate the registered decorations
	if (flags & MG_DECORATIONS)
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


int MapgenV7::generateTerrain()
{
	MapNode n_air(CONTENT_AIR);
	MapNode n_stone(c_stone);
	MapNode n_water(c_water_source);
	MapNode n_ice(c_ice);

	//// Calculate noise for terrain generation
	noise_terrain_persist->perlinMap2D(node_min.X, node_min.Z);
	float *persistmap = noise_terrain_persist->result;

	noise_terrain_base->perlinMap2D(node_min.X, node_min.Z, persistmap);
	noise_terrain_alt->perlinMap2D(node_min.X, node_min.Z, persistmap);
	noise_height_select->perlinMap2D(node_min.X, node_min.Z);

	if (spflags & MGV7_MOUNTAINS) {
		noise_mountain->perlinMap3D(node_min.X, node_min.Y - 1, node_min.Z);
		noise_mount_height->perlinMap2D(node_min.X, node_min.Z);
	}

	//// Place nodes
	v3s16 em = vm->m_area.getExtent();
	s16 stone_surface_max_y = -MAX_MAP_GENERATION_LIMIT;
	u32 index2d = 0;

	for (s16 z = node_min.Z; z <= node_max.Z; z++)
	for (s16 x = node_min.X; x <= node_max.X; x++, index2d++) {
		s16 surface_y = baseTerrainLevelFromMap(index2d);

		if (surface_y > stone_surface_max_y)
			stone_surface_max_y = surface_y;

		s16 heat = m_emerge->env->m_use_weather ? m_emerge->env->getServerMap().updateBlockHeat(m_emerge->env, v3POS(x,node_max.Y,z), nullptr, &heat_cache) : 0;

		u32 vi = vm->m_area.index(x, node_min.Y - 1, z);
		u32 index3d = (z - node_min.Z) * zstride_1u1d + (x - node_min.X);

		for (s16 y = node_min.Y - 1; y <= node_max.Y + 1; y++) {
			if (vm->m_data[vi].getContent() == CONTENT_IGNORE) {
				if (y <= surface_y) {
					vm->m_data[vi] = layers_get(index3d);  // Base terrain
				} else if ((spflags & MGV7_MOUNTAINS) &&
						getMountainTerrainFromMap(index3d, index2d, y)) {
					vm->m_data[vi] = layers_get(index3d);  // Mountain terrain
					if (y > stone_surface_max_y)
						stone_surface_max_y = y;
				} else if (y <= water_level) {
					//vm->m_data[vi] = n_water;
					vm->m_data[vi] = (heat < 0 && y > heat/3) ? n_ice : n_water;
					if (liquid_pressure && y <= 0)
						vm->m_data[vi].addLevel(m_emerge->ndef, water_level - y, 1);
				} else {
					vm->m_data[vi] = n_air;
				}
			}
			vm->m_area.add_y(em, vi, 1);
			index3d += ystride;
		}
	}

	return stone_surface_max_y;
}


void MapgenV7::generateRidgeTerrain()
{
	if (node_max.Y < water_level - 16)
		return;

	noise_ridge->perlinMap3D(node_min.X, node_min.Y - 1, node_min.Z);
	noise_ridge_uwater->perlinMap2D(node_min.X, node_min.Z);

	MapNode n_water(c_water_source);
	MapNode n_ice(c_ice);
	MapNode n_air(CONTENT_AIR);
	u32 index = 0;
	float width = 0.2;

	for (s16 z = node_min.Z; z <= node_max.Z; z++)
	for (s16 y = node_min.Y - 1; y <= node_max.Y + 1; y++) {
		u32 vi = vm->m_area.index(node_min.X, y, z);
		for (s16 x = node_min.X; x <= node_max.X; x++, index++, vi++) {
			int j = (z - node_min.Z) * csize.X + (x - node_min.X);

			float uwatern = noise_ridge_uwater->result[j] * 2;
			if (fabs(uwatern) > width)
				continue;

			float altitude = y - water_level;
			float height_mod = (altitude + 17) / 2.5;
			float width_mod  = width - fabs(uwatern);
			float nridge = noise_ridge->result[index] * MYMAX(altitude, 0) / 7.0;

			if (nridge + width_mod * height_mod < 0.6)
				continue;

			s16 heat = m_emerge->env->m_use_weather ? m_emerge->env->getServerMap().updateBlockHeat(m_emerge->env, v3POS(x,node_max.Y,z), NULL, &heat_cache) : 0;
			MapNode n_water_or_ice = (heat < 0 && y > water_level + heat/4) ? n_ice : n_water;

			vm->m_data[vi] = (y > water_level) ? n_air : n_water_or_ice;
		}
	}
}


////////////////////////////////////////////////////////////////////////////////
//// Code Boneyard
////
//// Much of the stuff here has potential to become useful again at some point
//// in the future, but we don't want it to get lost or forgotten in version
//// control.
////

#if 0
int MapgenV7::generateMountainTerrain(s16 ymax)
{
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
#endif


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


void MapgenV7::generateExperimental() {
	if (float_islands)
		if (float_islands_generate(node_min, node_max, float_islands, vm))
			dustTopNodes();
}

