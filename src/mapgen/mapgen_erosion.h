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

#include <vector>

#include "json/json.h"
#include "mapgen/mapgen_v7.h"

struct MapgenErosionParams : public MapgenV7Params
{
	MapgenErosionParams() = default;
	~MapgenErosionParams() = default;

	Json::Value params;

	void readParams(const Settings *settings) override;
	void writeParams(Settings *settings) const override;
	void setDefaultSettings(Settings *settings) override;
};

class MapgenErosion : public MapgenV7
{
public:
	MapgenErosionParams *mg_params = nullptr;

	MapgenType getType() const override { return MAPGEN_EROSION; }
	MapgenErosion(MapgenErosionParams *mg_params, EmergeParams *emerge);
	~MapgenErosion() = default;

	int generateTerrain() override;
	int getSpawnLevelAtPoint(v2pos_t p) override;
	int getGroundLevelAtPoint(v2pos_t p) override;
	bool visible(const v3pos_t &p) override;

private:
	struct Vec2f {
		float x = 0.0f;
		float y = 0.0f;
	};

	struct WaveSample {
		float cos_v = 1.0f;
		float sin_v = 0.0f;
	};

	int m_octaves = 5;
	float m_base_cell_size = 96.0f;
	float m_cell_lacunarity = 0.5f;
	float m_detail = 1.5f;
	float m_norm_k = 2.0f;
	float m_valley_alt = -48.0f;
	float m_peak_alt = 384.0f;
	float m_strength = 0.65f;
	float m_slope_scale = 0.12f;
	float m_jitter = 0.85f;
	float m_relief_scale = 1.75f;
	float m_continent_scale = 1.0f / 4200.0f;
	float m_warp_scale = 1.0f / 900.0f;
	float m_warp_strength = 220.0f;
	float m_detail_scale = 1.0f / 180.0f;
	float m_base_offset = -18.0f;
	float m_base_height = 96.0f;
	float m_mountain_height = 340.0f;
	float m_land_lift = 28.0f;
	float m_coast_blend = 72.0f;
	float m_mountain_boost = 0.5f;
	float m_mountain_threshold = 0.1f;

	float erosionHeightAtPoint(pos_t x, pos_t z);
	float baseHeightAtPoint(pos_t x, pos_t z) const;
	void applyErosionFilter(v3pos_t minp, v3pos_t maxp, std::vector<float> &heights);
	WaveSample sampleWave(const Vec2f &p, const Vec2f &grad, float cell_size, u32 octave_seed) const;
	float terrainBiasAtPoint(pos_t x, pos_t z, float base_height) const;
	float sampleFbm(float x, float z, float scale, s32 seed_off,
			int octaves, float persistence, float lacunarity) const;
	float sampleRidged(float x, float z, float scale, s32 seed_off,
			int octaves, float persistence, float lacunarity) const;

	static float clamp01(float v);
	static float lerp(float a, float b, float t);
	static float easeOut(float t);
	static float powInv(float t, float power);
	static float inverseLerp(float a, float b, float v);
	static float smoothstep5(float t);
	static float signNonZero(float v);
	static u32 hash2D(s32 x, s32 y, u32 seed);
	static float hashToUnitFloat(u32 h);
};
