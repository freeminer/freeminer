/*
Copyright (C) 2026 proller <proler@gmail.com>
*/

/*
This file is part of Freeminer.

Freeminer is free software: you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation, either version 3 of the License, or
(at your option) any later version.

Freeminer is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with Freeminer. If not, see <http://www.gnu.org/licenses/>.
*/

#include "content_abm_core.h"

#include "server.h"
#include "serverenvironment.h"
#include "servermap.h"
#include "util/numeric.h"

#include <algorithm>
#include <array>
#include <utility>

namespace core_abm
{
namespace
{

bool growth_near(ServerMap *map, const NodeDefManager *ndef,
		const v3pos_t &p, int radius, bool include_flowers)
{
	for (int x = -radius; x <= radius; ++x)
	for (int y = -radius; y <= radius; ++y)
	for (int z = -radius; z <= radius; ++z) {
		if (x == 0 && y == 0 && z == 0)
			continue;
		const auto &features = ndef->get(map->getNodeTry(p + v3pos_t(x, y, z)));
		if ((include_flowers && features.getGroup("flower") > 0) ||
				features.getGroup("tree") > 0 || features.getGroup("sapling") > 0)
			return true;
	}
	return false;
}

struct SoilWeatherNodes
{
	content_t dirt;
	content_t grass;
	content_t grass_footsteps;
	content_t dry_dirt;
	content_t dry_grass;
	content_t dry_dirt_grass;
	content_t snow_dirt;
	content_t snow;
	content_t snowblock;
	content_t ice;
	content_t grass_plant;
};

class SoilWeatherABM : public ConfigurableABM
{
	const SoilWeatherNodes m_nodes;
	const std::vector<content_t> m_flower_contents;
	const int m_grass_heat_max;
	const int m_grass_heat_extreme;
	const int m_grass_humidity_min;
	const int m_grass_humidity_dry;
	const int m_grass_light_min;
	const int m_dirt_dry_humidity;
	const bool m_debug_fast;

	void spread_flower(ServerEnvironment *env, ServerMap *map,
			const NodeDefManager *ndef, const v3pos_t &p, content_t flower) const
	{
		if (get_node_light(env, map, ndef, p, false) < 8 ||
				map->updateBlockHeat(env, p) < 5)
			return;

		int flora = 0;
		std::vector<v3pos_t> soils;
		const content_t surface = map->getNodeTry(p - v3pos_t(0, 1, 0)).getContent();
		for (int x = -4; x <= 4; ++x)
		for (int y = -4; y <= 4; ++y)
		for (int z = -4; z <= 4; ++z) {
			const v3pos_t soil_pos = p + v3pos_t(x, y, z);
			const MapNode node = map->getNodeTry(soil_pos);
			if (ndef->get(node).getGroup("flora") > 0 && ++flora > 3)
				return;
			if (node.getContent() == surface &&
					map->getNodeTry(soil_pos + v3pos_t(0, 1, 0)).getContent() ==
							CONTENT_AIR)
				soils.emplace_back(soil_pos);
		}

		if (soils.empty())
			return;
		for (int attempt = 0; attempt < 3; ++attempt) {
			const v3pos_t soil = soils[myrand_range(0, soils.size() - 1)];
			const v3pos_t above = soil + v3pos_t(0, 1, 0);
			if (get_node_light(env, map, ndef, above, false) >= 8)
				map->setNode(above, MapNode(flower));
		}
	}

public:
	SoilWeatherABM(const CoreABMDefinition &definition, const SoilWeatherNodes &nodes,
			std::vector<content_t> flower_contents) :
			ConfigurableABM(definition),
			m_nodes(nodes), m_flower_contents(std::move(flower_contents)),
			m_grass_heat_max(get_int(definition, "grass_heat_max", 51)),
			m_grass_heat_extreme(get_int(definition, "grass_heat_extreme", 71)),
			m_grass_humidity_min(get_int(definition, "grass_humidity_min", 4)),
			m_grass_humidity_dry(get_int(definition, "grass_humidity_dry", 40)),
			m_grass_light_min(get_int(definition, "grass_light_min", 6)),
			m_dirt_dry_humidity(get_int(definition, "dirt_dry_humidity", 10)),
			m_debug_fast(get_bool(definition, "debug_fast", false))
	{
	}

	void trigger(ServerEnvironment *env, v3pos_t p, MapNode n,
			u32 active_object_count, u32 active_object_count_wider, v3pos_t neighbor_pos,
			uint8_t activate) override
	{
		ServerMap *map = &env->getServerMap();
		const auto *ndef = env->getGameDef()->ndef();
		const content_t content = n.getContent();
		const v3pos_t top_pos = p + v3pos_t(0, 1, 0);
		const MapNode top_node = map->getNodeTry(top_pos);
		if (top_node.getContent() == CONTENT_IGNORE)
			return;

		const v3pos_t bottom_pos = p - v3pos_t(0, 1, 0);
		const content_t bottom = map->getNodeTry(bottom_pos).getContent();
		const bool bottom_air = bottom == CONTENT_AIR || bottom == CONTENT_IGNORE;
		const int light_day = get_node_light(env, map, ndef, top_pos, true);
		const int light = get_node_light(env, map, ndef, top_pos, false);
		const int heat = map->updateBlockHeat(env, p);
		const int humidity = map->updateBlockHumidity(env, p);
		content_t replacement = content;

		const auto &top_features = ndef->get(top_node);
		const bool open_cover =
				(top_features.sunlight_propagates || top_features.param_type == CPT_LIGHT) &&
				top_features.liquid_type == LIQUID_NONE;
		if (!open_cover) {
			if (!bottom_air &&
					(content == m_nodes.dry_grass || content == m_nodes.dry_dirt_grass))
				replacement = m_nodes.dry_dirt;
			else if (content == m_nodes.grass)
				replacement = m_nodes.dirt;
		} else if (!bottom_air) {
			if (top_node.getContent() == m_nodes.snow ||
					top_node.getContent() == m_nodes.snowblock ||
					top_node.getContent() == m_nodes.ice) {
				replacement = m_nodes.snow_dirt;
			} else if (top_node.getContent() == CONTENT_AIR) {
				if ((content == m_nodes.grass || content == m_nodes.grass_footsteps) &&
						(light_day < m_grass_light_min ||
								(heat > m_grass_heat_max &&
										humidity < m_grass_humidity_dry) ||
								humidity < 1 || heat > m_grass_heat_extreme)) {
					replacement = humidity < m_dirt_dry_humidity ?
							m_nodes.dry_dirt_grass : m_nodes.dry_grass;
				} else if (content == m_nodes.dirt &&
						(light_day < m_grass_light_min ||
								(heat > m_grass_heat_max &&
										humidity < m_grass_humidity_dry) ||
								humidity < m_grass_humidity_min ||
								heat > m_grass_heat_extreme)) {
					replacement = m_nodes.dry_dirt;
				}

				if (content == m_nodes.dirt) {
					if (heat < -5 && humidity > 5) {
						replacement = m_nodes.snow_dirt;
					} else if (heat > 5 && heat < m_grass_heat_max &&
							humidity > m_grass_humidity_min &&
							light >= m_grass_light_min) {
						replacement = m_nodes.grass;
					}
				}
			}
		}

		int air_sides = 0;
		for (const v3pos_t &side : {v3pos_t(-1, 0, 0), v3pos_t(1, 0, 0),
				v3pos_t(0, 0, -1), v3pos_t(0, 0, 1)}) {
			if (map->getNodeTry(p + side).getContent() == CONTENT_AIR)
				++air_sides;
		}

		const int roll = myrand_range(1, m_debug_fast ? 1 : 1000);
		if (roll < 10 && content != m_nodes.dirt && bottom == CONTENT_AIR &&
				top_node.getContent() == CONTENT_AIR && air_sides >= 2)
			replacement = m_nodes.dirt;

		if (replacement != content) {
			n.setContent(replacement);
			map->setNode(p, n);
			return;
		}

		if ((content != m_nodes.grass && content != m_nodes.grass_footsteps) ||
				top_node.getContent() != CONTENT_AIR || heat <= 5 ||
				heat >= m_grass_heat_max || humidity <= m_grass_humidity_min ||
				light < m_grass_light_min ||
				(activate != 1 && myrand_range(1, 40) != 1))
			return;

		const int growth_radius = rangelim(
				static_cast<int>(6.0f - 5.0f * humidity / 100.0f), 1, 6);
		if (growth_near(map, ndef, p, growth_radius, true))
			return;

		if (roll <= 10)
			return;
		if (roll <= 100 && !m_flower_contents.empty()) {
			const content_t flower = m_flower_contents[
					myrand_range(0, m_flower_contents.size() - 1)];
			spread_flower(env, map, ndef, top_pos, flower);
		} else {
			map->setNode(top_pos, MapNode(m_nodes.grass_plant));
		}
	}
};

struct GrassWeatherNodes
{
	std::array<content_t, 5> grass;
	std::array<content_t, 5> dry_grass;
	content_t dry_shrub;
	content_t jungle_sapling;
	content_t acacia_sapling;
	content_t pine_sapling;
	content_t aspen_sapling;
	content_t sapling;
};

class GrassWeatherABM : public ConfigurableABM
{
	const GrassWeatherNodes m_nodes;
	const int m_grass_heat_max;
	const int m_grass_humidity_min;
	const int m_grass_light_min;
	const int m_tree_light_min;
	const bool m_debug_fast;

	static int find_stage(
			const std::array<content_t, 5> &contents, content_t content)
	{
		const auto it = std::find(contents.begin(), contents.end(), content);
		return it == contents.end() ? -1 : std::distance(contents.begin(), it);
	}

public:
	GrassWeatherABM(
			const CoreABMDefinition &definition, const GrassWeatherNodes &nodes) :
			ConfigurableABM(definition),
			m_nodes(nodes),
			m_grass_heat_max(get_int(definition, "grass_heat_max", 51)),
			m_grass_humidity_min(get_int(definition, "grass_humidity_min", 4)),
			m_grass_light_min(get_int(definition, "grass_light_min", 6)),
			m_tree_light_min(get_int(definition, "tree_light_min", 12)),
			m_debug_fast(get_bool(definition, "debug_fast", false))
	{
	}

	void trigger(ServerEnvironment *env, v3pos_t p, MapNode n,
			u32 active_object_count, u32 active_object_count_wider, v3pos_t neighbor_pos,
			uint8_t activate) override
	{
		ServerMap *map = &env->getServerMap();
		const auto *ndef = env->getGameDef()->ndef();
		const content_t content = n.getContent();
		const int grass_stage = find_stage(m_nodes.grass, content);
		const int dry_grass_stage = find_stage(m_nodes.dry_grass, content);
		const int humidity = map->updateBlockHumidity(env, p);
		const int heat = map->updateBlockHeat(env, p);

		if ((heat < -5 || heat > m_grass_heat_max || humidity < 3) &&
				grass_stage >= 3) {
			n.setContent(m_nodes.dry_shrub);
			map->setNode(p, n);
			return;
		}

		const int light = get_node_light(env, map, ndef, p, false);
		if (heat < 5 || heat > m_grass_heat_max || light < m_grass_light_min)
			return;

		const int roll_max = std::max(1, 110 - humidity);
		const int roll = (activate != 0 || m_debug_fast) ?
				1 : myrand_range(1, roll_max);
		if (grass_stage == 4) {
			if (activate > 1 || roll >= 3)
				return;

			content_t sapling = CONTENT_IGNORE;
			if (humidity > 70 && heat > 25)
				sapling = m_nodes.jungle_sapling;
			else if (humidity >= 20 && humidity < 35 && heat > 25)
				sapling = m_nodes.acacia_sapling;
			else if (humidity > 20 && heat < 10)
				sapling = m_nodes.pine_sapling;
			else if (humidity > 45 && heat < 25)
				sapling = m_nodes.aspen_sapling;
			else if (humidity > 30 && heat < 40)
				sapling = m_nodes.sapling;
			else
				return;

			const int growth_radius = rangelim(
					static_cast<int>(7.0f - 5.0f * humidity / 100.0f), 1, 7);
			if (light < m_tree_light_min ||
					growth_near(map, ndef, p, growth_radius, false))
				return;
			n.setContent(sapling);
			map->setNode(p, n);
			return;
		}

		if (content == m_nodes.dry_shrub) {
			n.setContent(m_nodes.grass[0]);
			map->setNode(p, n);
			return;
		}
		if (roll >= 6)
			return;

		if (grass_stage >= 0 && grass_stage < 4)
			n.setContent(m_nodes.grass[grass_stage + 1]);
		else if (dry_grass_stage >= 0 && dry_grass_stage < 4)
			n.setContent(m_nodes.grass[dry_grass_stage]);
		else
			return;
		map->setNode(p, n);
	}
};

class SoilHydrateABM : public ConfigurableABM
{
	const content_t m_dirt_content;
	const content_t m_grass_content;
	const content_t m_dry_grass_content;
	const content_t m_dry_dirt_grass_content;
	const int m_heat_max;
	const int m_humidity_min;
	const int m_light_min;

public:
	SoilHydrateABM(const CoreABMDefinition &definition, content_t dirt_content,
			content_t grass_content, content_t dry_grass_content,
			content_t dry_dirt_grass_content) :
			ConfigurableABM(definition),
			m_dirt_content(dirt_content), m_grass_content(grass_content),
			m_dry_grass_content(dry_grass_content),
			m_dry_dirt_grass_content(dry_dirt_grass_content),
			m_heat_max(get_int(definition, "heat_max", 51)),
			m_humidity_min(get_int(definition, "humidity_min", 4)),
			m_light_min(get_int(definition, "light_min", 6))
	{
	}

	void trigger(ServerEnvironment *env, v3pos_t p, MapNode n,
			u32 active_object_count, u32 active_object_count_wider, v3pos_t neighbor_pos,
			uint8_t activate) override
	{
		ServerMap *map = &env->getServerMap();
		if (map->updateBlockHeat(env, p) > m_heat_max ||
				map->updateBlockHumidity(env, p) < m_humidity_min)
			return;

		const content_t content = n.getContent();
		if (content == m_dry_grass_content || content == m_dry_dirt_grass_content) {
			const auto *ndef = env->getGameDef()->ndef();
			if (get_node_light(
						env, map, ndef, p + v3pos_t(0, 1, 0), true) < m_light_min)
				return;
			n.setContent(m_grass_content);
		} else {
			n.setContent(m_dirt_content);
		}
		map->setNode(p, n);
	}
};

}

ActiveBlockModifier *create_growth(const CoreABMDefinition &definition,
		const NodeDefManager *nodedef, std::string *error)
{
	if (definition.action == "soil_weather") {
		if (!validate_params(definition,
					{"grass_heat_max", "grass_heat_extreme", "grass_humidity_min",
							"grass_humidity_dry", "grass_light_min", "dirt_dry_humidity"},
					{"debug_fast"},
					{"dirt_node", "grass_node", "grass_footsteps_node",
							"dry_dirt_node", "dry_grass_node", "dry_dirt_grass_node",
							"snow_dirt_node", "snow_node", "snowblock_node", "ice_node",
							"grass_plant_node"},
					error))
			return nullptr;

		SoilWeatherNodes nodes;
		auto resolve_node = [&](const char *param, const char *default_name,
				content_t &content) {
			const std::string name = get_string(definition, param, default_name);
			content = nodedef->getId(name);
			if (content != CONTENT_IGNORE)
				return true;
			if (error)
				*error = "unknown " + std::string(param) + " '" + name + "'";
			return false;
		};
		if (!resolve_node("dirt_node", "default:dirt", nodes.dirt) ||
				!resolve_node("grass_node", "default:dirt_with_grass", nodes.grass) ||
				!resolve_node("grass_footsteps_node",
						"default:dirt_with_grass_footsteps", nodes.grass_footsteps) ||
				!resolve_node("dry_dirt_node", "default:dry_dirt", nodes.dry_dirt) ||
				!resolve_node("dry_grass_node", "default:dirt_with_dry_grass",
						nodes.dry_grass) ||
				!resolve_node("dry_dirt_grass_node",
						"default:dry_dirt_with_dry_grass", nodes.dry_dirt_grass) ||
				!resolve_node("snow_dirt_node", "default:dirt_with_snow",
						nodes.snow_dirt) ||
				!resolve_node("snow_node", "default:snow", nodes.snow) ||
				!resolve_node("snowblock_node", "default:snowblock", nodes.snowblock) ||
				!resolve_node("ice_node", "default:ice", nodes.ice) ||
				!resolve_node("grass_plant_node", "default:grass_1", nodes.grass_plant))
			return nullptr;

		std::vector<content_t> flower_candidates;
		std::vector<content_t> flower_contents;
		nodedef->getIds("group:flower", flower_candidates);
		for (content_t content : flower_candidates) {
			if (nodedef->get(content).getGroup("flora") > 0)
				flower_contents.emplace_back(content);
		}
		return new SoilWeatherABM(definition, nodes, std::move(flower_contents));
	}

	if (definition.action == "grass_weather") {
		if (!validate_params(definition,
					{"grass_heat_max", "grass_humidity_min", "grass_light_min",
							"tree_light_min"},
					{"debug_fast"},
					{"grass_1_node", "grass_2_node", "grass_3_node", "grass_4_node",
							"grass_5_node", "dry_grass_1_node", "dry_grass_2_node",
							"dry_grass_3_node", "dry_grass_4_node", "dry_grass_5_node",
							"dry_shrub_node", "jungle_sapling_node", "acacia_sapling_node",
							"pine_sapling_node", "aspen_sapling_node", "sapling_node"},
					error))
			return nullptr;

		GrassWeatherNodes nodes;
		auto resolve_node = [&](const std::string &param,
				const std::string &default_name, content_t &content) {
			const std::string name = get_string(definition, param.c_str(), default_name);
			content = nodedef->getId(name);
			if (content != CONTENT_IGNORE)
				return true;
			if (error)
				*error = "unknown " + param + " '" + name + "'";
			return false;
		};
		for (size_t i = 0; i < nodes.grass.size(); ++i) {
			const std::string stage = std::to_string(i + 1);
			if (!resolve_node("grass_" + stage + "_node", "default:grass_" + stage,
						nodes.grass[i]) ||
					!resolve_node("dry_grass_" + stage + "_node",
							"default:dry_grass_" + stage, nodes.dry_grass[i]))
				return nullptr;
		}
		if (!resolve_node("dry_shrub_node", "default:dry_shrub", nodes.dry_shrub) ||
				!resolve_node("jungle_sapling_node", "default:junglesapling",
						nodes.jungle_sapling) ||
				!resolve_node("acacia_sapling_node", "default:acacia_sapling",
						nodes.acacia_sapling) ||
				!resolve_node("pine_sapling_node", "default:pine_sapling",
						nodes.pine_sapling) ||
				!resolve_node("aspen_sapling_node", "default:aspen_sapling",
						nodes.aspen_sapling) ||
				!resolve_node("sapling_node", "default:sapling", nodes.sapling))
			return nullptr;
		return new GrassWeatherABM(definition, nodes);
	}

	if (definition.action == "soil_hydrate") {
		if (!validate_params(definition,
					{"heat_max", "humidity_min", "light_min"}, {},
					{"dirt_node", "grass_node", "dry_grass_node",
							"dry_dirt_grass_node"},
					error))
			return nullptr;

		auto resolve_node = [&](const char *param, const char *default_name,
				content_t &content) {
			const std::string name = get_string(definition, param, default_name);
			content = nodedef->getId(name);
			if (content != CONTENT_IGNORE)
				return true;
			if (error)
				*error = "unknown " + std::string(param) + " '" + name + "'";
			return false;
		};
		content_t dirt_content;
		content_t grass_content;
		content_t dry_grass_content;
		content_t dry_dirt_grass_content;
		if (!resolve_node("dirt_node", "default:dirt", dirt_content) ||
				!resolve_node("grass_node", "default:dirt_with_grass", grass_content) ||
				!resolve_node("dry_grass_node", "default:dirt_with_dry_grass",
						dry_grass_content) ||
				!resolve_node("dry_dirt_grass_node",
						"default:dry_dirt_with_dry_grass", dry_dirt_grass_content))
			return nullptr;
		return new SoilHydrateABM(definition, dirt_content, grass_content,
				dry_grass_content, dry_dirt_grass_content);
	}

	if (error)
		*error = "unsupported growth action '" + definition.action + "'";
	return nullptr;
}

}
