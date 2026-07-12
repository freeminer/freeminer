/*
This file is part of Freeminer.

Freeminer is free software: you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation, either version 3 of the License, or
(at your option) any later version.

Freeminer is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with Freeminer. If not, see <http://www.gnu.org/licenses/>.
*/

#pragma once

#include <memory>
#include <string>
#include <vector>

#include "mapgen/mapgen_v7.h"

struct MapgenTerrainDiffusionParams : public MapgenV7Params
{
	MapgenTerrainDiffusionParams() = default;
	~MapgenTerrainDiffusionParams() = default;

	std::string height_model;
	std::string native_model_dir;
	s16 native_node_scale = 30;
	float native_height_scale = 1.0f;
	float native_height_offset = 0.0f;
	float native_residual_std = 0.7f;
	u16 native_cache_tiles = 8;
	u16 native_cache_mb = 128;
	std::string native_provider = "auto";
	s16 native_device_id = 0;
	s16 native_intra_threads = 8;
	std::string native_conditioning_stats;
	bool native_prefetch = false;
	float model_node_scale = 1.0f;
	float model_height_scale = 1.0f;
	float model_height_offset = 0.0f;
	std::string api_url;
	s16 api_scale = 1;
	float api_height_scale = 1.0f;
	float api_height_offset = 0.0f;
	s32 api_timeout_ms = 30000;
	bool api_send_seed = true;
	float fallback_height_scale = 160.0f;
	float fallback_detail_scale = 1.0f;

	void readParams(const Settings *settings) override;
	void writeParams(Settings *settings) const override;
	void setDefaultSettings(Settings *settings) override;
};

class TerrainDiffusionOnnxModel;
class TerrainDiffusionNativePipeline;

struct TerrainDiffusionSample
{
	float height = 0.0f;
	weather::heat_t heat = 20;
	weather::humidity_t humidity = 50;
	bool has_climate = false;
};

class MapgenTerrainDiffusion : public MapgenV7
{
public:
	MapgenTerrainDiffusionParams *mg_params = nullptr;

	MapgenType getType() const override { return MAPGEN_TERRAIN_DIFFUSION; }
	MapgenTerrainDiffusion(MapgenTerrainDiffusionParams *mg_params, EmergeParams *emerge);
	~MapgenTerrainDiffusion() override;

	int generateTerrain() override;
	int getSpawnLevelAtPoint(v2pos_t p) override;
	int getGroundLevelAtPoint(v2pos_t p) override;
	weather::heat_t calcBlockHeat(const v3pos_t &p, uint64_t seed, float timeofday,
			float totaltime, bool use_weather) override;
	weather::humidity_t calcBlockHumidity(const v3pos_t &p, uint64_t seed,
			float timeofday, float totaltime, bool use_weather) override;

	static weather::humidity_t precipToHumidity(float precipitation);

private:
	std::unique_ptr<TerrainDiffusionOnnxModel> m_model;
	std::unique_ptr<TerrainDiffusionNativePipeline> m_native;
	bool m_warned_no_model = false;
	bool m_warned_api_failed = false;

	bool sampleApiGrid(
			v3pos_t minp, v3pos_t maxp, std::vector<TerrainDiffusionSample> &samples);
	bool sampleOnnxGrid(
			v3pos_t minp, v3pos_t maxp, std::vector<TerrainDiffusionSample> &samples);
	TerrainDiffusionSample samplePoint(pos_t x, pos_t z);
	TerrainDiffusionSample fallbackSample(pos_t x, pos_t z) const;
	float fallbackHeightAtPoint(pos_t x, pos_t z) const;
	float sampleFbm(float x, float z, float scale, s32 seed_off, int octaves,
			float persistence, float lacunarity) const;
	float sampleRidged(float x, float z, float scale, s32 seed_off, int octaves,
			float persistence, float lacunarity) const;

	static float clamp01(float v);
	static float smoothstep(float v);
};
