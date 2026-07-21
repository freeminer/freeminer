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

#pragma once

#include "content_abm_core.h"
#include "fm_weather.h"

#include <limits>
#include <mutex>
#include <optional>
#include <unordered_map>

namespace core_abm
{

struct TerrainKey
{
	int x;
	int z;
	int min_y;
	int max_y;

	bool operator==(const TerrainKey &) const = default;
};

struct TerrainKeyHash
{
	size_t operator()(const TerrainKey &key) const;
};

class PrecipitationABM : public ConfigurableABM
{
protected:
	const int m_cloud_height;

	std::mutex m_terrain_cache_mutex;
	u32 m_terrain_cache_period = std::numeric_limits<u32>::max();
	std::unordered_map<TerrainKey, std::optional<pos_t>, TerrainKeyHash>
			m_terrain_cache;

	explicit PrecipitationABM(const CoreABMDefinition &definition);

	weather::wind_t get_wind(
			ServerEnvironment *env, ServerMap *map, const v3pos_t &p) const;
	float precipitation_factor(ServerEnvironment *env, ServerMap *map,
			const NodeDefManager *ndef, const v3pos_t &p, float humidity);

private:
	static s32 noise_seed(u64 map_seed, u32 seed_difference);
	float cloud_value(ServerEnvironment *env, ServerMap *map, const v3pos_t &p) const;
	std::optional<pos_t> highest_solid_y(ServerEnvironment *env, ServerMap *map,
			const NodeDefManager *ndef, float x, float z, int min_y, int max_y);
	float rain_shadow(ServerEnvironment *env, ServerMap *map,
			const NodeDefManager *ndef, const v3pos_t &p);
};

}
