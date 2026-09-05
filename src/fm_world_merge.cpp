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
#include <chrono>
#include <cstdint>
#include <cstdlib>
#include <cmath>
#include <functional>
#include <future>
#include <limits>
#include <mutex>
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
#include "fm_far_node.h"

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

static uint64_t valid_update_time(uint64_t timestamp)
{
	if (timestamp == std::numeric_limits<uint64_t>::max() ||
			timestamp == std::numeric_limits<uint32_t>::max())
		return 0;
	return timestamp;
}

static uint64_t newest_block_time(const MapBlockPtr &block)
{
	if (!block)
		return 0;

	const auto heat_time = valid_update_time(block->heat_last_update.load());
	const auto humidity_time = valid_update_time(block->humidity_last_update.load());
	return std::max<uint64_t>(
			block->getActualTimestamp(), std::max(heat_time, humidity_time));
}

static bool within_lazy_window(uint64_t source_time, uint64_t target_time, uint32_t lazy)
{
	if (!lazy)
		return false;
	if (source_time < target_time)
		return true;
	return source_time - target_time < lazy;
}

std::optional<size_t> world_merge::selectFarNodeIndex(
		const std::array<MapNode, 8> &samples, const std::array<bool, 8> *exposed,
		const NodeDefManager *ndef)
{
	constexpr size_t main_sample = 3;
	size_t valid_count = 0;
	size_t solid_count = 0;
	bool has_opaque_structure = false;

	for (const auto &node : samples) {
		const auto content = node.getContent();
		if (content == CONTENT_IGNORE || content == CONTENT_UNKNOWN)
			continue;
		++valid_count;
		if (content != CONTENT_AIR) {
			++solid_count;
			if (ndef && farmesh::isOpaqueStructure(ndef->get(content)))
				has_opaque_structure = true;
		}
	}

	if (!valid_count)
		return std::nullopt;

	// A tie is solid so a flat surface with four samples above and four below
	// remains closed. Sparse solids no longer survive merely because they happen
	// to contain the grid-aligned sample.
	const bool select_solid = solid_count * 2 >= valid_count;
	// Glass or foliage may cover ground or a wall in the upper samples. Do not let
	// that transparent cover replace the structure of the entire coarse cell.
	// Filter only the material vote: sparse cover must still lose to air.
	const auto material_candidate = [&](content_t content) {
		return content != CONTENT_IGNORE && content != CONTENT_UNKNOWN &&
			   (content != CONTENT_AIR) == select_solid &&
			   !(select_solid && has_opaque_structure &&
					   farmesh::isTransparentCover(ndef->get(content)));
	};
	// Occupancy remains volume-based, but material can come from the exposed
	// upper layer. This keeps grass, snow, roads, and similar surface covers from
	// being outvoted by the dirt or stone directly underneath them.
	constexpr std::array<uint8_t, 8> sample_y{{1, 0, 0, 0, 1, 1, 0, 1}};
	int selected_exposed_y = -1;
	if (select_solid && exposed) {
		for (size_t i = 0; i < samples.size(); ++i) {
			const auto content = samples[i].getContent();
			if ((*exposed)[i] && material_candidate(content)) {
				selected_exposed_y =
						std::max(selected_exposed_y, static_cast<int>(sample_y[i]));
			}
		}
	}
	std::optional<size_t> best_index;
	size_t best_count = 0;

	for (size_t i = 0; i < samples.size(); ++i) {
		const auto content = samples[i].getContent();
		if (!material_candidate(content) ||
				(selected_exposed_y >= 0 &&
						(!(*exposed)[i] || sample_y[i] != selected_exposed_y)))
			continue;

		size_t count = 0;
		for (size_t candidate_index = 0; candidate_index < samples.size();
				++candidate_index) {
			if (samples[candidate_index].getContent() == content &&
					(selected_exposed_y < 0 ||
							((*exposed)[candidate_index] &&
									sample_y[candidate_index] == selected_exposed_y)))
				++count;
		}
		// Preserve the grid-aligned surface material when it has support from
		// another sample. Occupancy was already decided above, so this keeps road
		// edges without bringing sparse underground fragments back.
		if (select_solid && solid_count < valid_count && i == main_sample && count > 1)
			count += 2;

		if (!best_index || count > best_count ||
				(count == best_count && i == main_sample)) {
			best_index = i;
			best_count = count;
		}
	}

	return best_index;
}

WorldMerger::~WorldMerger()
{
	std::future<void> async;
	{
		std::lock_guard<std::mutex> lock(changed_blocks_mutex);
		async = std::move(last_async);
	}
	if (async.valid())
		async.wait();
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
	one_block_stat_t one_step_stat;
	using light_points_t = std::unordered_map<v3pos_t, MapBlock::light_t>;
	std::unordered_map<v3bpos_t, light_points_t> generated_light_points;
	// Cache source-neighbour blocks used by six-sided light exposure checks.
	std::unordered_map<v3bpos_t, MapBlockPtr> light_neighbor_blocks;
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
					if (require_lighting_complete && !step &&
							nblock->getLightingComplete() != 0xffff) {
						one_step_stat.deferred = true;
						return one_step_stat;
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
			const auto source_time = std::max<uint64_t>(
					timestamp, std::max<uint64_t>(valid_update_time(heat_last_update),
									   valid_update_time(humidity_last_update)));
			if (within_lazy_window(source_time, newest_block_time(block_up), lazy_up)) {
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

	// Read a source node across both coarse-cell and mapblock boundaries.
	// World-merge blocks loaded outside the live map are retained in a small local
	// cache so an exposed border lamp is tested against all six actual neighbours.
	const auto get_light_neighbor = [&](const MapBlockPtr &source_block,
											const v3pos_t &node_pos) -> MapNode {
		const auto node_bpos = getNodeBlockPos(node_pos);
		MapBlockPtr node_block;
		if (source_block && source_block->getPos() == node_bpos) {
			node_block = source_block;
		} else if (!step) {
			const v3bpos_t relative_bpos = node_bpos - bpos_aligned;
			if (const auto block_it = blocks.find(relative_bpos);
					block_it != blocks.end()) {
				node_block = block_it->second;
			} else {
				auto [cache_it, inserted] = light_neighbor_blocks.try_emplace(node_bpos);
				if (inserted) {
					auto loaded = smap->getBlock(node_bpos);
					if (!loaded || !loaded->isGenerated())
						loaded = load_block(smap, dbase, node_bpos);
					cache_it->second = std::move(loaded);
				}
				node_block = cache_it->second;
			}
		}

		if (!node_block || !node_block->isGenerated())
			return MapNode(CONTENT_IGNORE);
		return node_block->getNodeNoLock(node_pos - node_block->getPosRelative());
	};

	size_t not_empty_nodes{};
	{
		const auto block_size = MAP_BLOCKSIZE;
		for (pos_t x = 0; x < block_size; ++x)
			for (pos_t y = 0; y < block_size; ++y)
				for (pos_t z = 0; z < block_size; ++z) {
					const v3pos_t npos(x, y, z);
					const v3bpos_t bbpos(x >> (MAP_BLOCKP - step_pow),
							y >> (MAP_BLOCKP - step_pow), z >> (MAP_BLOCKP - step_pow));

					const auto block_it = blocks.find(bbpos);
					if (block_it == blocks.end() || !block_it->second) {
						continue;
					}
					const auto &block = block_it->second;

					const v3pos_t lpos((x << step_pow) % MAP_BLOCKSIZE,
							(y << step_pow) % MAP_BLOCKSIZE,
							(z << step_pow) % MAP_BLOCKSIZE);
					// Resolve samples through the loaded 2x2x2 source-block group so
					// surface exposure is correct at a source mapblock boundary as well.
					const auto get_source_node = [&blocks, &bbpos](v3pos_t sample_pos) {
						const v3bpos_t sample_bbpos{
								static_cast<bpos_t>(
										bbpos.X + sample_pos.X / MAP_BLOCKSIZE),
								static_cast<bpos_t>(
										bbpos.Y + sample_pos.Y / MAP_BLOCKSIZE),
								static_cast<bpos_t>(
										bbpos.Z + sample_pos.Z / MAP_BLOCKSIZE)};
						const auto sample_block_it = blocks.find(sample_bbpos);
						if (sample_block_it == blocks.end() || !sample_block_it->second)
							return MapNode(CONTENT_IGNORE);
						sample_pos.X %= MAP_BLOCKSIZE;
						sample_pos.Y %= MAP_BLOCKSIZE;
						sample_pos.Z %= MAP_BLOCKSIZE;
						return sample_block_it->second->getNodeNoLock(sample_pos);
					};
					// TODO: tune block selector

#if 0
// Simple grid aligned
										auto n = block->getNodeNoLock(lpos);
#else
					// Decide occupancy first, then select a material. Combining both
					// votes used to preserve sparse underground fragments.
					std::array<MapNode, 8> samples;
					samples.fill(MapNode(CONTENT_IGNORE));
					// Mark solids whose node immediately above propagates light.
					std::array<bool, 8> exposed{};
					uint8_t max_light_night = 0;
					const std::array<v3pos_t, 8> sample_dirs{{
							{0, 1, 0},
							{1, 0, 0},
							{0, 0, 1},
							{0, 0, 0},
							{1, 1, 0},
							{0, 1, 1},
							{1, 0, 1},
							{1, 1, 1},
					}};
					// Visibility filtering applies to each light candidate, not only
					// to the node that later wins coarse material selection.
					const std::array<v3pos_t, 6> side_dirs{{
							{-1, 0, 0},
							{1, 0, 0},
							{0, -1, 0},
							{0, 1, 0},
							{0, 0, -1},
							{0, 0, 1},
					}};
					for (size_t sample_index = 0; sample_index < sample_dirs.size();
							++sample_index) {
						const auto &dir = sample_dirs[sample_index];
						const auto p = lpos + dir;
						const auto n = get_source_node(p);
						const auto c = n.getContent();
						if (c == CONTENT_IGNORE || c == CONTENT_UNKNOWN) {
							continue;
						}
						samples[sample_index] = n;
						// Prefer visible upper material without changing the solid/air
						// occupancy decision for the coarse cell.
						if (c != CONTENT_AIR) {
							const auto above = get_source_node(p + v3pos_t(0, 1, 0));
							const auto above_content = above.getContent();
							if (above_content != CONTENT_IGNORE &&
									above_content != CONTENT_UNKNOWN) {
								const auto &above_features = ndef->get(above_content);
								const auto &above_lighting =
										ndef->getLightingFlags(above_content);
								exposed[sample_index] = above_content == CONTENT_AIR ||
														above_features.isLiquid() ||
														above_lighting.light_propagates;
							}
						}

						const auto &lf = ndef->getLightingFlags(c);
						if (const auto light_night = n.getLightRaw(LIGHTBANK_NIGHT, lf);
								light_night) {
							max_light_night = std::max(max_light_night, light_night);
						}
					}

					// Extract every exposed emitter before choosing the coarse terrain
					// material. Sparse street lamps normally lose that occupancy vote to air.
					if (farlights && !step) {
						for (size_t source_index = 0; source_index < samples.size();
								++source_index) {
							const auto source_content =
									samples[source_index].getContent();
							if (source_content == CONTENT_IGNORE ||
									source_content == CONTENT_UNKNOWN)
								continue;

							const auto &source_lf =
									ndef->getLightingFlags(source_content);
							if (!source_lf.light_source)
								continue;

							bool has_transparent_side = false;
							const auto source_pos = sample_dirs[source_index];
							const auto plpos =
									block->getPosRelative() + lpos + source_pos;
							for (const auto &side_dir : side_dirs) {
								const auto side_content =
										get_light_neighbor(block, plpos + side_dir)
												.getContent();
								if (side_content == CONTENT_IGNORE ||
										side_content == CONTENT_UNKNOWN)
									continue;
								const auto &side_features = ndef->get(side_content);
								const auto &side_lf =
										ndef->getLightingFlags(side_content);
								if (side_content == CONTENT_AIR ||
										side_features.isLiquid() ||
										side_lf.light_propagates) {
									has_transparent_side = true;
									break;
								}
							}

							if (!has_transparent_side)
								continue;
							generated_light_points[bbpos].try_emplace(plpos,
									MapBlock::makeLightPoint(source_lf.light_source,
											get_light_source_color(
													ndef->get(source_content))));
						}
					}

					const auto selected =
							world_merge::selectFarNodeIndex(samples, &exposed, ndef);
					if (!selected)
						continue;
					auto n = samples[*selected];

					if (max_light_night) {
						n.setLight(LIGHTBANK_NIGHT, max_light_night,
								ndef->getLightingFlags(n));
					}
#endif

					if ( //n.getContent() == CONTENT_AIR ||
							n.getContent() == CONTENT_IGNORE)
						continue;

					block_up->setNodeNoLock(npos, n, true);
					if (n.getContent() != CONTENT_AIR) {
						// TODO better check
						++not_empty_nodes;
					}
				}
	}
	// TODO: skip full air;
	// This block is not published yet; build its new light-point snapshot in place.
	block_up->m_light_points = std::make_shared<MapBlock::light_points_t>();
	if (farlights) {
		constexpr auto some_magick_thinner_const = 2; // more -> less far ligts
		constexpr auto min_no_skip_lights =
				2; // do not skip this amount lights on block << farstep
		for (const auto &[bpos, block] : blocks) {
			if (!block) {
				continue;
			}
			const light_points_t *light_points = nullptr;
			std::shared_ptr<MapBlock::light_points_t> source_light_points;
			if (!step) {
				const auto lights_it = generated_light_points.find(bpos);
				if (lights_it == generated_light_points.end())
					continue;
				light_points = &lights_it->second;
			} else {
				{
					const auto lock = block->lock_shared_rec();
					source_light_points = block->m_light_points;
				}
				light_points = source_light_points.get();
			}
			if (!light_points || light_points->empty())
				continue;

			size_t lights_in_block = 0;
			//size_t lights_in_block_skipped = 0;
			// TODO: apply some smart? filtering here
			// block_up->m_light_points.insert(block->m_light_points.begin(), block->m_light_points.end());
			const auto size = light_points->size();
			if (!size)
				continue;
			block_up->m_light_points->reserve(
					block_up->m_light_points->size() + size / 2);
			const auto coef = std::log2(size);
			const auto keep_first =
					min_no_skip_lights * (static_cast<size_t>(step) + 1) * 3;
			for (const auto &lp : *light_points) {
				++one_step_stat.lights_count;
				++lights_in_block;
				const auto level = MapBlock::getLightPointLevel(lp.second);
				const auto mod = int(coef * some_magick_thinner_const * (16 - level));
				if (mod > 1 && lights_in_block > keep_first &&
						(one_step_stat.lights_count % mod)) {
					//++lights_in_block_skipped;
					continue;
				}
				++one_step_stat.lights_used;
				block_up->m_light_points->emplace(lp);
			}
		}
	}

	if (not_empty_nodes) {
		block_up->setGenerated(true);
		if (ServerMap::saveBlock(block_up.get(), dbase_up, m_map_compression_level) &&
				far_block_ready_func) {
			block_up->far_step = step + 1;
			far_block_ready_func(block_up, step + 1);
		}
	} else {
		dbase_up->deleteBlock(bpos_aligned);
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

	std::unordered_set<v3bpos_t> blocks_seen;
	std::unordered_set<v3bpos_t> blocks_deferred;
	std::unordered_set<v3bpos_t> blocks_processed;

	cur_n = 0;
	for (const auto &bpos : blocks_todo) {
		if (stop()) {
			if (require_lighting_complete)
				for (const auto &pending : blocks_todo)
					smap->changed_blocks_for_merge.emplace(pending);
			return true;
		}

		++cur_n;

		const bpos_t shift = step + 1;

		v3bpos_t bpos_aligned((bpos.X >> shift) << shift, (bpos.Y >> shift) << shift,
				(bpos.Z >> shift) << shift);
		if (blocks_seen.contains(bpos_aligned)) {
			if (require_lighting_complete && !step &&
					blocks_deferred.contains(bpos_aligned))
				smap->changed_blocks_for_merge.emplace(bpos);
			continue;
		}
		blocks_seen.emplace(bpos_aligned);

		try {
			const auto stat_block =
					merge_one_block(dbase_current, dbase_up, bpos_aligned, step);
			if (stat_block.deferred) {
				blocks_deferred.emplace(bpos_aligned);
				if (require_lighting_complete && !step)
					smap->changed_blocks_for_merge.emplace(bpos);
				continue;
			}
			blocks_processed.emplace(bpos_aligned);
			++processed;
			g_profiler->add("Server: World merge blocks", 1);
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
			if (require_lighting_complete && !step) {
				blocks_deferred.emplace(bpos_aligned);
				smap->changed_blocks_for_merge.emplace(bpos);
			}
			errorstream << "world merge" << ": exception: " << e.what() << "\n"
						<< stacktrace() << '\n';
		} catch (...) {
			if (require_lighting_complete && !step) {
				blocks_deferred.emplace(bpos_aligned);
				smap->changed_blocks_for_merge.emplace(bpos);
			}
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
	std::lock_guard<std::mutex> lock(merge_mutex);
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
	std::unordered_set<v3bpos_t> blocks_todo;
	{
		std::lock_guard<std::mutex> lock(changed_blocks_mutex);
		blocks_todo.swap(changed_blocks_for_merge);
	}
	if (blocks_todo.empty())
		return false;
	return merge_list(blocks_todo);
}

bool WorldMerger::merge_server_diff(
		concurrent_unordered_set<v3bpos_t> &smap_changed_blocks_for_merge,
		size_t min_blocks)
{
	std::unordered_set<v3bpos_t> blocks_todo;
	{
		const auto lock = smap_changed_blocks_for_merge.lock_unique_rec();
		if (smap_changed_blocks_for_merge.size() < min_blocks) {
			return false;
		}

		blocks_todo = smap_changed_blocks_for_merge;
		smap_changed_blocks_for_merge.clear();
	}
	return merge_list(blocks_todo);
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
	std::unordered_set<v3bpos_t> blocks_todo;
	std::future<void> previous_async;
	{
		std::lock_guard<std::mutex> lock(changed_blocks_mutex);
		changed_blocks_for_merge.emplace(bpos);

		if (changed_blocks_for_merge.size() < 1000) {
			return false;
		}
		blocks_todo = std::move(changed_blocks_for_merge);
		changed_blocks_for_merge.clear();
		previous_async = std::move(last_async);
		last_async = std::async(
				std::launch::async, [this, previous = std::move(previous_async),
											copy = std::move(blocks_todo)]() mutable {
					if (previous.valid())
						previous.wait();
					merge_list(copy);
				});
	}
	return true;
}

void WorldMerger::init()
{
	m_map_compression_level =
			rangelim(g_settings->getS16("map_compression_level_disk"), -1, 9);
};
