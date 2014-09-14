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

//#include "main.h"
//#include "profiler.h"

#if _MSC_VER
#define THREAD_LOCAL __declspec(thread)
#elif defined(__GNUC__) && ((__GNUC__*100 + __GNUC_MINOR__) < 407)
try_shared_mutex m_block_cache_mutex;
#define NO_THREAD_LOCAL
#define THREAD_LOCAL
#else
#define THREAD_LOCAL thread_local
#endif

THREAD_LOCAL MapBlock *m_block_cache = nullptr;
THREAD_LOCAL v3s16 m_block_cache_p;

MapBlock * Map::getBlockNoCreateNoEx(v3s16 p, bool trylock)
{
	//ScopeProfiler sp(g_profiler, "Map: getBlockBuffered");
	{
#ifdef NO_THREAD_LOCAL
		auto lock = try_shared_lock(m_block_cache_mutex, TRY_TO_LOCK);
		if(lock.owns_lock())
#endif
		if(m_block_cache && p == m_block_cache_p) {
			//g_profiler->add("Map: getBlockBuffered cache hit", 1);
			return m_block_cache;
		}
	}

	MapBlock *block;
	{
		auto lock = trylock ? m_blocks.try_lock_shared_rec() : m_blocks.lock_shared_rec();
		if (!lock->owns_lock())
			return nullptr;
		auto n = m_blocks.find(p);
		if(n == m_blocks.end())
			return nullptr;
		block = n->second;
	}

#ifdef NO_THREAD_LOCAL
		auto lock = unique_lock(m_block_cache_mutex, TRY_TO_LOCK);
		if(lock.owns_lock())
#endif
	{
		m_block_cache_p = p;
		m_block_cache = block;
	}

	return block;
}

MapBlock * Map::createBlankBlockNoInsert(v3s16 & p)
{
	auto block = new MapBlock(this, p, m_gamedef);
	return block;
}

MapBlock * Map::createBlankBlock(v3s16 & p)
{
	MapBlock *block = getBlockNoCreateNoEx(p);
	if (block != NULL) {
		infostream<<"Block already created p="<<block->getPos()<<std::endl;
		return block;
	}

	block = createBlankBlockNoInsert(p);
	
	m_blocks.set(p, block);

	return block;
}

void Map::insertBlock(MapBlock *block)
{
	auto block_p = block->getPos();

	auto block2 = getBlockNoCreateNoEx(block_p);
	if(block2){
		//throw AlreadyExistsException("Block already exists");
		infostream<<"Block already exists " << block_p <<std::endl;
		return; // memory leak, but very rare|impossible
	}

	// Insert into container
	m_blocks.set(block_p, block);
}

void Map::deleteBlock(MapBlock *block, bool now)
{
	auto block_p = block->getPos();
	if (now)
		delete block;
	else
		(*m_blocks_delete)[block] = 1;
	m_blocks.erase(block_p);
	m_block_cache = nullptr;
}

//END
