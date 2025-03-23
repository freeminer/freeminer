// Luanti
// SPDX-License-Identifier: LGPL-2.1-or-later
// Copyright (C) 2022 TurkeyMcMac, Jude Melton-Houghton <jwmhjwmh@gmail.com>

#pragma once

#include "map.h"
#include "mapsector.h"

class DummyMap : public Map
{
public:
	DummyMap(IGameDef *gamedef, v3s16 bpmin, v3s16 bpmax): Map(gamedef)
	{
		for (s16 z = bpmin.Z; z <= bpmax.Z; z++)
		for (s16 x = bpmin.X; x <= bpmax.X; x++) {
			//v2s16 p2d(x, z);
			//MapSector *sector = new MapSector(this, p2d, gamedef);
			//m_sectors[p2d] = sector;
			for (s16 y = bpmin.Y; y <= bpmax.Y; y++)
				createBlankBlock({x,y,z});
		}
	}

	~DummyMap() = default;

	void fill(v3s16 bpmin, v3s16 bpmax, MapNode n)
	{
		for (s16 z = bpmin.Z; z <= bpmax.Z; z++)
		for (s16 y = bpmin.Y; y <= bpmax.Y; y++)
		for (s16 x = bpmin.X; x <= bpmax.X; x++) {
			MapBlock *block = getBlockNoCreateNoEx({x, y, z});
			if (block) {
				for (size_t i = 0; i < MapBlock::nodecount; i++)
					block->getData()[i] = n;
				block->expireIsAirCache();
			}
		}
	}

	bool maySaveBlocks() override { return false; }
};
