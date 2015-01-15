/*
mapgen_singlenode.cpp
Copyright (C) 2010-2013 celeron55, Perttu Ahola <celeron55@gmail.com>
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

#include "mapgen_singlenode.h"
#include "voxel.h"
#include "mapblock.h"
#include "mapnode.h"
#include "map.h"
#include "nodedef.h"
#include "voxelalgorithms.h"
#include "profiler.h"
#include "emerge.h"

//////////////////////// Mapgen Singlenode parameter read/write

void MapgenSinglenodeParams::readParams(Settings *settings)
{
}


void MapgenSinglenodeParams::writeParams(Settings *settings)
{
}

///////////////////////////////////////////////////////////////////////////////

MapgenSinglenode::MapgenSinglenode(int mapgenid,
	MapgenParams *params, EmergeManager *emerge)
	: Mapgen(mapgenid, params, emerge)
{
	flags = params->flags;

	INodeDefManager *ndef = emerge->ndef;

	c_node = ndef->getId("mapgen_singlenode");
	if (c_node == CONTENT_IGNORE)
		c_node = CONTENT_AIR;
}


MapgenSinglenode::~MapgenSinglenode()
{
}

//////////////////////// Map generator

void MapgenSinglenode::makeChunk(BlockMakeData *data)
{
	assert(data->vmanip);
	assert(data->nodedef);
	assert(data->blockpos_requested.X >= data->blockpos_min.X &&
		   data->blockpos_requested.Y >= data->blockpos_min.Y &&
		   data->blockpos_requested.Z >= data->blockpos_min.Z);
	assert(data->blockpos_requested.X <= data->blockpos_max.X &&
		   data->blockpos_requested.Y <= data->blockpos_max.Y &&
		   data->blockpos_requested.Z <= data->blockpos_max.Z);

	this->generating = true;
	this->vm   = data->vmanip;
	this->ndef = data->nodedef;

	v3s16 blockpos_min = data->blockpos_min;
	v3s16 blockpos_max = data->blockpos_max;

	// Area of central chunk
	v3s16 node_min = blockpos_min*MAP_BLOCKSIZE;
	v3s16 node_max = (blockpos_max+v3s16(1,1,1))*MAP_BLOCKSIZE-v3s16(1,1,1);

	blockseed = getBlockSeed2(node_min, data->seed);

	MapNode n_node(c_node);

	for (s16 z = node_min.Z; z <= node_max.Z; z++)
	for (s16 y = node_min.Y; y <= node_max.Y; y++) {
		u32 i = vm->m_area.index(node_min.X, y, z);
		for (s16 x = node_min.X; x <= node_max.X; x++) {
			if (vm->m_data[i].getContent() == CONTENT_IGNORE)
				vm->m_data[i] = n_node;
			i++;
		}
	}

	// Add top and bottom side of water to transforming_liquid queue
	updateLiquid(node_min, node_max);

	// Calculate lighting
	if (flags & MG_LIGHT)
		calcLighting(node_min, node_max);

	this->generating = false;
}

int MapgenSinglenode::getGroundLevelAtPoint(v2s16 p)
{
	return 0;
}

