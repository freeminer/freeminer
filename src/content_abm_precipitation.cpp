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
#include "noise.h"
#include "server.h"
#include "serverenvironment.h"
#include "servermap.h"
#include "util/numeric.h"

#include <algorithm>
#include <array>
#include <cmath>

namespace core_abm
{

size_t TerrainKeyHash::operator()(const TerrainKey &key) const
{
	size_t hash = std::hash<int>{}(key.x);
	hash ^= std::hash<int>{}(key.z) + 0x9e3779b9 + (hash << 6) + (hash >> 2);
	hash ^= std::hash<int>{}(key.min_y) + 0x9e3779b9 + (hash << 6) + (hash >> 2);
	hash ^= std::hash<int>{}(key.max_y) + 0x9e3779b9 + (hash << 6) + (hash >> 2);
	return hash;
}

PrecipitationABM::PrecipitationABM(const CoreABMDefinition &definition) :
		ConfigurableABM(definition),
		m_cloud_height(get_int(definition, "cloud_height", 120))
{
}

weather::wind_t PrecipitationABM::get_wind(
		ServerEnvironment *env, ServerMap *map, const v3pos_t &p) const
{
	return map->updateBlockWind(env, p);
}

s32 PrecipitationABM::noise_seed(u64 map_seed, u32 seed_difference)
{
	return static_cast<s32>(static_cast<u32>(map_seed) + seed_difference);
}

float PrecipitationABM::cloud_value(
		ServerEnvironment *env, ServerMap *map, const v3pos_t &p) const
{
	const v3pos_t cloud_pos(p.X,
			rangelim(m_cloud_height, -MAX_MAP_GENERATION_LIMIT,
					MAX_MAP_GENERATION_LIMIT), p.Z);
	auto wind = get_wind(env, map, cloud_pos);
	if (std::abs(wind.X) + std::abs(wind.Z) < 0.15f) {
		wind.X = 0.45f;
		wind.Z = 0.18f;
	}

	const float time = static_cast<float>(env->getGameTime());
	const float x = p.X - wind.X * time * 0.35f;
	const float z = p.Z - wind.Z * time * 0.35f;
	const u64 seed = map->getSeed();
	const NoiseParams base_params(0.5f, 0.5f, v3f(260.0f, 260.0f, 260.0f),
			noise_seed(seed, 7283), 3, 0.55f, 2.0f);
	const NoiseParams detail_params(0.5f, 0.5f, v3f(80.0f, 80.0f, 80.0f),
			noise_seed(seed, 19351), 2, 0.45f, 2.0f);
	const float base = NoiseFractal2D(&base_params, x, z, 0);
	const float detail = NoiseFractal2D(&detail_params, x, z, 0);
	return rangelim(base * 0.75f + detail * 0.25f, 0.0f, 1.0f);
}

std::optional<pos_t> PrecipitationABM::highest_solid_y(ServerEnvironment *env,
		ServerMap *map, const NodeDefManager *ndef, float x, float z,
		int min_y, int max_y)
{
	min_y = rangelim(min_y, -MAX_MAP_GENERATION_LIMIT, MAX_MAP_GENERATION_LIMIT);
	max_y = rangelim(max_y, -MAX_MAP_GENERATION_LIMIT, MAX_MAP_GENERATION_LIMIT);
	if (min_y > max_y)
		return std::nullopt;

	const TerrainKey key{
			static_cast<int>(std::floor(x / 8.0f)),
			static_cast<int>(std::floor(z / 8.0f)),
			static_cast<int>(std::floor(min_y / 16.0f)),
			static_cast<int>(std::floor(max_y / 16.0f)),
	};
	const u32 period = env->getGameTime() / 30;
	{
		std::lock_guard<std::mutex> lock(m_terrain_cache_mutex);
		if (m_terrain_cache_period != period) {
			m_terrain_cache_period = period;
			m_terrain_cache.clear();
		}
		if (const auto it = m_terrain_cache.find(key); it != m_terrain_cache.end())
			return it->second;
	}

	std::optional<pos_t> result;
	const int sample_x = rangelim(static_cast<int>(std::floor(x)),
			-MAX_MAP_GENERATION_LIMIT, MAX_MAP_GENERATION_LIMIT);
	const int sample_z = rangelim(static_cast<int>(std::floor(z)),
			-MAX_MAP_GENERATION_LIMIT, MAX_MAP_GENERATION_LIMIT);
	for (int y = max_y; y >= min_y; y -= 4) {
		const MapNode node = map->getNodeTry(v3pos_t(sample_x, y, sample_z));
		if (node.getContent() == CONTENT_AIR || node.getContent() == CONTENT_IGNORE)
			continue;
		const auto &features = ndef->get(node);
		if (features.walkable && features.liquid_type == LIQUID_NONE) {
			result = y;
			break;
		}
	}

	{
		std::lock_guard<std::mutex> lock(m_terrain_cache_mutex);
		if (m_terrain_cache_period == period)
			m_terrain_cache.emplace(key, result);
	}
	return result;
}

float PrecipitationABM::rain_shadow(ServerEnvironment *env, ServerMap *map,
		const NodeDefManager *ndef, const v3pos_t &p)
{
	const v3pos_t cloud_pos(p.X,
			rangelim(m_cloud_height, -MAX_MAP_GENERATION_LIMIT,
					MAX_MAP_GENERATION_LIMIT), p.Z);
	const auto wind = get_wind(env, map, cloud_pos);
	const float speed = std::sqrt(wind.X * wind.X + wind.Z * wind.Z);
	if (speed < 0.2f)
		return 1.0f;

	const float wx = wind.X / speed;
	const float wz = wind.Z / speed;
	const int max_y = std::max<int>(p.Y + 24, m_cloud_height - 4);
	const int min_y = p.Y + 4;
	float shadow = 0.0f;
	float lift = 0.0f;
	constexpr int distances[] = {24, 48, 80, 120};
	for (size_t i = 0; i < std::size(distances); ++i) {
		const float weight = 1.0f / static_cast<float>(i + 1);
		const auto upwind_y = highest_solid_y(env, map, ndef,
				p.X - wx * distances[i], p.Z - wz * distances[i], min_y, max_y);
		if (upwind_y && *upwind_y > p.Y + 8)
			shadow += ((*upwind_y - p.Y - 8) / 80.0f) * weight;

		const auto downwind_y = highest_solid_y(env, map, ndef,
				p.X + wx * distances[i], p.Z + wz * distances[i], min_y, max_y);
		if (downwind_y && *downwind_y > p.Y + 8)
			lift += ((*downwind_y - p.Y - 8) / 110.0f) * weight;
	}

	return rangelim((1.0f + std::min(lift, 0.35f)) *
				(1.0f - std::min(shadow, 0.8f)), 0.2f, 1.35f);
}

float PrecipitationABM::precipitation_factor(ServerEnvironment *env, ServerMap *map,
		const NodeDefManager *ndef, const v3pos_t &p, float humidity)
{
	const float moisture = rangelim((humidity - 25.0f) / 65.0f, 0.05f, 1.25f);
	const float cover = rangelim(
			cloud_value(env, map, p) * (0.45f + moisture), 0.0f, 1.0f);
	return rangelim((cover - 0.38f) / 0.62f, 0.0f, 1.0f) *
			rain_shadow(env, map, ndef, p);
}

namespace
{

void queue_liquid_update(ServerMap *map, const v3pos_t &p)
{
	map->transforming_liquid_add(p);
}

class RainFillABM : public PrecipitationABM
{
	const content_t m_water_content;
	const float m_heat_min;
	const float m_heat_max;
	const float m_phase_heat_max;
	const float m_rain_humidity;
	const float m_wet_humidity;
	const float m_wet_span;
	const float m_rate;
	const int m_max_amount;
	const int m_existing_water_multiplier;
	const float m_wind_scale;
	const int m_wind_limit;
	const float m_wind_chance_scale;

	int rain_amount(ServerEnvironment *env, ServerMap *map,
			const NodeDefManager *ndef, const v3pos_t &p)
	{
		const float heat = map->updateBlockHeat(env, p);
		if (heat <= m_heat_min || heat > m_heat_max)
			return 0;
		const float humidity = rangelim(
				static_cast<float>(map->updateBlockHumidity(env, p)), 0.0f, 100.0f);
		if (humidity < m_rain_humidity)
			return 0;

		const float phase = heat >= m_phase_heat_max ? 1.0f :
				(heat - m_heat_min) / (m_phase_heat_max - m_heat_min);
		const float rain = ((humidity - m_rain_humidity) /
				(100.0f - m_rain_humidity)) * phase *
				precipitation_factor(env, map, ndef, p, humidity);
		const float wet_air = std::max(0.0f,
				(humidity - m_wet_humidity) / m_wet_span);
		const float amount = rain * (1.0f + wet_air) * m_rate;
		int whole = static_cast<int>(std::floor(amount));
		if (myrand_range(0.0f, 1.0f) < amount - whole)
			++whole;
		return rangelim(whole, 0, m_max_amount);
	}

public:
	RainFillABM(const CoreABMDefinition &definition, content_t water_content) :
			PrecipitationABM(definition),
			m_water_content(water_content),
			m_heat_min(static_cast<float>(get_number(definition, "heat_min", -2.0))),
			m_heat_max(static_cast<float>(get_number(definition, "heat_max", 50.0))),
			m_phase_heat_max(std::max(m_heat_min + 0.01f, static_cast<float>(
					get_number(definition, "phase_heat_max", 2.0)))),
			m_rain_humidity(rangelim(static_cast<float>(
					get_number(definition, "rain_humidity", 75.0)), 0.0f, 99.9f)),
			m_wet_humidity(static_cast<float>(
					get_number(definition, "wet_humidity", 55.0))),
			m_wet_span(std::max(0.01f, static_cast<float>(
					get_number(definition, "wet_span", 45.0)))),
			m_rate(std::max(0.0f, static_cast<float>(
					get_number(definition, "rate", 2.0)))),
			m_max_amount(std::max(0, get_int(definition, "max_amount", 5))),
			m_existing_water_multiplier(std::max(
					1, get_int(definition, "existing_water_multiplier", 4))),
			m_wind_scale(std::max(0.0f, static_cast<float>(
					get_number(definition, "wind_scale", 0.35)))),
			m_wind_limit(std::max(0, get_int(definition, "wind_limit", 1))),
			m_wind_chance_scale(std::max(0.0f, static_cast<float>(
					get_number(definition, "wind_chance_scale", 0.25))))
	{
	}

	void trigger(ServerEnvironment *env, v3pos_t p, MapNode n,
			u32 active_object_count, u32 active_object_count_wider, v3pos_t neighbor_pos,
			uint8_t activate) override
	{
		ServerMap *map = &env->getServerMap();
		const auto *ndef = env->getGameDef()->ndef();
		v3pos_t top_pos = p + v3pos_t(0, 1, 0);
		const MapNode top_node = map->getNodeTry(top_pos);
		if (top_node.getContent() != CONTENT_AIR ||
				top_node.getLight(LIGHTBANK_DAY, ndef->getLightingFlags(top_node)) <
						LIGHT_SUN)
			return;

		const int amount = rain_amount(env, map, ndef, p);
		if (amount <= 0)
			return;

		const auto wind = get_wind(env, map, top_pos);
		const float speed = std::sqrt(wind.X * wind.X + wind.Z * wind.Z);
		const float wind_chance = rangelim(
				speed * m_wind_chance_scale, 0.0f, 0.85f);
		if (speed >= 0.25f && myrand_range(0.0f, 1.0f) < wind_chance) {
			const int dx = rangelim(static_cast<int>(std::lround(wind.X * m_wind_scale)),
					-m_wind_limit, m_wind_limit);
			const int dz = rangelim(static_cast<int>(std::lround(wind.Z * m_wind_scale)),
					-m_wind_limit, m_wind_limit);
			if (dx != 0 || dz != 0) {
				const v3pos_t wind_top = top_pos + v3pos_t(dx, 0, dz);
				if (map->getNodeTry(wind_top).getContent() == CONTENT_AIR) {
					const v3pos_t wind_base = wind_top - v3pos_t(0, 1, 0);
					const MapNode wind_node = map->getNodeTry(wind_base);
					if (wind_node.getContent() == CONTENT_AIR ||
							ndef->get(wind_node).getGroup("rain_collect") > 0) {
						p = wind_base;
						top_pos = wind_top;
					}
				}
			}
		}

		MapNode target_node = map->getNodeTry(p);
		if (target_node.getContent() == m_water_content) {
			target_node.addLevel(ndef, m_existing_water_multiplier * amount);
			map->setNode(p, target_node);
			queue_liquid_update(map, p);
		} else if (map->getNodeTry(top_pos).getContent() == CONTENT_AIR) {
			MapNode water(m_water_content);
			water.setLevel(ndef, amount);
			map->setNode(top_pos, water);
			queue_liquid_update(map, top_pos);
		}
	}
};

class SnowFillABM : public PrecipitationABM
{
	const content_t m_snow_content;
	const content_t m_ice_content;
	const float m_heat_max;
	const float m_phase_heat_min;
	const float m_snow_humidity;
	const float m_wet_heat_min;
	const float m_wet_heat_span;
	const float m_humidity_reference;
	const float m_rate;
	const int m_max_amount;
	const int m_sky_tolerance;
	const float m_wind_scale;
	const int m_wind_limit;
	const float m_wind_chance_scale;
	const bool m_time_moves;

	static bool supports_snow(const ContentFeatures &features)
	{
		return features.drawtype == NDT_NORMAL || features.drawtype == NDT_NODEBOX ||
				features.drawtype == NDT_ALLFACES_OPTIONAL ||
				features.drawtype == NDT_GLASSLIKE;
	}

	int snow_amount(ServerEnvironment *env, ServerMap *map,
			const NodeDefManager *ndef, const v3pos_t &p, float heat, float humidity)
	{
		if (heat > m_heat_max || humidity < m_snow_humidity)
			return 0;
		const float phase = heat <= m_phase_heat_min ? 1.0f :
				rangelim((m_heat_max - heat) /
						(m_heat_max - m_phase_heat_min), 0.0f, 1.0f);
		const float snow = ((humidity - m_snow_humidity) /
				(100.0f - m_snow_humidity)) * phase *
				precipitation_factor(env, map, ndef, p, humidity);
		const float wet = rangelim(
				(heat - m_wet_heat_min) / m_wet_heat_span, 0.0f, 1.0f);
		const float humid = rangelim(
				humidity / m_humidity_reference, 0.5f, 1.5f);
		const float amount = snow * (1.0f + wet) * humid * m_rate;
		int whole = static_cast<int>(std::floor(amount));
		if (myrand_range(0.0f, 1.0f) < amount - whole)
			++whole;
		return rangelim(whole, 0, m_max_amount);
	}

public:
	SnowFillABM(const CoreABMDefinition &definition, content_t snow_content,
			content_t ice_content) :
			PrecipitationABM(definition),
			m_snow_content(snow_content), m_ice_content(ice_content),
			m_heat_max(static_cast<float>(get_number(definition, "heat_max", 2.0))),
			m_phase_heat_min(std::min(m_heat_max - 0.01f, static_cast<float>(
					get_number(definition, "phase_heat_min", -2.0)))),
			m_snow_humidity(rangelim(static_cast<float>(
					get_number(definition, "snow_humidity", 65.0)), 0.0f, 99.9f)),
			m_wet_heat_min(static_cast<float>(
					get_number(definition, "wet_heat_min", -12.0))),
			m_wet_heat_span(std::max(0.01f, static_cast<float>(
					get_number(definition, "wet_heat_span", 13.0)))),
			m_humidity_reference(std::max(0.01f, static_cast<float>(
					get_number(definition, "humidity_reference", 70.0)))),
			m_rate(std::max(0.0f, static_cast<float>(
					get_number(definition, "rate", 2.0)))),
			m_max_amount(std::max(0, get_int(definition, "max_amount", 6))),
			m_sky_tolerance(rangelim(
					get_int(definition, "sky_tolerance", 1), 0, LIGHT_SUN)),
			m_wind_scale(std::max(0.0f, static_cast<float>(
					get_number(definition, "wind_scale", 0.55)))),
			m_wind_limit(std::max(0, get_int(definition, "wind_limit", 2))),
			m_wind_chance_scale(std::max(0.0f, static_cast<float>(
					get_number(definition, "wind_chance_scale", 0.35)))),
			m_time_moves(get_number(definition, "time_speed", 1.0) > 0.0)
	{
	}

	void trigger(ServerEnvironment *env, v3pos_t p, MapNode n,
			u32 active_object_count, u32 active_object_count_wider, v3pos_t neighbor_pos,
			uint8_t activate) override
	{
		ServerMap *map = &env->getServerMap();
		const auto *ndef = env->getGameDef()->ndef();
		if (!supports_snow(ndef->get(n)))
			return;

		v3pos_t top_pos = p + v3pos_t(0, 1, 0);
		if (map->getNodeTry(top_pos).getContent() != CONTENT_AIR ||
				get_node_light(env, map, ndef, top_pos, true) <
						LIGHT_SUN - m_sky_tolerance)
			return;

		const float heat = map->updateBlockHeat(env, p);
		const float humidity = map->updateBlockHumidity(env, p);
		int add = snow_amount(env, map, ndef, p, heat, humidity);
		if (add <= 0)
			return;

		const auto wind = get_wind(env, map, top_pos);
		const float speed = std::sqrt(wind.X * wind.X + wind.Z * wind.Z);
		const float wind_chance = rangelim(
				speed * m_wind_chance_scale, 0.0f, 0.85f);
		if (speed >= 0.25f && myrand_range(0.0f, 1.0f) < wind_chance) {
			const int dx = rangelim(static_cast<int>(std::lround(wind.X * m_wind_scale)),
					-m_wind_limit, m_wind_limit);
			const int dz = rangelim(static_cast<int>(std::lround(wind.Z * m_wind_scale)),
					-m_wind_limit, m_wind_limit);
			if (dx != 0 || dz != 0) {
				const v3pos_t wind_top = top_pos + v3pos_t(dx, 0, dz);
				if (map->getNodeTry(wind_top).getContent() == CONTENT_AIR) {
					const v3pos_t wind_base = wind_top - v3pos_t(0, 1, 0);
					const MapNode wind_node = map->getNodeTry(wind_base);
					if (wind_node.getContent() == m_snow_content ||
							supports_snow(ndef->get(wind_node))) {
						p = wind_base;
						top_pos = wind_top;
					}
				}
			}
		}

		MapNode target = map->getNodeTry(p);
		if (target.getContent() == m_snow_content) {
			int min_level = target.getLevel(ndef);
			v3pos_t min_pos = p;
			bool update_falling = false;
			std::array<int, 7> directions{1, 2, 3, 4, 5, 6, 7};
			const size_t direction_count = heat < -10.0f ? 6 : 7;
			for (size_t i = direction_count; i > 1; --i) {
				const size_t j = myrand_range(0, static_cast<int>(i - 1));
				std::swap(directions[i - 1], directions[j]);
			}

			for (size_t i = 0; i < direction_count && min_level > 1; ++i) {
				if (directions[i] >= 5)
					break;
				v3pos_t offset(0, 0, 0);
				switch (directions[i]) {
				case 1: offset.X = 1; break;
				case 2: offset.X = -1; break;
				case 3: offset.Z = -1; break;
				case 4: offset.Z = 1; break;
				default: break;
				}

				const v3pos_t candidate = min_pos + offset;
				const MapNode candidate_node = map->getNodeTry(candidate);
				if (candidate_node.getContent() == CONTENT_AIR) {
					min_pos = candidate;
					env->setNode(min_pos, MapNode(m_snow_content), 2);
					const int random_max = static_cast<int>(-heat / 2.0f);
					if (random_max > 1 && myrand_range(1, random_max) > 1)
						update_falling = true;
					break;
				}
				if (candidate_node.getContent() == m_snow_content) {
					const int level = candidate_node.getLevel(ndef);
					if (level < min_level) {
						min_level = level;
						min_pos = candidate;
					}
				}
			}

			p = min_pos;
			top_pos = p + v3pos_t(0, 1, 0);
			target = map->getNodeTry(p);
			add = target.addLevel(ndef, add);
			env->swapNode(p, target, 2);
			if (!m_time_moves)
				add = 0;
			if (add > 0)
				env->setNode(p, MapNode(m_ice_content), 2);
			else if (activate != 1 && update_falling)
				env->nodeUpdate(p, 5, 1);
		}

		if (add > 0 && map->getNodeTry(top_pos).getContent() == CONTENT_AIR) {
			env->setNode(top_pos, MapNode(m_snow_content), 2);
			MapNode snow = map->getNodeTry(top_pos);
			snow.addLevel(ndef, add);
			env->swapNode(top_pos, snow, 2);
		}
	}
};

class SnowCompactABM : public ConfigurableABM
{
	const content_t m_snow_content;
	const content_t m_ice_content;

public:
	SnowCompactABM(const CoreABMDefinition &definition, content_t snow_content,
			content_t ice_content) :
			ConfigurableABM(definition),
			m_snow_content(snow_content), m_ice_content(ice_content)
	{
	}

	void trigger(ServerEnvironment *env, v3pos_t p, MapNode n,
			u32 active_object_count, u32 active_object_count_wider, v3pos_t neighbor_pos,
			uint8_t activate) override
	{
		ServerMap *map = &env->getServerMap();
		const content_t bottom_content =
				map->getNodeTry(p - v3pos_t(0, 1, 0)).getContent();
		if (bottom_content == CONTENT_IGNORE || bottom_content == CONTENT_AIR)
			return;

		const content_t top_content =
				map->getNodeTry(p + v3pos_t(0, 1, 0)).getContent();
		if (top_content == m_snow_content || top_content == m_ice_content)
			map->setNode(p, MapNode(m_ice_content));
	}
};

bool resolve_snow_nodes(const CoreABMDefinition &definition,
		const NodeDefManager *nodedef, content_t &snow_content,
		content_t &ice_content, std::string *error)
{
	const std::string snow_name = get_string(definition, "snow_node", "default:snow");
	const std::string ice_name = get_string(definition, "ice_node", "default:ice");
	snow_content = nodedef->getId(snow_name);
	if (snow_content == CONTENT_IGNORE) {
		if (error)
			*error = "unknown snow_node '" + snow_name + "'";
		return false;
	}
	ice_content = nodedef->getId(ice_name);
	if (ice_content == CONTENT_IGNORE) {
		if (error)
			*error = "unknown ice_node '" + ice_name + "'";
		return false;
	}
	return true;
}

}

ActiveBlockModifier *create_precipitation(const CoreABMDefinition &definition,
		const NodeDefManager *nodedef, std::string *error)
{
	if (definition.action == "rain_fill") {
		if (!validate_params(definition,
					{"cloud_height", "heat_min", "heat_max", "phase_heat_max",
							"rain_humidity", "wet_humidity", "wet_span", "rate",
							"max_amount", "existing_water_multiplier", "wind_scale",
							"wind_limit", "wind_chance_scale"},
					{}, {"water_node"}, error))
			return nullptr;
		const std::string water_name =
				get_string(definition, "water_node", "default:water_flowing");
		const content_t water_content = nodedef->getId(water_name);
		if (water_content == CONTENT_IGNORE) {
			if (error)
				*error = "unknown water_node '" + water_name + "'";
			return nullptr;
		}
		return new RainFillABM(definition, water_content);
	}
	if (definition.action == "snow_fill") {
		if (!validate_params(definition,
					{"cloud_height", "heat_max", "phase_heat_min", "snow_humidity",
							"wet_heat_min", "wet_heat_span", "humidity_reference", "rate",
							"max_amount", "sky_tolerance", "wind_scale", "wind_limit",
							"wind_chance_scale", "time_speed"},
					{}, {"snow_node", "ice_node"}, error))
			return nullptr;
		content_t snow_content;
		content_t ice_content;
		if (!resolve_snow_nodes(
					definition, nodedef, snow_content, ice_content, error))
			return nullptr;
		return new SnowFillABM(definition, snow_content, ice_content);
	}
	if (definition.action == "snow_compact") {
		if (!validate_params(
					definition, {}, {}, {"snow_node", "ice_node"}, error))
			return nullptr;
		content_t snow_content;
		content_t ice_content;
		if (!resolve_snow_nodes(
					definition, nodedef, snow_content, ice_content, error))
			return nullptr;
		return new SnowCompactABM(definition, snow_content, ice_content);
	}

	if (error)
		*error = "unsupported precipitation action '" + definition.action + "'";
	return nullptr;
}

}
