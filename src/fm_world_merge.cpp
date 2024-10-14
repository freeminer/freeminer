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

#include <cstdint>
#include <cstdlib>
#include <functional>
#include <string>
#include <unordered_map>
#include "constants.h"
#include "database/database.h"
#include "debug.h"
#include "fm_server.h"
#include "irr_v3d.h"
#include "irrlichttypes.h"
#include "log.h"
#include "mapblock.h"
#include "mapnode.h"
#include "profiler.h"
#include "server.h"
#include "settings.h"
#include "fm_world_merge.h"

//  https://stackoverflow.com/a/34937216
template <typename KeyType, typename ValueType>
std::pair<KeyType, ValueType> get_max(const std::unordered_map<KeyType, ValueType> &x)
{
	using pairtype = std::pair<KeyType, ValueType>;
	return *std::max_element(x.begin(), x.end(),
			[](const pairtype &p1, const pairtype &p2) { return p1.second < p2.second; });
}

static const auto load_block = [](Server *m_server, MapDatabase *dbase,
									   const v3bpos_t &pos) -> MapBlockP {
	auto block = m_server->loadBlockNoStore(dbase, pos);
	if (!block) {
		return {};
	}
	if (!block->isGenerated()) {
		return {};
	}
	return block;
};

void WorldMerger::merge_one_block(MapDatabase *dbase, MapDatabase *dbase_up,
		const v3bpos_t &bpos_aligned, MapBlock::block_step_t step)
{
	const auto step_pow = 1;
	const auto step_size = 1 << step_pow;
	std::unordered_map<v3bpos_t, MapBlockP> blocks;
	uint32_t timestamp = 0;
	{
		for (bpos_t x = 0; x < step_size; ++x)
			for (bpos_t y = 0; y < step_size; ++y)
				for (bpos_t z = 0; z < step_size; ++z) {
					const v3pos_t rpos(x, y, z);
					const v3bpos_t nbpos(bpos_aligned.X + (x << step),
							bpos_aligned.Y + (y << step), bpos_aligned.Z + (z << step));
					auto nblock = load_block(m_server, dbase, nbpos);
					if (!nblock) {
						continue;
					}
					if (const auto ts = nblock->getActualTimestamp(); ts > timestamp)
						timestamp = ts;
					blocks[rpos] = nblock;
				}
	}
	if (!timestamp)
		timestamp = m_server->getEnv().getGameTime();

	MapBlockP block_up;

	if (partial) {
		block_up = load_block(m_server, dbase_up, bpos_aligned);
	}
	if (!block_up) {
		block_up.reset(
				m_server->getEnv().getServerMap().createBlankBlockNoInsert(bpos_aligned));
	}
	block_up->setTimestampNoChangedFlag(timestamp);
	size_t not_empty_nodes{};
	{
		const auto block_size = MAP_BLOCKSIZE;
		for (pos_t x = 0; x < block_size; ++x)
			for (pos_t y = 0; y < block_size; ++y)
				for (pos_t z = 0; z < block_size; ++z) {
					const v3pos_t npos(x, y, z);
					const v3pos_t bbpos(x >> (4 - step_pow), y >> (4 - step_pow),
							z >> (4 - step_pow));

					const auto &block = blocks[bbpos];
					if (!block) {
						continue;
					}
					const v3pos_t lpos((x << step_pow) % MAP_BLOCKSIZE,
							(y << step_pow) % MAP_BLOCKSIZE,
							(z << step_pow) % MAP_BLOCKSIZE);
					// TODO: smart node select (top priority? content priority?)
					std::unordered_map<content_t, uint8_t> top_c;
					std::vector<uint8_t> top_light_night;
					std::unordered_map<content_t, MapNode> nodes;

					// TODO: tune block selector

#if 0
// Simple grid aligned
										auto n = block->getNodeNoLock(lpos);
#else
					// Top content count

					bool maybe_air{};
					MapNode air;
					for (const auto &dir : {
								 v3pos_t{0, 1, 0},
								 v3pos_t{1, 0, 0},
								 v3pos_t{0, 0, 1},
								 v3pos_t{0, 0, 0},
								 v3pos_t{1, 1, 0},
								 v3pos_t{0, 1, 1},
								 v3pos_t{1, 0, 1},
								 v3pos_t{1, 1, 1},
						 }) {
						const auto &n = block->getNodeNoLock(lpos + dir);
						const auto c = n.getContent();
						if (c == CONTENT_IGNORE) {
							continue;
						}
						if (c == CONTENT_AIR) {
							maybe_air = true;
							air = n;
							top_c[c] += 1;
							// continue;
						} else {
							top_c[c] += 2;
						}
						if (!dir.getLengthSQ()) {
							// main node priority TODO: tune 2
							top_c[c] += 4;
						}

						if (const auto light_night = n.getLightRaw(LIGHTBANK_NIGHT,
									m_server->getNodeDefManager()->getLightingFlags(n));
								light_night) {
							top_light_night.emplace_back(light_night);
						}

						nodes[c] = n;
					}

					if (top_c.empty()) {
						if (maybe_air) {
							++not_empty_nodes;
							block_up->setNodeNoLock(npos, air);
						}
						continue;
					}
					const auto &max = get_max(top_c);
					auto n = nodes[max.first];

					if (!top_light_night.empty()) {
						auto max_light = *max_element(
								std::begin(top_light_night), std::end(top_light_night));
						n.setLight(LIGHTBANK_NIGHT, max_light,
								m_server->getNodeDefManager()->getLightingFlags(n));
					}
#endif

					if ( //n.getContent() == CONTENT_AIR ||
							n.getContent() == CONTENT_IGNORE)
						continue;
					// TODO better check
					++not_empty_nodes;

					block_up->setNodeNoLock(npos, n);
				}
	}
	// TODO: skip full air;

	if (!not_empty_nodes) {
		return;
	}
	block_up->setGenerated(true);
	m_server->getEnv().getServerMap().saveBlock(block_up.get(), dbase_up,
			m_server->getEnv().getServerMap().m_map_compression_level);
}

bool WorldMerger::merge_one_step(
		MapBlock::block_step_t step, std::unordered_set<v3bpos_t> &blocks_todo)
{

	auto *dbase = m_server->GetFarDatabase(step);
	auto *dbase_up = m_server->GetFarDatabase(step + 1);

	if (world_merge_load_all && blocks_todo.empty()) {
		actionstream << "World merge full load " << (short)step << '\n';
		std::vector<v3bpos_t> loadable_blocks;
		dbase->listAllLoadableBlocks(loadable_blocks);
		for (const auto &bpos : loadable_blocks) {
			blocks_todo.emplace(bpos);
		}
	}

	if (blocks_todo.empty()) {
		infostream << "World merge step " << (short)step << " nothing to do " << '\n';
		return false;
	}

	size_t cur_n = 0;

	const auto blocks_size = blocks_todo.size();
	infostream << "World merge "
			   //<< "run " << run
			   << " step " << (short)step << " blocks "
			   << blocks_size
			   //<< " per " << (porting::getTimeMs() - time_start) / 1000 << "s"
			   << " max_clients " << world_merge_max_clients << " throttle "
			   << world_merge_throttle << '\n';
	size_t processed = 0;

	const auto time_start = porting::getTimeMs();

	const auto printstat = [&]() {
		const auto time = porting::getTimeMs();

		infostream << "World merge "
				   // << "run " << run
				   << " " << cur_n << "/" << blocks_size << " blocks loaded "
				   << m_server->getMap().m_blocks.size() << " processed " << processed
				   << " per " << (time - time_start) / 1000 << " speed "
				   << processed / (((time - time_start) / 1000) ?: 1) << '\n';
	};

	std::unordered_set<v3bpos_t> blocks_processed;

	cur_n = 0;
	for (const auto &bpos : blocks_todo) {
		if (stop()) {
			return true;
		}

		++cur_n;

		const bpos_t shift = step + 1;

		v3bpos_t bpos_aligned((bpos.X >> shift) << shift, (bpos.Y >> shift) << shift,
				(bpos.Z >> shift) << shift);
		if (blocks_processed.contains(bpos_aligned)) {
			continue;
		}
		blocks_processed.emplace(bpos_aligned);

		++processed;
		g_profiler->add("Server: World merge blocks", 1);

		try {

			merge_one_block(dbase, dbase_up, bpos_aligned, step);

			if (!(cur_n % 10000)) {
				printstat();
			}

			if (throttle()) {
				tracestream << "World merge throttle" << '\n';

				std::this_thread::sleep_for(std::chrono::seconds(1));
			} else if (world_merge_throttle) {
				std::this_thread::sleep_for(
						std::chrono::milliseconds(world_merge_throttle));
			}

#if !EXCEPTION_DEBUG
		} catch (const std::exception &e) {
			errorstream << "world merge" << ": exception: " << e.what() << "\n"
						<< stacktrace() << '\n';
		} catch (...) {
			errorstream << "world merge" << ": Unknown unhandled exception at "
						<< __PRETTY_FUNCTION__ << ":" << __LINE__ << '\n'
						<< stacktrace() << '\n';
#else
		} catch (int) { // nothing
#endif
		}
	}
	if (world_merge_load_all == 1) {
		blocks_todo.clear();
	} else {
		blocks_todo = blocks_processed;
	}

	printstat();

	return false;
}

bool WorldMerger::merge_list(std::unordered_set<v3bpos_t> &blocks_todo)
{
	for (MapBlock::block_step_t step = 0; step < FARMESH_STEP_MAX - 1; ++step) {
		if (merge_one_step(step, blocks_todo)) {
			return true;
		}
	}
	return false;
}

bool WorldMerger::merge_all()
{
	std::unordered_set<v3bpos_t> blocks_todo;
	return merge_list(blocks_todo);
}

bool WorldMerger::merge_server_diff()
{
	std::unordered_set<v3bpos_t> changed_blocks_for_merge;
	{
		const auto lock = m_server->getEnv()
								  .getServerMap()
								  .changed_blocks_for_merge.try_lock_unique_rec();
		changed_blocks_for_merge =
				m_server->getEnv().getServerMap().changed_blocks_for_merge;
		m_server->getEnv().getServerMap().changed_blocks_for_merge.clear();
	}

	if (!changed_blocks_for_merge.empty()) {
		return merge_list(changed_blocks_for_merge);
	}

	return false;
}

WorldMergeThread::WorldMergeThread(Server *server) :
		thread_vector("WorldMerge", 20), m_server(server)
{
}

void *WorldMergeThread::run()
{
	BEGIN_DEBUG_EXCEPTION_HANDLER

	u64 world_merge = 1;
	g_settings->getU64NoEx("world_merge", world_merge);
	if (!world_merge) {
		return {};
	}

	std::this_thread::sleep_for(std::chrono::seconds(3));

	WorldMerger merger{
			.m_server{m_server},
			.stop_func{[this]() { return stopRequested(); }},
			.throttle_func{[&]() {
				return (m_server->getEnv().getPlayerCount() >
						merger.world_merge_max_clients);
			}},
	};
	{
		g_settings->getU64NoEx("world_merge_throttle", merger.world_merge_throttle);
		merger.world_merge_max_clients = m_server->isSingleplayer() ? 1 : 0;
		g_settings->getU64NoEx("world_merge_max_clients", merger.world_merge_max_clients);

		{
			merger.world_merge_load_all = -1;
			g_settings->getS16NoEx("world_merge_load_all", merger.world_merge_load_all);
			merger.world_merge_throttle = m_server->isSingleplayer() ? 10 : 0;
			u64 world_merge_all = 0;
			g_settings->getU64NoEx("world_merge_all", world_merge_all);
			if (world_merge_all) {
				merger.merge_all();
			}
		}
	}
	merger.world_merge_load_all = 0;
	merger.partial = true;

	while (!stopRequested()) {
		if (merger.throttle()) {
			tracestream << "World merge wait" << '\n';
			sleep(10);
			continue;
		}
		if (merger.merge_server_diff()) {
			return {};
		}

		sleep(60);
	}

	{
		// unbreakable at max speed
		merger.stop_func = {};
		merger.throttle_func = {};
		merger.world_merge_throttle = 0;

		if (!m_server->getEnv().getServerMap().changed_blocks_for_merge.empty()) {
			actionstream
					<< "Merge last changed blocks "
					<< m_server->getEnv().getServerMap().changed_blocks_for_merge.size()
					<< "\n";
		}
		merger.merge_server_diff();
	}

	END_DEBUG_EXCEPTION_HANDLER;
	return {};
}
