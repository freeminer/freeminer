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
#include <string>
#include <unordered_map>
#include "constants.h"
#include "debug/iostream_debug_helpers.h"
#include "database/database.h"
#include "debug.h"
#include "fm_server.h"
#include "irr_v3d.h"
#include "irrlichttypes.h"
#include "mapblock.h"
#include "mapnode.h"
#include "profiler.h"
#include "server.h"
#include "settings.h"

WorldMergeThread::WorldMergeThread(Server *server) :
		thread_vector("WorldMerge", 20), m_server(server)
{
}

//  https://stackoverflow.com/a/34937216
template <typename KeyType, typename ValueType>
std::pair<KeyType, ValueType> get_max(const std::unordered_map<KeyType, ValueType> &x)
{
	using pairtype = std::pair<KeyType, ValueType>;
	return *std::max_element(x.begin(), x.end(),
			[](const pairtype &p1, const pairtype &p2) { return p1.second < p2.second; });
}

void *WorldMergeThread::run()
{
	BEGIN_DEBUG_EXCEPTION_HANDLER

	{
		u64 world_merge = 0;
		g_settings->getU64NoEx("world_merge", world_merge);
		if (!world_merge)
			return nullptr;
	}

	int16_t world_merge_load_all = -1; // -1 : auto;  0 : disable;   1 : force
	g_settings->getS16NoEx("world_merge_load_all", world_merge_load_all);
	u64 world_merge_throttle = m_server->isSingleplayer() ? 10 : 0;
	g_settings->getU64NoEx("world_merge_throttle", world_merge_throttle);
	u64 world_merge_max_clients = m_server->isSingleplayer() ? 1 : 0;
	g_settings->getU64NoEx("world_merge_max_clients", world_merge_max_clients);
	//		u64 abm_world_max_blocks = m_server->isSingleplayer() ? 2000 : 10000;
	//		g_settings->getU64NoEx("abm_world_max_blocks", abm_world_max_blocks);

	//auto &world_merge_last = m_server->getEnv().world_merge_last;
	const auto can_work = [&]() {
		return (m_server->getEnv().getPlayerCount() <= world_merge_max_clients
				//&& m_server->getMap().m_blocks.size() <= abm_world_max_blocks
		);
	};

	int32_t run = 0;
	//size_t pos_dir; // random start

	while (!stopRequested()) {
		++run;

		if (!can_work()) {
			tracestream << "Abm world wait" << '\n';
			sleep(10);
			continue;
		}

		auto time_start = porting::getTimeMs();

		for (MapBlock::block_step_t step = 0; step < FARMESH_STEP_MAX - 1; ++step) {
			std::vector<v3bpos_t> loadable_blocks;

			auto dbase = m_server->GetFarDatabase(step);
			auto dbase_up = m_server->GetFarDatabase(step + 1);

			if (world_merge_load_all && loadable_blocks.empty()) {
				actionstream << "World merge full load " << (short)step << '\n';
				dbase->listAllLoadableBlocks(loadable_blocks);
			}
			if (loadable_blocks.empty()) {
				break;
			}

			size_t cur_n = 0;

			const auto loadable_blocks_size = loadable_blocks.size();
			infostream << "World merge run " << run << " step " << (short)step
					   << " blocks " << loadable_blocks_size << " per "
					   << (porting::getTimeMs() - time_start) / 1000
					   << "s"
					   << " max_clients " << world_merge_max_clients << " throttle "
					   << world_merge_throttle
					   << '\n';
			size_t processed = 0;

			time_start = porting::getTimeMs();

			const auto printstat = [&]() {
				auto time = porting::getTimeMs();

				infostream << "World merge run " << run << " " << cur_n << "/"
						   << loadable_blocks_size << " blocks loaded "
						   << m_server->getMap().m_blocks.size() << " processed "
						   << processed
						   << " per " << (time - time_start) / 1000 << " speed "
						   << processed / (((time - time_start) / 1000) ?: 1)
						   << '\n';
			};

			std::unordered_set<v3bpos_t> blocks_processed;

			cur_n = 0;
			for (const auto &bpos : loadable_blocks) {
				++cur_n;

				const bpos_t shift = step + 1;

				v3bpos_t bpos_aligned((bpos.X >> shift) << shift,
						(bpos.Y >> shift) << shift, (bpos.Z >> shift) << shift);
				if (blocks_processed.contains(bpos_aligned)) {
					continue;
				}
				blocks_processed.emplace(bpos_aligned);

				if (stopRequested()) {
					return nullptr;
				}
				try {
					const auto load_block = [&](const v3bpos_t &pos) -> MapBlockP {
						auto block = m_server->loadBlockNoStore(dbase, pos);
						if (!block) {
							return {};
						}
						if (!block->isGenerated()) {
							return {};
						}
						return block;
					};

					g_profiler->add("Server: World merge blocks", 1);

					++processed;

					{
						const auto step_pow = 1;
						const auto step_size = 1 << step_pow;
						std::unordered_map<v3bpos_t, MapBlockP> blocks;
						uint32_t timestamp = 0;
						{
							for (bpos_t x = 0; x < step_size; ++x)
								for (bpos_t y = 0; y < step_size; ++y)
									for (bpos_t z = 0; z < step_size; ++z) {
										const v3pos_t rpos(
												x, y, z);
										const v3bpos_t nbpos(bpos_aligned.X + (x << step),
												bpos_aligned.Y + (y << step),
												bpos_aligned.Z + (z << step));
										auto nblock = load_block(nbpos);
										if (!nblock) {
											continue;
										}
										if (const auto ts = nblock->getActualTimestamp();
												ts > timestamp)
											timestamp = ts;
										blocks[rpos] = nblock;
									}
						}
						if (!timestamp)
							timestamp = m_server->getEnv().getGameTime();

						MapBlockP block_new{m_server->getMap().createBlankBlockNoInsert(
								bpos_aligned)};
						block_new->setTimestampNoChangedFlag(timestamp);
						size_t not_empty_nodes{};
						{
							const auto block_size = MAP_BLOCKSIZE;
							for (pos_t x = 0; x < block_size; ++x)
								for (pos_t y = 0; y < block_size; ++y)
									for (pos_t z = 0; z < block_size; ++z) {
										const v3pos_t npos(
												x, y, z);
										const v3pos_t bbpos(
												x >> (4 - step_pow), y >> (4 - step_pow),
												z >> (4 - step_pow));

										const auto &block = blocks[bbpos];
										if (!block) {
											continue;
										}
										const v3pos_t lpos(
												(x << step_pow) % MAP_BLOCKSIZE,
												(y << step_pow) % MAP_BLOCKSIZE,
												(z << step_pow) % MAP_BLOCKSIZE
										);
										// TODO: smart node select (top priority? content priority?)
										std::unordered_map<content_t, uint8_t> top_c;
										std::unordered_map<content_t, MapNode> nodes;

										// TODO: tune block selector

#if 0
// Simple grid aligned
										auto n = block->getNodeNoLock(lpos);
#else
										// Top content count

										bool maybe_air = false;
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
											const auto &n =
													block->getNodeNoLock(lpos + dir);
											const auto c = n.getContent();
											if (c == CONTENT_IGNORE) {
												continue;
											}
											if (c == CONTENT_AIR) {
												maybe_air = true;
												air = n;
												continue;
											}

											++top_c[c];
											if (!dir.getLengthSQ()) {
												// main node priority TODO: tune 2
												top_c[c] += 2;
											}
											nodes[c] = n;
										}

										if (top_c.empty()) {
											if (maybe_air) {
												++not_empty_nodes;
												block_new->setNodeNoLock(npos, air);
											}
											continue;
										}
										const auto &max = get_max(top_c);
										const auto &n = nodes[max.first];
#endif

										if ( //n.getContent() == CONTENT_AIR ||
												n.getContent() == CONTENT_IGNORE)
											continue;
										// TODO better check
										++not_empty_nodes;

										block_new->setNodeNoLock(npos, n);

									}
						}
						// TODO: skip full air;

						if (!not_empty_nodes) {
							continue;
						}
						block_new->setGenerated(true);
						m_server->getEnv().getServerMap().saveBlock(block_new.get(),
								dbase_up,
								m_server->getEnv()
										.getServerMap()
										.m_map_compression_level);
					}

					if (!(cur_n % 10000)) {
						printstat();
					}

					if (!can_work()) {
						tracestream << "World merge throttle" << '\n';

						std::this_thread::sleep_for(std::chrono::seconds(1));
					} else if (world_merge_throttle) {
						std::this_thread::sleep_for(
								std::chrono::milliseconds(world_merge_throttle));
					}

#if !EXCEPTION_DEBUG
				} catch (const std::exception &e) {
					errorstream << m_name << ": exception: " << e.what() << "\n"
								<< stacktrace() << '\n';
				} catch (...) {
					errorstream << m_name << ": Unknown unhandled exception at "
								<< __PRETTY_FUNCTION__ << ":" << __LINE__ << '\n'
								<< stacktrace() << '\n';
#else
				} catch (int) { // nothing
#endif
				}
			}
			printstat();
		}

		sleep(60);
		break;
	}
	END_DEBUG_EXCEPTION_HANDLER
	return nullptr;
}
