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
#include "util/numeric.h"
#include "log_types.h"
#include "emerge.h"
#include "environment.h"

void Mapgen_features::layers_init(EmergeManager *emerge, const Json::Value & layersj) {

	INodeDefManager *ndef = emerge->ndef;
	layers_n_stone = MapNode(ndef->getId("mapgen_stone"));
	if (!layersj.empty())
		for (int i = 0; i < layersj.size(); ++i) {
			if (layersj[i].empty())
				continue;
			const auto & layerj = layersj[i];
			const auto & name = layerj["name"].asString();
			if (name.empty())
				continue;
			auto content = ndef->getId(name);
			if (content == CONTENT_IGNORE)
				continue;

			auto layer = layer_data{ content, MapNode(content, layerj["param1"].asInt(), layerj["param2"].asInt()) };
			layer.height_min = layerj.get("height_min", -MAP_GENERATION_LIMIT).asInt();
			layer.height_max = layerj.get("height_max", +MAP_GENERATION_LIMIT).asInt();
			layer.thickness  = layerj.get("thickness", 1).asInt();

			layers.emplace_back(layer);
		}
	if (layers.empty()) {
		infostream << "layers empty, using only default:stone"<<std::endl;
	}
}

void Mapgen_features::layers_prepare(const v3s16 & node_min, const v3s16 & node_max) {
	int x = node_min.X;
	int y = node_min.Y;
	int z = node_min.Z;

		noise_layers->perlinMap3D(
			x + 0.33 * noise_layers->np->spread.X * farscale(noise_layers->np->farspread, x, y, z),
			y + 0.33 * noise_layers->np->spread.Y * farscale(noise_layers->np->farspread, x, y, z),
			z + 0.33 * noise_layers->np->spread.Z * farscale(noise_layers->np->farspread, x, y, z)
		);
		noise_layers->transformNoiseMap(x, y, z);

	for (const auto & layer : layers) {
		if (layer.height_max < node_min.Y || layer.height_min > node_max.Y)
			continue;
		for (int i = 0; i < layer.thickness; ++i)
			layers_node.emplace_back(layer.node);
	}

	if (layers_node.empty()) {
		layers_node.emplace_back(layers_n_stone);
	}
}

MapNode Mapgen_features::layers_get(int index) {
	auto layer_index = rangelim(myround((noise_layers->result[index]/((noise_layers->np->offset+noise_layers->np->scale) - (noise_layers->np->offset-noise_layers->np->scale))) * layers_node.size()),0, layers_node.size()-1);
	return layers_node[layer_index];
}

Mapgen_features::~Mapgen_features() {
	delete noise_layers;
}


MapgenIndev::MapgenIndev(int mapgenid, MapgenParams *params, EmergeManager *emerge)
	: MapgenV6(mapgenid, params, emerge)
{
	sp = (MapgenIndevParams *)params->sparams;

	this->ystride = csize.X;
	this->zstride = csize.X * csize.Y;

	noise_float_islands1  = new Noise(&sp->np_float_islands1, seed, csize.X, csize.Y, csize.Z);
	noise_float_islands2  = new Noise(&sp->np_float_islands2, seed, csize.X, csize.Y, csize.Z);
	noise_float_islands3  = new Noise(&sp->np_float_islands3, seed, csize.X, csize.Z);

	noise_layers          = new Noise(&sp->np_layers,         seed, csize.X, csize.Y, csize.Z);
	layers_init(emerge, sp->paramsj["layers"]);
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

		if (sp->float_islands && y >= sp->float_islands) {
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

	layers_prepare(node_min, node_max);
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
	np_layers          = NoiseParams(500,  500, v3f(100, 100, 100), 3663,  3, 0.6,  1,   5,   0.5);
}

void MapgenIndevParams::readParams(Settings *settings) {
	MapgenV6Params::readParams(settings);

	paramsj = settings->getJson("mg_params", paramsj);

	settings->getS16NoEx("mg_float_islands", float_islands);

	settings->getNoiseIndevParams("mgindev_np_terrain_base",   np_terrain_base);
	settings->getNoiseIndevParams("mgindev_np_terrain_higher", np_terrain_higher);
	settings->getNoiseIndevParams("mgindev_np_steepness",      np_steepness);
	settings->getNoiseIndevParams("mgindev_np_height_select",  np_height_select);
	settings->getNoiseIndevParams("mgindev_np_mud",            np_mud);
	settings->getNoiseIndevParams("mgindev_np_beach",          np_beach);
	settings->getNoiseIndevParams("mgindev_np_biome",          np_biome);
	settings->getNoiseIndevParams("mg_np_float_islands1", np_float_islands1);
	settings->getNoiseIndevParams("mg_np_float_islands2", np_float_islands2);
	settings->getNoiseIndevParams("mg_np_float_islands3", np_float_islands3);
	settings->getNoiseIndevParams("mg_np_layers",         np_layers);
}

void MapgenIndevParams::writeParams(Settings *settings) {
	MapgenV6Params::writeParams(settings);

	//settings->setJson("mg_indev", paramsj);

	settings->setS16("mg_float_islands", float_islands);

	settings->setNoiseIndevParams("mgindev_np_terrain_base",   np_terrain_base);
	settings->setNoiseIndevParams("mgindev_np_terrain_higher", np_terrain_higher);
	settings->setNoiseIndevParams("mgindev_np_steepness",      np_steepness);
	settings->setNoiseIndevParams("mgindev_np_height_select",  np_height_select);
	settings->setNoiseIndevParams("mgindev_np_mud",            np_mud);
	settings->setNoiseIndevParams("mgindev_np_beach",          np_beach);
	settings->setNoiseIndevParams("mgindev_np_biome",          np_biome);
	settings->setNoiseIndevParams("mg_np_float_islands1", np_float_islands1);
	settings->setNoiseIndevParams("mg_np_float_islands2", np_float_islands2);
	settings->setNoiseIndevParams("mg_np_float_islands3", np_float_islands3);
	settings->setNoiseIndevParams("mg_np_layers",         np_layers);
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
				vm->m_data[i] = layers_get(index);
			}
		}
	}
	//if (generated)
	//	dustTopNodes();
}

void MapgenIndev::generateExperimental() {
	if (sp->float_islands)
		generateFloatIslands(sp->float_islands);
}

int MapgenIndev::generateGround() {
	//TimeTaker timer1("Generating ground level");
	MapNode n_air(CONTENT_AIR), n_water_source(c_water_source);
	MapNode n_stone(c_stone), n_desert_stone(c_desert_stone);
	MapNode n_ice(c_ice), n_dirt(c_dirt),n_sand(c_sand), n_gravel(c_gravel), n_lava_source(c_lava_source);
	int stone_surface_max_y = -MAP_GENERATION_LIMIT;
	u32 index = 0;

	for (s16 z = node_min.Z; z <= node_max.Z; z++)
	for (s16 x = node_min.X; x <= node_max.X; x++, index++) {
		// Surface height
		s16 surface_y = (s16)baseTerrainLevelFromMap(index);
		
		// Log it
		if (surface_y > stone_surface_max_y)
			stone_surface_max_y = surface_y;

		BiomeType bt = getBiome(index, v2s16(x, z));
		
		s16 heat = emerge->env->m_use_weather ? emerge->env->getServerMap().updateBlockHeat(emerge->env, v3s16(x,node_max.Y,z), nullptr, &heat_cache) : 0;

		// Fill ground with stone
		v3s16 em = vm->m_area.getExtent();
		u32 i = vm->m_area.index(x, node_min.Y, z);

		for (s16 y = node_min.Y; y <= node_max.Y; y++) {
			if (vm->m_data[i].getContent() == CONTENT_IGNORE) {

				if (y <= surface_y) {
					int index3 = (z - node_min.Z) * zstride +
						(y - node_min.Y) * ystride +
						(x - node_min.X);

					vm->m_data[i] = (y > water_level - surface_y && bt == BT_DESERT) ?
						n_desert_stone : layers_get(index3);
				} else if (y <= water_level) {
					vm->m_data[i] = (heat < 0 && y > heat/3) ? n_ice : n_water_source;
				} else {
					vm->m_data[i] = n_air;
				}
			}
			vm->m_area.add_y(em, i, 1);
		}
	}
	
	return stone_surface_max_y;
}

