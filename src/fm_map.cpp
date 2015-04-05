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

#include "map.h"
#include "mapblock.h"
#include "log_types.h"
#include "profiler.h"

#include "nodedef.h"
#include "environment.h"
#include "emerge.h"
#include "mg_biome.h"
#include "gamedef.h"
#include "util/directiontables.h"


#if HAVE_THREAD_LOCAL
thread_local MapBlockP m_block_cache = nullptr;
thread_local v3POS m_block_cache_p;
#endif

//TODO: REMOVE THIS func and use Map::getBlock
MapBlock* Map::getBlockNoCreateNoEx(v3POS p, bool trylock, bool nocache) {

#ifndef NDEBUG
	ScopeProfiler sp(g_profiler, "Map: getBlock");
#endif

#if !ENABLE_THREADS
	nocache = true; //very dirty hack. fix and remove. Also compare speed: no cache and cache with lock
#endif

	if (!nocache) {
#if ENABLE_THREADS && !HAVE_THREAD_LOCAL
		auto lock = try_shared_lock(m_block_cache_mutex, TRY_TO_LOCK);
		if(lock.owns_lock())
#endif
			if(m_block_cache && p == m_block_cache_p) {
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
		auto n = m_blocks.find(p);
		if(n == m_blocks.end())
			return nullptr;
		block = n->second;
	}

	if (!nocache) {
#if ENABLE_THREADS && !HAVE_THREAD_LOCAL
		auto lock = unique_lock(m_block_cache_mutex, TRY_TO_LOCK);
		if(lock.owns_lock())
#endif
		{
			m_block_cache_p = p;
			m_block_cache = block;
		}
	}

	return block;
}

MapBlockP Map::getBlock(v3POS p, bool trylock, bool nocache) {
	return getBlockNoCreateNoEx(p, trylock, nocache);
}

void Map::getBlockCacheFlush() {
#if ENABLE_THREADS && !HAVE_THREAD_LOCAL
	auto lock = unique_lock(m_block_cache_mutex);
#endif
	m_block_cache = nullptr;
}

MapBlock * Map::createBlankBlockNoInsert(v3POS & p) {
	auto block = new MapBlock(this, p, m_gamedef);
	return block;
}

MapBlock * Map::createBlankBlock(v3POS & p) {
	auto lock = m_blocks.lock_unique_rec();
	MapBlock *block = getBlockNoCreateNoEx(p, false, true);
	if (block != NULL) {
		infostream << "Block already created p=" << block->getPos() << std::endl;
		return block;
	}

	block = createBlankBlockNoInsert(p);

	m_blocks.set(p, block);

	return block;
}

bool Map::insertBlock(MapBlock *block) {
	auto block_p = block->getPos();

	auto lock = m_blocks.lock_unique_rec();

	auto block2 = getBlockNoCreateNoEx(block_p, false, true);
	if(block2) {
		//throw AlreadyExistsException("Block already exists");
		infostream << "Block already exists " << block_p << std::endl;
		return false;
	}

	// Insert into container
	m_blocks.set(block_p, block);
	return true;
}

void Map::deleteBlock(MapBlockP block) {
	auto block_p = block->getPos();
	(*m_blocks_delete)[block] = 1;
	m_blocks.erase(block_p);
#if ENABLE_THREADS && !HAVE_THREAD_LOCAL
	auto lock = unique_lock(m_block_cache_mutex);
#endif
	m_block_cache = nullptr;
}

MapNode Map::getNodeTry(v3POS p) {
#ifndef NDEBUG
	ScopeProfiler sp(g_profiler, "Map: getNodeTry");
#endif
	auto blockpos = getNodeBlockPos(p);
	auto block = getBlockNoCreateNoEx(blockpos, true);
	if(!block)
		return MapNode(CONTENT_IGNORE);
	auto relpos = p - blockpos * MAP_BLOCKSIZE;
	return block->getNodeTry(relpos);
}

/*
MapNode Map::getNodeLog(v3POS p){
	auto blockpos = getNodeBlockPos(p);
	auto block = getBlockNoCreateNoEx(blockpos);
	v3s16 relpos = p - blockpos*MAP_BLOCKSIZE;
	auto node = block->getNodeNoEx(relpos);
	infostream<<"getNodeLog("<<p<<") blockpos="<<blockpos<<" block="<<block<<" relpos="<<relpos<<" n="<<node<<std::endl;
	return node;
}
*/

/*
MapNode Map::getNodeNoLock(v3s16 p) //dont use
{
	v3s16 blockpos = getNodeBlockPos(p);
	MapBlock *block = getBlockNoCreateNoEx(blockpos);
	if(block == NULL)
		return MapNode(CONTENT_IGNORE);
	return block->getNodeNoLock(p - blockpos*MAP_BLOCKSIZE);
}
*/
v3POS Map::transforming_liquid_pop() {
	std::lock_guard<std::mutex> lock(m_transforming_liquid_mutex);
	auto front = m_transforming_liquid.front();
	m_transforming_liquid.pop_front();
	return front;

	//auto lock = m_transforming_liquid.lock_unique_rec();
	//auto it = m_transforming_liquid.begin();
	//auto value = it->first;
	//m_transforming_liquid.erase(it);
	//return value;
}

s16 Map::getHeat(v3s16 p, bool no_random) {
	MapBlock *block = getBlockNoCreateNoEx(getNodeBlockPos(p));
	if(block != NULL) {
		s16 value = block->heat;
		return value + (no_random ? 0 : myrand_range(0, 1));
	}
	//errorstream << "No heat for " << p.X<<"," << p.Z << std::endl;
	return 0;
}

s16 Map::getHumidity(v3s16 p, bool no_random) {
	MapBlock *block = getBlockNoCreateNoEx(getNodeBlockPos(p));
	if(block != NULL) {
		s16 value = block->humidity;
		return value + (no_random ? 0 : myrand_range(0, 1));
	}
	//errorstream << "No humidity for " << p.X<<"," << p.Z << std::endl;
	return 0;
}




s16 ServerMap::updateBlockHeat(ServerEnvironment *env, v3POS p, MapBlock *block, std::map<v3POS, s16> * cache) {
	auto bp = getNodeBlockPos(p);
	auto gametime = env->getGameTime();
	if (block) {
		if (gametime < block->heat_last_update)
			return block->heat + myrand_range(0, 1);
	} else if (!cache) {
		block = getBlockNoCreateNoEx(bp, true);
	}
	if (cache && cache->count(bp))
		return cache->at(bp) + myrand_range(0, 1);

	auto value = m_emerge->biomemgr->calcBlockHeat(p, getSeed(),
	             env->getTimeOfDayF(), gametime * env->getTimeOfDaySpeed(), env->m_use_weather);

	if(block) {
		block->heat = value;
		block->heat_last_update = env->m_use_weather ? gametime + 30 : -1;
	}
	if (cache)
		(*cache)[bp] = value;
	return value + myrand_range(0, 1);
}

s16 ServerMap::updateBlockHumidity(ServerEnvironment *env, v3POS p, MapBlock *block, std::map<v3POS, s16> * cache) {
	auto bp = getNodeBlockPos(p);
	auto gametime = env->getGameTime();
	if (block) {
		if (gametime < block->humidity_last_update)
			return block->humidity + myrand_range(0, 1);
	} else if (!cache) {
		block = getBlockNoCreateNoEx(bp, true);
	}
	if (cache && cache->count(bp))
		return cache->at(bp) + myrand_range(0, 1);

	auto value = m_emerge->biomemgr->calcBlockHumidity(p, getSeed(),
	             env->getTimeOfDayF(), gametime * env->getTimeOfDaySpeed(), env->m_use_weather);

	if(block) {
		block->humidity = value;
		block->humidity_last_update = env->m_use_weather ? gametime + 30 : -1;
	}
	if (cache)
		(*cache)[bp] = value;
	return value + myrand_range(0, 1);
}

int ServerMap::getSurface(v3s16 basepos, int searchup, bool walkable_only) {

	s16 max = MYMIN(searchup + basepos.Y, 0x7FFF);

	MapNode last_node = getNodeNoEx(basepos);
	MapNode node = last_node;
	v3s16 runpos = basepos;
	INodeDefManager *nodemgr = m_gamedef->ndef();

	bool last_was_walkable = nodemgr->get(node).walkable;

	while ((runpos.Y < max) && (node.param0 != CONTENT_AIR)) {
		runpos.Y += 1;
		last_node = node;
		node = getNodeNoEx(runpos);

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


INodeDefManager* Map::getNodeDefManager() {
	return m_gamedef->ndef();
}

void Map::copy_27_blocks_to_vm(MapBlock * block, VoxelManipulator & vmanip) {

	v3POS blockpos = block->getPos();
	v3POS blockpos_nodes = blockpos*MAP_BLOCKSIZE;

	// Allocate this block + neighbors
	vmanip.clear();
	VoxelArea voxel_area(blockpos_nodes - v3POS(1,1,1) * MAP_BLOCKSIZE,
			blockpos_nodes + v3POS(1,1,1) * MAP_BLOCKSIZE*2-v3POS(1,1,1));
	vmanip.addArea(voxel_area);

	block->copyTo(vmanip);

	auto * map = block->getParent();

	for(u16 i=0; i<26; i++) {
		v3POS bp = blockpos + g_26dirs[i];
		MapBlock *b = map->getBlockNoCreateNoEx(bp);
		if(b)
			b->copyTo(vmanip);
	}

}

