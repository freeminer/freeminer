/*
map.cpp
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

#include "map.h"
#include "mapsector.h"
#include "mapblock.h"
/*#ifndef SERVER
	#include "mapblock_mesh.h"
#endif*/
#include "main.h"
#include "filesys.h"
#include "voxel.h"
#include "porting.h"
#include "serialization.h"
#include "nodemetadata.h"
#include "settings.h"
#include "log.h"
#include "profiler.h"
#include "nodedef.h"
#include "gamedef.h"
#include "util/directiontables.h"
#include "util/mathconstants.h"
#include "rollback_interface.h"
#include "environment.h"
#include "emerge.h"
#include "mapgen_v6.h"
#include "biome.h"
#include "config.h"
#include "server.h"
#include "database.h"
#include "database-dummy.h"
#include "database-sqlite3.h"
#include "circuit.h"
#include "scripting_game.h"
#if USE_LEVELDB
#include "database-leveldb.h"
#endif
#if USE_REDIS
#include "database-redis.h"
#endif

#define PP(x) "("<<(x).X<<","<<(x).Y<<","<<(x).Z<<")"

/*
	SQLite format specification:
	- Initially only replaces sectors/ and sectors2/

	If map.sqlite does not exist in the save dir
	or the block was not found in the database
	the map will try to load from sectors folder.
	In either case, map.sqlite will be created
	and all future saves will save there.

	Structure of map.sqlite:
	Tables:
		blocks
			(PK) INT pos
			BLOB data
*/

/*
	Map
*/

Map::Map(std::ostream &dout, IGameDef *gamedef):
	m_liquid_step_flow(1000),
	m_dout(dout),
	m_gamedef(gamedef),
	m_sectors_update_last(0),
	m_sectors_save_last(0),
	m_sector_cache(NULL)
{
	updateLighting_last[LIGHTBANK_DAY] = updateLighting_last[LIGHTBANK_NIGHT] = 0;
	m_circuit = NULL;
}

// TODO: mmerge with ^^ with curcuit=NULL
Map::Map(std::ostream &dout, IGameDef *gamedef, Circuit* circuit):
	m_liquid_step_flow(1000),
	m_dout(dout),
	m_gamedef(gamedef),
	m_circuit(circuit),
	m_sectors_update_last(0),
	m_sectors_save_last(0),
	m_sector_cache(NULL)
{
	updateLighting_last[LIGHTBANK_DAY] = updateLighting_last[LIGHTBANK_NIGHT] = 0;
}

Map::~Map()
{
	/*
		Free all MapSectors
	*/
	for(std::map<v2s16, MapSector*>::iterator i = m_sectors.begin();
		i != m_sectors.end(); ++i)
	{
		delete i->second;
	}
}

void Map::addEventReceiver(MapEventReceiver *event_receiver)
{
	m_event_receivers.insert(event_receiver);
}

void Map::removeEventReceiver(MapEventReceiver *event_receiver)
{
	m_event_receivers.erase(event_receiver);
}

void Map::dispatchEvent(MapEditEvent *event)
{
	for(std::set<MapEventReceiver*>::iterator
			i = m_event_receivers.begin();
			i != m_event_receivers.end(); ++i)
	{
		(*i)->onMapEditEvent(event);
	}
}

MapSector * Map::getSectorNoGenerateNoExNoLock(v2s16 p)
{
	if(m_sector_cache != NULL && p == m_sector_cache_p){
		MapSector * sector = m_sector_cache;
		return sector;
	}

	std::map<v2s16, MapSector*>::iterator n = m_sectors.find(p);

	if(n == m_sectors.end())
		return NULL;

	MapSector *sector = n->second;

	// Cache the last result
	m_sector_cache_p = p;
	m_sector_cache = sector;

	return sector;
}

MapSector * Map::getSectorNoGenerateNoEx(v2s16 p)
{
	return getSectorNoGenerateNoExNoLock(p);
}

MapSector * Map::getSectorNoGenerate(v2s16 p)
{
	MapSector *sector = getSectorNoGenerateNoEx(p);
	if(sector == NULL)
		throw InvalidPositionException();

	return sector;
}

MapBlock * Map::getBlockNoCreateNoEx(v3s16 p3d)
{
	v2s16 p2d(p3d.X, p3d.Z);
	MapSector * sector = getSectorNoGenerateNoEx(p2d);
	if(sector == NULL)
		return NULL;
	MapBlock *block = sector->getBlockNoCreateNoEx(p3d.Y);
	return block;
}

MapBlock * Map::getBlockNoCreate(v3s16 p3d)
{
	MapBlock *block = getBlockNoCreateNoEx(p3d);
	if(block == NULL)
		throw InvalidPositionException();
	return block;
}

bool Map::isNodeUnderground(v3s16 p)
{
	v3s16 blockpos = getNodeBlockPos(p);
	try{
		MapBlock * block = getBlockNoCreate(blockpos);
		return block->getIsUnderground();
	}
	catch(InvalidPositionException &e)
	{
		return false;
	}
}

bool Map::isValidPosition(v3s16 p)
{
	v3s16 blockpos = getNodeBlockPos(p);
	MapBlock *block = getBlockNoCreate(blockpos);
	return (block != NULL);
}

// Returns a CONTENT_IGNORE node if not found
MapNode Map::getNodeNoEx(v3s16 p)
{
	v3s16 blockpos = getNodeBlockPos(p);
	MapBlock *block = getBlockNoCreateNoEx(blockpos);
	if(block == NULL)
		return MapNode(CONTENT_IGNORE);
	v3s16 relpos = p - blockpos*MAP_BLOCKSIZE;
	return block->getNodeNoCheck(relpos);
}

// throws InvalidPositionException if not found
MapNode Map::getNode(v3s16 p)
{
	v3s16 blockpos = getNodeBlockPos(p);
	MapBlock *block = getBlockNoCreateNoEx(blockpos);
	if(block == NULL)
		throw InvalidPositionException();
	v3s16 relpos = p - blockpos*MAP_BLOCKSIZE;
	return block->getNodeNoCheck(relpos);
}

// throws InvalidPositionException if not found
void Map::setNode(v3s16 p, MapNode & n)
{
	v3s16 blockpos = getNodeBlockPos(p);
	MapBlock *block = getBlockNoCreate(blockpos);
	v3s16 relpos = p - blockpos*MAP_BLOCKSIZE;
	// Never allow placing CONTENT_IGNORE, it fucks up stuff
	if(n.getContent() == CONTENT_IGNORE){
		errorstream<<"Map::setNode(): Not allowing to place CONTENT_IGNORE"
				<<" while trying to replace \""
				<<m_gamedef->ndef()->get(block->getNodeNoCheck(relpos)).name
				<<"\" at "<<PP(p)<<" (block "<<PP(blockpos)<<")"<<std::endl;
		debug_stacks_print_to(infostream);
		return;
	}
	block->setNodeNoCheck(relpos, n);
}


/*
	Goes recursively through the neighbours of the node.

	Alters only transparent nodes.

	If the lighting of the neighbour is lower than the lighting of
	the node was (before changing it to 0 at the step before), the
	lighting of the neighbour is set to 0 and then the same stuff
	repeats for the neighbour.

	The ending nodes of the routine are stored in light_sources.
	This is useful when a light is removed. In such case, this
	routine can be called for the light node and then again for
	light_sources to re-light the area without the removed light.

	values of from_nodes are lighting values.
*/
void Map::unspreadLight(enum LightBank bank,
		std::map<v3s16, u8> & from_nodes,
		std::set<v3s16> & light_sources,
		std::map<v3s16, MapBlock*>  & modified_blocks)
{
	INodeDefManager *nodemgr = m_gamedef->ndef();

	v3s16 dirs[6] = {
		v3s16(0,0,1), // back
		v3s16(0,1,0), // top
		v3s16(1,0,0), // right
		v3s16(0,0,-1), // front
		v3s16(0,-1,0), // bottom
		v3s16(-1,0,0), // left
	};

	if(from_nodes.size() == 0)
		return;

	u32 blockchangecount = 0;

	std::map<v3s16, u8> unlighted_nodes;

	/*
		Initialize block cache
	*/
	v3s16 blockpos_last;
	MapBlock *block = NULL;
	// Cache this a bit, too
	bool block_checked_in_modified = false;

	for(std::map<v3s16, u8>::iterator j = from_nodes.begin();
		j != from_nodes.end(); ++j)
	{
		v3s16 pos = j->first;
		v3s16 blockpos = getNodeBlockPos(pos);

		// Only fetch a new block if the block position has changed
		try{
			if(block == NULL || blockpos != blockpos_last){
				block = getBlockNoCreate(blockpos);
				blockpos_last = blockpos;

				block_checked_in_modified = false;
				blockchangecount++;
			}
		}
		catch(InvalidPositionException &e)
		{
			continue;
		}

		if(block->isDummy())
			continue;

		// Calculate relative position in block
		//v3s16 relpos = pos - blockpos_last * MAP_BLOCKSIZE;

		// Get node straight from the block
		//MapNode n = block->getNode(relpos);

		u8 oldlight = j->second;

		// Loop through 6 neighbors
		for(u16 i=0; i<6; i++)
		{
			// Get the position of the neighbor node
			v3s16 n2pos = pos + dirs[i];

			// Get the block where the node is located
			v3s16 blockpos = getNodeBlockPos(n2pos);

			try
			{
				// Only fetch a new block if the block position has changed
				try{
					if(block == NULL || blockpos != blockpos_last){
						block = getBlockNoCreate(blockpos);
						blockpos_last = blockpos;

						block_checked_in_modified = false;
						blockchangecount++;
					}
				}
				catch(InvalidPositionException &e)
				{
					continue;
				}

				// Calculate relative position in block
				v3s16 relpos = n2pos - blockpos * MAP_BLOCKSIZE;
				// Get node straight from the block
				MapNode n2 = block->getNode(relpos);

				bool changed = false;

				//TODO: Optimize output by optimizing light_sources?

				/*
					If the neighbor is dimmer than what was specified
					as oldlight (the light of the previous node)
				*/
				if(n2.getLight(bank, nodemgr) < oldlight)
				{
					/*
						And the neighbor is transparent and it has some light
					*/
					if(nodemgr->get(n2).light_propagates
							&& n2.getLight(bank, nodemgr) != 0)
					{
						/*
							Set light to 0 and add to queue
						*/

						u8 current_light = n2.getLight(bank, nodemgr);
						n2.setLight(bank, 0, nodemgr);
						block->setNode(relpos, n2);

						unlighted_nodes[n2pos] = current_light;
						changed = true;

						/*
							Remove from light_sources if it is there
							NOTE: This doesn't happen nearly at all
						*/
						/*if(light_sources.find(n2pos))
						{
							infostream<<"Removed from light_sources"<<std::endl;
							light_sources.remove(n2pos);
						}*/
					}

					/*// DEBUG
					if(light_sources.find(n2pos) != NULL)
						light_sources.remove(n2pos);*/
				}
				else{
					light_sources.insert(n2pos);
				}

				// Add to modified_blocks
				if(changed == true && block_checked_in_modified == false)
				{
					// If the block is not found in modified_blocks, add.
					if(modified_blocks.find(blockpos) == modified_blocks.end())
					{
						modified_blocks[blockpos] = block;
					}
					block_checked_in_modified = true;
				}
			}
			catch(InvalidPositionException &e)
			{
				continue;
			}
		}
	}

	/*infostream<<"unspreadLight(): Changed block "
			<<blockchangecount<<" times"
			<<" for "<<from_nodes.size()<<" nodes"
			<<std::endl;*/

	if(unlighted_nodes.size() > 0)
		unspreadLight(bank, unlighted_nodes, light_sources, modified_blocks);
}

/*
	A single-node wrapper of the above
*/
void Map::unLightNeighbors(enum LightBank bank,
		v3s16 pos, u8 lightwas,
		std::set<v3s16> & light_sources,
		std::map<v3s16, MapBlock*>  & modified_blocks)
{
	std::map<v3s16, u8> from_nodes;
	from_nodes[pos] = lightwas;

	unspreadLight(bank, from_nodes, light_sources, modified_blocks);
}

/*
	Lights neighbors of from_nodes, collects all them and then
	goes on recursively.
*/
void Map::spreadLight(enum LightBank bank,
		std::set<v3s16> & from_nodes,
		std::map<v3s16, MapBlock*> & modified_blocks)
{
	INodeDefManager *nodemgr = m_gamedef->ndef();

	const v3s16 dirs[6] = {
		v3s16(0,0,1), // back
		v3s16(0,1,0), // top
		v3s16(1,0,0), // right
		v3s16(0,0,-1), // front
		v3s16(0,-1,0), // bottom
		v3s16(-1,0,0), // left
	};

	if(from_nodes.size() == 0)
		return;

	u32 blockchangecount = 0;

	std::set<v3s16> lighted_nodes;

	/*
		Initialize block cache
	*/
	v3s16 blockpos_last;
	MapBlock *block = NULL;
	// Cache this a bit, too
	bool block_checked_in_modified = false;

	for(std::set<v3s16>::iterator j = from_nodes.begin();
		j != from_nodes.end(); ++j)
	{
		v3s16 pos = *j;
		v3s16 blockpos = getNodeBlockPos(pos);

		// Only fetch a new block if the block position has changed
		try{
			if(block == NULL || blockpos != blockpos_last){
				block = getBlockNoCreate(blockpos);
				blockpos_last = blockpos;

				block_checked_in_modified = false;
				blockchangecount++;
			}
		}
		catch(InvalidPositionException &e)
		{
			continue;
		}

		if(block->isDummy())
			continue;

		// Calculate relative position in block
		v3s16 relpos = pos - blockpos_last * MAP_BLOCKSIZE;

		// Get node straight from the block
		MapNode n = block->getNode(relpos);

		u8 oldlight = n.getLight(bank, nodemgr);
		u8 newlight = diminish_light(oldlight);

		// Loop through 6 neighbors
		for(u16 i=0; i<6; i++){
			// Get the position of the neighbor node
			v3s16 n2pos = pos + dirs[i];

			// Get the block where the node is located
			v3s16 blockpos = getNodeBlockPos(n2pos);

			try
			{
				// Only fetch a new block if the block position has changed
				try{
					if(block == NULL || blockpos != blockpos_last){
						block = getBlockNoCreate(blockpos);
						blockpos_last = blockpos;

						block_checked_in_modified = false;
						blockchangecount++;
					}
				}
				catch(InvalidPositionException &e)
				{
					continue;
				}

				// Calculate relative position in block
				v3s16 relpos = n2pos - blockpos * MAP_BLOCKSIZE;
				// Get node straight from the block
				MapNode n2 = block->getNode(relpos);

				bool changed = false;
				/*
					If the neighbor is brighter than the current node,
					add to list (it will light up this node on its turn)
				*/
				if(n2.getLight(bank, nodemgr) > undiminish_light(oldlight))
				{
					lighted_nodes.insert(n2pos);
					changed = true;
				}
				/*
					If the neighbor is dimmer than how much light this node
					would spread on it, add to list
				*/
				if(n2.getLight(bank, nodemgr) < newlight)
				{
					if(nodemgr->get(n2).light_propagates)
					{
						n2.setLight(bank, newlight, nodemgr);
						block->setNode(relpos, n2);
						lighted_nodes.insert(n2pos);
						changed = true;
					}
				}

				// Add to modified_blocks
				if(changed == true && block_checked_in_modified == false)
				{
					// If the block is not found in modified_blocks, add.
					if(modified_blocks.find(blockpos) == modified_blocks.end())
					{
						modified_blocks[blockpos] = block;
					}
					block_checked_in_modified = true;
				}
			}
			catch(InvalidPositionException &e)
			{
				continue;
			}
		}
	}

	/*infostream<<"spreadLight(): Changed block "
			<<blockchangecount<<" times"
			<<" for "<<from_nodes.size()<<" nodes"
			<<std::endl;*/

	if(lighted_nodes.size() > 0)
		spreadLight(bank, lighted_nodes, modified_blocks);
}

/*
	A single-node source variation of the above.
*/
void Map::lightNeighbors(enum LightBank bank,
		v3s16 pos,
		std::map<v3s16, MapBlock*> & modified_blocks)
{
	std::set<v3s16> from_nodes;
	from_nodes.insert(pos);
	spreadLight(bank, from_nodes, modified_blocks);
}

v3s16 Map::getBrightestNeighbour(enum LightBank bank, v3s16 p)
{
	INodeDefManager *nodemgr = m_gamedef->ndef();

	v3s16 dirs[6] = {
		v3s16(0,0,1), // back
		v3s16(0,1,0), // top
		v3s16(1,0,0), // right
		v3s16(0,0,-1), // front
		v3s16(0,-1,0), // bottom
		v3s16(-1,0,0), // left
	};

	u8 brightest_light = 0;
	v3s16 brightest_pos(0,0,0);
	bool found_something = false;

	// Loop through 6 neighbors
	for(u16 i=0; i<6; i++){
		// Get the position of the neighbor node
		v3s16 n2pos = p + dirs[i];
		MapNode n2;
		try{
			n2 = getNode(n2pos);
		}
		catch(InvalidPositionException &e)
		{
			continue;
		}
		if(n2.getLight(bank, nodemgr) > brightest_light || found_something == false){
			brightest_light = n2.getLight(bank, nodemgr);
			brightest_pos = n2pos;
			found_something = true;
		}
	}

	if(found_something == false)
		throw InvalidPositionException();

	return brightest_pos;
}

/*
	Propagates sunlight down from a node.
	Starting point gets sunlight.

	Returns the lowest y value of where the sunlight went.

	Mud is turned into grass in where the sunlight stops.
*/
s16 Map::propagateSunlight(v3s16 start,
		std::map<v3s16, MapBlock*> & modified_blocks)
{
	INodeDefManager *nodemgr = m_gamedef->ndef();

	s16 y = start.Y;
	for(; ; y--)
	{
		v3s16 pos(start.X, y, start.Z);

		v3s16 blockpos = getNodeBlockPos(pos);
		MapBlock *block;
		try{
			block = getBlockNoCreate(blockpos);
		}
		catch(InvalidPositionException &e)
		{
			break;
		}

		v3s16 relpos = pos - blockpos*MAP_BLOCKSIZE;
		MapNode n = block->getNode(relpos);

		if(nodemgr->get(n).sunlight_propagates)
		{
			n.setLight(LIGHTBANK_DAY, LIGHT_SUN, nodemgr);
			block->setNode(relpos, n);

			modified_blocks[blockpos] = block;
		}
		else
		{
			// Sunlight goes no further
			break;
		}
	}
	return y + 1;
}

u32 Map::updateLighting(enum LightBank bank,
		std::map<v3s16, MapBlock*> & a_blocks,
		std::map<v3s16, MapBlock*> & modified_blocks, int max_cycle_ms)
{
	INodeDefManager *nodemgr = m_gamedef->ndef();

	/*m_dout<<DTIME<<"Map::updateLighting(): "
			<<a_blocks.size()<<" blocks."<<std::endl;*/

	//TimeTaker timer("updateLighting");

	// For debugging
	//bool debug=true;
	//u32 count_was = modified_blocks.size();

	std::map<v3s16, MapBlock*> blocks_to_update;

	std::set<v3s16> light_sources;

	std::map<v3s16, u8> unlight_from;

	int num_bottom_invalid = 0;

	JMutexAutoLock lock2(m_update_lighting_mutex);

	{
	TimeTaker t("updateLighting: first stuff");

	u32 n = 0, calls = 0, end_ms = porting::getTimeMs() + max_cycle_ms;
	if(!max_cycle_ms)
		updateLighting_last[bank] = 0;
	for(std::map<v3s16, MapBlock*>::iterator i = a_blocks.begin();
		i != a_blocks.end(); ++i)
	{
			if (n++ < updateLighting_last[bank])
				continue;
			else
				updateLighting_last[bank] = 0;
			++calls;

		MapBlock *block = getBlockNoCreateNoEx(i->first);
		//MapBlock *block = i->second;

		if(!block || block->isDummy())
			continue;

		for(;;)
		{
			// Don't bother with dummy blocks.
			if(block->isDummy())
				break;

			v3s16 pos = block->getPos();
			v3s16 posnodes = block->getPosRelative();
			modified_blocks[pos] = block;
			blocks_to_update[pos] = block;

			/*
				Clear all light from block
			*/
			for(s16 z=0; z<MAP_BLOCKSIZE; z++)
			for(s16 x=0; x<MAP_BLOCKSIZE; x++)
			for(s16 y=0; y<MAP_BLOCKSIZE; y++)
			{

				try{
					v3s16 p(x,y,z);
					MapNode n = block->getNode(p);
					u8 oldlight = n.getLight(bank, nodemgr);
					n.setLight(bank, 0, nodemgr);
					block->setNode(p, n);

					// If node sources light, add to list
					u8 source = nodemgr->get(n).light_source;
					if(source != 0)
						light_sources.insert(p + posnodes);

					// Collect borders for unlighting
					if((x==0 || x == MAP_BLOCKSIZE-1
					|| y==0 || y == MAP_BLOCKSIZE-1
					|| z==0 || z == MAP_BLOCKSIZE-1)
					&& oldlight != 0)
					{
						v3s16 p_map = p + posnodes;
						unlight_from[p_map] = oldlight;
					}
				}
				catch(InvalidPositionException &e)
				{
					/*
						This would happen when dealing with a
						dummy block.
					*/
					//assert(0);
					infostream<<"updateLighting(): InvalidPositionException"
							<<std::endl;
				}
			}

			if(bank == LIGHTBANK_DAY)
			{
				bool bottom_valid = block->propagateSunlight(light_sources);

				if(!bottom_valid)
					num_bottom_invalid++;

				// If bottom is valid, we're done.
				if(bottom_valid)
					break;
			}
			else if(bank == LIGHTBANK_NIGHT)
			{
				// For night lighting, sunlight is not propagated
				break;
			}
			else
			{
				// Invalid lighting bank
				assert(0);
			}

			/*infostream<<"Bottom for sunlight-propagated block ("
					<<pos.X<<","<<pos.Y<<","<<pos.Z<<") not valid"
					<<std::endl;*/

			// Bottom sunlight is not valid; get the block and loop to it

			pos.Y--;
			try{
				block = getBlockNoCreate(pos);
			}
			catch(InvalidPositionException &e)
			{
				goto L_END_BLOCK;
			}

		}
		L_END_BLOCK:;
		if (porting::getTimeMs() > end_ms) {
			updateLighting_last[bank] = n;
			break;
		}
	}
	if (!calls)
		updateLighting_last[bank] = 0;
	}
	/*
		Enable this to disable proper lighting for speeding up map
		generation for testing or whatever
	*/
#if 0
	//if(g_settings->get(""))
	{
		core::map<v3s16, MapBlock*>::Iterator i;
		i = blocks_to_update.getIterator();
		for(; i.atEnd() == false; i++)
		{
			MapBlock *block = i.getNode()->getValue();
			v3s16 p = block->getPos();
			block->setLightingExpired(false);
		}
		return;
	}
#endif

#if 1
	{
		TimeTaker timer("updateLighting: unspreadLight");
		unspreadLight(bank, unlight_from, light_sources, modified_blocks);
	}

	/*if(debug)
	{
		u32 diff = modified_blocks.size() - count_was;
		count_was = modified_blocks.size();
		infostream<<"unspreadLight modified "<<diff<<std::endl;
	}*/

	{
		TimeTaker timer("updateLighting: spreadLight");
		spreadLight(bank, light_sources, modified_blocks);
	}

	/*if(debug)
	{
		u32 diff = modified_blocks.size() - count_was;
		count_was = modified_blocks.size();
		infostream<<"spreadLight modified "<<diff<<std::endl;
	}*/
#endif

#if 0
	{
		//MapVoxelManipulator vmanip(this);

		// Make a manual voxel manipulator and load all the blocks
		// that touch the requested blocks
		ManualMapVoxelManipulator vmanip(this);

		{
		//TimeTaker timer("initialEmerge");

		core::map<v3s16, MapBlock*>::Iterator i;
		i = blocks_to_update.getIterator();
		for(; i.atEnd() == false; i++)
		{
			MapBlock *block = i.getNode()->getValue();
			v3s16 p = block->getPos();

			// Add all surrounding blocks
			vmanip.initialEmerge(p - v3s16(1,1,1), p + v3s16(1,1,1));

			/*
				Add all surrounding blocks that have up-to-date lighting
				NOTE: This doesn't quite do the job (not everything
					  appropriate is lighted)
			*/
			/*for(s16 z=-1; z<=1; z++)
			for(s16 y=-1; y<=1; y++)
			for(s16 x=-1; x<=1; x++)
			{
				v3s16 p2 = p + v3s16(x,y,z);
				MapBlock *block = getBlockNoCreateNoEx(p2);
				if(block == NULL)
					continue;
				if(block->isDummy())
					continue;
				if(block->getLightingExpired())
					continue;
				vmanip.initialEmerge(p2, p2);
			}*/

			// Lighting of block will be updated completely
			block->setLightingExpired(false);
		}
		}

		{
			//TimeTaker timer("unSpreadLight");
			vmanip.unspreadLight(bank, unlight_from, light_sources, nodemgr);
		}
		{
			//TimeTaker timer("spreadLight");
			vmanip.spreadLight(bank, light_sources, nodemgr);
		}
		{
			//TimeTaker timer("blitBack");
			vmanip.blitBack(modified_blocks);
		}
		/*infostream<<"emerge_time="<<emerge_time<<std::endl;
		emerge_time = 0;*/
	}
#endif
	return updateLighting_last[bank];
	//m_dout<<"Done ("<<getTimestamp()<<")"<<std::endl;
}

u32 Map::updateLighting(std::map<v3s16, MapBlock*> & a_blocks,
		std::map<v3s16, MapBlock*> & modified_blocks, int max_cycle_ms)
{
	int ret = 0;
{
TimeTaker timer("updateLighting(LIGHTBANK_DAY)");

	ret += updateLighting(LIGHTBANK_DAY, a_blocks, modified_blocks, max_cycle_ms);
}
{
TimeTaker timer("updateLighting(LIGHTBANK_NIGHT)");
	ret += updateLighting(LIGHTBANK_NIGHT, a_blocks, modified_blocks, max_cycle_ms);
}

	if (max_cycle_ms && ret)
		return ret;

TimeTaker timer("updateLighting expireDayNightDiff");
	JMutexAutoLock lock2(m_update_lighting_mutex);

	/*
		Update information about whether day and night light differ
	*/
	for(std::map<v3s16, MapBlock*>::iterator
			i = modified_blocks.begin();
			i != modified_blocks.end(); ++i)
	{
		MapBlock *block = getBlockNoCreateNoEx(i->first);
		//can contain already deleted block from Map::timerUpdate -> MapSector::deleteBlock
		//MapBlock *block = i->second;
		if(block == NULL || block->isDummy())
			continue;
		block->expireDayNightDiff();
	}
	return ret;
}

/*
*/
void Map::addNodeAndUpdate(v3s16 p, MapNode n,
		std::map<v3s16, MapBlock*> &modified_blocks,
		bool remove_metadata)
{
	INodeDefManager *ndef = m_gamedef->ndef();

	/*PrintInfo(m_dout);
	m_dout<<DTIME<<"Map::addNodeAndUpdate(): p=("
			<<p.X<<","<<p.Y<<","<<p.Z<<")"<<std::endl;*/

	/*
		From this node to nodes underneath:
		If lighting is sunlight (1.0), unlight neighbours and
		set lighting to 0.
		Else discontinue.
	*/

	v3s16 toppos = p + v3s16(0,1,0);
	//v3s16 bottompos = p + v3s16(0,-1,0);

	bool node_under_sunlight = true;
	std::set<v3s16> light_sources;

	/*
		Collect old node for rollback
	*/
	RollbackNode rollback_oldnode(this, p, m_gamedef);

	/*
		If there is a node at top and it doesn't have sunlight,
		there has not been any sunlight going down.

		Otherwise there probably is.
	*/
	try{
		MapNode topnode = getNode(toppos);

		if(topnode.getLight(LIGHTBANK_DAY, ndef) != LIGHT_SUN)
			node_under_sunlight = false;
	}
	catch(InvalidPositionException &e)
	{
	}

	/*
		Remove all light that has come out of this node
	*/

	enum LightBank banks[] =
	{
		LIGHTBANK_DAY,
		LIGHTBANK_NIGHT
	};
	for(s32 i=0; i<2; i++)
	{
		enum LightBank bank = banks[i];

		u8 lightwas = getNode(p).getLight(bank, ndef);

		// Add the block of the added node to modified_blocks
		v3s16 blockpos = getNodeBlockPos(p);
		MapBlock * block = getBlockNoCreate(blockpos);
		assert(block != NULL);
		modified_blocks[blockpos] = block;

		assert(isValidPosition(p));

		// Unlight neighbours of node.
		// This means setting light of all consequent dimmer nodes
		// to 0.
		// This also collects the nodes at the border which will spread
		// light again into this.
		unLightNeighbors(bank, p, lightwas, light_sources, modified_blocks);

		n.setLight(bank, 0, ndef);
	}

	/*
		If node lets sunlight through and is under sunlight, it has
		sunlight too.
	*/
	if(node_under_sunlight && ndef->get(n).sunlight_propagates)
	{
		n.setLight(LIGHTBANK_DAY, LIGHT_SUN, ndef);
	}

	/*
		Remove node metadata
	*/
	if (remove_metadata) {
		removeNodeMetadata(p);
	}

	/*
		Set the node on the map
	*/

	setNode(p, n);

	/*
		If node is under sunlight and doesn't let sunlight through,
		take all sunlighted nodes under it and clear light from them
		and from where the light has been spread.
		TODO: This could be optimized by mass-unlighting instead
			  of looping
	*/
	if(node_under_sunlight && !ndef->get(n).sunlight_propagates)
	{
		s16 y = p.Y - 1;
		for(;; y--){
			//m_dout<<DTIME<<"y="<<y<<std::endl;
			v3s16 n2pos(p.X, y, p.Z);

			MapNode n2;
			try{
				n2 = getNode(n2pos);
			}
			catch(InvalidPositionException &e)
			{
				break;
			}

			if(n2.getLight(LIGHTBANK_DAY, ndef) == LIGHT_SUN)
			{
				unLightNeighbors(LIGHTBANK_DAY,
						n2pos, n2.getLight(LIGHTBANK_DAY, ndef),
						light_sources, modified_blocks);
				n2.setLight(LIGHTBANK_DAY, 0, ndef);
				setNode(n2pos, n2);
			}
			else
				break;
		}
	}

	for(s32 i=0; i<2; i++)
	{
		enum LightBank bank = banks[i];

		/*
			Spread light from all nodes that might be capable of doing so
		*/
		spreadLight(bank, light_sources, modified_blocks);
	}

	/*
		Update information about whether day and night light differ
	*/
	for(std::map<v3s16, MapBlock*>::iterator
			i = modified_blocks.begin();
			i != modified_blocks.end(); ++i)
	{
		i->second->expireDayNightDiff();
	}

	/*
		Report for rollback
	*/
	if(m_gamedef->rollback())
	{
		RollbackNode rollback_newnode(this, p, m_gamedef);
		RollbackAction action;
		action.setSetNode(p, rollback_oldnode, rollback_newnode);
		m_gamedef->rollback()->reportAction(action);
	}

	/*
		Add neighboring liquid nodes and the node itself if it is
		liquid (=water node was added) to transform queue.
		note: todo: for liquid_real enough to add only self node
	*/
	v3s16 dirs[7] = {
		v3s16(0,0,0), // self
		v3s16(0,0,1), // back
		v3s16(0,1,0), // top
		v3s16(1,0,0), // right
		v3s16(0,0,-1), // front
		v3s16(0,-1,0), // bottom
		v3s16(-1,0,0), // left
	};
	for(u16 i=0; i<7; i++)
	{
		try
		{

		v3s16 p2 = p + dirs[i];

		MapNode n2 = getNode(p2);
		if(ndef->get(n2).isLiquid() || n2.getContent() == CONTENT_AIR)
		{
			transforming_liquid_push_back(p2);
		}

		}catch(InvalidPositionException &e)
		{
		}
	}
}

/*
*/
void Map::removeNodeAndUpdate(v3s16 p,
		std::map<v3s16, MapBlock*> &modified_blocks)
{
	INodeDefManager *ndef = m_gamedef->ndef();

	/*PrintInfo(m_dout);
	m_dout<<DTIME<<"Map::removeNodeAndUpdate(): p=("
			<<p.X<<","<<p.Y<<","<<p.Z<<")"<<std::endl;*/

	bool node_under_sunlight = true;

	v3s16 toppos = p + v3s16(0,1,0);

	// Node will be replaced with this
	content_t replace_material = CONTENT_AIR;

	/*
		Collect old node for rollback
	*/
	RollbackNode rollback_oldnode(this, p, m_gamedef);

	/*
		If there is a node at top and it doesn't have sunlight,
		there will be no sunlight going down.
	*/
	try{
		MapNode topnode = getNode(toppos);

		if(topnode.getLight(LIGHTBANK_DAY, ndef) != LIGHT_SUN)
			node_under_sunlight = false;
	}
	catch(InvalidPositionException &e)
	{
	}

	std::set<v3s16> light_sources;

	enum LightBank banks[] =
	{
		LIGHTBANK_DAY,
		LIGHTBANK_NIGHT
	};
	for(s32 i=0; i<2; i++)
	{
		enum LightBank bank = banks[i];

		/*
			Unlight neighbors (in case the node is a light source)
		*/
		unLightNeighbors(bank, p,
				getNode(p).getLight(bank, ndef),
				light_sources, modified_blocks);
	}

	/*
		Remove node metadata
	*/

	removeNodeMetadata(p);

	/*
		Remove the node.
		This also clears the lighting.
	*/

	MapNode n;
	n.setContent(replace_material);
	setNode(p, n);

	for(s32 i=0; i<2; i++)
	{
		enum LightBank bank = banks[i];

		/*
			Recalculate lighting
		*/
		spreadLight(bank, light_sources, modified_blocks);
	}

	// Add the block of the removed node to modified_blocks
	v3s16 blockpos = getNodeBlockPos(p);
	MapBlock * block = getBlockNoCreate(blockpos);
	assert(block != NULL);
	modified_blocks[blockpos] = block;

	/*
		If the removed node was under sunlight, propagate the
		sunlight down from it and then light all neighbors
		of the propagated blocks.
	*/
	if(node_under_sunlight)
	{
		s16 ybottom = propagateSunlight(p, modified_blocks);
		/*m_dout<<DTIME<<"Node was under sunlight. "
				"Propagating sunlight";
		m_dout<<DTIME<<" -> ybottom="<<ybottom<<std::endl;*/
		s16 y = p.Y;
		for(; y >= ybottom; y--)
		{
			v3s16 p2(p.X, y, p.Z);
			/*m_dout<<DTIME<<"lighting neighbors of node ("
					<<p2.X<<","<<p2.Y<<","<<p2.Z<<")"
					<<std::endl;*/
			lightNeighbors(LIGHTBANK_DAY, p2, modified_blocks);
		}
	}
	else
	{
		// Set the lighting of this node to 0
		// TODO: Is this needed? Lighting is cleared up there already.
		try{
			MapNode n = getNode(p);
			n.setLight(LIGHTBANK_DAY, 0, ndef);
			setNode(p, n);
		}
		catch(InvalidPositionException &e)
		{
			assert(0);
		}
	}

	for(s32 i=0; i<2; i++)
	{
		enum LightBank bank = banks[i];

		// Get the brightest neighbour node and propagate light from it
		v3s16 n2p = getBrightestNeighbour(bank, p);
		try{
			//MapNode n2 = getNode(n2p);
			lightNeighbors(bank, n2p, modified_blocks);
		}
		catch(InvalidPositionException &e)
		{
		}
	}

	/*
		Update information about whether day and night light differ
	*/
	for(std::map<v3s16, MapBlock*>::iterator
			i = modified_blocks.begin();
			i != modified_blocks.end(); ++i)
	{
		i->second->expireDayNightDiff();
	}

	/*
		Report for rollback
	*/
	if(m_gamedef->rollback())
	{
		RollbackNode rollback_newnode(this, p, m_gamedef);
		RollbackAction action;
		action.setSetNode(p, rollback_oldnode, rollback_newnode);
		m_gamedef->rollback()->reportAction(action);
	}

	/*
		Add neighboring liquid nodes and this node to transform queue.
		(it's vital for the node itself to get updated last.)
		note: todo: for liquid_real enough to add only self node
	*/
	v3s16 dirs[7] = {
		v3s16(0,0,1), // back
		v3s16(0,1,0), // top
		v3s16(1,0,0), // right
		v3s16(0,0,-1), // front
		v3s16(0,-1,0), // bottom
		v3s16(-1,0,0), // left
		v3s16(0,0,0), // self
	};
	for(u16 i=0; i<7; i++)
	{
		try
		{

		v3s16 p2 = p + dirs[i];

		MapNode n2 = getNode(p2);
		if(ndef->get(n2).isLiquid() || n2.getContent() == CONTENT_AIR)
		{
			transforming_liquid_push_back(p2);
		}

		}catch(InvalidPositionException &e)
		{
		}
	}
}

bool Map::addNodeWithEvent(v3s16 p, MapNode n, bool remove_metadata)
{
	MapEditEvent event;
	event.type = remove_metadata ? MEET_ADDNODE : MEET_SWAPNODE;
	event.p = p;
	event.n = n;

	bool succeeded = true;
	try{
		std::map<v3s16, MapBlock*> modified_blocks;
		addNodeAndUpdate(p, n, modified_blocks, remove_metadata);

		// Copy modified_blocks to event
		for(std::map<v3s16, MapBlock*>::iterator
				i = modified_blocks.begin();
				i != modified_blocks.end(); ++i)
		{
			event.modified_blocks.insert(i->first);
		}
	}
	catch(InvalidPositionException &e){
		succeeded = false;
	}

	dispatchEvent(&event);

	return succeeded;
}

bool Map::removeNodeWithEvent(v3s16 p)
{
	MapEditEvent event;
	event.type = MEET_REMOVENODE;
	event.p = p;

	bool succeeded = true;
	try{
		std::map<v3s16, MapBlock*> modified_blocks;
		removeNodeAndUpdate(p, modified_blocks);

		// Copy modified_blocks to event
		for(std::map<v3s16, MapBlock*>::iterator
				i = modified_blocks.begin();
				i != modified_blocks.end(); ++i)
		{
			event.modified_blocks.insert(i->first);
		}
	}
	catch(InvalidPositionException &e){
		succeeded = false;
	}

	dispatchEvent(&event);

	return succeeded;
}

bool Map::getDayNightDiff(v3s16 blockpos)
{
	try{
		v3s16 p = blockpos + v3s16(0,0,0);
		MapBlock *b = getBlockNoCreate(p);
		if(b->getDayNightDiff())
			return true;
	}
	catch(InvalidPositionException &e){}
	// Leading edges
	try{
		v3s16 p = blockpos + v3s16(-1,0,0);
		MapBlock *b = getBlockNoCreate(p);
		if(b->getDayNightDiff())
			return true;
	}
	catch(InvalidPositionException &e){}
	try{
		v3s16 p = blockpos + v3s16(0,-1,0);
		MapBlock *b = getBlockNoCreate(p);
		if(b->getDayNightDiff())
			return true;
	}
	catch(InvalidPositionException &e){}
	try{
		v3s16 p = blockpos + v3s16(0,0,-1);
		MapBlock *b = getBlockNoCreate(p);
		if(b->getDayNightDiff())
			return true;
	}
	catch(InvalidPositionException &e){}
	// Trailing edges
	try{
		v3s16 p = blockpos + v3s16(1,0,0);
		MapBlock *b = getBlockNoCreate(p);
		if(b->getDayNightDiff())
			return true;
	}
	catch(InvalidPositionException &e){}
	try{
		v3s16 p = blockpos + v3s16(0,1,0);
		MapBlock *b = getBlockNoCreate(p);
		if(b->getDayNightDiff())
			return true;
	}
	catch(InvalidPositionException &e){}
	try{
		v3s16 p = blockpos + v3s16(0,0,1);
		MapBlock *b = getBlockNoCreate(p);
		if(b->getDayNightDiff())
			return true;
	}
	catch(InvalidPositionException &e){}

	return false;
}

/*
	Updates usage timers
*/
u32 Map::timerUpdate(float uptime, float unload_timeout,
		int max_cycle_ms,
		std::list<v3s16> *unloaded_blocks)
{
	bool save_before_unloading = (mapType() == MAPTYPE_SERVER);

	// Profile modified reasons
	Profiler modprofiler;

	std::list<v2s16> sector_deletion_queue;
	u32 deleted_blocks_count = 0;
	u32 saved_blocks_count = 0;
	u32 block_count_all = 0;

/*
	beginSave();
*/
	u32 n = 0, calls = 0, end_ms = porting::getTimeMs() + max_cycle_ms;

	for(std::map<v2s16, MapSector*>::iterator si = m_sectors.begin();
		si != m_sectors.end(); ++si)
	{
		if (n++ < m_sectors_update_last)
			continue;
		else
			m_sectors_update_last = 0;
		++calls;

		MapSector *sector = si->second;

		bool all_blocks_deleted = true;

		std::list<MapBlock*> blocks;
		sector->getBlocks(blocks);

		for(std::list<MapBlock*>::iterator i = blocks.begin();
				i != blocks.end(); ++i)
		{
			MapBlock *block = (*i);

			if (!block->m_uptime_timer_last)  // not very good place, but minimum modifications
				block->m_uptime_timer_last = uptime - 0.1;
			block->incrementUsageTimer(uptime - block->m_uptime_timer_last);
			block->m_uptime_timer_last = uptime;

			if(block->refGet() == 0 && block->getUsageTimer() > unload_timeout)
			{
				v3s16 p = block->getPos();

				// Save if modified
				if(block->getModified() != MOD_STATE_CLEAN
						&& save_before_unloading)
				{
					modprofiler.add(block->getModifiedReason(), 1);
					beginSave();
					saveBlock(block);
					endSave();
					saved_blocks_count++;
				}

				// Delete from memory
				sector->deleteBlock(block);

				if(unloaded_blocks)
					unloaded_blocks->push_back(p);

				deleted_blocks_count++;
			}
			else
			{
				all_blocks_deleted = false;
				block_count_all++;

/*#ifndef SERVER
				if(block->refGet() == 0 && block->getUsageTimer() >
						g_settings->getFloat("unload_unused_meshes_timeout"))
				{
					if(block->mesh){
						delete block->mesh;
						block->mesh = NULL;
					}
				}
#endif*/
			}
		}

		if(all_blocks_deleted)
		{
			sector_deletion_queue.push_back(si->first);
		}

		if (porting::getTimeMs() > end_ms) {
			m_sectors_update_last = n;
			break;
		}

	}
	if (!calls)
		m_sectors_update_last = 0;

	if(m_circuit != NULL) {
		m_circuit->save();
	}
/*
	endSave();
*/

	// Finally delete the empty sectors

	deleteSectors(sector_deletion_queue);

	if(deleted_blocks_count != 0)
	{
		if (m_sectors_update_last)
			infostream<<"ServerMap: timerUpdate(): Sectors processed:"<<calls<<"/"<<m_sectors.size()<<" to "<<m_sectors_update_last<<std::endl;
		PrintInfo(infostream); // ServerMap/ClientMap:
		infostream<<"Unloaded "<<deleted_blocks_count
				<<" blocks from memory";
		if(save_before_unloading)
			infostream<<", of which "<<saved_blocks_count<<" were written";
/*
		infostream<<", "<<block_count_all<<" blocks in memory";
*/
		infostream<<"."<<std::endl;
		if(saved_blocks_count != 0){
			PrintInfo(infostream); // ServerMap/ClientMap:
			infostream<<"Blocks modified by: "<<std::endl;
			modprofiler.print(infostream);
		}
	}
	return m_sectors_update_last;
}

void Map::unloadUnreferencedBlocks(std::list<v3s16> *unloaded_blocks)
{
	timerUpdate(0.0, -1.0, 100, unloaded_blocks);
}

void Map::deleteSectors(std::list<v2s16> &list)
{
	for(std::list<v2s16>::iterator j = list.begin();
		j != list.end(); ++j)
	{
		MapSector *sector = m_sectors[*j];
		// If sector is in sector cache, remove it from there
		if(m_sector_cache == sector)
			m_sector_cache = NULL;
		// Remove from map and delete
		m_sectors.erase(*j);
		delete sector;
	}
}

#if 0
void Map::unloadUnusedData(float timeout,
		core::list<v3s16> *deleted_blocks)
{
	core::list<v2s16> sector_deletion_queue;
	u32 deleted_blocks_count = 0;
	u32 saved_blocks_count = 0;

	core::map<v2s16, MapSector*>::Iterator si = m_sectors.getIterator();
	for(; si.atEnd() == false; si++)
	{
		MapSector *sector = si.getNode()->getValue();

		bool all_blocks_deleted = true;

		core::list<MapBlock*> blocks;
		sector->getBlocks(blocks);
		for(core::list<MapBlock*>::Iterator i = blocks.begin();
				i != blocks.end(); i++)
		{
			MapBlock *block = (*i);

			if(block->getUsageTimer() > timeout)
			{
				// Save if modified
				if(block->getModified() != MOD_STATE_CLEAN)
				{
					saveBlock(block);
					saved_blocks_count++;
				}
				// Delete from memory
				sector->deleteBlock(block);
				deleted_blocks_count++;
			}
			else
			{
				all_blocks_deleted = false;
			}
		}

		if(all_blocks_deleted)
		{
			sector_deletion_queue.push_back(si.getNode()->getKey());
		}
	}

	deleteSectors(sector_deletion_queue);

	infostream<<"Map: Unloaded "<<deleted_blocks_count<<" blocks from memory"
			<<", of which "<<saved_blocks_count<<" were wr."
			<<std::endl;

	//return sector_deletion_queue.getSize();
	//return deleted_blocks_count;
}
#endif

void Map::PrintInfo(std::ostream &out)
{
	out<<"Map: ";
}

enum NeighborType {
	NEIGHBOR_UPPER,
	NEIGHBOR_SAME_LEVEL,
	NEIGHBOR_LOWER
};
struct NodeNeighbor {
	MapNode n;
	NeighborType t;
	v3s16 p;
	bool l; //can liquid
	bool i; //infinity
	int weight;
	int drop; //drop by liquid
};

void Map::transforming_liquid_push_back(v3s16 & p) {
	JMutexAutoLock lock(m_transforming_liquid_mutex);
	m_transforming_liquid.push_back(p);
}

u32 Map::transforming_liquid_size() {
        return m_transforming_liquid.size();
}

Circuit* Map::getCircuit()
{
	return m_circuit;
}

INodeDefManager* Map::getNodeDefManager()
{
	return m_gamedef->ndef();
}

const v3s16 liquid_flow_dirs[7] =
{
	// +right, +top, +back
	v3s16( 0,-1, 0), // bottom
	v3s16( 0, 0, 0), // self
	v3s16( 0, 0, 1), // back
	v3s16( 0, 0,-1), // front
	v3s16( 1, 0, 0), // right
	v3s16(-1, 0, 0), // left
	v3s16( 0, 1, 0)  // top
};

// when looking around we must first check self node for correct type definitions
const s8 liquid_explore_map[7] = {1,0,6,2,3,4,5};
const s8 liquid_random_map[4][7] = {
	{0,1,2,3,4,5,6},
	{0,1,4,3,5,2,6},
	{0,1,3,5,4,2,6},
	{0,1,5,3,2,4,6}
};

#define D_BOTTOM 0
#define D_TOP 6
#define D_SELF 1

u32 Map::transformLiquidsFinite(Server *m_server, std::map<v3s16, MapBlock*> & modified_blocks, std::map<v3s16, MapBlock*> & lighting_modified_blocks, int max_cycle_ms)
{
	INodeDefManager *nodemgr = m_gamedef->ndef();

	DSTACK(__FUNCTION_NAME);
	//TimeTaker timer("transformLiquidsFinite()");

	u32 loopcount = 0;
	u32 initial_size = m_transforming_liquid.size();

	u8 relax = g_settings->getS16("liquid_relax");
	bool fast_flood = g_settings->getS16("liquid_fast_flood");
	int water_level = g_settings->getS16("water_level");

	// list of nodes that due to viscosity have not reached their max level height
	UniqueQueue<v3s16> must_reflow, must_reflow_second;

	// List of MapBlocks that will require a lighting update (due to lava)
	u16 loop_rand = myrand();

	u32 end_ms = porting::getTimeMs() + max_cycle_ms;

	while (m_transforming_liquid.size() > 0)
	{
		NEXT_LIQUID:;
		// This should be done here so that it is done when continue is used
		if (loopcount >= initial_size || porting::getTimeMs() > end_ms)
			break;
		loopcount++;
		/*
			Get a queued transforming liquid node
		*/
		v3s16 p0;
		{
			JMutexAutoLock lock(m_transforming_liquid_mutex);
			p0 = m_transforming_liquid.pop_front();
		}
		u16 total_level = 0;
		//u16 level_max = 0;
		// surrounding flowing liquid nodes
		NodeNeighbor neighbors[7];
		// current level of every block
		s8 liquid_levels[7] = {-1, -1, -1, -1, -1, -1, -1};
		 // target levels
		s8 liquid_levels_want[7] = {-1, -1, -1, -1, -1, -1, -1};
		s8 can_liquid_same_level = 0;
		content_t liquid_kind = CONTENT_IGNORE;
		content_t liquid_kind_flowing = CONTENT_IGNORE;
		content_t melt_kind = CONTENT_IGNORE;
		content_t melt_kind_flowing = CONTENT_IGNORE;
		//s8 viscosity = 0;
		/*
			Collect information about the environment, start from self
		 */
		for (u8 e = 0; e < 7; e++) {
			u8 i = liquid_explore_map[e];
			NodeNeighbor & nb = neighbors[i];
			NeighborType nt = NEIGHBOR_SAME_LEVEL;
			switch (i) {
				case D_TOP:
					nt = NEIGHBOR_UPPER;
					break;
				case D_BOTTOM:
					nt = NEIGHBOR_LOWER;
					break;
			}
			neighbors[i].p = p0 + liquid_flow_dirs[i];
			neighbors[i].n = getNodeNoEx(neighbors[i].p);
			neighbors[i].t = nt;
			neighbors[i].l = 0;
			neighbors[i].i = 0;
			neighbors[i].weight = 0;
			neighbors[i].drop = 0;

			if (nb.n.getContent() == CONTENT_IGNORE)
				continue;

			switch (nodemgr->get(nb.n.getContent()).liquid_type) {
				case LIQUID_NONE:
					if (nb.n.getContent() == CONTENT_AIR) {
						liquid_levels[i] = 0;
						nb.l = 1;
					}
					//TODO: if (nb.n.getContent() == CONTENT_AIR || nodemgr->get(nb.n).buildable_to && !nodemgr->get(nb.n).walkable) { // need lua drop api for drop torches
					else if (	melt_kind_flowing != CONTENT_IGNORE &&
							nb.n.getContent() == melt_kind_flowing &&
							nb.t != NEIGHBOR_UPPER &&
							!(loopcount % 2)) {
						u8 melt_max_level = nb.n.getMaxLevel(nodemgr);
						u8 my_max_level = MapNode(liquid_kind_flowing).getMaxLevel(nodemgr);
						liquid_levels[i] = (float)my_max_level / melt_max_level * nb.n.getLevel(nodemgr);
						if (liquid_levels[i])
							nb.l = 1;
					} else if (	melt_kind != CONTENT_IGNORE &&
							nb.n.getContent() == melt_kind &&
							nb.t != NEIGHBOR_UPPER &&
							!(loopcount % 8)) {
						liquid_levels[i] = nodemgr->get(liquid_kind_flowing).getMaxLevel();
						if (liquid_levels[i])
							nb.l = 1;
					} else {
						int drop = ((ItemGroupList) nodemgr->get(nb.n).groups)["drop_by_liquid"];
						if (drop && !(loopcount % drop) ) {
							liquid_levels[i] = 0;
							nb.l = 1;
							nb.drop = 1;
						}
					}

					// todo: for erosion add something here..
					break;
				case LIQUID_SOURCE:
					// if this node is not (yet) of a liquid type,
					// choose the first liquid type we encounter
					if (liquid_kind_flowing == CONTENT_IGNORE)
						liquid_kind_flowing = nodemgr->getId(
							nodemgr->get(nb.n).liquid_alternative_flowing);
					if (liquid_kind == CONTENT_IGNORE)
						liquid_kind = nb.n.getContent();
					if (liquid_kind_flowing == CONTENT_IGNORE)
						liquid_kind_flowing = liquid_kind;
					if (melt_kind == CONTENT_IGNORE)
						melt_kind = nodemgr->getId(nodemgr->get(nb.n).melt);
					if (melt_kind_flowing == CONTENT_IGNORE)
						melt_kind_flowing =
							nodemgr->getId(
							nodemgr->get(nodemgr->getId(nodemgr->get(nb.n).melt)
									).liquid_alternative_flowing);
					if (melt_kind_flowing == CONTENT_IGNORE)
						melt_kind_flowing = melt_kind;
					if (nb.n.getContent() == liquid_kind) {
						liquid_levels[i] = nb.n.getLevel(nodemgr); //LIQUID_LEVEL_SOURCE;
						nb.l = 1;
						nb.i = (nb.n.param2 & LIQUID_INFINITY_MASK);
					}
					break;
				case LIQUID_FLOWING:
					// if this node is not (yet) of a liquid type,
					// choose the first liquid type we encounter
					if (liquid_kind_flowing == CONTENT_IGNORE)
						liquid_kind_flowing = nb.n.getContent();
					if (liquid_kind == CONTENT_IGNORE)
						liquid_kind = nodemgr->getId(
							nodemgr->get(nb.n).liquid_alternative_source);
					if (liquid_kind == CONTENT_IGNORE)
						liquid_kind = liquid_kind_flowing;
					if (melt_kind_flowing == CONTENT_IGNORE)
						melt_kind_flowing = nodemgr->getId(nodemgr->get(nb.n).melt);
					if (melt_kind == CONTENT_IGNORE)
						melt_kind = nodemgr->getId(nodemgr->get(nodemgr->getId(
							nodemgr->get(nb.n).melt)).liquid_alternative_source);
					if (melt_kind == CONTENT_IGNORE)
						melt_kind = melt_kind_flowing;
					if (nb.n.getContent() == liquid_kind_flowing) {
						liquid_levels[i] = nb.n.getLevel(nodemgr);
						nb.l = 1;
					}
					break;
			}

			// only self, top, bottom swap
			if (nodemgr->get(nb.n.getContent()).liquid_type && e <= 2) {
				try{
					nb.weight = ((ItemGroupList) nodemgr->get(nb.n).groups)["weight"];
					if (e == 1 && neighbors[D_BOTTOM].weight && neighbors[D_SELF].weight > neighbors[D_BOTTOM].weight) {
						setNode(neighbors[D_SELF].p, neighbors[D_BOTTOM].n);
						setNode(neighbors[D_BOTTOM].p, neighbors[D_SELF].n);
						must_reflow_second.push_back(neighbors[D_SELF].p);
						must_reflow_second.push_back(neighbors[D_BOTTOM].p);
						goto NEXT_LIQUID;
					}
					if (e == 2 && neighbors[D_SELF].weight && neighbors[D_TOP].weight > neighbors[D_SELF].weight) {
						setNode(neighbors[D_SELF].p, neighbors[D_TOP].n);
						setNode(neighbors[D_TOP].p, neighbors[D_SELF].n);
						must_reflow_second.push_back(neighbors[D_SELF].p);
						must_reflow_second.push_back(neighbors[D_TOP].p);
						goto NEXT_LIQUID;
					}
				}
				catch(InvalidPositionException &e) {
					infostream<<"transformLiquidsFinite: setNode() failed:"<<PP(neighbors[i].p)<<":"<<e.what()<<std::endl;
					goto NEXT_LIQUID;
				}
			}
			
			if (nb.l && nb.t == NEIGHBOR_SAME_LEVEL)
				++can_liquid_same_level;
			if (liquid_levels[i] > 0)
				total_level += liquid_levels[i];

			/*
			infostream << "get node i=" <<(int)i<<" " << PP(nb.p) << " c="
			<< nb.n.getContent() <<" p0="<< (int)nb.n.param0 <<" p1="
			<< (int)nb.n.param1 <<" p2="<< (int)nb.n.param2 << " lt="
			<< nodemgr->get(nb.n.getContent()).liquid_type
			//<< " lk=" << liquid_kind << " lkf=" << liquid_kind_flowing
			<< " l="<< nb.l	<< " inf="<< nb.i << " nlevel=" << (int)liquid_levels[i]
			<< " totallevel=" << (int)total_level << " cansame="
			<< (int)can_liquid_same_level << " Lmax="<<(int)nodemgr->get(liquid_kind_flowing).getMaxLevel()<<std::endl;
			*/
		}
		s16 level_max = nodemgr->get(liquid_kind_flowing).getMaxLevel();
		s16 level_max_compressed = nodemgr->get(liquid_kind_flowing).getMaxLevel(1);
		//s16 total_was = total_level; //debug
		//viscosity = nodemgr->get(liquid_kind).viscosity;

		if (liquid_kind == CONTENT_IGNORE || !neighbors[D_SELF].l || total_level <= 0)
			continue;

		// fill bottom block
		if (neighbors[D_BOTTOM].l) {
			liquid_levels_want[D_BOTTOM] = total_level > level_max ?
				level_max : total_level;
			total_level -= liquid_levels_want[D_BOTTOM];
		}

		//relax up
		if (	nodemgr->get(liquid_kind).liquid_renewable &&
			relax &&
			((p0.Y == water_level) || (fast_flood && p0.Y <= water_level)) &&
			level_max > 1 &&
			liquid_levels[D_TOP] == 0 &&
			liquid_levels[D_BOTTOM] == level_max &&
			total_level >= level_max * can_liquid_same_level - (can_liquid_same_level - relax) &&
			can_liquid_same_level >= relax + 1) {
			total_level = level_max * can_liquid_same_level;
		}

		// prevent lakes in air above unloaded blocks
		if (	liquid_levels[D_TOP] == 0 &&
			p0.Y > water_level &&
			level_max > 1 &&
			neighbors[D_BOTTOM].n.getContent() == CONTENT_IGNORE &&
			!(loopcount % 3)) {
			--total_level;
		}

		// calculate self level 5 blocks
		u16 want_level =
			  total_level >= level_max * can_liquid_same_level
			? level_max
			: total_level / can_liquid_same_level;
		total_level -= want_level * can_liquid_same_level;

		//relax down
		if (	nodemgr->get(liquid_kind).liquid_renewable &&
			relax &&
			p0.Y == water_level + 1 &&
			liquid_levels[D_TOP] == 0 &&
			level_max > 1 &&
			liquid_levels[D_BOTTOM] == level_max &&
			want_level == 0 &&
			total_level <= (can_liquid_same_level - relax) &&
			can_liquid_same_level >= relax + 1) {
			total_level = 0;
		}

		for (u16 ir = D_SELF; ir < D_TOP; ++ir) { // fill only same level
			u16 ii = liquid_random_map[(loopcount+loop_rand+1)%4][ir];
			if (!neighbors[ii].l)
				continue;
			liquid_levels_want[ii] = want_level;
			//if (viscosity > 1 && (liquid_levels_want[ii]-liquid_levels[ii]>8-viscosity))
			if (liquid_levels_want[ii] < level_max && total_level > 0) {
				if (level_max > LIQUID_LEVEL_SOURCE || loopcount % 3 || liquid_levels[ii] <= 0){
					if (liquid_levels[ii] > liquid_levels_want[ii]) {
						++liquid_levels_want[ii];
						--total_level;
					}
				} else {
					++liquid_levels_want[ii];
					--total_level;
				}
			}
		}

		for (u16 ir = D_SELF; ir < D_TOP; ++ir) {
			if (total_level < 1) break;
			u16 ii = liquid_random_map[(loopcount+loop_rand+2)%4][ir];
			if (liquid_levels_want[ii] >= 0 &&
				liquid_levels_want[ii] < level_max) {
				++liquid_levels_want[ii];
				--total_level;
			}
		}

		// fill top block if can
		if (neighbors[D_TOP].l) {
			//infostream<<"compressing to top was="<<liquid_levels_want[D_TOP]<<" add="<<total_level<<std::endl;
			liquid_levels_want[D_TOP] = total_level>level_max_compressed?level_max_compressed:total_level;
			total_level -= liquid_levels_want[D_TOP];
		}

		if (total_level > 0) { // very rare, compressed only
			//infostream<<"compressing to self was="<<liquid_levels_want[D_SELF]<<" add="<<total_level<<std::endl;
			liquid_levels_want[D_SELF] += total_level;
			total_level = 0;
		}

		for (u16 ii = 0; ii < 7; ii++) // infinity and cave flood optimization
			if (    neighbors[ii].i			||
				(liquid_levels_want[ii] >= 0	&&
				 level_max > 1			&&
				 fast_flood			&&
				 p0.Y < water_level		&&
				 initial_size >= 1000		&&
				 ii != D_TOP			&&
				 want_level >= level_max/4	&&
				 can_liquid_same_level >= 5	&&
				 liquid_levels[D_TOP] >= level_max))
					liquid_levels_want[ii] = level_max;

		/*
		if (total_level > 0) //|| flowed != volume)
			infostream <<" AFTER level=" << (int)total_level
			//<< " flowed="<<flowed<< " volume=" << volume
			<< " max="<<(int)level_max
			<< " wantsame="<<(int)want_level<< " top="
			<< (int)liquid_levels_want[D_TOP]<< " topwas="
			<< (int)liquid_levels[D_TOP]
			<< " bot=" << (int)liquid_levels_want[D_BOTTOM] 
			<< " botwas=" << (int)liquid_levels[D_BOTTOM]
			<<std::endl;
		*/

		//s16 flowed = 0; // for debug
		for (u16 r = 0; r < 7; r++) {
			u16 i = liquid_random_map[(loopcount+loop_rand+3)%4][r];
			if (liquid_levels_want[i] < 0 || !neighbors[i].l)
				continue;

			//infostream <<" set=" <<i<< " " << PP(neighbors[i].p) << " want="<<(int)liquid_levels_want[i] << " was=" <<(int) liquid_levels[i] << std::endl;
			
			/* disabled because brokes constant volume of lava
			u8 viscosity = nodemgr->get(liquid_kind).liquid_viscosity;
			if (viscosity > 1 && liquid_levels_want[i] != liquid_levels[i]) {
				// amount to gain, limited by viscosity
				// must be at least 1 in absolute value
				s8 level_inc = liquid_levels_want[i] - liquid_levels[i];
				if (level_inc < -viscosity || level_inc > viscosity)
					new_node_level = liquid_levels[i] + level_inc/viscosity;
				else if (level_inc < 0)
					new_node_level = liquid_levels[i] - 1;
				else if (level_inc > 0)
					new_node_level = liquid_levels[i] + 1;
			} else {
			*/

			// last level must flow down on stairs
			if (liquid_levels_want[i] != liquid_levels[i] &&
				liquid_levels[D_TOP] <= 0 && (!neighbors[D_BOTTOM].l || level_max == 1) &&
				liquid_levels_want[i] >= 1 && liquid_levels_want[i] <= 2) {
				for (u16 ir = D_SELF + 1; ir < D_TOP; ++ir) { // only same level
					u16 ii = liquid_random_map[(loopcount+loop_rand+4)%4][ir];
					if (neighbors[ii].l)
						must_reflow_second.push_back(neighbors[i].p + liquid_flow_dirs[ii]);
				}
			}

			//flowed += liquid_levels_want[i];
			if (liquid_levels[i] == liquid_levels_want[i]) {
				continue;
			}

			if (neighbors[i].drop) {// && level_max > 1 && total_level >= level_max - 1
				JMutexAutoLock envlock(m_server->m_env_mutex); // 8(
				m_server->getEnv().getScriptIface()->node_drop(neighbors[i].p, 2);
			}

			neighbors[i].n.setContent(liquid_kind_flowing);
			neighbors[i].n.setLevel(nodemgr, liquid_levels_want[i], 1);

			try{
				setNode(neighbors[i].p, neighbors[i].n);
			} catch(InvalidPositionException &e) {
				infostream<<"transformLiquidsFinite: setNode() failed:"<<PP(neighbors[i].p)<<":"<<e.what()<<std::endl;
			}

			// If node emits light, MapBlock requires lighting update
			// or if node removed
			v3s16 blockpos = getNodeBlockPos(neighbors[i].p);
			MapBlock *block = getBlockNoCreateNoEx(blockpos);
			if(block != NULL) {
				modified_blocks[blockpos] = block;
				if(!nodemgr->get(neighbors[i].n).light_propagates || nodemgr->get(neighbors[i].n).light_source) // better to update always
					lighting_modified_blocks[block->getPos()] = block;
			}
			must_reflow.push_back(neighbors[i].p);


		}

		//if (total_was!=flowed) infostream<<" flowed "<<flowed<<"/"<<total_was<<std::endl;
		/* //for better relax  only same level
		if (changed)  for (u16 ii = D_SELF + 1; ii < D_TOP; ++ii) {
			if (!neighbors[ii].l) continue;
			must_reflow.push_back(p0 + dirs[ii]);
		}*/
	}

	u32 ret = loopcount >= initial_size ? 0 : m_transforming_liquid.size();
	if (ret || loopcount > m_liquid_step_flow)
		m_liquid_step_flow += (m_liquid_step_flow > loopcount ? -1 : 1) * (int)loopcount/10;
	/*
	if (loopcount)
		infostream<<"Map::transformLiquidsFinite(): loopcount="<<loopcount
		<<" avgflow="<<m_liquid_step_flow
		<<" reflow="<<must_reflow.size()
		<<" queue="<< m_transforming_liquid.size()
		<<" per="<< porting::getTimeMs() - (end_ms - 1000 * g_settings->getFloat("dedicated_server_step"))
		<<" ret="<<ret<<std::endl;
	*/

	JMutexAutoLock lock(m_transforming_liquid_mutex);

	while (must_reflow.size() > 0)
		m_transforming_liquid.push_back(must_reflow.pop_front());
	while (must_reflow_second.size() > 0)
		m_transforming_liquid.push_back(must_reflow_second.pop_front());
	//updateLighting(lighting_modified_blocks, modified_blocks);

	g_profiler->add("Server: liquids processed", loopcount);

	return ret;
}

#define WATER_DROP_BOOST 4

u32 Map::transformLiquids(Server *m_server, std::map<v3s16, MapBlock*> & modified_blocks, std::map<v3s16, MapBlock*> & lighting_modified_blocks, int max_cycle_ms)
{

	if (g_settings->getBool("liquid_real"))
		return Map::transformLiquidsFinite(m_server, modified_blocks, lighting_modified_blocks, max_cycle_ms);

	INodeDefManager *nodemgr = m_gamedef->ndef();

	DSTACK(__FUNCTION_NAME);
	//TimeTaker timer("transformLiquids()");

	JMutexAutoLock lock(m_transforming_liquid_mutex);

	u32 loopcount = 0;
	u32 initial_size = m_transforming_liquid.size();

	/*if(initial_size != 0)
		infostream<<"transformLiquids(): initial_size="<<initial_size<<std::endl;*/

	// list of nodes that due to viscosity have not reached their max level height
	UniqueQueue<v3s16> must_reflow;

	// List of MapBlocks that will require a lighting update (due to lava)
	//std::map<v3s16, MapBlock*> lighting_modified_blocks;

	u32 end_ms = porting::getTimeMs() + max_cycle_ms;

	while(m_transforming_liquid.size() != 0)
	{
		// This should be done here so that it is done when continue is used
		if(loopcount >= initial_size || porting::getTimeMs() > end_ms)
			break;
		loopcount++;

		/*
			Get a queued transforming liquid node
		*/
		v3s16 p0 = m_transforming_liquid.pop_front();

		MapNode n0 = getNodeNoEx(p0);

		/*
			Collect information about current node
		 */
		s8 liquid_level = -1;
		content_t liquid_kind = CONTENT_IGNORE;
		LiquidType liquid_type = nodemgr->get(n0).liquid_type;
		switch (liquid_type) {
			case LIQUID_SOURCE:
				liquid_level = n0.getLevel(nodemgr);
				liquid_kind = nodemgr->getId(nodemgr->get(n0).liquid_alternative_flowing);
				break;
			case LIQUID_FLOWING:
				liquid_level = n0.getLevel(nodemgr);
				liquid_kind = n0.getContent();
				break;
			case LIQUID_NONE:
				// if this is an air node, it *could* be transformed into a liquid. otherwise,
				// continue with the next node.
				if (n0.getContent() != CONTENT_AIR)
					continue;
				liquid_kind = CONTENT_AIR;
				break;
		}

		/*
			Collect information about the environment
		 */
		const v3s16 *dirs = g_6dirs;
		NodeNeighbor sources[6]; // surrounding sources
		int num_sources = 0;
		NodeNeighbor flows[6]; // surrounding flowing liquid nodes
		int num_flows = 0;
		NodeNeighbor airs[6]; // surrounding air
		int num_airs = 0;
		NodeNeighbor neutrals[6]; // nodes that are solid or another kind of liquid
		int num_neutrals = 0;
		bool flowing_down = false;
		for (u16 i = 0; i < 6; i++) {
			NeighborType nt = NEIGHBOR_SAME_LEVEL;
			switch (i) {
				case 1:
					nt = NEIGHBOR_UPPER;
					break;
				case 4:
					nt = NEIGHBOR_LOWER;
					break;
			}
			v3s16 npos = p0 + dirs[i];
			NodeNeighbor nb = {getNodeNoEx(npos), nt, npos};
			switch (nodemgr->get(nb.n.getContent()).liquid_type) {
				case LIQUID_NONE:
					if (nb.n.getContent() == CONTENT_AIR) {
						airs[num_airs++] = nb;
						// if the current node is a water source the neighbor
						// should be enqueded for transformation regardless of whether the
						// current node changes or not.
						if (nb.t != NEIGHBOR_UPPER && liquid_type != LIQUID_NONE)
							m_transforming_liquid.push_back(npos);
						// if the current node happens to be a flowing node, it will start to flow down here.
						if (nb.t == NEIGHBOR_LOWER) {
							flowing_down = true;
						}
					} else {
						neutrals[num_neutrals++] = nb;
					}
					break;
				case LIQUID_SOURCE:
					// if this node is not (yet) of a liquid type, choose the first liquid type we encounter
					if (liquid_kind == CONTENT_AIR)
						liquid_kind = nodemgr->getId(nodemgr->get(nb.n).liquid_alternative_flowing);
					if (nodemgr->getId(nodemgr->get(nb.n).liquid_alternative_flowing) != liquid_kind) {
						neutrals[num_neutrals++] = nb;
					} else {
						// Do not count bottom source, it will screw things up
						if(dirs[i].Y != -1)
							sources[num_sources++] = nb;
					}
					break;
				case LIQUID_FLOWING:
					// if this node is not (yet) of a liquid type, choose the first liquid type we encounter
					if (liquid_kind == CONTENT_AIR)
						liquid_kind = nodemgr->getId(nodemgr->get(nb.n).liquid_alternative_flowing);
					if (nodemgr->getId(nodemgr->get(nb.n).liquid_alternative_flowing) != liquid_kind) {
						neutrals[num_neutrals++] = nb;
					} else {
						flows[num_flows++] = nb;
						if (nb.t == NEIGHBOR_LOWER)
							flowing_down = true;
					}
					break;
			}
		}
		u16 level_max = nodemgr->get(liquid_kind).getMaxLevel(); // source level
		if (level_max <= 1)
			continue;
		level_max -= 1; // source - 1 = max flowing level
		/*
			decide on the type (and possibly level) of the current node
		 */
		content_t new_node_content;
		s8 new_node_level = -1;
		s8 max_node_level = -1;
		if ((num_sources >= 2 && nodemgr->get(liquid_kind).liquid_renewable) || liquid_type == LIQUID_SOURCE) {
			// liquid_kind will be set to either the flowing alternative of the node (if it's a liquid)
			// or the flowing alternative of the first of the surrounding sources (if it's air), so
			// it's perfectly safe to use liquid_kind here to determine the new node content.
			//new_node_content = nodemgr->getId(nodemgr->get(liquid_kind).liquid_alternative_source);
			//new_node_content = liquid_kind;
			//max_node_level = level_max + 1;
			new_node_level = level_max + 1;
		} else if (num_sources >= 1 && sources[0].t != NEIGHBOR_LOWER) {
			// liquid_kind is set properly, see above
			//new_node_content = liquid_kind;
			new_node_level = level_max;
		} else {
			// no surrounding sources, so get the maximum level that can flow into this node
			for (u16 i = 0; i < num_flows; i++) {
				u8 nb_liquid_level = (flows[i].n.getLevel(nodemgr));
				switch (flows[i].t) {
					case NEIGHBOR_UPPER:
						if (nb_liquid_level + WATER_DROP_BOOST > max_node_level) {
							max_node_level = level_max;
							if (nb_liquid_level + WATER_DROP_BOOST < level_max)
								max_node_level = nb_liquid_level + WATER_DROP_BOOST;
						} else if (nb_liquid_level > max_node_level)
							max_node_level = nb_liquid_level;
						break;
					case NEIGHBOR_LOWER:
						break;
					case NEIGHBOR_SAME_LEVEL:
						if ((flows[i].n.param2 & LIQUID_FLOW_DOWN_MASK) != LIQUID_FLOW_DOWN_MASK &&
							nb_liquid_level > 0 && nb_liquid_level - 1 > max_node_level) {
							max_node_level = nb_liquid_level - 1;
						}
						break;
				}
			}
			u8 viscosity = nodemgr->get(liquid_kind).liquid_viscosity;
			if (viscosity > 1 && max_node_level != liquid_level) {
				if (liquid_level < 0)
					liquid_level = 0;
				// amount to gain, limited by viscosity
				// must be at least 1 in absolute value
				s8 level_inc = max_node_level - liquid_level;
				if (level_inc < -viscosity || level_inc > viscosity)
					new_node_level = liquid_level + level_inc/viscosity;
				else if (level_inc < 0)
					new_node_level = liquid_level - 1;
				else if (level_inc > 0)
					new_node_level = liquid_level + 1;
				if (new_node_level != max_node_level)
					must_reflow.push_back(p0);
			} else
				new_node_level = max_node_level;
		}
		new_node_content = liquid_kind;

		/*
			check if anything has changed. if not, just continue with the next node.
		 */
/*
		if (new_node_content == n0.getContent() && (nodemgr->get(n0.getContent()).liquid_type != LIQUID_FLOWING ||
										 ((n0.param2 & LIQUID_LEVEL_MASK) == (u8)new_node_level &&
										 ((n0.param2 & LIQUID_FLOW_DOWN_MASK) == LIQUID_FLOW_DOWN_MASK)
										 == flowing_down)))
*/
		if (liquid_level == new_node_level || new_node_level < 0)
			continue;

//errorstream << " was="<<(int)liquid_level<<" new="<< (int)new_node_level<< " ncon="<< (int)new_node_content << " flodo="<<(int)flowing_down<< " lmax="<<level_max<< " nameNE="<<nodemgr->get(new_node_content).name<<" nums="<<(int)num_sources<<" wasname="<<nodemgr->get(n0).name<<std::endl;

		/*
			update the current node
		 */
		MapNode n00 = n0;
		//bool flow_down_enabled = (flowing_down && ((n0.param2 & LIQUID_FLOW_DOWN_MASK) != LIQUID_FLOW_DOWN_MASK));
/*
		if (nodemgr->get(new_node_content).liquid_type == LIQUID_FLOWING) {
			// set level to last 3 bits, flowing down bit to 4th bit
			n0.param2 = (flowing_down ? LIQUID_FLOW_DOWN_MASK : 0x00) | (new_node_level & LIQUID_LEVEL_MASK);
		} else {
			// set the liquid level and flow bit to 0
			n0.param2 = ~(LIQUID_LEVEL_MASK | LIQUID_FLOW_DOWN_MASK);
		}
*/
		n0.setContent(new_node_content);
		n0.setLevel(nodemgr, new_node_level); // set air, flowing, source depend on level
		if (nodemgr->get(n0).liquid_type == LIQUID_FLOWING)
			n0.param2 |= (flowing_down ? LIQUID_FLOW_DOWN_MASK : 0x00);

		// Find out whether there is a suspect for this action
		std::string suspect;
		if(m_gamedef->rollback()){
			suspect = m_gamedef->rollback()->getSuspect(p0, 83, 1);
		}

		if(!suspect.empty()){
			// Blame suspect
			RollbackScopeActor rollback_scope(m_gamedef->rollback(), suspect, true);
			// Get old node for rollback
			RollbackNode rollback_oldnode(this, p0, m_gamedef);
			// Set node
			setNode(p0, n0);
			// Report
			RollbackNode rollback_newnode(this, p0, m_gamedef);
			RollbackAction action;
			action.setSetNode(p0, rollback_oldnode, rollback_newnode);
			m_gamedef->rollback()->reportAction(action);
		} else {
			// Set node
			try{
				setNode(p0, n0);
			}
			catch(InvalidPositionException &e)
			{
				infostream<<"transformLiquids: setNode() failed:"<<PP(p0)<<":"<<e.what()<<std::endl;
			}
		}
		v3s16 blockpos = getNodeBlockPos(p0);
		MapBlock *block = getBlockNoCreateNoEx(blockpos);
		if(block != NULL) {
			modified_blocks[blockpos] =  block;
			// If new or old node emits light, MapBlock requires lighting update
			if(nodemgr->get(n0).light_source != 0 ||
					nodemgr->get(n00).light_source != 0)
				lighting_modified_blocks[block->getPos()] = block;
		}

		/*
			enqueue neighbors for update if neccessary
		 */
		switch (nodemgr->get(n0.getContent()).liquid_type) {
			case LIQUID_SOURCE:
			case LIQUID_FLOWING:
				// make sure source flows into all neighboring nodes
				for (u16 i = 0; i < num_flows; i++)
					if (flows[i].t != NEIGHBOR_UPPER)
						m_transforming_liquid.push_back(flows[i].p);
				for (u16 i = 0; i < num_airs; i++)
					if (airs[i].t != NEIGHBOR_UPPER)
						m_transforming_liquid.push_back(airs[i].p);
				break;
			case LIQUID_NONE:
				// this flow has turned to air; neighboring flows might need to do the same
				for (u16 i = 0; i < num_flows; i++)
					m_transforming_liquid.push_back(flows[i].p);
				break;
		}
	}

	u32 ret = loopcount >= initial_size ? 0 : m_transforming_liquid.size();

	//infostream<<"Map::transformLiquids(): loopcount="<<loopcount<<" per="<<timer.getTimerTime()<<" ret="<<ret<<std::endl;

	while (must_reflow.size() > 0)
		m_transforming_liquid.push_back(must_reflow.pop_front());
	//updateLighting(lighting_modified_blocks, modified_blocks);

	g_profiler->add("Server: liquids processed", loopcount);

	return ret;
}

NodeMetadata *Map::getNodeMetadata(v3s16 p)
{
	v3s16 blockpos = getNodeBlockPos(p);
	v3s16 p_rel = p - blockpos*MAP_BLOCKSIZE;
	MapBlock *block = getBlockNoCreateNoEx(blockpos);
	if(!block){
		infostream<<"Map::getNodeMetadata(): Need to emerge "
				<<PP(blockpos)<<std::endl;
		block = emergeBlock(blockpos, false);
	}
	if(!block){
		infostream<<"WARNING: Map::getNodeMetadata(): Block not found"
				<<std::endl;
		return NULL;
	}
	NodeMetadata *meta = block->m_node_metadata.get(p_rel);
	return meta;
}

bool Map::setNodeMetadata(v3s16 p, NodeMetadata *meta)
{
	v3s16 blockpos = getNodeBlockPos(p);
	v3s16 p_rel = p - blockpos*MAP_BLOCKSIZE;
	MapBlock *block = getBlockNoCreateNoEx(blockpos);
	if(!block){
		infostream<<"Map::setNodeMetadata(): Need to emerge "
				<<PP(blockpos)<<std::endl;
		block = emergeBlock(blockpos, false);
	}
	if(!block){
		infostream<<"WARNING: Map::setNodeMetadata(): Block not found"
				<<std::endl;
		return false;
	}
	block->m_node_metadata.set(p_rel, meta);
	return true;
}

void Map::removeNodeMetadata(v3s16 p)
{
	v3s16 blockpos = getNodeBlockPos(p);
	v3s16 p_rel = p - blockpos*MAP_BLOCKSIZE;
	MapBlock *block = getBlockNoCreateNoEx(blockpos);
	if(block == NULL)
	{
		infostream<<"WARNING: Map::removeNodeMetadata(): Block not found"
				<<std::endl;
		return;
	}
	block->m_node_metadata.remove(p_rel);
}

NodeTimer Map::getNodeTimer(v3s16 p)
{
	v3s16 blockpos = getNodeBlockPos(p);
	v3s16 p_rel = p - blockpos*MAP_BLOCKSIZE;
	MapBlock *block = getBlockNoCreateNoEx(blockpos);
	if(!block){
		infostream<<"Map::getNodeTimer(): Need to emerge "
				<<PP(blockpos)<<std::endl;
		block = emergeBlock(blockpos, false);
	}
	if(!block){
		infostream<<"WARNING: Map::getNodeTimer(): Block not found"
				<<std::endl;
		return NodeTimer();
	}
	NodeTimer t = block->m_node_timers.get(p_rel);
	return t;
}

void Map::setNodeTimer(v3s16 p, NodeTimer t)
{
	v3s16 blockpos = getNodeBlockPos(p);
	v3s16 p_rel = p - blockpos*MAP_BLOCKSIZE;
	MapBlock *block = getBlockNoCreateNoEx(blockpos);
	if(!block){
		infostream<<"Map::setNodeTimer(): Need to emerge "
				<<PP(blockpos)<<std::endl;
		block = emergeBlock(blockpos, false);
	}
	if(!block){
		infostream<<"WARNING: Map::setNodeTimer(): Block not found"
				<<std::endl;
		return;
	}
	block->m_node_timers.set(p_rel, t);
}

void Map::removeNodeTimer(v3s16 p)
{
	v3s16 blockpos = getNodeBlockPos(p);
	v3s16 p_rel = p - blockpos*MAP_BLOCKSIZE;
	MapBlock *block = getBlockNoCreateNoEx(blockpos);
	if(block == NULL)
	{
		infostream<<"WARNING: Map::removeNodeTimer(): Block not found"
				<<std::endl;
		return;
	}
	block->m_node_timers.remove(p_rel);
}

s16 Map::getHeat(v3s16 p, bool no_random)
{
	MapBlock *block = getBlockNoCreateNoEx(getNodeBlockPos(p));
	if(block != NULL) {
		s16 value = block->heat;
		// compatibility with minetest
		if (value == HUMIDITY_UNDEFINED)
			return 0;
		return value + (no_random ? 0 : myrand_range(0, 1));
	}
	//errorstream << "No heat for " << p.X<<"," << p.Z << std::endl;
	return 0;
}

s16 Map::getHumidity(v3s16 p, bool no_random)
{
	MapBlock *block = getBlockNoCreateNoEx(getNodeBlockPos(p));
	if(block != NULL) {
		s16 value = block->humidity;
		// compatibility with minetest
		if (value == HUMIDITY_UNDEFINED)
			return 0;
		return value + (no_random ? 0 : myrand_range(0, 1));
	}
	//errorstream << "No humidity for " << p.X<<"," << p.Z << std::endl;
	return 0;
}

/*
	ServerMap
*/
ServerMap::ServerMap(std::string savedir, IGameDef *gamedef, EmergeManager *emerge, Circuit* circuit):
	Map(dout_server, gamedef, circuit),
	m_emerge(emerge),
	m_map_metadata_changed(true)
{
	verbosestream<<__FUNCTION_NAME<<std::endl;

	/*
		Try to load map; if not found, create a new one.
	*/

	// Determine which database backend to use
	std::string conf_path = savedir + DIR_DELIM + "world.mt";
	Settings conf;
	bool succeeded = conf.readConfigFile(conf_path.c_str());
	if (!succeeded || !conf.exists("backend")) {
		// fall back to sqlite3
		dbase = new Database_SQLite3(this, savedir);
		conf.set("backend", "sqlite3");
	} else {
		std::string backend = conf.get("backend");
		if (backend == "dummy")
			dbase = new Database_Dummy(this);
		else if (backend == "sqlite3")
			dbase = new Database_SQLite3(this, savedir);
		#if USE_LEVELDB
		else if (backend == "leveldb")
			dbase = new Database_LevelDB(this, savedir);
		#endif
		#if USE_REDIS
		else if (backend == "redis")
			dbase = new Database_Redis(this, savedir);
		#endif
		else
			throw BaseException("Unknown map backend");
	}

	m_savedir = savedir;
	m_map_saving_enabled = false;

	try
	{
		// If directory exists, check contents and load if possible
		if(fs::PathExists(m_savedir))
		{
			// If directory is empty, it is safe to save into it.
			if(fs::GetDirListing(m_savedir).size() == 0)
			{
				infostream<<"ServerMap: Empty save directory is valid."
						<<std::endl;
				m_map_saving_enabled = true;
			}
			else
			{
				try{
					// Load map metadata (seed, chunksize)
					loadMapMeta();
				}
				catch(SettingNotFoundException &e){
					infostream<<"ServerMap:  Some metadata not found."
							  <<" Using default settings."<<std::endl;
				}
				catch(FileNotGoodException &e){
					infostream<<"WARNING: Could not load map metadata"
							//<<" Disabling chunk-based generator."
							<<std::endl;
					//m_chunksize = 0;
				}

				infostream<<"ServerMap: Successfully loaded map "
						<<"metadata from "<<savedir
						<<", assuming valid save directory."
						<<" seed="<< m_emerge->params.seed <<"."
						<<std::endl;

				m_map_saving_enabled = true;
				// Map loaded, not creating new one
				return;
			}
		}
		// If directory doesn't exist, it is safe to save to it
		else{
			m_map_saving_enabled = true;
		}
	}
	catch(std::exception &e)
	{
		infostream<<"WARNING: ServerMap: Failed to load map from "<<savedir
				<<", exception: "<<e.what()<<std::endl;
		infostream<<"Please remove the map or fix it."<<std::endl;
		infostream<<"WARNING: Map saving will be disabled."<<std::endl;
	}

	infostream<<"Initializing new map."<<std::endl;

	// Create zero sector
	emergeSector(v2s16(0,0));

	// Initially write whole map
	save(MOD_STATE_CLEAN);
}

ServerMap::~ServerMap()
{
	verbosestream<<__FUNCTION_NAME<<std::endl;

	try
	{
		if(m_map_saving_enabled)
		{
			// Save only changed parts
			save(MOD_STATE_WRITE_AT_UNLOAD);
			infostream<<"ServerMap: Saved map to "<<m_savedir<<std::endl;
		}
		else
		{
			infostream<<"ServerMap: Map not saved"<<std::endl;
		}
	}
	catch(std::exception &e)
	{
		infostream<<"ServerMap: Failed to save map to "<<m_savedir
				<<", exception: "<<e.what()<<std::endl;
	}

	/*
		Close database if it was opened
	*/
	delete dbase;

#if 0
	/*
		Free all MapChunks
	*/
	core::map<v2s16, MapChunk*>::Iterator i = m_chunks.getIterator();
	for(; i.atEnd() == false; i++)
	{
		MapChunk *chunk = i.getNode()->getValue();
		delete chunk;
	}
#endif
}

u64 ServerMap::getSeed()
{
	return m_emerge->params.seed;
}

s16 ServerMap::getWaterLevel()
{
	return m_emerge->params.water_level;
}

bool ServerMap::initBlockMake(BlockMakeData *data, v3s16 blockpos)
{
	bool enable_mapgen_debug_info = m_emerge->mapgen_debug_info;
	EMERGE_DBG_OUT("initBlockMake(): " PP(blockpos) " - " PP(blockpos));

	s16 chunksize = m_emerge->params.chunksize;
	s16 coffset = -chunksize / 2;
	v3s16 chunk_offset(coffset, coffset, coffset);
	v3s16 blockpos_div = getContainerPos(blockpos - chunk_offset, chunksize);
	v3s16 blockpos_min = blockpos_div * chunksize;
	v3s16 blockpos_max = blockpos_div * chunksize + v3s16(1,1,1)*(chunksize-1);
	blockpos_min += chunk_offset;
	blockpos_max += chunk_offset;

	v3s16 extra_borders(1,1,1);

	// Do nothing if not inside limits (+-1 because of neighbors)
	if(blockpos_over_limit(blockpos_min - extra_borders) ||
		blockpos_over_limit(blockpos_max + extra_borders))
		return false;

	data->seed = m_emerge->params.seed;
	data->blockpos_min = blockpos_min;
	data->blockpos_max = blockpos_max;
	data->blockpos_requested = blockpos;
	data->nodedef = m_gamedef->ndef();

	/*
		Create the whole area of this and the neighboring blocks
	*/
	{
		//TimeTaker timer("initBlockMake() create area");

		for(s16 x=blockpos_min.X-extra_borders.X;
				x<=blockpos_max.X+extra_borders.X; x++)
		for(s16 z=blockpos_min.Z-extra_borders.Z;
				z<=blockpos_max.Z+extra_borders.Z; z++)
		{
			v2s16 sectorpos(x, z);
			// Sector metadata is loaded from disk if not already loaded.
			ServerMapSector *sector = createSector(sectorpos);
			assert(sector);
			(void) sector;

			for(s16 y=blockpos_min.Y-extra_borders.Y;
					y<=blockpos_max.Y+extra_borders.Y; y++)
			{
				v3s16 p(x,y,z);
				//MapBlock *block = createBlock(p);
				// 1) get from memory, 2) load from disk
				MapBlock *block = emergeBlock(p, false);
				// 3) create a blank one
				if(block == NULL)
				{
					block = createBlock(p);

					/*
						Block gets sunlight if this is true.

						Refer to the map generator heuristics.
					*/
					bool ug = m_emerge->isBlockUnderground(p);
					block->setIsUnderground(ug);
				}

				// Lighting will not be valid after make_chunk is called
				block->setLightingExpired(true);
				// Lighting will be calculated
				//block->setLightingExpired(false);
			}
		}
	}

	/*
		Now we have a big empty area.

		Make a ManualMapVoxelManipulator that contains this and the
		neighboring blocks
	*/

	// The area that contains this block and it's neighbors
	v3s16 bigarea_blocks_min = blockpos_min - extra_borders;
	v3s16 bigarea_blocks_max = blockpos_max + extra_borders;

	data->vmanip = new ManualMapVoxelManipulator(this);
	//data->vmanip->setMap(this);

	// Add the area
	{
		//TimeTaker timer("initBlockMake() initialEmerge");
		data->vmanip->initialEmerge(bigarea_blocks_min, bigarea_blocks_max, false);
	}
	
	// Ensure none of the blocks to be generated were marked as containing CONTENT_IGNORE
/*	for (s16 z = blockpos_min.Z; z <= blockpos_max.Z; z++) {
		for (s16 y = blockpos_min.Y; y <= blockpos_max.Y; y++) {
			for (s16 x = blockpos_min.X; x <= blockpos_max.X; x++) {
				core::map<v3s16, u8>::Node *n;
				n = data->vmanip->m_loaded_blocks.find(v3s16(x, y, z));
				if (n == NULL)
					continue;
				u8 flags = n->getValue();
				flags &= ~VMANIP_BLOCK_CONTAINS_CIGNORE;
				n->setValue(flags);
			}
		}
	}*/

	// Data is ready now.
	return true;
}

MapBlock* ServerMap::finishBlockMake(BlockMakeData *data,
		std::map<v3s16, MapBlock*> &changed_blocks)
{
	v3s16 blockpos_min = data->blockpos_min;
	v3s16 blockpos_max = data->blockpos_max;
	v3s16 blockpos_requested = data->blockpos_requested;
	/*infostream<<"finishBlockMake(): ("<<blockpos_requested.X<<","
			<<blockpos_requested.Y<<","
			<<blockpos_requested.Z<<")"<<std::endl;*/

	v3s16 extra_borders(1,1,1);

	bool enable_mapgen_debug_info = m_emerge->mapgen_debug_info;

	/*infostream<<"Resulting vmanip:"<<std::endl;
	data->vmanip.print(infostream);*/

	// Make sure affected blocks are loaded
	for(s16 x=blockpos_min.X-extra_borders.X;
			x<=blockpos_max.X+extra_borders.X; x++)
	for(s16 z=blockpos_min.Z-extra_borders.Z;
			z<=blockpos_max.Z+extra_borders.Z; z++)
	for(s16 y=blockpos_min.Y-extra_borders.Y;
			y<=blockpos_max.Y+extra_borders.Y; y++)
	{
		v3s16 p(x, y, z);
		// Load from disk if not already in memory
		emergeBlock(p, false);
	}

	/*
		Blit generated stuff to map
		NOTE: blitBackAll adds nearly everything to changed_blocks
	*/
	{
		// 70ms @cs=8
		//TimeTaker timer("finishBlockMake() blitBackAll");
		data->vmanip->blitBackAll(&changed_blocks);
	}

	EMERGE_DBG_OUT("finishBlockMake: changed_blocks.size()=" << changed_blocks.size());

	/*
		Copy transforming liquid information
	*/
	{
	JMutexAutoLock lock(m_transforming_liquid_mutex);
	while(data->transforming_liquid.size() > 0)
	{
		v3s16 p = data->transforming_liquid.pop_front();
		m_transforming_liquid.push_back(p);
	}
	}

	/*
		Do stuff in central blocks
	*/

	/*
		Update lighting
	*/
	{
#if 0
		TimeTaker t("finishBlockMake lighting update");

		core::map<v3s16, MapBlock*> lighting_update_blocks;

		// Center blocks
		for(s16 x=blockpos_min.X-extra_borders.X;
				x<=blockpos_max.X+extra_borders.X; x++)
		for(s16 z=blockpos_min.Z-extra_borders.Z;
				z<=blockpos_max.Z+extra_borders.Z; z++)
		for(s16 y=blockpos_min.Y-extra_borders.Y;
				y<=blockpos_max.Y+extra_borders.Y; y++)
		{
			v3s16 p(x, y, z);
			MapBlock *block = getBlockNoCreateNoEx(p);
			assert(block);
			lighting_update_blocks.insert(block->getPos(), block);
		}

		updateLighting(lighting_update_blocks, changed_blocks);
#endif

		/*
			Set lighting to non-expired state in all of them.
			This is cheating, but it is not fast enough if all of them
			would actually be updated.
		*/
		for(s16 x=blockpos_min.X-extra_borders.X;
				x<=blockpos_max.X+extra_borders.X; x++)
		for(s16 z=blockpos_min.Z-extra_borders.Z;
				z<=blockpos_max.Z+extra_borders.Z; z++)
		for(s16 y=blockpos_min.Y-extra_borders.Y;
				y<=blockpos_max.Y+extra_borders.Y; y++)
		{
			v3s16 p(x, y, z);
			MapBlock *block = getBlockNoCreateNoEx(p);
			if (block == NULL)
				continue;
			block->setLightingExpired(false);
		}

#if 0
		if(enable_mapgen_debug_info == false)
			t.stop(true); // Hide output
#endif
	}

	/*
		Go through changed blocks
	*/
	for(std::map<v3s16, MapBlock*>::iterator i = changed_blocks.begin();
			i != changed_blocks.end(); ++i)
	{
		MapBlock *block = i->second;
		if (block == NULL)
			continue;
		assert(block);
		/*
			Update day/night difference cache of the MapBlocks
		*/
		block->expireDayNightDiff();
		/*
			Set block as modified
		*/
/*
		block->raiseModified(MOD_STATE_WRITE_NEEDED,
				"finishBlockMake expireDayNightDiff");
*/
	}

	/*
		Set central blocks as generated
	*/
	for(s16 x=blockpos_min.X; x<=blockpos_max.X; x++)
	for(s16 z=blockpos_min.Z; z<=blockpos_max.Z; z++)
	for(s16 y=blockpos_min.Y; y<=blockpos_max.Y; y++)
	{
		v3s16 p(x, y, z);
		MapBlock *block = getBlockNoCreateNoEx(p);
		if (block == NULL)
			continue;
		assert(block);
		block->setGenerated(true);
	}

	/*
		Save changed parts of map
		NOTE: Will be saved later.
	*/
	//save(MOD_STATE_WRITE_AT_UNLOAD);

	/*infostream<<"finishBlockMake() done for ("<<blockpos_requested.X
			<<","<<blockpos_requested.Y<<","
			<<blockpos_requested.Z<<")"<<std::endl;*/
			
	/*
		Update weather data in blocks
	*/
	ServerEnvironment *senv = &((Server *)m_gamedef)->getEnv();
	for(s16 x=blockpos_min.X-extra_borders.X;x<=blockpos_max.X+extra_borders.X; x++)
	for(s16 z=blockpos_min.Z-extra_borders.Z;z<=blockpos_max.Z+extra_borders.Z; z++)
	for(s16 y=blockpos_min.Y-extra_borders.Y;y<=blockpos_max.Y+extra_borders.Y; y++) {
		v3s16 p(x, y, z);
		MapBlock *block = getBlockNoCreateNoEx(p);
		if (block == NULL)
			continue;
		updateBlockHeat(senv, p * MAP_BLOCKSIZE, block);
		updateBlockHumidity(senv, p * MAP_BLOCKSIZE, block);
	}

#if 0
	if(enable_mapgen_debug_info)
	{
		/*
			Analyze resulting blocks
		*/
		/*for(s16 x=blockpos_min.X-1; x<=blockpos_max.X+1; x++)
		for(s16 z=blockpos_min.Z-1; z<=blockpos_max.Z+1; z++)
		for(s16 y=blockpos_min.Y-1; y<=blockpos_max.Y+1; y++)*/
		for(s16 x=blockpos_min.X-0; x<=blockpos_max.X+0; x++)
		for(s16 z=blockpos_min.Z-0; z<=blockpos_max.Z+0; z++)
		for(s16 y=blockpos_min.Y-0; y<=blockpos_max.Y+0; y++)
		{
			v3s16 p = v3s16(x,y,z);
			MapBlock *block = getBlockNoCreateNoEx(p);
			char spos[20];
			snprintf(spos, 20, "(%2d,%2d,%2d)", x, y, z);
			infostream<<"Generated "<<spos<<": "
					<<analyze_block(block)<<std::endl;
		}
	}
#endif

	MapBlock *block = getBlockNoCreateNoEx(blockpos_requested);
	if(!block) {
		errorstream<<"finishBlockMake(): created NULL block at "<<PP(blockpos_requested)<<std::endl;
	}

	return block;
}

ServerMapSector * ServerMap::createSector(v2s16 p2d)
{
	DSTACKF("%s: p2d=(%d,%d)",
			__FUNCTION_NAME,
			p2d.X, p2d.Y);
	/*
		Check if it exists already in memory
	*/
	ServerMapSector *sector = (ServerMapSector*)getSectorNoGenerateNoEx(p2d);
	if(sector != NULL)
		return sector;

	/*
		Try to load it from disk (with blocks)
	*/
	//if(loadSectorFull(p2d) == true)

	/*
		Try to load metadata from disk
	*/
#if 0
	if(loadSectorMeta(p2d) == true)
	{
		ServerMapSector *sector = (ServerMapSector*)getSectorNoGenerateNoEx(p2d);
		if(sector == NULL)
		{
			infostream<<"ServerMap::createSector(): loadSectorFull didn't make a sector"<<std::endl;
			throw InvalidPositionException("");
		}
		return sector;
	}
#endif
	/*
		Do not create over-limit
	*/
	if(p2d.X < -MAP_GENERATION_LIMIT / MAP_BLOCKSIZE
	|| p2d.X > MAP_GENERATION_LIMIT / MAP_BLOCKSIZE
	|| p2d.Y < -MAP_GENERATION_LIMIT / MAP_BLOCKSIZE
	|| p2d.Y > MAP_GENERATION_LIMIT / MAP_BLOCKSIZE)
		throw InvalidPositionException("createSector(): pos. over limit");

	/*
		Generate blank sector
	*/

	sector = new ServerMapSector(this, p2d, m_gamedef);

	// Sector position on map in nodes
	//v2s16 nodepos2d = p2d * MAP_BLOCKSIZE;

	/*
		Insert to container
	*/
	m_sectors[p2d] = sector;

	return sector;
}

#if 0
/*
	This is a quick-hand function for calling makeBlock().
*/
MapBlock * ServerMap::generateBlock(
		v3s16 p,
		std::map<v3s16, MapBlock*> &modified_blocks
)
{
	DSTACKF("%s: p=(%d,%d,%d)", __FUNCTION_NAME, p.X, p.Y, p.Z);

	/*infostream<<"generateBlock(): "
			<<"("<<p.X<<","<<p.Y<<","<<p.Z<<")"
			<<std::endl;*/

	bool enable_mapgen_debug_info = g_settings->getBool("enable_mapgen_debug_info");

	TimeTaker timer("generateBlock");

	//MapBlock *block = original_dummy;

	v2s16 p2d(p.X, p.Z);
	v2s16 p2d_nodes = p2d * MAP_BLOCKSIZE;

	/*
		Do not generate over-limit
	*/
	if(blockpos_over_limit(p))
	{
		infostream<<__FUNCTION_NAME<<": Block position over limit"<<std::endl;
		throw InvalidPositionException("generateBlock(): pos. over limit");
	}

	/*
		Create block make data
	*/
	BlockMakeData data;
	initBlockMake(&data, p);

	/*
		Generate block
	*/
	{
		TimeTaker t("mapgen::make_block()");
		mapgen->makeChunk(&data);
		//mapgen::make_block(&data);

		if(enable_mapgen_debug_info == false)
			t.stop(true); // Hide output
	}

	/*
		Blit data back on map, update lighting, add mobs and whatever this does
	*/
	finishBlockMake(&data, modified_blocks);

	/*
		Get central block
	*/
	MapBlock *block = getBlockNoCreateNoEx(p);

#if 0
	/*
		Check result
	*/
	if(block)
	{
		bool erroneus_content = false;
		for(s16 z0=0; z0<MAP_BLOCKSIZE; z0++)
		for(s16 y0=0; y0<MAP_BLOCKSIZE; y0++)
		for(s16 x0=0; x0<MAP_BLOCKSIZE; x0++)
		{
			v3s16 p(x0,y0,z0);
			MapNode n = block->getNode(p);
			if(n.getContent() == CONTENT_IGNORE)
			{
				infostream<<"CONTENT_IGNORE at "
						<<"("<<p.X<<","<<p.Y<<","<<p.Z<<")"
						<<std::endl;
				erroneus_content = true;
				assert(0);
			}
		}
		if(erroneus_content)
		{
			assert(0);
		}
	}
#endif

#if 0
	/*
		Generate a completely empty block
	*/
	if(block)
	{
		for(s16 z0=0; z0<MAP_BLOCKSIZE; z0++)
		for(s16 x0=0; x0<MAP_BLOCKSIZE; x0++)
		{
			for(s16 y0=0; y0<MAP_BLOCKSIZE; y0++)
			{
				MapNode n;
				n.setContent(CONTENT_AIR);
				block->setNode(v3s16(x0,y0,z0), n);
			}
		}
	}
#endif

	if(enable_mapgen_debug_info == false)
		timer.stop(true); // Hide output

	return block;
}
#endif

MapBlock * ServerMap::createBlock(v3s16 p)
{
	DSTACKF("%s: p=(%d,%d,%d)",
			__FUNCTION_NAME, p.X, p.Y, p.Z);

	/*
		Do not create over-limit
	*/
	if(p.X < -MAP_GENERATION_LIMIT / MAP_BLOCKSIZE
	|| p.X > MAP_GENERATION_LIMIT / MAP_BLOCKSIZE
	|| p.Y < -MAP_GENERATION_LIMIT / MAP_BLOCKSIZE
	|| p.Y > MAP_GENERATION_LIMIT / MAP_BLOCKSIZE
	|| p.Z < -MAP_GENERATION_LIMIT / MAP_BLOCKSIZE
	|| p.Z > MAP_GENERATION_LIMIT / MAP_BLOCKSIZE)
		throw InvalidPositionException("createBlock(): pos. over limit");

	v2s16 p2d(p.X, p.Z);
	s16 block_y = p.Y;
	/*
		This will create or load a sector if not found in memory.
		If block exists on disk, it will be loaded.

		NOTE: On old save formats, this will be slow, as it generates
		      lighting on blocks for them.
	*/
	ServerMapSector *sector;
	try{
		sector = (ServerMapSector*)createSector(p2d);
		assert(sector->getId() == MAPSECTOR_SERVER);
	}
	catch(InvalidPositionException &e)
	{
		infostream<<"createBlock: createSector() failed"<<std::endl;
		throw e;
	}
	/*
		NOTE: This should not be done, or at least the exception
		should not be passed on as std::exception, because it
		won't be catched at all.
	*/
	/*catch(std::exception &e)
	{
		infostream<<"createBlock: createSector() failed: "
				<<e.what()<<std::endl;
		throw e;
	}*/

	/*
		Try to get a block from the sector
	*/

	MapBlock *block = sector->getBlockNoCreateNoEx(block_y);
	if(block)
	{
		if(block->isDummy())
			block->unDummify();
		return block;
	}
	// Create blank
	block = sector->createBlankBlock(block_y);

	return block;
}

MapBlock * ServerMap::emergeBlock(v3s16 p, bool create_blank)
{
	DSTACKF("%s: p=(%d,%d,%d), create_blank=%d",
			__FUNCTION_NAME,
			p.X, p.Y, p.Z, create_blank);
	{
		MapBlock *block = getBlockNoCreateNoEx(p);
		if(block && block->isDummy() == false)
		{
			return block;
		}
	}

	{
		MapBlock *block = loadBlock(p);
		m_circuit->processElementsQueue(*this, m_gamedef->ndef());
		if(block)
			return block;
	}

	if (create_blank) {
		ServerMapSector *sector = createSector(v2s16(p.X, p.Z));
		MapBlock *block = sector->createBlankBlock(p.Y);

		return block;
	}

#if 0
	if(allow_generate)
	{
		std::map<v3s16, MapBlock*> modified_blocks;
		MapBlock *block = generateBlock(p, modified_blocks);
		if(block)
		{
			MapEditEvent event;
			event.type = MEET_OTHER;
			event.p = p;

			// Copy modified_blocks to event
			for(std::map<v3s16, MapBlock*>::iterator
					i = modified_blocks.begin();
					i != modified_blocks.end(); ++i)
			{
				event.modified_blocks.insert(i->first);
			}

			// Queue event
			dispatchEvent(&event);

			return block;
		}
	}
#endif

	return NULL;
}

MapBlock *ServerMap::getBlockOrEmerge(v3s16 p3d)
{
	MapBlock *block = getBlockNoCreateNoEx(p3d);
	if (block == NULL)
		m_emerge->enqueueBlockEmerge(PEER_ID_INEXISTENT, p3d, false);

	return block;
}

void ServerMap::prepareBlock(MapBlock *block) {
	ServerEnvironment *senv = &((Server *)m_gamedef)->getEnv();

	// Calculate weather conditions
	//block->heat_last_update     = 0;
	//block->humidity_last_update = 0;
	v3s16 p = block->getPos() *  MAP_BLOCKSIZE;
	updateBlockHeat(senv, p, block);
	updateBlockHumidity(senv, p, block);
}

/**
 * Get the ground level by searching for a non CONTENT_AIR node in a column from top to bottom
 */
s16 ServerMap::findGroundLevel(v2s16 p2d, bool cacheBlocks)
{
	
	s16 level;

	// The reference height is the original mapgen height
	s16 referenceHeight = m_emerge->getGroundLevelAtPoint(p2d);
	s16 maxSearchHeight =  63 + referenceHeight;
	s16 minSearchHeight = -63 + referenceHeight;
	v3s16 probePosition(p2d.X, maxSearchHeight, p2d.Y);
	v3s16 blockPosition = getNodeBlockPos(probePosition);
	v3s16 prevBlockPosition = blockPosition;

	// Cache the block to be inspected.
	if(cacheBlocks) {
		emergeBlock(blockPosition, true);
	}

	// Probes the nodes in the given column
	for(; probePosition.Y > minSearchHeight; probePosition.Y--)
	{
		if(cacheBlocks) {
			// Calculate the block position of the given node
			blockPosition = getNodeBlockPos(probePosition); 

			// If the node is in an different block, cache it
			if(blockPosition != prevBlockPosition) {
				emergeBlock(blockPosition, true);
				prevBlockPosition = blockPosition;
			}
		}

		MapNode node = getNodeNoEx(probePosition);
		if (node.getContent() != CONTENT_IGNORE &&
		    node.getContent() != CONTENT_AIR) {
			break;
		}
	}

	// Could not determine the ground. Use map generator noise functions.
	if(probePosition.Y == minSearchHeight) {
		level = referenceHeight; 
	} else {
		level = probePosition.Y;
	}

	return level;
}

bool ServerMap::loadFromFolders() {
	if(!dbase->Initialized() && !fs::PathExists(m_savedir + DIR_DELIM + "map.sqlite")) // ?
		return true;
	return false;
}

void ServerMap::createDirs(std::string path)
{
	if(fs::CreateAllDirs(path) == false)
	{
		m_dout<<DTIME<<"ServerMap: Failed to create directory "
				<<"\""<<path<<"\""<<std::endl;
		throw BaseException("ServerMap failed to create directory");
	}
}

std::string ServerMap::getSectorDir(v2s16 pos, int layout)
{
	char cc[9];
	switch(layout)
	{
		case 1:
			snprintf(cc, 9, "%.4x%.4x",
				(unsigned int)pos.X&0xffff,
				(unsigned int)pos.Y&0xffff);

			return m_savedir + DIR_DELIM + "sectors" + DIR_DELIM + cc;
		case 2:
			snprintf(cc, 9, "%.3x" DIR_DELIM "%.3x",
				(unsigned int)pos.X&0xfff,
				(unsigned int)pos.Y&0xfff);

			return m_savedir + DIR_DELIM + "sectors2" + DIR_DELIM + cc;
		default:
			assert(false);
			return "";
	}
	return "";
}

v2s16 ServerMap::getSectorPos(std::string dirname)
{
	unsigned int x = 0, y = 0;
	int r;
	std::string component;
	fs::RemoveLastPathComponent(dirname, &component, 1);
	if(component.size() == 8)
	{
		// Old layout
		r = sscanf(component.c_str(), "%4x%4x", &x, &y);
	}
	else if(component.size() == 3)
	{
		// New layout
		fs::RemoveLastPathComponent(dirname, &component, 2);
		r = sscanf(component.c_str(), "%3x" DIR_DELIM "%3x", &x, &y);
		// Sign-extend the 12 bit values up to 16 bits...
		if(x&0x800) x|=0xF000;
		if(y&0x800) y|=0xF000;
	}
	else
	{
		assert(false);
	}
	assert(r == 2);
	v2s16 pos((s16)x, (s16)y);
	return pos;
}

v3s16 ServerMap::getBlockPos(std::string sectordir, std::string blockfile)
{
	v2s16 p2d = getSectorPos(sectordir);

	if(blockfile.size() != 4){
		throw InvalidFilenameException("Invalid block filename");
	}
	unsigned int y;
	int r = sscanf(blockfile.c_str(), "%4x", &y);
	if(r != 1)
		throw InvalidFilenameException("Invalid block filename");
	return v3s16(p2d.X, y, p2d.Y);
}

std::string ServerMap::getBlockFilename(v3s16 p)
{
	char cc[5];
	snprintf(cc, 5, "%.4x", (unsigned int)p.Y&0xffff);
	return cc;
}

s32 ServerMap::save(ModifiedState save_level, bool breakable)
{
	DSTACK(__FUNCTION_NAME);
	if(m_map_saving_enabled == false)
	{
		infostream<<"WARNING: Not saving map, saving disabled."<<std::endl;
		return 0;
	}

	if(save_level == MOD_STATE_CLEAN)
		infostream<<"ServerMap: Saving whole map, this can take time."
				<<std::endl;

	if(m_map_metadata_changed || save_level == MOD_STATE_CLEAN)
	{
		saveMapMeta();
	}

	// Profile modified reasons
	Profiler modprofiler;

	u32 sector_meta_count = 0;
	u32 block_count = 0;
	u32 block_count_all = 0; // Number of blocks in memory

	// Don't do anything with sqlite unless something is really saved
	bool save_started = false;
	u32 n = 0, calls = 0, end_ms = porting::getTimeMs() + u32(1000 * g_settings->getFloat("dedicated_server_step"));
	if (!breakable)
		m_sectors_save_last = 0;
	for(std::map<v2s16, MapSector*>::iterator i = m_sectors.begin();
		i != m_sectors.end(); ++i)
	{
			if (n++ < m_sectors_save_last)
				continue;
			else
				m_sectors_save_last = 0;
			++calls;

		ServerMapSector *sector = (ServerMapSector*)i->second;
		assert(sector->getId() == MAPSECTOR_SERVER);

/*
		if(sector->differs_from_disk || save_level == MOD_STATE_CLEAN)
		{
			saveSectorMeta(sector);
			sector_meta_count++;
		}
*/
		std::list<MapBlock*> blocks;
		sector->getBlocks(blocks);

		for(std::list<MapBlock*>::iterator j = blocks.begin();
			j != blocks.end(); ++j)
		{
			MapBlock *block = *j;

			block_count_all++;

			if(block->getModified() >= (u32)save_level)
			{
				// Lazy beginSave()
				if(!save_started){
					beginSave();
					save_started = true;
				}

				modprofiler.add(block->getModifiedReason(), 1);

				saveBlock(block);
				block_count++;

				/*infostream<<"ServerMap: Written block ("
						<<block->getPos().X<<","
						<<block->getPos().Y<<","
						<<block->getPos().Z<<")"
						<<std::endl;*/
			}
		}
		if (breakable && porting::getTimeMs() > end_ms) {
				m_sectors_save_last = n;
				break;
		}
	}
	if (!calls)
		m_sectors_save_last = 0;

	if(save_started)
		endSave();

	/*
		Only print if something happened or saved whole map
	*/
	if(save_level == MOD_STATE_CLEAN || sector_meta_count != 0
			|| block_count != 0)
	{
		infostream<<"ServerMap: Written: "
				<<sector_meta_count<<" sector metadata files, "
				<<block_count<<" block files"
				<<", "<<block_count_all<<" blocks in memory."
				<<std::endl;
		PrintInfo(infostream); // ServerMap/ClientMap:
		infostream<<"Blocks modified by: "<<std::endl;
		modprofiler.print(infostream);
	}
	return m_sectors_save_last;
}

void ServerMap::listAllLoadableBlocks(std::list<v3s16> &dst)
{
	if(loadFromFolders()){
		errorstream<<"Map::listAllLoadableBlocks(): Result will be missing "
				<<"all blocks that are stored in flat files"<<std::endl;
	}
	dbase->listAllLoadableBlocks(dst);
}

void ServerMap::listAllLoadedBlocks(std::list<v3s16> &dst)
{
	for(std::map<v2s16, MapSector*>::iterator si = m_sectors.begin();
		si != m_sectors.end(); ++si)
	{
		MapSector *sector = si->second;

		std::list<MapBlock*> blocks;
		sector->getBlocks(blocks);

		for(std::list<MapBlock*>::iterator i = blocks.begin();
				i != blocks.end(); ++i)
		{
			MapBlock *block = (*i);
			v3s16 p = block->getPos();
			dst.push_back(p);
		}
	}
}

void ServerMap::saveMapMeta()
{
	DSTACK(__FUNCTION_NAME);

	/*infostream<<"ServerMap::saveMapMeta(): "
			<<"seed="<<m_seed
			<<std::endl;*/

	createDirs(m_savedir);

	std::string fullpath = m_savedir + DIR_DELIM + "map_meta.txt";
	std::ostringstream ss(std::ios_base::binary);

	Settings params;

	m_emerge->saveParamsToSettings(&params);
	params.writeLines(ss);

	ss<<"[end_of_params]\n";

	if(!fs::safeWriteToFile(fullpath, ss.str()))
	{
		infostream<<"ERROR: ServerMap::saveMapMeta(): "
				<<"could not write "<<fullpath<<std::endl;
		throw FileNotGoodException("Cannot save chunk metadata");
	}

	m_map_metadata_changed = false;
}

void ServerMap::loadMapMeta()
{
	DSTACK(__FUNCTION_NAME);

	/*infostream<<"ServerMap::loadMapMeta(): Loading map metadata"
			<<std::endl;*/

	std::string fullpath = m_savedir + DIR_DELIM + "map_meta.txt";
	std::ifstream is(fullpath.c_str(), std::ios_base::binary);
	if(is.good() == false)
	{
		infostream<<"ERROR: ServerMap::loadMapMeta(): "
				<<"could not open"<<fullpath<<std::endl;
		throw FileNotGoodException("Cannot open map metadata");
	}

	Settings params;

	for(;;)
	{
		if(is.eof())
			throw SerializationError
					("ServerMap::loadMapMeta(): [end_of_params] not found");
		std::string line;
		std::getline(is, line);
		std::string trimmedline = trim(line);
		if(trimmedline == "[end_of_params]")
			break;
		params.parseConfigLine(line);
	}
	
	m_emerge->loadParamsFromSettings(&params);

	verbosestream<<"ServerMap::loadMapMeta(): seed="
		<< m_emerge->params.seed<<std::endl;
}

void ServerMap::saveSectorMeta(ServerMapSector *sector)
{
	DSTACK(__FUNCTION_NAME);
	// Format used for writing
	u8 version = SER_FMT_VER_HIGHEST_WRITE;
	// Get destination
	v2s16 pos = sector->getPos();
	std::string dir = getSectorDir(pos);
	createDirs(dir);

	std::string fullpath = dir + DIR_DELIM + "meta";
	std::ostringstream ss(std::ios_base::binary);

	sector->serialize(ss, version);

	if(!fs::safeWriteToFile(fullpath, ss.str()))
		throw FileNotGoodException("Cannot write sector metafile");

	sector->differs_from_disk = false;
}

MapSector* ServerMap::loadSectorMeta(std::string sectordir, bool save_after_load)
{
	DSTACK(__FUNCTION_NAME);
	// Get destination
	v2s16 p2d = getSectorPos(sectordir);

	ServerMapSector *sector = NULL;

	std::string fullpath = sectordir + DIR_DELIM + "meta";
	std::ifstream is(fullpath.c_str(), std::ios_base::binary);
	if(is.good() == false)
	{
		// If the directory exists anyway, it probably is in some old
		// format. Just go ahead and create the sector.
		if(fs::PathExists(sectordir))
		{
			/*infostream<<"ServerMap::loadSectorMeta(): Sector metafile "
					<<fullpath<<" doesn't exist but directory does."
					<<" Continuing with a sector with no metadata."
					<<std::endl;*/
			sector = new ServerMapSector(this, p2d, m_gamedef);
			m_sectors[p2d] = sector;
		}
		else
		{
			throw FileNotGoodException("Cannot open sector metafile");
		}
	}
	else
	{
		sector = ServerMapSector::deSerialize
				(is, this, p2d, m_sectors, m_gamedef);
		if(save_after_load)
			saveSectorMeta(sector);
	}

	sector->differs_from_disk = false;

	return sector;
}

bool ServerMap::loadSectorMeta(v2s16 p2d)
{
	DSTACK(__FUNCTION_NAME);

	MapSector *sector = NULL;

	// The directory layout we're going to load from.
	//  1 - original sectors/xxxxzzzz/
	//  2 - new sectors2/xxx/zzz/
	//  If we load from anything but the latest structure, we will
	//  immediately save to the new one, and remove the old.
	int loadlayout = 1;
	std::string sectordir1 = getSectorDir(p2d, 1);
	std::string sectordir;
	if(fs::PathExists(sectordir1))
	{
		sectordir = sectordir1;
	}
	else
	{
		loadlayout = 2;
		sectordir = getSectorDir(p2d, 2);
	}

	try{
		sector = loadSectorMeta(sectordir, loadlayout != 2);
	}
	catch(InvalidFilenameException &e)
	{
		return false;
	}
	catch(FileNotGoodException &e)
	{
		return false;
	}
	catch(std::exception &e)
	{
		return false;
	}

	return true;
}

#if 0
bool ServerMap::loadSectorFull(v2s16 p2d)
{
	DSTACK(__FUNCTION_NAME);

	MapSector *sector = NULL;

	// The directory layout we're going to load from.
	//  1 - original sectors/xxxxzzzz/
	//  2 - new sectors2/xxx/zzz/
	//  If we load from anything but the latest structure, we will
	//  immediately save to the new one, and remove the old.
	int loadlayout = 1;
	std::string sectordir1 = getSectorDir(p2d, 1);
	std::string sectordir;
	if(fs::PathExists(sectordir1))
	{
		sectordir = sectordir1;
	}
	else
	{
		loadlayout = 2;
		sectordir = getSectorDir(p2d, 2);
	}

	try{
		sector = loadSectorMeta(sectordir, loadlayout != 2);
	}
	catch(InvalidFilenameException &e)
	{
		return false;
	}
	catch(FileNotGoodException &e)
	{
		return false;
	}
	catch(std::exception &e)
	{
		return false;
	}

	/*
		Load blocks
	*/
	std::vector<fs::DirListNode> list2 = fs::GetDirListing
			(sectordir);
	std::vector<fs::DirListNode>::iterator i2;
	for(i2=list2.begin(); i2!=list2.end(); i2++)
	{
		// We want files
		if(i2->dir)
			continue;
		try{
			loadBlock(sectordir, i2->name, sector, loadlayout != 2);
		}
		catch(InvalidFilenameException &e)
		{
			// This catches unknown crap in directory
		}
	}

	if(loadlayout != 2)
	{
		infostream<<"Sector converted to new layout - deleting "<<
			sectordir1<<std::endl;
		fs::RecursiveDelete(sectordir1);
	}

	return true;
}
#endif

void ServerMap::beginSave() {
	dbase->beginSave();
}

void ServerMap::endSave() {
	dbase->endSave();
}

void ServerMap::saveBlock(MapBlock *block)
{
  dbase->saveBlock(block);
}

void ServerMap::loadBlock(std::string sectordir, std::string blockfile, MapSector *sector, bool save_after_load)
{
	DSTACK(__FUNCTION_NAME);
	std::string fullpath = sectordir+DIR_DELIM+blockfile;
	try{

		std::ifstream is(fullpath.c_str(), std::ios_base::binary);
		if(is.good() == false)
			throw FileNotGoodException("Cannot open block file");

		v3s16 p3d = getBlockPos(sectordir, blockfile);
		v2s16 p2d(p3d.X, p3d.Z);

		assert(sector->getPos() == p2d);

		u8 version = SER_FMT_VER_INVALID;
		is.read((char*)&version, 1);

		if(is.fail())
			throw SerializationError("ServerMap::loadBlock(): Failed"
					" to read MapBlock version");

		/*u32 block_size = MapBlock::serializedLength(version);
		SharedBuffer<u8> data(block_size);
		is.read((char*)*data, block_size);*/

		// This will always return a sector because we're the server
		//MapSector *sector = emergeSector(p2d);

		MapBlock *block = NULL;
		bool created_new = false;
		block = sector->getBlockNoCreateNoEx(p3d.Y);
		if(block == NULL)
		{
			block = sector->createBlankBlockNoInsert(p3d.Y);
			created_new = true;
		}

		// Read basic data
		block->deSerialize(is, version, true);
		block->pushElementsToCircuit(m_circuit);

		// If it's a new block, insert it to the map
		if(created_new)
			sector->insertBlock(block);

		/*
			Save blocks loaded in old format in new format
		*/

		if(version < SER_FMT_VER_HIGHEST_WRITE || save_after_load)
		{
			saveBlock(block);

			// Should be in database now, so delete the old file
			fs::RecursiveDelete(fullpath);
		}

		// We just loaded it from the disk, so it's up-to-date.
		block->resetModified();

	}
	catch(SerializationError &e)
	{
		infostream<<"WARNING: Invalid block data on disk "
				<<"fullpath="<<fullpath
				<<" (SerializationError). "
				<<"what()="<<e.what()
				<<std::endl;
				// Ignoring. A new one will be generated.
		assert(0);

		// TODO: Backup file; name is in fullpath.
	}
}

void ServerMap::loadBlock(std::string *blob, v3s16 p3d, MapSector *sector, bool save_after_load)
{
	DSTACK(__FUNCTION_NAME);

	try {
		std::istringstream is(*blob, std::ios_base::binary);

		u8 version = SER_FMT_VER_INVALID;
		is.read((char*)&version, 1);

		if(is.fail())
			throw SerializationError("ServerMap::loadBlock(): Failed"
					" to read MapBlock version");

		/*u32 block_size = MapBlock::serializedLength(version);
		SharedBuffer<u8> data(block_size);
		is.read((char*)*data, block_size);*/

		// This will always return a sector because we're the server
		//MapSector *sector = emergeSector(p2d);

		MapBlock *block = NULL;
		bool created_new = false;
		block = sector->getBlockNoCreateNoEx(p3d.Y);
		if(block == NULL)
		{
			block = sector->createBlankBlockNoInsert(p3d.Y);
			created_new = true;
		}

		// Read basic data
		block->deSerialize(is, version, true);

		// If it's a new block, insert it to the map
		if(created_new)
			sector->insertBlock(block);

		/*
			Save blocks loaded in old format in new format
		*/

		//if(version < SER_FMT_VER_HIGHEST_READ || save_after_load)
		// Only save if asked to; no need to update version
		if(save_after_load)
			saveBlock(block);

		// We just loaded it from, so it's up-to-date.
		block->resetModified();

	}
	catch(SerializationError &e)
	{
		errorstream<<"Invalid block data in database"
				<<" ("<<p3d.X<<","<<p3d.Y<<","<<p3d.Z<<")"
				<<" (SerializationError): "<<e.what()<<std::endl;

		// TODO: Block should be marked as invalid in memory so that it is
		// not touched but the game can run

		if(g_settings->getBool("ignore_world_load_errors")){
			errorstream<<"Ignoring block load error. Duck and cover! "
					<<"(ignore_world_load_errors)"<<std::endl;
		} else {
			throw SerializationError("Invalid block data in database");
			//assert(0);
		}
	}
}

MapBlock* ServerMap::loadBlock(v3s16 blockpos)
{
	DSTACK(__FUNCTION_NAME);

	v2s16 p2d(blockpos.X, blockpos.Z);

	MapBlock *ret;

	ret = dbase->loadBlock(blockpos);
	if (ret) return (ret);
	return ret;

	// Not found in database, try the files

	// The directory layout we're going to load from.
	//  1 - original sectors/xxxxzzzz/
	//  2 - new sectors2/xxx/zzz/
	//  If we load from anything but the latest structure, we will
	//  immediately save to the new one, and remove the old.
	int loadlayout = 1;
	std::string sectordir1 = getSectorDir(p2d, 1);
	std::string sectordir;
	if(fs::PathExists(sectordir1))
	{
		sectordir = sectordir1;
	}
	else
	{
		loadlayout = 2;
		sectordir = getSectorDir(p2d, 2);
	}

	/*
		Make sure sector is loaded
	*/
	MapSector *sector = getSectorNoGenerateNoEx(p2d);
	if(sector == NULL)
	{
		try{
			sector = loadSectorMeta(sectordir, loadlayout != 2);
		}
		catch(InvalidFilenameException &e)
		{
			return NULL;
		}
		catch(FileNotGoodException &e)
		{
			return NULL;
		}
		catch(std::exception &e)
		{
			return NULL;
		}
	}

	/*
		Make sure file exists
	*/

	std::string blockfilename = getBlockFilename(blockpos);
	if(fs::PathExists(sectordir+DIR_DELIM+blockfilename) == false)
		return NULL;

	/*
		Load block and save it to the database
	*/
	loadBlock(sectordir, blockfilename, sector, true);
	return getBlockNoCreateNoEx(blockpos);
}

void ServerMap::PrintInfo(std::ostream &out)
{
	out<<"ServerMap: ";
}

s16 ServerMap::updateBlockHeat(ServerEnvironment *env, v3s16 p, MapBlock *block, std::map<v3s16, s16> * cache)
{
	auto bp = getNodeBlockPos(p);
	auto gametime = env->getGameTime();
	if (block) {
		if (gametime < block->heat_last_update)
			return block->heat + myrand_range(0, 1);
	} else if (!cache) {
		block = getBlockNoCreateNoEx(bp);
	}
	if (cache && cache->count(bp))
		return cache->at(bp) + myrand_range(0, 1);

	auto value = m_emerge->biomedef->calcBlockHeat(p, getSeed(),
			env->getTimeOfDayF(), gametime * env->getTimeOfDaySpeed(), env->m_use_weather);

	if(block) {
		block->heat = value;
		block->heat_last_update = env->m_use_weather ? gametime + 30 : -1;
	}
	if (cache)
		(*cache)[bp] = value;
	return value + myrand_range(0, 1);
}

s16 ServerMap::updateBlockHumidity(ServerEnvironment *env, v3s16 p, MapBlock *block, std::map<v3s16, s16> * cache)
{
	auto bp = getNodeBlockPos(p);
	auto gametime = env->getGameTime();
	if (block) {
		if (gametime < block->humidity_last_update)
			return block->humidity + myrand_range(0, 1);
	} else if (!cache) {
		block = getBlockNoCreateNoEx(bp);
	}
	if (cache && cache->count(bp))
		return cache->at(bp) + myrand_range(0, 1);

	auto value = m_emerge->biomedef->calcBlockHumidity(p, getSeed(),
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

	s16 max = MYMIN(searchup + basepos.Y,0x7FFF);

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
		}
		else {
			bool is_walkable = nodemgr->get(node).walkable;

			if (last_was_walkable && (!is_walkable)) {
				return runpos.Y;
			}
			last_was_walkable = is_walkable;
		}
	}

	return basepos.Y -1;
}

/*
	MapVoxelManipulator
*/

MapVoxelManipulator::MapVoxelManipulator(Map *map)
{
	m_map = map;
}

MapVoxelManipulator::~MapVoxelManipulator()
{
	/*infostream<<"MapVoxelManipulator: blocks: "<<m_loaded_blocks.size()
			<<std::endl;*/
}

void MapVoxelManipulator::emerge(VoxelArea a, s32 caller_id)
{
	TimeTaker timer1("emerge", &emerge_time);

	// Units of these are MapBlocks
	v3s16 p_min = getNodeBlockPos(a.MinEdge);
	v3s16 p_max = getNodeBlockPos(a.MaxEdge);

	VoxelArea block_area_nodes
			(p_min*MAP_BLOCKSIZE, (p_max+1)*MAP_BLOCKSIZE-v3s16(1,1,1));

	addArea(block_area_nodes);

	for(s32 z=p_min.Z; z<=p_max.Z; z++)
	for(s32 y=p_min.Y; y<=p_max.Y; y++)
	for(s32 x=p_min.X; x<=p_max.X; x++)
	{
		u8 flags = 0;
		MapBlock *block;
		v3s16 p(x,y,z);
		std::map<v3s16, u8>::iterator n;
		n = m_loaded_blocks.find(p);
		if(n != m_loaded_blocks.end())
			continue;

		bool block_data_inexistent = false;
		try
		{
			TimeTaker timer1("emerge load", &emerge_load_time);

			/*infostream<<"Loading block (caller_id="<<caller_id<<")"
					<<" ("<<p.X<<","<<p.Y<<","<<p.Z<<")"
					<<" wanted area: ";
			a.print(infostream);
			infostream<<std::endl;*/

			block = m_map->getBlockNoCreate(p);
			if(block->isDummy())
				block_data_inexistent = true;
			else
				block->copyTo(*this);
		}
		catch(InvalidPositionException &e)
		{
			block_data_inexistent = true;
		}

		if(block_data_inexistent)
		{
			flags |= VMANIP_BLOCK_DATA_INEXIST;

			VoxelArea a(p*MAP_BLOCKSIZE, (p+1)*MAP_BLOCKSIZE-v3s16(1,1,1));
			// Fill with VOXELFLAG_INEXISTENT
			for(s32 z=a.MinEdge.Z; z<=a.MaxEdge.Z; z++)
			for(s32 y=a.MinEdge.Y; y<=a.MaxEdge.Y; y++)
			{
				s32 i = m_area.index(a.MinEdge.X,y,z);
				memset(&m_flags[i], VOXELFLAG_INEXISTENT, MAP_BLOCKSIZE);
			}
		}
		/*else if (block->getNode(0, 0, 0).getContent() == CONTENT_IGNORE)
		{
			// Mark that block was loaded as blank
			flags |= VMANIP_BLOCK_CONTAINS_CIGNORE;
		}*/

		m_loaded_blocks[p] = flags;
	}

	//infostream<<"emerge done"<<std::endl;
}

/*
	SUGG: Add an option to only update eg. water and air nodes.
	      This will make it interfere less with important stuff if
		  run on background.
*/
void MapVoxelManipulator::blitBack
		(std::map<v3s16, MapBlock*> & modified_blocks)
{
	if(m_area.getExtent() == v3s16(0,0,0))
		return;

	//TimeTaker timer1("blitBack");

	/*infostream<<"blitBack(): m_loaded_blocks.size()="
			<<m_loaded_blocks.size()<<std::endl;*/

	/*
		Initialize block cache
	*/
	v3s16 blockpos_last;
	MapBlock *block = NULL;
	bool block_checked_in_modified = false;

	for(s32 z=m_area.MinEdge.Z; z<=m_area.MaxEdge.Z; z++)
	for(s32 y=m_area.MinEdge.Y; y<=m_area.MaxEdge.Y; y++)
	for(s32 x=m_area.MinEdge.X; x<=m_area.MaxEdge.X; x++)
	{
		v3s16 p(x,y,z);

		u8 f = m_flags[m_area.index(p)];
		if(f & (VOXELFLAG_NOT_LOADED|VOXELFLAG_INEXISTENT))
			continue;

		MapNode &n = m_data[m_area.index(p)];

		v3s16 blockpos = getNodeBlockPos(p);

		try
		{
			// Get block
			if(block == NULL || blockpos != blockpos_last){
				block = m_map->getBlockNoCreate(blockpos);
				blockpos_last = blockpos;
				block_checked_in_modified = false;
			}

			// Calculate relative position in block
			v3s16 relpos = p - blockpos * MAP_BLOCKSIZE;

			// Don't continue if nothing has changed here
			if(block->getNode(relpos) == n)
				continue;

			//m_map->setNode(m_area.MinEdge + p, n);
			block->setNode(relpos, n);

			/*
				Make sure block is in modified_blocks
			*/
			if(block_checked_in_modified == false)
			{
				modified_blocks[blockpos] = block;
				block_checked_in_modified = true;
			}
		}
		catch(InvalidPositionException &e)
		{
		}
	}
}

ManualMapVoxelManipulator::ManualMapVoxelManipulator(Map *map):
		MapVoxelManipulator(map),
		m_create_area(false)
{
}

ManualMapVoxelManipulator::~ManualMapVoxelManipulator()
{
}

void ManualMapVoxelManipulator::emerge(VoxelArea a, s32 caller_id)
{
	// Just create the area so that it can be pointed to
	VoxelManipulator::emerge(a, caller_id);
}

void ManualMapVoxelManipulator::initialEmerge(v3s16 blockpos_min,
						v3s16 blockpos_max, bool load_if_inexistent)
{
	TimeTaker timer1("initialEmerge", &emerge_time);

	// Units of these are MapBlocks
	v3s16 p_min = blockpos_min;
	v3s16 p_max = blockpos_max;

	VoxelArea block_area_nodes
			(p_min*MAP_BLOCKSIZE, (p_max+1)*MAP_BLOCKSIZE-v3s16(1,1,1));

	u32 size_MB = block_area_nodes.getVolume()*4/1000000;
	if(size_MB >= 1)
	{
		infostream<<"initialEmerge: area: ";
		block_area_nodes.print(infostream);
		infostream<<" ("<<size_MB<<"MB)";
		infostream<<std::endl;
	}

	addArea(block_area_nodes);

	for(s32 z=p_min.Z; z<=p_max.Z; z++)
	for(s32 y=p_min.Y; y<=p_max.Y; y++)
	for(s32 x=p_min.X; x<=p_max.X; x++)
	{
		u8 flags = 0;
		MapBlock *block;
		v3s16 p(x,y,z);
		std::map<v3s16, u8>::iterator n;
		n = m_loaded_blocks.find(p);
		if(n != m_loaded_blocks.end())
			continue;

		bool block_data_inexistent = false;
		try
		{
			TimeTaker timer1("emerge load", &emerge_load_time);

			block = m_map->getBlockNoCreate(p);
			if(block->isDummy())
				block_data_inexistent = true;
			else
				block->copyTo(*this);
		}
		catch(InvalidPositionException &e)
		{
			block_data_inexistent = true;
		}

		if(block_data_inexistent)
		{
			
			if (load_if_inexistent) {
				ServerMap *svrmap = (ServerMap *)m_map;
				block = svrmap->emergeBlock(p, false);
				if (block == NULL)
					block = svrmap->createBlock(p);
				else
					block->copyTo(*this);
			} else {
				flags |= VMANIP_BLOCK_DATA_INEXIST;
				
				/*
					Mark area inexistent
				*/
				VoxelArea a(p*MAP_BLOCKSIZE, (p+1)*MAP_BLOCKSIZE-v3s16(1,1,1));
				// Fill with VOXELFLAG_INEXISTENT
				for(s32 z=a.MinEdge.Z; z<=a.MaxEdge.Z; z++)
				for(s32 y=a.MinEdge.Y; y<=a.MaxEdge.Y; y++)
				{
					s32 i = m_area.index(a.MinEdge.X,y,z);
					memset(&m_flags[i], VOXELFLAG_INEXISTENT, MAP_BLOCKSIZE);
				}
			}
		}
		/*else if (block->getNode(0, 0, 0).getContent() == CONTENT_IGNORE)
		{
			// Mark that block was loaded as blank
			flags |= VMANIP_BLOCK_CONTAINS_CIGNORE;
		}*/

		m_loaded_blocks[p] = flags;
	}
}

void ManualMapVoxelManipulator::blitBackAll(
		std::map<v3s16, MapBlock*> * modified_blocks)
{
	if(m_area.getExtent() == v3s16(0,0,0))
		return;

	/*
		Copy data of all blocks
	*/
	for(std::map<v3s16, u8>::iterator
			i = m_loaded_blocks.begin();
			i != m_loaded_blocks.end(); ++i)
	{
		v3s16 p = i->first;
		MapBlock *block = m_map->getBlockNoCreateNoEx(p);
		bool existed = !(i->second & VMANIP_BLOCK_DATA_INEXIST);
		if(!block || existed == false)
		{
			continue;
		}

		block->copyFrom(*this);

		if(modified_blocks)
			(*modified_blocks)[p] = block;
	}
}

//END
