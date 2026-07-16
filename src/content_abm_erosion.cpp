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

#include "content_abm_precipitation.h"

#include "light.h"
#include "server.h"
#include "serverenvironment.h"
#include "servermap.h"
#include "util/numeric.h"

#include <algorithm>
#include <cmath>
#include <unordered_set>
#include <utility>

namespace core_abm
{
namespace
{

struct ErosionRule
{
	content_t target;
	float resistance;
	float humidity_min;
};

class ErosionABM : public PrecipitationABM
{
	const std::unordered_set<content_t> m_water_contents;
	const std::unordered_map<content_t, ErosionRule> m_erosion_rules;
	const std::unordered_map<content_t, content_t> m_deposit_rules;
	const float m_rain_heat_min;
	const float m_rain_heat_max;
	const float m_rain_phase_max;
	const float m_rain_humidity;
	const int m_sky_tolerance;
	const float m_water_above_weight;
	const float m_water_side_weight;
	const float m_water_below_weight;
	const float m_water_level_divisor;
	const float m_water_level_max_reduction;
	const float m_water_strength;
	const float m_rain_strength;
	const float m_wet_min;
	const float m_humidity_scale;
	const float m_cold_offset;
	const float m_cold_scale;
	const float m_cold_min;
	const float m_unsupported_slope;
	const float m_air_side_slope;
	const float m_deposit_strength_max;
	const float m_deposit_humidity_min;
	const float m_deposit_chance_max;
	const float m_deposit_chance_divisor;
	const float m_erosion_chance_max;
	const float m_erosion_chance_scale;

	float rain_amount(ServerEnvironment *env, ServerMap *map,
			const NodeDefManager *ndef, const v3pos_t &p, float heat, float humidity)
	{
		if (heat < m_rain_heat_min || heat > m_rain_heat_max ||
				humidity < m_rain_humidity)
			return 0.0f;
		const float phase = heat >= m_rain_phase_max ? 1.0f :
				rangelim((heat - m_rain_heat_min) /
						(m_rain_phase_max - m_rain_heat_min), 0.0f, 1.0f);
		return ((humidity - m_rain_humidity) / (100.0f - m_rain_humidity)) *
				phase * precipitation_factor(env, map, ndef, p, humidity);
	}

	float water_energy(ServerMap *map, const NodeDefManager *ndef,
			const v3pos_t &p) const
	{
		const auto contribution = [&](const v3pos_t &offset, float weight) {
			const MapNode node = map->getNodeTry(p + offset);
			if (!m_water_contents.contains(node.getContent()))
				return 0.0f;
			const float level = node.getLevel(ndef);
			const float moving = 1.0f - std::min(
					m_water_level_max_reduction, level / m_water_level_divisor);
			return weight * moving;
		};

		return contribution(v3pos_t(0, 1, 0), m_water_above_weight) +
				contribution(v3pos_t(1, 0, 0), m_water_side_weight) +
				contribution(v3pos_t(-1, 0, 0), m_water_side_weight) +
				contribution(v3pos_t(0, 0, 1), m_water_side_weight) +
				contribution(v3pos_t(0, 0, -1), m_water_side_weight) +
				contribution(v3pos_t(0, -1, 0), m_water_below_weight);
	}

	float slope_energy(ServerMap *map, const v3pos_t &p) const
	{
		if (map->getNodeTry(p - v3pos_t(0, 1, 0)).getContent() == CONTENT_AIR)
			return m_unsupported_slope;

		int air_sides = 0;
		for (const v3pos_t &offset : {v3pos_t(1, 0, 0), v3pos_t(-1, 0, 0),
				v3pos_t(0, 0, 1), v3pos_t(0, 0, -1)}) {
			if (map->getNodeTry(p + offset).getContent() == CONTENT_AIR)
				++air_sides;
		}
		return air_sides * m_air_side_slope;
	}

public:
	ErosionABM(const CoreABMDefinition &definition,
			std::unordered_set<content_t> water_contents,
			std::unordered_map<content_t, ErosionRule> erosion_rules,
			std::unordered_map<content_t, content_t> deposit_rules) :
			PrecipitationABM(definition),
			m_water_contents(std::move(water_contents)),
			m_erosion_rules(std::move(erosion_rules)),
			m_deposit_rules(std::move(deposit_rules)),
			m_rain_heat_min(static_cast<float>(
					get_number(definition, "rain_heat_min", -2.0))),
			m_rain_heat_max(std::max(m_rain_heat_min, static_cast<float>(
					get_number(definition, "rain_heat_max", 50.0)))),
			m_rain_phase_max(std::max(m_rain_heat_min + 0.01f, static_cast<float>(
					get_number(definition, "rain_phase_max", 2.0)))),
			m_rain_humidity(rangelim(static_cast<float>(
					get_number(definition, "rain_humidity", 75.0)), 0.0f, 99.9f)),
			m_sky_tolerance(rangelim(
					get_int(definition, "sky_tolerance", 1), 0, LIGHT_SUN)),
			m_water_above_weight(std::max(0.0f, static_cast<float>(
					get_number(definition, "water_above_weight", 2.2)))),
			m_water_side_weight(std::max(0.0f, static_cast<float>(
					get_number(definition, "water_side_weight", 1.0)))),
			m_water_below_weight(std::max(0.0f, static_cast<float>(
					get_number(definition, "water_below_weight", 0.35)))),
			m_water_level_divisor(std::max(0.01f, static_cast<float>(
					get_number(definition, "water_level_divisor", 16.0)))),
			m_water_level_max_reduction(rangelim(static_cast<float>(
					get_number(definition, "water_level_max_reduction", 0.75)),
					0.0f, 1.0f)),
			m_water_strength(std::max(0.0f, static_cast<float>(
					get_number(definition, "water_strength", 1.25)))),
			m_rain_strength(std::max(0.0f, static_cast<float>(
					get_number(definition, "rain_strength", 0.9)))),
			m_wet_min(std::max(0.0f, static_cast<float>(
					get_number(definition, "wet_min", 0.15)))),
			m_humidity_scale(std::max(0.01f, static_cast<float>(
					get_number(definition, "humidity_scale", 85.0)))),
			m_cold_offset(static_cast<float>(get_number(definition, "cold_offset", 20.0))),
			m_cold_scale(std::max(0.01f, static_cast<float>(
					get_number(definition, "cold_scale", 20.0)))),
			m_cold_min(std::max(0.0f, static_cast<float>(
					get_number(definition, "cold_min", 0.05)))),
			m_unsupported_slope(std::max(0.0f, static_cast<float>(
					get_number(definition, "unsupported_slope", 0.6)))),
			m_air_side_slope(std::max(0.0f, static_cast<float>(
					get_number(definition, "air_side_slope", 0.12)))),
			m_deposit_strength_max(std::max(0.0f, static_cast<float>(
					get_number(definition, "deposit_strength_max", 0.55)))),
			m_deposit_humidity_min(static_cast<float>(
					get_number(definition, "deposit_humidity_min", 45.0))),
			m_deposit_chance_max(rangelim(static_cast<float>(
					get_number(definition, "deposit_chance_max", 0.18)), 0.0f, 1.0f)),
			m_deposit_chance_divisor(std::max(0.01f, static_cast<float>(
					get_number(definition, "deposit_chance_divisor", 180.0)))),
			m_erosion_chance_max(rangelim(static_cast<float>(
					get_number(definition, "erosion_chance_max", 0.3)), 0.0f, 1.0f)),
			m_erosion_chance_scale(std::max(0.0f, static_cast<float>(
					get_number(definition, "erosion_chance_scale", 0.07))))
	{
	}

	void trigger(ServerEnvironment *env, v3pos_t p, MapNode n,
			u32 active_object_count, u32 active_object_count_wider, v3pos_t neighbor_pos,
			uint8_t activate) override
	{
		ServerMap *map = &env->getServerMap();
		const auto *ndef = env->getGameDef()->ndef();
		const auto rule = m_erosion_rules.find(n.getContent());
		const float humidity = map->updateBlockHumidity(env, p);
		if (rule != m_erosion_rules.end() && rule->second.humidity_min > 0.0f &&
				humidity < rule->second.humidity_min)
			return;

		const float heat = map->updateBlockHeat(env, p);
		const float water = water_energy(map, ndef, p);
		float rain = 0.0f;
		const v3pos_t top_pos = p + v3pos_t(0, 1, 0);
		if (map->getNodeTry(top_pos).getContent() == CONTENT_AIR &&
				get_node_light(env, map, ndef, top_pos, true) >=
						LIGHT_SUN - m_sky_tolerance)
			rain = rain_amount(env, map, ndef, p, heat, humidity);
		if (water <= 0.0f && rain <= 0.0f)
			return;

		const float wet = std::max(m_wet_min, humidity / m_humidity_scale);
		const float warm = heat > 0.0f ? 1.0f :
				std::max(m_cold_min, (heat + m_cold_offset) / m_cold_scale);
		const float strength = (water * m_water_strength + rain * m_rain_strength) *
				wet * warm * (1.0f + slope_energy(map, p));

		const auto deposit = m_deposit_rules.find(n.getContent());
		if (deposit != m_deposit_rules.end() && strength <= m_deposit_strength_max &&
				humidity >= m_deposit_humidity_min &&
				map->getNodeTry(top_pos).getContent() == CONTENT_AIR) {
			const float chance = std::min(m_deposit_chance_max,
					(m_deposit_strength_max - strength) * humidity /
							m_deposit_chance_divisor);
			if (myrand_range(0.0f, 1.0f) < chance) {
				env->setNode(p, MapNode(deposit->second));
				return;
			}
		}

		if (rule == m_erosion_rules.end())
			return;
		const float chance = std::min(m_erosion_chance_max,
				(strength / rule->second.resistance) * m_erosion_chance_scale);
		if (myrand_range(0.0f, 1.0f) >= chance)
			return;

		env->setNode(p, MapNode(rule->second.target));
		env->nodeUpdate(p, 5, 1);
	}
};

}

ActiveBlockModifier *create_erosion(const CoreABMDefinition &definition,
		const NodeDefManager *nodedef, std::string *error)
{
	if (!validate_params_with_lists(definition,
				{"cloud_height", "rain_heat_min", "rain_heat_max", "rain_phase_max",
						"rain_humidity", "sky_tolerance", "water_above_weight",
						"water_side_weight", "water_below_weight", "water_level_divisor",
						"water_level_max_reduction", "water_strength", "rain_strength",
						"wet_min", "humidity_scale", "cold_offset", "cold_scale",
						"cold_min", "unsupported_slope", "air_side_slope",
						"deposit_strength_max", "deposit_humidity_min",
						"deposit_chance_max", "deposit_chance_divisor",
						"erosion_chance_max", "erosion_chance_scale"},
				{}, {}, {"erosion_resistances", "erosion_humidity_min"},
				{"water_nodes", "erosion_nodes", "erosion_targets",
						"deposit_nodes", "deposit_targets"},
				error))
		return nullptr;

	const auto *water_names = get_string_list(definition, "water_nodes");
	const auto *erosion_names = get_string_list(definition, "erosion_nodes");
	const auto *erosion_targets = get_string_list(definition, "erosion_targets");
	const auto *erosion_resistances = get_number_list(definition, "erosion_resistances");
	const auto *erosion_humidity = get_number_list(definition, "erosion_humidity_min");
	const auto *deposit_names = get_string_list(definition, "deposit_nodes");
	const auto *deposit_targets = get_string_list(definition, "deposit_targets");
	if (!water_names || !erosion_names || !erosion_targets ||
			!erosion_resistances || !erosion_humidity || !deposit_names ||
			!deposit_targets) {
		if (error)
			*error = "erosion action requires water, erosion, and deposit lists";
		return nullptr;
	}
	if (erosion_names->size() != erosion_targets->size() ||
			erosion_names->size() != erosion_resistances->size() ||
			erosion_names->size() != erosion_humidity->size()) {
		if (error)
			*error = "erosion_nodes, erosion_targets, erosion_resistances, and "
					"erosion_humidity_min must have equal lengths";
		return nullptr;
	}
	if (deposit_names->size() != deposit_targets->size()) {
		if (error)
			*error = "deposit_nodes and deposit_targets must have equal lengths";
		return nullptr;
	}

	auto resolve = [&](const std::string &name, const char *list) {
		const content_t content = nodedef->getId(name);
		if (content == CONTENT_IGNORE && error)
			*error = "unknown node '" + name + "' in " + list;
		return content;
	};
	std::unordered_set<content_t> water_contents;
	for (const std::string &name : *water_names) {
		const content_t content = resolve(name, "water_nodes");
		if (content == CONTENT_IGNORE)
			return nullptr;
		water_contents.emplace(content);
	}

	std::unordered_map<content_t, ErosionRule> erosion_rules;
	for (size_t i = 0; i < erosion_names->size(); ++i) {
		const content_t content = resolve((*erosion_names)[i], "erosion_nodes");
		const content_t target = resolve((*erosion_targets)[i], "erosion_targets");
		if (content == CONTENT_IGNORE || target == CONTENT_IGNORE)
			return nullptr;
		if ((*erosion_resistances)[i] <= 0.0) {
			if (error)
				*error = "erosion resistance must be positive for '" +
						(*erosion_names)[i] + "'";
			return nullptr;
		}
		if (!erosion_rules.emplace(content, ErosionRule{target,
					static_cast<float>((*erosion_resistances)[i]),
					static_cast<float>((*erosion_humidity)[i])}).second) {
			if (error)
				*error = "duplicate erosion node '" + (*erosion_names)[i] + "'";
			return nullptr;
		}
	}

	std::unordered_map<content_t, content_t> deposit_rules;
	for (size_t i = 0; i < deposit_names->size(); ++i) {
		const content_t content = resolve((*deposit_names)[i], "deposit_nodes");
		const content_t target = resolve((*deposit_targets)[i], "deposit_targets");
		if (content == CONTENT_IGNORE || target == CONTENT_IGNORE)
			return nullptr;
		if (!deposit_rules.emplace(content, target).second) {
			if (error)
				*error = "duplicate deposit node '" + (*deposit_names)[i] + "'";
			return nullptr;
		}
	}

	return new ErosionABM(definition, std::move(water_contents),
			std::move(erosion_rules), std::move(deposit_rules));
}

}
