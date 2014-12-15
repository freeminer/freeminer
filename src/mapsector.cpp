/*
mapsector.cpp
Copyright (C) 2013 celeron55, Perttu Ahola <celeron55@gmail.com>
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

#include "map.h"
#include "mapblock.h"
#include "log_types.h"
#include "util/lock.h"

//#include "config.h"
#include "profiler.h"
//#include "porting.h"

#if CMAKE_HAVE_THREAD_LOCAL
thread_local MapBlockP m_block_cache = nullptr;
thread_local v3POS m_block_cache_p;
#endif

//TODO: REMOVE THIS func and use Map::getBlock
MapBlock* Map::getBlockNoCreateNoEx(v3POS p, bool trylock, bool nocache)
{
#ifndef NDEBUG
	ScopeProfiler sp(g_profiler, "Map: getBlock");
#endif

	if (!nocache) {
#if CMAKE_THREADS && !CMAKE_HAVE_THREAD_LOCAL
		auto lock = try_shared_lock(m_block_cache_mutex, TRY_TO_LOCK);
		if(lock.owns_lock())
#endif
		if(m_block_cache && p == m_block_cache_p) {
#ifndef NDEBUG
			g_profiler->add("Map: getBlock cache hit", 1);
#endif
			return m_block_cache.get();
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
#if CMAKE_THREADS && !CMAKE_HAVE_THREAD_LOCAL
		auto lock = unique_lock(m_block_cache_mutex, TRY_TO_LOCK);
		if(lock.owns_lock())
#endif
		{
			m_block_cache_p = p;
			m_block_cache = block;
		}
	}

	return block.get();
}

MapBlockP Map::getBlock(v3POS p, bool trylock, bool nocache)
{
#ifndef NDEBUG
	ScopeProfiler sp(g_profiler, "Map: getBlock");
#endif

	if (!nocache) {
#if CMAKE_THREADS && !CMAKE_HAVE_THREAD_LOCAL
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
#if CMAKE_THREADS && !CMAKE_HAVE_THREAD_LOCAL
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

MapBlock * Map::createBlankBlockNoInsert(v3POS & p)
{
	auto block = new MapBlock(this, p, m_gamedef);
	return block;
}

MapBlock * Map::createBlankBlock(v3POS & p)
{
	auto lock = m_blocks.lock_unique_rec();
	MapBlock *block = getBlockNoCreateNoEx(p, false, true);
	if (block != NULL) {
		infostream<<"Block already created p="<<block->getPos()<<std::endl;
		return block;
	}

	block = createBlankBlockNoInsert(p);
	
	m_blocks.set(p, MapBlockP(block));

	return block;
}

void Map::insertBlock(MapBlock *block)
{
	auto block_p = block->getPos();

	auto block2 = getBlockNoCreateNoEx(block_p, false, true);
	if(block2){
		//throw AlreadyExistsException("Block already exists");
		infostream<<"Block already exists " << block_p <<std::endl;
		return; // memory leak, but very rare|impossible
	}

	// Insert into container
	m_blocks.set(block_p, MapBlockP(block));
}

void Map::deleteBlock(MapBlockP block)
{
	auto block_p = block->getPos();
	(*m_blocks_delete)[block] = 1;
	m_blocks.erase(block_p);
#if CMAKE_THREADS && !CMAKE_HAVE_THREAD_LOCAL
	auto lock = unique_lock(m_block_cache_mutex);
#endif
	m_block_cache = nullptr;
}

//END
