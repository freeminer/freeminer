/*
map.h
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

#pragma once

#include <cstdint>
#include <iostream>
#include <sstream>
#include <set>
#include <map>
#include "irr_v3d.h"
#include "util/unordered_map_hash.h"
#include "threading/concurrent_unordered_map.h"
#include "threading/concurrent_set.h"
#include <list>

#include "irrlichttypes_bloated.h"
#include "mapblock.h"
#include "mapnode.h"
#include "constants.h"
#include "voxel.h"
#include "modifiedstate.h"
#include "util/container.h"
#include "util/metricsbackend.h"
#include "util/numeric.h"
#include "nodetimer.h"
#include "map_settings_manager.h"
#include "debug.h"

#include "mapblock.h"
#include <sys/types.h>
#include <unordered_set>
#include "fm_nodecontainer.h"
#include "config.h"

class Settings;
class MapDatabase;
class ClientMap;
class MapSector;
class ServerMapSector;
class MapBlock;
class NodeMetadata;
class IGameDef;
class IRollbackManager;
class EmergeManager;
class MetricsBackend;
class ServerEnvironment;
struct BlockMakeData;
class Server;

/*
	MapEditEvent
*/

enum MapEditEventType{
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
	v3s16 p;
	MapNode n = CONTENT_AIR;
	std::vector<v3s16> modified_blocks; // Represents a set
	bool is_private_change = false;

	MapEditEvent() = default;

	// Sets the event's position and marks the block as modified.
	void setPositionModified(v3s16 pos)
	{
		assert(modified_blocks.empty()); // only meant for initialization (once)
		p = pos;
		modified_blocks.push_back(getNodeBlockPos(pos));
	}

	void setModifiedBlocks(const std::map<v3s16, MapBlock *> blocks)
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
			for (v3s16 p : modified_blocks) {
				v3s16 np1 = p*MAP_BLOCKSIZE;
				v3s16 np2 = np1 + v3s16(1,1,1)*MAP_BLOCKSIZE - v3s16(1,1,1);
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

	/*
		Drop (client) or delete (server) the map.
	*/
	virtual void drop()
	{
		delete this;
	}

	void addEventReceiver(MapEventReceiver *event_receiver);
	void removeEventReceiver(MapEventReceiver *event_receiver);
	// event shall be deleted by caller after the call.
	void dispatchEvent(const MapEditEvent &event);

	// Returns InvalidPositionException if not found
	MapBlock * getBlockNoCreate(v3s16 p);
	// Returns NULL if not found
	MapBlock * getBlockNoCreateNoEx(v3pos_t p, bool trylock = false, bool nocache = false);
	MapBlockP getBlock(v3pos_t p, bool trylock = false, bool nocache = false);
	void getBlockCacheFlush();

	/* Server overrides */
	virtual MapBlock * emergeBlock(v3bpos_t p, bool create_blank=false)
	{ return getBlockNoCreateNoEx(p); }

	inline const NodeDefManager * getNodeDefManager() { return m_nodedef; }

	bool isValidPosition(v3s16 p);

	// throws InvalidPositionException if not found
	void setNode(v3pos_t p, MapNode n, bool important = false);

	// Returns a CONTENT_IGNORE node if not found
	// If is_valid_position is not NULL then this will be set to true if the
	// position is valid, otherwise false
	MapNode getNode(v3s16 p, bool *is_valid_position = NULL);

	/*
		These handle lighting but not faces.
	*/
	virtual void addNodeAndUpdate(v3s16 p, MapNode n,
			std::map<v3s16, MapBlock*> &modified_blocks,
			bool remove_metadata = true,
			int fast = 0, bool important = false
			);
	void removeNodeAndUpdate(v3s16 p,
			std::map<v3bpos_t, MapBlock*> &modified_blocks, int fast = 0, bool important = false);

	/*
		Wrappers for the latter ones.
		These emit events.
		Return true if succeeded, false if not.
	*/
	bool addNodeWithEvent(v3pos_t p, MapNode n, bool remove_metadata = true, bool important = false);
	bool removeNodeWithEvent(v3pos_t p, bool important);

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
	virtual bool deleteBlock(v3s16 blockpos) { return false; }

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
	void unloadUnreferencedBlocks(std::vector<v3s16> *unloaded_blocks=NULL);

	// For debug printing. Prints "Map: ", "ServerMap: " or "ClientMap: "
	virtual void PrintInfo(std::ostream &out);

	/*
		Node metadata
		These are basically coordinate wrappers to MapBlock
	*/

	std::vector<v3s16> findNodesWithMetadata(v3s16 p1, v3s16 p2);
	NodeMetadata *getNodeMetadata(v3s16 p);

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
	bool setNodeMetadata(v3s16 p, NodeMetadata *meta);
	void removeNodeMetadata(v3s16 p);

	/*
		Node Timers
		These are basically coordinate wrappers to MapBlock
	*/

	NodeTimer getNodeTimer(v3s16 p);
	void setNodeTimer(const NodeTimer &t);
	void removeNodeTimer(v3s16 p);

	/*
		Utilities
	*/



//freeminer:
	MapNode &getNodeTry(const v3pos_t &p);
	//MapNode getNodeNoLock(v3s16 p); // dont use

	std::atomic_uint m_liquid_step_flow{0};

	virtual s16 getHeat(const v3pos_t &p, bool no_random = 0);
	virtual s16 getHumidity(const v3pos_t &p, bool no_random = 0);

	// from old mapsector:
	typedef concurrent_unordered_map<v3pos_t, MapBlockP, v3posHash, v3posEqual>
			m_blocks_type;
	m_blocks_type m_blocks;
	typedef concurrent_shared_unordered_map<v3pos_t, std::shared_ptr<MapBlock>, v3posHash,
			v3posEqual>
			m_far_blocks_type;
	m_far_blocks_type m_far_blocks;
	v3pos_t m_far_blocks_last_cam_pos;
	std::vector<std::shared_ptr<MapBlock>> m_far_blocks_delete_1, m_far_blocks_delete_2;
	bool m_far_blocks_delete_current = false;

	//static constexpr bool m_far_fast =			true; // show generated far farmesh stable(0) or instant(1)
	uint32_t m_far_blocks_use_timestamp = 0;
	uint32_t m_far_blocks_clean_timestamp = 0;
	// MapBlock * getBlockNoCreateNoEx(v3pos_t & p);
	MapBlock *createBlankBlockNoInsert(const v3bpos_t &p);
	MapBlock *createBlankBlock(const v3bpos_t &p);
	bool insertBlock(MapBlock *block);
	void eraseBlock(const MapBlockP block);
	std::unordered_map<MapBlockP, int> *m_blocks_delete = nullptr;
	std::unordered_map<MapBlockP, int> m_blocks_delete_1, m_blocks_delete_2;
	uint64_t m_blocks_delete_time = 0;
	// void getBlocks(std::list<MapBlock*> &dest);
	concurrent_shared_unordered_map<v3pos_t, int, v3posHash, v3posEqual> m_db_miss;

#if !ENABLE_THREADS
	locker<> m_nothread_locker;
#endif
#if ENABLE_THREADS && !HAVE_THREAD_LOCAL
	try_shared_mutex m_block_cache_mutex;
#endif
#if !HAVE_THREAD_LOCAL
	MapBlockP m_block_cache;
	v3POS m_block_cache_p;
#endif
	void copy_27_blocks_to_vm(MapBlock *block, VoxelManipulator &vmanip);

protected:
	u32 m_blocks_update_last = 0;
	u32 m_blocks_save_last = 0;

public:
	std::atomic_uint time_life {0};

	inline MapNode getNodeNoEx(const v3pos_t &p) override { return getNodeTry(p); };
	inline MapNode getNodeNoExNoEmerge(const v3pos_t &p) override
	{
		return getNodeTry(p);
	};
	inline MapNode &getNodeRefUnsafe(const v3pos_t &p) override { return getNodeTry(p); }

	//end of freeminer




public:
	// Iterates through all nodes in the area in an unspecified order.
	// The given callback takes the position as its first argument and the node
	// as its second. If it returns false, forEachNodeInArea returns early.
	template<typename F>
	void forEachNodeInArea(v3s16 minp, v3s16 maxp, F func)
	{
		v3s16 bpmin = getNodeBlockPos(minp);
		v3s16 bpmax = getNodeBlockPos(maxp);
		for (s16 bz = bpmin.Z; bz <= bpmax.Z; bz++)
		for (s16 bx = bpmin.X; bx <= bpmax.X; bx++)
		for (s16 by = bpmin.Y; by <= bpmax.Y; by++) {
			// y is iterated innermost to make use of the sector cache.
			v3s16 bp(bx, by, bz);
			MapBlock *block = getBlockNoCreateNoEx(bp);
			v3s16 basep = bp * MAP_BLOCKSIZE;
			s16 minx_block = rangelim(minp.X - basep.X, 0, MAP_BLOCKSIZE - 1);
			s16 miny_block = rangelim(minp.Y - basep.Y, 0, MAP_BLOCKSIZE - 1);
			s16 minz_block = rangelim(minp.Z - basep.Z, 0, MAP_BLOCKSIZE - 1);
			s16 maxx_block = rangelim(maxp.X - basep.X, 0, MAP_BLOCKSIZE - 1);
			s16 maxy_block = rangelim(maxp.Y - basep.Y, 0, MAP_BLOCKSIZE - 1);
			s16 maxz_block = rangelim(maxp.Z - basep.Z, 0, MAP_BLOCKSIZE - 1);
			for (s16 z_block = minz_block; z_block <= maxz_block; z_block++)
			for (s16 y_block = miny_block; y_block <= maxy_block; y_block++)
			for (s16 x_block = minx_block; x_block <= maxx_block; x_block++) {
				v3s16 p = basep + v3s16(x_block, y_block, z_block);
				MapNode n = block ?
						block->getNodeNoCheck(x_block, y_block, z_block) :
						MapNode(CONTENT_IGNORE);
				if (!func(p, n))
					return;
			}
		}
	}

	bool isBlockOccluded(MapBlock *block, v3s16 cam_pos_nodes);
protected:
	IGameDef *m_gamedef;
	std::set<MapEventReceiver*> m_event_receivers;

/*
	std::unordered_map<v2s16, MapSector*> m_sectors;

	// Be sure to set this to NULL when the cached sector is deleted
	MapSector *m_sector_cache = nullptr;
	v2s16 m_sector_cache_p;
*/

	// This stores the properties of the nodes on the map.
	const NodeDefManager *m_nodedef;

	// Can be implemented by child class
	virtual void reportMetrics(u64 save_time_us, u32 saved_blocks, u32 all_blocks) {}

	bool determineAdditionalOcclusionCheck(const v3s16 &pos_camera,
		const core::aabbox3d<s16> &block_bounds, v3s16 &check);
	bool isOccluded(const v3s16 &pos_camera, const v3s16 &pos_target,
		float step, float stepfac, float start_offset, float end_offset,
		u32 needed_count);
};

/*
	ServerMap

	This is the only map class that is able to generate map.
*/

class ServerMap : public Map
{
public:
//freeminer:
	virtual s16 updateBlockHeat(ServerEnvironment *env, const v3pos_t &p, MapBlock *block = nullptr, unordered_map_v3pos<s16> *cache = nullptr, bool block_add = true);
	virtual s16 updateBlockHumidity(ServerEnvironment *env, const v3pos_t & p, MapBlock *block = nullptr, unordered_map_v3pos<s16> *cache = nullptr, bool block_add = true);

	size_t transforming_liquid_size();
	v3pos_t transforming_liquid_pop();
	void transforming_liquid_add(const v3pos_t &p);
	size_t transformLiquidsReal(Server *m_server, const unsigned int max_cycle_ms);

	//getSurface level starting on basepos.y up to basepos.y + searchup
	//returns basepos.y -1 if no surface has been found
	// (due to limited data range of basepos.y this will always give a unique
	// return value as long as minetest is compiled at least on 32bit architecture)
	//int getSurface(v3s16 basepos, int searchup, bool walkable_only);
	virtual int getSurface(const v3pos_t& basepos, int searchup, bool walkable_only);
/*
	{
		return basepos.Y - 1;
	}
*/

	//concurrent_unordered_map<v3POS, bool, v3posHash, v3posEqual> m_transforming_liquid;
	std::mutex m_transforming_liquid_mutex;
	typedef unordered_map_v3pos<int> lighting_map_t;
	std::mutex m_lighting_modified_mutex;
	std::map<v3pos_t, int> m_lighting_modified_blocks;
	std::map<unsigned int, lighting_map_t> m_lighting_modified_blocks_range;
	void lighting_modified_add(const v3pos_t& pos, int range = 5);

	void unspreadLight(enum LightBank bank, std::map<v3pos_t, u8> &from_nodes,
			std::set<v3pos_t> &light_sources, std::map<v3bpos_t, MapBlock *> &modified_blocks);
	void spreadLight(enum LightBank bank, std::set<v3pos_t> &from_nodes,
			std::map<v3bpos_t, MapBlock *> &modified_blocks, uint64_t end_ms);

	u32 updateLighting(concurrent_map<v3pos_t, MapBlock *> &a_blocks,
			std::map<v3pos_t, MapBlock *> &modified_blocks, unsigned int max_cycle_ms);
	u32 updateLighting(lighting_map_t & a_blocks, unordered_map_v3pos<int> & processed, unsigned int max_cycle_ms = 0);
	unsigned int updateLightingQueue(unsigned int max_cycle_ms, int & loopcount);

	bool propagateSunlight(
			const v3pos_t& pos, std::set<v3pos_t> &light_sources, bool remove_light = false);

//end of freeminer






	/*
		savedir: directory to which map data should be saved
	*/
	ServerMap(const std::string &savedir, IGameDef *gamedef, EmergeManager *emerge, MetricsBackend *mb);
	~ServerMap();

	/*
		Blocks are generated by using these and makeBlock().
	*/
	bool blockpos_over_mapgen_limit(v3s16 p);
	bool initBlockMake(v3s16 blockpos, BlockMakeData *data);
	void finishBlockMake(BlockMakeData *data,
		std::map<v3s16, MapBlock*> *changed_blocks);

	/*
		Get a block from somewhere.
		- Memory
		- Create blank
	*/
	MapBlock *createBlock(v3s16 p);

	/*
		Forcefully get a block from somewhere.
		- Memory
		- Load from disk
		- Create blank filled with CONTENT_IGNORE

	*/
	MapBlock *emergeBlock(v3bpos_t p, bool create_blank=false) override;

	/*
		Try to get a block.
		If it does not exist in memory, add it to the emerge queue.
		- Memory
		- Emerge Queue (deferred disk or generate)
	*/
	MapBlock *getBlockOrEmerge(v3s16 p3d);

	// Carries out any initialization necessary before block is sent
	void prepareBlock(MapBlock *block);

	// Helper for placing objects on ground level
	s16 findGroundLevel(v2pos_t p2d, bool cacheBlocks);

	bool isBlockInQueue(v3s16 pos);

	void addNodeAndUpdate(v3s16 p, MapNode n,
			std::map<v3s16, MapBlock*> &modified_blocks,
			bool remove_metadata
			, int fast = 0, bool important = false) override;

	/*
		Database functions
	*/
	static MapDatabase *createDatabase(const std::string &name, const std::string &savedir, Settings &conf);

	// Call these before and after saving of blocks
	void beginSave() override;
	void endSave() override;

	s32 save(ModifiedState save_level, float dedicated_server_step = 0.1, bool breakable = 0) override;
	void listAllLoadableBlocks(std::vector<v3s16> &dst);
	void listAllLoadedBlocks(std::vector<v3s16> &dst);

	MapgenParams *getMapgenParams();

	bool saveBlock(MapBlock *block) override;
	static bool saveBlock(MapBlock *block, MapDatabase *db, int compression_level = -1);
	MapBlock* loadBlock(v3s16 p);

	// Blocks are removed from the map but not deleted from memory until
	// deleteDetachedBlocks() is called, since pointers to them may still exist
	// when deleteBlock() is called.
	bool deleteBlock(v3s16 blockpos) override;

	void deleteDetachedBlocks();

	void step();

	void updateVManip(v3s16 pos);

	// For debug printing
	void PrintInfo(std::ostream &out) override;

	bool isSavingEnabled(){ return m_map_saving_enabled; }

	u64 getSeed();

	/*!
	 * Fixes lighting in one map block.
	 * May modify other blocks as well, as light can spread
	 * out of the specified block.
	 * Returns false if the block is not generated (so nothing
	 * changed), true otherwise.
	 */
	bool repairBlockLight(v3s16 blockpos,
		std::map<v3s16, MapBlock *> *modified_blocks);

	size_t transformLiquids(std::map<v3bpos_t, MapBlock*> & modified_blocks,
			ServerEnvironment *env
            , Server *m_server, unsigned int max_cycle_ms			
			);

	MapSettingsManager settings_mgr;

protected:

	void reportMetrics(u64 save_time_us, u32 saved_blocks, u32 all_blocks) override;

private:
	friend class ModApiMapgen; // for m_transforming_liquid

	// Emerge manager
	EmergeManager *m_emerge;

public:
	std::string m_savedir;
	bool m_map_saving_enabled;
	bool m_map_loading_enabled;
	concurrent_shared_unordered_map<v3pos_t, unsigned int, v3posHash, v3posEqual> m_mapgen_process;
private:

	int m_map_compression_level;

	concurrent_set<v3bpos_t> m_chunks_in_progress;

	// used by deleteBlock() and deleteDetachedBlocks()
	std::vector<std::unique_ptr<MapBlock>> m_detached_blocks;

	// Queued transforming water nodes
	UniqueQueue<v3s16> m_transforming_liquid;
	f32 m_transforming_liquid_loop_count_multiplier = 1.0f;
	u32 m_unprocessed_count = 0;
	u64 m_inc_trending_up_start_time = 0; // milliseconds
	bool m_queue_size_timer_started = false;

	/*
		Metadata is re-written on disk only if this is true.
		This is reset to false when written on disk.
	*/
	bool m_map_metadata_changed = true;
public:
	MapDatabase *dbase = nullptr;
private:
	MapDatabase *dbase_ro = nullptr;

	// Map metrics
	MetricGaugePtr m_loaded_blocks_gauge;
	MetricCounterPtr m_save_time_counter;
	MetricCounterPtr m_save_count_counter;
};

#if !ENABLE_THREADS
	#define MAP_NOTHREAD_LOCK(map) auto lock_map = map->m_nothread_locker.lock_unique_rec();
#else
	#define MAP_NOTHREAD_LOCK(map) ;
#endif

#define VMANIP_BLOCK_DATA_INEXIST     1
#define VMANIP_BLOCK_CONTAINS_CIGNORE 2

class MMVManip : public VoxelManipulator
{
public:
	MMVManip(Map *map);
	virtual ~MMVManip() = default;

	virtual void clear()
	{
		VoxelManipulator::clear();
		m_loaded_blocks.clear();
	}

	void initialEmerge(v3s16 blockpos_min, v3s16 blockpos_max,
		bool load_if_inexistent = true);

	// This is much faster with big chunks of generated data
	void blitBackAll(std::map<v3s16, MapBlock*> * modified_blocks,
		bool overwrite_generated = true
		, bool save_generated_block = true);

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
protected:
	/*
		key = blockpos
		value = flags describing the block
	*/
	std::map<v3s16, u8> m_loaded_blocks;
};
