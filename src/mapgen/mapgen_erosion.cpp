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

// https://blog.runevision.com/2026/03/fast-and-gorgeous-erosion-filter.html
// https://www.shadertoy.com/view/wXcfWn

#include "servermap.h"

#include <cmath>
#include <vector>

#include "mapgen_erosion.h"
#include "emerge.h"
#include "mapnode.h"
#include "noise.h"
#include "nodedef.h"
#include "serverenvironment.h"
#include "settings.h"
#include "voxel.h"

void MapgenErosionParams::readParams(const Settings *settings)
{
	MapgenV7Params::readParams(settings);
	settings->getFlagStrNoEx("mgerosion_spflags", spflags, flagdesc_mapgen_v7);

	auto erosion = settings->getJson("mg_erosion");
	if (!erosion.isNull())
		params = erosion;
}


void MapgenErosionParams::writeParams(Settings *settings) const
{
	settings->setJson("mg_erosion", params);
	MapgenV7Params::writeParams(settings);
	settings->setFlagStr("mgerosion_spflags", spflags, flagdesc_mapgen_v7);
}


void MapgenErosionParams::setDefaultSettings(Settings *settings)
{
	settings->setDefault("mgerosion_spflags", flagdesc_mapgen_v7, 0);
}


MapgenErosion::MapgenErosion(MapgenErosionParams *params, EmergeParams *emerge) :
	MapgenV7((MapgenV7Params *)params, emerge),
	mg_params(params)
{
	Json::Value &cfg = mg_params->params;
	m_octaves = rangelim(cfg.get("octaves", 5).asInt(), 1, 8);
	m_base_cell_size = rangelim(cfg.get("base_cell_size", 96.0).asFloat(), 8.0f, 1024.0f);
	m_cell_lacunarity = rangelim(cfg.get("cell_lacunarity", 0.5).asFloat(), 0.2f, 0.95f);
	m_detail = rangelim(cfg.get("detail", 1.5).asFloat(), 0.25f, 6.0f);
	m_norm_k = rangelim(cfg.get("normalization_k", 2.0).asFloat(), 1.0f, 8.0f);
	m_valley_alt = cfg.get("valley_alt", float(water_level - 48)).asFloat();
	m_peak_alt = cfg.get("peak_alt", float(water_level + 384)).asFloat();
	m_strength = rangelim(cfg.get("strength", 0.65).asFloat(), 0.0f, 2.0f);
	m_slope_scale = rangelim(cfg.get("slope_scale", 0.12).asFloat(), 0.001f, 2.0f);
	m_jitter = rangelim(cfg.get("jitter", 0.85).asFloat(), 0.0f, 1.0f);
	m_relief_scale = rangelim(cfg.get("relief_scale", 1.75).asFloat(), 0.5f, 4.0f);
	m_continent_scale = rangelim(cfg.get("continent_scale", 1.0 / 4200.0).asFloat(),
			1.0f / 20000.0f, 1.0f / 256.0f);
	m_warp_scale = rangelim(cfg.get("warp_scale", 1.0 / 900.0).asFloat(),
			1.0f / 8000.0f, 1.0f / 64.0f);
	m_warp_strength = rangelim(cfg.get("warp_strength", 220.0).asFloat(), 0.0f, 2048.0f);
	m_detail_scale = rangelim(cfg.get("detail_scale", 1.0 / 180.0).asFloat(),
			1.0f / 4000.0f, 1.0f / 16.0f);
	m_base_offset = rangelim(cfg.get("base_offset", -18.0).asFloat(), -256.0f, 256.0f);
	m_base_height = rangelim(cfg.get("base_height", 96.0).asFloat(), 0.0f, 1024.0f);
	m_mountain_height = rangelim(cfg.get("mountain_height", 340.0).asFloat(), 0.0f, 2048.0f);
	m_land_lift = rangelim(cfg.get("land_lift", 28.0).asFloat(), 0.0f, 128.0f);
	m_coast_blend = rangelim(cfg.get("coast_blend", 72.0).asFloat(), 8.0f, 256.0f);
	m_mountain_boost = rangelim(cfg.get("mountain_boost", 0.5).asFloat(), 0.0f, 2.0f);
	m_mountain_threshold = rangelim(cfg.get("mountain_threshold", 0.1).asFloat(), -1.0f, 1.0f);
}


float MapgenErosion::clamp01(float v)
{
	return rangelim(v, 0.0f, 1.0f);
}


float MapgenErosion::lerp(float a, float b, float t)
{
	return a + (b - a) * t;
}


float MapgenErosion::easeOut(float t)
{
	float v = 1.0f - clamp01(t);
	return 1.0f - v * v;
}


float MapgenErosion::powInv(float t, float power)
{
	return 1.0f - std::pow(1.0f - clamp01(t), power);
}


float MapgenErosion::inverseLerp(float a, float b, float v)
{
	if (a == b)
		return 0.5f;
	return clamp01((v - a) / (b - a));
}


float MapgenErosion::smoothstep5(float t)
{
	t = clamp01(t);
	return t * t * t * (t * (t * 6.0f - 15.0f) + 10.0f);
}


float MapgenErosion::signNonZero(float v)
{
	return v < 0.0f ? -1.0f : 1.0f;
}


float MapgenErosion::sampleFbm(float x, float z, float scale, s32 seed_off,
		int octaves, float persistence, float lacunarity) const
{
	float amplitude = 1.0f;
	float frequency = scale;
	float total = 0.0f;
	float denom = 0.0f;

	for (int i = 0; i < octaves; ++i) {
		total += noise2d_value(x * frequency, z * frequency, seed + seed_off + i, true) *
				amplitude;
		denom += amplitude;
		amplitude *= persistence;
		frequency *= lacunarity;
	}

	return denom > 0.0f ? total / denom : 0.0f;
}


float MapgenErosion::sampleRidged(float x, float z, float scale, s32 seed_off,
		int octaves, float persistence, float lacunarity) const
{
	float amplitude = 1.0f;
	float frequency = scale;
	float total = 0.0f;
	float denom = 0.0f;

	for (int i = 0; i < octaves; ++i) {
		float n = noise2d_value(x * frequency, z * frequency, seed + seed_off + i, true);
		float ridge = 1.0f - std::fabs(n);
		ridge *= ridge;
		total += ridge * amplitude;
		denom += amplitude;
		amplitude *= persistence;
		frequency *= lacunarity;
	}

	return denom > 0.0f ? total / denom : 0.0f;
}


float MapgenErosion::baseHeightAtPoint(pos_t x, pos_t z) const
{
	float warp_x = sampleFbm(x, z, m_warp_scale, 100, 3, 0.5f, 2.0f);
	float warp_z = sampleFbm(x, z, m_warp_scale, 200, 3, 0.5f, 2.0f);
	float px = x + warp_x * m_warp_strength;
	float pz = z + warp_z * m_warp_strength;

	float continent = sampleFbm(px, pz, m_continent_scale, 300, 5, 0.5f, 2.0f);
	continent = smoothstep5(clamp01(continent * 0.5f + 0.5f));

	float macro = sampleFbm(px, pz, m_continent_scale * 3.0f, 400, 4, 0.55f, 2.1f);
	float hills = sampleFbm(px, pz, m_detail_scale * 0.35f, 500, 5, 0.52f, 2.0f);
	float detail = sampleFbm(px, pz, m_detail_scale, 600, 4, 0.5f, 2.0f);
	float ridged = sampleRidged(px, pz, m_detail_scale * 0.22f, 700, 5, 0.55f, 2.0f);

	float land_mask = smoothstep5(clamp01((continent - 0.38f) / 0.4f));
	float basin_mask = smoothstep5(clamp01((0.55f - continent) / 0.55f));
	float continental_height = lerp(-0.9f, 1.0f, continent) * m_base_height;
	float macro_height = (macro * 42.0f + hills * 30.0f + detail * 12.0f) *
			lerp(0.45f, 1.0f, land_mask);
	float mountain_mask = smoothstep5(clamp01((continent - 0.42f) / 0.48f));
	float mountain_height = std::pow(ridged, 1.35f) * mountain_mask * m_mountain_height;
	float basin_depth = basin_mask * basin_mask * (m_base_height * 0.55f);

	return water_level + m_base_offset + continental_height +
			macro_height + mountain_height - basin_depth;
}


float MapgenErosion::terrainBiasAtPoint(pos_t x, pos_t z, float base_height) const
{
	float coast_t = 1.0f - inverseLerp(
			water_level - m_coast_blend, water_level + m_coast_blend * 0.5f, base_height);
	float coast_lift = smoothstep5(coast_t) * m_land_lift;

	float ridged = sampleRidged((float)x, (float)z, m_detail_scale * 0.18f, 800, 4, 0.55f, 2.0f);
	float mountain_mask = smoothstep5(clamp01((ridged - m_mountain_threshold) /
			std::max(1.0f - m_mountain_threshold, 0.001f)));
	float mountain_lift = mountain_mask * ridged * m_mountain_height * m_mountain_boost;

	return coast_lift + mountain_lift;
}


u32 MapgenErosion::hash2D(s32 x, s32 y, u32 seed)
{
	u32 h = seed;
	h ^= 0x9e3779b9u + (u32)x + (h << 6) + (h >> 2);
	h ^= 0x85ebca6bu + (u32)y + (h << 6) + (h >> 2);
	h ^= h >> 16;
	h *= 0x7feb352du;
	h ^= h >> 15;
	h *= 0x846ca68bu;
	h ^= h >> 16;
	return h;
}


float MapgenErosion::hashToUnitFloat(u32 h)
{
	return (h & 0x00ffffffu) / float(0x01000000u);
}


MapgenErosion::WaveSample MapgenErosion::sampleWave(
	const Vec2f &p, const Vec2f &grad, float cell_size, u32 octave_seed) const
{
	Vec2f flow = grad;
	float grad_len = std::sqrt(flow.x * flow.x + flow.y * flow.y);
	if (grad_len > 0.0001f) {
		flow.x /= grad_len;
		flow.y /= grad_len;
	} else {
		flow.x = 1.0f;
		flow.y = 0.0f;
	}

	Vec2f ortho{-flow.y, flow.x};
	float gx = std::floor(p.x / cell_size);
	float gy = std::floor(p.y / cell_size);
	float fx = p.x / cell_size - gx;
	float fy = p.y / cell_size - gy;
	float wx1 = smoothstep5(fx);
	float wy1 = smoothstep5(fy);
	float wx0 = 1.0f - wx1;
	float wy0 = 1.0f - wy1;

	float stripe_scale = (2.0f * M_PI) / cell_size;
	float sum_cos = 0.0f;
	float sum_sin = 0.0f;

	for (int dz = 0; dz <= 1; ++dz) {
		for (int dx = 0; dx <= 1; ++dx) {
			float weight = (dx ? wx1 : wx0) * (dz ? wy1 : wy0);
			s32 cx = (s32)gx + dx;
			s32 cz = (s32)gy + dz;
			u32 h = hash2D(cx, cz, octave_seed);
			float jx = (hashToUnitFloat(h) - 0.5f) * m_jitter;
			float jz = (hashToUnitFloat(h ^ 0x68bc21ebu) - 0.5f) * m_jitter;
			Vec2f pivot{
				(cx + 0.5f + jx) * cell_size,
				(cz + 0.5f + jz) * cell_size
			};
			float phase = ((p.x - pivot.x) * ortho.x + (p.y - pivot.y) * ortho.y) * stripe_scale;
			sum_cos += std::cos(phase) * weight;
			sum_sin += std::sin(phase) * weight;
		}
	}

	float len = std::sqrt(sum_cos * sum_cos + sum_sin * sum_sin);
	if (len > 0.0001f) {
		float scale = std::min(len * m_norm_k, 1.0f) / len;
		sum_cos *= scale;
		sum_sin *= scale;
	}

	// Use the sign of the sine as the internal slope direction so smaller
	// features branch away more decisively instead of tracking along ridges.
	return {sum_cos, signNonZero(sum_sin) * std::fabs(sum_sin)};
}


void MapgenErosion::applyErosionFilter(v3pos_t minp, v3pos_t maxp, std::vector<float> &heights)
{
	const s32 w = maxp.X - minp.X + 1;
	const s32 d = maxp.Z - minp.Z + 1;
	const auto idx = [w](s32 x, s32 z) -> size_t {
		return (size_t)z * w + x;
	};

	std::vector<float> base_values = heights;
	std::vector<float> values(heights.size(), 0.0f);
	std::vector<float> combi_mask(w * d, 1.0f);
	std::vector<float> next_values(w * d, 0.0f);

	for (size_t i = 0; i < base_values.size(); ++i) {
		float target = inverseLerp(m_valley_alt, m_peak_alt, base_values[i]) * 2.0f - 1.0f;
		values[i] = target;
		combi_mask[i] = 1.0f - easeOut(clamp01(std::fabs(target) * 0.35f));
	}

	float cell_size = m_base_cell_size;
	for (int octave = 0; octave < m_octaves; ++octave) {
		u32 octave_seed = (u32)seed + (u32)octave * 0x9e3779b9u;
		float octave_weight = m_strength / std::pow(1.7f, (float)octave);

		for (s32 z = 0; z < d; ++z) {
			s32 zm = std::max<s32>(z - 1, 0);
			s32 zp = std::min<s32>(z + 1, d - 1);
			for (s32 x = 0; x < w; ++x) {
				s32 xm = std::max<s32>(x - 1, 0);
				s32 xp = std::min<s32>(x + 1, w - 1);

				float dx = values[idx(xp, z)] - values[idx(xm, z)];
				float dz = values[idx(x, zp)] - values[idx(x, zm)];
				float fade_target = inverseLerp(m_valley_alt, m_peak_alt, values[idx(x, z)]) * 2.0f - 1.0f;

				Vec2f p{float(minp.X + x), float(minp.Z + z)};
				Vec2f grad{dx, dz};
				WaveSample sample = sampleWave(p, grad, cell_size, octave_seed);
				float slope = std::sqrt(dx * dx + dz * dz) * 0.5f;
				float terrain_mask = easeOut(clamp01(slope * m_slope_scale));
				float new_mask = terrain_mask * easeOut(std::fabs(sample.sin_v));
				float erosion_mask = clamp01(combi_mask[idx(x, z)] * terrain_mask);

				float gully = lerp(fade_target, sample.cos_v, combi_mask[idx(x, z)]);
				combi_mask[idx(x, z)] = powInv(combi_mask[idx(x, z)], m_detail) * new_mask;
				next_values[idx(x, z)] =
					lerp(values[idx(x, z)], gully, erosion_mask * octave_weight);
			}
		}

		values.swap(next_values);
		cell_size = std::max(cell_size * m_cell_lacunarity, 4.0f);
	}

	for (size_t i = 0; i < heights.size(); ++i) {
		s32 x = (s32)(i % w);
		s32 z = (s32)(i / w);
		float t = (clamp01(values[i] * 0.5f + 0.5f));
		float eroded_height = lerp(m_valley_alt, m_peak_alt, t);
		float mid = 0.5f * (m_valley_alt + m_peak_alt);
		heights[i] = mid + (eroded_height - mid) * m_relief_scale;
		heights[i] += terrainBiasAtPoint(minp.X + x, minp.Z + z, base_values[i]);
		heights[i] = std::max(heights[i], base_values[i] - 12.0f);
	}
}


float MapgenErosion::erosionHeightAtPoint(pos_t x, pos_t z)
{
	v3pos_t minp(x - 1, 0, z - 1);
	v3pos_t maxp(x + 1, 0, z + 1);
	std::vector<float> heights(9);
	size_t i = 0;
	for (pos_t zz = minp.Z; zz <= maxp.Z; ++zz)
		for (pos_t xx = minp.X; xx <= maxp.X; ++xx)
			heights[i++] = baseHeightAtPoint(xx, zz);

	applyErosionFilter(minp, maxp, heights);
	return heights[4];
}


int MapgenErosion::getGroundLevelAtPoint(v2pos_t p)
{
	return myround(erosionHeightAtPoint(p.X, p.Y));
}


int MapgenErosion::getSpawnLevelAtPoint(v2pos_t p)
{
	pos_t y = getGroundLevelAtPoint(p);
	if (y < water_level || y > water_level + 256)
		return MAX_MAP_GENERATION_LIMIT;
	return y + 2;
}


bool MapgenErosion::visible(const v3pos_t &p)
{
	return getGroundLevelAtPoint({p.X, p.Z}) >= p.Y;
}


int MapgenErosion::generateTerrain()
{
	MapNode n_air(CONTENT_AIR);
	MapNode n_stone(c_stone);
	MapNode n_water(c_water_source);
	MapNode n_ice(c_ice);

	const s32 w = node_max.X - node_min.X + 1;
	const s32 d = node_max.Z - node_min.Z + 1;
	std::vector<float> heights((size_t)w * d);

	size_t index2d = 0;
	for (pos_t z = node_min.Z; z <= node_max.Z; ++z)
		for (pos_t x = node_min.X; x <= node_max.X; ++x)
			heights[index2d++] = baseHeightAtPoint(x, z);

	applyErosionFilter(node_min, node_max, heights);

	const v3s32 &em = vm->m_area.getExtent();
	pos_t stone_surface_max_y = -MAX_MAP_GENERATION_LIMIT;
	index2d = 0;

	for (pos_t z = node_min.Z; z <= node_max.Z; ++z) {
		for (pos_t x = node_min.X; x <= node_max.X; ++x, ++index2d) {
			pos_t surface_y = myround(heights[index2d]);
			stone_surface_max_y = std::max(stone_surface_max_y, surface_y);

			s16 heat = m_emerge->env->m_use_weather ?
				m_emerge->env->getServerMap().updateBlockHeat(
					m_emerge->env, v3pos_t(x, node_max.Y, z), nullptr, &heat_cache) : 0;

			u32 vi = vm->m_area.index(x, node_min.Y - 1, z);
			u32 index3d = (z - node_min.Z) * zstride_1u1d + (x - node_min.X);

			for (pos_t y = node_min.Y - 1; y <= node_max.Y + 1;
					y++, index3d += ystride, VoxelArea::add_y(em, vi, 1)) {
				if (vm->m_data[vi].getContent() != CONTENT_IGNORE)
					continue;

				if (y <= surface_y) {
					if (cave_noise_threshold &&
							noise_cave_indev->result[index3d] > cave_noise_threshold) {
						vm->m_data[vi] = n_air;
					} else {
						MapNode n = layers_get(index3d);
						bool protect = n.getContent() != CONTENT_AIR;
						if (cave_noise_threshold &&
								noise_cave_indev->result[index3d] > cave_noise_threshold - 50) {
							vm->m_data[vi] = protect ? n_stone : n;
							protect = true;
						} else {
							vm->m_data[vi] = n;
						}
						if (protect)
							vm->m_flags[vi] |= VOXELFLAG_CHECKED2;
					}
				} else if (y <= water_level) {
					vm->m_data[vi] = (heat < 0 && y > heat / 3) ? n_ice : n_water;
					if (liquid_pressure && y <= 0)
						vm->m_data[vi].addLevel(m_emerge->ndef, water_level - y, 1);
				} else {
					vm->m_data[vi] = n_air;
				}
			}
		}
	}

	return stone_surface_max_y;
}
