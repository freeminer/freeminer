/*
mapsector.h
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

#pragma once

#if WTF

#include "irrlichttypes.h"
#include "irr_v2d.h"
#include "mapblock.h"
#include <ostream>
#include <map>
#include <vector>

class Map;
class IGameDef;

/*
	This is an Y-wise stack of MapBlocks.
*/

#define MAPSECTOR_SERVER 0
#define MAPSECTOR_CLIENT 1

class MapSector
{
public:

	MapSector(Map *parent, v2s16 pos, IGameDef *gamedef);
	virtual ~MapSector();

	void deleteBlocks();

	v2s16 getPos()
	{
		return m_pos;
	}

	MapBlock *getBlockNoCreateNoEx(s16 y);
	std::unique_ptr<MapBlock> createBlankBlockNoInsert(s16 y);
	MapBlock *createBlankBlock(s16 y);

	void insertBlock(std::unique_ptr<MapBlock> block);

	void deleteBlock(MapBlock *block);

	// Remove a block from the map and the sector without deleting it
	// Returns an owning ptr to block.
	std::unique_ptr<MapBlock> detachBlock(MapBlock *block);

	void getBlocks(MapBlockVect &dest);

	bool empty() const { return m_blocks.empty(); }

	int size() const { return m_blocks.size(); }
protected:

	// The pile of MapBlocks
	std::unordered_map<s16, std::unique_ptr<MapBlock>> m_blocks;

	Map *m_parent;
	// Position on parent (in MapBlock widths)
	v2s16 m_pos;

	IGameDef *m_gamedef;

	// Last-used block is cached here for quicker access.
	// Be sure to set this to nullptr when the cached block is deleted
	MapBlock *m_block_cache = nullptr;
	s16 m_block_cache_y;

	/*
		Private methods
	*/
	MapBlock *getBlockBuffered(s16 y);

};

#endif
