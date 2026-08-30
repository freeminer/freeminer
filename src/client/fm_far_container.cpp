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

#include "fm_far_container.h"
#include "client.h"
#include "client/clientmap.h"
#include "constants.h"
#include "database/database.h"
#include "fm_far_calc.h"
#include "irr_v3d.h"
#include "mapblock.h"
#include "mapgen/mapgen.h"
#include "mapnode.h"
#include "server.h"
#include "settings.h"

FarContainer::FarContainer(Client *client) :
		m_client{client},
		m_surface_depth{std::clamp(g_settings->getS32("farmesh_surface_depth"), -1, 16)}
{
}

namespace
{
thread_local MapBlockPtr block_cache{};
thread_local std::pair<block_step_t, v3bpos_t> block_cache_p;

struct SurfaceHeightCacheEntry
{
	const FarContainer *owner{};
	const Mapgen *mapgen{};
	v2pos_t pos;
	pos_t height{};
	bool valid{};
};

pos_t get_surface_height_cached(
		const FarContainer *owner, Mapgen *mapgen, const v2pos_t &pos)
{
	// Far mesh generation repeatedly asks for all Y values and six neighbours
	// in the same few columns. A small thread-local direct cache avoids running
	// the mapgen height function for every face.
	thread_local std::array<SurfaceHeightCacheEntry, 64> cache;
	const auto x = static_cast<uint32_t>(pos.X);
	const auto z = static_cast<uint32_t>(pos.Y);
	const size_t index = (x * 73856093u ^ z * 19349663u) & (cache.size() - 1);
	auto &entry = cache[index];
	if (!entry.valid || entry.owner != owner || entry.mapgen != mapgen ||
			entry.pos != pos) {
		entry.owner = owner;
		entry.mapgen = mapgen;
		entry.pos = pos;
		entry.height = mapgen->getGroundLevelAtPoint(pos);
		entry.valid = true;
	}
	return entry.height;
}
}

std::pair<const MapNode, bool> FarContainer::getNodeRefAndVisible(const v3pos_t &pos)
{
	const auto block_pos = getNodeBlockPos(pos);
	auto &client_map = m_client->getEnv().getClientMap();
	const auto player_block_pos = getNodeBlockPos(client_map.far_cam_pos_mesh);
	const auto &control = client_map.getControl();

	const auto tree_result =
			farmesh::getFarParams(control, player_block_pos, block_pos, true);
	if (tree_result) {
		const auto &step = tree_result->step;
		const v3bpos_t &bpos_aligned = tree_result->pos;

		if (m_mg->surface_2d()) {
			const auto surface_y =
					get_surface_height_cached(this, m_mg, v2pos_t(pos.X, pos.Z));

			// Far-mesh sea is visual only. Prefer the mapgen's calculated water
			// throughout the space between below-sea-level terrain and water_level,
			// even when the stored/generated block correctly contains air there.
			if (pos.Y > surface_y && m_mg->visible_water_level(pos)) {
				const auto fill = m_mg->visible_content(pos, use_weather);
				const auto content = fill.getContent();
				if (content != CONTENT_IGNORE && content != CONTENT_UNKNOWN &&
						content != CONTENT_AIR) {
					return {fill, false};
				}
			}

			if (m_surface_depth >= 0) {
				const auto cell_size = static_cast<pos_t>(1) << step;
				const auto height_above_surface = pos.Y - surface_y;
				if (height_above_surface >= 0 && height_above_surface < cell_size) {
					// Far cells end at the sampled Y and expand downward. The cell that
					// contains the terrain surface therefore normally has its sample just
					// above the exact height. Give it the calculated weather-dependent
					// surface material instead of a stored dirt/ore sample.
					const auto fill = m_mg->visible_content(
							v3pos_t(pos.X, surface_y, pos.Z), use_weather);
					const auto content = fill.getContent();
					if (content != CONTENT_IGNORE && content != CONTENT_UNKNOWN &&
							content != CONTENT_AIR) {
						return {fill, false};
					}
				}
				const auto preserved_depth =
						static_cast<pos_t>(m_surface_depth) * cell_size;
				if (pos.Y <= surface_y - preserved_depth) {
					const auto fill = m_mg->visible_surface;
					const auto content = fill.getContent();
					if (content != CONTENT_IGNORE && content != CONTENT_UNKNOWN &&
							content != CONTENT_AIR) {
						// Deep underground detail cannot affect the visible silhouette.
						// Filling it suppresses cave and fragment faces, and also avoids a
						// far database lookup for this sample.
						return {fill, false};
					}
				}
			}
		}

		MapBlockPtr block;
		const auto step_block_pos = std::make_pair(step, bpos_aligned);
		if (block_cache && step_block_pos == block_cache_p) {
			block = block_cache;
		}

		if (!block && step < FARMESH_STEP_MAX) {
			const auto &storage = client_map.far_blocks_storage[step];
			block = storage.get(bpos_aligned).block;
		}

		const auto loadBlock = [this, &client_map](
									   const auto &bpos, const auto step) -> MapBlockPtr {
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

		if (!block && !m_client->m_simple_singleplayer_mode &&
				!m_client->far_container.have_params) {
			thread_local static std::array<std::unordered_set<v3bpos_t>, FARMESH_STEP_MAX>
					miss_cache;
			if (!miss_cache[step].contains(bpos_aligned)) {
				block = loadBlock(bpos_aligned, step);
				if (!block) {
					miss_cache[step].emplace(bpos_aligned);
				}
			}
		}

		if (block) {
			block_cache_p = step_block_pos;
			block_cache = block;

			const v3pos_t relpos{pos - bpos_aligned * MAP_BLOCKSIZE};
			const auto &relpos_shift = step;
			const v3pos_t relpos_shifted{static_cast<pos_t>(std::min(MAP_BLOCKSIZE - 1,
												 relpos.X >> relpos_shift)),
					static_cast<pos_t>(
							std::min(MAP_BLOCKSIZE - 1, relpos.Y >> relpos_shift)),
					static_cast<pos_t>(
							std::min(MAP_BLOCKSIZE - 1, relpos.Z >> relpos_shift))};
			{
				const auto n = block->getNodeNoLock(relpos_shifted);
				if (n.getContent() != CONTENT_IGNORE) {
					// Dangerous, returning ref to not locked block
					return {n, false};
				}
			}
		}
	}

	if (const auto v = m_mg->visible_content(pos, use_weather);
			v.getContent() != CONTENT_IGNORE && v.getContent() != CONTENT_UNKNOWN) {
		const auto visible = m_mg->surface_2d() && v.getContent() != CONTENT_AIR;
		return {v, visible};
	}

	return {m_mg->visible_transparent, false};
};
