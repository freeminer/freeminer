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
#include "mapblock.h"
#ifndef SERVER
	#include "mapblock_mesh.h"
#endif
#include "main.h"
#include "filesys.h"
#include "voxel.h"
#include "porting.h"
#include "serialization.h"
#include "nodemetadata.h"
#include "settings.h"
#include "log_types.h"
#include "profiler.h"
#include "nodedef.h"
#include "gamedef.h"
#include "util/directiontables.h"
#include "util/mathconstants.h"
#include "rollback_interface.h"
#include "environment.h"
#include "emerge.h"
#include "mapgen_v6.h"
#include "mg_biome.h"
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
Map::Map(IGameDef *gamedef, Circuit* circuit):
	m_liquid_step_flow(1000),
	m_blocks_delete(&m_blocks_delete_1),
	m_gamedef(gamedef),
	m_transforming_liquid_loop_count_multiplier(1.0f),
	m_unprocessed_count(0),
	m_inc_trending_up_start_time(0),
	m_queue_size_timer_started(false)
    ,
	m_circuit(circuit),
	m_blocks_update_last(0),
	m_blocks_save_last(0)
{
	updateLighting_last[LIGHTBANK_DAY] = updateLighting_last[LIGHTBANK_NIGHT] = 0;
	time_life = 0;
#if !CMAKE_HAVE_THREAD_LOCAL
	m_block_cache = nullptr;
#endif
}

Map::~Map()
{
	auto lock = m_blocks.lock_unique_rec();
#ifndef SERVER
	if(g_settings->getBool("enable_vbo"))
	for(auto &i : m_blocks) {
		// We dont have gamedef here anymore, so we cant remove the hardwarebuffers
		if(i.second && i.second->mesh)
			i.second->mesh->clearHardwareBuffer = false;
	}
#endif
	for (auto & ir : m_blocks_delete_1)
		delete ir.first;
	for (auto & ir : m_blocks_delete_2)
		delete ir.first;
	for(auto & ir : m_blocks)
		delete ir.second;
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

MapBlock * Map::getBlockNoCreate(v3s16 p3d)
{
	MapBlock *block = getBlockNoCreateNoEx(p3d);
	if(block == NULL)
		throw InvalidPositionException("getBlockNoCreate block=NULL");
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
MapNode Map::getNodeNoEx(v3s16 p, bool *is_valid_position)
{
#ifndef NDEBUG
	ScopeProfiler sp(g_profiler, "Map: getNodeNoEx");
#endif

	v3s16 blockpos = getNodeBlockPos(p);
	MapBlock *block = getBlockNoCreateNoEx(blockpos);
	if (block == NULL) {
		if (is_valid_position != NULL)
			*is_valid_position = false;
		return MapNode(CONTENT_IGNORE);
	}

	v3s16 relpos = p - blockpos*MAP_BLOCKSIZE;
	bool is_valid_p;
	MapNode node = block->getNodeNoCheck(relpos, &is_valid_p);
	if (is_valid_position != NULL)
		*is_valid_position = is_valid_p;
	return node;
}

MapNode Map::getNodeTry(v3POS p)
{
#ifndef NDEBUG
	ScopeProfiler sp(g_profiler, "Map: getNodeTry");
#endif
	auto blockpos = getNodeBlockPos(p);
	auto block = getBlockNoCreateNoEx(blockpos, true);
	if(!block)
		return MapNode(CONTENT_IGNORE);
	auto relpos = p - blockpos*MAP_BLOCKSIZE;
	return block->getNodeTry(relpos);
}

/*
MapNode Map::getNodeLog(v3POS p){
	auto blockpos = getNodeBlockPos(p);
	auto block = getBlockNoCreateNoEx(blockpos);
	v3s16 relpos = p - blockpos*MAP_BLOCKSIZE;
	auto node = block->getNodeNoEx(relpos);
	infostream<<"getNodeLog("<<p<<") blockpos="<<blockpos<<" block="<<block<<" relpos="<<relpos<<" n="<<node<<std::endl;
	return node;
}
*/

/*
MapNode Map::getNodeNoLock(v3s16 p) //dont use
{
	v3s16 blockpos = getNodeBlockPos(p);
	MapBlock *block = getBlockNoCreateNoEx(blockpos);
	if(block == NULL)
		return MapNode(CONTENT_IGNORE);
	return block->getNodeNoLock(p - blockpos*MAP_BLOCKSIZE);
}
*/

#if 0
// Deprecated
// throws InvalidPositionException if not found
// TODO: Now this is deprecated, getNodeNoEx should be renamed
MapNode Map::getNode(v3s16 p)
{
	v3s16 blockpos = getNodeBlockPos(p);
	MapBlock *block = getBlockNoCreateNoEx(blockpos);
	if (block == NULL)
		throw InvalidPositionException("getNode block=NULL");
	v3s16 relpos = p - blockpos*MAP_BLOCKSIZE;
	bool is_valid_position;
	MapNode node = block->getNodeNoCheck(relpos, &is_valid_position);
	if (!is_valid_position)
		throw InvalidPositionException();
	return node;
}
#endif

// throws InvalidPositionException if not found
void Map::setNode(v3s16 p, MapNode & n)
{
	v3s16 blockpos = getNodeBlockPos(p);
	MapBlock *block = getBlockNoCreate(blockpos);
	v3s16 relpos = p - blockpos*MAP_BLOCKSIZE;
	// Never allow placing CONTENT_IGNORE, it fucks up stuff
	if(n.getContent() == CONTENT_IGNORE){
		bool temp_bool;
		errorstream<<"Map::setNode(): Not allowing to place CONTENT_IGNORE"
				<<" while trying to replace \""
				<<m_gamedef->ndef()->get(block->getNodeNoCheck(relpos, &temp_bool)).name
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

	if(from_nodes.empty())
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

		if(!block || block->isDummy())
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

			// Only fetch a new block if the block position has changed
			try {
				if(block == NULL || blockpos != blockpos_last){
					block = getBlockNoCreate(blockpos);

					if (!block || block->isDummy())
						continue;

					blockpos_last = blockpos;

					block_checked_in_modified = false;
					blockchangecount++;
				}
			}
			catch(InvalidPositionException &e) {
				continue;
			}

			// Calculate relative position in block
			v3s16 relpos = n2pos - blockpos * MAP_BLOCKSIZE;
			// Get node straight from the block
			bool is_valid_position;
			MapNode n2 = block->getNode(relpos, &is_valid_position);
			if (!is_valid_position)
				continue;

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
	}

	/*infostream<<"unspreadLight(): Changed block "
			<<blockchangecount<<" times"
			<<" for "<<from_nodes.size()<<" nodes"
			<<std::endl;*/

	if(!unlighted_nodes.empty())
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
		std::map<v3s16, MapBlock*> & modified_blocks, int recursive)
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

	if(from_nodes.empty())
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
		try {
			if(block == NULL || blockpos != blockpos_last){
				block = getBlockNoCreate(blockpos);
				blockpos_last = blockpos;

				block_checked_in_modified = false;
				blockchangecount++;
			}
		}
		catch(InvalidPositionException &e) {
			continue;
		}

		if(block->isDummy())
			continue;

		//auto lock = block->try_lock_unique_rec();
		//if (!lock->owns_lock())
		//	continue;

		// Calculate relative position in block
		v3s16 relpos = pos - blockpos_last * MAP_BLOCKSIZE;

		// Get node straight from the block
		bool is_valid_position;
		MapNode n = block->getNode(relpos, &is_valid_position);
		if (n.getContent() == CONTENT_IGNORE)
			continue;

		u8 oldlight = is_valid_position ? n.getLight(bank, nodemgr) : 0;
		u8 newlight = diminish_light(oldlight);

		// Loop through 6 neighbors
		for(u16 i=0; i<6; i++){
			// Get the position of the neighbor node
			v3s16 n2pos = pos + dirs[i];

			// Get the block where the node is located
			v3s16 blockpos = getNodeBlockPos(n2pos);

			// Only fetch a new block if the block position has changed
			try {
				if(block == NULL || blockpos != blockpos_last){
					block = getBlockNoCreate(blockpos);
					blockpos_last = blockpos;

					block_checked_in_modified = false;
					blockchangecount++;
				}
			}
			catch(InvalidPositionException &e) {
				continue;
			}
			// Calculate relative position in block
			v3s16 relpos = n2pos - blockpos * MAP_BLOCKSIZE;
			// Get node straight from the block
			MapNode n2 = block->getNode(relpos, &is_valid_position);
			if (!is_valid_position)
				continue;

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
	}

	/*infostream<<"spreadLight(): Changed block "
			<<blockchangecount<<" times"
			<<" for "<<from_nodes.size()<<" nodes"
			<<std::endl;*/

	if(!lighted_nodes.empty() && recursive <= 32) { // maybe 32 too small
/*
		infostream<<"spreadLight(): recursive("<<recursive<<"): changed=" <<blockchangecount
			<<" from="<<from_nodes.size()
			<<" lighted="<<lighted_nodes.size()
			<<" modifiedB="<<modified_blocks.size()
			<<std::endl;
*/
		spreadLight(bank, lighted_nodes, modified_blocks, ++recursive);
	}
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
		bool is_valid_position;
		n2 = getNodeNoEx(n2pos, &is_valid_position);
		if (!is_valid_position)
			continue;

		if(n2.getLight(bank, nodemgr) > brightest_light || found_something == false){
			brightest_light = n2.getLight(bank, nodemgr);
			brightest_pos = n2pos;
			found_something = true;
		}
	}

	if(found_something == false)
		throw InvalidPositionException("getBrightestNeighbour nothing found");

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
		bool is_valid_position;
		MapNode n = block->getNode(relpos, &is_valid_position);
		if (!is_valid_position)
			break;

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
		shared_map<v3POS, MapBlock*> & a_blocks,
		std::map<v3POS, MapBlock*> & modified_blocks, int max_cycle_ms)
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

	//JMutexAutoLock lock2(m_update_lighting_mutex);

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

		for(;;)
		{
			// Don't bother with dummy blocks.
			if(!block || block->isDummy())
				break;

			auto lock = block->try_lock_unique_rec();
			if (!lock->owns_lock())
				break; // may cause dark areas
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
				v3s16 p(x,y,z);
				bool is_valid_position;
				MapNode n = block->getNode(p, &is_valid_position);
				if (!is_valid_position) {
					/* This would happen when dealing with a
					   dummy block.
					*/
					infostream<<"updateLighting(): InvalidPositionException"
							<<std::endl;
					continue;
				}
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

			/*infostream<<"Bottom for sunlight-propagated block ("
					<<pos.X<<","<<pos.Y<<","<<pos.Z<<") not valid"
					<<std::endl;*/

			block->setLightingExpired(true);

			// Bottom sunlight is not valid; get the block and loop to it

			pos.Y--;
			block = getBlockNoCreateNoEx(pos);
		}
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

	for (auto & ir : blocks_to_update) {
		auto block = getBlockNoCreateNoEx(ir.first);
		block->setLightingExpired(false);
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

u32 Map::updateLighting(shared_map<v3POS, MapBlock*> & a_blocks,
		std::map<v3POS, MapBlock*> & modified_blocks, int max_cycle_ms)
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

	a_blocks.clear();
TimeTaker timer("updateLighting expireDayNightDiff");
	//JMutexAutoLock lock2(m_update_lighting_mutex);

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

	bool is_valid_position;
	MapNode topnode = getNodeNoEx(toppos, &is_valid_position);

	if(is_valid_position && topnode.getLight(LIGHTBANK_DAY, ndef) != LIGHT_SUN)
		node_under_sunlight = false;

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

		u8 lightwas = getNodeNoEx(p).getLight(bank, ndef);

		// Add the block of the added node to modified_blocks
		v3s16 blockpos = getNodeBlockPos(p);
		MapBlock * block = getBlockNoCreate(blockpos);
		if(!block)
			break;
		modified_blocks[blockpos] = block;

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

			n2 = getNodeNoEx(n2pos, &is_valid_position);
			if (!is_valid_position)
				break;

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
		v3s16 p2 = p + dirs[i];

		MapNode n2 = getNodeNoEx(p2, &is_valid_position);
		if(is_valid_position
				&& (ndef->get(n2).isLiquid() || n2.getContent() == CONTENT_AIR))
		{
			transforming_liquid_push_back(p2);
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
	bool is_valid_position;
	MapNode topnode = getNodeNoEx(toppos, &is_valid_position);

	if(is_valid_position && topnode.getLight(LIGHTBANK_DAY, ndef) != LIGHT_SUN)
		node_under_sunlight = false;

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
				getNodeNoEx(p).getLight(bank, ndef),
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
	if(!block)
		return;
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
		MapNode n = getNodeNoEx(p, &is_valid_position);
		if (is_valid_position) {
			n.setLight(LIGHTBANK_DAY, 0, ndef);
			setNode(p, n);
		} else {
			//assert(0);
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
		v3s16 p2 = p + dirs[i];

		bool is_position_valid;
		MapNode n2 = getNodeNoEx(p2, &is_position_valid);
		if (is_position_valid
				&& (ndef->get(n2).isLiquid() || n2.getContent() == CONTENT_AIR))
		{
			transforming_liquid_push_back(p2);
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

	if (/*!m_blocks_update_last && */ m_blocks_delete->size() > 1000) {
		m_blocks_delete = (m_blocks_delete == &m_blocks_delete_1 ? &m_blocks_delete_2 : &m_blocks_delete_1);
		verbosestream<<"Deleting blocks="<<m_blocks_delete->size()<<std::endl;
		for (auto & ir : *m_blocks_delete)
			delete ir.first;
		m_blocks_delete->clear();
	}

	u32 deleted_blocks_count = 0;
	u32 saved_blocks_count = 0;
	u32 block_count_all = 0;

	u32 n = 0, calls = 0, end_ms = porting::getTimeMs() + max_cycle_ms;

	std::vector<MapBlockP> blocks_delete;
	int save_started = 0;
	{
	auto lock = m_blocks.try_lock_shared_rec();
	if (!lock->owns_lock())
		return m_blocks_update_last;
	for(auto ir : m_blocks) {
		if (n++ < m_blocks_update_last) {
			continue;
		}
		else {
			m_blocks_update_last = 0;
		}
		++calls;

		auto block = ir.second;
		if (!block)
			continue;

		{
			auto lock = block->try_lock_unique_rec();
			if (!lock->owns_lock())
				continue;
			if(block->refGet() == 0 && block->getUsageTimer() > unload_timeout)
			{
				v3s16 p = block->getPos();
				//infostream<<" deleting block p="<<p<<" ustimer="<<block->getUsageTimer() <<" to="<< unload_timeout<<" inc="<<(uptime - block->m_uptime_timer_last)<<" state="<<block->getModified()<<std::endl;
				// Save if modified
				if (block->getModified() != MOD_STATE_CLEAN && save_before_unloading)
				{
					//modprofiler.add(block->getModifiedReason(), 1);
					if(!save_started++)
						beginSave();
					if (!saveBlock(block))
						continue;
					saved_blocks_count++;
				}

				blocks_delete.push_back(block);

				if(unloaded_blocks)
					unloaded_blocks->push_back(p);

				deleted_blocks_count++;
			}
			else
			{

#ifndef SERVER
			if (block->mesh_old)
				block->mesh_old = nullptr;
#endif

			if (!block->m_uptime_timer_last)  // not very good place, but minimum modifications
				block->m_uptime_timer_last = uptime - 0.1;
			block->incrementUsageTimer(uptime - block->m_uptime_timer_last);
			block->m_uptime_timer_last = uptime;

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

		} // block lock

		if (porting::getTimeMs() > end_ms) {
			m_blocks_update_last = n;
			break;
		}

	}
	}
	if(save_started)
		endSave();

	if (!calls)
		m_blocks_update_last = 0;

	for (auto & block : blocks_delete)
		this->deleteBlock(block);

	if(m_circuit != NULL) {
		m_circuit->save();
	}

	// Finally delete the empty sectors

	if(deleted_blocks_count != 0)
	{
		if (m_blocks_update_last)
			infostream<<"ServerMap: timerUpdate(): Blocks processed:"<<calls<<"/"<<m_blocks.size()<<" to "<<m_blocks_update_last<<std::endl;
		PrintInfo(infostream); // ServerMap/ClientMap:
		infostream<<"Unloaded "<<deleted_blocks_count<<"/"<<(block_count_all + deleted_blocks_count)
				<<" blocks from memory";
		infostream<<" (deleteq1="<<m_blocks_delete_1.size()<< " deleteq2="<<m_blocks_delete_2.size()<<")";
		if(saved_blocks_count)
			infostream<<", of which "<<saved_blocks_count<<" were written";
/*
		infostream<<", "<<block_count_all<<" blocks in memory";
*/
		infostream<<"."<<std::endl;
		if(saved_blocks_count != 0){
			PrintInfo(infostream); // ServerMap/ClientMap:
			//infostream<<"Blocks modified by: "<<std::endl;
			modprofiler.print(infostream);
		}
	}
	return m_blocks_update_last;
}

void Map::unloadUnreferencedBlocks(std::list<v3s16> *unloaded_blocks)
{
	timerUpdate(0.0, -1.0, 100, unloaded_blocks);
}

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
	content_t c;
	bool l; //can liquid
	bool i; //infinity
	int weight;
	int drop; //drop by liquid
};

void Map::transforming_liquid_push_back(v3POS p) {
	std::lock_guard<std::mutex> lock(m_transforming_liquid_mutex);
	//m_transforming_liquid.set(p, 1);
	m_transforming_liquid.push_back(p);
}

v3POS Map::transforming_liquid_pop() {
	std::lock_guard<std::mutex> lock(m_transforming_liquid_mutex);
	return m_transforming_liquid.pop_front();

	//auto lock = m_transforming_liquid.lock_unique_rec();
	//auto it = m_transforming_liquid.begin();
	//auto value = it->first;
	//m_transforming_liquid.erase(it);
	//return value;
}

u32 Map::transforming_liquid_size() {
	std::lock_guard<std::mutex> lock(m_transforming_liquid_mutex);
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

const v3POS liquid_flow_dirs[7] =
{
	// +right, +top, +back
	v3POS( 0,-1, 0), // bottom
	v3POS( 0, 0, 0), // self
	v3POS( 0, 0, 1), // back
	v3POS( 0, 0,-1), // front
	v3POS( 1, 0, 0), // right
	v3POS(-1, 0, 0), // left
	v3POS( 0, 1, 0)  // top
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

u32 Map::transformLiquidsReal(Server *m_server, int max_cycle_ms)
{

	INodeDefManager *nodemgr = m_gamedef->ndef();

	DSTACK(__FUNCTION_NAME);
	//TimeTaker timer("transformLiquidsReal()");
	u32 loopcount = 0;
	u32 initial_size = transforming_liquid_size();

	u8 relax = g_settings->getS16("liquid_relax");
	bool fast_flood = g_settings->getS16("liquid_fast_flood");
	int water_level = g_settings->getS16("water_level");

	// list of nodes that due to viscosity have not reached their max level height
	//std::unordered_map<v3POS, bool, v3POSHash, v3POSEqual> must_reflow, must_reflow_second, must_reflow_third;
	std::list<v3POS> must_reflow, must_reflow_second, must_reflow_third;
	// List of MapBlocks that will require a lighting update (due to lava)
	u16 loop_rand = myrand();

	u32 end_ms = porting::getTimeMs() + max_cycle_ms;

	NEXT_LIQUID:;
	while (transforming_liquid_size() > 0)
	{
		// This should be done here so that it is done when continue is used
		if (loopcount >= initial_size*2 || porting::getTimeMs() > end_ms)
			break;
		++loopcount;
		/*
			Get a queued transforming liquid node
		*/
		v3POS p0;
		{
			//JMutexAutoLock lock(m_transforming_liquid_mutex);
			p0 = transforming_liquid_pop();
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
			nb.p = p0 + liquid_flow_dirs[i];
			nb.n = getNodeNoEx(neighbors[i].p);
			nb.c = nb.n.getContent();
			NeighborType nt = NEIGHBOR_SAME_LEVEL;
			switch (i) {
				case D_TOP:
					nt = NEIGHBOR_UPPER;
					break;
				case D_BOTTOM:
					nt = NEIGHBOR_LOWER;
					break;
			}
			nb.t = nt;
			nb.l = 0;
			nb.i = 0;
			nb.weight = 0;
			nb.drop = 0;

			if (nb.c == CONTENT_IGNORE) {
				//if (i == D_SELF && (loopcount % 2) && initial_size < m_liquid_step_flow * 3)
				//	must_reflow_third[nb.p] = 1;
				//	must_reflow_third.push_back(nb.p);
				continue;
			}

			switch (nodemgr->get(nb.c).liquid_type) {
				case LIQUID_NONE:
					if (nb.c == CONTENT_AIR) {
						liquid_levels[i] = 0;
						nb.l = 1;
					}
					//TODO: if (nb.c == CONTENT_AIR || nodemgr->get(nb.n).buildable_to && !nodemgr->get(nb.n).walkable) { // need lua drop api for drop torches
					else if (	melt_kind_flowing != CONTENT_IGNORE &&
							nb.c == melt_kind_flowing &&
							nb.t != NEIGHBOR_UPPER &&
							!(loopcount % 2)) {
						u8 melt_max_level = nb.n.getMaxLevel(nodemgr);
						u8 my_max_level = MapNode(liquid_kind_flowing).getMaxLevel(nodemgr);
						liquid_levels[i] = (float)my_max_level / melt_max_level * nb.n.getLevel(nodemgr);
						if (liquid_levels[i])
							nb.l = 1;
					} else if (	melt_kind != CONTENT_IGNORE &&
							nb.c == melt_kind &&
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
						liquid_kind = nb.c;
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
					if (nb.c == liquid_kind) {
						liquid_levels[i] = nb.n.getLevel(nodemgr); //LIQUID_LEVEL_SOURCE;
						nb.l = 1;
						nb.i = (nb.n.param2 & LIQUID_INFINITY_MASK);
					}
					break;
				case LIQUID_FLOWING:
					// if this node is not (yet) of a liquid type,
					// choose the first liquid type we encounter
					if (liquid_kind_flowing == CONTENT_IGNORE)
						liquid_kind_flowing = nb.c;
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
					if (nb.c == liquid_kind_flowing) {
						liquid_levels[i] = nb.n.getLevel(nodemgr);
						nb.l = 1;
					}
					break;
			}

			// only self, top, bottom swap
			if (nodemgr->get(nb.c).liquid_type && e <= 2) {
				try{
					nb.weight = ((ItemGroupList) nodemgr->get(nb.n).groups)["weight"];
					if (e == 1 && neighbors[D_BOTTOM].weight && neighbors[D_SELF].weight > neighbors[D_BOTTOM].weight) {
						setNode(neighbors[D_SELF].p, neighbors[D_BOTTOM].n);
						setNode(neighbors[D_BOTTOM].p, neighbors[D_SELF].n);
						//must_reflow_second[neighbors[D_SELF].p] = 1;
						//must_reflow_second[neighbors[D_BOTTOM].p] = 1;
						must_reflow_second.push_back(neighbors[D_SELF].p);
						must_reflow_second.push_back(neighbors[D_BOTTOM].p);
						goto NEXT_LIQUID;
					}
					if (e == 2 && neighbors[D_SELF].weight && neighbors[D_TOP].weight > neighbors[D_SELF].weight) {
						setNode(neighbors[D_SELF].p, neighbors[D_TOP].n);
						setNode(neighbors[D_TOP].p, neighbors[D_SELF].n);
						//must_reflow_second[neighbors[D_SELF].p] = 1;
						//must_reflow_second[neighbors[D_TOP].p] = 1;
						must_reflow_second.push_back(neighbors[D_SELF].p);
						must_reflow_second.push_back(neighbors[D_TOP].p);
						goto NEXT_LIQUID;
					}
				}
				catch(InvalidPositionException &e) {
					verbosestream<<"transformLiquidsReal: weight: setNode() failed:"<< nb.p<<":"<<e.what()<<std::endl;
					//goto NEXT_LIQUID;
				}
			}
			
			if (nb.l && nb.t == NEIGHBOR_SAME_LEVEL)
				++can_liquid_same_level;
			if (liquid_levels[i] > 0)
				total_level += liquid_levels[i];

			/*
			infostream << "get node i=" <<(int)i<<" " << PP(nb.p) << " c="
			<< nb.c <<" p0="<< (int)nb.n.param0 <<" p1="
			<< (int)nb.n.param1 <<" p2="<< (int)nb.n.param2 << " lt="
			<< nodemgr->get(nb.c).liquid_type
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
			neighbors[D_BOTTOM].c == CONTENT_IGNORE &&
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
			p0.Y == water_level &&
			liquid_levels[D_TOP] == 0 &&
			!(loopcount % 2) &&
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
						//must_reflow_second[neighbors[i].p + liquid_flow_dirs[ii]] = 1;
				}
			}

			//flowed += liquid_levels_want[i];
			if (liquid_levels[i] == liquid_levels_want[i]) {
				continue;
			}

			if (neighbors[i].drop) {// && level_max > 1 && total_level >= level_max - 1
				//JMutexAutoLock envlock(m_server->m_env_mutex); // 8(
				m_server->getEnv().getScriptIface()->node_drop(neighbors[i].p, 2);
			}

			neighbors[i].n.setContent(liquid_kind_flowing);
			neighbors[i].n.setLevel(nodemgr, liquid_levels_want[i], 1);

			try{
				setNode(neighbors[i].p, neighbors[i].n);
			} catch(InvalidPositionException &e) {
				verbosestream<<"transformLiquidsReal: setNode() failed:"<<neighbors[i].p<<":"<<e.what()<<std::endl;
			}

			// If node emits light, MapBlock requires lighting update
			// or if node removed
			v3POS blockpos = getNodeBlockPos(neighbors[i].p);
			MapBlock *block = getBlockNoCreateNoEx(blockpos, true); // remove true if light bugs
			if(block != NULL) {
				//modified_blocks[blockpos] = block;
				if(!nodemgr->get(neighbors[i].n).light_propagates || nodemgr->get(neighbors[i].n).light_source) // better to update always
					lighting_modified_blocks.set_try(block->getPos(), block);
			}
			//must_reflow[neighbors[i].p] = 1;
			must_reflow.push_back(neighbors[i].p);

		}

		//if (total_was!=flowed) infostream<<" flowed "<<flowed<<"/"<<total_was<<std::endl;
		/* //for better relax  only same level
		if (changed)  for (u16 ii = D_SELF + 1; ii < D_TOP; ++ii) {
			if (!neighbors[ii].l) continue;
			must_reflow.push_back(p0 + dirs[ii]);
		}*/
		//g_profiler->graphAdd("liquids", 1);
	}

	u32 ret = loopcount >= initial_size ? 0 : transforming_liquid_size();
	if (ret || loopcount > m_liquid_step_flow)
		m_liquid_step_flow += (m_liquid_step_flow > loopcount ? -1 : 1) * (int)loopcount/10;
	/*
	if (loopcount)
		infostream<<"Map::transformLiquidsReal(): loopcount="<<loopcount<<" initial_size="<<initial_size
		<<" avgflow="<<m_liquid_step_flow
		<<" reflow="<<must_reflow.size()
		<<" reflow_second="<<must_reflow_second.size()
		<<" reflow_third="<<must_reflow_third.size()
		<<" queue="<< transforming_liquid_size()
		<<" per="<< porting::getTimeMs() - (end_ms - max_cycle_ms)
		<<" ret="<<ret<<std::endl;
	*/

	//JMutexAutoLock lock(m_transforming_liquid_mutex);

	{
		//TimeTaker timer13("transformLiquidsReal() reflow");
		//auto lock = m_transforming_liquid.lock_unique_rec();
		std::lock_guard<std::mutex> lock(m_transforming_liquid_mutex);

		//m_transforming_liquid.insert(must_reflow.begin(), must_reflow.end());
		for (const auto & p : must_reflow)
			m_transforming_liquid.push_back(p);
		must_reflow.clear();
		//m_transforming_liquid.insert(must_reflow_second.begin(), must_reflow_second.end());
		for (const auto & p : must_reflow_second)
			m_transforming_liquid.push_back(p);
		must_reflow_second.clear();
		//m_transforming_liquid.insert(must_reflow_third.begin(), must_reflow_third.end());
		for (const auto & p : must_reflow_third)
			m_transforming_liquid.push_back(p);
		must_reflow_third.clear();
	}

	g_profiler->add("Server: liquids real processed", loopcount);

	return loopcount;
}

#define WATER_DROP_BOOST 4

u32 Map::transformLiquids(Server *m_server, int max_cycle_ms)
{

	if (g_settings->getBool("liquid_real"))
		return Map::transformLiquidsReal(m_server, max_cycle_ms);

	INodeDefManager *nodemgr = m_gamedef->ndef();

	DSTACK(__FUNCTION_NAME);
	//TimeTaker timer("transformLiquids()");

	//JMutexAutoLock lock(m_transforming_liquid_mutex);

	u32 loopcount = 0;
	u32 initial_size = transforming_liquid_size();

	u32 curr_time = getTime(PRECISION_MILLI);

	/*if(initial_size != 0)
		infostream<<"transformLiquids(): initial_size="<<initial_size<<std::endl;*/

	// list of nodes that due to viscosity have not reached their max level height
	UniqueQueue<v3s16> must_reflow;

	// List of MapBlocks that will require a lighting update (due to lava)
	//std::map<v3s16, MapBlock*> lighting_modified_blocks;

	u32 end_ms = porting::getTimeMs() + max_cycle_ms;

	u32 liquid_loop_max = g_settings->getS32("liquid_loop_max");
	u32 loop_max = liquid_loop_max;

//	std::cout << "transformLiquids(): loopmax initial="
//	           << loop_max * m_transforming_liquid_loop_count_multiplier;

	// If liquid_loop_max is not keeping up with the queue size increase
	// loop_max up to a maximum of liquid_loop_max * dedicated_server_step.
	if (m_transforming_liquid.size() > loop_max * 2) {
		// "Burst" mode
		float server_step = g_settings->getFloat("dedicated_server_step");
		if (m_transforming_liquid_loop_count_multiplier - 1.0 < server_step)
			m_transforming_liquid_loop_count_multiplier *= 1.0 + server_step / 10;
	} else {
		m_transforming_liquid_loop_count_multiplier = 1.0;
	}

	loop_max *= m_transforming_liquid_loop_count_multiplier;

//	std::cout << " queue sz=" << m_transforming_liquid.size()
//	           << " loop_max=" << loop_max;

	while(transforming_liquid_size() != 0)
	{
		// This should be done here so that it is done when continue is used
		if(loopcount >= initial_size || porting::getTimeMs() > end_ms)
			break;
		loopcount++;

		/*
			Get a queued transforming liquid node
		*/
		v3s16 p0 = transforming_liquid_pop();

		MapNode n0 = getNodeTry(p0);

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
			NodeNeighbor nb = {getNodeTry(npos), nt, npos};
			switch (nodemgr->get(nb.n.getContent()).liquid_type) {
				case LIQUID_NONE:
					if (nb.n.getContent() == CONTENT_AIR) {
						airs[num_airs++] = nb;
						// if the current node is a water source the neighbor
						// should be enqueded for transformation regardless of whether the
						// current node changes or not.
						if (nb.t != NEIGHBOR_UPPER && liquid_type != LIQUID_NONE)
							transforming_liquid_push_back(npos);
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
			//modified_blocks[blockpos] =  block;
			// If new or old node emits light, MapBlock requires lighting update
			if(nodemgr->get(n0).light_source != 0 ||
					nodemgr->get(n00).light_source != 0)
				lighting_modified_blocks.set(block->getPos(), block);
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
						transforming_liquid_push_back(flows[i].p);
				for (u16 i = 0; i < num_airs; i++)
					if (airs[i].t != NEIGHBOR_UPPER)
						transforming_liquid_push_back(airs[i].p);
				break;
			case LIQUID_NONE:
				// this flow has turned to air; neighboring flows might need to do the same
				for (u16 i = 0; i < num_flows; i++)
					transforming_liquid_push_back(flows[i].p);
				break;
		}
	}

	u32 ret = loopcount >= initial_size ? 0 : transforming_liquid_size();

	//infostream<<"Map::transformLiquids(): loopcount="<<loopcount<<" per="<<timer.getTimerTime()<<" ret="<<ret<<std::endl;

	while (must_reflow.size() > 0)
		m_transforming_liquid.push_back(must_reflow.pop_front());
	//updateLighting(lighting_modified_blocks, modified_blocks);

	/*
	 * Queue size limiting
	 */
	u32 prev_unprocessed = m_unprocessed_count;
	m_unprocessed_count = m_transforming_liquid.size();

	// if unprocessed block count is decreasing or stable
	if (m_unprocessed_count <= prev_unprocessed) {
		m_queue_size_timer_started = false;
	} else {
		if (!m_queue_size_timer_started)
			m_inc_trending_up_start_time = curr_time;
		m_queue_size_timer_started = true;
	}

	u16 time_until_purge = g_settings->getU16("liquid_queue_purge_time");
	time_until_purge *= 1000;	// seconds -> milliseconds

//	std::cout << " growing for: "
//	           << (m_queue_size_timer_started ? curr_time - m_inc_trend_up_start_time : 0)
//	           << "ms" << std::endl;

	// Account for curr_time overflowing
	if (m_queue_size_timer_started && m_inc_trending_up_start_time > curr_time)
		m_queue_size_timer_started = false;

	/* If the queue has been growing for more than liquid_queue_purge_time seconds
	 * and the number of unprocessed blocks is still > liquid_loop_max then we
	 * cannot keep up; dump the oldest blocks from the queue so that the queue
	 * has liquid_loop_max items in it
	 */
	if (m_queue_size_timer_started
			&& curr_time - m_inc_trending_up_start_time > time_until_purge
			&& m_unprocessed_count > liquid_loop_max) {

		size_t dump_qty = m_unprocessed_count - liquid_loop_max;

		infostream << "transformLiquids(): DUMPING " << dump_qty
		           << " blocks from the queue" << std::endl;

		while (dump_qty--)
			m_transforming_liquid.pop_front();

		m_queue_size_timer_started = false; // optimistically assume we can keep up now
		m_unprocessed_count = m_transforming_liquid.size();
	}

	g_profiler->add("Server: liquids processed", loopcount);

	return ret;
}

NodeMetadata *Map::getNodeMetadata(v3s16 p)
{
	v3s16 blockpos = getNodeBlockPos(p);
	v3s16 p_rel = p - blockpos*MAP_BLOCKSIZE;
	MapBlock *block = getBlockNoCreateNoEx(blockpos, false, true);
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
	MapBlock *block = getBlockNoCreateNoEx(blockpos, false, true);
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
	MapBlock *block = getBlockNoCreateNoEx(blockpos, false, true);
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
		return value + (no_random ? 0 : myrand_range(0, 1));
	}
	//errorstream << "No humidity for " << p.X<<"," << p.Z << std::endl;
	return 0;
}

/*
	ServerMap
*/
ServerMap::ServerMap(std::string savedir, IGameDef *gamedef, EmergeManager *emerge, Circuit* circuit):
	Map(gamedef, circuit),
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
	m_map_loading_enabled = true;

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

	// Initially write whole map
	save(MOD_STATE_CLEAN);
}

ServerMap::~ServerMap()
{
	verbosestream<<__FUNCTION_NAME<<std::endl;

	try
	{
			// Save only changed parts
			save(MOD_STATE_WRITE_AT_UNLOAD);
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
		data->vmanip->initialEmerge(bigarea_blocks_min, bigarea_blocks_max);
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

void ServerMap::finishBlockMake(BlockMakeData *data,
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
		data->vmanip->blitBackAll(&changed_blocks, false);
	}

	EMERGE_DBG_OUT("finishBlockMake: changed_blocks.size()=" << changed_blocks.size());

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
			MapBlock * block = getBlockNoCreateNoEx(p);
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
		if (!block)
			continue;
		/*
			Update day/night difference cache of the MapBlocks
		*/
		block->expireDayNightDiff();
		/*
			Set block as modified
		*/

		if (g_settings->getBool("save_generated_block"))
		block->raiseModified(MOD_STATE_WRITE_NEEDED,
				"finishBlockMake expireDayNightDiff");

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
		if (!block)
			continue;
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
		v3POS p(x, y, z);
		MapBlock *block = getBlockNoCreateNoEx(p);
		if (!block)
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

	MapBlock * block = getBlockNoCreateNoEx(blockpos_requested, false, true);
	if(!block) {
		errorstream<<"finishBlockMake(): created NULL block at "<<PP(blockpos_requested)<<std::endl;
	}

}

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

	MapBlock *block = this->getBlockNoCreateNoEx(p, false, true);
	if(block)
	{
		if(block->isDummy())
			block->unDummify();
		return block;
	}
	// Create blank
	block = this->createBlankBlock(p);

	return block;
}

MapBlock * ServerMap::emergeBlock(v3s16 p, bool create_blank)
{
	DSTACKF("%s: p=(%d,%d,%d), create_blank=%d",
			__FUNCTION_NAME,
			p.X, p.Y, p.Z, create_blank);
	{
		MapBlock *block = getBlockNoCreateNoEx(p, false, true);
		if(block && block->isDummy() == false)
		{
			return block;
		}
	}

	if (!m_map_loading_enabled)
		return nullptr;

	{
		MapBlock *block = loadBlock(p);
		if(block)
			return block;
	}

	if (create_blank) {
		return this->createBlankBlock(p);
	}

	return NULL;
}

MapBlock *ServerMap::getBlockOrEmerge(v3s16 p3d)
{
	MapBlock *block = getBlockNoCreateNoEx(p3d);
	if (block == NULL && m_map_loading_enabled)
		m_emerge->enqueueBlockEmerge(PEER_ID_INEXISTENT, p3d, false);

	return block;
}

void ServerMap::prepareBlock(MapBlock *block) {
	ServerEnvironment *senv = &((Server *)m_gamedef)->getEnv();

	// Calculate weather conditions
	//block->heat_last_update     = 0;
	//block->humidity_last_update = 0;
	v3POS p = block->getPos() *  MAP_BLOCKSIZE;
	updateBlockHeat(senv, p, block);
	updateBlockHumidity(senv, p, block);
}

// N.B.  This requires no synchronization, since data will not be modified unless
// the VoxelManipulator being updated belongs to the same thread.
void ServerMap::updateVManip(v3s16 pos)
{
	Mapgen *mg = m_emerge->getCurrentMapgen();
	if (!mg)
		return;

	ManualMapVoxelManipulator *vm = mg->vm;
	if (!vm)
		return;

	if (!vm->m_area.contains(pos))
		return;

	s32 idx = vm->m_area.index(pos);
	vm->m_data[idx] = getNodeNoEx(pos);
	vm->m_flags[idx] &= ~VOXELFLAG_NO_DATA;

	vm->m_is_dirty = true;
}

/**
 * Get the ground level by searching for a non CONTENT_AIR node in a column from top to bottom
 */
s16 ServerMap::findGroundLevel(v2POS p2d, bool cacheBlocks)
{
	
	POS level;

	// The reference height is the original mapgen height
	POS referenceHeight = m_emerge->getGroundLevelAtPoint(p2d);
	POS maxSearchHeight =  63 + referenceHeight;
	POS minSearchHeight = -63 + referenceHeight;
	v3POS probePosition(p2d.X, maxSearchHeight, p2d.Y);
	v3POS blockPosition = getNodeBlockPos(probePosition);
	v3POS prevBlockPosition = blockPosition;

	// Cache the block to be inspected.
	if(cacheBlocks) {
		emergeBlock(blockPosition, false);
	}

	// Probes the nodes in the given column
	for(; probePosition.Y > minSearchHeight; probePosition.Y--)
	{
		if(cacheBlocks) {
			// Calculate the block position of the given node
			blockPosition = getNodeBlockPos(probePosition); 

			// If the node is in an different block, cache it
			if(blockPosition != prevBlockPosition) {
				emergeBlock(blockPosition, false);
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

void ServerMap::createDirs(std::string path)
{
	if(fs::CreateAllDirs(path) == false)
	{
		errorstream<<DTIME<<"ServerMap: Failed to create directory "
				<<"\""<<path<<"\""<<std::endl;
		throw BaseException("ServerMap failed to create directory");
	}
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

	u32 block_count = 0;
	u32 block_count_all = 0; // Number of blocks in memory

	// Don't do anything with sqlite unless something is really saved
	bool save_started = false;
	u32 n = 0, calls = 0, end_ms = porting::getTimeMs() + u32(1000 * g_settings->getFloat("dedicated_server_step"));
	if (!breakable)
		m_blocks_save_last = 0;

	{
		auto lock = breakable ? m_blocks.try_lock_shared_rec() : m_blocks.lock_shared_rec();
		if (!lock->owns_lock())
			return m_blocks_save_last;

		for(auto &jr : m_blocks)
		{
			if (n++ < m_blocks_save_last)
				continue;
			else
				m_blocks_save_last = 0;
			++calls;

			MapBlock *block = jr.second;

			if (!block)
				continue;

			block_count_all++;

			if(block->getModified() >= (u32)save_level)
			{
				// Lazy beginSave()
				if(!save_started){
					beginSave();
					save_started = true;
				}

				//modprofiler.add(block->getModifiedReason(), 1);
				auto lock = breakable ? block->try_lock_unique_rec() : block->lock_unique_rec();
				if (!lock->owns_lock())
					continue;

				saveBlock(block);
				block_count++;

				/*infostream<<"ServerMap: Written block ("
						<<block->getPos().X<<","
						<<block->getPos().Y<<","
						<<block->getPos().Z<<")"
						<<std::endl;*/
			}
		if (breakable && porting::getTimeMs() > end_ms) {
				m_blocks_save_last = n;
				break;
		}
	}
	}
	if (!calls)
		m_blocks_save_last = 0;

	if(save_started)
		endSave();

	/*
		Only print if something happened or saved whole map
	*/
	if(/*save_level == MOD_STATE_CLEAN
			||*/ block_count != 0)
	{
		infostream<<"ServerMap: Written: "
				<<block_count<<"/"<<block_count_all<<" blocks from "
				<<m_blocks.size();
		if (m_blocks_save_last)
			infostream<<" to "<< m_blocks_save_last;
		infostream<<std::endl;
		PrintInfo(infostream); // ServerMap/ClientMap:
		//infostream<<"Blocks modified by: "<<std::endl;
		modprofiler.print(infostream);
	}
	return m_blocks_save_last;
}

void ServerMap::listAllLoadableBlocks(std::list<v3s16> &dst)
{
	dbase->listAllLoadableBlocks(dst);
}

void ServerMap::listAllLoadedBlocks(std::list<v3s16> &dst)
{
	auto lock = m_blocks.lock_shared_rec();
	for(auto & i : m_blocks)
		dst.push_back(i.second->getPos());
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

	if (!params.writeJsonFile(m_savedir + DIR_DELIM + "map_meta.json")) {
		errorstream<<"cant write "<<m_savedir + DIR_DELIM + "map_meta.json"<<std::endl;
	}

	m_map_metadata_changed = false;

}

void ServerMap::loadMapMeta()
{
	DSTACK(__FUNCTION_NAME);

	Settings params;

	if (!params.readJsonFile(m_savedir + DIR_DELIM + "map_meta.json")) {
	//todo: remove deprecated

	std::string fullpath = m_savedir + DIR_DELIM "map_meta.txt";
	infostream<<"Cant read map_meta.json , fallback to " << fullpath << std::endl;
	std::ifstream is(fullpath.c_str(), std::ios_base::binary);
	if (!is.good()) {
		errorstream << "ServerMap::loadMapMeta(): "
				<< "could not open " << fullpath << std::endl;
		throw FileNotGoodException("Cannot open map metadata");
	}

	if (!params.parseConfigLines(is, "[end_of_params]")) {
		throw SerializationError("ServerMap::loadMapMeta(): "
				"[end_of_params] not found!");
	}

	}

	m_emerge->loadParamsFromSettings(&params);

	verbosestream << "ServerMap::loadMapMeta(): seed="
		<< m_emerge->params.seed << std::endl;
}

void ServerMap::beginSave()
{
	dbase->beginSave();
}

void ServerMap::endSave()
{
	dbase->endSave();
}

bool ServerMap::saveBlock(MapBlock *block)
{
	return saveBlock(block, dbase);
}

bool ServerMap::saveBlock(MapBlock *block, Database *db)
{
	v3s16 p3d = block->getPos();

	if (!block->isGenerated()) {
		infostream << "WARNING: saveBlock: Not writing not generated block p="<< p3d << std::endl;
		return true;
	}

	// Format used for writing
	u8 version = SER_FMT_VER_HIGHEST_WRITE;

	/*
		[0] u8 serialization version
		[1] data
	*/
	std::ostringstream o(std::ios_base::binary);
	o.write((char*) &version, 1);
	block->serialize(o, version, true);

	std::string data = o.str();
	bool ret = db->saveBlock(p3d, data);
	if(ret) {
		// We just wrote it to the disk so clear modified flag
		block->resetModified();
	}
	return ret;
}

MapBlock * ServerMap::loadBlock(v3s16 p3d)
{
	DSTACK(__FUNCTION_NAME);
	ScopeProfiler sp(g_profiler, "ServerMap::loadBlock");
	const auto sector = this;
	auto blob = dbase->loadBlock(p3d);
	if(!blob.length())
		return nullptr;

	try {
		std::istringstream is(blob, std::ios_base::binary);

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
		block = sector->getBlockNoCreateNoEx(p3d);
		if(block == NULL)
		{
			block = sector->createBlankBlockNoInsert(p3d);
			created_new = true;
		}

		// Read basic data
		if (!block->deSerialize(is, version, true))
			return nullptr;

		// If it's a new block, insert it to the map
		if(created_new)
			sector->insertBlock(block);

		// We just loaded it from, so it's up-to-date.
		block->resetModified();

		if (block->getLightingExpired()) {
			infostream<<"Loaded block with exiried lighting. (maybe sloooow appear), try recalc " << p3d<<std::endl;
			lighting_modified_blocks.set(p3d, nullptr);
		}

		return block;
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
	return nullptr;
}

void ServerMap::PrintInfo(std::ostream &out)
{
	out<<"ServerMap: ";
}

s16 ServerMap::updateBlockHeat(ServerEnvironment *env, v3POS p, MapBlock *block, std::map<v3POS, s16> * cache)
{
	auto bp = getNodeBlockPos(p);
	auto gametime = env->getGameTime();
	if (block) {
		if (gametime < block->heat_last_update)
			return block->heat + myrand_range(0, 1);
	} else if (!cache) {
		block = getBlockNoCreateNoEx(bp, true);
	}
	if (cache && cache->count(bp))
		return cache->at(bp) + myrand_range(0, 1);

	auto value = m_emerge->biomemgr->calcBlockHeat(p, getSeed(),
			env->getTimeOfDayF(), gametime * env->getTimeOfDaySpeed(), env->m_use_weather);

	if(block) {
		block->heat = value;
		block->heat_last_update = env->m_use_weather ? gametime + 30 : -1;
	}
	if (cache)
		(*cache)[bp] = value;
	return value + myrand_range(0, 1);
}

s16 ServerMap::updateBlockHumidity(ServerEnvironment *env, v3POS p, MapBlock *block, std::map<v3POS, s16> * cache)
{
	auto bp = getNodeBlockPos(p);
	auto gametime = env->getGameTime();
	if (block) {
		if (gametime < block->humidity_last_update)
			return block->humidity + myrand_range(0, 1);
	} else if (!cache) {
		block = getBlockNoCreateNoEx(bp, true);
	}
	if (cache && cache->count(bp))
		return cache->at(bp) + myrand_range(0, 1);

	auto value = m_emerge->biomemgr->calcBlockHumidity(p, getSeed(),
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

ManualMapVoxelManipulator::ManualMapVoxelManipulator(Map *map):
		VoxelManipulator(),
		m_is_dirty(false),
		m_create_area(false),
		m_map(map)
{
}

ManualMapVoxelManipulator::~ManualMapVoxelManipulator()
{
}

void ManualMapVoxelManipulator::initialEmerge(v3s16 blockpos_min,
						v3s16 blockpos_max, bool load_if_inexistent)
{
	TimeTaker timer1("initialEmerge");

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
		infostream<<" load_if_inexistent="<<load_if_inexistent;
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
			TimeTaker timer1("emerge load");

			block = m_map->getBlockNoCreate(p);
			if(!block || block->isDummy())
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
				// Fill with VOXELFLAG_NO_DATA
				for(s32 z=a.MinEdge.Z; z<=a.MaxEdge.Z; z++)
				for(s32 y=a.MinEdge.Y; y<=a.MaxEdge.Y; y++)
				{
					s32 i = m_area.index(a.MinEdge.X,y,z);
					memset(&m_flags[i], VOXELFLAG_NO_DATA, MAP_BLOCKSIZE);
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

	m_is_dirty = false;
}

void ManualMapVoxelManipulator::blitBackAll(
		std::map<v3s16, MapBlock*> *modified_blocks,
		bool overwrite_generated)
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
		if ((existed == false) || (block == NULL) ||
			(overwrite_generated == false && block->isGenerated() == true))
			continue;

		block->copyFrom(*this);

		if(modified_blocks)
			(*modified_blocks)[p] = block;
	}
}

//END
