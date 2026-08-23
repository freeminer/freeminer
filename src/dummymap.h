// Luanti
// SPDX-License-Identifier: LGPL-2.1-or-later
// Copyright (C) 2022 TurkeyMcMac, Jude Melton-Houghton <jwmhjwmh@gmail.com>

#pragma once

#include "irr_v2d.h"
#include "irr_v3d.h"
#include "irrlichttypes.h"
#include "map.h"
#include "mapsector.h"

class DummyMap : public Map
{
public:
	DummyMap(IGameDef *gamedef, v3bpos_t bpmin, v3bpos_t bpmax): Map(gamedef)
	{
		for (bpos_t z = bpmin.Z; z <= bpmax.Z; z++)
		for (bpos_t x = bpmin.X; x <= bpmax.X; x++) {
			v2bpos_t p2d(x, z);
			MapSector *sector = new MapSector(this, p2d, gamedef);
			m_sectors[p2d] = sector;
			for (auto y = bpmin.Y; y <= bpmax.Y; y++)
				sector->createBlankBlock(y);
		}
	}

	~DummyMap() = default;

	void fill(v3bpos_t bpmin, v3bpos_t bpmax, MapNode n)
	{
		for (bpos_t z = bpmin.Z; z <= bpmax.Z; z++)
		for (bpos_t x = bpmin.X; x <= bpmax.X; x++)
		for (bpos_t y = bpmin.Y; y <= bpmax.Y; y++) {
			MapBlock *block = getBlockNoCreateNoEx({x, y, z});
			if (block) {
				for (s16 zn=0; zn < MAP_BLOCKSIZE; zn++)
				for (s16 yn=0; yn < MAP_BLOCKSIZE; yn++)
				for (s16 xn=0; xn < MAP_BLOCKSIZE; xn++) {
					block->setNodeNoCheck(xn, yn, zn, n);
				}
				block->expireIsAirCache();
			}
		}
	}

	bool maySaveBlocks() override { return false; }
};
