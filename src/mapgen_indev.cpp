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
#include "settings.h"

void Mapgen_features::layers_init(EmergeManager *emerge, const Json::Value & paramsj) {
	const auto & layersj = paramsj["layers"];
	INodeDefManager *ndef = emerge->ndef;
	auto layer_default_thickness = paramsj.get("layer_default_thickness", 1).asInt();
	auto layer_thickness_multiplier = paramsj.get("layer_thickness_multiplier", 1).asInt();
	if (!layersj.empty())
		for (unsigned int i = 0; i < layersj.size(); ++i) {
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
			layer.thickness  = layerj.get("thickness", layer_default_thickness).asInt() * layer_thickness_multiplier;

			//layer.name = name; //dev
			layers.emplace_back(layer);
		}
	if (layers.empty())
		infostream << "layers empty, using only default:stone mg_params="<<paramsj<<std::endl;
	else
		verbosestream << "layers size=" << layers.size() << std::endl;
}

void Mapgen_features::layers_prepare(const v3POS & node_min, const v3POS & node_max) {
	int x = node_min.X;
	int y = node_min.Y;
	int z = node_min.Z;

	noise_layers->perlinMap3D_PO(x, 0.33, y, 0.33, z, 0.33);

	noise_layers_width = ((noise_layers->np.offset+noise_layers->np.scale) - (noise_layers->np.offset-noise_layers->np.scale));

	layers_node.clear();
	for (const auto & layer : layers) {
		if (layer.height_max < node_min.Y || layer.height_min > node_max.Y)
			continue;
		for (int i = 0; i < layer.thickness; ++i) {
			layers_node.emplace_back(layer.node);
		}
	}

	if (layers_node.empty()) {
		layers_node.emplace_back(n_stone);
	}
	layers_node_size = layers_node.size();
	//infostream<<"layers_prepare "<<node_min<<" "<< node_max<<" w="<<noise_layers_width<<" sz="<<layers_node_size<<std::endl;
}

MapNode Mapgen_features::layers_get(unsigned int index) {
	auto layer_index = rangelim((unsigned int)myround((noise_layers->result[index] / noise_layers_width) * layers_node_size), 0, layers_node_size-1);
	//errorstream<<"ls: index="<<index<< " layer_index="<<layer_index<<" off="<<noise_layers->np.offset<<" sc="<<noise_layers->np.scale<<" noise_layers_width="<<noise_layers_width<<" layers_node_size="<<layers_node_size<<std::endl;
	return layers_node[layer_index];
}

void Mapgen_features::float_islands_prepare(const v3POS & node_min, const v3POS & node_max, int min_y) {
	int x = node_min.X;
	int y = node_min.Y;
	int z = node_min.Z;
	if (min_y && y >= min_y) {
		noise_float_islands1->perlinMap3D_PO(x, 0.33, y, 0.33, z, 0.33);
		noise_float_islands2->perlinMap3D_PO(x, 0.33, y, 0.33, z, 0.33);
		noise_float_islands3->perlinMap2D_PO(x, 0.5, z, 0.5);
	}
}


Mapgen_features::Mapgen_features(int mapgenid, MapgenParams *params, EmergeManager *emerge) :
	noise_layers(nullptr),
	layers_node_size(0),
	noise_float_islands1(nullptr),
	noise_float_islands2(nullptr),
	noise_float_islands3(nullptr)
{
	auto ndef = emerge->ndef;
	n_stone = MapNode(ndef->getId("mapgen_stone"));
}

Mapgen_features::~Mapgen_features() {
	if (noise_layers)
		delete noise_layers;
	if (noise_float_islands1)
		delete noise_float_islands1;
	if (noise_float_islands2)
		delete noise_float_islands2;
	if (noise_float_islands3)
		delete noise_float_islands3;

}


MapgenIndev::MapgenIndev(int mapgenid, MapgenParams *params, EmergeManager *emerge)
	: MapgenV6(mapgenid, params, emerge)
	, Mapgen_features(mapgenid, params, emerge)
{
	sp = (MapgenIndevParams *)params->sparams;

	this->ystride = csize.X;
	this->zstride = csize.X * csize.Y;

	noise_float_islands1  = new Noise(&sp->np_float_islands1, seed, csize.X, csize.Y, csize.Z);
	noise_float_islands2  = new Noise(&sp->np_float_islands2, seed, csize.X, csize.Y, csize.Z);
	noise_float_islands3  = new Noise(&sp->np_float_islands3, seed, csize.X, csize.Z);

	noise_layers          = new Noise(&sp->np_layers,         seed, csize.X, csize.Y, csize.Z);
	layers_init(emerge, sp->paramsj);
}

MapgenIndev::~MapgenIndev() {
}

void MapgenIndev::calculateNoise() {
	MapgenV6::calculateNoise();
	if (!(flags & MG_FLAT)) {
		float_islands_prepare(node_min, node_max, sp->float_islands);
	}

	layers_prepare(node_min, node_max);
}

MapgenIndevParams::MapgenIndevParams() {
	float_islands = 500;
	np_terrain_base    = NoiseParams(-4,   20,  v3f(250, 250, 250), 82341, 5, 0.6,  2.0, NOISE_FLAG_DEFAULTS, 10,  10,  0.5);
	np_terrain_higher  = NoiseParams(20,   16,  v3f(500, 500, 500), 85039, 5, 0.6,  2.0, NOISE_FLAG_DEFAULTS, 10,  10,  0.5);
	np_steepness       = NoiseParams(0.85, 0.5, v3f(125, 125, 125), -932,  5, 0.7,  2.0, NOISE_FLAG_DEFAULTS, 2,   10,  0.5);
	np_height_select   = NoiseParams(0.5,  1,   v3f(250, 250, 250), 4213,  5, 0.69, 2.0, NOISE_FLAG_DEFAULTS, 10,  10,  0.5);
	np_mud             = NoiseParams(4,    2,   v3f(200, 200, 200), 91013, 3, 0.55, 2.0, NOISE_FLAG_DEFAULTS, 1,   1,   1);
	np_beach           = NoiseParams(0,    1,   v3f(250, 250, 250), 59420, 3, 0.50, 2.0, NOISE_FLAG_DEFAULTS, 1,   1,   1);
	np_biome           = NoiseParams(0,    1,   v3f(250, 250, 250), 9130,  3, 0.50, 2.0, NOISE_FLAG_DEFAULTS, 1,   10,  1);
	np_float_islands1  = NoiseParams(0,    1,   v3f(256, 256, 256), 3683,  6, 0.6,  2.0, NOISE_FLAG_DEFAULTS, 1,   1.5, 1);
	np_float_islands2  = NoiseParams(0,    1,   v3f(8,   8,   8  ), 9292,  2, 0.5,  2.0, NOISE_FLAG_DEFAULTS, 1,   1.5, 1);
	np_float_islands3  = NoiseParams(0,    1,   v3f(256, 256, 256), 6412,  2, 0.5,  2.0, NOISE_FLAG_DEFAULTS, 1,   0.5, 1);
	np_layers          = NoiseParams(500,  500, v3f(100, 50,  100), 3663,  5, 0.6,  2.0, NOISE_FLAG_DEFAULTS, 1,   5,   0.5);
}

void MapgenIndevParams::readParams(Settings *settings) {
	MapgenV6Params::readParams(settings);

	paramsj = settings->getJson("mg_params", paramsj);
	settings->getS16NoEx("mg_float_islands", float_islands);

	settings->getNoiseParamsFromGroup("mgindev_np_terrain_base",   np_terrain_base);
	settings->getNoiseParamsFromGroup("mgindev_np_terrain_higher", np_terrain_higher);
	settings->getNoiseParamsFromGroup("mgindev_np_steepness",      np_steepness);
	settings->getNoiseParamsFromGroup("mgindev_np_height_select",  np_height_select);
	settings->getNoiseParamsFromGroup("mgindev_np_mud",            np_mud);
	settings->getNoiseParamsFromGroup("mgindev_np_beach",          np_beach);
	settings->getNoiseParamsFromGroup("mgindev_np_biome",          np_biome);
	settings->getNoiseParamsFromGroup("mg_np_float_islands1", np_float_islands1);
	settings->getNoiseParamsFromGroup("mg_np_float_islands2", np_float_islands2);
	settings->getNoiseParamsFromGroup("mg_np_float_islands3", np_float_islands3);
	settings->getNoiseParamsFromGroup("mg_np_layers",         np_layers);
}

void MapgenIndevParams::writeParams(Settings *settings) {
	MapgenV6Params::writeParams(settings);

	settings->setJson("mg_params", paramsj);

	settings->setS16("mg_float_islands", float_islands);

	settings->setNoiseParams("mgindev_np_terrain_base",   np_terrain_base);
	settings->setNoiseParams("mgindev_np_terrain_higher", np_terrain_higher);
	settings->setNoiseParams("mgindev_np_steepness",      np_steepness);
	settings->setNoiseParams("mgindev_np_height_select",  np_height_select);
	settings->setNoiseParams("mgindev_np_mud",            np_mud);
	settings->setNoiseParams("mgindev_np_beach",          np_beach);
	settings->setNoiseParams("mgindev_np_biome",          np_biome);
	settings->setNoiseParams("mg_np_float_islands1", np_float_islands1);
	settings->setNoiseParams("mg_np_float_islands2", np_float_islands2);
	settings->setNoiseParams("mg_np_float_islands3", np_float_islands3);
	settings->setNoiseParams("mg_np_layers",         np_layers);
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
	
	if (getBiome(v2POS(node_min.X, node_min.Z)) == BT_DESERT) {
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
				v3POS node_min, bool is_large_cave) {
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
void MapgenIndev::float_islands_generate(int min_y) {
	if (node_min.Y < min_y) return;
	v3POS p0(node_min.X, node_min.Y, node_min.Z);
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
					v3POS p = p0 + v3POS(x1, y1, z1);
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

int Mapgen_features::float_islands_generate(const v3POS & node_min, const v3POS & node_max, int min_y, ManualMapVoxelManipulator *vm) {
	int generated = 0;
	if (node_min.Y < min_y) return generated;
	// originally from http://forum.minetest.net/viewtopic.php?id=4776
	float RAR = 0.8 * farscale(0.4, node_min.Y); // 0.4; // Island rarity in chunk layer. -0.4 = thick layer with holes, 0 = 50%, 0.4 = desert rarity, 0.7 = very rare.
	float AMPY = 24; // 24; // Amplitude of island centre y variation.
	float TGRAD = 24; // 24; // Noise gradient to create top surface. Tallness of island top.
	float BGRAD = 24; // 24; // Noise gradient to create bottom surface. Tallness of island bottom.

	v3POS p0(node_min.X, node_min.Y, node_min.Z);

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
				v3POS p = p0 + v3POS(x1, y1, z1);
				u32 i = vm->m_area.index(p);
				if (!vm->m_area.contains(i))
					continue;
				// Cancel if not  air
				if (vm->m_data[i].getContent() != CONTENT_AIR)
					continue;
				vm->m_data[i] = layers_get(index);
				++generated;
			}
		}
	}
	return generated;
}

void MapgenIndev::generateExperimental() {
	if (sp->float_islands)
		float_islands_generate(node_min, node_max, sp->float_islands, vm);
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

		auto bt = getBiome(index, v2POS(x, z));
		
		s16 heat = m_emerge->env->m_use_weather ? m_emerge->env->getServerMap().updateBlockHeat(m_emerge->env, v3POS(x,node_max.Y,z), nullptr, &heat_cache) : 0;

		// Fill ground with stone
		v3POS em = vm->m_area.getExtent();
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

