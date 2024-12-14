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
#include <memory>
#include "database/database.h"
#include "irr_v3d.h"
#include "irrlichttypes.h"
#include "map.h"
#include "mapblock.h"
#include "profiler.h"
#include "nodedef.h"
#include "environment.h"
#include "emerge.h"
#include "mapgen/mg_biome.h"
#include "gamedef.h"
#include "reflowscan.h"
#include "server.h"
#include "server/ban.h"
#include "util/directiontables.h"
#include "serverenvironment.h"
#include "voxelalgorithms.h"

#if HAVE_THREAD_LOCAL
namespace
{
thread_local MapBlockPtr block_cache{};
thread_local v3bpos_t block_cache_p;
}
#endif

std::atomic_uint ServerMap::time_life {};

// TODO: REMOVE THIS func and use Map::getBlock
MapBlockPtr Map::getBlock(v3bpos_t p, bool trylock, bool nocache)
{

#ifndef NDEBUG
	ScopeProfiler sp(g_profiler, "Map: getBlock");
#endif

#if !ENABLE_THREADS
	nocache = true; // very dirty hack. fix and remove. Also compare speed: no cache and
					// cache with lock
#endif

	if (!nocache) {
#if ENABLE_THREADS && !HAVE_THREAD_LOCAL
		const auto lock = maybe_shared_lock(m_block_cache_mutex, try_to_lock);
		if (lock.owns_lock())
#endif
			if (block_cache && p == block_cache_p) {
#ifndef NDEBUG
				g_profiler->add("Map: getBlock cache hit", 1);
#endif
				return block_cache;
			}
	}

	MapBlockPtr block;
	{
		const auto lock = trylock ? m_blocks.try_lock_shared_rec() : m_blocks.lock_shared_rec();
		if (!lock->owns_lock())
			return nullptr;
		const auto &n = m_blocks.find(p);
		if (n == m_blocks.end())
			return nullptr;
		block = n->second;
	}

	if (!nocache) {
#if ENABLE_THREADS && !HAVE_THREAD_LOCAL
		const auto lock = unique_lock(m_block_cache_mutex, try_to_lock);
		if (lock.owns_lock())
#endif
		{
			block_cache_p = p;
			block_cache = block;
		}
	}

	return block;
}

MapBlock *Map::getBlockNoCreateNoEx(v3pos_t p, bool trylock, bool nocache)
{
	return getBlock(p, trylock, nocache).get();
}

void Map::getBlockCacheFlush()
{
#if ENABLE_THREADS && !HAVE_THREAD_LOCAL
	const auto lock = unique_lock(m_block_cache_mutex);
#endif
	block_cache = nullptr;
}

MapBlockPtr Map::createBlankBlockNoInsert(const v3pos_t &p)
{
	const auto block = std::make_shared<MapBlock>(p, m_gamedef);
	return block;
}

MapBlockPtr Map::createBlankBlock(const v3pos_t &p)
{
	m_db_miss.erase(p);

	auto block = getBlock(p, false, true);
	if (block != NULL) {
		infostream << "Block already created p=" << block->getPos() << std::endl;
		return block;
	}

	block = createBlankBlockNoInsert(p);

	const auto lock = m_blocks.lock_unique_rec();

	m_blocks.insert_or_assign(p, block);

	return block;
}

bool Map::insertBlock(MapBlockPtr block)
{
	auto block_p = block->getPos();

	m_db_miss.erase(block_p);

	const auto lock = m_blocks.lock_unique_rec();

	auto block2 = getBlock(block_p, false, true);
	if (block2) {
		verbosestream << "Block already exists " << block_p << std::endl;
		return false;
	}

	// Insert into container
	m_blocks.insert_or_assign(block_p, block);
	return true;
}

MapBlockPtr ServerMap::createBlock(v3bpos_t p)
{
	if (const auto block = getBlock(p, false, true)) {
		return block;
	}
	return createBlankBlock(p);
}

/*
bool Map::eraseBlock(v3pos_t blockpos)
{
	auto block = getBlockNoCreateNoEx(blockpos);
	if (!block)
		return false;
	eraseBlock(block);
	return true;
}
*/

void Map::eraseBlock(const MapBlockPtr block)
{
	const auto block_p = block->getPos();
	(*m_blocks_delete)[block] = 1;
	m_blocks.erase(block_p);
#if ENABLE_THREADS && !HAVE_THREAD_LOCAL
	const auto lock = unique_lock(m_block_cache_mutex);
#endif
	block_cache = nullptr;
}

MapNode Map::getNodeTry(const v3pos_t &p)
{
#ifndef NDEBUG
	ScopeProfiler sp(g_profiler, "Map: getNodeTry");
#endif
	auto blockpos = getNodeBlockPos(p);
	auto block = getBlockNoCreateNoEx(blockpos, true);
	if (!block) {
		return {CONTENT_IGNORE};
	}
	auto relpos = p - blockpos * MAP_BLOCKSIZE;
	return block->getNodeRef(relpos);
}

MapNode &Map::getNodeRef(const v3pos_t &p)
{
#ifndef NDEBUG
	ScopeProfiler sp(g_profiler, "Map: getNodeTry");
#endif
	auto blockpos = getNodeBlockPos(p);
	auto block = getBlockNoCreateNoEx(blockpos, true);
	if (!block) {
		static thread_local MapNode dummy{CONTENT_IGNORE};
		return dummy;
	}
	auto relpos = p - blockpos * MAP_BLOCKSIZE;
	return block->getNodeRef(relpos);
}

/*
MapNode Map::getNodeLog(v3POS p){
	auto blockpos = getNodeBlockPos(p);
	auto block = getBlockNoCreateNoEx(blockpos);
	v3POS relpos = p - blockpos*MAP_BLOCKSIZE;
	auto node = block->getNodeNoEx(relpos);
	infostream<<"getNodeLog("<<p<<") blockpos="<<blockpos<<" block="<<block<<"
relpos="<<relpos<<" n="<<node<<std::endl; return node;
}
*/

/*
MapNode Map::getNodeNoLock(v3POS p) //dont use
{
	v3POS blockpos = getNodeBlockPos(p);
	MapBlock *block = getBlockNoCreateNoEx(blockpos);
	if(block == NULL)
		return MapNode(CONTENT_IGNORE);
	return block->getNodeNoLock(p - blockpos*MAP_BLOCKSIZE);
}
*/

s16 Map::getHeat(const v3pos_t &p, bool no_random)
{
	MapBlock *block = getBlockNoCreateNoEx(getNodeBlockPos(p));
	if (block != NULL) {
		s16 value = block->heat + block->heat_add;
		return value + (no_random ? 0 : myrand_range(0, 1));
	}
	// errorstream << "No heat for " << p.X<<"," << p.Z << std::endl;
	return 0;
}

s16 Map::getHumidity(const v3pos_t &p, bool no_random)
{
	MapBlock *block = getBlockNoCreateNoEx(getNodeBlockPos(p));
	if (block != NULL) {
		s16 value = block->humidity + block->humidity_add;
		return value + (no_random ? 0 : myrand_range(0, 1));
	}
	// errorstream << "No humidity for " << p.X<<"," << p.Z << std::endl;
	return 0;
}

s16 ServerMap::updateBlockHeat(ServerEnvironment *env, const v3pos_t &p, MapBlock *block,
		unordered_map_v3pos<s16> *cache, bool block_add)
{
	const auto bp = getNodeBlockPos(p);
	const auto gametime = env->getGameTime();
	if (block) {
		if (gametime < block->heat_last_update) {
			return block->heat +
				   (block_add ? (short)block->heat_add : 0); // + myrand_range(0, 1);
		}
	} else if (!cache) {
		block = getBlockNoCreateNoEx(bp, true);
	}
	if (cache && cache->contains(bp)) {
		return cache->at(bp); // + myrand_range(0, 1);
	}
	auto value = m_emerge->biomemgr->calcBlockHeat(p, getSeed(), env->getTimeOfDayF(),
			gametime * env->m_time_of_day_speed, env->m_use_weather);

	if (block) {
		block->heat = value;
		block->heat_last_update = env->m_use_weather ? gametime + 30 : -1;
		if (block_add)
			value += block->heat_add; // in cache stored total value
	}
	if (cache) {
		(*cache)[bp] = value;
	}
	return value; // + myrand_range(0, 1);
}

s16 ServerMap::updateBlockHumidity(ServerEnvironment *env, const v3pos_t &p,
		MapBlock *block, unordered_map_v3pos<s16> *cache, bool block_add)
{
	const auto bp = getNodeBlockPos(p);
	const auto gametime = env->getGameTime();
	if (block) {
		if (gametime < block->humidity_last_update)
			return block->humidity +
				   (block_add ? (short)block->humidity_add : 0); //+ myrand_range(0, 1);
	} else if (!cache) {
		block = getBlockNoCreateNoEx(bp, true);
	}
	if (cache && cache->count(bp))
		return cache->at(bp) + myrand_range(0, 1);

	auto value = m_emerge->biomemgr->calcBlockHumidity(p, getSeed(), env->getTimeOfDayF(),
			gametime * env->m_time_of_day_speed, env->m_use_weather);

	if (block) {
		block->humidity = value;
		block->humidity_last_update = env->m_use_weather ? gametime + 30 : -1;
		if (block_add)
			value += block->humidity_add;
	}
	if (cache)
		(*cache)[bp] = value;
	value += myrand_range(0, 1);
	return value > 100 ? 100 : value;
}

int ServerMap::getSurface(const v3pos_t &basepos, int searchup, bool walkable_only)
{

	s16 max = MYMIN(searchup + basepos.Y, 0x7FFF);

	MapNode last_node = getNode(basepos);
	MapNode node = last_node;
	v3pos_t runpos = basepos;
	auto *nodemgr = m_gamedef->ndef();

	bool last_was_walkable = nodemgr->get(node).walkable;

	while ((runpos.Y < max) && (node.param0 != CONTENT_AIR)) {
		runpos.Y += 1;
		last_node = node;
		node = getNode(runpos);

		if (!walkable_only) {
			if ((last_node.param0 != CONTENT_AIR) &&
					(last_node.param0 != CONTENT_IGNORE) &&
					(node.param0 == CONTENT_AIR)) {
				return runpos.Y;
			}
		} else {
			bool is_walkable = nodemgr->get(node).walkable;

			if (last_was_walkable && (!is_walkable)) {
				return runpos.Y;
			}
			last_was_walkable = is_walkable;
		}
	}

	return basepos.Y - 1;
}

/*NodeDefManager* Map::getNodeDefManager() {
	return m_gamedef->ndef();
}*/

void Map::copy_27_blocks_to_vm(MapBlock *block, VoxelManipulator &vmanip)
{

	v3pos_t blockpos = block->getPos();
	v3pos_t blockpos_nodes = blockpos * MAP_BLOCKSIZE;

	// Allocate this block + neighbors
	vmanip.clear();
	VoxelArea voxel_area(blockpos_nodes - v3pos_t(1, 1, 1) * MAP_BLOCKSIZE,
			blockpos_nodes + v3pos_t(1, 1, 1) * MAP_BLOCKSIZE * 2 - v3pos_t(1, 1, 1));
	vmanip.addArea(voxel_area);

	block->copyTo(vmanip);

	for (u16 i = 0; i < 26; i++) {
		v3pos_t bp = blockpos + g_26dirs[i];
		auto b = getBlockNoCreateNoEx(bp);
		if (b)
			b->copyTo(vmanip);
	}
}

u32 Map::timerUpdate(float uptime, float unload_timeout, s32 max_loaded_blocks,
		std::vector<v3bpos_t> *unloaded_blocks, unsigned int max_cycle_ms)
{
	bool save_before_unloading = maySaveBlocks();

	// Profile modified reasons
	Profiler modprofiler;

	if (porting::getTimeMs() > m_blocks_delete_time) {
		m_blocks_delete = (m_blocks_delete == &m_blocks_delete_1 ? &m_blocks_delete_2
																 : &m_blocks_delete_1);
		if (!m_blocks_delete->empty())
			verbosestream << "Deleting blocks=" << m_blocks_delete->size() << std::endl;
		m_blocks_delete->clear();
		getBlockCacheFlush();
		const thread_local static auto block_delete_time =
				g_settings->getS16("block_delete_time");
		m_blocks_delete_time = porting::getTimeMs() + block_delete_time * 1000;
	}

	u32 deleted_blocks_count = 0;
	u32 saved_blocks_count = 0;
	u32 block_count_all = 0;

	u32 n = 0, calls = 0;
	const auto end_ms = porting::getTimeMs() + max_cycle_ms;

	std::vector<MapBlockPtr> blocks_delete;
	int save_started = 0;
	{
		const auto lock = m_blocks.try_lock_shared_rec();
		if (!lock->owns_lock()) {
			return m_blocks_update_last;
		}

#if !ENABLE_THREADS
		auto lock_map = m_nothread_locker.try_lock_unique_rec();
		if (!lock_map->owns_lock())
			return m_blocks_update_last;
#endif

		auto m_blocks_size = m_blocks.size();

		for (const auto &ir : m_blocks) {
			if (n++ < m_blocks_update_last) {
				continue;
			} else {
				m_blocks_update_last = 0;
			}
			++calls;

			const auto block = ir.second;
			if (!block) {
				blocks_delete.emplace_back(block);
				continue;
			}

			/*
			if (block->refGet()) {
				continue;
			}
			*/

			if (!block->isGenerated())
#if CHECK_CLIENT_BUILD()
				if (!block->getLodMesh(0, true))
#endif
				{
					blocks_delete.emplace_back(block);
					continue;
				}

			{
				const auto lock = block->try_lock_unique_rec();
				if (!lock->owns_lock()) {
					continue;
				}
				if (block->getUsageTimer() > unload_timeout) { // block->refGet() <= 0 &&
					const v3bpos_t p = block->getPos();
					// infostream<<" deleting block p="<<p<<"
					// ustimer="<<block->getUsageTimer() <<" to="<< unload_timeout<<"
					// inc="<<(uptime - block->m_uptime_timer_last)<<"
					// state="<<block->getModified()<<std::endl;
					//  Save if modified
					if (block->getModified() != MOD_STATE_CLEAN &&
							save_before_unloading) {
						// modprofiler.add(block->getModifiedReasonString(), 1);
						if (!save_started++)
							beginSave();
						if (!saveBlock(block.get())) {
							continue;
						}
						saved_blocks_count++;
					}

					blocks_delete.emplace_back(block);

					if (unloaded_blocks)
						unloaded_blocks->push_back(p);

					deleted_blocks_count++;
				} else {
					if (!block->m_uptime_timer_last) // not very good place, but minimum
													 // modifications
						block->m_uptime_timer_last = uptime - 0.1;
					block->incrementUsageTimer(uptime - block->m_uptime_timer_last);
					block->m_uptime_timer_last = uptime;

					block_count_all++;
				}

			} // block lock

			if (calls > std::max(size_t(100), m_blocks_size / 10) &&
					porting::getTimeMs() > end_ms) {
				m_blocks_update_last = n;
				break;
			}
		}
	}
	if (save_started)
		endSave();

	if (!calls)
		m_blocks_update_last = 0;

	for (auto &block : blocks_delete)
		eraseBlock(block);

	// Finally delete the empty sectors

	if (deleted_blocks_count != 0) {
		if (m_blocks_update_last)
			infostream << "ServerMap: timerUpdate(): Blocks processed:" << calls << "/"
					   << m_blocks.size() << " to " << m_blocks_update_last << std::endl;
		PrintInfo(infostream); // ServerMap/ClientMap:
		infostream << "Unloaded " << deleted_blocks_count << "/"
				   << (block_count_all + deleted_blocks_count) << " blocks from memory";
		infostream << " (deleteq1=" << m_blocks_delete_1.size()
				   << " deleteq2=" << m_blocks_delete_2.size() << ")";
		if (saved_blocks_count)
			infostream << ", of which " << saved_blocks_count << " were written";
		/*
				infostream<<", "<<block_count_all<<" blocks in memory";
		*/
		infostream << "." << std::endl;
		if (saved_blocks_count != 0) {
			PrintInfo(infostream); // ServerMap/ClientMap:
			// infostream<<"Blocks modified by: "<<std::endl;
			modprofiler.print(infostream);
		}
	}
	return m_blocks_update_last;
}

//#if TODO
inline u8 diminish_light(u8 light, float amount = 1)
{
	if (light == 0)
		return 0;
	if (amount < 1) {
		amount = myrand_range(0, 1) < amount;
	}
	if (light >= LIGHT_MAX)
		return LIGHT_MAX - amount;

	return light - amount;
}

/*
inline u8 diminish_light(u8 light, u8 distance)
{
	if (distance >= light)
		return 0;
	return light - distance;
}
*/

inline u8 undiminish_light(u8 light)
{
	// We don't know if light should undiminish from this particular 0.
	// Thus, keep it at 0.
	if (light == 0)
		return 0;
	if (light == LIGHT_MAX)
		return light;

	return light + 1;
}

const v3pos_t dirs6[6] = {
		{0, 0, 1},	// back
		{0, 1, 0},	// top
		{1, 0, 0},	// right
		{0, 0, -1}, // front
		{0, -1, 0}, // bottom
		{-1, 0, 0}, // left
};

/*
	Goes recursively through the neighbours of the node.

	Alters only transparent nodes.

	If the lighting of the neighbour is lower than the lighting of
	the node was (before changing it to 0 at the step before), the
	lighting of the neighbour is set to 0 and then the same stuff
	repeats for the neighbour.

	The ending nodes of the routine are stored in light_sources.
	This is useful when a light is removed. In such case, this
	routine can be called for the light node and then again for
	light_sources to re-light the area without the removed light.

	values of from_nodes are lighting values.
*/
void ServerMap::unspreadLight(enum LightBank bank, std::map<v3pos_t, u8> &from_nodes,
		std::set<v3pos_t> &light_sources, std::map<v3bpos_t, MapBlock *> &modified_blocks)
{
	auto *nodemgr = m_gamedef->ndef();

	if (from_nodes.empty())
		return;

	u32 blockchangecount = 0;

	std::map<v3pos_t, u8> unlighted_nodes;

	/*
		Initialize block cache
	*/
	v3bpos_t blockpos_last;
	MapBlock *block = NULL;
	// Cache this a bit, too
	bool block_checked_in_modified = false;

	for (auto j = from_nodes.begin(); j != from_nodes.end(); ++j) {
		auto pos = j->first;
		auto blockpos = getNodeBlockPos(pos);

		// Only fetch a new block if the block position has changed
		/*
				try{
		*/
		if (block == NULL || blockpos != blockpos_last) {
			block = getBlockNoCreateNoEx(blockpos);
			blockpos_last = blockpos;

			block_checked_in_modified = false;
			blockchangecount++;
		}
		/*
				}
				catch(InvalidPositionException &e)
				{
					continue;
				}
		*/

		if (!block)
			continue;

		// Calculate relative position in block
		// v3pos_t relpos = pos - blockpos_last * MAP_BLOCKSIZE;

		// Get node straight from the block
		// MapNode n = block->getNode(relpos);

		u8 oldlight = j->second;

		// Loop through 6 neighbors
		for (u16 i = 0; i < 6; i++) {
			// Get the position of the neighbor node
			v3pos_t n2pos = pos + dirs6[i];

			// Get the block where the node is located
			v3bpos_t blockpos;
			v3pos_t relpos;
			getNodeBlockPosWithOffset(n2pos, blockpos, relpos);

			// Only fetch a new block if the block position has changed
			/*
						try {
			*/
			if (block == NULL || blockpos != blockpos_last) {
				block = getBlockNoCreateNoEx(blockpos);

				if (!block)
					continue;

				blockpos_last = blockpos;

				block_checked_in_modified = false;
				blockchangecount++;
			}
			/*
						}
						catch(InvalidPositionException &e) {
							continue;
						}
			*/

			// Get node straight from the block
			bool is_valid_position;
			MapNode n2 = block->getNode(relpos, &is_valid_position);
			if (!is_valid_position)
				continue;
			const auto &f2 = nodemgr->getLightingFlags(n2);

			bool changed = false;

			// TODO: Optimize output by optimizing light_sources?

			/*
				If the neighbor is dimmer than what was specified
				as oldlight (the light of the previous node)
			*/
			if (n2.getLight(bank, f2) < oldlight) {
				/*
					And the neighbor is transparent and it has some light
				*/
				if (nodemgr->get(n2).light_propagates && n2.getLight(bank, f2) != 0) {
					/*
						Set light to 0 and add to queue
					*/

					u8 current_light = n2.getLight(bank, f2);
					n2.setLight(bank, 0, f2);
					block->setNode(relpos, n2);

					unlighted_nodes[n2pos] = current_light;
					changed = true;

					/*
						Remove from light_sources if it is there
						NOTE: This doesn't happen nearly at all
					*/
					/*if(light_sources.find(n2pos))
					{
						infostream<<"Removed from light_sources"<<std::endl;
						light_sources.remove(n2pos);
					}*/
				}

				/*// DEBUG
				if(light_sources.find(n2pos) != NULL)
					light_sources.remove(n2pos);*/
			} else {
				light_sources.insert(n2pos);
			}

			// Add to modified_blocks
			if (changed == true && block_checked_in_modified == false) {
				// If the block is not found in modified_blocks, add.
				/*
								if(modified_blocks.find(blockpos) ==
				   modified_blocks.end())
								{
				*/
				//++block->lighting_broken;
				block->setLightingComplete(0);

				modified_blocks[blockpos] = block;
				/*
								}
				*/
				block_checked_in_modified = true;
			}
		}
	}

	/*infostream<<"unspreadLight(): Changed block "
	<<blockchangecount<<" times"
	<<" for "<<from_nodes.size()<<" nodes"
	<<std::endl;*/

	if (!unlighted_nodes.empty())
		unspreadLight(bank, unlighted_nodes, light_sources, modified_blocks);
}

/*
	Lights neighbors of from_nodes, collects all them and then
	goes on recursively.
*/
void ServerMap::spreadLight(enum LightBank bank, std::set<v3pos_t> &from_nodes,
		std::map<v3bpos_t, MapBlock *> &modified_blocks, uint64_t end_ms)
{
	auto *nodemgr = m_gamedef->ndef();

	if (from_nodes.empty())
		return;

	u32 blockchangecount = 0;

	std::set<v3pos_t> lighted_nodes;

	/*
		Initialize block cache
	*/
	v3bpos_t blockpos_last;
	MapBlock *block = NULL;
	// Cache this a bit, too
	bool block_checked_in_modified = false;

	for (auto j = from_nodes.begin(); j != from_nodes.end(); ++j) {
		auto pos = *j;
		v3bpos_t blockpos;
		v3pos_t relpos;

		getNodeBlockPosWithOffset(pos, blockpos, relpos);

		// Only fetch a new block if the block position has changed
		if (block == NULL || blockpos != blockpos_last) {
#if !ENABLE_THREADS
			const auto lock = m_nothread_locker.try_lock_shared_rec();
			if (!lock->owns_lock())
				continue;
#endif
			block = getBlockNoCreateNoEx(blockpos);
			if (!block)
				continue;
			blockpos_last = blockpos;

			block_checked_in_modified = false;
			blockchangecount++;
		}

		// const auto lock = block->try_lock_unique_rec();
		// if (!lock->owns_lock())
		//	continue;

		// Get node straight from the block
		bool is_valid_position;
		MapNode n = block->getNode(relpos, &is_valid_position);
		if (n.getContent() == CONTENT_IGNORE)
			continue;
		const auto &f = nodemgr->getLightingFlags(n);

		u8 oldlight = is_valid_position ? n.getLight(bank, f) : 0;
		u8 newlight = diminish_light(oldlight);

		// Loop through 6 neighbors
		for (u16 i = 0; i < 6; i++) {
			// Get the position of the neighbor node
			v3pos_t n2pos = pos + dirs6[i];

			// Get the block where the node is located
			v3bpos_t blockpos;
			v3pos_t relpos;
			getNodeBlockPosWithOffset(n2pos, blockpos, relpos);

			// Only fetch a new block if the block position has changed
			// try {
			if (block == NULL || blockpos != blockpos_last) {
				block = getBlockNoCreateNoEx(blockpos);
				if (!block)
					continue;
				blockpos_last = blockpos;

				block_checked_in_modified = false;
				blockchangecount++;
			}
			/*
						}
						catch(InvalidPositionException &e) {
							continue;
						}
			*/

			// Get node straight from the block
			MapNode n2 = block->getNode(relpos, &is_valid_position);
			if (!is_valid_position)
				continue;
			const auto &f2 = nodemgr->getLightingFlags(n);

			bool changed = false;
			/*
				If the neighbor is brighter than the current node,
				add to list (it will light up this node on its turn)
			*/
			if (n2.getLight(bank, f2) > undiminish_light(oldlight)) {
				lighted_nodes.insert(n2pos);
				changed = true;
			}
			/*
				If the neighbor is dimmer than how much light this node
				would spread on it, add to list
			*/
			if (n2.getLight(bank, f2) < newlight) {
				if (nodemgr->get(n2).light_propagates) {
					n2.setLight(bank, newlight, f2);
					block->setNode(relpos, n2);
					lighted_nodes.insert(n2pos);
					changed = true;
				}
			}

			// Add to modified_blocks
			if (changed == true && block_checked_in_modified == false) {
				// If the block is not found in modified_blocks, add.
				/*
								if(modified_blocks.find(blockpos) ==
				   modified_blocks.end())
								{
				*/
				modified_blocks[blockpos] = block;
				/*
								}
				*/
				block_checked_in_modified = true;
			}
		}
	}

	/*infostream<<"spreadLight(): Changed block "
			<<blockchangecount<<" times"
			<<" for "<<from_nodes.size()<<" nodes"
			<<std::endl;*/

	if (end_ms && porting::getTimeMs() > end_ms) {
		return;
	}

	if (!lighted_nodes.empty()) {
		/*
																   infostream<<"spreadLight(): recursive("<<recursive<<"): changed="
															  <<blockchangecount
																	   <<" from="<<from_nodes.size()
																	   <<" lighted="<<lighted_nodes.size()
																	   <<" modifiedB="<<modified_blocks.size()
																	   <<std::endl;
														   */
		spreadLight(bank, lighted_nodes, modified_blocks, end_ms);
	}
}

u32 ServerMap::updateLighting(concurrent_map<v3pos_t, MapBlock *> &a_blocks,
		std::map<v3pos_t, MapBlock *> &modified_blocks, unsigned int max_cycle_ms)
{
	lighting_map_t lighting_mblocks;
	for (auto &i : a_blocks)
		lighting_mblocks[i.first] = 0;
	unordered_map_v3pos<int> processed;
	return updateLighting(lighting_mblocks, processed, max_cycle_ms);
}

#if OLD_
u32 ServerMap::updateLighting(lighting_map_t &a_blocks,
		unordered_map_v3pos<int> &processed, unsigned int max_cycle_ms)
{
	std::map<v3pos_t, MapBlock *> modified_blocks;

	auto *nodemgr = m_gamedef->ndef();

	int ret = 0;
	int loopcount = 0;

	TimeTaker timer("updateLighting");

	// For debugging
	// bool debug=true;
	// u32 count_was = modified_blocks.size();

	// std::unordered_set<v3POS, v3posHash, v3posEqual> light_sources;
	// std::unordered_map<v3POS, u8, v3posHash, v3posEqual> unlight_from_day,
	// unlight_from_night;
	std::set<v3pos_t> light_sources;
	std::map<v3pos_t, u8> unlight_from_day, unlight_from_night;
	// unordered_map_v3pos<int> processed;

	int num_bottom_invalid = 0;

	// MutexAutoLock lock2(m_update_lighting_mutex);

	MAP_NOTHREAD_LOCK(this);

	{
		// TimeTaker t("updateLighting: first stuff");

		const auto end_ms = porting::getTimeMs() + max_cycle_ms;
		for (auto i = a_blocks.begin(); i != a_blocks.end();) {

			// processed[i->first] = //1000000;
			// infostream<<"Light: start col if=" << i->first << std::endl;
			auto block = getBlockNoCreateNoEx(i->first);

			for (;;) {
				// Don't bother with dummy blocks.
				if (!block || !block->isGenerated()) {
					i = a_blocks.erase(i);
					goto ablocks_end;
				}
				const auto lock = block->try_lock_unique_rec();
				if (!lock->owns_lock()) {
					break; // may cause dark areas
				}
				v3pos_t pos = block->getPos();
				// if (processed.count(pos)) infostream<<"Light: test pos" << pos << "
				// pps="<<processed[pos] << " >= if="<< i->first.Y <<std::endl;

				if (processed.count(pos) && processed[pos] >= i->first.Y) {
					// infostream<<"Light: skipping pos" << pos << " pps="<<processed[pos]
					// << " >= if="<< i->first.Y <<std::endl;
					break;
				}
				++loopcount;
				processed[pos] = i->first.Y;
				v3pos_t posnodes = block->getPosRelative();
				// modified_blocks[pos] = block;

				block->setLightingExpired(true);
				block->setLightingComplete(0);
				//++block->lighting_broken;

				/*
					Clear all light from block
				*/
				for (s16 z = 0; z < MAP_BLOCKSIZE; z++)
					for (s16 x = 0; x < MAP_BLOCKSIZE; x++)
						for (s16 y = 0; y < MAP_BLOCKSIZE; y++) {
							v3pos_t p(x, y, z);
							bool is_valid_position;
							MapNode n = block->getNode(p, &is_valid_position);
							if (!is_valid_position) {
								/* This would happen when dealing with a
								   dummy block.
								*/
								infostream << "updateLighting(): "
											  "InvalidPositionException"
										   << std::endl;
								continue;
							}
							const auto &f = nodemgr->getLightingFlags(n);

							u8 oldlight_day = n.getLight(LIGHTBANK_DAY, f);
							u8 oldlight_night = n.getLight(LIGHTBANK_NIGHT, f);
							n.setLight(LIGHTBANK_DAY, 0, f);
							n.setLight(LIGHTBANK_NIGHT, 0, f);
							block->setNode(p, n);

							// If node sources light, add to list
							// u8 source = nodemgr->get(n).light_source;
							if (nodemgr->get(n).light_source)
								light_sources.insert(p + posnodes);

							v3pos_t p_map = p + posnodes;
							// Collect borders for unlighting
							if (x == 0 || x == MAP_BLOCKSIZE - 1 || y == 0 ||
									y == MAP_BLOCKSIZE - 1 || z == 0 ||
									z == MAP_BLOCKSIZE - 1) {
								if (oldlight_day)
									unlight_from_day[p_map] = oldlight_day;
								if (oldlight_night)
									unlight_from_night[p_map] = oldlight_night;
							}
						}

				lock->unlock();

				bool bottom_valid = propagateSunlight(pos, light_sources);

				if (!bottom_valid)
					num_bottom_invalid++;

				pos.Y--;
				block = getBlockNoCreateNoEx(pos);
			}

			// magic for erase:
			++i;
		ablocks_end:

			if (porting::getTimeMs() > end_ms) {
				++ret;
				break;
			}
		}
	}

	{
		// TimeTaker timer("updateLighting: unspreadLight");
		unspreadLight(LIGHTBANK_DAY, unlight_from_day, light_sources, modified_blocks);
		unspreadLight(
				LIGHTBANK_NIGHT, unlight_from_night, light_sources, modified_blocks);
	}

	{
		// TimeTaker timer("updateLighting: spreadLight");
		spreadLight(LIGHTBANK_DAY, light_sources, modified_blocks,
				porting::getTimeMs() + max_cycle_ms * 10);
		spreadLight(LIGHTBANK_NIGHT, light_sources, modified_blocks,
				porting::getTimeMs() + max_cycle_ms * 10);
	}

	// infostream<<"light: processed="<<processed.size()<< " loopcount="<<loopcount<< "
	// ablocks_bef="<<a_blocks.size();

	for (auto &i : modified_blocks) {
		// a_blocks.erase(i.first);
		processed[i.first] = 1;
	}
	for (auto &i : processed) {
		a_blocks.erase(i.first);
		MapBlock *block = getBlockNoCreateNoEx(i.first);
		if (!block)
			continue;
		block->setLightingExpired(false);
		block->setLightingComplete(0xFFFF);
		// block->lighting_broken = 0;
	}
	// infostream<< " ablocks_aft="<<a_blocks.size()<<std::endl;
	g_profiler->add("Server: light blocks", loopcount);

	return ret;
}
#endif

u32 ServerMap::updateLighting(lighting_map_t &a_blocks,
		unordered_map_v3pos<int> &processed, unsigned int max_cycle_ms)
{
	std::map<v3pos_t, MapBlock *> modified_blocks;

	int ret = 0;
	int loopcount = 0;

	TimeTaker timer("updateLighting");

	for (auto i = a_blocks.begin(); i != a_blocks.end();) {
		auto block = getBlockNoCreateNoEx(i->first);
		if (!block) {
			i = a_blocks.erase(i);
			continue;
		}
		if (!voxalgo::repair_block_light(this, block, &modified_blocks)) {
			processed[block->getPos()] = block->getPos().Y;
		}
		++loopcount;
		++i;
	}

	for (const auto &i : modified_blocks) {
		a_blocks.erase(i.first);
	}
	for (const auto &i : processed) {
		a_blocks.erase(i.first);
	}
	g_profiler->add("Server: light blocks", loopcount);

	return ret;
}

const v3pos_t g_4dirs[4] = {
		// +right, +top, +back
		{0, 0, 1},	// back
		{1, 0, 0},	// right
		{0, 0, -1}, // front
		{-1, 0, 0}, // left
};

bool ServerMap::propagateSunlight(
		const v3bpos_t &pos, std::set<v3pos_t> &light_sources, bool remove_light)
{
	MapBlock *block = getBlockNoCreateNoEx(pos);

	// const auto lock = block->lock_unique_rec(); //no: in block_below_is_valid getnode outside
	// block

	auto *nodemgr = m_gamedef->ndef();

	// Whether the sunlight at the top of the bottom block is valid
	bool block_below_is_valid = true;

	static thread_local const bool light_ambient = g_settings->getBool("light_ambient");

	const v3pos_t pos_relative = block->getPosRelative();

	for (s16 x = 0; x < MAP_BLOCKSIZE; ++x) {
		for (s16 z = 0; z < MAP_BLOCKSIZE; ++z) {
			bool no_sunlight = false;

			// Check if node above block has sunlight

			MapNode n = getNode(pos_relative + v3pos_t(x, MAP_BLOCKSIZE, z));
			const auto &f = nodemgr->getLightingFlags(n);

			if (n) {
				if (n.getLight(LIGHTBANK_DAY, f) != LIGHT_SUN && !light_ambient) {
					no_sunlight = true;
				}
			} else {

				// NOTE: This makes over-ground roofed places sunlighted
				// Assume sunlight, unless is_underground==true
				if (block->getIsUnderground()) {
					no_sunlight = true;
				} else {
					MapNode n = block->getNode(v3pos_t(x, MAP_BLOCKSIZE - 1, z));
					if (n && nodemgr->get(n).sunlight_propagates == false)
						no_sunlight = true;
				}
				// NOTE: As of now, this just would make everything dark.
				// No sunlight here
				// no_sunlight = true;
			}

			s16 y = MAP_BLOCKSIZE - 1;

			// This makes difference to diminishing in water.
			// bool stopped_to_solid_object = false;

			u8 current_light = no_sunlight ? 0 : LIGHT_SUN;

			if (no_sunlight && !remove_light)
				for (const auto &dir : g_4dirs) {
					const auto n =
							getNode(pos_relative + v3pos_t(x, MAP_BLOCKSIZE, z) + dir);
					const auto &f = nodemgr->getLightingFlags(n);

					if (n.getLight(LIGHTBANK_DAY, f) == LIGHT_SUN) {
						current_light = LIGHT_SUN;
						break;
					}
				}

			for (; y >= 0; --y) {
				v3pos_t pos(x, y, z);
				MapNode n = block->getNode(pos);
				const auto &f = nodemgr->getLightingFlags(n);

				const auto &ndef = nodemgr->get(n);
				if (current_light == 0) {
					//break;
					// Do nothing
				} else if (current_light == LIGHT_SUN && ndef.sunlight_propagates) {
					// Do nothing: Sunlight is continued
				} else if (ndef.light_propagates == false) {
					// A solid object is on the way.
					// stopped_to_solid_object = true;

					// Light stops.
					current_light = 0;
				} else {
					// Diminish light
					current_light =
							diminish_light(current_light, ndef.light_vertical_dimnish);
				}

				u8 old_light = n.getLight(LIGHTBANK_DAY, f);

				if (current_light > old_light || remove_light) {
					n.setLight(LIGHTBANK_DAY, current_light, f);
					block->setNode(pos, n);
				}

				if (diminish_light(current_light) != 0) {
					light_sources.insert(pos_relative + pos);
				}
			}

			// Whether or not the block below should see LIGHT_SUN
			bool sunlight_should_go_down = (current_light == LIGHT_SUN);

			/*
				If the block below hasn't already been marked invalid:

				Check if the node below the block has proper sunlight at top.
				If not, the block below is invalid.

				Ignore non-transparent nodes as they always have no light
			*/

			if (block_below_is_valid) {
				MapNode n = getNode(pos_relative + v3pos_t(x, -1, z));
				const auto &f = nodemgr->getLightingFlags(n);

				if (n) {
					if (nodemgr->get(n).light_propagates) {
						if (n.getLight(LIGHTBANK_DAY, f) == LIGHT_SUN &&
								sunlight_should_go_down == false)
							block_below_is_valid = false;
						else if (n.getLight(LIGHTBANK_DAY, f) != LIGHT_SUN &&
								 sunlight_should_go_down == true)
							block_below_is_valid = false;
					}
				} else {
					// Just no block below, no need to panic.
				}
			}
		}
	}

	return block_below_is_valid;
}
//#endif

void ServerMap::lighting_modified_add(const v3pos_t &pos, int range)
{
	MutexAutoLock lock(m_lighting_modified_mutex);
	if (m_lighting_modified_blocks.contains(pos)) {
		auto old_range = m_lighting_modified_blocks[pos];
		if (old_range <= range)
			return;
		m_lighting_modified_blocks_range[old_range].erase(pos);
	}
	m_lighting_modified_blocks[pos] = range;
	m_lighting_modified_blocks_range[range][pos] = range;
};

unsigned int ServerMap::updateLightingQueue(unsigned int max_cycle_ms, int &loopcount)
{
	unsigned int ret = 0;
	const auto end_ms = porting::getTimeMs() + max_cycle_ms;
	unordered_map_v3pos<int> processed;
	for (;;) {
		lighting_map_t blocks;
		int range = 5;
		{
			MutexAutoLock lock(m_lighting_modified_mutex);
			auto r = m_lighting_modified_blocks_range.begin();
			if (r == m_lighting_modified_blocks_range.end())
				break;
			++loopcount;
			range = r->first;
			blocks = r->second;
			// infostream <<" go light range="<< r->first << " size="<<blocks.size()<< " ranges="<<m_lighting_modified_blocks_range.size()<<" total blk"<<m_lighting_modified_blocks.size()<< std::endl;
			m_lighting_modified_blocks_range.erase(r);
			for (auto &i : blocks)
				m_lighting_modified_blocks.erase(i.first);
		}
		ret += updateLighting(blocks, processed, max_cycle_ms);

		{
			MutexAutoLock lock(m_lighting_modified_mutex);
			for (auto &i : blocks) {
				m_lighting_modified_blocks_range[range][i.first] = i.second;
				m_lighting_modified_blocks[i.first] = i.second;
			}
		}
		// infostream << " ok light range=" << range << " retbacksize=" << blocks.size()
		// << " ret="<< ret << " processed="<<processed.size()<< std::endl;
		if (porting::getTimeMs() > end_ms)
			break;
	}

	{
		MutexAutoLock lock(m_lighting_modified_mutex);
		for (auto &i : processed) {
			if (m_lighting_modified_blocks.count(i.first)) {
				m_lighting_modified_blocks_range[m_lighting_modified_blocks[i.first]]
						.erase(i.first);
				m_lighting_modified_blocks.erase(i.first);
			}
		}
	}

	// infostream << "light ret=" << ret << " " << loopcount << std::endl;

	return ret;
}

/*
MapNode Map::getNodeNoEx(v3pos_t p) {
#ifndef NDEBUG
	ScopeProfiler sp(g_profiler, "Map: getNodeNoEx");
#endif

	v3bpos_t blockpos = getNodeBlockPos(p);
	MapBlock *block = getBlockNoCreateNoEx(blockpos);
	if (!block)
		return ignoreNode;

	v3pos_t relpos = p - blockpos * MAP_BLOCKSIZE;
	return block->getNode(relpos);
}
*/

/**
 * Get the ground level by searching for a non CONTENT_AIR node in a column from top to
 * bottom
 */
s16 ServerMap::findGroundLevel(v2pos_t p2d, bool cacheBlocks)
{

	pos_t level;

	// The reference height is the original mapgen height
	pos_t referenceHeight = m_emerge->getGroundLevelAtPoint(p2d);
	pos_t maxSearchHeight = 63 + referenceHeight;
	pos_t minSearchHeight = -63 + referenceHeight;
	v3pos_t probePosition(p2d.X, maxSearchHeight, p2d.Y);
	v3pos_t blockPosition = getNodeBlockPos(probePosition);
	v3pos_t prevBlockPosition = blockPosition;

	MAP_NOTHREAD_LOCK(this);

	// Cache the block to be inspected.
	if (cacheBlocks) {
		emergeBlock(blockPosition, false);
	}

	// Probes the nodes in the given column
	for (; probePosition.Y > minSearchHeight; probePosition.Y--) {
		if (cacheBlocks) {
			// Calculate the block position of the given node
			blockPosition = getNodeBlockPos(probePosition);

			// If the node is in an different block, cache it
			if (blockPosition != prevBlockPosition) {
				emergeBlock(blockPosition, false);
				prevBlockPosition = blockPosition;
			}
		}

		MapNode node = getNode(probePosition);
		if (node.getContent() != CONTENT_IGNORE && node.getContent() != CONTENT_AIR) {
			break;
		}
	}

	// Could not determine the ground. Use map generator noise functions.
	if (probePosition.Y == minSearchHeight) {
		level = referenceHeight;
	} else {
		level = probePosition.Y;
	}

	return level;
}

void ServerMap::prepareBlock(MapBlock *block)
{
	ServerEnvironment *senv = &((Server *)m_gamedef)->getEnv();

	// Calculate weather conditions
	//block->heat_last_update     = 0;
	//block->humidity_last_update = 0;
	v3pos_t p = block->getPos() * MAP_BLOCKSIZE;
	updateBlockHeat(senv, p, block);
	updateBlockHumidity(senv, p, block);
}

#if 0
MapBlockPtr ServerMap::loadBlockPtr(v3bpos_t p3d)
{
	if (!m_map_loading_enabled) {
		return {};
	}

	ScopeProfiler sp(g_profiler, "ServerMap::loadBlock");
	const auto sector = this;
	MapBlockPtr block;
	try {
		std::string blob;
		m_db.dbase->loadBlock(p3d, &blob);
		if (!blob.length() && m_db.dbase_ro) {
			m_db.dbase_ro->loadBlock(p3d, &blob);
		}
		if (!blob.length()) {
			m_db_miss.emplace(p3d);
			return nullptr;
		}

		std::istringstream is(blob, std::ios_base::binary);

		u8 version = SER_FMT_VER_INVALID;
		is.read((char *)&version, 1);

		if (is.fail()) {
			throw SerializationError("ServerMap::loadBlock(): Failed"
									 " to read MapBlock version");
		}
		/*u32 block_size = MapBlock::serializedLength(version);
		SharedBuffer<u8> data(block_size);
		is.read((char*)*data, block_size);*/

		// This will always return a sector because we're the server
		//MapSector *sector = emergeSector(p2d);

		bool created_new = false;
		block = sector->getBlock(p3d, false, true);
		if (block == NULL) {
			block = sector->createBlankBlockNoInsert(p3d);
			created_new = true;
		}

		// Read basic data
		if (!block->deSerialize(is, version, true)) {
			if (created_new && block)
				//delete block;
				return nullptr;
		}

		// If it's a new block, insert it to the map
		if (created_new)
			if (!sector->insertBlock(block)) {
				//delete block;
				return nullptr;
			}

		if (!g_settings->getBool("liquid_real")) {
			ReflowScan scanner(this, m_emerge->ndef);
			scanner.scan(block.get(), &m_transforming_liquid);
		}

		// We just loaded it from, so it's up-to-date.
		block->resetModified();

		/*
		if (block->getLightingExpired()) {
			verbosestream<<"Loaded block with exiried lighting. (maybe sloooow appear), try recalc " << p3d<<std::endl;
			lighting_modified_blocks.set(p3d, nullptr);
		}
*/

		//MapBlock *block = getBlockNoCreateNoEx(blockpos);
		if (created_new && (block != NULL)) {
			std::map<v3bpos_t, MapBlock *> modified_blocks;
			// Fix lighting if necessary
			voxalgo::update_block_border_lighting(this, block.get(), modified_blocks);
			if (!modified_blocks.empty()) {
				//Modified lighting, send event
				MapEditEvent event;
				event.type = MEET_OTHER;
				for (auto it = modified_blocks.begin(); it != modified_blocks.end(); ++it)
					event.modified_blocks.push_back(it->first);
				dispatchEvent(event);
			}
		}

		return block;
	} catch (const std::exception &e) {
		//if (block)
		//	delete block;

		errorstream << "Invalid block data in database" << " (" << p3d.X << "," << p3d.Y
					<< "," << p3d.Z << ")" << " (SerializationError): " << e.what()
					<< std::endl;

		// TODO: Block should be marked as invalid in memory so that it is
		// not touched but the game can run

		if (g_settings->getBool("ignore_world_load_errors")) {
			errorstream << "Ignoring block load error. Duck and cover! "
						<< "(ignore_world_load_errors)" << std::endl;
		} else {
			throw SerializationError("Invalid block data in database");
		}
	}
	return nullptr;
}
#endif

s32 ServerMap::save(ModifiedState save_level, float dedicated_server_step, bool breakable)
{
	if (!m_map_saving_enabled) {
		warningstream << "Not saving map, saving disabled." << std::endl;
		return 0;
	}

	const auto start_time = porting::getTimeUs();

	if (save_level == MOD_STATE_CLEAN)
		infostream << "ServerMap: Saving whole map, this can take time." << std::endl;

	if (m_map_metadata_changed || save_level == MOD_STATE_CLEAN) {
		if (settings_mgr.saveMapMeta())
			m_map_metadata_changed = false;
	}

	// Profile modified reasons
	Profiler modprofiler;

	u32 block_count = 0;
	u32 block_count_all = 0; // Number of blocks in memory

	// Don't do anything with sqlite unless something is really saved
	bool save_started = false;
	u32 n = 0, calls = 0;
	const auto end_ms = porting::getTimeMs() + u32(1000 * dedicated_server_step);
	if (!breakable)
		m_blocks_save_last = 0;

	MAP_NOTHREAD_LOCK(this);

	{
		auto lock =
				breakable ? m_blocks.try_lock_shared_rec() : m_blocks.lock_shared_rec();
		if (!lock->owns_lock())
			return m_blocks_save_last;

		for (const auto &[pos, block] : m_blocks) {
			if (n++ < m_blocks_save_last)
				continue;
			else
				m_blocks_save_last = 0;
			++calls;

			if (!block)
				continue;

			block_count_all++;

			if (block->getModified() >= (u32)save_level) {
				// Lazy beginSave()
				if (!save_started) {
					beginSave();
					save_started = true;
				}

				//modprofiler.add(block->getModifiedReasonString(), 1);

				const auto lock = breakable ? block->try_lock_unique_rec()
									  : block->lock_unique_rec();
				if (!lock->owns_lock())
					continue;

				saveBlock(block.get());
				block_count++;
			}
			if (breakable && porting::getTimeMs() > end_ms) {
				m_blocks_save_last = n;
				break;
			}
		}
	}
	if (!calls)
		m_blocks_save_last = 0;

	if (save_started)
		endSave();

	/*
		Only print if something happened or saved whole map
	*/
	if (/*save_level == MOD_STATE_CLEAN
			||*/
			block_count != 0) {
		infostream << "ServerMap: Written: " << block_count << " blocks" << ", "
				   << block_count_all << " blocks in memory."

				   << " Total=" << m_blocks.size() << ".";
		if (m_blocks_save_last)
			infostream << " Break at " << m_blocks_save_last;
		infostream

				<< std::endl;
		PrintInfo(infostream); // ServerMap/ClientMap:
		//infostream<<"Blocks modified by: "<<std::endl;
		modprofiler.print(infostream);
	}

	const auto end_time = porting::getTimeUs();

	m_save_time_counter->increment(end_time - start_time);

	reportMetrics(end_time - start_time, block_count, block_count_all);

	return m_blocks_save_last;
}

int Server::save(float dtime, float dedicated_server_step, bool breakable)
{
	// Save map, players and auth stuff
	int ret = 0;
	float &counter = m_savemap_timer;
	counter += dtime;
	static thread_local const float save_interval =
			g_settings->getFloat("server_map_save_interval");
	if (counter >= save_interval) {
		counter = 0.0;
		TimeTaker timer_step("Server step: Save map, players and auth stuff");
		//MutexAutoLock lock(m_env_mutex);

		ScopeProfiler sp(g_profiler, "Server: map saving (sum)");

		// Save changed parts of map
		if (m_env->getMap().save(
					MOD_STATE_WRITE_NEEDED, dedicated_server_step, breakable)) {
			// partial save, will continue on next step
			counter = g_settings->getFloat("server_map_save_interval");
			++ret;
			if (breakable)
				goto save_break;
		}

		// Save ban file
		if (m_banmanager->isModified()) {
			m_banmanager->save();
		}

		// Save players
		m_env->saveLoadedPlayers();

		// Save environment metadata
		m_env->saveMeta();

		stat.save();
		m_env->blocks_with_abm.save();
	}
save_break:;

	return ret;
}
// Copypaste of isBlockOccluded working without block data

inline core::aabbox3d<bpos_t> getBox(const v3bpos_t &pos)
{
	return core::aabbox3d<bpos_t>(
			pos, pos + v3bpos_t(MAP_BLOCKSIZE, MAP_BLOCKSIZE, MAP_BLOCKSIZE) -
						 v3bpos_t(1, 1, 1));
}

#if 0
bool Map::isBlockOccluded(const v3pos_t &pos, const v3pos_t &cam_pos_nodes)
{
	// Check occlusion for center and all 8 corners of the mapblock
	// Overshoot a little for less flickering
	static const pos_t bs2 = MAP_BLOCKSIZE / 2 + 1;
	static const v3pos_t dir9[9] = {
			v3pos_t(0, 0, 0),
			v3pos_t(1, 1, 1) * bs2,
			v3pos_t(1, 1, -1) * bs2,
			v3pos_t(1, -1, 1) * bs2,
			v3pos_t(1, -1, -1) * bs2,
			v3pos_t(-1, 1, 1) * bs2,
			v3pos_t(-1, 1, -1) * bs2,
			v3pos_t(-1, -1, 1) * bs2,
			v3pos_t(-1, -1, -1) * bs2,
	};

	auto pos_blockcenter = pos + (MAP_BLOCKSIZE / 2);

	// Starting step size, value between 1m and sqrt(3)m
	float step = BS * 1.2f;
	// Multiply step by each iteraction by 'stepfac' to reduce checks in distance
	float stepfac = 1.05f;

	float start_offset = BS * 1.0f;

	// The occlusion search of 'isOccluded()' must stop short of the target
	// point by distance 'end_offset' to not enter the target mapblock.
	// For the 8 mapblock corners 'end_offset' must therefore be the maximum
	// diagonal of a mapblock, because we must consider all view angles.
	// sqrt(1^2 + 1^2 + 1^2) = 1.732
	float end_offset = -BS * MAP_BLOCKSIZE * 1.732f;

	// to reduce the likelihood of falsely occluded blocks
	// require at least two solid blocks
	// this is a HACK, we should think of a more precise algorithm
	u32 needed_count = 2;

	// Additional occlusion check, see comments in that function
	v3pos_t check;
	if (determineAdditionalOcclusionCheck(cam_pos_nodes, getBox(pos), check)) {
		// node is always on a side facing the camera, end_offset can be lower
		if (!isOccluded(cam_pos_nodes, check, step, stepfac, start_offset, -1.0f,
					needed_count))
			return false;
	}

	for (const auto &dir : dir9) {
		if (!isOccluded(cam_pos_nodes, pos_blockcenter + dir, step, stepfac, start_offset,
					end_offset, needed_count))
			return false;
	}
	return true;
}
#endif
