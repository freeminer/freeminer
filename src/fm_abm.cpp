/*
Copyright (C) 2022 proller <proler@gmail.com>
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
#include <cstddef>
#include <cstdint>
#include <limits>
#include <sstream>
#include <string>
#include "itemgroup.h"
#include "irr_v3d.h"
#include "map.h"
#include "profiler.h"
#include "server.h"
//#include "server/abmhandler.h"
#include "serverenvironment.h"
#include "servermap.h"

namespace
{

template <typename GetNode>
bool neighborsMatch(const ABMWithState &abmws, const v3pos_t &pos, uint8_t activate,
		v3pos_t &neighbor_pos, GetNode &&get_node)
{
	const FMBitset &required_neighbors = (activate & ABM_ACTIVATE_CATCH_UP)
												 ? abmws.required_neighbors_activate
												 : abmws.required_neighbors;
	const FMBitset &without_neighbors = abmws.without_neighbors;

	neighbor_pos = {};
	if (required_neighbors.empty() && without_neighbors.empty())
		return true;

	bool found_required = required_neighbors.empty();
	const int range = abmws.neighbors_range;
	for (pos_t x = pos.X - range; x <= pos.X + range; ++x)
		for (pos_t y = pos.Y - range; y <= pos.Y + range; ++y)
			for (pos_t z = pos.Z - range; z <= pos.Z + range; ++z) {
				const v3pos_t neighbor(x, y, z);
				if (neighbor == pos)
					continue;

				const content_t content = get_node(neighbor).getContent();
				if (content == CONTENT_IGNORE)
					continue;
				if (without_neighbors.get(content))
					return false;
				if (!found_required && required_neighbors.get(content)) {
					found_required = true;
					neighbor_pos = neighbor;
					if (without_neighbors.empty())
						return true;
				}
			}

	return found_required;
}

size_t getABMRunCount(const ABMWithState &abmws, float dtime)
{
	static const u32 max_catch_up_runs =
			std::clamp<u32>(g_settings->getU32("abm_max_catch_up_runs"), 1, 100);

	double intervals = dtime / abmws.interval;
	if (!abmws.simple_catchup)
		intervals = 1.0;
	if (intervals <= 0.0) {
		verbosestream << "abm: intervals=" << intervals << " dtime=" << dtime << '\n';
		intervals = 1.0;
	}

	const double scaled_chance =
			static_cast<double>(std::max<u32>(abmws.chance, 1)) / intervals;
	if (scaled_chance >= 1.0) {
		const u32 chance = static_cast<u32>(std::min(
				scaled_chance, static_cast<double>(std::numeric_limits<u32>::max())));
		return myrand() % chance ? 0 : 1;
	}

	const double expected_runs = 1.0 / scaled_chance;
	if (expected_runs >= max_catch_up_runs)
		return max_catch_up_runs;

	size_t runs = static_cast<size_t>(expected_runs);
	const double fractional_run = expected_runs - runs;
	if (myrand_float() < fractional_run)
		++runs;
	return runs;
}

} // namespace

ABMHandler::ABMHandler(ServerEnvironment *env) : m_env(env)
{
}

void ABMHandler::init(std::vector<ABMWithState> &abms)
{
	if (m_initialized) {
		warningstream << "ABMHandler::init() called more than once; ignoring" << '\n';
		return;
	}
	m_initialized = true;

	for (auto &abmws : abms) {
		ActiveABM aabm;
		aabm.abmws = &abmws;

		aabm.min_y = abmws.abm->getMinY();
		aabm.max_y = abmws.abm->getMaxY();

		// Trigger contents
		for (content_t c : abmws.trigger_ids) {
			if (!m_aabms[c]) {
				m_aabms[c] = std::make_unique<std::vector<ActiveABM>>();
			}
			m_aabms[c]->emplace_back(aabm);
			m_aabms_empty = false;
		}
	}
}

// Find out how many objects the given block and its neighbours contain.
// Returns the number of objects in the block, and also in 'wider' the
// number of objects in the block and all its neighbours. The latter
// may an estimate if any neighbours are unloaded.
u32 ABMHandler::countObjects(MapBlock *block, ServerMap *map, u32 &wider)
{
	wider = 0;
	u32 wider_unknown_count = 0;
	for (s16 x = -1; x <= 1; x++)
		for (s16 y = -1; y <= 1; y++)
			for (s16 z = -1; z <= 1; z++) {
				MapBlock *block2 = map->getBlockNoCreateNoEx(
						block->getPos() + v3bpos_t(x, y, z), true);
				if (block2 == NULL) {
					wider_unknown_count++;
					continue;
				}
				const auto lock = block2->m_static_objects.m_active.lock_shared_rec();
				wider += block2->m_static_objects.size();
				// wider += block2->m_static_objects.m_active.size() +
				//			block2->m_static_objects.m_stored.size();
			}
	// Extrapolate
	//u32 active_object_count = block->m_static_objects.m_active.size();
	u32 active_object_count = block->m_static_objects.getActiveSize();
	u32 wider_known_count = 3 * 3 * 3 - wider_unknown_count;
	if (wider_known_count)
		wider += wider_unknown_count * wider / wider_known_count;
	return active_object_count;
}

void ABMHandler::apply(MapBlock *block, uint8_t activate)
{
	if (!block)
		return;

#if ENABLE_THREADS
	auto map = std::make_unique<VoxelManipulator>();
	m_env->getServerMap().copy_27_blocks_to_vm(block, *map);
#else
	ServerMap *map = &m_env->getServerMap();
	auto lock_map = map->m_nothread_locker.try_lock_shared_rec();
	if (!lock_map->owns_lock())
		return;
#endif

	ScopeProfiler sp(g_profiler, "ABM select", SPT_ADD);

	u32 active_object_count = 0;
	u32 active_object_count_wider = 0;
	if (!m_aabms_empty) {
		active_object_count =
				countObjects(block, &m_env->getServerMap(), active_object_count_wider);
	}

	const NodeDefManager *ndef = m_env->getGameDef()->ndef();
	MapBlock::abm_triggers_type selected_triggers;
	int heat_num = 0;
	float heat_sum = 0.0f;
	int humidity_num = 0;

	const auto record_climate = [&](const MapNode &node, int count = 1) {
		const ItemGroupList &groups = ndef->get(node).groups;
		const int hot = itemgroup_get(groups, "hot");
		if (hot) {
			heat_num += count;
			heat_sum += static_cast<float>(hot) * count;
		}

		if (itemgroup_get(groups, "water") || itemgroup_get(groups, "steam"))
			humidity_num += count;
	};

	const v3pos_t block_origin = block->getPosRelative();
	bool mono_without_abm = false;
	if (block->m_is_mono_block) {
		const MapNode node = map->getNodeTry(block_origin);
		const content_t content = node.getContent();
		if (content != CONTENT_IGNORE && !m_aabms[content]) {
			constexpr int node_count = MAP_BLOCKSIZE * MAP_BLOCKSIZE * MAP_BLOCKSIZE;
			record_climate(node, node_count);
			mono_without_abm = true;
		}
	}

	if (!mono_without_abm) {
		v3pos_t relative_pos;
		for (relative_pos.X = 0; relative_pos.X < MAP_BLOCKSIZE; ++relative_pos.X)
			for (relative_pos.Y = 0; relative_pos.Y < MAP_BLOCKSIZE; ++relative_pos.Y)
				for (relative_pos.Z = 0; relative_pos.Z < MAP_BLOCKSIZE;
						++relative_pos.Z) {
					const v3pos_t pos = relative_pos + block_origin;
#if ENABLE_THREADS
					const MapNode node = map->getNodeTry(pos);
#else
					const MapNode node = block->getNodeTry(relative_pos);
#endif
					const content_t content = node.getContent();
					if (content == CONTENT_IGNORE)
						continue;

					record_climate(node);
					if (!m_aabms[content])
						continue;

					for (ActiveABM &active_abm : *m_aabms[content]) {
						if (pos.Y < active_abm.min_y || pos.Y > active_abm.max_y)
							continue;

						selected_triggers.emplace_back(abm_trigger_one{&active_abm, pos,
								content, active_object_count, active_object_count_wider,
								{}, activate});
					}
				}
	}

	if (heat_num) {
		const float heat_avg = heat_sum / static_cast<float>(heat_num);
		constexpr float min_nodes = 2.0f * MAP_BLOCKSIZE;
		const float effect_nodes =
				heat_avg >= 1.0f
						? min_nodes + (1024.0f - min_nodes) / (4096.0f / heat_avg)
						: min_nodes;
		const float base_heat = block->heat.load();
		const float density = std::min(1.0f, heat_num / effect_nodes);
		const float requested_add =
				((base_heat < 0.0f ? -base_heat : 0.0f) + heat_avg) * density;
		const float max_add = std::max(0.0f, heat_avg - base_heat);
		block->heat_add = std::clamp(requested_add, 0.0f, max_add);
	} else {
		block->heat_add = 0;
	}

	constexpr float max_humidity_effect = 70.0f;
	const float base_humidity = block->humidity.load();
	if (humidity_num && base_humidity < max_humidity_effect) {
		constexpr float max_nodes = 4.0f * MAP_BLOCKSIZE;
		const float density = std::min(1.0f, humidity_num / max_nodes);
		const float max_add = max_humidity_effect - base_humidity;
		block->humidity_add = std::clamp(max_add * density, 0.0f, max_add);
	} else {
		block->humidity_add = 0;
	}

	auto replacement = selected_triggers.empty()
							   ? nullptr
							   : std::make_unique<MapBlock::abm_triggers_type>(
										 std::move(selected_triggers));
	{
		std::lock_guard<std::mutex> lock(block->abm_triggers_mutex);
		block->abm_triggers = std::move(replacement);
	}
}

size_t MapBlock::abmTriggersRun(ServerEnvironment *m_env, u32 time, uint8_t activate)
{
	ScopeProfiler sp(g_profiler, "ABM trigger blocks", SPT_ADD);

	std::unique_lock<std::mutex> lock(abm_triggers_mutex, std::try_to_lock);
	if (!lock.owns_lock())
		return {};

	if (!abm_triggers)
		return {};

	ServerMap *map = &m_env->getServerMap();

#if !ENABLE_THREADS
	auto lock_map = m_env->getServerMap().m_nothread_locker.try_lock_shared_rec();
	if (!lock_map->owns_lock())
		return 0;
#endif

	float dtime = 1.0f;
	if (m_abm_timestamp && time >= m_abm_timestamp) {
		dtime = time - m_abm_timestamp;
	} else {
		const u32 ts = getActualTimestamp();
		if (ts && time >= ts)
			dtime = time - ts;
	}
	if (dtime <= 0.0f)
		dtime = 1.0f;

	size_t triggers_count = 0;
	unordered_map_v3bpos<u32> active_object_added;

	m_abm_timestamp = time;
	for (size_t index = 0; index < abm_triggers->size();) {
		auto remove_trigger = [&]() {
			if (index + 1 != abm_triggers->size())
				(*abm_triggers)[index] = std::move(abm_triggers->back());
			abm_triggers->pop_back();
		};

		auto &abm_trigger = (*abm_triggers)[index];
		if (!abm_trigger.abm || !abm_trigger.abm->abmws || !abm_trigger.abm->abmws->abm ||
				abm_trigger.abm->abmws->interval <= 0.0f) {
			infostream << "remove strange abm trigger dtime=" << dtime << '\n';
			remove_trigger();
			continue;
		}
		ActiveABM &aabm = *abm_trigger.abm;

		const v3pos_t &p = abm_trigger.pos;
		if (p.Y < aabm.min_y || p.Y > aabm.max_y) {
			++index;
			continue;
		}
		const uint8_t trigger_activate = abm_trigger.activate | activate;
		// Activation is a property of this run, not of the persistent candidate.
		abm_trigger.activate = ABM_ACTIVATE_NORMAL;

		const size_t run_count = getABMRunCount(*aabm.abmws, dtime);
		if (!run_count) {
			++index;
			continue;
		}

		const v3bpos_t blockpos = getNodeBlockPos(abm_trigger.pos);
		const auto added_it = active_object_added.find(blockpos);
		const u32 active_object_add =
				added_it == active_object_added.end() ? 0 : added_it->second;
		u32 active_object_count = abm_trigger.active_object_count + active_object_add;
		u32 active_object_count_wider =
				abm_trigger.active_object_count_wider + active_object_add;
		bool remove_current_trigger = false;

		for (size_t run = 0; run < run_count; ++run) {
			const MapNode node = map->getNodeTry(abm_trigger.pos);
			if (node.getContent() != abm_trigger.content) {
				remove_current_trigger = static_cast<bool>(node);
				break;
			}

			v3pos_t neighbor_pos;
			if (!neighborsMatch(*aabm.abmws, p, trigger_activate, neighbor_pos,
						[&](const v3pos_t &neighbor) {
							return map->getNodeTry(neighbor);
						}))
				break;
			abm_trigger.neighbor_pos = neighbor_pos;

			const u32 objects_added_before = m_env->m_added_objects.load();
			aabm.abmws->abm->trigger(m_env, abm_trigger.pos, node, active_object_count,
					active_object_count_wider, abm_trigger.neighbor_pos,
					trigger_activate);
			++triggers_count;
			if (isOrphan())
				break;

			if (m_env->m_added_objects.load() != objects_added_before) {
				auto block = map->getBlock(blockpos);
				if (block) {
					u32 wider = 0;
					const u32 current =
							m_env->m_abmhandler.countObjects(block.get(), map, wider);
					if (current > active_object_count)
						active_object_added[blockpos] += current - active_object_count;
					active_object_count = current;
					active_object_count_wider = wider;
					abm_trigger.active_object_count = current;
					abm_trigger.active_object_count_wider = wider;
				}
			}
		}
		if (isOrphan())
			break;
		if (remove_current_trigger) {
			remove_trigger();
			continue;
		}

		++index;
	}
	if (abm_triggers->empty())
		abm_triggers.reset();

	if (triggers_count) {
		std::stringstream key;
		key << "a" << getPos().X << "," << getPos().Y << "," << getPos().Z;
		m_env->blocks_with_abm.put(key.str(), std::to_string(time));
	}

	return triggers_count;
}

bool MapBlock::hasAbmTriggers()
{
	std::unique_lock<std::mutex> lock(abm_triggers_mutex, std::try_to_lock);
	return lock.owns_lock() && abm_triggers && !abm_triggers->empty();
}

uint8_t ServerEnvironment::analyzeBlock(MapBlockPtr block)
{
	u32 block_timestamp = block->getActualTimestamp();
	if (block->m_next_analyze_timestamp > block_timestamp) {
		// infostream<<"not anlalyzing: "<< block->getPos() <<"ats="<<block->m_next_analyze_timestamp<< " bts="<< block_timestamp<<std::endl;
		return {};
	}
	ScopeProfiler sp(g_profiler, "ABM analyze", SPT_ADD);
	if (!block->analyzeContent())
		return {};
	const uint8_t activate = block_timestamp - block->m_next_analyze_timestamp > 3600
									 ? ABM_ACTIVATE_CATCH_UP
									 : ABM_ACTIVATE_NORMAL;
	m_abmhandler.apply(block.get(), activate);
	// infostream<<"ServerEnvironment::analyzeBlock p="<<block->getPos()<< " tdiff="<<block_timestamp - block->m_next_analyze_timestamp <<" co="<<block->content_only <<" triggers="<<(block->abm_triggers ? block->abm_triggers->size() : -1) <<std::endl;
	block->m_next_analyze_timestamp = block_timestamp + 2;
	return activate;
}
