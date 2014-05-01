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
#include "log.h"

MapBlock * Map::getBlockBuffered(v3s16 & p)
{
	MapBlock *block;

	if(m_block_cache != NULL && p == m_block_cache_p){
		return m_block_cache;
	}
	
	// If block doesn't exist, return NULL
	auto n = m_blocks.find(p);
	if(n == m_blocks.end())
	{
		block = NULL;
	}
	// If block exists, return it
	else{
		block = n->second;
	}
	
	// Cache the last result
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
		errorstream<<"Block already created"<<"std::endl";
		return block;
	}
	
	block = new MapBlock(this, p, m_gamedef);
	
	return block;
}

MapBlock * Map::createBlankBlock(v3s16 & p)
{
	MapBlock *block = createBlankBlockNoInsert(p);
	
	m_blocks[p] = block;

	return block;
}

void Map::insertBlock(MapBlock *block)
{
	auto block_p = block->getPos();

	auto block2 = getBlockBuffered(block_p);
	if(block2){
		//throw AlreadyExistsException("Block already exists");
		errorstream<<"Block already exists" /*<PP(block->getPos())*/ <<std::endl;
	}

	// Insert into container
	m_blocks[block_p] = block;
}

void Map::deleteBlock(MapBlock *block)
{
	auto block_p = block->getPos();

	// Clear from cache
	m_block_cache = nullptr;
	
	// Remove from container
	m_blocks.erase(block_p);

	// Delete
	delete block;
}

void Map::getBlocks(std::list<MapBlock*> &dest)
{
	for(auto & bi : m_blocks)
		dest.push_back(bi.second);
}


//END
