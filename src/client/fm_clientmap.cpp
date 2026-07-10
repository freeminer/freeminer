/*
Copyright (C) 2026 proller <proler@gmail.com>
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

#include "clientmap.h"

#include "client.h"
#include "client/shader.h"
#include "fm_far_calc.h"
#include "mapgen/mapgen.h"
#include "mapblock.h"
#include "profiler.h"
#include "settings.h"
#include "util/numeric.h"

#include <IVideoDriver.h>
#include <matrix4.h>

#include <algorithm>
#include <atomic>
#include <cmath>
#include <cstdint>
#include <iterator>
#include <unordered_map>

namespace
{
constexpr float FAR_FOG_STORM_HUMIDITY = 99.0f;
constexpr float FAR_FOG_STORM_MIN_CLOUD_HEIGHT_RATIO = 2.0f / 3.0f;
constexpr size_t FAR_FOG_TERRAIN_CACHE_MAX = 32768;
constexpr float FAR_FOG_MAX_WIND = 8.0f;
constexpr float FAR_FOG_TWO_PI = 6.2831853f;
constexpr float FAR_FOG_MIN_VISIBLE_HUMIDITY = 48.0f * 1.3f;

struct FarFogClimate
{
	float heat = 0.0f;
	float humidity = 0.0f;
};

float smoothstep_f(float edge0, float edge1, float x)
{
	const float range = edge1 - edge0;
	if (std::abs(range) < 0.0001f)
		return x >= edge1 ? 1.0f : 0.0f;

	float t = (x - edge0) / range;
	t = std::clamp(t, 0.0f, 1.0f);
	return t * t * (3.0f - 2.0f * t);
}

float far_fog_wicked_time_of_day(float time_of_day)
{
	constexpr float nightlength = 0.415f;
	constexpr float wn = nightlength / 2.0f;
	if (time_of_day > wn && time_of_day < 1.0f - wn)
		return (time_of_day - wn) / (1.0f - wn * 2.0f) * 0.5f + 0.25f;
	if (time_of_day < 0.5f)
		return time_of_day / wn * 0.25f;
	return 1.0f - ((1.0f - time_of_day) / wn * 0.25f);
}

float far_fog_moon_brightness(float time_of_day, const v3opos_t &camera_position)
{
	const float wicked_time = far_fog_wicked_time_of_day(time_of_day);
	v3f moon_direction(0.0f, 0.0f, -1.0f);
	moon_direction.rotateXZBy(270.0f);
	moon_direction.rotateXYBy(wicked_time * 360.0f - 90.0f);

	const v3pos_t player_position = floatToInt(camera_position, BS);
	const float orbit_tilt =
			-70.0f * static_cast<float>(player_position.Z) / MAX_MAP_GENERATION_LIMITF;
	moon_direction.rotateYZBy(orbit_tilt);

	return std::clamp(107.143f * moon_direction.Y, 0.0f, 1.0f);
}

u8 far_fog_color_channel(float value, float light)
{
	return static_cast<u8>(
			std::clamp<int>(static_cast<int>(std::lround(value * light)), 0, 255));
}

uint32_t far_fog_hash(v3bpos_t pos, block_step_t step, uint32_t salt)
{
	uint32_t h = 2166136261u ^ salt ^ static_cast<uint32_t>(step);
	const auto mix = [&h](int32_t value) {
		h ^= static_cast<uint32_t>(value);
		h *= 16777619u;
		h ^= h >> 13;
	};
	mix(pos.X);
	mix(pos.Y);
	mix(pos.Z);
	h ^= h >> 16;
	h *= 2246822519u;
	h ^= h >> 13;
	h *= 3266489917u;
	h ^= h >> 16;
	return h;
}

float far_fog_signed_noise(v3bpos_t pos, block_step_t step, uint32_t salt)
{
	constexpr float scale = 1.0f / 2147483647.5f;
	return static_cast<float>(far_fog_hash(pos, step, salt)) * scale - 1.0f;
}

float far_fog_cycle_distance(float value, float center)
{
	value -= std::floor(value);
	center -= std::floor(center);
	const float distance = std::abs(value - center);
	return std::min(distance, 1.0f - distance);
}

struct FarFogClimateCacheKey
{
	const Mapgen *mapgen = nullptr;
	v3bpos_t block_pos;
	uint64_t seed = 0;
	block_step_t step = 0;
	uint16_t time_bucket = 0;
	int32_t weather_bucket = 0;
	bool use_weather = false;

	bool operator==(const FarFogClimateCacheKey &other) const
	{
		return mapgen == other.mapgen && block_pos == other.block_pos &&
			   seed == other.seed && step == other.step &&
			   time_bucket == other.time_bucket &&
			   weather_bucket == other.weather_bucket && use_weather == other.use_weather;
	}
};

struct FarFogClimateCacheKeyHash
{
	std::size_t operator()(const FarFogClimateCacheKey &key) const
	{
		std::size_t h = far_fog_hash(key.block_pos, key.step, 0x74a7c15u);
		const auto mix = [&h](std::size_t value) {
			h ^= value + 0x9e3779b97f4a7c15ull + (h << 6) + (h >> 2);
		};
		mix(std::hash<const Mapgen *>{}(key.mapgen));
		mix(std::hash<uint64_t>{}(key.seed));
		mix(key.time_bucket);
		mix(static_cast<uint32_t>(key.weather_bucket));
		mix(key.use_weather ? 1u : 0u);
		return h;
	}
};

FarFogClimate far_fog_climate_from_block(const MapBlockPtr &block)
{
	return {
			.heat = static_cast<float>(block->heat.load() + block->heat_add.load()),
			.humidity = static_cast<float>(
					block->humidity.load() + block->humidity_add.load()),
	};
}

FarFogClimate far_fog_generated_climate(
		Client *client, const v3bpos_t &block_pos, block_step_t step, bpos_t block_span)
{
	auto *mapgen = client ? client->far_container.m_mg : nullptr;
	if (mapgen) {
		const float timeofday = client->getEnv().getTimeOfDayF();
		const float weather_time = static_cast<float>(
				client->m_uptime.load(std::memory_order_relaxed) *
				client->getEnv().m_time_of_day_speed.load(std::memory_order_relaxed));
		const bool use_weather = client->use_weather && client->far_container.use_weather;
		const auto time_bucket = static_cast<uint16_t>(std::clamp<int>(
				static_cast<int>(std::lround(timeofday * 240.0f)), 0, 240));
		const auto weather_bucket =
				static_cast<int32_t>(std::floor(weather_time / 60.0f));
		const FarFogClimateCacheKey key{
				.mapgen = mapgen,
				.block_pos = block_pos,
				.seed = client->getMapSeed(),
				.step = step,
				.time_bucket = time_bucket,
				.weather_bucket = use_weather ? weather_bucket : 0,
				.use_weather = use_weather,
		};

		thread_local static std::unordered_map<FarFogClimateCacheKey, FarFogClimate,
				FarFogClimateCacheKeyHash>
				cache;
		if (cache.size() > 16384)
			cache.clear();

		if (const auto it = cache.find(key); it != cache.end())
			return it->second;

		const bpos_t center_offset = block_span * MAP_BLOCKSIZE / 2;
		const v3pos_t sample_pos{
				static_cast<pos_t>(block_pos.X * MAP_BLOCKSIZE + center_offset),
				static_cast<pos_t>(block_pos.Y * MAP_BLOCKSIZE + center_offset),
				static_cast<pos_t>(block_pos.Z * MAP_BLOCKSIZE + center_offset),
		};
		const FarFogClimate climate{
				.heat = static_cast<float>(mapgen->calcBlockHeat(sample_pos,
						client->getMapSeed(), timeofday, weather_time, use_weather)),
				.humidity = static_cast<float>(mapgen->calcBlockHumidity(sample_pos,
						client->getMapSeed(), timeofday, weather_time, use_weather)),
		};
		cache.emplace(key, climate);
		return climate;
	}

	const float center_y =
			static_cast<float>(block_pos.Y) + static_cast<float>(block_span) * 0.5f;
	const float altitude_nodes = center_y * MAP_BLOCKSIZE;
	const float altitude_dry = smoothstep_f(80.0f, 720.0f, altitude_nodes);
	const float lowland_mist = 1.0f - smoothstep_f(24.0f, 180.0f, altitude_nodes);
	const float humidity_noise = far_fog_signed_noise(block_pos, step, 0x9e3779b9u);
	const float heat_noise = far_fog_signed_noise(block_pos, step, 0x85ebca6bu);

	return {
			.heat = std::clamp(
					24.0f + heat_noise * 18.0f - altitude_dry * 18.0f, -20.0f, 80.0f),
			.humidity = std::clamp(74.0f + humidity_noise * 24.0f + lowland_mist * 18.0f -
										   altitude_dry * 32.0f,
					18.0f, 100.0f),
	};
}

bool far_fog_climate_missing(const FarFogClimate &climate)
{
	return climate.heat == 0.0f && climate.humidity == 0.0f;
}

struct FarFogAltitudeProfile
{
	float density = 0.0f;
	bool cave = false;
};

FarFogAltitudeProfile far_fog_altitude_profile(
		float y_nodes, float cloud_height, float morning_factor, float morning_height)
{
	const float cloud_base = cloud_height - 90.0f;
	const float cloud_core_top = cloud_height + 100.0f;
	const float cloud_top = cloud_height + 320.0f;
	const float cloud_layer = smoothstep_f(cloud_base, cloud_height - 20.0f, y_nodes) *
							  (1.0f - smoothstep_f(cloud_core_top, cloud_top, y_nodes));
	const float cave_layer = 1.0f - smoothstep_f(-256.0f, -32.0f, y_nodes);
	const float surface_layer =
			morning_factor * smoothstep_f(-morning_height * 0.75f, -8.0f, y_nodes) *
			(1.0f - smoothstep_f(morning_height * 0.45f, morning_height, y_nodes));
	const float surface_density = surface_layer * 0.72f;

	return {
			.density = std::max({cloud_layer, cave_layer * 0.80f, surface_density}),
			.cave = cave_layer > cloud_layer && cave_layer > surface_density,
	};
}

float far_fog_density(const FarFogClimate &climate, const FarFogAltitudeProfile &altitude)
{
	const float humidity = climate.humidity;
	if (humidity <= FAR_FOG_MIN_VISIBLE_HUMIDITY || altitude.density <= 0.0f)
		return 0.0f;

	const float heat = climate.heat;
	const float humidity_base =
			smoothstep_f(FAR_FOG_MIN_VISIBLE_HUMIDITY, 100.0f, humidity);
	const float humidity_density = humidity_base * humidity_base;
	const float cool_density = 1.0f - smoothstep_f(16.0f, 44.0f, heat);
	const float warm_mist = smoothstep_f(84.0f, 100.0f, humidity) * 0.22f;
	const float saturated_air = smoothstep_f(88.0f, 100.0f, humidity);
	const float saturation_boost =
			saturated_air * saturated_air * (0.52f + cool_density * 0.24f);
	return std::clamp((humidity_density * (0.18f + cool_density * 0.72f) + warm_mist +
							  saturation_boost) *
							  altitude.density,
			0.0f, 1.0f);
}

float far_fog_visual_height(float source_size, const FarFogAltitudeProfile &altitude)
{
	constexpr float puff_scale = 1.5f;
	if (altitude.cave)
		return std::max(1.0f, source_size * 1.84f * puff_scale);

	return std::max(1.0f, source_size * 1.64f * puff_scale);
}

float far_fog_visual_width(float source_size)
{
	constexpr float puff_scale = 1.5f;
	return std::max(1.0f, source_size * 1.64f * puff_scale);
}

float far_fog_visual_depth(float source_size)
{
	constexpr float puff_scale = 1.5f;
	return std::max(1.0f, source_size * 1.64f * puff_scale);
}

v3f far_fog_limit_wind(v3f wind)
{
	if (!std::isfinite(wind.X) || !std::isfinite(wind.Y) || !std::isfinite(wind.Z))
		return {};

	const float length = wind.getLength();
	if (length > FAR_FOG_MAX_WIND && length > 0.0001f)
		wind *= FAR_FOG_MAX_WIND / length;

	return wind * BS;
}

v3f far_fog_wind_from_block(const MapBlockPtr &block)
{
	if (!block)
		return {};

	return far_fog_limit_wind(block->wind);
}

v3f far_fog_generated_wind(Client *client, const v3bpos_t &block_pos, bpos_t block_span)
{
	auto *mapgen = client ? client->far_container.m_mg : nullptr;
	if (!mapgen)
		return {};

	const float timeofday = client->getEnv().getTimeOfDayF();
	const float weather_time = static_cast<float>(
			client->m_uptime.load(std::memory_order_relaxed) *
			client->getEnv().m_time_of_day_speed.load(std::memory_order_relaxed));
	const bool use_weather = client->use_weather && client->far_container.use_weather;
	const bpos_t center_offset = block_span * MAP_BLOCKSIZE / 2;
	const v3pos_t sample_pos{
			static_cast<pos_t>(block_pos.X * MAP_BLOCKSIZE + center_offset),
			static_cast<pos_t>(block_pos.Y * MAP_BLOCKSIZE + center_offset),
			static_cast<pos_t>(block_pos.Z * MAP_BLOCKSIZE + center_offset),
	};

	weather::wind_t wind;
	if (!mapgen->calcBlockWind(sample_pos, client->getMapSeed(), timeofday, weather_time,
				use_weather, &wind))
		return {};

	return far_fog_limit_wind(wind);
}

v3f far_fog_fallback_wind(
		const v3bpos_t &block_pos, block_step_t step, const FarFogClimate &climate)
{
	const float humidity_motion = smoothstep_f(45.0f, 100.0f, climate.humidity);
	const float heat_motion = 1.0f - smoothstep_f(-12.0f, 38.0f, climate.heat);
	const float speed = (0.18f + humidity_motion * 0.52f + heat_motion * 0.16f) * BS;
	const float angle =
			(far_fog_signed_noise(block_pos, step, 0xd1b54a35u) + 1.0f) * 3.14159265f;
	const float vertical =
			far_fog_signed_noise(block_pos, step, 0x94d049bbu) * 0.16f * speed;

	return v3f(std::cos(angle) * speed, vertical, std::sin(angle) * speed);
}

float far_fog_phase(const v3bpos_t &block_pos, block_step_t step)
{
	return (far_fog_signed_noise(block_pos, step, 0x4cf5ad43u) + 1.0f) *
		   (FAR_FOG_TWO_PI * 0.5f);
}

v3f far_fog_scene_camera_position(
		const v3opos_t &camera_position, const v3pos_t &camera_offset)
{
	return oposToV3f(camera_position - intToFloat(camera_offset, BS));
}

v3bpos_t far_fog_align_block_pos(const v3bpos_t &block_pos, uint8_t align_pow)
{
	if (!align_pow)
		return block_pos;
	return {
			static_cast<bpos_t>((block_pos.X >> align_pow) << align_pow),
			static_cast<bpos_t>((block_pos.Y >> align_pow) << align_pow),
			static_cast<bpos_t>((block_pos.Z >> align_pow) << align_pow),
	};
}

float far_fog_distance_to_box(const v3f &point, const v3f &box_min, float box_size)
{
	const v3f box_max = box_min + v3f(box_size, box_size, box_size);
	const float dx = std::max(std::max(box_min.X - point.X, 0.0f), point.X - box_max.X);
	const float dy = std::max(std::max(box_min.Y - point.Y, 0.0f), point.Y - box_max.Y);
	const float dz = std::max(std::max(box_min.Z - point.Z, 0.0f), point.Z - box_max.Z);
	return std::sqrt(dx * dx + dy * dy + dz * dz);
}

std::size_t far_fog_terrain_cache_key(const v3bpos_t &block_pos, bpos_t block_span)
{
	const v3bpos_t column_pos{block_pos.X, 0, block_pos.Z};
	std::size_t h =
			far_fog_hash(column_pos, farmesh::rangeToStep(block_span), 0x5c13d53u);
	h ^= std::hash<bpos_t>{}(block_span) + 0x9e3779b97f4a7c15ull + (h << 6) + (h >> 2);
	return h;
}

float far_fog_average_terrain_y(
		Mapgen *mapgen, const v3bpos_t &block_pos, bpos_t block_span)
{
	if (!mapgen || block_span <= 0)
		return 0.0f;

	const pos_t min_x = block_pos.X * MAP_BLOCKSIZE;
	const pos_t min_z = block_pos.Z * MAP_BLOCKSIZE;
	const pos_t max_x = (block_pos.X + block_span) * MAP_BLOCKSIZE - 1;
	const pos_t max_z = (block_pos.Z + block_span) * MAP_BLOCKSIZE - 1;
	const pos_t center_x = min_x + (max_x - min_x) / 2;
	const pos_t center_z = min_z + (max_z - min_z) / 2;

	float sum = 0.0f;
	int count = 0;
	const auto sample = [&](pos_t x, pos_t z) {
		sum += static_cast<float>(mapgen->getGroundLevelAtPoint(v2pos_t(x, z)));
		++count;
	};

	sample(center_x, center_z);
	sample(min_x, min_z);
	sample(max_x, min_z);
	sample(min_x, max_z);
	sample(max_x, max_z);

	return count ? sum / static_cast<float>(count) : 0.0f;
}
}

irr_ptr<ClientMap> ClientMap::create(Client *client, RenderingEngine *rendering_engine,
		MapDrawControl &control, s32 id)
{
	return irr_ptr<ClientMap>(new ClientMap(client, rendering_engine, control, id));
}

void ClientMap::initFarFogMaterial()
{
	if (m_far_fog_material_ready)
		return;

	auto *shader_source = m_client->getShaderSource();
	const u32 shader_id = shader_source->getShaderRaw("far_fog_shader", true);

	m_far_fog_material = video::SMaterial();
	m_far_fog_material.MaterialType = shader_source->getShaderInfo(shader_id).material;
	m_far_fog_material.ColorParam = video::SColor(255, 255, 255, 255);
	m_far_fog_material.BackfaceCulling = false;
	m_far_fog_material.FrontfaceCulling = false;
	m_far_fog_material.FogEnable = false;
	m_far_fog_material.ZBuffer = video::ECFN_LESSEQUAL;
	m_far_fog_material.ZWriteEnable = video::EZW_OFF;
	m_far_fog_material.AntiAliasing = video::EAAM_SIMPLE;

	m_far_fog_meshbuffers.clear();
	m_far_fog_meshbuffers.emplace_back(new scene::SMeshBuffer());
	m_far_fog_meshbuffers.back()->setHardwareMappingHint(scene::EHM_DYNAMIC);
	m_far_fog_meshbuffers.back()->Material = m_far_fog_material;

	m_far_fog_material_ready = true;
}

void ClientMap::updateFarFogCells()
{
	if (!m_control.farmesh)
		return;

	const auto far_iteration_draw_snapshot = far_iteration_draw;
	const v3bpos_t player_block_pos = getNodeBlockPos(m_camera_position_node);
	const auto cell_size_pow = m_control.cell_size_pow;
	const auto farmesh = m_control.farmesh;
	const auto fog_farmesh =
			std::max<pos_t>(farmesh, g_settings->getPos("volumetric_fog"));
	const auto farmesh_quality_pow = m_control.farmesh_quality_pow;
	const auto camera_offset = m_camera_offset;
	const auto camera_scene_position =
			far_fog_scene_camera_position(m_camera_position, m_camera_offset);
	const auto far_iteration_clean_snapshot = far_iteration_clean;
	if (m_far_fog_cells_ready && player_block_pos == m_far_fog_cells_origin &&
			m_far_fog_cells_iteration_draw == far_iteration_draw_snapshot)
		return;

	const auto async_status = m_far_fog_async.step(
			[this, cell_size_pow, fog_farmesh, farmesh_quality_pow, player_block_pos,
					camera_offset, camera_scene_position, far_iteration_clean_snapshot,
					far_iteration_draw_snapshot]() {
				std::vector<FarFogCell> cells;
				cells.reserve(4096);
				std::unordered_map<v3bpos_t, size_t, v3posHash, v3posEqual> cell_indexes;
				cell_indexes.reserve(4096);
				std::unordered_map<v3bpos_t, MapBlockPtr, v3posHash, v3posEqual>
						published_blocks;
				std::unordered_map<std::size_t, float> terrain_cache;
				terrain_cache.reserve(4096);
				std::unordered_map<std::size_t, float> terrain_updates;
				terrain_updates.reserve(1024);
				auto *terrain_mapgen = m_client ? m_client->far_container.m_mg : nullptr;

				{
					std::lock_guard<std::mutex> lock(m_far_fog_terrain_cache_mutex);
					terrain_cache = m_far_fog_terrain_cache;
				}

				{
					const auto lock = m_far_blocks.lock_shared_rec();
					published_blocks.reserve(m_far_blocks.size());
					for (const auto &[block_pos, block] : m_far_blocks) {
						if (!block)
							continue;
						if (far_iteration_clean_snapshot &&
								block->far_iteration < far_iteration_clean_snapshot)
							continue;
						if (block->far_iteration < far_iteration_draw_snapshot)
							continue;

						const block_step_t step = block->far_step_draw ?: block->far_step;
						if (step >= FARMESH_STEP_MAX)
							continue;

						published_blocks.emplace(block_pos, block);
					}
				}

				const auto find_stored_block = [&](const v3bpos_t &block_pos,
													   block_step_t step) -> MapBlockPtr {
					if (const auto it = published_blocks.find(block_pos);
							it != published_blocks.end())
						return it->second;

					if (!step)
						return getBlock(block_pos, true, true);

					if (step >= far_blocks_storage.size())
						return {};

					auto &storage = far_blocks_storage[step];
					const auto lock = storage.lock_shared_rec();
					const auto it = storage.find(block_pos);
					if (it != storage.end() && it->second.block)
						return it->second.block;
					return {};
				};

				const auto cell_center = [&](const v3bpos_t &block_pos,
												 bpos_t block_span) {
					const float block_size =
							static_cast<float>(MAP_BLOCKSIZE * block_span) * BS;
					const v3f block_min = oposToV3f(
							intToFloat(block_pos * MAP_BLOCKSIZE - camera_offset, BS));
					return block_min +
						   v3f(block_size * 0.5f, block_size * 0.5f, block_size * 0.5f);
				};

				const auto terrain_reference = [&](const v3bpos_t &block_pos,
													   bpos_t block_span) -> float {
					const auto key = far_fog_terrain_cache_key(block_pos, block_span);
					if (const auto it = terrain_cache.find(key);
							it != terrain_cache.end())
						return it->second;
					const float terrain_y = far_fog_average_terrain_y(
							terrain_mapgen, block_pos, block_span);
					terrain_cache.emplace(key, terrain_y);
					terrain_updates.emplace(key, terrain_y);
					return terrain_y;
				};

				const auto add_cell = [&](const v3bpos_t &block_pos, block_step_t step,
											  bpos_t block_span, const MapBlockPtr &block,
											  const FarFogClimate *generated_climate) {
					if (step >= FARMESH_STEP_MAX)
						return;
					if (block_span <= 0)
						return;

					FarFogClimate known_climate;
					bool has_known_climate = false;
					if (block) {
						known_climate = far_fog_climate_from_block(block);
						has_known_climate = !far_fog_climate_missing(known_climate);
					}
					if (generated_climate) {
						known_climate = *generated_climate;
						has_known_climate = !far_fog_climate_missing(known_climate);
					}
					if (has_known_climate && known_climate.humidity <= 0.0f)
						return;

					const float terrain_y = terrain_reference(block_pos, block_span);
					v3f wind = far_fog_wind_from_block(block);
					if (wind.getLengthSQ() <= 0.0001f)
						wind = far_fog_generated_wind(m_client, block_pos, block_span);
					if (const auto it = cell_indexes.find(block_pos);
							it != cell_indexes.end()) {
						auto &cell = cells[it->second];
						if (block && !cell.block) {
							cell.block = block;
							cell.wind = wind;
						} else if (wind.getLengthSQ() > 0.0001f &&
								   cell.wind.getLengthSQ() <= 0.0001f) {
							cell.wind = wind;
						}
						if (generated_climate && !cell.has_climate) {
							cell.has_climate = true;
							cell.heat = generated_climate->heat;
							cell.humidity = generated_climate->humidity;
						}
						if (block_span < cell.block_span) {
							const v3f block_center = cell_center(block_pos, block_span);
							cell.step = step;
							cell.block_span = block_span;
							cell.distance =
									block_center.getDistanceFrom(camera_scene_position);
							cell.terrain_y = terrain_y;
							cell.wind = wind;
						}
						return;
					}

					const v3f block_center = cell_center(block_pos, block_span);
					const float distance =
							block_center.getDistanceFrom(camera_scene_position);

					const bool has_generated_climate = generated_climate != nullptr;
					cell_indexes.emplace(block_pos, cells.size());
					cells.push_back(FarFogCell{
							.block_pos = block_pos,
							.step = step,
							.block_span = block_span,
							.distance = distance,
							.block = block,
							.has_climate = has_generated_climate,
							.heat = has_generated_climate ? generated_climate->heat
														  : 0.0f,
							.humidity = has_generated_climate
												? generated_climate->humidity
												: 0.0f,
							.terrain_y = terrain_y,
							.wind = wind,
					});
				};

				for (const auto &[block_pos, block] : published_blocks) {
					const block_step_t step = block->far_step_draw ?: block->far_step;
					const bpos_t block_span = 1 << (step + cell_size_pow);
					FarFogClimate generated_climate;
					const FarFogClimate *generated_climate_ptr = nullptr;
					if (far_fog_climate_missing(far_fog_climate_from_block(block))) {
						generated_climate = far_fog_generated_climate(
								m_client, block_pos, step, block_span);
						generated_climate_ptr = &generated_climate;
					}
					add_cell(block_pos, step, block_span, block, generated_climate_ptr);
				}

				const auto add_generated_grid_cell = [&](const v3bpos_t &block_pos,
															 const bpos_t block_span,
															 const block_step_t step) {
					if (step >= FARMESH_STEP_MAX)
						return;
					const auto block = find_stored_block(block_pos, step);
					FarFogClimate generated_climate;
					const FarFogClimate *generated_climate_ptr = nullptr;
					if (!block ||
							far_fog_climate_missing(far_fog_climate_from_block(block))) {
						generated_climate = far_fog_generated_climate(
								m_client, block_pos, step, block_span);
						generated_climate_ptr = &generated_climate;
					}
					add_cell(block_pos, step, block_span, block, generated_climate_ptr);
				};

				farmesh::runFarAll(player_block_pos, cell_size_pow, fog_farmesh,
						farmesh_quality_pow, 0, false, 0,
						[&](const v3bpos_t &block_pos, const bpos_t &block_span,
								const block_step_t &step) -> bool {
							add_generated_grid_cell(block_pos,
									static_cast<bpos_t>(block_span << cell_size_pow),
									step);
							return false;
						});

				std::sort(cells.begin(), cells.end(),
						[](const FarFogCell &a, const FarFogCell &b) {
							if (a.distance != b.distance)
								return a.distance > b.distance;
							if (a.step != b.step)
								return a.step > b.step;
							return a.block_pos > b.block_pos;
						});

				const auto current =
						m_far_fog_cells_current.load(std::memory_order_relaxed);
				const uint8_t write_index = current ? 0 : 1;
				{
					std::lock_guard<std::mutex> lock(m_far_fog_cells_mutex);
					m_far_fog_cells[write_index] = std::move(cells);
					m_far_fog_cells_current.store(write_index, std::memory_order_release);
					m_far_fog_cells_ready.store(true, std::memory_order_release);
				}
				if (!terrain_updates.empty()) {
					std::lock_guard<std::mutex> lock(m_far_fog_terrain_cache_mutex);
					if (m_far_fog_terrain_cache.size() + terrain_updates.size() >
							FAR_FOG_TERRAIN_CACHE_MAX) {
						m_far_fog_terrain_cache.clear();
					}
					for (const auto &[key, terrain_y] : terrain_updates)
						m_far_fog_terrain_cache[key] = terrain_y;
				}
			});
	if (async_status != async_step_runner::IN_PROGRESS) {
		m_far_fog_cells_origin = player_block_pos;
		m_far_fog_cells_iteration_draw = far_iteration_draw_snapshot;
	}
}

u32 ClientMap::rebuildFarFogMeshBuffer()
{
	if (!m_control.farmesh)
		return 0;

	const pos_t volumetric_fog_range_nodes = g_settings->getPos("volumetric_fog");
	if (volumetric_fog_range_nodes <= 0)
		return 0;
	const float volumetric_fog_range =
			static_cast<float>(volumetric_fog_range_nodes) * BS;

	initFarFogMaterial();
	updateFarFogCells();
	const auto far_fog_vertex_count = [&]() {
		u32 vertex_count = 0;
		for (const auto &buffer : m_far_fog_meshbuffers) {
			if (buffer)
				vertex_count += buffer->getVertexCount();
		}
		return vertex_count;
	};

	v3f forward = m_camera_direction;
	if (forward.getLengthSQ() < 0.0001f)
		forward = v3f(0.0f, 0.0f, 1.0f);
	forward.normalize();

	constexpr pos_t camera_bucket_size = MAP_BLOCKSIZE;
	const v3pos_t camera_bucket{
			static_cast<pos_t>(m_camera_position_node.X / camera_bucket_size),
			static_cast<pos_t>(m_camera_position_node.Y / camera_bucket_size),
			static_cast<pos_t>(m_camera_position_node.Z / camera_bucket_size),
	};
	const float time_of_day = m_client->getEnv().getTimeOfDayF();
	const auto time_bucket = static_cast<uint16_t>(
			std::clamp<int>(static_cast<int>(std::lround(time_of_day * 240.0f)), 0, 240));
	const bool cells_ready = m_far_fog_cells_ready.load(std::memory_order_acquire);
	const uint8_t cells_current = m_far_fog_cells_current.load(std::memory_order_acquire);
	const uint32_t cells_iteration = m_far_fog_cells_iteration_draw;
	const bool direction_changed =
			m_far_fog_mesh_camera_direction.dotProduct(forward) < 0.985f;

	if (m_far_fog_mesh_valid && m_far_fog_mesh_cells_ready == cells_ready &&
			m_far_fog_mesh_cells_current == cells_current &&
			m_far_fog_mesh_iteration_draw == cells_iteration &&
			m_far_fog_mesh_camera_bucket == camera_bucket &&
			m_far_fog_mesh_camera_offset == m_camera_offset &&
			m_far_fog_mesh_time_bucket == time_bucket && !direction_changed) {
		return far_fog_vertex_count();
	}

	for (auto &buffer : m_far_fog_meshbuffers) {
		if (!buffer)
			continue;
		buffer->Vertices->Data.clear();
		buffer->Indices->Data.clear();
		buffer->Material = m_far_fog_material;
	}

	v3f right = v3f(0.0f, 1.0f, 0.0f).crossProduct(forward);
	if (right.getLengthSQ() < 0.0001f)
		right = v3f(1.0f, 0.0f, 0.0f);
	right.normalize();

	v3f up = forward.crossProduct(right);
	if (up.getLengthSQ() < 0.0001f)
		up = v3f(0.0f, 1.0f, 0.0f);
	up.normalize();

	constexpr u32 max_fog_quads = 0x3fff;
	const u32 max_fog_buffers =
			std::clamp<u32>(static_cast<u32>(volumetric_fog_range_nodes / 2000), 6, 16);
	constexpr float fog_alpha_scale = 0.46f;
	constexpr u32 fog_quad_limit = max_fog_quads;
	static const float cloud_height_nodes =
			static_cast<float>(g_settings->getS16("cloud_height"));
	static const float morning_height_nodes =
			std::max<float>(16.0f, g_settings->getS16("weather_humidity_morning_height"));
	const float morning_factor =
			1.0f -
			smoothstep_f(0.035f, 0.175f, far_fog_cycle_distance(time_of_day, 0.24f));
	u32 candidate_count = 0;
	u32 stored_count = 0;
	u32 generated_count = 0;
	u32 low_density_count = 0;
	u32 low_alpha_count = 0;
	u32 slice_count = 0;
	u32 protected_count = 0;
	u32 wind_count = 0;
	float wind_sum = 0.0f;

	const u32 daynight_ratio = m_client->getEnv().getDayNightRatio();
	const float daylight =
			smoothstep_f(175.0f, 700.0f, static_cast<float>(daynight_ratio));
	const float moon_brightness = far_fog_moon_brightness(time_of_day, m_camera_position);
	const float night_brightness = 0.12f + moon_brightness * 0.48f;
	const float fog_light =
			std::clamp(daylight + (1.0f - daylight) * night_brightness, 0.10f, 1.0f);
	const float fog_alpha_light = std::clamp(
			daylight * 1.45f + (1.0f - daylight) * (0.35f + moon_brightness * 0.30f),
			0.22f, 1.45f);
	const v3f camera_scene_position =
			far_fog_scene_camera_position(m_camera_position, m_camera_offset);
	static const pos_t water_level_nodes = g_settings->getS16("water_level");
	const bool camera_above_water = m_camera_position_node.Y > water_level_nodes;
	const float near_fog_reserve_range = std::min(volumetric_fog_range, 4096.0f * BS);
	const auto terrain_reference = [&](const v3bpos_t &block_pos,
										   bpos_t block_span) -> float {
		const auto key = far_fog_terrain_cache_key(block_pos, block_span);
		std::lock_guard<std::mutex> lock(m_far_fog_terrain_cache_mutex);
		if (const auto it = m_far_fog_terrain_cache.find(key);
				it != m_far_fog_terrain_cache.end())
			return it->second;
		return 0.0f;
	};
	const auto fog_color_for_climate = [&](u8 alpha, const FarFogClimate &climate,
											   const FarFogAltitudeProfile &altitude,
											   float fog_y_relative_nodes, float density,
											   float top_view) {
		const float cold = 1.0f - smoothstep_f(-5.0f, 35.0f, climate.heat);
		const float humidity = std::clamp(climate.humidity, 0.0f, 100.0f);
		const float thin_mist = 1.0f - smoothstep_f(52.0f, 78.0f, humidity);
		const float density_shadow = smoothstep_f(0.16f, 0.82f, density);
		const float humidity_shadow = smoothstep_f(76.0f, 100.0f, humidity);
		const bool storm_height =
				fog_y_relative_nodes >=
				cloud_height_nodes * FAR_FOG_STORM_MIN_CLOUD_HEIGHT_RATIO;
		const float storm_cloud =
				altitude.cave || !storm_height
						? 0.0f
						: smoothstep_f(FAR_FOG_STORM_HUMIDITY, 100.0f, humidity);
		const float storm_light = 1.0f - storm_cloud * (0.38f + daylight * 0.12f);
		const float density_light = std::clamp(
				1.0f - density_shadow * (0.07f + humidity_shadow * 0.16f) -
						top_view * (0.14f + density_shadow * 0.18f +
										   humidity_shadow * 0.10f + daylight * 0.06f),
				0.56f, 1.0f);
		constexpr float light_r = 190.0f;
		constexpr float light_g = 198.0f;
		constexpr float light_b = 214.0f;
		return video::SColor(alpha,
				far_fog_color_channel(std::min<float>(255.0f,
											  light_r + cold * 18.0f + thin_mist * 28.0f),
						fog_light * storm_light * density_light *
								(0.84f + daylight * 0.18f)),
				far_fog_color_channel(std::min<float>(255.0f,
											  light_g + cold * 20.0f + thin_mist * 30.0f),
						fog_light * storm_light * density_light *
								(0.91f + daylight * 0.10f)),
				far_fog_color_channel(std::min<float>(255.0f,
											  light_b + cold * 28.0f + thin_mist * 34.0f),
						std::min(1.0f, fog_light * storm_light * density_light *
											   (1.07f - daylight * 0.08f))));
	};

	if (m_far_fog_meshbuffers.empty())
		m_far_fog_meshbuffers.emplace_back(new scene::SMeshBuffer());

	size_t current_fog_buffer = 0;
	auto *buffer = m_far_fog_meshbuffers[current_fog_buffer].get();
	buffer->setHardwareMappingHint(scene::EHM_DYNAMIC);
	buffer->Material = m_far_fog_material;
	auto *vertices = &buffer->Vertices->Data;
	auto *indices = &buffer->Indices->Data;
	vertices->reserve(max_fog_quads * 4);
	indices->reserve(max_fog_quads * 6);

	constexpr u32 max_near_fog_extra_buffers = 4;
	bool allow_near_fog_extra_buffers = false;
	const auto fog_buffer_limit = [&]() -> u32 {
		return max_fog_buffers +
			   (allow_near_fog_extra_buffers ? max_near_fog_extra_buffers : 0);
	};

	const auto next_fog_buffer = [&]() -> bool {
		if (current_fog_buffer + 1 >= fog_buffer_limit())
			return false;

		++current_fog_buffer;
		if (current_fog_buffer >= m_far_fog_meshbuffers.size())
			m_far_fog_meshbuffers.emplace_back(new scene::SMeshBuffer());

		buffer = m_far_fog_meshbuffers[current_fog_buffer].get();
		buffer->setHardwareMappingHint(scene::EHM_DYNAMIC);
		buffer->Material = m_far_fog_material;
		vertices = &buffer->Vertices->Data;
		indices = &buffer->Indices->Data;
		vertices->reserve(max_fog_quads * 4);
		indices->reserve(max_fog_quads * 6);
		return true;
	};

	const auto add_fog_cell_at =
			[&](const v3f &cell_min, const float cell_size, const v3bpos_t &cell_pos,
					const block_step_t cell_step, const FarFogClimate &climate,
					const float terrain_y_nodes, const v3f &wind_world) -> bool {
		if (vertices->size() / 4 >= fog_quad_limit && !next_fog_buffer())
			return true;

		++candidate_count;

		const float cell_size_nodes = cell_size / BS;
		const float cell_min_y_nodes = static_cast<float>(cell_pos.Y * MAP_BLOCKSIZE);
		const float cell_max_y_nodes = cell_min_y_nodes + cell_size_nodes;
		const float cell_min_y_relative = cell_min_y_nodes - terrain_y_nodes;
		const float cell_max_y_relative = cell_max_y_nodes - terrain_y_nodes;
		if (camera_above_water && cell_max_y_nodes <= water_level_nodes)
			return false;

		const float cell_center_y_nodes = (cell_min_y_nodes + cell_max_y_nodes) * 0.5f;
		const float cell_center_y_relative = cell_center_y_nodes - terrain_y_nodes;
		const float cloud_focus_y_relative = std::clamp(
				cloud_height_nodes + 40.0f, cell_min_y_relative, cell_max_y_relative);
		const float surface_focus_y_relative = std::clamp(
				morning_height_nodes * 0.25f, cell_min_y_relative, cell_max_y_relative);
		const float cloud_focus_y_nodes = terrain_y_nodes + cloud_focus_y_relative;
		const float surface_focus_y_nodes = terrain_y_nodes + surface_focus_y_relative;
		const auto center_altitude = far_fog_altitude_profile(cell_center_y_relative,
				cloud_height_nodes, morning_factor, morning_height_nodes);
		const auto cloud_altitude = far_fog_altitude_profile(cloud_focus_y_relative,
				cloud_height_nodes, morning_factor, morning_height_nodes);
		const auto surface_altitude = far_fog_altitude_profile(surface_focus_y_relative,
				cloud_height_nodes, morning_factor, morning_height_nodes);
		auto altitude = center_altitude;
		float visual_center_y_nodes = cell_center_y_nodes;
		if (cloud_altitude.density > altitude.density) {
			altitude = cloud_altitude;
			visual_center_y_nodes = cloud_focus_y_nodes;
		}
		if (surface_altitude.density > altitude.density) {
			altitude = surface_altitude;
			visual_center_y_nodes = surface_focus_y_nodes;
		}

		float density = far_fog_density(climate, altitude);
		if (density <= 0.01f) {
			++low_density_count;
			return false;
		} else {
			const float humidity_visibility =
					smoothstep_f(FAR_FOG_MIN_VISIBLE_HUMIDITY, 88.0f, climate.humidity);
			const float saturated_visibility =
					smoothstep_f(88.0f, 100.0f, climate.humidity);
			const float min_density =
					0.05f + humidity_visibility * 0.10f + saturated_visibility * 0.08f;
			density = std::max(density, min_density);
		}

		const float visual_width = far_fog_visual_width(cell_size);
		const float visual_height = far_fog_visual_height(cell_size, altitude);
		const float visual_depth = far_fog_visual_depth(cell_size);
		const float half_height_nodes = visual_height / BS * 0.5f;
		const float clamped_half_height_nodes =
				std::min(half_height_nodes, cell_size_nodes * 0.5f);
		visual_center_y_nodes = std::clamp(visual_center_y_nodes,
				cell_min_y_nodes + clamped_half_height_nodes,
				cell_max_y_nodes - clamped_half_height_nodes);

		const float sphere_radius =
				std::min({visual_width, visual_height, visual_depth}) * 0.5f;
		const float radius_variation = std::clamp(
				1.0f + far_fog_signed_noise(cell_pos, cell_step, 0x7f4a7c15u) * 0.18f,
				0.82f, 1.18f);
		const float aspect_x = std::clamp(
				1.0f + far_fog_signed_noise(cell_pos, cell_step, 0x94d049bbu) * 0.07f,
				0.90f, 1.10f);
		const float aspect_y = std::clamp(
				1.0f + far_fog_signed_noise(cell_pos, cell_step, 0x6c8e9cf5u) * 0.07f,
				0.90f, 1.10f);
		const float half_width = sphere_radius * radius_variation * aspect_x;
		const float half_height = sphere_radius * radius_variation * aspect_y;
		const v3f center_jitter(far_fog_signed_noise(cell_pos, cell_step, 0x51ed270bu) *
										visual_width * 0.12f,
				far_fog_signed_noise(cell_pos, cell_step, 0xc2b2ae35u) * visual_height *
						0.08f,
				far_fog_signed_noise(cell_pos, cell_step, 0x27d4eb2fu) * visual_depth *
						0.12f);
		const v3f visual_center(cell_min.X + cell_size * 0.5f + center_jitter.X,
				cell_min.Y + (visual_center_y_nodes - cell_min_y_nodes) * BS +
						center_jitter.Y,
				cell_min.Z + cell_size * 0.5f + center_jitter.Z);

		const float cell_box_distance =
				far_fog_distance_to_box(camera_scene_position, cell_min, cell_size);
		const float visual_distance =
				visual_center.getDistanceFrom(camera_scene_position);
		const float near_fade =
				cell_box_distance <= 0.001f
						? 1.0f
						: std::max(0.45f,
								  smoothstep_f(1.0f * BS, 8.0f * BS, cell_box_distance));
		const float far_fade = 1.0f - smoothstep_f(volumetric_fog_range * 0.82f,
											  volumetric_fog_range, visual_distance);
		const float alpha_variation = std::clamp(
				1.0f + far_fog_signed_noise(cell_pos, cell_step, 0xa24baedu) * 0.18f,
				0.72f, 1.22f);
		const float humidity_alpha =
				std::clamp(0.30f +
								   smoothstep_f(FAR_FOG_MIN_VISIBLE_HUMIDITY, 90.0f,
										   climate.humidity) *
										   0.78f +
								   smoothstep_f(90.0f, 100.0f, climate.humidity) * 0.28f,
						0.20f, 1.36f);
		const float alpha_f =
				std::clamp(density * humidity_alpha * near_fade * far_fade *
								   alpha_variation * 144.0f * fog_alpha_light,
						0.0f, 196.0f);
		if (alpha_f < 2.0f) {
			++low_alpha_count;
			return false;
		}
		const auto alpha =
				static_cast<u8>(std::clamp(alpha_f * fog_alpha_scale, 1.0f, 196.0f));
		const float top_view =
				altitude.cave
						? 0.0f
						: smoothstep_f(0.15f, 0.85f, -forward.Y) *
								  smoothstep_f(30.0f, 220.0f,
										  static_cast<float>(m_camera_position_node.Y) -
												  visual_center_y_nodes);
		const auto color = fog_color_for_climate(alpha, climate, altitude,
				visual_center_y_nodes - terrain_y_nodes, density, top_view);

		const v3f rx = right * half_width;
		const v3f uy = up * half_height;
		const v3f packed_wind(
				wind_world.X, far_fog_phase(cell_pos, cell_step), wind_world.Z);
		const u16 base = static_cast<u16>(vertices->size());
		vertices->emplace_back(
				visual_center - rx - uy, packed_wind, color, v2f(0.0f, 1.0f));
		vertices->emplace_back(
				visual_center + rx - uy, packed_wind, color, v2f(1.0f, 1.0f));
		vertices->emplace_back(
				visual_center + rx + uy, packed_wind, color, v2f(1.0f, 0.0f));
		vertices->emplace_back(
				visual_center - rx + uy, packed_wind, color, v2f(0.0f, 0.0f));

		indices->push_back(base + 0);
		indices->push_back(base + 1);
		indices->push_back(base + 2);
		indices->push_back(base + 2);
		indices->push_back(base + 3);
		indices->push_back(base + 0);
		++slice_count;

		return vertices->size() / 4 >= fog_quad_limit &&
			   current_fog_buffer + 1 >= fog_buffer_limit();
	};

	const auto add_fog_cell =
			[&](const v3bpos_t &block_pos, const block_step_t fog_step,
					const bpos_t block_span, const FarFogClimate &climate,
					const float terrain_y_nodes, const v3f &wind_world) -> bool {
		const float block_size = static_cast<float>(MAP_BLOCKSIZE * block_span) * BS;
		const v3f block_min =
				oposToV3f(intToFloat(block_pos * MAP_BLOCKSIZE - m_camera_offset, BS));
		return add_fog_cell_at(block_min, block_size, block_pos, fog_step, climate,
				terrain_y_nodes, wind_world);
	};

	const auto cell_distance = [&](const v3bpos_t &block_pos, bpos_t block_span) {
		const float block_size = static_cast<float>(MAP_BLOCKSIZE * block_span) * BS;
		const v3f block_min =
				oposToV3f(intToFloat(block_pos * MAP_BLOCKSIZE - m_camera_offset, BS));
		const v3f block_center =
				block_min + v3f(block_size * 0.5f, block_size * 0.5f, block_size * 0.5f);
		return block_center.getDistanceFrom(camera_scene_position);
	};

	const auto fog_split_pow = [&](block_step_t fog_step, bpos_t block_span,
									   float distance) -> block_step_t {
		const float block_size = static_cast<float>(MAP_BLOCKSIZE * block_span) * BS;
		float target_size = static_cast<float>(MAP_BLOCKSIZE) * 4.0f * BS;
		const float range_part = distance / std::max(volumetric_fog_range, 1.0f);
		if (range_part > 0.55f)
			target_size *= 4.0f;
		else if (range_part > 0.30f)
			target_size *= 2.0f;
		block_step_t split_pow = 0;
		while ((1 << split_pow) < block_span && split_pow < 2 &&
				block_size / static_cast<float>(1 << split_pow) > target_size) {
			++split_pow;
		}
		return split_pow;
	};
	const auto draw_fog_source =
			[&](const v3bpos_t &block_pos, const block_step_t fog_step,
					const bpos_t block_span, const MapBlockPtr &block,
					const FarFogClimate *generated_climate, const float source_terrain_y,
					const v3f &source_wind_world) -> bool {
		if (vertices->size() / 4 >= fog_quad_limit && !next_fog_buffer())
			return true;

		const block_step_t split_pow =
				fog_split_pow(fog_step, block_span, cell_distance(block_pos, block_span));
		const bpos_t split_count = 1 << split_pow;
		const bpos_t cell_span = block_span >> split_pow;
		const block_step_t cell_step = static_cast<block_step_t>(
				std::min<int>(FARMESH_STEP_MAX - 1, farmesh::rangeToStep(cell_span)));
		FarFogClimate climate;
		bool missing_climate = true;
		if (block) {
			climate = far_fog_climate_from_block(block);
			missing_climate = far_fog_climate_missing(climate);
		}
		if (missing_climate && generated_climate) {
			climate = *generated_climate;
			missing_climate = far_fog_climate_missing(climate);
			if (!missing_climate)
				++generated_count;
		}
		if (!missing_climate && block)
			++stored_count;
		if (missing_climate)
			return false;

		v3f wind_world = source_wind_world;
		if (wind_world.getLengthSQ() <= 0.0001f)
			wind_world = far_fog_wind_from_block(block);
		if (wind_world.getLengthSQ() <= 0.0001f)
			wind_world = far_fog_fallback_wind(block_pos, fog_step, climate);
		const float wind_length = wind_world.getLength();
		if (wind_length > 0.001f) {
			wind_sum += wind_length / BS;
			++wind_count;
		}

		if (!split_pow) {
			if (add_fog_cell(block_pos, fog_step, block_span, climate, source_terrain_y,
						wind_world))
				return true;
			return false;
		}

		const float cell_size = static_cast<float>(MAP_BLOCKSIZE * cell_span) * BS;
		const v3f block_min =
				oposToV3f(intToFloat(block_pos * MAP_BLOCKSIZE - m_camera_offset, BS));
		const bool reverse_x =
				camera_scene_position.X <
				block_min.X + cell_size * static_cast<float>(split_count) * 0.5f;
		const bool reverse_y =
				camera_scene_position.Y <
				block_min.Y + cell_size * static_cast<float>(split_count) * 0.5f;
		const bool reverse_z =
				camera_scene_position.Z <
				block_min.Z + cell_size * static_cast<float>(split_count) * 0.5f;
		const auto ordered_index = [](bpos_t i, bpos_t count, bool reverse) -> bpos_t {
			return reverse ? count - 1 - i : i;
		};

		bool done = false;
		for (bpos_t ix = 0; ix < split_count && !done; ++ix) {
			const bpos_t sx = ordered_index(ix, split_count, reverse_x);
			for (bpos_t iz = 0; iz < split_count && !done; ++iz) {
				const bpos_t sz = ordered_index(iz, split_count, reverse_z);
				for (bpos_t iy = 0; iy < split_count; ++iy) {
					const bpos_t sy = ordered_index(iy, split_count, reverse_y);
					if (vertices->size() / 4 >= fog_quad_limit && !next_fog_buffer()) {
						done = true;
						break;
					}

					const v3bpos_t cell_pos{
							static_cast<bpos_t>(block_pos.X + sx * cell_span),
							static_cast<bpos_t>(block_pos.Y + sy * cell_span),
							static_cast<bpos_t>(block_pos.Z + sz * cell_span),
					};
					FarFogClimate cell_climate = climate;

					const v3f cell_min =
							block_min + v3f(static_cast<float>(sx) * cell_size,
												static_cast<float>(sy) * cell_size,
												static_cast<float>(sz) * cell_size);
					if (add_fog_cell_at(cell_min, cell_size, cell_pos, cell_step,
								cell_climate, source_terrain_y, wind_world)) {
						done = true;
						break;
					}
				}
			}
		}
		if (done)
			return true;
		return false;
	};

	struct DrawlistFogCell
	{
		v3bpos_t block_pos;
		block_step_t step = 0;
		bpos_t block_span = 0;
		float distance = 0.0f;
		MapBlockPtr block;
	};

	const auto farthest_first = [](const DrawlistFogCell &a, const DrawlistFogCell &b) {
		if (a.distance != b.distance)
			return a.distance > b.distance;
		if (a.step != b.step)
			return a.step > b.step;
		return a.block_pos > b.block_pos;
	};

	const auto draw_near_drawlist_blocks = [&]() -> bool {
		const bpos_t near_cell_span = 1 << m_control.cell_size_pow;
		std::unordered_map<v3bpos_t, MapBlockPtr, v3posHash, v3posEqual> near_cells;
		{
			std::lock_guard<std::recursive_mutex> lock(m_drawlist_mutex);
			const bool drawlist_current =
					m_drawlist_current.load(std::memory_order_acquire);
			const auto &drawlist = drawlist_current ? m_drawlist_1 : m_drawlist_0;
			near_cells.reserve(drawlist.size());
			for (const auto &[block_pos, block] : drawlist) {
				if (!block)
					continue;

				const auto fog_step = block->far_step_draw ?: block->far_step;
				if (fog_step)
					continue;

				const v3bpos_t cell_pos =
						far_fog_align_block_pos(block_pos, m_control.cell_size_pow);
				auto [it, inserted] = near_cells.emplace(cell_pos, block);
				if (!inserted &&
						far_fog_climate_missing(far_fog_climate_from_block(it->second)))
					it->second = block;
			}
		}

		std::vector<DrawlistFogCell> sorted_near_cells;
		sorted_near_cells.reserve(near_cells.size());
		for (const auto &[cell_pos, block] : near_cells) {
			sorted_near_cells.push_back(DrawlistFogCell{
					.block_pos = cell_pos,
					.step = 0,
					.block_span = near_cell_span,
					.distance = cell_distance(cell_pos, near_cell_span),
					.block = block,
			});
		}
		std::sort(sorted_near_cells.begin(), sorted_near_cells.end(), farthest_first);

		for (const auto &cell : sorted_near_cells) {
			FarFogClimate generated_climate;
			const FarFogClimate *generated_climate_ptr = nullptr;
			if (far_fog_climate_missing(far_fog_climate_from_block(cell.block))) {
				generated_climate = far_fog_generated_climate(
						m_client, cell.block_pos, cell.step, cell.block_span);
				generated_climate_ptr = &generated_climate;
			}
			if (draw_fog_source(cell.block_pos, cell.step, cell.block_span, cell.block,
						generated_climate_ptr,
						terrain_reference(cell.block_pos, cell.block_span), {}))
				return true;
		}
		return false;
	};

	const auto draw_far_fog_cells = [&](bool near_only) -> bool {
		if (!cells_ready)
			return false;

		std::lock_guard<std::mutex> lock(m_far_fog_cells_mutex);
		const auto &fog_cells = m_far_fog_cells[cells_current];
		for (const auto &cell : fog_cells) {
			if (cell.step >= FARMESH_STEP_MAX)
				continue;
			const bool near_cell = cell.distance <= near_fog_reserve_range;
			if (near_only != near_cell)
				continue;
			if (near_only)
				++protected_count;
			const FarFogClimate generated_climate{
					.heat = cell.heat,
					.humidity = cell.humidity,
			};
			if (draw_fog_source(cell.block_pos, cell.step, cell.block_span, cell.block,
						cell.has_climate ? &generated_climate : nullptr, cell.terrain_y,
						cell.wind)) {
				return true;
			}
		}
		return false;
	};

	const auto draw_far_drawlist_blocks = [&](bool near_only) -> bool {
		std::vector<DrawlistFogCell> sorted_far_cells;
		{
			std::lock_guard<std::recursive_mutex> lock(m_drawlist_mutex);
			const bool drawlist_current =
					m_drawlist_current.load(std::memory_order_acquire);
			const auto &drawlist = drawlist_current ? m_drawlist_1 : m_drawlist_0;
			sorted_far_cells.reserve(drawlist.size());
			for (const auto &[block_pos, block] : drawlist) {
				if (!block)
					continue;

				const auto fog_step = block->far_step_draw ?: block->far_step;
				if (fog_step >= FARMESH_STEP_MAX)
					continue;
				if (!fog_step)
					continue;

				const bpos_t block_span = 1 << (fog_step + m_control.cell_size_pow);
				sorted_far_cells.push_back(DrawlistFogCell{
						.block_pos = block_pos,
						.step = fog_step,
						.block_span = block_span,
						.distance = cell_distance(block_pos, block_span),
						.block = block,
				});
			}
		}
		std::sort(sorted_far_cells.begin(), sorted_far_cells.end(), farthest_first);

		for (const auto &cell : sorted_far_cells) {
			const bool near_cell = cell.distance <= near_fog_reserve_range;
			if (near_only != near_cell)
				continue;
			if (near_only)
				++protected_count;
			FarFogClimate generated_climate;
			const FarFogClimate *generated_climate_ptr = nullptr;
			if (far_fog_climate_missing(far_fog_climate_from_block(cell.block))) {
				generated_climate = far_fog_generated_climate(
						m_client, cell.block_pos, cell.step, cell.block_span);
				generated_climate_ptr = &generated_climate;
			}
			if (draw_fog_source(cell.block_pos, cell.step, cell.block_span, cell.block,
						generated_climate_ptr,
						terrain_reference(cell.block_pos, cell.block_span), {}))
				return true;
		}
		return false;
	};

	bool done = draw_far_fog_cells(false);
	if (!done && !cells_ready)
		done = draw_far_drawlist_blocks(false);
	const bool far_done = done;
	allow_near_fog_extra_buffers = true;
	const bool near_generated_done =
			cells_ready ? draw_far_fog_cells(true) : draw_far_drawlist_blocks(true);
	const bool near_drawlist_done = draw_near_drawlist_blocks();
	allow_near_fog_extra_buffers = false;
	done = far_done || near_generated_done || near_drawlist_done;

	const u32 vertex_count = far_fog_vertex_count();
	for (auto &fog_buffer : m_far_fog_meshbuffers) {
		if (!fog_buffer)
			continue;
		fog_buffer->Material = m_far_fog_material;
		fog_buffer->setDirty(scene::EBT_VERTEX);
		fog_buffer->setDirty(scene::EBT_INDEX);
		fog_buffer->recalculateBoundingBox();
	}
	m_far_fog_mesh_valid = true;
	m_far_fog_mesh_cells_ready = cells_ready;
	m_far_fog_mesh_cells_current = cells_current;
	m_far_fog_mesh_iteration_draw = cells_iteration;
	m_far_fog_mesh_camera_bucket = camera_bucket;
	m_far_fog_mesh_camera_offset = m_camera_offset;
	m_far_fog_mesh_camera_direction = forward;
	m_far_fog_mesh_time_bucket = time_bucket;

	g_profiler->avg("Client: Far fog candidates", candidate_count);
	g_profiler->avg("Client: Far fog stored", stored_count);
	g_profiler->avg("Client: Far fog generated", generated_count);
	g_profiler->avg("Client: Far fog low density", low_density_count);
	g_profiler->avg("Client: Far fog low alpha", low_alpha_count);
	g_profiler->avg("Client: Far fog daylight", daylight * 100.0f);
	g_profiler->avg("Client: Far fog moon brightness", moon_brightness * 100.0f);
	g_profiler->avg("Client: Far fog slices", slice_count);
	g_profiler->avg("Client: Far fog protected", protected_count);
	g_profiler->avg("Client: Far fog wind", wind_count ? wind_sum / wind_count : 0.0f);
	g_profiler->avg("Client: Far fog vertices", vertex_count);
	g_profiler->avg("Client: Far fog buffers", current_fog_buffer + 1);
	g_profiler->avg("Client: Far fog max buffers", max_fog_buffers);
	g_profiler->avg("Client: Far fog near extra buffers", max_near_fog_extra_buffers);
	return vertex_count;
}

u32 ClientMap::renderFarFog(video::IVideoDriver *driver)
{
	thread_local static const auto volumetric_fog_range_nodes =
			g_settings->getPos("volumetric_fog");
	if (volumetric_fog_range_nodes <= 0)
		return 0;

	const u32 vertex_count = rebuildFarFogMeshBuffer();
	if (!vertex_count)
		return 0;

	core::matrix4 identity;
	driver->setTransform(video::ETS_WORLD, identity);
	driver->setMaterial(m_far_fog_material);
	for (const auto &buffer : m_far_fog_meshbuffers) {
		if (buffer && buffer->getVertexCount())
			driver->drawMeshBuffer(buffer.get());
	}
	return vertex_count;
}
