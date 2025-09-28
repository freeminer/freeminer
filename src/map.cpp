// Luanti
// SPDX-License-Identifier: LGPL-2.1-or-later
// Copyright (C) 2010-2013 celeron55, Perttu Ahola <celeron55@gmail.com>

#include "map.h"
#include "irr_v3d.h"
#include "log.h"
#include "mapblock.h"
#if IS_CLIENT_BUILD
	#include "client/mapblock_mesh.h"
#endif
#include "voxel.h"
#include "voxelalgorithms.h"
#include "porting.h"
#include "nodemetadata.h"
#include "log.h"
#include "profiler.h"
#include "nodedef.h"
#include "gamedef.h"
#include "util/directiontables.h"
#include "rollback_interface.h"
#include "environment.h"
#include "irrlicht_changes/printing.h"

/*
	Map
*/

Map::Map(IGameDef *gamedef):
	m_gamedef(gamedef),
	m_nodedef(gamedef->ndef())
{
	getBlockCacheFlush();
}

Map::~Map()
{
	const auto lock = m_blocks.lock_unique_rec();
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
MapSector * Map::getSectorNoGenerateNoLock(v2s16 p)
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

static void set_node_in_block(const NodeDefManager *nodedef, MapBlock *block,
		v3pos_t relpos, MapNode n, bool important = false)
{
	// Never allow placing CONTENT_IGNORE, it causes problems
	if(n.getContent() == CONTENT_IGNORE){
		auto blockpos = block->getPos();
		v3pos_t p = blockpos * MAP_BLOCKSIZE + relpos;
		errorstream<<"Not allowing to place CONTENT_IGNORE"
				<<" while trying to replace \""
				<<nodedef->get(block->getNodeNoCheck(relpos)).name
				<<"\" at "<<p<<" (block "<<blockpos<<")"<<std::endl;
		return;
	}
	block->setNodeNoCheck(relpos, n, important);
}

// throws InvalidPositionException if not found
void Map::setNode(const v3pos_t &p, const MapNode &n, bool important)
{
	v3bpos_t blockpos = getNodeBlockPos(p);
	MapBlock *block = getBlockNoCreate(blockpos);
	v3pos_t relpos = p - blockpos*MAP_BLOCKSIZE;
	set_node_in_block(m_gamedef->ndef(), block, relpos, n, important);
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
	v3pos_t relpos = p - blockpos * MAP_BLOCKSIZE;

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
		set_node_in_block(m_gamedef->ndef(), block, relpos, n, important);

		modified_blocks[blockpos] = block;
	} else {
		// Ignore light (because calling voxalgo::update_lighting_nodes)
		n.setLight(LIGHTBANK_DAY, 0, f);
		n.setLight(LIGHTBANK_NIGHT, 0, f);
		set_node_in_block(m_gamedef->ndef(), block, relpos, n, important);

		// Update lighting
		std::vector<std::pair<v3pos_t, MapNode> > oldnodes;
		oldnodes.emplace_back(p, oldnode);
		voxalgo::update_lighting_nodes(this, oldnodes, modified_blocks);
	}

	if (n.getContent() != oldnode.getContent() &&
			(oldnode.getContent() == CONTENT_AIR || n.getContent() == CONTENT_AIR))
		block->expireIsAirCache();

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
	//MapSector *sect;
	MapBlock *block;

	TimeOrderedMapBlock(/*MapSector *sect,*/ MapBlock *block) :
		//sect(sect),
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
		MapBlockVect blocks;
		for (auto &sector_it : m_sectors) {
			MapSector *sector = sector_it.second;

			bool all_blocks_deleted = true;

			blocks.clear();
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
		MapBlockVect blocks;
		for (auto &sector_it : m_sectors) {
			MapSector *sector = sector_it.second;

			blocks.clear();
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
	timerUpdate(0, -1, 100, unloaded_blocks);
}

#if WTF
void Map::deleteSectors(const std::vector<v2s16> &sectorList)
{
	for (v2bpos_t j : sectorList) {
		MapSector *sector = m_sectors[j];
		// If sector is in sector cache, remove it from there
		if (m_sector_cache == sector)
			m_sector_cache = nullptr;
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
	v3pos_t p_rel = p - blockpos*MAP_BLOCKSIZE;
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
	v3pos_t p_rel = p - blockpos*MAP_BLOCKSIZE;
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
	v3pos_t p_rel = p - blockpos*MAP_BLOCKSIZE;
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
	v3pos_t p_rel = p - blockpos*MAP_BLOCKSIZE;
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
	v3pos_t p_rel = p - blockpos*MAP_BLOCKSIZE;
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

bool Map::determineAdditionalOcclusionCheck(const v3pos_t pos_camera,
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

bool Map::isOccluded(const v3pos_t pos_camera, const v3pos_t pos_target,
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

bool Map::isBlockOccluded(v3pos_t pos_relative, v3pos_t cam_pos_nodes, bool simple_check)
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

	v3pos_t pos_blockcenter = pos_relative + (MAP_BLOCKSIZE / 2);

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

	// This should be only used in server occlusion cullung.
	// The client recalculates the complete drawlist periodically,
	// and random sampling could lead to visible flicker.
	if (simple_check) {
		v3pos_t random_point(myrand_range(-bs2, bs2), myrand_range(-bs2, bs2), myrand_range(-bs2, bs2));
		return isOccluded(cam_pos_nodes, pos_blockcenter + random_point, step, stepfac,
					start_offset, end_offset, 1);
	}

	// Additional occlusion check, see comments in that function
	v3pos_t check;
	if (determineAdditionalOcclusionCheck(cam_pos_nodes, MapBlock::getBox(pos_relative), check)) {
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

MMVManip::MMVManip(Map *map):
		VoxelManipulator(),
		m_map(map)
{
	assert(map);
}

void MMVManip::initialEmerge(v3bpos_t p_min, v3bpos_t p_max, bool load_if_inexistent)
{
	TimeTaker timer1("initialEmerge");

	assert(m_map);

	VoxelArea block_area_nodes
			(getBlockPosRelative(p_min), getBlockPosRelative(p_max+1)-v3pos_t(1,1,1));

	u32 size_MB = block_area_nodes.getVolume() * sizeof(MapNode) / 1000000U;
	if(size_MB >= 1)
	{
		infostream<<"initialEmerge: area: ";
		block_area_nodes.print(infostream);
		infostream<<" ("<<size_MB<<"MB)";
		infostream<<" load_if_inexistent="<<load_if_inexistent;
		infostream<<std::endl;
	}

	std::map<v3bpos_t, bool> had_blocks;
	// we can skip this calculation if the areas are disjoint
	if (!m_area.intersect(block_area_nodes).hasEmptyExtent())
		had_blocks = getCoveredBlocks();

	const bool all_new = m_area.hasEmptyExtent();
	addArea(block_area_nodes);

	for(s32 z=p_min.Z; z<=p_max.Z; z++)
	for(s32 y=p_min.Y; y<=p_max.Y; y++)
	for(s32 x=p_min.X; x<=p_max.X; x++)
	{
		v3bpos_t p(x,y,z);
		// if this block was already in the vmanip and it has data, skip
		if (auto it = had_blocks.find(p); it != had_blocks.end() && it->second)
			continue;

		auto block = m_map->getBlock(p, false, true);
		if (block) {
			block->copyTo(*this);
		} else {
			if (load_if_inexistent && !blockpos_over_max_limit(p)) {
				block = m_map->emergeBlockPtr(p, true);
				assert(block);
				block->copyTo(*this);
			} else {
				// Mark area inexistent
				VoxelArea a(p*MAP_BLOCKSIZE, (p+1)*MAP_BLOCKSIZE-v3pos_t(1,1,1));
				setFlags(a, VOXELFLAG_NO_DATA);
			}
		}
	}

	if (all_new)
		m_is_dirty = false;
}

std::map<v3bpos_t, bool> MMVManip::getCoveredBlocks() const
{
	std::map<v3bpos_t, bool> ret;
	if (m_area.hasEmptyExtent())
		return ret;

	// Figure out if *any* node in this block has data according to m_flags
	const auto &check_block = [this] (v3bpos_t bp) -> bool {
		v3pos_t pmin = bp * MAP_BLOCKSIZE;
		v3pos_t pmax = pmin + v3bpos_t(MAP_BLOCKSIZE-1);
		for(auto z=pmin.Z; z<=pmax.Z; z++)
		for(auto y=pmin.Y; y<=pmax.Y; y++)
		for(auto x=pmin.X; x<=pmax.X; x++) {
			if (!(m_flags[m_area.index(x,y,z)] & VOXELFLAG_NO_DATA))
				return true;
		}
		return false;
	};

	auto bpmin = getNodeBlockPos(m_area.MinEdge);
	auto bpmax = getNodeBlockPos(m_area.MaxEdge);

	if (bpmin * MAP_BLOCKSIZE != m_area.MinEdge)
		throw BaseException("MMVManip not block-aligned");
	if ((bpmax+1) * MAP_BLOCKSIZE - v3bpos_t(1) != m_area.MaxEdge)
		throw BaseException("MMVManip not block-aligned");

	for(auto z=bpmin.Z; z<=bpmax.Z; z++)
	for(auto y=bpmin.Y; y<=bpmax.Y; y++)
	for(auto x=bpmin.X; x<=bpmax.X; x++) {
		v3bpos_t bp(x,y,z);
		ret[bp] = check_block(bp);
	}
	return ret;
}

void MMVManip::blitBackAll(std::map<v3bpos_t, MapBlock*> *modified_blocks,
	bool overwrite_generated, bool save_generated_block) const
{
	if (m_area.hasEmptyExtent())
		return;
	assert(m_map);

	size_t nload = 0;

	// Copy all the blocks with data back to the map
	const auto loaded_blocks = getCoveredBlocks();
	for (auto &it : loaded_blocks) {
		if (!it.second)
			continue;
		auto p = it.first;
		auto block = m_map->getBlock(p, false, true);
		if (!block) {
			if (!blockpos_over_max_limit(p)) {
				block = m_map->emergeBlockPtr(p, true);
				nload++;
			}
		}
		if (!block) {
			warningstream << "blitBackAll: Couldn't load block " << p
				<< " to write data to map" << std::endl;
			continue;
		}
		if (!overwrite_generated && block->isGenerated())
			continue;

		block->copyFrom(*this);

   	  if (save_generated_block)
		block->raiseModified(MOD_STATE_WRITE_NEEDED, MOD_REASON_VMANIP, false);
		block->expireIsAirCache();

		if(modified_blocks)
			(*modified_blocks)[p] = block.get(); // should not be used by pointer
	}

	if (nload > 0) {
		verbosestream << "blitBackAll: " << nload << " blocks had to be loaded for writing" << std::endl;
	}
}

MMVManip *MMVManip::clone() const
{
	MMVManip *ret = new MMVManip();

	const u32 size = m_area.getVolume();
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

	return ret;
}

void MMVManip::reparent(Map *map)
{
	assert(map && !m_map);
	m_map = map;
}

//END
