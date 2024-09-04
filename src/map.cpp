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
#include <memory>
#include "irr_v3d.h"
#include "log.h"
#include "irr_v3d.h"
#include "irrlichttypes.h"
#include "mapsector.h"
#include "mapblock.h"
#ifndef SERVER
	#include "client/mapblock_mesh.h"
#endif
#include "filesys.h"
#include "voxel.h"
#include "voxelalgorithms.h"
#include "porting.h"
#include "serialization.h"
#include "nodemetadata.h"
#include "settings.h"
#include "log_types.h"
#include "profiler.h"
#include "nodedef.h"
#include "gamedef.h"
#include "util/directiontables.h"
#include "rollback_interface.h"
#include "environment.h"
#include "reflowscan.h"
#include "emerge.h"
#include "mapgen/mapgen_v6.h"
#include "mapgen/mg_biome.h"
#include "config.h"
#include "server.h"
#include "database/database.h"
#include "database/database-dummy.h"
#include "database/database-sqlite3.h"
#include "script/scripting_server.h"
#include "irrlicht_changes/printing.h"
#include <deque>
#include <queue>
#if USE_LEVELDB
#include "database/database-leveldb.h"
#endif
#if USE_REDIS
#include "database/database-redis.h"
#endif
#if USE_POSTGRESQL
#include "database/database-postgresql.h"
#endif


/*
	Map
*/

Map::Map(IGameDef *gamedef):
	m_gamedef(gamedef),
	m_nodedef(gamedef->ndef())
{
	m_liquid_step_flow = 1000;
	time_life = 0;
	getBlockCacheFlush();
}

Map::~Map()
{
	auto lock = m_blocks.lock_unique_rec();
/*
	for (auto & ir : m_blocks_delete_1)
		delete ir.first;
	for (auto & ir : m_blocks_delete_2)
		delete ir.first;
	for(auto & ir : m_blocks)
		delete ir.second;
*/		
	getBlockCacheFlush();
#if WTF
	/*
		Free all MapSectors
	*/
	for (auto &sector : m_sectors) {
		delete sector.second;
	}
#endif
}

void Map::addEventReceiver(MapEventReceiver *event_receiver)
{
	m_event_receivers.insert(event_receiver);
}

void Map::removeEventReceiver(MapEventReceiver *event_receiver)
{
	m_event_receivers.erase(event_receiver);
}

void Map::dispatchEvent(const MapEditEvent &event)
{
	for (MapEventReceiver *event_receiver : m_event_receivers) {
		event_receiver->onMapEditEvent(event);
	}
}

/*
MapSector * Map::getSectorNoGenerateNoLock(v2bpos_t p)
{
	if(m_sector_cache != NULL && p == m_sector_cache_p){
		MapSector * sector = m_sector_cache;
		return sector;
	}

	auto n = m_sectors.find(p);

	if (n == m_sectors.end())
		return NULL;

	MapSector *sector = n->second;

	// Cache the last result
	m_sector_cache_p = p;
	m_sector_cache = sector;

	return sector;
}

MapSector *Map::getSectorNoGenerate(v2bpos_t p)
{
	return getSectorNoGenerateNoLock(p);
}

MapBlock *Map::getBlockNoCreateNoEx(v3bpos_t p3d)
{
	v2bpos_t p2d(p3d.X, p3d.Z);
	MapSector *sector = getSectorNoGenerate(p2d);
	if (!sector)
		return nullptr;
	MapBlock *block = sector->getBlockNoCreateNoEx(p3d.Y);
	return block;
}
*/

MapBlock *Map::getBlockNoCreate(v3bpos_t p3d)
{
	MapBlock *block = getBlockNoCreateNoEx(p3d);
	if(block == NULL)
		throw InvalidPositionException("getBlockNoCreate block=NULL");
	return block;
}

bool Map::isValidPosition(v3pos_t p)
{
	v3bpos_t blockpos = getNodeBlockPos(p);
	MapBlock *block = getBlockNoCreateNoEx(blockpos);
	return (block != NULL);
}

// Returns a CONTENT_IGNORE node if not found
MapNode Map::getNode(v3pos_t p, bool *is_valid_position)
{
#ifndef NDEBUG
	ScopeProfiler sp(g_profiler, "Map: getNodeNoEx");
#endif

	v3bpos_t blockpos = getNodeBlockPos(p);
	MapBlock *block = getBlockNoCreateNoEx(blockpos);
	if (block == NULL) {
		if (is_valid_position != NULL)
			*is_valid_position = false;
		return {CONTENT_IGNORE};
	}

	v3pos_t relpos = p - getBlockPosRelative(blockpos);
	MapNode node = block->getNodeNoCheck(relpos);
	if (is_valid_position != NULL)
		*is_valid_position = true;
	return node;
}

static void set_node_in_block(MapBlock *block, v3pos_t relpos, MapNode n, bool important = false)
{
	// Never allow placing CONTENT_IGNORE, it causes problems
	if(n.getContent() == CONTENT_IGNORE){
		const NodeDefManager *nodedef = block->getParent()->getNodeDefManager();
		v3bpos_t blockpos = block->getPos();
		v3pos_t p = getBlockPosRelative(blockpos) + relpos;
		errorstream<<"Not allowing to place CONTENT_IGNORE"
				<<" while trying to replace \""
				<<nodedef->get(block->getNodeNoCheck(relpos)).name
				<<"\" at "<<p<<" (block "<<blockpos<<")"<<std::endl;
		return;
	}
	block->setNodeNoCheck(relpos, n, important);
}

// throws InvalidPositionException if not found
void Map::setNode(v3pos_t p, MapNode n, bool important)
{
	v3bpos_t blockpos = getNodeBlockPos(p);
	MapBlock *block = getBlockNoCreate(blockpos);
	v3pos_t relpos = p - blockpos*MAP_BLOCKSIZE;
	set_node_in_block(block, relpos, n, important);
}

void Map::addNodeAndUpdate(v3pos_t p, MapNode n,
		std::map<v3bpos_t, MapBlock*> &modified_blocks,
		bool remove_metadata, int fast, bool important)
{
	if (fast == 1 || fast == 2) { // fast: 1: just place node; 2: place ang get light from old; 3: place, recalculate light and skip liquid queue
		if (fast == 2 && !n.param1) {
			MapNode oldnode = getNode(p);
			if (oldnode) {
				ContentLightingFlags f = m_nodedef->getLightingFlags(n);
				ContentLightingFlags oldf = m_nodedef->getLightingFlags(oldnode);

				n.setLight(LIGHTBANK_DAY, oldnode.getLightRaw(LIGHTBANK_DAY, oldf), f);
				n.setLight(LIGHTBANK_NIGHT, oldnode.getLightRaw(LIGHTBANK_NIGHT, oldf), f);
			}

/*
			const auto & f = ndef->get(from_node);
			if (f.light_propagates || f.sunlight_propagates || f.light_source) {
				MapBlock *block = getBlockNoCreateNoEx(getNodeBlockPos(p));
				if (block)
					block->setLightingExpired(true);
			}
*/

		}
		if (remove_metadata)
			removeNodeMetadata(p);
		setNode(p, n, important);
		modified_blocks[getNodeBlockPos(p)] = nullptr;
		return;
	}


	// Collect old node for rollback
	RollbackNode rollback_oldnode(this, p, m_gamedef);

	v3bpos_t blockpos = getNodeBlockPos(p);
	MapBlock *block = getBlockNoCreate(blockpos);
	v3pos_t relpos = p - getBlockPosRelative(blockpos);

	// This is needed for updating the lighting
	MapNode oldnode = block->getNodeNoCheck(relpos);

	// Remove node metadata
	if (remove_metadata) {
		removeNodeMetadata(p);
	}

	// Set the node on the map
	ContentLightingFlags f = m_nodedef->getLightingFlags(n);
	ContentLightingFlags oldf = m_nodedef->getLightingFlags(oldnode);
	if (f == oldf) {
		// No light update needed, just copy over the old light.
		n.setLight(LIGHTBANK_DAY, oldnode.getLightRaw(LIGHTBANK_DAY, oldf), f);
		n.setLight(LIGHTBANK_NIGHT, oldnode.getLightRaw(LIGHTBANK_NIGHT, oldf), f);
		set_node_in_block(block, relpos, n, important);

		modified_blocks[blockpos] = block;
	} else {
		// Ignore light (because calling voxalgo::update_lighting_nodes)
		n.setLight(LIGHTBANK_DAY, 0, f);
		n.setLight(LIGHTBANK_NIGHT, 0, f);
		set_node_in_block(block, relpos, n, important);

		// Update lighting
		std::vector<std::pair<v3pos_t, MapNode> > oldnodes;
		oldnodes.emplace_back(p, oldnode);
		voxalgo::update_lighting_nodes(this, oldnodes, modified_blocks);

		for (auto &modified_block : modified_blocks) {
			modified_block.second->expireDayNightDiff();
		}
	}

	// Report for rollback
	if(m_gamedef->rollback())
	{
		RollbackNode rollback_newnode(this, p, m_gamedef);
		RollbackAction action;
		action.setSetNode(p, rollback_oldnode, rollback_newnode);
		m_gamedef->rollback()->reportAction(action);
	}
}

void Map::removeNodeAndUpdate(v3pos_t p,
		std::map<v3bpos_t, MapBlock*> &modified_blocks, int fast, bool important)
{
	addNodeAndUpdate(p, MapNode(CONTENT_AIR), modified_blocks, true, fast, important);
}

bool Map::addNodeWithEvent(v3pos_t p, MapNode n, bool remove_metadata, bool important)
{
	MapEditEvent event;
	event.type = remove_metadata ? MEET_ADDNODE : MEET_SWAPNODE;
	event.p = p;
	event.n = n;

	bool succeeded = true;
	try{
		std::map<v3bpos_t, MapBlock*> modified_blocks;
		addNodeAndUpdate(p, n, modified_blocks, remove_metadata, 0, important);

		event.setModifiedBlocks(modified_blocks);
	}
	catch(InvalidPositionException &e){
		succeeded = false;
	}

	dispatchEvent(event);

	return succeeded;
}

bool Map::removeNodeWithEvent(v3pos_t p, int fast, bool important)
{
	MapEditEvent event;
	event.type = MEET_REMOVENODE;
	event.p = p;

	bool succeeded = true;
	try{
		std::map<v3bpos_t, MapBlock*> modified_blocks;
		removeNodeAndUpdate(p, modified_blocks, fast, important);

		event.setModifiedBlocks(modified_blocks);
	}
	catch(InvalidPositionException &e){
		succeeded = false;
	}

	dispatchEvent(event);

	return succeeded;
}

struct TimeOrderedMapBlock {
	MapSector *sect;
	MapBlock *block;

	TimeOrderedMapBlock(MapSector *sect, MapBlock *block) :
		sect(sect),
		block(block)
	{}

	bool operator<(const TimeOrderedMapBlock &b) const
	{
		return block->getUsageTimer() < b.block->getUsageTimer();
	};
};

/*
	Updates usage timers
*/
#if WTF
void Map::timerUpdate(float dtime, float unload_timeout, s32 max_loaded_blocks,
		std::vector<v3bpos_t> *unloaded_blocks)
{
	bool save_before_unloading = maySaveBlocks();

	// Profile modified reasons
	Profiler modprofiler;

	std::vector<v2bpos_t> sector_deletion_queue;
	u32 deleted_blocks_count = 0;
	u32 saved_blocks_count = 0;
	u32 block_count_all = 0;
	u32 locked_blocks = 0;

	const auto start_time = porting::getTimeUs();
	beginSave();

	// If there is no practical limit, we spare creation of mapblock_queue
	if (max_loaded_blocks < 0) {
		for (auto &sector_it : m_sectors) {
			MapSector *sector = sector_it.second;

			bool all_blocks_deleted = true;

			MapBlockVect blocks;
			sector->getBlocks(blocks);

			for (MapBlock *block : blocks) {
				block->incrementUsageTimer(dtime);

				if (block->refGet() == 0
						&& block->getUsageTimer() > unload_timeout) {
					v3bpos_t p = block->getPos();

					// Save if modified
					if (block->getModified() != MOD_STATE_CLEAN
							&& save_before_unloading) {
						modprofiler.add(block->getModifiedReasonString(), 1);
						if (!saveBlock(block))
							continue;
						saved_blocks_count++;
					}

					// Delete from memory
					sector->deleteBlock(block);

					if (unloaded_blocks)
						unloaded_blocks->push_back(p);

					deleted_blocks_count++;
				} else {
					all_blocks_deleted = false;
					block_count_all++;
				}
			}

			// Delete sector if we emptied it
			if (all_blocks_deleted) {
				sector_deletion_queue.push_back(sector_it.first);
			}
		}
	} else {
		std::priority_queue<TimeOrderedMapBlock> mapblock_queue;
		for (auto &sector_it : m_sectors) {
			MapSector *sector = sector_it.second;

			MapBlockVect blocks;
			sector->getBlocks(blocks);

			for (MapBlock *block : blocks) {
				block->incrementUsageTimer(dtime);
				mapblock_queue.push(TimeOrderedMapBlock(sector, block));
			}
		}
		block_count_all = mapblock_queue.size();

		// Delete old blocks, and blocks over the limit from the memory
		while (!mapblock_queue.empty() && ((s32)mapblock_queue.size() > max_loaded_blocks
				|| mapblock_queue.top().block->getUsageTimer() > unload_timeout)) {
			TimeOrderedMapBlock b = mapblock_queue.top();
			mapblock_queue.pop();

			MapBlock *block = b.block;

			if (block->refGet() != 0) {
				locked_blocks++;
				continue;
			}

			v3bpos_t p = block->getPos();

			// Save if modified
			if (block->getModified() != MOD_STATE_CLEAN && save_before_unloading) {
				modprofiler.add(block->getModifiedReasonString(), 1);
				if (!saveBlock(block))
					continue;
				saved_blocks_count++;
			}

			// Delete from memory
			b.sect->deleteBlock(block);

			if (unloaded_blocks)
				unloaded_blocks->push_back(p);

			deleted_blocks_count++;
			block_count_all--;
		}

		// Delete empty sectors
		for (auto &sector_it : m_sectors) {
			if (sector_it.second->empty()) {
				sector_deletion_queue.push_back(sector_it.first);
			}
		}
	}

	endSave();
	const auto end_time = porting::getTimeUs();

	reportMetrics(end_time - start_time, saved_blocks_count, block_count_all);

	// Finally delete the empty sectors
	deleteSectors(sector_deletion_queue);

	if(deleted_blocks_count != 0)
	{
		PrintInfo(infostream); // ServerMap/ClientMap:
		infostream<<"Unloaded "<<deleted_blocks_count
				<<" blocks from memory";
		if(save_before_unloading)
			infostream<<", of which "<<saved_blocks_count<<" were written";
		infostream<<", "<<block_count_all<<" blocks in memory, " << locked_blocks << " locked";
		infostream<<"."<<std::endl;
		if(saved_blocks_count != 0){
			PrintInfo(infostream); // ServerMap/ClientMap:
			infostream<<"Blocks modified by: "<<std::endl;
			modprofiler.print(infostream);
		}
	}
}

#endif

void Map::unloadUnreferencedBlocks(std::vector<v3bpos_t> *unloaded_blocks)
{
	timerUpdate(0.0, -1.0, 100, unloaded_blocks);
}

#if WTF
void Map::deleteSectors(std::vector<v2bpos_t> &sectorList)
{
	for (v2bpos_t j : sectorList) {
		MapSector *sector = m_sectors[j];
		// If sector is in sector cache, remove it from there
		if(m_sector_cache == sector)
			m_sector_cache = NULL;
		// Remove from map and delete
		m_sectors.erase(j);
		delete sector;
	}
}
#endif

void Map::PrintInfo(std::ostream &out)
{
	out<<"Map: ";
}

#define WATER_DROP_BOOST 4

const static v3pos_t liquid_6dirs[6] = {
	// order: upper before same level before lower
	v3pos_t( 0, 1, 0),
	v3pos_t( 0, 0, 1),
	v3pos_t( 1, 0, 0),
	v3pos_t( 0, 0,-1),
	v3pos_t(-1, 0, 0),
	v3pos_t( 0,-1, 0)
};

enum NeighborType : u8 {
	NEIGHBOR_UPPER,
	NEIGHBOR_SAME_LEVEL,
	NEIGHBOR_LOWER
};

struct NodeNeighbor {
	MapNode n;
	NeighborType t;
	v3pos_t p;

	NodeNeighbor()
		: n(CONTENT_AIR), t(NEIGHBOR_SAME_LEVEL)
	{ }

	NodeNeighbor(const MapNode &node, NeighborType n_type, const v3pos_t &pos)
		: n(node),
		  t(n_type),
		  p(pos)
	{ }
};

void ServerMap::transforming_liquid_add(v3pos_t p) {
		std::lock_guard<std::mutex> lock(m_transforming_liquid_mutex);
		m_transforming_liquid.push_back(p);
}

size_t ServerMap::transformLiquids(std::map<v3bpos_t, MapBlock*> &modified_blocks,
		ServerEnvironment *env
		, Server *m_server, unsigned int max_cycle_ms
		)
{
	g_profiler->avg("Server: liquids queue", transforming_liquid_size());

	if (thread_local const auto static liquid_real = g_settings->getBool("liquid_real"); liquid_real)
		return ServerMap::transformLiquidsReal(m_server, max_cycle_ms);

	const auto end_ms = porting::getTimeMs() + max_cycle_ms;

	u32 loopcount = 0;
	const auto initial_size = transforming_liquid_size();

	/*if(initial_size != 0)
		infostream<<"transformLiquids(): initial_size="<<initial_size<<std::endl;*/

	// list of nodes that due to viscosity have not reached their max level height
	std::vector<v3pos_t> must_reflow;

/*
	std::vector<std::pair<v3pos_t, MapNode> > changed_nodes;
*/

	std::vector<v3pos_t> check_for_falling;

	u32 liquid_loop_max = g_settings->getS32("liquid_loop_max");
	u32 loop_max = liquid_loop_max;

	while (transforming_liquid_size() != 0)
	{
		// This should be done here so that it is done when continue is used
		if (loopcount >= initial_size || loopcount >= loop_max || porting::getTimeMs() > end_ms)
			break;
		loopcount++;

		/*
			Get a queued transforming liquid node
		*/
		v3pos_t p0 = transforming_liquid_pop();

		MapNode n0 = getNode(p0);

		/*
			Collect information about current node
		 */
		s8 liquid_level = -1;
		// The liquid node which will be placed there if
		// the liquid flows into this node.
		content_t liquid_kind = CONTENT_IGNORE;
		// The node which will be placed there if liquid
		// can't flow into this node.
		content_t floodable_node = CONTENT_AIR;
		const ContentFeatures &cf = m_nodedef->get(n0);
		LiquidType liquid_type = cf.liquid_type;
		switch (liquid_type) {
			case LIQUID_SOURCE:
				liquid_level = LIQUID_LEVEL_SOURCE;
				liquid_kind = cf.liquid_alternative_flowing_id;
				break;
			case LIQUID_FLOWING:
				liquid_level = (n0.param2 & LIQUID_LEVEL_MASK);
				liquid_kind = n0.getContent();
				break;
			case LIQUID_NONE:
				// if this node is 'floodable', it *could* be transformed
				// into a liquid, otherwise, continue with the next node.
				if (!cf.floodable)
					continue;
				floodable_node = n0.getContent();
				liquid_kind = CONTENT_AIR;
				break;
		}

		/*
			Collect information about the environment
		 */
		NodeNeighbor sources[6]; // surrounding sources
		int num_sources = 0;
		NodeNeighbor flows[6]; // surrounding flowing liquid nodes
		int num_flows = 0;
		NodeNeighbor airs[6]; // surrounding air
		int num_airs = 0;
		NodeNeighbor neutrals[6]; // nodes that are solid or another kind of liquid
		int num_neutrals = 0;
		bool flowing_down = false;
		bool ignored_sources = false;
		bool floating_node_above = false;
		for (u16 i = 0; i < 6; i++) {
			NeighborType nt = NEIGHBOR_SAME_LEVEL;
			switch (i) {
				case 0:
					nt = NEIGHBOR_UPPER;
					break;
				case 5:
					nt = NEIGHBOR_LOWER;
					break;
				default:
					break;
			}
			v3pos_t npos = p0 + liquid_6dirs[i];
			NodeNeighbor nb(getNode(npos), nt, npos);
			const ContentFeatures &cfnb = m_nodedef->get(nb.n);
			if (nt == NEIGHBOR_UPPER && cfnb.floats)
				floating_node_above = true;
			switch (cfnb.liquid_type) {
				case LIQUID_NONE:
					if (cfnb.floodable) {
						airs[num_airs++] = nb;
						// if the current node is a water source the neighbor
						// should be enqueded for transformation regardless of whether the
						// current node changes or not.
						if (nb.t != NEIGHBOR_UPPER && liquid_type != LIQUID_NONE)
							transforming_liquid_add(npos);
						// if the current node happens to be a flowing node, it will start to flow down here.
						if (nb.t == NEIGHBOR_LOWER)
							flowing_down = true;
					} else {
						neutrals[num_neutrals++] = nb;
						if (nb.n.getContent() == CONTENT_IGNORE) {
							// If node below is ignore prevent water from
							// spreading outwards and otherwise prevent from
							// flowing away as ignore node might be the source
							if (nb.t == NEIGHBOR_LOWER)
								flowing_down = true;
							else
								ignored_sources = true;
						}
					}
					break;
				case LIQUID_SOURCE:
					// if this node is not (yet) of a liquid type, choose the first liquid type we encounter
					if (liquid_kind == CONTENT_AIR)
						liquid_kind = cfnb.liquid_alternative_flowing_id;
					if (cfnb.liquid_alternative_flowing_id != liquid_kind) {
						neutrals[num_neutrals++] = nb;
					} else {
						// Do not count bottom source, it will screw things up
						if(nt != NEIGHBOR_LOWER)
							sources[num_sources++] = nb;
					}
					break;
				case LIQUID_FLOWING:
					if (nb.t != NEIGHBOR_SAME_LEVEL ||
						(nb.n.param2 & LIQUID_FLOW_DOWN_MASK) != LIQUID_FLOW_DOWN_MASK) {
						// if this node is not (yet) of a liquid type, choose the first liquid type we encounter
						// but exclude falling liquids on the same level, they cannot flow here anyway
						if (liquid_kind == CONTENT_AIR)
							liquid_kind = cfnb.liquid_alternative_flowing_id;
					}
					if (cfnb.liquid_alternative_flowing_id != liquid_kind) {
						neutrals[num_neutrals++] = nb;
					} else {
						flows[num_flows++] = nb;
						if (nb.t == NEIGHBOR_LOWER)
							flowing_down = true;
					}
					break;
			}
		}

		u16 level_max = m_nodedef->get(liquid_kind).getMaxLevel(); // source level
		if (level_max <= 1)
			continue;

		/*
			decide on the type (and possibly level) of the current node
		 */
		content_t new_node_content;
		s8 new_node_level = -1;
		s8 max_node_level = -1;

		u8 range = m_nodedef->get(liquid_kind).liquid_range;
		if (range > LIQUID_LEVEL_MAX + 1)
			range = LIQUID_LEVEL_MAX + 1;

		if ((num_sources >= 2 && m_nodedef->get(liquid_kind).liquid_renewable) || liquid_type == LIQUID_SOURCE) {
			// liquid_kind will be set to either the flowing alternative of the node (if it's a liquid)
			// or the flowing alternative of the first of the surrounding sources (if it's air), so
			// it's perfectly safe to use liquid_kind here to determine the new node content.
			new_node_content = m_nodedef->get(liquid_kind).liquid_alternative_source_id;
		} else if (num_sources >= 1 && sources[0].t != NEIGHBOR_LOWER) {
			// liquid_kind is set properly, see above
			max_node_level = new_node_level = LIQUID_LEVEL_MAX;
			if (new_node_level >= (LIQUID_LEVEL_MAX + 1 - range))
				new_node_content = liquid_kind;
			else
				new_node_content = floodable_node;
		} else if (ignored_sources && liquid_level >= 0) {
			// Maybe there are neighboring sources that aren't loaded yet
			// so prevent flowing away.
			new_node_level = liquid_level;
			new_node_content = liquid_kind;
		} else {
			// no surrounding sources, so get the maximum level that can flow into this node
			for (u16 i = 0; i < num_flows; i++) {
				u8 nb_liquid_level = (flows[i].n.param2 & LIQUID_LEVEL_MASK);
				switch (flows[i].t) {
					case NEIGHBOR_UPPER:
						if (nb_liquid_level + WATER_DROP_BOOST > max_node_level) {
							max_node_level = LIQUID_LEVEL_MAX;
							if (nb_liquid_level + WATER_DROP_BOOST < LIQUID_LEVEL_MAX)
								max_node_level = nb_liquid_level + WATER_DROP_BOOST;
						} else if (nb_liquid_level > max_node_level) {
							max_node_level = nb_liquid_level;
						}
						break;
					case NEIGHBOR_LOWER:
						break;
					case NEIGHBOR_SAME_LEVEL:
						if ((flows[i].n.param2 & LIQUID_FLOW_DOWN_MASK) != LIQUID_FLOW_DOWN_MASK &&
								nb_liquid_level > 0 && nb_liquid_level - 1 > max_node_level)
							max_node_level = nb_liquid_level - 1;
						break;
				}
			}

			u8 viscosity = m_nodedef->get(liquid_kind).liquid_viscosity;
			if (viscosity > 1 && max_node_level != liquid_level) {
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
			} else {
				new_node_level = max_node_level;
			}

			if (max_node_level >= (LIQUID_LEVEL_MAX + 1 - range))
				new_node_content = liquid_kind;
			else
				new_node_content = floodable_node;

		}

		if (!new_node_level && m_nodedef->get(n0.getContent()).liquid_type == LIQUID_FLOWING)
			new_node_content = CONTENT_AIR;

		//if (liquid_level == new_node_level || new_node_level < 0)
		//	continue;

		/*
			check if anything has changed. if not, just continue with the next node.
		 */
		if (new_node_content == n0.getContent() &&
				(m_nodedef->get(n0.getContent()).liquid_type != LIQUID_FLOWING ||
				((n0.param2 & LIQUID_LEVEL_MASK) == (u8)new_node_level &&
				((n0.param2 & LIQUID_FLOW_DOWN_MASK) == LIQUID_FLOW_DOWN_MASK)
				== flowing_down)))
			continue;

		//errorstream << " was="<<(int)liquid_level<<" new="<< (int)new_node_level<< " ncon="<< (int)new_node_content << " flodo="<<(int)flowing_down<< " lmax="<<level_max<< " nameNE="<<nodemgr->get(new_node_content).name<<" nums="<<(int)num_sources<<" wasname="<<nodemgr->get(n0).name<<std::endl;

		/*
			check if there is a floating node above that needs to be updated.
		 */
		if (floating_node_above && new_node_content == CONTENT_AIR)
			check_for_falling.push_back(p0);

		/*
			update the current node
		 */
		MapNode n00 = n0;
		//bool flow_down_enabled = (flowing_down && ((n0.param2 & LIQUID_FLOW_DOWN_MASK) != LIQUID_FLOW_DOWN_MASK));
		if (m_nodedef->get(new_node_content).liquid_type == LIQUID_FLOWING) {
			// set level to last 3 bits, flowing down bit to 4th bit
			n0.param2 = (flowing_down ? LIQUID_FLOW_DOWN_MASK : 0x00) | (new_node_level & LIQUID_LEVEL_MASK);
		} else {
			// set the liquid level and flow bits to 0
			n0.param2 &= ~(LIQUID_LEVEL_MASK | LIQUID_FLOW_DOWN_MASK);
		}

		// change the node.
		n0.setContent(new_node_content);

		// on_flood() the node
		if (floodable_node != CONTENT_AIR) {
			if (env->getScriptIface()->node_on_flood(p0, n00, n0))
				continue;
		}

		// Ignore light (because calling voxalgo::update_lighting_nodes)
		ContentLightingFlags f0 = m_nodedef->getLightingFlags(n0);
		n0.setLight(LIGHTBANK_DAY, 0, f0);
		n0.setLight(LIGHTBANK_NIGHT, 0, f0);

		// Find out whether there is a suspect for this action
		std::string suspect;
		if (m_gamedef->rollback())
			suspect = m_gamedef->rollback()->getSuspect(p0, 83, 1);

		if (m_gamedef->rollback() && !suspect.empty()) {
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
			try {
				setNode(p0, n0);
			}
			catch(InvalidPositionException &e) {
				infostream<<"transformLiquids: setNode() failed:"<<p0<<":"<<e.what()<<std::endl;
			}
		}

/*
		v3bpos_t blockpos = getNodeBlockPos(p0);
		MapBlock *block = getBlockNoCreateNoEx(blockpos);
		if (block != NULL) {
			modified_blocks[blockpos] =  block;
			changed_nodes.emplace_back(p0, n00);
		}
*/

		/*
			enqueue neighbors for update if necessary
		 */
		switch (m_nodedef->get(n0.getContent()).liquid_type) {
			case LIQUID_SOURCE:
			case LIQUID_FLOWING:
				// make sure source flows into all neighboring nodes
				for (u16 i = 0; i < num_flows; i++)
					if (flows[i].t != NEIGHBOR_UPPER)
						transforming_liquid_add(flows[i].p);
				for (u16 i = 0; i < num_airs; i++)
					if (airs[i].t != NEIGHBOR_UPPER)
						transforming_liquid_add(airs[i].p);
				break;
			case LIQUID_NONE:
				// this flow has turned to air; neighboring flows might need to do the same
				for (u16 i = 0; i < num_flows; i++)
					transforming_liquid_add(flows[i].p);
				break;
		}
	}

	u32 ret = loopcount >= initial_size ? 0 : transforming_liquid_size();

	//infostream<<"Map::transformLiquids(): loopcount="<<loopcount<<" per="<<timer.getTimerTime()<<" ret="<<ret<<std::endl;

	for (const auto &iter : must_reflow)
		transforming_liquid_add(iter);

/*
	voxalgo::update_lighting_nodes(this, changed_nodes, modified_blocks);

	for (const v3pos_t &p : check_for_falling) {
		env->getScriptIface()->check_for_falling(p);
	}

	env->getScriptIface()->on_liquid_transformed(changed_nodes);
*/

	/* ----------------------------------------------------------------------
	 * Manage the queue so that it does not grow indefinitely
	 */
	u16 time_until_purge = g_settings->getU16("liquid_queue_purge_time");

	if (time_until_purge == 0)
		return ret; // Feature disabled

	time_until_purge *= 1000;	// seconds -> milliseconds

	u64 curr_time = porting::getTimeMs();
	u32 prev_unprocessed = m_unprocessed_count;
	m_unprocessed_count = transforming_liquid_size();

	// if unprocessed block count is decreasing or stable
	if (m_unprocessed_count <= prev_unprocessed) {
		m_queue_size_timer_started = false;
	} else {
		if (!m_queue_size_timer_started)
			m_inc_trending_up_start_time = curr_time;
		m_queue_size_timer_started = true;
	}

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
			transforming_liquid_pop();

		m_queue_size_timer_started = false; // optimistically assume we can keep up now
		m_unprocessed_count = transforming_liquid_size();
	}

	g_profiler->add("Server: liquids processed", loopcount);

	return ret;
}

std::vector<v3pos_t> Map::findNodesWithMetadata(v3pos_t p1, v3pos_t p2)
{
	std::vector<v3pos_t> positions_with_meta;

	sortBoxVerticies(p1, p2);
	v3bpos_t bpmin = getNodeBlockPos(p1);
	v3bpos_t bpmax = getNodeBlockPos(p2);

	VoxelArea area(p1, p2);

	for (bpos_t z = bpmin.Z; z <= bpmax.Z; z++)
	for (bpos_t y = bpmin.Y; y <= bpmax.Y; y++)
	for (bpos_t x = bpmin.X; x <= bpmax.X; x++) {
		v3bpos_t blockpos(x, y, z);

		MapBlock *block = getBlockNoCreateNoEx(blockpos, false, true);
		if (!block) {
			verbosestream << "Map::getNodeMetadata(): Need to emerge "
				<< blockpos << std::endl;
			block = emergeBlock(blockpos, false);
		}
		if (!block) {
			infostream << "WARNING: Map::getNodeMetadata(): Block not found"
				<< std::endl;
			continue;
		}

		v3pos_t p_base = getBlockPosRelative(blockpos);
		std::vector<v3pos_t> keys = block->m_node_metadata.getAllKeys();
		for (size_t i = 0; i != keys.size(); i++) {
			v3pos_t p(keys[i] + p_base);
			if (!area.contains(p))
				continue;

			positions_with_meta.push_back(p);
		}
	}

	return positions_with_meta;
}

NodeMetadata *Map::getNodeMetadata(v3pos_t p)
{
	v3bpos_t blockpos = getNodeBlockPos(p);
	v3pos_t p_rel = p - getBlockPosRelative(blockpos);
	MapBlock *block = getBlockNoCreateNoEx(blockpos, false, true);
	if(!block){
		infostream<<"Map::getNodeMetadata(): Need to emerge "
				<<blockpos<<std::endl;
		block = emergeBlock(blockpos, false);
	}
	if(!block){
		warningstream<<"Map::getNodeMetadata(): Block not found"
				<<std::endl;
		return NULL;
	}
	NodeMetadata *meta = block->m_node_metadata.get(p_rel);
	return meta;
}

bool Map::setNodeMetadata(v3pos_t p, NodeMetadata *meta)
{
	v3bpos_t blockpos = getNodeBlockPos(p);
	v3pos_t p_rel = p - getBlockPosRelative(blockpos);
	MapBlock *block = getBlockNoCreateNoEx(blockpos, false, true);
	if(!block){
		infostream<<"Map::setNodeMetadata(): Need to emerge "
				<<blockpos<<std::endl;
		block = emergeBlock(blockpos, false);
	}
	if(!block){
		warningstream<<"Map::setNodeMetadata(): Block not found"
				<<std::endl;
		return false;
	}
	block->m_node_metadata.set(p_rel, meta);
	return true;
}

void Map::removeNodeMetadata(v3pos_t p)
{
	v3bpos_t blockpos = getNodeBlockPos(p);
	v3pos_t p_rel = p - getBlockPosRelative(blockpos);
	MapBlock *block = getBlockNoCreateNoEx(blockpos, false, true);
	if(block == NULL)
	{
		verbosestream<<"Map::removeNodeMetadata(): Block not found"
				<<std::endl;
		return;
	}
	block->m_node_metadata.remove(p_rel);
}

NodeTimer Map::getNodeTimer(v3pos_t p)
{
	v3bpos_t blockpos = getNodeBlockPos(p);
	v3pos_t p_rel = p - getBlockPosRelative(blockpos);
	MapBlock *block = getBlockNoCreateNoEx(blockpos, false, true);
	if(!block){
		infostream<<"Map::getNodeTimer(): Need to emerge "
				<<blockpos<<std::endl;
		block = emergeBlock(blockpos, false);
	}
	if(!block){
		warningstream<<"Map::getNodeTimer(): Block not found"
				<<std::endl;
		return NodeTimer();
	}
	NodeTimer t = block->getNodeTimer(p_rel);
	NodeTimer nt(t.timeout, t.elapsed, p);
	return nt;
}

void Map::setNodeTimer(const NodeTimer &t)
{
	v3pos_t p = t.position;
	v3bpos_t blockpos = getNodeBlockPos(p);
	v3pos_t p_rel = p - getBlockPosRelative(blockpos);
	MapBlock *block = getBlockNoCreateNoEx(blockpos, false, true);
	if(!block){
		infostream<<"Map::setNodeTimer(): Need to emerge "
				<<blockpos<<std::endl;
		block = emergeBlock(blockpos, false);
	}
	if(!block){
		warningstream<<"Map::setNodeTimer(): Block not found"
				<<std::endl;
		return;
	}
	NodeTimer nt(t.timeout, t.elapsed, p_rel);
	block->setNodeTimer(nt);
}

void Map::removeNodeTimer(v3pos_t p)
{
	v3bpos_t blockpos = getNodeBlockPos(p);
	v3pos_t p_rel = p - getBlockPosRelative(blockpos);
	MapBlock *block = getBlockNoCreateNoEx(blockpos);
	if(block == NULL)
	{
		warningstream<<"Map::removeNodeTimer(): Block not found"
				<<std::endl;
		return;
	}
	block->removeNodeTimer(p_rel);
}

bool Map::determineAdditionalOcclusionCheck(const v3pos_t &pos_camera,
	const core::aabbox3d<pos_t> &block_bounds, v3pos_t &check)
{
	/*
		This functions determines the node inside the target block that is
		closest to the camera position. This increases the occlusion culling
		accuracy in straight and diagonal corridors.
		The returned position will be occlusion checked first in addition to the
		others (8 corners + center).
		No position is returned if
		- the closest node is a corner, corners are checked anyway.
		- the camera is inside the target block, it will never be occluded.
	*/
#define CLOSEST_EDGE(pos, bounds, axis) \
	((pos).axis <= (bounds).MinEdge.axis) ? (bounds).MinEdge.axis : \
	(bounds).MaxEdge.axis

	bool x_inside = (block_bounds.MinEdge.X <= pos_camera.X) &&
			(pos_camera.X <= block_bounds.MaxEdge.X);
	bool y_inside = (block_bounds.MinEdge.Y <= pos_camera.Y) &&
			(pos_camera.Y <= block_bounds.MaxEdge.Y);
	bool z_inside = (block_bounds.MinEdge.Z <= pos_camera.Z) &&
			(pos_camera.Z <= block_bounds.MaxEdge.Z);

	if (x_inside && y_inside && z_inside)
		return false; // Camera inside target mapblock

	// straight
	if (x_inside && y_inside) {
		check = v3pos_t(pos_camera.X, pos_camera.Y, 0);
		check.Z = CLOSEST_EDGE(pos_camera, block_bounds, Z);
		return true;
	} else if (y_inside && z_inside) {
		check = v3pos_t(0, pos_camera.Y, pos_camera.Z);
		check.X = CLOSEST_EDGE(pos_camera, block_bounds, X);
		return true;
	} else if (x_inside && z_inside) {
		check = v3pos_t(pos_camera.X, 0, pos_camera.Z);
		check.Y = CLOSEST_EDGE(pos_camera, block_bounds, Y);
		return true;
	}

	// diagonal
	if (x_inside) {
		check = v3pos_t(pos_camera.X, 0, 0);
		check.Y = CLOSEST_EDGE(pos_camera, block_bounds, Y);
		check.Z = CLOSEST_EDGE(pos_camera, block_bounds, Z);
		return true;
	} else if (y_inside) {
		check = v3pos_t(0, pos_camera.Y, 0);
		check.X = CLOSEST_EDGE(pos_camera, block_bounds, X);
		check.Z = CLOSEST_EDGE(pos_camera, block_bounds, Z);
		return true;
	} else if (z_inside) {
		check = v3pos_t(0, 0, pos_camera.Z);
		check.X = CLOSEST_EDGE(pos_camera, block_bounds, X);
		check.Y = CLOSEST_EDGE(pos_camera, block_bounds, Y);
		return true;
	}

	// Closest node would be a corner, none returned
	return false;
}

bool Map::isOccluded(const v3pos_t &pos_camera, const v3pos_t &pos_target,
	float step, float stepfac, float offset, float end_offset, u32 needed_count)
{
	auto direction = intToFloat(pos_target - pos_camera, BS);
	float distance = direction.getLength();

	// Normalize direction vector
	if (distance > 0.0f)
		direction /= distance;

	auto pos_origin_f = intToFloat(pos_camera, BS);
	u32 count = 0;
	bool is_valid_position;

	for (; offset < distance + end_offset; offset += step) {
		auto pos_node_f = pos_origin_f + direction * offset;
		v3pos_t pos_node = floatToInt(pos_node_f, BS);

		MapNode node = getNode(pos_node, &is_valid_position);

		if (is_valid_position &&
				!m_nodedef->getLightingFlags(node).light_propagates) {
			// Cannot see through light-blocking nodes --> occluded
			count++;
			if (count >= needed_count)
				return true;
		}
		step *= stepfac;
	}
	return false;
}

bool Map::isBlockOccluded(MapBlock *block, v3pos_t cam_pos_nodes)
{
	// Check occlusion for center and all 8 corners of the mapblock
	// Overshoot a little for less flickering
	static const s16 bs2 = MAP_BLOCKSIZE / 2 + 1;
	static const v3pos_t dir9[9] = {
		v3pos_t( 0,  0,  0),
		v3pos_t( 1,  1,  1) * bs2,
		v3pos_t( 1,  1, -1) * bs2,
		v3pos_t( 1, -1,  1) * bs2,
		v3pos_t( 1, -1, -1) * bs2,
		v3pos_t(-1,  1,  1) * bs2,
		v3pos_t(-1,  1, -1) * bs2,
		v3pos_t(-1, -1,  1) * bs2,
		v3pos_t(-1, -1, -1) * bs2,
	};

	v3pos_t pos_blockcenter = block->getPosRelative() + (MAP_BLOCKSIZE / 2);

	// Starting step size, value between 1m and sqrt(3)m
	float step = BS * 1.2f;
	// Multiply step by each iteraction by 'stepfac' to reduce checks in distance
	float stepfac = 1.05f;

	float start_offset = BS * 1.0f;

	// The occlusion search of 'isOccluded()' must stop short of the target
	// point by distance 'end_offset' to not enter the target mapblock.
	// For the 8 mapblock corners 'end_offset' must therefore be the maximum
	// diagonal of a mapblock, because we must consider all view angles.
	// sqrt(1^2 + 1^2 + 1^2) = 1.732
	float end_offset = -BS * MAP_BLOCKSIZE * 1.732f;

	// to reduce the likelihood of falsely occluded blocks
	// require at least two solid blocks
	// this is a HACK, we should think of a more precise algorithm
	u32 needed_count = 2;

	// Additional occlusion check, see comments in that function
	v3pos_t check;
	if (determineAdditionalOcclusionCheck(cam_pos_nodes, block->getBox(), check)) {
		// node is always on a side facing the camera, end_offset can be lower
		if (!isOccluded(cam_pos_nodes, check, step, stepfac, start_offset,
				-1.0f, needed_count))
			return false;
	}

	for (const v3pos_t &dir : dir9) {
		if (!isOccluded(cam_pos_nodes, pos_blockcenter + dir, step, stepfac,
				start_offset, end_offset, needed_count))
			return false;
	}
	return true;
}

/*
	ServerMap
*/
ServerMap::ServerMap(const std::string &savedir, IGameDef *gamedef,
		EmergeManager *emerge, MetricsBackend *mb):
	Map(gamedef),
	settings_mgr(savedir + DIR_DELIM + "map_meta"),
	m_emerge(emerge)
{
	verbosestream<<FUNCTION_NAME<<std::endl;

	// Tell the EmergeManager about our MapSettingsManager
	emerge->map_settings_mgr = &settings_mgr;

	/*
		Try to load map; if not found, create a new one.
	*/

	// Determine which database backend to use
	std::string conf_path = savedir + DIR_DELIM + "world.mt";
	Settings conf;
	bool succeeded = conf.readConfigFile(conf_path.c_str());
	if (!succeeded || !conf.exists("backend")) {
		// fall back to sqlite3
		#if USE_LEVELDB
		conf.set("backend", "leveldb");
		#elif USE_SQLITE3
		conf.set("backend", "sqlite3");
		#elif USE_POS32 && USE_LEVELDB
		conf.set("backend", "leveldb");
		#endif
	}
	std::string backend = conf.get("backend");
	dbase = createDatabase(backend, savedir, conf);

	if (conf.exists("readonly_backend")) {
		std::string readonly_dir = savedir + DIR_DELIM + "readonly";
		dbase_ro = createDatabase(conf.get("readonly_backend"), readonly_dir, conf);
	}
	if (!conf.updateConfigFile(conf_path.c_str()))
		errorstream << "ServerMap::ServerMap(): Failed to update world.mt!" << std::endl;

	m_savedir = savedir;
	m_map_saving_enabled = false;
	m_map_loading_enabled = true;

	m_save_time_counter = mb->addCounter(
		"minetest_map_save_time", "Time spent saving blocks (in microseconds)");
	m_save_count_counter = mb->addCounter(
		"minetest_map_saved_blocks", "Number of blocks saved");
	m_loaded_blocks_gauge = mb->addGauge(
		"minetest_map_loaded_blocks", "Number of loaded blocks");

	m_map_compression_level = rangelim(g_settings->getS16("map_compression_level_disk"), -1, 9);

	try {
		// If directory exists, check contents and load if possible
		if (fs::PathExists(m_savedir)) {
			// If directory is empty, it is safe to save into it.
			if (fs::GetDirListing(m_savedir).empty()) {
				infostream<<"ServerMap: Empty save directory is valid."
						<<std::endl;
				m_map_saving_enabled = true;
			}
			else
			{

				if (settings_mgr.loadMapMeta()) {
					infostream << "ServerMap: Metadata loaded from "
						<< savedir << std::endl;
				} else {
					infostream << "ServerMap: Metadata could not be loaded "
						"from " << savedir << ", assuming valid save "
						"directory." << std::endl;
				}

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
		warningstream<<"ServerMap: Failed to load map from "<<savedir
				<<", exception: "<<e.what()<<std::endl;
		infostream<<"Please remove the map or fix it."<<std::endl;
		warningstream<<"Map saving will be disabled."<<std::endl;
	}
}

ServerMap::~ServerMap()
{
	verbosestream<<FUNCTION_NAME<<std::endl;

	try
	{
/*
		if (m_map_saving_enabled) {
			// Save only changed parts
*/			
			save(MOD_STATE_WRITE_AT_UNLOAD);
/*
			infostream << "ServerMap: Saved map to " << m_savedir << std::endl;
		} else {
			infostream << "ServerMap: Map not saved" << std::endl;
		}
*/
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
	delete dbase_ro;

	deleteDetachedBlocks();
}

MapgenParams *ServerMap::getMapgenParams()
{
	// getMapgenParams() should only ever be called after Server is initialized
	assert(settings_mgr.mapgen_params != NULL);
	return settings_mgr.mapgen_params;
}

u64 ServerMap::getSeed()
{
	return getMapgenParams()->seed;
}

bool ServerMap::blockpos_over_mapgen_limit(v3bpos_t p)
{
	const bpos_t mapgen_limit_bp = rangelim(
		getMapgenParams()->mapgen_limit, 0, MAX_MAP_GENERATION_LIMIT) /
		MAP_BLOCKSIZE;
	return p.X < -mapgen_limit_bp ||
		p.X >  mapgen_limit_bp ||
		p.Y < -mapgen_limit_bp ||
		p.Y >  mapgen_limit_bp ||
		p.Z < -mapgen_limit_bp ||
		p.Z >  mapgen_limit_bp;
}

bool ServerMap::initBlockMake(v3bpos_t blockpos, BlockMakeData *data)
{
	s16 csize = getMapgenParams()->chunksize;
	v3bpos_t bpmin = EmergeManager::getContainingChunk(blockpos, csize);
	v3bpos_t bpmax = bpmin + v3bpos_t(1, 1, 1) * (csize - 1);

	if (!m_chunks_in_progress.insert(bpmin).second)
		return false;

	bool enable_mapgen_debug_info = m_emerge->enable_mapgen_debug_info;
	EMERGE_DBG_OUT("initBlockMake(): " << bpmin << " - " << bpmax);

	{
		auto lock = m_mapgen_process.lock_unique_rec();
		auto gen = m_mapgen_process.get(bpmin);
		auto now = porting::getTimeMs();
		if (gen > now - 60000 ) {
			//verbosestream << " already generating" << blockpos_min << " for " << blockpos << " gentime=" << now - gen << std::endl;
			return false;
		}
		m_mapgen_process.insert_or_assign(bpmin, now);
	}

	v3bpos_t extra_borders(1, 1, 1);
	v3bpos_t full_bpmin = bpmin - extra_borders;
	v3bpos_t full_bpmax = bpmax + extra_borders;

	// Do nothing if not inside mapgen limits (+-1 because of neighbors)
	if (blockpos_over_mapgen_limit(full_bpmin) ||
			blockpos_over_mapgen_limit(full_bpmax))
		return false;

	data->seed = getSeed();
	data->blockpos_min = bpmin;
	data->blockpos_max = bpmax;
	data->nodedef = m_nodedef;

	/*
		Create the whole area of this and the neighboring blocks
	*/
	{
		//TimeTaker timer("initBlockMake() create area");

	for (bpos_t x = full_bpmin.X; x <= full_bpmax.X; x++)
	for (bpos_t z = full_bpmin.Z; z <= full_bpmax.Z; z++) {
/*
		v2bpos_t sectorpos(x, z);
		// Sector metadata is loaded from disk if not already loaded.
		MapSector *sector = createSector(sectorpos);
		FATAL_ERROR_IF(sector == NULL, "createSector() failed");
*/
		for (bpos_t y = full_bpmin.Y; y <= full_bpmax.Y; y++) {
			v3bpos_t p(x, y, z);

			MapBlock *block = emergeBlock(p, false);
			if (block == NULL) {
				block = createBlock(p);

				// Block gets sunlight if this is true.
				// Refer to the map generator heuristics.
				bool ug = m_emerge->isBlockUnderground(p);
				block->setIsUnderground(ug);
			}
		}
	}
	}

	/*
		Now we have a big empty area.

		Make a ManualMapVoxelManipulator that contains this and the
		neighboring blocks
	*/

	data->vmanip = new MMVManip(this);
	data->vmanip->initialEmerge(full_bpmin, full_bpmax);

	// Data is ready now.
	return true;
}

void ServerMap::finishBlockMake(BlockMakeData *data,
	std::map<v3bpos_t, MapBlock*> *changed_blocks)
{
	v3bpos_t bpmin = data->blockpos_min;
	v3bpos_t bpmax = data->blockpos_max;

	bool enable_mapgen_debug_info = m_emerge->enable_mapgen_debug_info;
	EMERGE_DBG_OUT("finishBlockMake(): " << bpmin << " - " << bpmax);

	static const thread_local auto save_generated_block = g_settings->getBool("save_generated_block");

	/*
		Blit generated stuff to map
		NOTE: blitBackAll adds nearly everything to changed_blocks
	*/
	{
		MAP_NOTHREAD_LOCK(this);
		// 70ms @cs=8
		//TimeTaker timer("finishBlockMake() blitBackAll");
	data->vmanip->blitBackAll(changed_blocks, false, save_generated_block);
	}

	EMERGE_DBG_OUT("finishBlockMake: changed_blocks.size()="
		<< changed_blocks->size());

	/*
		Copy transforming liquid information
	*/


	while (data->transforming_liquid.size()) {
		transforming_liquid_add(data->transforming_liquid.front());
		data->transforming_liquid.pop_front();
	}

	for (auto &changed_block : *changed_blocks) {
		MapBlock *block = changed_block.second;
		if (!block)
			continue;
		/*
			Update day/night difference cache of the MapBlocks
		*/
		block->expireDayNightDiff();
		/*
			Set block as modified
		*/

		if (save_generated_block)
		block->raiseModified(MOD_STATE_WRITE_NEEDED,
			MOD_REASON_EXPIRE_DAYNIGHTDIFF);
		else
			block->setLightingComplete(0);
	}

	/*
		Set central blocks as generated
		Update weather data in blocks
	*/
	ServerEnvironment *senv = &((Server *)m_gamedef)->getEnv();
	for (bpos_t x = bpmin.X; x <= bpmax.X; x++)
	for (bpos_t z = bpmin.Z; z <= bpmax.Z; z++)
	for (bpos_t y = bpmin.Y; y <= bpmax.Y; y++) {
		v3bpos_t p(x, y, z);
		MapBlock *block = getBlockNoCreateNoEx(p, false, true);
		if (!block)
			continue;

		block->setGenerated(true);
		updateBlockHeat(senv, getBlockPosRelative(p), block);
		updateBlockHumidity(senv, getBlockPosRelative(p), block);
	}

	/*
		Save changed parts of map
		NOTE: Will be saved later.
	*/
	//save(MOD_STATE_WRITE_AT_UNLOAD);
	m_chunks_in_progress.erase(bpmin);

    //fmtodo merge with m_chunks_in_progress
	m_mapgen_process.erase(bpmin);
}

#if WTF
MapSector *ServerMap::createSector(v2bpos_t p2d)
{
	/*
		Check if it exists already in memory
	*/
	MapSector *sector = getSectorNoGenerate(p2d);
	if (sector)
		return sector;

	/*
		Do not create over max mapgen limit
	*/
	if (blockpos_over_max_limit(v3bpos_t(p2d.X, 0, p2d.Y)))
		throw InvalidPositionException("createSector(): pos. over max mapgen limit");

	MapBlock *block = this->getBlockNoCreateNoEx(p, false, true);
	if(block)
	{
		if(block->isDummy())
			block->unDummify();
		return block;
	}
	// Create blank
	block = this->createBlankBlock(p);

	sector = new MapSector(this, p2d, m_gamedef);

	/*
		Insert to container
	*/
	m_sectors[p2d] = sector;

	return sector;
}

MapBlock * ServerMap::createBlock(v3bpos_t p)
{
	/*
		Do not create over max mapgen limit
	*/
	if (blockpos_over_max_limit(p))
		throw InvalidPositionException("createBlock(): pos. over max mapgen limit");

	v2bpos_t p2d(p.X, p.Z);
	bpos_t block_y = p.Y;
	/*
		This will create or load a sector if not found in memory.
		If block exists on disk, it will be loaded.

		NOTE: On old save formats, this will be slow, as it generates
		      lighting on blocks for them.
	*/
	MapSector *sector;
	try {
		sector = createSector(p2d);
	} catch (InvalidPositionException &e) {
		infostream<<"createBlock: createSector() failed"<<std::endl;
		throw e;
	}

	/*
		Try to get a block from the sector
	*/

	MapBlock *block = sector->getBlockNoCreateNoEx(block_y);
	if (block) {
		return block;
	}
	// Create blank
	block = sector->createBlankBlock(block_y);

	return block;
}


#endif // WTF

MapBlock * ServerMap::emergeBlock(v3bpos_t p, bool create_blank)
{
	TimeTaker timer("generateBlock");
	MAP_NOTHREAD_LOCK(this);

	{
		MapBlock *block = getBlockNoCreateNoEx(p, false, true);
		if (block)
			return block;
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
/*
		MapSector *sector = createSector(v2bpos_t(p.X, p.Z));
		MapBlock *block = sector->createBlankBlock(p.Y);

		return block;
*/
	}

	return NULL;
}

MapBlock *ServerMap::getBlockOrEmerge(v3bpos_t p3d)
{
	MapBlock *block = getBlockNoCreateNoEx(p3d, false, true);
	if (block == NULL && m_map_loading_enabled)
		m_emerge->enqueueBlockEmerge(PEER_ID_INEXISTENT, p3d, false);

	return block;
}

void ServerMap::prepareBlock(MapBlock *block) {
	ServerEnvironment *senv = &((Server *)m_gamedef)->getEnv();

	// Calculate weather conditions
	//block->heat_last_update     = 0;
	//block->humidity_last_update = 0;
	v3pos_t p = getBlockPosRelative(block->getPos());
	updateBlockHeat(senv, p, block);
	updateBlockHumidity(senv, p, block);
}

bool ServerMap::isBlockInQueue(v3bpos_t pos)
{
	return m_emerge && m_emerge->isBlockInQueue(pos);
}

void ServerMap::addNodeAndUpdate(v3pos_t p, MapNode n,
		std::map<v3bpos_t, MapBlock*> &modified_blocks,
		bool remove_metadata,
		int fast, bool important)
{
	Map::addNodeAndUpdate(p, n, modified_blocks, remove_metadata, fast, important);

	/*
		Add neighboring liquid nodes and this node to transform queue.
		(it's vital for the node itself to get updated last, if it was removed.)
	 */
    if (!fast)
	for (const v3pos_t &dir : g_7dirs) {
		v3pos_t p2 = p + dir;

		bool is_valid_position;
		MapNode n2 = getNode(p2, &is_valid_position);
		if(is_valid_position &&
				(m_nodedef->get(n2).isLiquid() ||
				n2.getContent() == CONTENT_AIR))
			transforming_liquid_add(p2);
	}
}

// N.B.  This requires no synchronization, since data will not be modified unless
// the VoxelManipulator being updated belongs to the same thread.
void ServerMap::updateVManip(v3pos_t pos)
{
	Mapgen *mg = m_emerge->getCurrentMapgen();
	if (!mg)
		return;

	MMVManip *vm = mg->vm;
	if (!vm)
		return;

	if (!vm->m_area.contains(pos))
		return;

	s32 idx = vm->m_area.index(pos);
	vm->m_data[idx] = getNode(pos);
	vm->m_flags[idx] &= ~VOXELFLAG_NO_DATA;

	vm->m_is_dirty = true;
}

void ServerMap::reportMetrics(u64 save_time_us, u32 saved_blocks, u32 all_blocks)
{
	m_loaded_blocks_gauge->set(all_blocks);
	m_save_time_counter->increment(save_time_us);
	m_save_count_counter->increment(saved_blocks);
}

s32 ServerMap::save(ModifiedState save_level, float dedicated_server_step, bool breakable)
{
	if (!m_map_saving_enabled) {
		warningstream<<"Not saving map, saving disabled."<<std::endl;
		return 0;
	}

	const auto start_time = porting::getTimeUs();

	if(save_level == MOD_STATE_CLEAN)
		infostream<<"ServerMap: Saving whole map, this can take time."
				<<std::endl;

	if (m_map_metadata_changed || save_level == MOD_STATE_CLEAN) {
		if (settings_mgr.saveMapMeta())
			m_map_metadata_changed = false;
	}

	// Profile modified reasons
	Profiler modprofiler;

	u32 block_count = 0;
	u32 block_count_all = 0; // Number of blocks in memory

	// Don't do anything with sqlite unless something is really saved
	bool save_started = false;
	u32 n = 0, calls = 0;
	const auto end_ms = porting::getTimeMs() + u32(1000 * dedicated_server_step);
	if (!breakable)
		m_blocks_save_last = 0;

	MAP_NOTHREAD_LOCK(this);

	{
		auto lock = breakable ? m_blocks.try_lock_shared_rec() : m_blocks.lock_shared_rec();
		if (!lock->owns_lock())
			return m_blocks_save_last;

		for(const auto &[pos, block] : m_blocks)
		{
			if (n++ < m_blocks_save_last)
				continue;
			else
				m_blocks_save_last = 0;
			++calls;

			if (!block)
				continue;

			block_count_all++;

			if(block->getModified() >= (u32)save_level) {
				// Lazy beginSave()
				if(!save_started) {
					beginSave();
					save_started = true;
				}

				//modprofiler.add(block->getModifiedReasonString(), 1);

				auto lock = breakable ? block->try_lock_unique_rec() : block->lock_unique_rec();
				if (!lock->owns_lock())
					continue;

				saveBlock(block.get());
				block_count++;
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
			||*/ block_count != 0) {
		infostream << "ServerMap: Written: "
				<< block_count << " blocks"
				<< ", " << block_count_all << " blocks in memory."

                << " Total=" << m_blocks.size() << ".";
				if (m_blocks_save_last)
					infostream<<" Break at "<< m_blocks_save_last;
				infostream

				<< std::endl;
		PrintInfo(infostream); // ServerMap/ClientMap:
		//infostream<<"Blocks modified by: "<<std::endl;
		modprofiler.print(infostream);
	}

	const auto end_time = porting::getTimeUs();

	m_save_time_counter->increment(end_time - start_time);

	reportMetrics(end_time - start_time, block_count, block_count_all);

	return m_blocks_save_last;
}

void ServerMap::listAllLoadableBlocks(std::vector<v3bpos_t> &dst)
{
	dbase->listAllLoadableBlocks(dst);
	if (dbase_ro)
		dbase_ro->listAllLoadableBlocks(dst);
}

void ServerMap::listAllLoadedBlocks(std::vector<v3bpos_t> &dst)
{

	auto lock = m_blocks.lock_shared_rec();
	for(const auto & i : m_blocks)
		dst.push_back(i.second->getPos());
/*
	for (auto &sector_it : m_sectors) {
		MapSector *sector = sector_it.second;

		MapBlockVect blocks;
		sector->getBlocks(blocks);

		for (MapBlock *block : blocks) {
			v3bpos_t p = block->getPos();
			dst.push_back(p);
		}
	}
*/
}

MapDatabase *ServerMap::createDatabase(
	const std::string &name,
	const std::string &savedir,
	Settings &conf)
{
	#if USE_SQLITE3
 	if (name == "sqlite3")
		return new MapDatabaseSQLite3(savedir);
	#endif
	if (name == "dummy")
		return new Database_Dummy();
	#if USE_LEVELDB
	if (name == "leveldb")
		return new Database_LevelDB(savedir);
	#endif
	#if USE_REDIS
	if (name == "redis")
		return new Database_Redis(conf);
	#endif
	#if USE_POSTGRESQL
	if (name == "postgresql") {
		std::string connect_string;
		conf.getNoEx("pgsql_connection", connect_string);
		return new MapDatabasePostgreSQL(connect_string);
	}
	#endif

	throw BaseException(std::string("Database backend ") + name + " not supported.");
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
	return saveBlock(block, dbase, m_map_compression_level);
}

bool ServerMap::saveBlock(MapBlock *block, MapDatabase *db, int compression_level)
{
	v3bpos_t p3d = block->getPos();

	if (!block->isGenerated()) {
		//warningstream << "saveBlock: Not writing not generated block p="<< p3d << std::endl;
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
	block->serialize(o, version, true, compression_level);

	bool ret = db->saveBlock(p3d, o.str());
	if (ret) {
		// We just wrote it to the disk so clear modified flag
		block->resetModified();
	}
	return ret;
}

MapBlock * ServerMap::loadBlock(v3bpos_t p3d)
{
	ScopeProfiler sp(g_profiler, "ServerMap::loadBlock");
	const auto sector = this;
	MapBlock *block = nullptr;
	try {
		std::string blob;
		dbase->loadBlock(p3d, &blob);
		if (!blob.length() && dbase_ro) {
			dbase_ro->loadBlock(p3d, &blob);
		}
		if (!blob.length()) {
			m_db_miss.emplace(p3d);
			return nullptr;
		}

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

		bool created_new = false;
		block = sector->getBlockNoCreateNoEx(p3d, false, true);
		if(block == NULL)
		{
			block = sector->createBlankBlockNoInsert(p3d);
			created_new = true;
		}

		// Read basic data
		if (!block->deSerialize(is, version, true)) {
			if (created_new && block)
				delete block;
			return nullptr;
		}

		// If it's a new block, insert it to the map
		if(created_new)
			if(!sector->insertBlock(block)) {
				delete block;
				return nullptr;
			}

		if (!g_settings->getBool("liquid_real")) {
			ReflowScan scanner(this, m_emerge->ndef);
			scanner.scan(block, &m_transforming_liquid);
		}

		// We just loaded it from, so it's up-to-date.
		block->resetModified();

/*
		if (block->getLightingExpired()) {
			verbosestream<<"Loaded block with exiried lighting. (maybe sloooow appear), try recalc " << p3d<<std::endl;
			lighting_modified_blocks.set(p3d, nullptr);
		}
*/

	//MapBlock *block = getBlockNoCreateNoEx(blockpos);
	if (created_new && (block != NULL)) {
		std::map<v3bpos_t, MapBlock*> modified_blocks;
		// Fix lighting if necessary
		voxalgo::update_block_border_lighting(this, block, modified_blocks);
		if (!modified_blocks.empty()) {
			//Modified lighting, send event
			MapEditEvent event;
			event.type = MEET_OTHER;
			for (auto it = modified_blocks.begin();
					it != modified_blocks.end(); ++it)
				event.modified_blocks.push_back(it->first);
			dispatchEvent(event);
		}
	}



		return block;
	} catch (const std::exception &e) {
		if (block)
			delete block;

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
		}
	}
	return nullptr;
}

#if WTF

void ServerMap::loadBlock(std::string *blob, v3bpos_t p3d, MapSector *sector, bool save_after_load)
{
	try {
		std::istringstream is(*blob, std::ios_base::binary);

		u8 version = SER_FMT_VER_INVALID;
		is.read((char*)&version, 1);

		if(is.fail())
			throw SerializationError("ServerMap::loadBlock(): Failed"
					" to read MapBlock version");

		MapBlock *block = nullptr;
		std::unique_ptr<MapBlock> block_created_new;
		block = sector->getBlockNoCreateNoEx(p3d.Y);
		if (!block) {
			block_created_new = sector->createBlankBlockNoInsert(p3d.Y);
			block = block_created_new.get();
		}

		{
		ScopeProfiler sp(g_profiler, "ServerMap: deSer block", SPT_AVG);
		// Read basic data
		block->deSerialize(is, version, true);
		}

		// If it's a new block, insert it to the map
		if (block_created_new) {
			sector->insertBlock(std::move(block_created_new));
			ReflowScan scanner(this, m_emerge->ndef);
			scanner.scan(block, &m_transforming_liquid);
		}

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
		}
	}
}

MapBlock* ServerMap::loadBlock(v3bpos_t blockpos)
{
	ScopeProfiler sp(g_profiler, "ServerMap: load block", SPT_AVG);
	bool created_new = (getBlockNoCreateNoEx(blockpos) == NULL);

	v2bpos_t p2d(blockpos.X, blockpos.Z);

	std::string ret;
	dbase->loadBlock(blockpos, &ret);
	if (!ret.empty()) {
		loadBlock(&ret, blockpos, createSector(p2d), false);
	} else if (dbase_ro) {
		dbase_ro->loadBlock(blockpos, &ret);
		if (!ret.empty()) {
			loadBlock(&ret, blockpos, createSector(p2d), false);
		}
	} else {
		return NULL;
	}

	MapBlock *block = getBlockNoCreateNoEx(blockpos);
	if (created_new && (block != NULL)) {
		std::map<v3bpos_t, MapBlock*> modified_blocks;
		// Fix lighting if necessary
		voxalgo::update_block_border_lighting(this, block, modified_blocks);
		if (!modified_blocks.empty()) {
			//Modified lighting, send event
			MapEditEvent event;
			event.type = MEET_OTHER;
			event.setModifiedBlocks(modified_blocks);
			dispatchEvent(event);
		}
	}
	return block;
}
#endif

bool ServerMap::deleteBlock(v3bpos_t blockpos)
{
	if (!dbase->deleteBlock(blockpos))
		return false;

	MapBlock *block = getBlockNoCreateNoEx(blockpos);
	if (block) {
		Map::deleteBlock(blockpos);
/*
		v2bpos_t p2d(blockpos.X, blockpos.Z);
		MapSector *sector = getSectorNoGenerate(p2d);
		if (!sector)
			return false;
		// It may not be safe to delete the block from memory at the moment
		// (pointers to it could still be in use)
		sector->detachBlock(block);
		m_detached_blocks.push_back(sector->detachBlock(block));
*/
		m_detached_blocks.push_back(std::unique_ptr<MapBlock>{block});
	}

	return true;
}

void ServerMap::deleteDetachedBlocks()
{
	for (const auto &block : m_detached_blocks) {
		assert(block->isOrphan());
		(void)block; // silence unused-variable warning in release builds
	}

	m_detached_blocks.clear();
}

void ServerMap::step()
{
	// Delete from memory blocks removed by deleteBlocks() only when pointers
	// to them are (probably) no longer in use
	deleteDetachedBlocks();
}

void ServerMap::PrintInfo(std::ostream &out)
{
	out<<"ServerMap: ";
}

bool ServerMap::repairBlockLight(v3bpos_t blockpos,
	std::map<v3bpos_t, MapBlock *> *modified_blocks)
{
	MapBlock *block = emergeBlock(blockpos, false);
	if (!block || !block->isGenerated())
		return false;
	voxalgo::repair_block_light(this, block, modified_blocks);
	return true;
}

MMVManip::MMVManip(Map *map):
		VoxelManipulator(),
		m_map(map)
{
	assert(map);
}

void MMVManip::initialEmerge(v3bpos_t blockpos_min, v3bpos_t blockpos_max,
	bool load_if_inexistent)
{
	TimeTaker timer1("initialEmerge");

	assert(m_map);

	// Units of these are MapBlocks
	v3bpos_t p_min = blockpos_min;
	v3bpos_t p_max = blockpos_max;

	VoxelArea block_area_nodes
			(getBlockPosRelative(p_min), getBlockPosRelative(p_max+1)-v3pos_t(1,1,1));

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
		v3bpos_t p(x,y,z);
		std::map<v3bpos_t, u8>::iterator n;
		n = m_loaded_blocks.find(p);
		if(n != m_loaded_blocks.end())
			continue;

		bool block_data_inexistent = false;
		{
			TimeTaker timer2("emerge load");

			block = m_map->getBlockNoCreateNoEx(p, false, true);
			if (!block)
				block_data_inexistent = true;
			else
				block->copyTo(*this);
		}

		if(block_data_inexistent)
		{

			if (load_if_inexistent && !blockpos_over_max_limit(p)) {
				ServerMap *svrmap = (ServerMap *)m_map;
				block = svrmap->emergeBlock(p, false);
				if (block == NULL)
					block = svrmap->createBlock(p);
				block->copyTo(*this);
			} else {
				flags |= VMANIP_BLOCK_DATA_INEXIST;

				/*
					Mark area inexistent
				*/
				VoxelArea a(getBlockPosRelative(p), getBlockPosRelative(p+1)-v3pos_t(1,1,1));
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

void MMVManip::blitBackAll(std::map<v3bpos_t, MapBlock*> *modified_blocks,
	bool overwrite_generated, bool save_generated_block)
{
	if(m_area.getExtent() == v3pos_t(0,0,0))
		return;
	assert(m_map);

	/*
		Copy data of all blocks
	*/
	for (auto &loaded_block : m_loaded_blocks) {
		v3bpos_t p = loaded_block.first;
		MapBlock *block = m_map->getBlockNoCreateNoEx(p, false, true);
		bool existed = !(loaded_block.second & VMANIP_BLOCK_DATA_INEXIST);
		if (!existed || (block == NULL) ||
			(!overwrite_generated && block->isGenerated()))
			continue;

		block->copyFrom(*this);

   	  if (save_generated_block)
		block->raiseModified(MOD_STATE_WRITE_NEEDED, MOD_REASON_VMANIP);

		if(modified_blocks)
			(*modified_blocks)[p] = block;
	}
}

MMVManip *MMVManip::clone() const
{
	MMVManip *ret = new MMVManip();

	const s32 size = m_area.getVolume();
	ret->m_area = m_area;
	if (m_data) {
		ret->m_data = new MapNode[size];
		memcpy(ret->m_data, m_data, size * sizeof(MapNode));
	}
	if (m_flags) {
		ret->m_flags = new u8[size];
		memcpy(ret->m_flags, m_flags, size * sizeof(u8));
	}

	ret->m_is_dirty = m_is_dirty;
	// Even if the copy is disconnected from a map object keep the information
	// needed to write it back to one
	ret->m_loaded_blocks = m_loaded_blocks;

	return ret;
}

void MMVManip::reparent(Map *map)
{
	assert(map && !m_map);
	m_map = map;
}

//END
