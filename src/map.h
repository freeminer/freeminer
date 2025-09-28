// Luanti
// SPDX-License-Identifier: LGPL-2.1-or-later
// Copyright (C) 2010-2013 celeron55, Perttu Ahola <celeron55@gmail.com>

#pragma once

#include <cstdint>
#include <iostream>
#include <set>
#include <map>
#include "irr_v3d.h"
#include "threading/concurrent_set.h"
#include "threading/concurrent_unordered_map.h"
#include "threading/concurrent_unordered_set.h"
#include "util/unordered_map_hash.h"
#include <list>

#include "irr_v2d.h"
#include "irr_v3d.h"
#include "irrlichttypes.h"
#include "irrlichttypes_bloated.h"
#include "mapblock.h"
#include "mapnode.h"
#include "constants.h"
#include "voxel.h"
#include "modifiedstate.h"
#include "util/numeric.h"
#include "nodetimer.h"
#include "debug.h"

/*
class MapSector;
*/
class NodeMetadata;
class IGameDef;
class IRollbackManager;
class MapDatabase;

#if !ENABLE_THREADS
	#define MAP_NOTHREAD_LOCK(map) auto lock_map = map->m_nothread_locker.lock_unique_rec();
#else
	#define MAP_NOTHREAD_LOCK(map) ;
#endif



/*
	MapEditEvent
*/

enum MapEditEventType {
	// Node added (changed from air or something else to something)
	MEET_ADDNODE,
	// Node removed (changed to air)
	MEET_REMOVENODE,
	// Node swapped (changed without metadata change)
	MEET_SWAPNODE,
	// Node metadata changed
	MEET_BLOCK_NODE_METADATA_CHANGED,
	// Anything else (modified_blocks are set unsent)
	MEET_OTHER
};

struct MapEditEvent
{
	MapEditEventType type = MEET_OTHER;
	v3pos_t p;
	MapNode n = CONTENT_AIR;
	std::vector<v3bpos_t> modified_blocks; // Represents a set
	bool is_private_change = false;
	// Setting low_priority to true allows the server
	// to send this change to clients with some delay.
	bool low_priority = false;

	MapEditEvent() = default;

	// Sets the event's position and marks the block as modified.
	void setPositionModified(v3pos_t pos)
	{
		assert(modified_blocks.empty()); // only meant for initialization (once)
		p = pos;
		modified_blocks.push_back(getNodeBlockPos(pos));
	}

	void setModifiedBlocks(const std::map<v3bpos_t, MapBlock *>& blocks)
	{
		assert(modified_blocks.empty()); // only meant for initialization (once)
		modified_blocks.reserve(blocks.size());
		for (const auto &block : blocks)
			modified_blocks.push_back(block.first);
	}

	VoxelArea getArea() const
	{
		switch(type){
		case MEET_ADDNODE:
		case MEET_REMOVENODE:
		case MEET_SWAPNODE:
		case MEET_BLOCK_NODE_METADATA_CHANGED:
			return VoxelArea(p);
		case MEET_OTHER:
		{
			VoxelArea a;
			for (const auto &p : modified_blocks) {
				v3pos_t np1 = v3pos_t(p.X, p.Y, p.Z)*MAP_BLOCKSIZE;
				v3pos_t np2 = np1 + v3pos_t(MAP_BLOCKSIZE-1);
				a.addPoint(np1);
				a.addPoint(np2);
			}
			return a;
		}
		}
		return VoxelArea();
	}
};

class MapEventReceiver
{
public:
	// event shall be deleted by caller after the call.
	virtual void onMapEditEvent(const MapEditEvent &event) = 0;
};

class Map : public NodeContainer
{
public:
	Map(IGameDef *gamedef);
	virtual ~Map();
	DISABLE_CLASS_COPY(Map);

	void addEventReceiver(MapEventReceiver *event_receiver);
	void removeEventReceiver(MapEventReceiver *event_receiver);
	// event shall be deleted by caller after the call.
	void dispatchEvent(const MapEditEvent &event);

	// Returns InvalidPositionException if not found
	MapBlock * getBlockNoCreate(v3bpos_t p);
	// Returns NULL if not found
	MapBlock * getBlockNoCreateNoEx(v3bpos_t p, bool trylock = false, bool nocache = false);
	MapBlockPtr getBlock(v3bpos_t p, bool trylock = false, bool nocache = false);
	void getBlockCacheFlush();

	/* Server overrides */
	virtual MapBlock * emergeBlock(v3bpos_t p, bool create_blank=false)
	{ return getBlockNoCreateNoEx(p); }

	inline const NodeDefManager * getNodeDefManager() { return m_nodedef; }

	bool isValidPosition(v3pos_t p);

	// throws InvalidPositionException if not found
	void setNode(const v3pos_t &p, const MapNode &n, bool important = false) override;

	// Returns a CONTENT_IGNORE node if not found
	// If is_valid_position is not NULL then this will be set to true if the
	// position is valid, otherwise false
	MapNode getNode(v3pos_t p, bool *is_valid_position = NULL);

	/*
		These handle lighting but not faces.
	*/
	virtual void addNodeAndUpdate(v3pos_t p, MapNode n,
			std::map<v3bpos_t, MapBlock*> &modified_blocks,
			bool remove_metadata = true,
			int fast = 0, bool important = false
			);
	void removeNodeAndUpdate(v3pos_t p,
			std::map<v3bpos_t, MapBlock*> &modified_blocks, int fast = 0, bool important = false);

	/*
		Wrappers for the latter ones.
		These emit events.
		Return true if succeeded, false if not.
	*/
	bool addNodeWithEvent(v3pos_t p, MapNode n, bool remove_metadata = true, bool important = false);
	bool removeNodeWithEvent(v3pos_t p, int fast, bool important);

	// Call these before and after saving of many blocks
	virtual void beginSave() {}
	virtual void endSave() {}

	virtual s32 save(ModifiedState save_level, float dedicated_server_step, bool breakable){ FATAL_ERROR("FIXME"); return 0;};

	/*
		Return true unless the map definitely cannot save blocks.
	*/
	virtual bool maySaveBlocks() { return true; }

	// Server implements these.
	// Client leaves them as no-op.
	virtual bool saveBlock(MapBlock *block) { return false; }
	virtual bool deleteBlock(v3bpos_t blockpos) { return false; }

	/*
		Updates usage timers and unloads unused blocks and sectors.
		Saves modified blocks before unloading if possible.
	*/
	u32 timerUpdate(float uptime, float unload_timeout, s32 max_loaded_blocks,
			std::vector<v3bpos_t> *unloaded_blocks=NULL
			, unsigned int max_cycle_ms = 100
			);

	/*
		Unloads all blocks with a zero refCount().
		Saves modified blocks before unloading if possible.
	*/
	void unloadUnreferencedBlocks(std::vector<v3bpos_t> *unloaded_blocks=NULL);

	// Deletes sectors and their blocks from memory
	// Takes cache into account
	// If deleted sector is in sector cache, clears cache
/*
	void deleteSectors(const std::vector<v2s16> &list);
*/

	// For debug printing. Prints "Map: ", "ServerMap: " or "ClientMap: "
	virtual void PrintInfo(std::ostream &out);

	/*
		Node metadata
		These are basically coordinate wrappers to MapBlock
	*/

	std::vector<v3pos_t> findNodesWithMetadata(v3pos_t p1, v3pos_t p2);
	NodeMetadata *getNodeMetadata(v3pos_t p);

	/**
	 * Sets metadata for a node.
	 * This method sets the metadata for a given node.
	 * On success, it returns @c true and the object pointed to
	 * by @p meta is then managed by the system and should
	 * not be deleted by the caller.
	 *
	 * In case of failure, the method returns @c false and the
	 * caller is still responsible for deleting the object!
	 *
	 * @param p node coordinates
	 * @param meta pointer to @c NodeMetadata object
	 * @return @c true on success, false on failure
	 */
	bool setNodeMetadata(v3pos_t p, NodeMetadata *meta);
	void removeNodeMetadata(v3pos_t p);

	/*
		Node Timers
		These are basically coordinate wrappers to MapBlock
	*/

	NodeTimer getNodeTimer(v3pos_t p);
	void setNodeTimer(const NodeTimer &t);
	void removeNodeTimer(v3pos_t p);

	/*
		Utilities
	*/

	// freeminer:
	MapNode getNodeTry(const v3pos_t &p);
	//MapNode getNodeNoLock(v3s16 p); // dont use

	std::atomic_size_t m_liquid_step_flow{1000};
	std::atomic_size_t m_transforming_liquid_local_size{0};

	virtual s16 getHeat(const v3pos_t &p, bool no_random = 0);
	virtual s16 getHumidity(const v3pos_t &p, bool no_random = 0);

	// from old mapsector:
	using m_blocks_type =
			concurrent_unordered_map<v3bpos_t, MapBlockPtr, v3posHash, v3posEqual>;
	m_blocks_type m_blocks;
	using m_far_blocks_type =
			concurrent_shared_unordered_map<v3bpos_t, MapBlockPtr, v3posHash, v3posEqual>;
	m_far_blocks_type m_far_blocks;
	std::vector<std::shared_ptr<MapBlock>> m_far_blocks_delete;
	bool m_far_blocks_currrent {};
	//using far_blocks_ask_t = concurrent_shared_unordered_map<v3bpos_t, block_step_t>;
	using far_blocks_req_t = std::unordered_map<v3bpos_t,
			std::pair<block_step_t, uint32_t>>; // server
	using far_blocks_ask_t = concurrent_shared_unordered_map<v3bpos_t,
			std::pair<block_step_t, uint32_t>>; // client
	far_blocks_ask_t m_far_blocks_ask;
	struct BlockUsed
	{
		MapBlockPtr block{};
		int32_t last_used{};
	};
	std::array<concurrent_unordered_map<v3bpos_t, BlockUsed>, FARMESH_STEP_MAX> far_blocks_storage;
	//double m_far_blocks_created = 0;
	float far_blocks_sent_timer{1};
	v3pos_t far_blocks_last_cam_pos;
	std::vector<MapBlockPtr> m_far_blocks_delete_1, m_far_blocks_delete_2;
	bool m_far_blocks_delete_current {};

	//static constexpr bool m_far_fast =			true; // show generated far farmesh stable(0) or instant(1)
	uint32_t far_iteration_use{};
	uint32_t far_iteration_clean{};
	// MapBlock * getBlockNoCreateNoEx(v3pos_t & p);
	MapBlockPtr createBlankBlockNoInsert(const v3bpos_t &p);
	MapBlockPtr createBlankBlock(const v3bpos_t &p);
	bool insertBlock(MapBlockPtr block);
	void eraseBlock(const MapBlockPtr block);
	std::unordered_map<MapBlockPtr, int> *m_blocks_delete = nullptr;
	std::unordered_map<MapBlockPtr, int> m_blocks_delete_1, m_blocks_delete_2;
	uint64_t m_blocks_delete_time{};
	// void getBlocks(std::list<MapBlock*> &dest);
	concurrent_shared_unordered_set<v3bpos_t, v3posHash, v3posEqual> m_db_miss;
	MapNode &getNodeRef(const v3pos_t &p);

#if !ENABLE_THREADS
	locker<> m_nothread_locker;
#endif
#if ENABLE_THREADS && !HAVE_THREAD_LOCAL
	try_shared_mutex m_block_cache_mutex;
#endif
#if !HAVE_THREAD_LOCAL
	MapBlockPtr m_block_cache;
	v3pos_t m_block_cache_p;
#endif
	void copy_27_blocks_to_vm(MapBlock *block, VoxelManipulator &vmanip);

protected:
	u32 m_blocks_update_last{};
	u32 m_blocks_save_last{};

public:
	inline MapNode getNodeNoEx(const v3pos_t &p) override { return getNodeTry(p); };
	inline MapNode getNodeNoExNoEmerge(const v3pos_t &p) override
	{
		return getNodeTry(p);
	};
	inline MapNode &getNodeRefUnsafe(const v3pos_t &p) override { return getNodeRef(p); }

	// bool isBlockOccluded(const v3pos_t &pos, const v3pos_t &cam_pos_nodes);

	concurrent_unordered_set<v3bpos_t> changed_blocks_for_merge;
	using far_dbases_t = std::array<std::shared_ptr<MapDatabase>, FARMESH_STEP_MAX>;

	virtual MapBlockPtr emergeBlockPtr(v3bpos_t p, bool create_blank=false)
	{ return getBlock(p); }

	//end of freeminer




public:
	// Iterates through all nodes in the area in an unspecified order.
	// The given callback takes the position as its first argument and the node
	// as its second. If it returns false, forEachNodeInArea returns early.
	template<typename F>
	void forEachNodeInArea(v3pos_t minp, v3pos_t maxp, F func)
	{
		v3bpos_t bpmin = getNodeBlockPos(minp);
		v3bpos_t bpmax = getNodeBlockPos(maxp);
		for (auto bz = bpmin.Z; bz <= bpmax.Z; bz++)
		for (auto bx = bpmin.X; bx <= bpmax.X; bx++)
		for (auto by = bpmin.Y; by <= bpmax.Y; by++) {
			// y is iterated innermost to make use of the sector cache.
			v3bpos_t bp(bx, by, bz);
			auto block = getBlockNoCreateNoEx(bp);
			v3pos_t basep = bp * MAP_BLOCKSIZE;
			pos_t minx_block = rangelim(minp.X - basep.X, 0, MAP_BLOCKSIZE - 1);
			pos_t miny_block = rangelim(minp.Y - basep.Y, 0, MAP_BLOCKSIZE - 1);
			pos_t minz_block = rangelim(minp.Z - basep.Z, 0, MAP_BLOCKSIZE - 1);
			pos_t maxx_block = rangelim(maxp.X - basep.X, 0, MAP_BLOCKSIZE - 1);
			pos_t maxy_block = rangelim(maxp.Y - basep.Y, 0, MAP_BLOCKSIZE - 1);
			pos_t maxz_block = rangelim(maxp.Z - basep.Z, 0, MAP_BLOCKSIZE - 1);
			for (pos_t z_block = minz_block; z_block <= maxz_block; z_block++)
			for (pos_t y_block = miny_block; y_block <= maxy_block; y_block++)
			for (pos_t x_block = minx_block; x_block <= maxx_block; x_block++) {
				v3bpos_t p = basep + v3pos_t(x_block, y_block, z_block);
				MapNode n = block ?
						block->getNodeNoCheck(x_block, y_block, z_block) :
						MapNode(CONTENT_IGNORE);
				if (!func(p, n))
					return;
			}
		}
	}

	bool isBlockOccluded(MapBlock *block, v3pos_t cam_pos_nodes)
	{
		return isBlockOccluded(block->getPosRelative(), cam_pos_nodes, false);
	}
	bool isBlockOccluded(v3pos_t pos_relative, v3pos_t cam_pos_nodes, bool simple_check = false);

protected:
	IGameDef *m_gamedef;
	std::set<MapEventReceiver*> m_event_receivers;

/*
	std::unordered_map<v2bpos_t, MapSector*> m_sectors;

	// Be sure to set this to NULL when the cached sector is deleted
	MapSector *m_sector_cache = nullptr;
	v2bpos_t m_sector_cache_p;
*/

	// This stores the properties of the nodes on the map.
	const NodeDefManager *m_nodedef;

	// Can be implemented by child class
	virtual void reportMetrics(u64 save_time_us, u32 saved_blocks, u32 all_blocks) {}

	bool determineAdditionalOcclusionCheck(v3pos_t pos_camera,
		const core::aabbox3d<pos_t> &block_bounds, v3pos_t &to_check);
	bool isOccluded(v3pos_t pos_camera, v3pos_t pos_target,
		float step, float stepfac, float start_offset, float end_offset,
		u32 needed_count);
};

class MMVManip : public VoxelManipulator
{
public:
	MMVManip(Map *map);
	virtual ~MMVManip() = default;

	/*
		Loads specified area from map and *adds* it to the area already
		contained in the VManip.
	*/
	void initialEmerge(v3bpos_t blockpos_min, v3bpos_t blockpos_max,
		bool load_if_inexistent = true);

	/**
		Uses the flags array to determine which blocks the VManip covers,
		and for which of them we have any data.
		@warning requires VManip area to be block-aligned
		@return map of blockpos -> any data?
	*/
	std::map<v3bpos_t, bool> getCoveredBlocks() const;

	/**
		Writes data in VManip back to the map. Blocks without any data in the VManip
		are skipped.
		@note VOXELFLAG_NO_DATA is checked per-block, not per-node. So you need
		to ensure that the relevant parts of m_data are initialized.
		@param modified_blocks output array of touched blocks (optional)
		@param overwrite_generated if false, blocks marked as generate in the map are not changed
	*/
	void blitBackAll(std::map<v3bpos_t, MapBlock*> * modified_blocks,
		bool overwrite_generated = true
		, bool save_generated_block = true) const;

	/*
		Creates a copy of this VManip including contents, the copy will not be
		associated with a Map.
	*/
	MMVManip *clone() const;

	// Reassociates a copied VManip to a map
	void reparent(Map *map);

	// Is it impossible to call initialEmerge / blitBackAll?
	inline bool isOrphan() const { return !m_map; }

	bool m_is_dirty = false;

protected:
	MMVManip() {};

	// may be null
public:
	Map *m_map = nullptr;
};
