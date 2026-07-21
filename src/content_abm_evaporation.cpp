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

#include "light.h"
#include "server.h"
#include "serverenvironment.h"
#include "servermap.h"
#include "util/numeric.h"

#include <algorithm>
#include <cmath>

namespace core_abm
{
namespace
{

void queue_liquid_update(ServerMap *map, const v3pos_t &p)
{
	map->transforming_liquid_add(p);
}

class WaterEvaporateABM : public ConfigurableABM
{
	const int m_humidity_stop;
	const int m_humid_air_stop;
	const int m_humid_heat_min;
	const int m_humid_heat_max;
	const int m_heat_min;
	const float m_warm_scale;
	const float m_warm_max;
	const float m_shade_factor;
	const float m_rate;
	const int m_max_level_loss;

public:
	explicit WaterEvaporateABM(const CoreABMDefinition &definition) :
			ConfigurableABM(definition),
			m_humidity_stop(rangelim(get_int(definition, "humidity_stop", 96), 1, 100)),
			m_humid_air_stop(rangelim(get_int(definition, "humid_air_stop", 75), 0, 100)),
			m_humid_heat_min(get_int(definition, "humid_heat_min", -2)),
			m_humid_heat_max(get_int(definition, "humid_heat_max", 50)),
			m_heat_min(get_int(definition, "heat_min", -5)),
			m_warm_scale(std::max(0.01f,
					static_cast<float>(get_number(definition, "warm_scale", 45.0)))),
			m_warm_max(std::max(0.0f,
					static_cast<float>(get_number(definition, "warm_max", 1.8)))),
			m_shade_factor(rangelim(static_cast<float>(
					get_number(definition, "shade_factor", 0.35)), 0.0f, 1.0f)),
			m_rate(std::max(0.0f,
					static_cast<float>(get_number(definition, "rate", 6.0)))),
			m_max_level_loss(std::max(0, get_int(definition, "max_level_loss", 8)))
	{
	}

	void trigger(ServerEnvironment *env, v3pos_t p, MapNode n,
			u32 active_object_count, u32 active_object_count_wider, v3pos_t neighbor_pos,
			uint8_t activate) override
	{
		ServerMap *map = &env->getServerMap();
		const auto *ndef = env->getGameDef()->ndef();
		const v3pos_t top_pos = p + v3pos_t(0, 1, 0);
		const MapNode top_node = map->getNodeTry(top_pos);
		if (top_node.getContent() != CONTENT_AIR)
			return;

		int humidity = map->updateBlockHumidity(env, p);
		if (humidity >= m_humidity_stop)
			return;
		humidity = rangelim(humidity, 0, 100);

		const int heat = map->updateBlockHeat(env, p);
		if (heat < m_heat_min)
			return;
		if (humidity >= m_humid_air_stop && heat >= m_humid_heat_min &&
				heat <= m_humid_heat_max)
			return;

		const auto light =
				top_node.getLight(LIGHTBANK_DAY, ndef->getLightingFlags(top_node));
		const float sun = light >= LIGHT_SUN ? 1.0f :
				static_cast<float>(light) / static_cast<float>(LIGHT_SUN);
		const float dry = std::max(0.0f,
				static_cast<float>(m_humidity_stop - humidity) / m_humidity_stop);
		const float warm = rangelim(
				static_cast<float>(heat - m_heat_min) / m_warm_scale, 0.0f, m_warm_max);
		const float amount = dry * warm *
				(m_shade_factor + (1.0f - m_shade_factor) * sun) * m_rate;
		int whole = static_cast<int>(std::floor(amount));
		const float frac = amount - whole;
		if (myrand_range(0.0f, 1.0f) < frac)
			++whole;

		const int evaporate = rangelim(whole, 0, m_max_level_loss);
		if (evaporate <= 0)
			return;

		n.addLevel(ndef, -evaporate);
		map->setNode(p, n);
		queue_liquid_update(map, p);
	}
};

class SteamEvaporateABM : public ConfigurableABM
{
	const int m_humidity_reference;
	const int m_min_evaporation_chance;
	const int m_level_step;
	const bool m_evaporate_on_activate;

public:
	explicit SteamEvaporateABM(const CoreABMDefinition &definition) :
			ConfigurableABM(definition),
			m_humidity_reference(rangelim(
					get_int(definition, "humidity_reference", 100), 1, 100)),
			m_min_evaporation_chance(rangelim(
					get_int(definition, "min_evaporation_chance", 1), 0, 100)),
			m_level_step(std::max(1, get_int(definition, "level_step", 1))),
			m_evaporate_on_activate(
					get_bool(definition, "evaporate_on_activate", true))
	{
	}

	void trigger(ServerEnvironment *env, v3pos_t p, MapNode n,
			u32 active_object_count, u32 active_object_count_wider, v3pos_t neighbor_pos,
			uint8_t activate) override
	{
		ServerMap *map = &env->getServerMap();
		const auto *ndef = env->getGameDef()->ndef();

		int humidity = map->updateBlockHumidity(env, p);
		humidity = rangelim(humidity, 0, 100);

		const int evaporation_chance = std::max(
				m_min_evaporation_chance, m_humidity_reference - humidity);
		if (activate && !m_evaporate_on_activate)
			return;
		if (!activate && myrand_range(1, 100) > evaporation_chance)
			return;

		const u8 level = n.getLevel(ndef);
		if (level <= m_level_step)
			n = MapNode(CONTENT_AIR);
		else
			n.setLevel(ndef, level - m_level_step);
		map->setNode(p, n);
		queue_liquid_update(map, p);
	}
};

}

ActiveBlockModifier *create_evaporation(const CoreABMDefinition &definition,
		const NodeDefManager *nodedef, std::string *error)
{
	if (definition.action == "water_evaporate") {
		if (!validate_params(definition,
					{"humidity_stop", "humid_air_stop", "humid_heat_min",
							"humid_heat_max", "heat_min", "warm_scale", "warm_max",
							"shade_factor", "rate", "max_level_loss"},
					{}, {}, error))
			return nullptr;
		return new WaterEvaporateABM(definition);
	}
	if (definition.action == "steam_evaporate") {
		if (!validate_params(definition,
					{"humidity_reference", "min_evaporation_chance", "level_step"},
					{"evaporate_on_activate"}, {}, error))
			return nullptr;
		return new SteamEvaporateABM(definition);
	}

	if (error)
		*error = "unsupported evaporation action '" + definition.action + "'";
	return nullptr;
}

}
