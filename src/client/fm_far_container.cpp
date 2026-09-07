/*
Copyright (C) 2024 proller <proler@gmail.com>
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

#include <algorithm>
#include <array>
#include <optional>

#include "fm_far_container.h"
#include "fm_far_sample_cache.h"
#include "client.h"
#include "client/clientmap.h"
#include "constants.h"
#include "database/database.h"
#include "fm_far_calc.h"
#include "irr_v3d.h"
#include "irrlichttypes.h"
#include "mapblock.h"
#include "mapgen/mapgen.h"
#include "mapnode.h"
#include "server.h"
#include "settings.h"

struct FarContainer::Cache
{
	using Sample = std::pair<MapNode, bool>;
	struct Source
	{
		bool generated{};
		std::vector<MapNode> nodes;
	};
	farmesh::GridSampleCache<Sample> nodes;
	std::unordered_map<v2pos_t, pos_t> heights;
	std::array<std::unordered_map<v3bpos_t, Source>, FARMESH_STEP_MAX> sources;
	std::array<std::unordered_map<v3bpos_t, bool>, FARMESH_STEP_MAX> columns;
	v3bpos_t player;
	uint8_t cell_pow;
	int range;
	uint8_t quality_pow;

	Cache(Client *client, const v3pos_t &origin, pos_t side, block_step_t step) :
			nodes(origin, side, step)
	{
		const auto &map = client->getEnv().getClientMap();
		const auto &control = map.getControl();
		player = getNodeBlockPos(map.far_cam_pos_mesh);
		cell_pow = control.cell_size_pow;
		range = control.farmesh;
		quality_pow = control.farmesh_quality_pow;
	}

	auto params(const v3bpos_t &pos, bool storage_cell) const
	{
		return farmesh::getFarParams(
				player, cell_pow, range, quality_pow, pos, storage_cell);
	}
};

FarContainer::FarContainer(Client *client) :
		m_client{client},
		m_surface_depth{std::clamp(g_settings->getS32("farmesh_surface_depth"), -1, 16)}
{
}

FarContainer::FarContainer(const FarContainer &source, const v3pos_t &origin, pos_t side,
		block_step_t step) :
		m_client(source.m_client), m_surface_depth(source.m_surface_depth),
		m_cache(std::make_unique<Cache>(source.m_client, origin, side, step)),
		m_mg(source.m_mg), use_weather(source.use_weather),
		have_params(source.have_params)
{
}

FarContainer::~FarContainer() = default;

std::pair<const MapNode, bool> FarContainer::getNodeRefAndVisible(const v3pos_t &pos, block_step_t step)
{
	if (!m_cache) {
		// The client-wide container holds settings. Direct queries also get an
		// isolated sampler instead of leaving stale thread-local blocks behind.
		FarContainer sampler(*this, pos, 1, 0);
		return sampler.getNodeRefAndVisible(pos, step);
	}
	return m_cache->nodes.get(pos, [&]() -> Cache::Sample { return sample(pos, step); });
}

std::pair<const MapNode, bool> FarContainer::sample(const v3pos_t &pos, block_step_t step)
{
	const auto block_pos = getNodeBlockPos(pos);
	auto &client_map = m_client->getEnv().getClientMap();
	const auto tree_result = m_cache->params(block_pos, true);
	if (tree_result) {
		const auto &step = tree_result->step;
		const v3bpos_t &bpos_aligned = tree_result->pos;
		if (step >= FARMESH_STEP_MAX)
			return {m_mg->visible_transparent, false};
		auto [source_it, inserted] = m_cache->sources[step].try_emplace(bpos_aligned);
		auto &source = source_it->second;
		if (inserted) {
			MapBlockPtr block;
			{
				const auto &storage = client_map.far_blocks_storage[step];
				const auto lock = storage.lock_shared_rec();
				if (auto it = storage.find(bpos_aligned); it != storage.end())
					block = it->second.block;
			}

			if (!block &&
					!m_client->m_simple_singleplayer_mode
					// TODO: remove and fix
					&& !have_params
					// ====
			) {
				const auto loadBlock = [this, &client_map](const auto &bpos,
											   const auto step) -> MapBlockPtr {
					auto *dbase = GetFarDatabase(
							{}, m_client->far_dbases, m_client->m_world_path, step);
					if (!dbase) {
						return {};
					}
					MapBlockPtr block = client_map.createBlankBlockNoInsert(bpos);

					std::string blob;
					dbase->loadBlock(bpos, &blob);
					if (!blob.length()) {
						return {};
					}

					std::istringstream is(blob, std::ios_base::binary);

					u8 version = SER_FMT_VER_INVALID;
					is.read((char *)&version, 1);

					if (is.fail()) {
						return {};
					}

					// Read basic data
					if (!block->deSerialize(is, version, true)) {
						return {};
					}
					return block;
				};

				block = loadBlock(bpos_aligned, step);
			}
			if (block) {
				// Copy while locked: received data can replace/reallocate the source
				// during this job. Face and neighbour passes must see the same nodes.
				const auto lock = block->lock_shared_rec();
				source.generated = block->isGenerated();
				if (source.generated) {
					source.nodes.reserve(MAP_BLOCKSIZE * MAP_BLOCKSIZE * MAP_BLOCKSIZE);
					for (pos_t z = 0; z < MAP_BLOCKSIZE; ++z)
						for (pos_t y = 0; y < MAP_BLOCKSIZE; ++y)
							for (pos_t x = 0; x < MAP_BLOCKSIZE; ++x)
								source.nodes.push_back(
										block->getNodeNoLock(v3pos_t(x, y, z)));
				}
			}
		}
		if (source.generated) {
			const v3pos_t rel = pos - bpos_aligned * MAP_BLOCKSIZE;
			const auto x = std::clamp<int>(rel.X >> step, 0, MAP_BLOCKSIZE - 1);
			const auto y = std::clamp<int>(rel.Y >> step, 0, MAP_BLOCKSIZE - 1);
			const auto z = std::clamp<int>(rel.Z >> step, 0, MAP_BLOCKSIZE - 1);
			const auto n = source.nodes[(z * MAP_BLOCKSIZE + y) * MAP_BLOCKSIZE + x];
			// Explicit AIR and material cells own their volume. Only unavailable
			// samples are allowed to fall back to calculated terrain.
			if (n.getContent() != CONTENT_IGNORE && n.getContent() != CONTENT_UNKNOWN)
				return {n, false};
		}

		// Calculated terrain is owned by the missing-data path. Defer every
		// synthesized node until after the far-block lookup so it can never be
		// composited with an available world-merge block.
		std::optional<MapNode> mapgen_fallback;
		bool underground_occluder_candidate = false;
		// ===

		if (m_mg->surface_2d()) {
			auto [height_it, height_inserted] =
					m_cache->heights.try_emplace(v2pos_t(pos.X, pos.Z));
			if (height_inserted)
				height_it->second = m_mg->getGroundLevelAtPointStep(height_it->first, step);
			const auto surface_y = height_it->second;
			// Only samples strictly below the calculated surface may stand in as
			// invisible occluders for omitted world-merge blocks. Surface and water
			// samples still need visible mapgen fallback when their merge block is absent.
			underground_occluder_candidate = pos.Y < surface_y;
			// ===

			// Far-mesh sea fallback is visual only. Retain calculated water between
			// below-sea-level terrain and water_level when no merged block is available.
			if (pos.Y > surface_y && m_mg->visible_water_level(pos)) {
				const auto fill = m_mg->visible_content(pos, use_weather, step);
				const auto content = fill.getContent();
				if (content != CONTENT_IGNORE && content != CONTENT_UNKNOWN &&
						content != CONTENT_AIR) {
					mapgen_fallback = fill;
				}
			}

			if (m_surface_depth >= 0) {
				const auto cell_size = static_cast<pos_t>(1) << step;
				const auto height_above_surface = pos.Y - surface_y;
				if (!mapgen_fallback && height_above_surface >= 0 &&
						height_above_surface < cell_size) {
					// Far cells end at the sampled Y and expand downward. The cell that
					// contains the terrain surface therefore normally has its sample just
					// above the exact height. Retain its calculated weather-dependent
					// surface material for the missing-block fallback.
					const auto fill = m_mg->visible_content(
							v3pos_t(pos.X, surface_y, pos.Z), use_weather, step);
					const auto content = fill.getContent();
					if (content != CONTENT_IGNORE && content != CONTENT_UNKNOWN &&
							content != CONTENT_AIR) {
						// Defer synthesized terrain until stored surface data was tried.
						mapgen_fallback = fill;
						// ===
					}
				}
				const auto preserved_depth =
						static_cast<pos_t>(m_surface_depth) * cell_size;
				// At depth zero the surface sample also satisfies this boundary.
				// Do not let the deep-fill optimization bypass stored road data there.
				const bool fill_deep =
						!mapgen_fallback && pos.Y <= surface_y - preserved_depth;
				// ===
				if (fill_deep) {
					const auto fill = m_mg->visible_surface;
					const auto content = fill.getContent();
					if (content != CONTENT_IGNORE && content != CONTENT_UNKNOWN &&
							content != CONTENT_AIR) {
						// Filling deep missing terrain suppresses cave and fragment
						// faces without repeatedly evaluating the mapgen material.
						mapgen_fallback = fill;
					}
				}
			}
		}

		// World merge deliberately omits completely empty blocks. If another
		// generated block exists in this X/Z mesh column, the column is nevertheless
		// owned by world merge. Return IGNORE as an invisible occluder for its absent
		// underground cells: the far mesher does not draw IGNORE itself, but it uses
		// it to suppress the artificial black sides of adjacent solid cells.
		if (underground_occluder_candidate && step < FARMESH_STEP_MAX) {
			const auto mesh_result = m_cache->params(block_pos, false);
			if (mesh_result && mesh_result->step == step) {
				const v3bpos_t column_key(
						bpos_aligned.X, mesh_result->pos.Y, bpos_aligned.Z);
				auto [it, inserted] =
						m_cache->columns[step].try_emplace(column_key, false);
				if (inserted) {
					const auto &storage = client_map.far_blocks_storage[step];
					const auto storage_lock = storage.lock_shared_rec();
					const bpos_t step_width = static_cast<bpos_t>(1) << step;
					const bpos_t count = static_cast<bpos_t>(1) << m_cache->cell_pow;
					for (bpos_t y = 0; y < count; ++y) {
						const v3bpos_t p(column_key.X, column_key.Y + y * step_width,
								column_key.Z);
						const auto block_it = storage.find(p);
						if (block_it != storage.end() && block_it->second.block &&
								block_it->second.block->isGenerated()) {
							it->second = true;
							break;
						}
					}
				}
				if (it->second)
					return {MapNode(CONTENT_IGNORE), false};
			}
		}
		// ===

		// No generated world-merge block was available; only now may mapgen
		// supply the retained fallback node.
		if (mapgen_fallback)
			return {*mapgen_fallback, false};
		// ===
	}

	if (const auto v = m_mg->visible_content(pos, use_weather, step);
			v.getContent() != CONTENT_IGNORE && v.getContent() != CONTENT_UNKNOWN) {
		const auto visible = m_mg->surface_2d() && v.getContent() != CONTENT_AIR;
		return {v, visible};
	}

	return {m_mg->visible_transparent, false};
};
