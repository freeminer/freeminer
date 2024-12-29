// Luanti
// SPDX-License-Identifier: LGPL-2.1-or-later
// Copyright (C) 2013 celeron55, Perttu Ahola <celeron55@gmail.com>

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

	MapSector(Map *parent, v2bpos_t pos, IGameDef *gamedef);
	virtual ~MapSector();

	void deleteBlocks();

	v2bpos_t getPos() const
	{
		return m_pos;
	}

	MapBlock *getBlockNoCreateNoEx(bpos_t y);
	std::unique_ptr<MapBlock> createBlankBlockNoInsert(bpos_t y);
	MapBlock *createBlankBlock(bpos_t y);

	void insertBlock(std::unique_ptr<MapBlock> block);

	void deleteBlock(MapBlock *block);

	// Remove a block from the map and the sector without deleting it
	// Returns an owning ptr to block.
	std::unique_ptr<MapBlock> detachBlock(MapBlock *block);

	// This makes a copy of the internal collection.
	// Prefer getBlocks() if possible.
	void getBlocks(MapBlockVect &dest);

	// Get access to the internal collection
	// This is explicitly only allowed on a const object since modifying anything while iterating is unsafe.
	// The caller needs to make sure that this does not happen.
	const auto &getBlocks() const { return m_blocks; }
	const auto &getBlocks() = delete;

	bool empty() const { return m_blocks.empty(); }

	int size() const { return m_blocks.size(); }
protected:

	// The pile of MapBlocks
	std::unordered_map<bpos_t, std::unique_ptr<MapBlock>> m_blocks;

	Map *m_parent;
	// Position on parent (in MapBlock widths)
	v2bpos_t m_pos;

	IGameDef *m_gamedef;

	// Last-used block is cached here for quicker access.
	// Be sure to set this to nullptr when the cached block is deleted
	MapBlock *m_block_cache = nullptr;
	bpos_t m_block_cache_y;

	/*
		Private methods
	*/
	MapBlock *getBlockBuffered(bpos_t y);

};

#endif
