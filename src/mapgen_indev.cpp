/*
mapgen_indev.cpp
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

#include "mapgen_indev.h"
#include "constants.h"
#include "map.h"
#include "main.h"
#include "util/numeric.h"
#include "log.h"

///////////////////////////////////////////////////////////////////////////////

MapgenIndev::MapgenIndev(int mapgenid, MapgenParams *params, EmergeManager *emerge) 
	: MapgenV6(mapgenid, params, emerge)
{
	MapgenIndevParams *sp = (MapgenIndevParams *)params->sparams;

	float_islands = sp->float_islands;

	noise_float_islands1  = new Noise(&sp->np_float_islands1, seed, csize.X, csize.Y, csize.Z);
	noise_float_islands2  = new Noise(&sp->np_float_islands2, seed, csize.X, csize.Y, csize.Z);
	noise_float_islands3  = new Noise(&sp->np_float_islands3, seed, csize.X, csize.Z);
}

MapgenIndev::~MapgenIndev() {
	delete noise_float_islands1;
	delete noise_float_islands2;
	delete noise_float_islands3;
}

void MapgenIndev::calculateNoise() {
	MapgenV6::calculateNoise();
	int x = node_min.X;
	int y = node_min.Y;
	int z = node_min.Z;
	// Need to adjust for the original implementation's +.5 offset...
	if (!(flags & MG_FLAT)) {

		noise_float_islands1->perlinMap3D(
			x + 0.33 * noise_float_islands1->np->spread.X * farscale(noise_float_islands1->np->farspread, x, y, z),
			y + 0.33 * noise_float_islands1->np->spread.Y * farscale(noise_float_islands1->np->farspread, x, y, z),
			z + 0.33 * noise_float_islands1->np->spread.Z * farscale(noise_float_islands1->np->farspread, x, y, z)
		);
		noise_float_islands1->transformNoiseMap(x, y, z);

		noise_float_islands2->perlinMap3D(
			x + 0.33 * noise_float_islands2->np->spread.X * farscale(noise_float_islands2->np->farspread, x, y, z),
			y + 0.33 * noise_float_islands2->np->spread.Y * farscale(noise_float_islands2->np->farspread, x, y, z),
			z + 0.33 * noise_float_islands2->np->spread.Z * farscale(noise_float_islands2->np->farspread, x, y, z)
		);
		noise_float_islands2->transformNoiseMap(x, y, z);

		noise_float_islands3->perlinMap2D(
			x + 0.5 * noise_float_islands3->np->spread.X * farscale(noise_float_islands3->np->farspread, x, z),
			z + 0.5 * noise_float_islands3->np->spread.Z * farscale(noise_float_islands3->np->farspread, x, z));
		noise_float_islands3->transformNoiseMap(x, y, z);

	}
}

MapgenIndevParams::MapgenIndevParams() {
	float_islands = 500;
	np_terrain_base    = NoiseParams(-4,   20,  v3f(250, 250, 250), 82341, 5, 0.6,  10,  10,  0.5);
	np_terrain_higher  = NoiseParams(20,   16,  v3f(500, 500, 500), 85039, 5, 0.6,  10,  10,  0.5);
	np_steepness       = NoiseParams(0.85, 0.5, v3f(125, 125, 125), -932,  5, 0.7,  2,   10,  0.5);
	np_height_select   = NoiseParams(0.5,  1,   v3f(250, 250, 250), 4213,  5, 0.69, 10,  10,  0.5);
	np_mud             = NoiseParams(4,    2,   v3f(200, 200, 200), 91013, 3, 0.55, 1,   1,   1);
	np_beach           = NoiseParams(0,    1,   v3f(250, 250, 250), 59420, 3, 0.50, 1,   1,   1);
	np_biome           = NoiseParams(0,    1,   v3f(250, 250, 250), 9130,  3, 0.50, 1,   10,  1);
	np_float_islands1  = NoiseParams(0,    1,   v3f(256, 256, 256), 3683,  6, 0.6,  1,   1.5, 1);
	np_float_islands2  = NoiseParams(0,    1,   v3f(8,   8,   8  ), 9292,  2, 0.5,  1,   1.5, 1);
	np_float_islands3  = NoiseParams(0,    1,   v3f(256, 256, 256), 6412,  2, 0.5,  1,   0.5, 1);
}

void MapgenIndevParams::readParams(Settings *settings) {
	MapgenV6Params::readParams(settings);

	settings->getS16NoEx("mgindev_float_islands", float_islands);

	settings->getNoiseIndevParams("mgindev_np_terrain_base",   np_terrain_base);
	settings->getNoiseIndevParams("mgindev_np_terrain_higher", np_terrain_higher);
	settings->getNoiseIndevParams("mgindev_np_steepness",      np_steepness);
	settings->getNoiseIndevParams("mgindev_np_height_select",  np_height_select);
	settings->getNoiseIndevParams("mgindev_np_mud",            np_mud);
	settings->getNoiseIndevParams("mgindev_np_beach",          np_beach);
	settings->getNoiseIndevParams("mgindev_np_biome",          np_biome);
	settings->getNoiseIndevParams("mgindev_np_float_islands1", np_float_islands1);
	settings->getNoiseIndevParams("mgindev_np_float_islands2", np_float_islands2);
	settings->getNoiseIndevParams("mgindev_np_float_islands3", np_float_islands3);
}

void MapgenIndevParams::writeParams(Settings *settings) {
	MapgenV6Params::writeParams(settings);

	settings->setS16("mgindev_float_islands", float_islands);

	settings->setNoiseIndevParams("mgindev_np_terrain_base",   np_terrain_base);
	settings->setNoiseIndevParams("mgindev_np_terrain_higher", np_terrain_higher);
	settings->setNoiseIndevParams("mgindev_np_steepness",      np_steepness);
	settings->setNoiseIndevParams("mgindev_np_height_select",  np_height_select);
	settings->setNoiseIndevParams("mgindev_np_mud",            np_mud);
	settings->setNoiseIndevParams("mgindev_np_beach",          np_beach);
	settings->setNoiseIndevParams("mgindev_np_biome",          np_biome);
	settings->setNoiseIndevParams("mgindev_np_float_islands1", np_float_islands1);
	settings->setNoiseIndevParams("mgindev_np_float_islands2", np_float_islands2);
	settings->setNoiseIndevParams("mgindev_np_float_islands3", np_float_islands3);
}

void MapgenIndev::generateCaves(int max_stone_y) {
	float cave_amount = NoisePerlin2D(np_cave, node_min.X, node_min.Y, seed);
	int volume_nodes = (node_max.X - node_min.X + 1) *
					   (node_max.Y - node_min.Y + 1) * MAP_BLOCKSIZE;
	cave_amount = MYMAX(0.0, cave_amount);
	u32 caves_count = cave_amount * volume_nodes / 50000;
	u32 bruises_count = 1;
	PseudoRandom ps(blockseed + 21343);
	PseudoRandom ps2(blockseed + 1032);
	
	if (ps.range(1, 6) == 1)
		bruises_count = ps.range(0, ps.range(0, 2));
	
	if (getBiome(v2s16(node_min.X, node_min.Z)) == BT_DESERT) {
		caves_count   /= 3;
		bruises_count /= 3;
	}
	
	for (u32 i = 0; i < caves_count + bruises_count; i++) {
		bool large_cave = (i >= caves_count);
		CaveIndev cave(this, &ps, &ps2, node_min, large_cave);

		cave.makeCave(node_min, node_max, max_stone_y);
	}
}

CaveIndev::CaveIndev(MapgenIndev *mg, PseudoRandom *ps, PseudoRandom *ps2,
				v3s16 node_min, bool is_large_cave) {
	this->mg = mg;
	this->vm = mg->vm;
	this->ndef = mg->ndef;
	this->water_level = mg->water_level;
	this->large_cave = is_large_cave;
	this->ps  = ps;
	this->ps2 = ps2;
	this->c_water_source = mg->c_water_source;
	this->c_lava_source  = mg->c_lava_source;
	this->c_ice          = mg->c_ice;

	min_tunnel_diameter = 2;
	max_tunnel_diameter = ps->range(2,6);
	dswitchint = ps->range(1,14);
	flooded = large_cave && ps->range(0,4);
	if (large_cave) {
		part_max_length_rs = ps->range(2,4);
		float scale = farscale(0.4, node_min.X, node_min.Y, node_min.Z);
		if (node_min.Y < -100 && !ps->range(0, scale * 14)) { //huge
			flooded = !ps->range(0, 10);
			tunnel_routepoints = ps->range(10, 50);
			min_tunnel_diameter = 30;
			max_tunnel_diameter = ps->range(40, ps->range(50, 80));
		} else {
			tunnel_routepoints = ps->range(5, ps->range(15,30));
			min_tunnel_diameter = 5;
			max_tunnel_diameter = ps->range(7, ps->range(8,24));
		}
		flooded_water = !ps->range(0, 2);
	} else {
		part_max_length_rs = ps->range(2,9);
		tunnel_routepoints = ps->range(10, ps->range(15,30));
	}
	large_cave_is_flat = (ps->range(0,1) == 0);
}

/*
// version with one perlin3d. use with good params like
settings->setDefault("mgindev_np_float_islands1",  "-9.5, 10,  (20,  50,  50 ), 45123, 5, 0.6,  1.5, 5");
void MapgenIndev::generateFloatIslands(int min_y) {
	if (node_min.Y < min_y) return;
	v3s16 p0(node_min.X, node_min.Y, node_min.Z);
	MapNode n1(c_stone), n2(c_desert_stone);
	int xl = node_max.X - node_min.X;
	int yl = node_max.Y - node_min.Y;
	int zl = node_max.Z - node_min.Z;
	u32 index = 0;
	for (int x1 = 0; x1 <= xl; x1++)
	{
		//int x = x1 + node_min.Y;
		for (int z1 = 0; z1 <= zl; z1++)
		{
			//int z = z1 + node_min.Z;
			for (int y1 = 0; y1 <= yl; y1++, index++)
			{
				//int y = y1 + node_min.Y;
				float noise = noise_float_islands1->result[index];
				//dstream << " y1="<<y1<< " x1="<<x1<<" z1="<<z1<< " noise="<<noise << std::endl;
				if (noise > 0 ) {
					v3s16 p = p0 + v3s16(x1, y1, z1);
					u32 i = vm->m_area.index(p);
					if (!vm->m_area.contains(i))
						continue;
					// Cancel if not  air
					if (vm->m_data[i].getContent() != CONTENT_AIR)
						continue;
					vm->m_data[i] = noise > 1 ? n1 : n2;
				}
			}
		}
	}
}
*/

void MapgenIndev::generateFloatIslands(int min_y) {
	if (node_min.Y < min_y) return;
	PseudoRandom pr(blockseed + 985);
	// originally from http://forum.minetest.net/viewtopic.php?id=4776
	float RAR = 0.8 * farscale(0.4, node_min.Y); // 0.4; // Island rarity in chunk layer. -0.4 = thick layer with holes, 0 = 50%, 0.4 = desert rarity, 0.7 = very rare.
	float AMPY = 24; // 24; // Amplitude of island centre y variation.
	float TGRAD = 24; // 24; // Noise gradient to create top surface. Tallness of island top.
	float BGRAD = 24; // 24; // Noise gradient to create bottom surface. Tallness of island bottom.

	v3s16 p0(node_min.X, node_min.Y, node_min.Z);
	MapNode n1(c_stone);

	float xl = node_max.X - node_min.X;
	float yl = node_max.Y - node_min.Y;
	float zl = node_max.Z - node_min.Z;
	u32 zstride = xl + 1;
	float midy = node_min.Y + yl * 0.5;
	u32 index = 0;
	for (int z1 = 0; z1 <= zl; ++z1)
	for (int y1 = 0; y1 <= yl; ++y1)
	for (int x1 = 0; x1 <= xl; ++x1, ++index) {
		int y = y1 + node_min.Y;
		u32 index2d = z1 * zstride + x1;
		float noise3 = noise_float_islands3->result[index2d];
		float pmidy = midy + noise3 / 1.5 * AMPY;
		float noise1 = noise_float_islands1->result[index];
		float offset = y > pmidy ? (y - pmidy) / TGRAD : (pmidy - y) / BGRAD;
		float noise1off = noise1 - offset - RAR;
		if (noise1off > 0 && noise1off < 0.7) {
			float noise2 = noise_float_islands2->result[index];
			if (noise2 - noise1off > -0.7) {
				v3s16 p = p0 + v3s16(x1, y1, z1);
				u32 i = vm->m_area.index(p);
				if (!vm->m_area.contains(i))
					continue;
				// Cancel if not  air
				if (vm->m_data[i].getContent() != CONTENT_AIR)
					continue;
				vm->m_data[i] = n1;
			}
		}
	}
}

void MapgenIndev::generateExperimental() {
	if (float_islands)
		generateFloatIslands(float_islands);
}
