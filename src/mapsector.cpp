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

MapBlock * Map::getBlockBuffered(v3s16 & p)
{
	{
		auto lock = try_shared_lock(m_block_cache_mutex);
		if(m_block_cache && p == m_block_cache_p)
			return m_block_cache;
	}
	
	MapBlock *block;
	{
		auto lock_blocks = m_blocks.lock_shared_rec();
		auto n = m_blocks.find(p);
		if(n == m_blocks.end())
			return nullptr;
		block = n->second;
	}

	auto lock_cache = unique_lock(m_block_cache_mutex);
	m_block_cache_p = p;
	m_block_cache = block;
	return block;
}

MapBlock * Map::getBlockNoCreateNoEx(v3s16 p)
{
	return getBlockBuffered(p);
}

MapBlock * Map::createBlankBlockNoInsert(v3s16 & p)
{
	MapBlock *block = getBlockBuffered(p);
	if (block != NULL) {
		infostream<<"Block already created "<<block->getPos()<<std::endl;
		return block;
	}
	
	block = new MapBlock(this, p, m_gamedef);
	
	return block;
}

MapBlock * Map::createBlankBlock(v3s16 & p)
{
	MapBlock *block = createBlankBlockNoInsert(p);
	
	m_blocks.set(p, block);

	return block;
}

void Map::insertBlock(MapBlock *block)
{
	auto block_p = block->getPos();

	auto block2 = getBlockBuffered(block_p);
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
	auto lock_cache = unique_lock(m_block_cache_mutex);
	m_block_cache = nullptr;
}

//END
