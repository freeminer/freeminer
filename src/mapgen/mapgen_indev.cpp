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
#include "mapgen/mapgen_v6.h"
#include "serverenvironment.h"
#include "util/numeric.h"
#include "log_types.h"
#include "emerge.h"
#include "environment.h"
#include "settings.h"

#include "mapgen/cavegen.h"

void Mapgen_features::layers_init(EmergeParams *emerge, const Json::Value &paramsj)
{
	const auto &layersj = paramsj["layers"];
	auto *ndef = emerge->ndef;
	auto layer_default_thickness = paramsj.get("layer_default_thickness", 1).asInt();
	auto layer_thickness_multiplier =
			paramsj.get("layer_thickness_multiplier", 1).asInt();
	if (!layersj.empty())
		for (unsigned int i = 0; i < layersj.size(); ++i) {
			if (layersj[i].empty())
				continue;
			const auto &layerj = layersj[i];
			const auto &name = layerj["name"].asString();
			if (name.empty())
				continue;
			auto content = ndef->getId(name);
			if (content == CONTENT_IGNORE)
				continue;

			auto layer = layer_data{content,
					MapNode(content, layerj["param1"].asInt(), layerj["param2"].asInt())};
			layer.height_min =
					layerj.get("y_min",
								  layerj.get("height_min", -MAX_MAP_GENERATION_LIMIT)
										  .asInt())
							.asInt();
			layer.height_max =
					layerj.get("y_max",
								  layerj.get("height_max", +MAX_MAP_GENERATION_LIMIT)
										  .asInt())
							.asInt();
			layer.thickness = layerj.get("thickness", layer_default_thickness).asInt() *
							  layer_thickness_multiplier;

			//layer.name = name; //dev
			layers.emplace_back(layer);
		}
	if (layers.empty())
		infostream << "layers empty, using only default:stone mg_params=" << paramsj
				   << std::endl;
	else
		verbosestream << "layers size=" << layers.size() << std::endl;
}

void Mapgen_features::layers_prepare(const v3pos_t &node_min, const v3pos_t &node_max)
{
	noise_layers->perlinMap3D(node_min.X, node_min.Y - y_oversize_down, node_min.Z);

	noise_layers_width = ((noise_layers->np.offset + noise_layers->np.scale) -
						  (noise_layers->np.offset - noise_layers->np.scale));

	layers_node.clear();
	for (const auto &layer : layers) {
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

MapNode Mapgen_features::layers_get(unsigned int index)
{
	const auto layer_index = rangelim(
			(unsigned int)myround((noise_layers->result[index] / noise_layers_width) *
								  layers_node_size),
			0, layers_node_size - 1);
	//errorstream<<"ls: index="<<index<< " layer_index="<<layer_index<<" off="<<noise_layers->np.offset<<" sc="<<noise_layers->np.scale<<noise_layers_width="<<noise_layers_width<<" layers_node_size="<<layers_node_size<<std::endl;
	return layers_node[layer_index];
}

void Mapgen_features::cave_prepare(
		const v3pos_t &node_min, const v3pos_t &node_max, int max_y)
{
	if (!max_y || node_min.Y > max_y) {
		cave_noise_threshold = 0;
		return;
	}
	noise_cave_indev->perlinMap3D(node_min.X, node_min.Y - y_oversize_down, node_min.Z);
	cave_noise_threshold = 800;
}

Mapgen_features::Mapgen_features(MapgenParams *params, EmergeParams *emerge)
{
	auto ndef = emerge->ndef;
	n_stone = MapNode(ndef->getId("mapgen_stone"));
}

Mapgen_features::~Mapgen_features()
{
	delete noise_layers;
	noise_layers = nullptr;
	delete noise_cave_indev;
	noise_cave_indev = nullptr;
}

MapgenIndev::MapgenIndev(MapgenIndevParams *params, EmergeParams *emerge) :
		MapgenV6(params, emerge), Mapgen_features(params, emerge)
{
	spflags = params->spflags;

	//sp = (MapgenIndevParams *)params->sparams;
	sp = params;

	// Number of elements to skip to get to the next Y coordinate
	this->ystride = csize.X;

	// Number of elements to skip to get to the next Z coordinate
	this->zstride = csize.X * (csize.Y + y_oversize_down + y_oversize_up);

	// Z-stride value for maps oversized for 1-down overgeneration
	//this->zstride_1d = csize.X * (csize.Y + 1);

	// Z-stride value for maps oversized for 1-up 1-down overgeneration
	//this->zstride_1u1d = csize.X * (csize.Y + 2);

	floatland_ymin = params->floatland_ymin;
	floatland_ymax = params->floatland_ymax;
	floatland_taper = params->floatland_taper;
	float_taper_exp = params->float_taper_exp;
	floatland_density = params->floatland_density;
	floatland_ywater = params->floatland_ywater;

	noise_layers =
			new Noise(&sp->np_layers, seed, csize.X, csize.Y + y_oversize_down + y_oversize_up, csize.Z);
	layers_init(emerge, sp->paramsj);

	noise_cave_indev = new Noise(
			&sp->np_cave_indev, seed, csize.X, csize.Y + y_oversize_down + y_oversize_up, csize.Z);

	if (spflags & MGV6_FLOATLANDS) {

		// Allocate floatland noise offset cache
		this->float_offset_cache = new float[csize.Y + 2];

		// 3D noise, 1 up, 1 down overgeneration
		noise_floatland =
				new Noise(&params->np_floatland, seed, csize.X, csize.Y + y_oversize_down + y_oversize_up, csize.Z);
	}
}

MapgenIndev::~MapgenIndev()
{
	if (spflags & MGV6_FLOATLANDS) {
		delete noise_floatland;
		delete[] float_offset_cache;
	}
}

void MapgenIndev::calculateNoise()
{
	MapgenV6::calculateNoise();

	layers_prepare(node_min, node_max);

	int ocean_min =
			baseTerrainLevel(
					sp->np_terrain_base.offset - farscale(sp->np_terrain_base.far_scale,
														 node_min.X, node_min.Z) *
														 sp->np_terrain_base.scale,
					sp->np_terrain_higher.offset -
							farscale(sp->np_terrain_higher.far_scale, node_min.X,
									node_min.Z) *
									sp->np_terrain_higher.scale,
					sp->np_steepness.offset -
							farscale(sp->np_steepness.far_scale, node_min.X, node_min.Z) *
									sp->np_steepness.scale,
					sp->np_height_select.offset - farscale(sp->np_height_select.far_scale,
														  node_min.X, node_min.Z) *
														  sp->np_height_select.scale) -
			MAP_BLOCKSIZE;

	cave_prepare(node_min, node_max, sp->paramsj.get("cave_indev", ocean_min).asInt());
}

MapgenIndevParams::MapgenIndevParams()
{
	np_terrain_base = NoiseParams(-4, 20, v3f(250, 250, 250), 82341, 5, 0.6, 2.0,
			NOISE_FLAG_DEFAULTS, 10, 1, 0.2);
	np_terrain_higher = NoiseParams(20, 16, v3f(500, 500, 500), 85039, 5, 0.6, 2.0,
			NOISE_FLAG_DEFAULTS, 10, 1, 0.3);
	np_steepness = NoiseParams(0.85, 0.5, v3f(125, 125, 125), -932, 5, 0.7, 2.0,
			NOISE_FLAG_DEFAULTS, 2, 1, 0.5);
	np_height_select = NoiseParams(0.5, 1, v3f(250, 250, 250), 4213, 5, 0.69, 2.0,
			NOISE_FLAG_DEFAULTS, 2, 1, 0.5);
	np_mud = NoiseParams(
			4, 2, v3f(200, 200, 200), 91013, 3, 0.55, 2.0, NOISE_FLAG_DEFAULTS, 1, 1, 1);
	np_beach = NoiseParams(
			0, 1, v3f(250, 250, 250), 59420, 3, 0.50, 2.0, NOISE_FLAG_DEFAULTS, 1, 1, 1);
	np_biome = NoiseParams(
			0, 1, v3f(250, 250, 250), 9130, 3, 0.50, 2.0, NOISE_FLAG_DEFAULTS, 1, 1, 1);
	np_floatland = NoiseParams(0.0, 0.6, v3f(384, 96, 384), 1009, 4, 0.75, 1.618,
			NOISE_FLAG_DEFAULTS, 2, 1, 0.9);
	np_layers = NoiseParams(500, 500, v3f(100, 100, 100), 3663, 5, 0.6, 2.0,
			NOISE_FLAG_DEFAULTS, 1, 1, 0.5);
	np_cave_indev = NoiseParams(
			0, 1000, v3f(500, 500, 500), 3664, 4, 0.8, 2.0, NOISE_FLAG_DEFAULTS, 4, 1, 1);
}

void MapgenIndevParams::readParams(const Settings *settings)
{
	MapgenV6Params::readParams(settings);

	auto mg_params = settings->getJson("mg_params", paramsj);
	if (!mg_params.isNull())
		paramsj = mg_params;

	settings->getPosNoEx("mgindev_floatland_ymin", floatland_ymin);
	settings->getPosNoEx("mgindev_floatland_ymax", floatland_ymax);
	settings->getPosNoEx("mgindev_floatland_taper", floatland_taper);
	settings->getFloatNoEx("mgindev_float_taper_exp", float_taper_exp);
	settings->getFloatNoEx("mgindev_floatland_density", floatland_density);
	settings->getPosNoEx("mgindev_floatland_ywater", floatland_ywater);

	settings->getNoiseParamsFromGroup("mgindev_np_terrain_base", np_terrain_base);
	settings->getNoiseParamsFromGroup("mgindev_np_terrain_higher", np_terrain_higher);
	settings->getNoiseParamsFromGroup("mgindev_np_steepness", np_steepness);
	settings->getNoiseParamsFromGroup("mgindev_np_height_select", np_height_select);
	settings->getNoiseParamsFromGroup("mgindev_np_mud", np_mud);
	settings->getNoiseParamsFromGroup("mgindev_np_beach", np_beach);
	settings->getNoiseParamsFromGroup("mgindev_np_biome", np_biome);
	settings->getNoiseParamsFromGroup("mgindev_np_floatland", np_floatland);
	settings->getNoiseParamsFromGroup("mg_np_layers", np_layers);
	settings->getNoiseParamsFromGroup("mgindev_np_cave_indev", np_cave_indev);
}

void MapgenIndevParams::writeParams(Settings *settings) const
{
	MapgenV6Params::writeParams(settings);

	settings->setJson("mg_params", paramsj);

	settings->setPos("mgindev_floatland_ymin", floatland_ymin);
	settings->setPos("mgindev_floatland_ymax", floatland_ymax);
	settings->setPos("mgindev_floatland_taper", floatland_taper);
	settings->setFloat("mgindev_float_taper_exp", float_taper_exp);
	settings->setFloat("mgindev_floatland_density", floatland_density);
	settings->setPos("mgindev_floatland_ywater", floatland_ywater);

	settings->setNoiseParams("mgindev_np_terrain_base", np_terrain_base);
	settings->setNoiseParams("mgindev_np_terrain_higher", np_terrain_higher);
	settings->setNoiseParams("mgindev_np_steepness", np_steepness);
	settings->setNoiseParams("mgindev_np_height_select", np_height_select);
	settings->setNoiseParams("mgindev_np_mud", np_mud);
	settings->setNoiseParams("mgindev_np_beach", np_beach);
	settings->setNoiseParams("mgindev_np_biome", np_biome);
	settings->setNoiseParams("mgindev_np_floatland", np_floatland);

	settings->setNoiseParams("mg_np_layers", np_layers);
	settings->setNoiseParams("mgindev_np_cave_indev", np_cave_indev);
}

void MapgenIndevParams::setDefaultSettings(Settings *settings)
{
	settings->setDefault("mgindev_spflags", flagdesc_mapgen_v6,
			MGV6_JUNGLES | MGV6_SNOWBIOMES | MGV6_TREES | MGV6_BIOMEBLEND | MGV6_MUDFLOW |
					MGV6_FLOATLANDS);

	settings->setDefault("mgv6_spflags", flagdesc_mapgen_v6,
			MGV6_JUNGLES | MGV6_SNOWBIOMES | MGV6_TREES | MGV6_BIOMEBLEND | MGV6_MUDFLOW |
					MGV6_FLOATLANDS);
	;
}

void MapgenIndev::generateCaves(int max_stone_y)
{
	MapgenV6::generateCaves(max_stone_y);
	return;

	/*
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
	
	if (getBiome(node_min) == BT_DESERT) {
		caves_count   /= 3;
		bruises_count /= 3;
	}
	
	for (u32 i = 0; i < caves_count + bruises_count; i++) {
		bool large_cave = (i >= caves_count);
		CaveIndev cave(this, &ps, &ps2, node_min, large_cave);

		cave.makeCave(node_min, node_max, max_stone_y);
	}
*/
}

//not used
#if 0
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
	} else {
		part_max_length_rs = ps->range(2,9);
		tunnel_routepoints = ps->range(10, ps->range(15,30));
	}
	large_cave_is_flat = (ps->range(0,1) == 0);
}
#endif

void MapgenIndev::generateExperimental()
{
}

int MapgenIndev::generateGround()
{
	//TimeTaker timer1("Generating ground level");
	MapNode n_air(CONTENT_AIR), n_water_source(c_water_source);
	MapNode n_stone(c_stone), n_desert_stone(c_desert_stone);
	MapNode n_ice(c_ice), n_dirt(c_dirt), n_sand(c_sand), n_gravel(c_gravel),
			n_lava_source(c_lava_source);

	//// Floatlands
	// 'Generate floatlands in this mapchunk' bool for
	// simplification of condition checks in y-loop.
	bool gen_floatlands = false;
	u8 cache_index = 0;
	// Y values where floatland tapering starts
	pos_t float_taper_ymax = floatland_ymax - floatland_taper;
	pos_t float_taper_ymin = floatland_ymin + floatland_taper;

	if ((spflags & MGV6_FLOATLANDS) && node_max.Y >= floatland_ymin &&
			node_min.Y <= floatland_ymax) {
		gen_floatlands = true;
		// Calculate noise for floatland generation
		noise_floatland->perlinMap3D(node_min.X, node_min.Y - y_oversize_down, node_min.Z);

		// Cache floatland noise offset values, for floatland tapering
		for (pos_t y = node_min.Y - y_oversize_down; y <= node_max.Y + y_oversize_up; y++, cache_index++) {
			float float_offset = 0.0f;
			if (y > float_taper_ymax) {
				float_offset = std::pow((y - float_taper_ymax) / (float)floatland_taper,
									   float_taper_exp) *
							   4.0f;
			} else if (y < float_taper_ymin) {
				float_offset = std::pow((float_taper_ymin - y) / (float)floatland_taper,
									   float_taper_exp) *
							   4.0f;
			}
			float_offset_cache[cache_index] = float_offset;
		}
	}

	int stone_surface_max_y = -MAX_MAP_GENERATION_LIMIT;
	u32 index = 0;

	for (pos_t z = node_min.Z; z <= node_max.Z; z++)
		for (pos_t x = node_min.X; x <= node_max.X; x++, index++) {
			// Surface height
			pos_t surface_y = (pos_t)baseTerrainLevelFromMap(index);

			// Log it
			if (surface_y > stone_surface_max_y)
				stone_surface_max_y = surface_y;

			auto bt = getBiome(index, v3pos_t(x, surface_y, z));

			const auto heat =
					m_emerge->env->m_use_weather
							? m_emerge->env->getServerMap().updateBlockHeat(m_emerge->env,
									  v3pos_t(x, node_max.Y, z), nullptr, &heat_cache)
							: 0;

			// Fill ground with stone
			v3pos_t em = vm->m_area.getExtent();
			u32 i = vm->m_area.index(x, node_min.Y, z);

			cache_index = 0;
			u32 index3d = (z - node_min.Z) * zstride + (x - node_min.X);

			for (pos_t y = node_min.Y - y_oversize_down; y <= node_max.Y + y_oversize_up; y++, index3d += ystride, cache_index++) {
				if (!vm->m_data[i]) {

					if (y <= surface_y) {
						if (cave_noise_threshold && noise_cave_indev->result[index3d] >
															cave_noise_threshold) {
							vm->m_data[i] = n_air;
						} else {
							auto n = (y > water_level - surface_y && bt == BT_DESERT)
											 ? n_desert_stone
											 : layers_get(index3d);
							bool protect = n.getContent() != CONTENT_AIR;
							if (cave_noise_threshold &&
									noise_cave_indev->result[index3d] >
											cave_noise_threshold - 50) {
								vm->m_data[i] =
										protect ? n_stone : n; //cave shell without layers
								protect = true;
							} else {
								vm->m_data[i] = n;
							}
							if (protect)
								vm->m_flags[i] |= VOXELFLAG_CHECKED2; // no cave liquid
						}

					} else if (gen_floatlands &&
							   getFloatlandTerrainFromMap(
									   index3d, float_offset_cache[cache_index])) {
						//vm->m_data[vi] = n_stone; // Floatland terrain
						vm->m_data[i] = layers_get(index3d);
						if (y > stone_surface_max_y)
							stone_surface_max_y = y;

					} else if (y <= water_level) {
						vm->m_data[i] =
								(heat < 0 && y > heat / 3) ? n_ice : n_water_source;
						if (liquid_pressure && y <= 0)
							vm->m_data[i].addLevel(m_emerge->ndef, water_level - y, 1);
					} else if (gen_floatlands && y >= float_taper_ymax &&
							   y <= floatland_ywater) {
						//vm->m_data[vi] = n_water; // Water for solid floatland layer only
						vm->m_data[i] = (heat < 0 && y - float_taper_ymax > heat / 3)
												? n_ice
												: n_water_source;
					} else {
						vm->m_data[i] = n_air;
					}
				}
				vm->m_area.add_y(em, i, 1);
			}
		}

	return stone_surface_max_y;
}

bool MapgenIndev::getFloatlandTerrainFromMap(int idx_xyz, float float_offset)
{
	return noise_floatland->result[idx_xyz] + floatland_density - float_offset >= 0.0f;
}
