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
#include <cmath>
#include <functional>
#include <future>
#include <string>
#include <unordered_map>
#include "constants.h"
#include "database/database.h"
#include "irr_v3d.h"
#include "irrlichttypes.h"
#include "log.h"
#include "map.h"
#include "mapblock.h"
#include "mapnode.h"
#include "nodedef.h"
#include "profiler.h"
#include "server.h"
#include "fm_world_merge.h"

//  https://stackoverflow.com/a/34937216
template <typename KeyType, typename ValueType>
std::pair<KeyType, ValueType> get_max(const std::unordered_map<KeyType, ValueType> &x)
{
	using pairtype = std::pair<KeyType, ValueType>;
	return *std::max_element(x.begin(), x.end(),
			[](const pairtype &p1, const pairtype &p2) { return p1.second < p2.second; });
}

static video::SColor get_light_source_color(const ContentFeatures &cf)
{
	auto has_custom_color = [](const video::SColor &color) {
		return color.getAlpha() != 255 || color.getRed() != 255 ||
			   color.getGreen() != 255 || color.getBlue() != 255;
	};

	if (has_custom_color(cf.color))
		return cf.color;

	for (const auto &tile : cf.tiledef) {
		if (tile.has_color)
			return tile.color;
	}

	if (cf.post_effect_color.getAlpha())
		return video::SColor(255, cf.post_effect_color.getRed(),
				cf.post_effect_color.getGreen(), cf.post_effect_color.getBlue());

	return video::SColor(255, 255, 255, 255);
}

static const auto load_block = [](Map *smap, MapDatabase *dbase,
									   const v3bpos_t &pos) -> MapBlockPtr {
	auto block = loadBlockNoStore(smap, dbase, pos);
	if (!block) {
		return {};
	}
	if (!block->isGenerated()) {
		return {};
	}
	return block;
};

static int16_t average_climate(int64_t sum, size_t count)
{
	if (!count)
		return 0;

	if (sum >= 0)
		return static_cast<int16_t>(
				(sum + static_cast<int64_t>(count / 2)) / static_cast<int64_t>(count));

	return static_cast<int16_t>(
			(sum - static_cast<int64_t>(count / 2)) / static_cast<int64_t>(count));
}

WorldMerger::~WorldMerger()
{
	if (last_async.valid()) {
		last_async.wait();
	}
	merge_changed();
}

WorldMerger::one_block_stat_t WorldMerger::merge_one_block(MapDatabase *dbase,
		MapDatabase *dbase_up, const v3bpos_t &bpos_aligned, block_step_t step)
{
	const auto step_pow = 1;
	const auto step_size = 1 << step_pow;
	std::unordered_map<v3bpos_t, MapBlockPtr> blocks;
	uint32_t timestamp = 0;
	int64_t heat_sum = 0;
	int64_t humidity_sum = 0;
	int64_t heat_add_sum = 0;
	int64_t humidity_add_sum = 0;
	v3f wind_sum;
	size_t wind_count = 0;
	uint64_t heat_last_update = 0;
	uint32_t humidity_last_update = 0;
	{
		for (bpos_t x = 0; x < step_size; ++x)
			for (bpos_t y = 0; y < step_size; ++y)
				for (bpos_t z = 0; z < step_size; ++z) {
					const v3pos_t rpos(x, y, z);
					const v3bpos_t nbpos(bpos_aligned.X + (x << step),
							bpos_aligned.Y + (y << step), bpos_aligned.Z + (z << step));
					MapBlockPtr nblock;
					if (!step) {
						const auto block = smap->getBlock(nbpos);
						if (block && block->isGenerated()) {
							nblock = block;
						}
					}
					if (!nblock) {
						nblock = load_block(smap, dbase, nbpos);
						if (!nblock || !nblock->isGenerated()) {
							continue;
						}
					}
					if (const auto ts = nblock->getActualTimestamp(); ts > timestamp)
						timestamp = ts;
					heat_sum += nblock->heat;
					humidity_sum += nblock->humidity;
					heat_add_sum += nblock->heat_add;
					humidity_add_sum += nblock->humidity_add;
					const auto wind = nblock->wind;
					if (std::isfinite(wind.X) && std::isfinite(wind.Y) &&
							std::isfinite(wind.Z)) {
						wind_sum += wind;
						++wind_count;
					}
					if (const auto ts = nblock->heat_last_update.load();
							ts > heat_last_update) {
						heat_last_update = ts;
					}
					if (const auto ts = nblock->humidity_last_update.load();
							ts > humidity_last_update) {
						humidity_last_update = ts;
					}
					blocks[rpos] = nblock;
				}
	}

	if (!timestamp && get_time_func) {
		timestamp = get_time_func();
	}

	MapBlockPtr block_up;

	if (partial) {
		block_up = load_block(smap, dbase_up, bpos_aligned);
		if (block_up && lazy_up) {
			// actionstream << "s=" << step <<" at=" << block_up->getActualTimestamp() << " t=" << block_up->getTimestamp() <<  " myts=" << timestamp << "\n";
			const auto up_ts = block_up->getActualTimestamp();
			if (timestamp < up_ts + lazy_up) {
				return {};
			}
		}
	}

	if (!block_up) {
		block_up = smap->createBlankBlockNoInsert(bpos_aligned);
	}

	block_up->setTimestampNoChangedFlag(timestamp);
	const auto climate_blocks = blocks.size();
	if (climate_blocks) {
		block_up->heat = average_climate(heat_sum, climate_blocks);
		block_up->humidity = average_climate(humidity_sum, climate_blocks);
		block_up->heat_add = average_climate(heat_add_sum, climate_blocks);
		block_up->humidity_add = average_climate(humidity_add_sum, climate_blocks);
		block_up->heat_last_update = heat_last_update;
		block_up->humidity_last_update = humidity_last_update;
		block_up->wind = wind_count ? wind_sum / static_cast<float>(wind_count) : v3f();
	}

	size_t not_empty_nodes{};
	{
		const auto block_size = MAP_BLOCKSIZE;
		for (pos_t x = 0; x < block_size; ++x)
			for (pos_t y = 0; y < block_size; ++y)
				for (pos_t z = 0; z < block_size; ++z) {
					const v3pos_t npos(x, y, z);
					const v3pos_t bbpos(x >> (MAP_BLOCKP - step_pow),
							y >> (MAP_BLOCKP - step_pow), z >> (MAP_BLOCKP - step_pow));

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
						const auto p = lpos + dir;
						const auto &n = block->getNodeNoLock(p);
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

						const auto &lf = ndef->getLightingFlags(c);
						const auto &cf = ndef->get(c);

						if (const auto light_night = n.getLightRaw(LIGHTBANK_NIGHT, lf);
								light_night) {
							top_light_night.emplace_back(light_night);
						}
						// TODO: whats with lava?
						if (farlights && !step && (lf.light_source) && !cf.isLiquid()) {
							const auto plpos =
									block->getPosRelative() + p; //pos_in_block;
							block->m_light_points.try_emplace(
									plpos, MapBlock::makeLightPoint(lf.light_source,
												   get_light_source_color(cf)));
						}

						nodes[c] = n;
					}

					if (top_c.empty()) {
						if (maybe_air) {
							++not_empty_nodes;
							block_up->setNodeNoLock(npos, air, true);
						}
						continue;
					}
					const auto &max = get_max(top_c);
					auto n = nodes[max.first];

					if (!top_light_night.empty()) {
						auto max_light = *max_element(
								std::begin(top_light_night), std::end(top_light_night));
						n.setLight(LIGHTBANK_NIGHT, max_light, ndef->getLightingFlags(n));
					}
#endif

					if ( //n.getContent() == CONTENT_AIR ||
							n.getContent() == CONTENT_IGNORE)
						continue;
					// TODO better check
					++not_empty_nodes;

					block_up->setNodeNoLock(npos, n, true);
				}
	}
	// TODO: skip full air;
	one_block_stat_t one_step_stat;
	block_up->m_light_points.clear();
	if (farlights) {
		constexpr auto some_magick_thinner_const = 1; // more -> less far ligts
		constexpr auto min_no_skip_lights =
				2; // do not skip this amount lights on block << farstep
		for (const auto &[bpos, block] : blocks) {
			if (!block) {
				continue;
			}
			size_t lights_in_block = 0;
			//size_t lights_in_block_skipped = 0;
			// TODO: apply some smart? filtering here
			// block_up->m_light_points.insert(block->m_light_points.begin(), block->m_light_points.end());
			const auto size = block->m_light_points.size();
			if (!size)
				continue;
			block_up->m_light_points.reserve(block_up->m_light_points.size() + size / 2);
			const auto coef = std::log2(size);
			for (const auto &lp : block->m_light_points) {
				++one_step_stat.lights_count;
				++lights_in_block;
				const auto level = MapBlock::getLightPointLevel(lp.second);
				const auto mod = int(coef * some_magick_thinner_const * (16 - level));
				if (mod &&
						(step >= 1 ||
								lights_in_block > (min_no_skip_lights * step * 3)) &&
						(one_step_stat.lights_count % mod)) {
					//++lights_in_block_skipped;
					continue;
				}
				++one_step_stat.lights_used;
				block_up->m_light_points.emplace(lp);
			}
		}
	}

	if (not_empty_nodes) {
		block_up->setGenerated(true);
		ServerMap::saveBlock(block_up.get(), dbase_up, m_map_compression_level);
	}

	return one_step_stat;
}

bool WorldMerger::merge_one_step(
		block_step_t step, std::unordered_set<v3bpos_t> &blocks_todo)
{
	auto *dbase_current = GetFarDatabase(dbase.get(), far_dbases, save_dir, step);
	auto *dbase_up = GetFarDatabase({}, far_dbases, save_dir, step + 1);
	if (!dbase_up) {
		errorstream << "World merge: No database up for step " << (short)step << "\n";
		return true;
	}
	if (world_merge_load_all && blocks_todo.empty()) {
		actionstream << "World merge full load " << (short)step << '\n';
		std::vector<v3bpos_t> loadable_blocks;
		dbase_current->listAllLoadableBlocks(loadable_blocks);
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
	WorldMerger::one_block_stat_t stat_step;
	const auto printstat = [&]() {
		const auto time = porting::getTimeMs();

		infostream << "World merge "
				   // << "run " << run
				   << " " << cur_n << "/"
				   << blocks_size
				   //<< " blocks loaded " << m_server->getMap().m_blocks.size()
				   << " processed " << processed << " per " << (time - time_start) / 1000
				   << " lights " << stat_step.lights_used << "/" << stat_step.lights_count
				   << " speed " << processed / (((time - time_start) / 1000) ?: 1)
				   << '\n';
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
			const auto stat_block =
					merge_one_block(dbase_current, dbase_up, bpos_aligned, step);
			stat_step.lights_count += stat_block.lights_count;
			stat_step.lights_used += stat_block.lights_used;

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

	return !processed;
}

bool WorldMerger::merge_list(std::unordered_set<v3bpos_t> &blocks_todo)
{
	for (block_step_t step = 0; step < FARMESH_STEP_MAX - 1; ++step) {
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

bool WorldMerger::merge_changed()
{
	if (!changed_blocks_for_merge.empty()) {
		const auto res = merge_list(changed_blocks_for_merge);
		changed_blocks_for_merge.clear();
		return res;
	}
	return false;
}

bool WorldMerger::merge_server_diff(
		concurrent_unordered_set<v3bpos_t> &smap_changed_blocks_for_merge,
		size_t min_blocks)
{
	{
		const auto lock = smap_changed_blocks_for_merge.try_lock_unique_rec();
		if (smap_changed_blocks_for_merge.size() < min_blocks) {
			return false;
		}

		changed_blocks_for_merge = smap_changed_blocks_for_merge;
		smap_changed_blocks_for_merge.clear();
	}
	return merge_changed();
}

bool WorldMerger::stop()
{
	if (stop_func)
		return stop_func();
	return false;
}

bool WorldMerger::throttle()
{
	if (throttle_func)
		return throttle_func();
	return false;
}

bool WorldMerger::add_changed(const v3bpos_t &bpos)
{
	changed_blocks_for_merge.emplace(bpos);

	if (changed_blocks_for_merge.size() < 1000) {
		return false;
	}
	last_async =
			std::async(std::launch::async, [copy = std::move(changed_blocks_for_merge),
												   this]() mutable { merge_list(copy); });
	changed_blocks_for_merge.clear();
	return true;
}

void WorldMerger::init()
{
	m_map_compression_level =
			rangelim(g_settings->getS16("map_compression_level_disk"), -1, 9);
};
