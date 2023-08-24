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
#include "irr_v3d.h"
#include "map.h"
#include "mapblock.h"
#include "log_types.h"
#include "profiler.h"

#include "nodedef.h"
#include "environment.h"
#include "emerge.h"
#include "mapgen/mg_biome.h"
#include "gamedef.h"
#include "util/directiontables.h"
#include "serverenvironment.h"
#include "voxelalgorithms.h"

#if HAVE_THREAD_LOCAL
thread_local MapBlockP m_block_cache = nullptr;
thread_local v3pos_t m_block_cache_p;
#endif

// TODO: REMOVE THIS func and use Map::getBlock
MapBlock *Map::getBlockNoCreateNoEx(v3bpos_t p, bool trylock, bool nocache)
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
		auto lock = try_shared_lock(m_block_cache_mutex, try_to_lock);
		if (lock.owns_lock())
#endif
			if (m_block_cache && p == m_block_cache_p) {
#ifndef NDEBUG
				g_profiler->add("Map: getBlock cache hit", 1);
#endif
				return m_block_cache;
			}
	}

	MapBlockP block;
	{
		auto lock = trylock ? m_blocks.try_lock_shared_rec() : m_blocks.lock_shared_rec();
		if (!lock->owns_lock())
			return nullptr;
		const auto &n = m_blocks.find(p);
		if (n == m_blocks.end())
			return nullptr;
		block = n->second;
	}

	if (!nocache) {
#if ENABLE_THREADS && !HAVE_THREAD_LOCAL
		auto lock = unique_lock(m_block_cache_mutex, try_to_lock);
		if (lock.owns_lock())
#endif
		{
			m_block_cache_p = p;
			m_block_cache = block;
		}
	}

	return block;
}

MapBlockP Map::getBlock(v3pos_t p, bool trylock, bool nocache)
{
	return getBlockNoCreateNoEx(p, trylock, nocache);
}

void Map::getBlockCacheFlush()
{
#if ENABLE_THREADS && !HAVE_THREAD_LOCAL
	auto lock = unique_lock(m_block_cache_mutex);
#endif
	m_block_cache = nullptr;
}

MapBlock *Map::createBlankBlockNoInsert(const v3pos_t &p)
{
	auto block = new MapBlock(this, p, m_gamedef);
	return block;
}

MapBlock *Map::createBlankBlock(const v3pos_t &p)
{
	m_db_miss.erase(p);

	auto lock = m_blocks.lock_unique_rec();
	MapBlock *block = getBlockNoCreateNoEx(p, false, true);
	if (block != NULL) {
		infostream << "Block already created p=" << block->getPos() << std::endl;
		return block;
	}

	block = createBlankBlockNoInsert(p);

	m_blocks.insert_or_assign(p, block);

	return block;
}

bool Map::insertBlock(MapBlock *block)
{
	auto block_p = block->getPos();

	m_db_miss.erase(block_p);

	auto lock = m_blocks.lock_unique_rec();

	auto block2 = getBlockNoCreateNoEx(block_p, false, true);
	if (block2) {
		verbosestream << "Block already exists " << block_p << std::endl;
		return false;
	}

	// Insert into container
	m_blocks.insert_or_assign(block_p, block);
	return true;
}

MapBlock *ServerMap::createBlock(v3pos_t p)
{
	if (MapBlock *block = getBlockNoCreateNoEx(p, false, true)) {
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

void Map::eraseBlock(const MapBlockP block)
{
	auto block_p = block->getPos();
	(*m_blocks_delete)[block] = 1;
	m_blocks.erase(block_p);
#if ENABLE_THREADS && !HAVE_THREAD_LOCAL
	auto lock = unique_lock(m_block_cache_mutex);
#endif
	m_block_cache = nullptr;
}

MapNode Map::getNodeTry(const v3pos_t &p)
{
#ifndef NDEBUG
	ScopeProfiler sp(g_profiler, "Map: getNodeTry");
#endif
	auto blockpos = getNodeBlockPos(p);
	auto block = getBlockNoCreateNoEx(blockpos, true);
	if (!block)
		return MapNode(CONTENT_IGNORE);
	auto relpos = p - blockpos * MAP_BLOCKSIZE;
	return block->getNodeTry(relpos);
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

	auto *map = block->getParent();

	for (u16 i = 0; i < 26; i++) {
		v3pos_t bp = blockpos + g_26dirs[i];
		MapBlock *b = map->getBlockNoCreateNoEx(bp);
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
		if (m_blocks_delete->size())
			verbosestream << "Deleting blocks=" << m_blocks_delete->size() << std::endl;
		for (auto &ir : *m_blocks_delete) {
			delete ir.first;
		}
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

	std::vector<MapBlockP> blocks_delete;
	int save_started = 0;
	{
		auto lock = m_blocks.try_lock_shared_rec();
		if (!lock->owns_lock()) {
			return m_blocks_update_last;
		}

#if !ENABLE_THREADS
		auto lock_map = m_nothread_locker.try_lock_unique_rec();
		if (!lock_map->owns_lock())
			return m_blocks_update_last;
#endif

		auto m_blocks_size = m_blocks.size();

		for (auto ir : m_blocks) {
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

			if (block->refGet()) {
				continue;
			}

			if (!block->isGenerated()) {
				blocks_delete.emplace_back(block);
				continue;
			}

			{
				auto lock = block->try_lock_unique_rec();
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
						if (!saveBlock(block)) {
							continue;
						}
						saved_blocks_count++;
					}

					blocks_delete.emplace_back(block);

					if (unloaded_blocks)
						unloaded_blocks->push_back(p);

					deleted_blocks_count++;
				} else {

#if BUILD_CLIENT
					if (block->mesh_old)
						block->mesh_old = nullptr;
#endif

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
				block->setLightingExpired(true);

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
			auto lock = m_nothread_locker.try_lock_shared_rec();
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

		// auto lock = block->try_lock_unique_rec();
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
				auto lock = block->try_lock_unique_rec();
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
		const v3pos_t &pos, std::set<v3pos_t> &light_sources, bool remove_light)
{
	MapBlock *block = getBlockNoCreateNoEx(pos);

	// auto lock = block->lock_unique_rec(); //no: in block_below_is_valid getnode outside
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
