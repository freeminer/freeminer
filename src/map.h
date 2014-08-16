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

#ifndef MAP_HEADER
#define MAP_HEADER

#include <iostream>
#include <sstream>
#include <set>
#include <map>
#include "util/unordered_map_hash.h"
#include <list>

#include "irrlichttypes_bloated.h"
#include "mapnode.h"
#include "constants.h"
#include "voxel.h"
#include "modifiedstate.h"
#include "util/container.h"
#include "nodetimer.h"

class Database;
class ClientMap;
class MapSector;
class ServerMapSector;
class MapBlock;
class NodeMetadata;
class IGameDef;
class IRollbackReportSink;
class EmergeManager;
class ServerEnvironment;
struct BlockMakeData;
struct MapgenParams;
class Circuit;
class Server;


/*
	MapEditEvent
*/

#define MAPTYPE_BASE 0
#define MAPTYPE_SERVER 1
#define MAPTYPE_CLIENT 2

enum MapEditEventType{
	// Node added (changed from air or something else to something)
	MEET_ADDNODE,
	// Node removed (changed to air)
	MEET_REMOVENODE,
	// Node swapped (changed without metadata change)
	MEET_SWAPNODE,
	// Node metadata of block changed (not knowing which node exactly)
	// p stores block coordinate
	MEET_BLOCK_NODE_METADATA_CHANGED,
	// Anything else (modified_blocks are set unsent)
	MEET_OTHER
};

struct MapEditEvent
{
	MapEditEventType type;
	v3s16 p;
	MapNode n;
	std::set<v3s16> modified_blocks;
	u16 already_known_by_peer;

	MapEditEvent():
		type(MEET_OTHER),
		already_known_by_peer(0)
	{
	}

	MapEditEvent * clone()
	{
		MapEditEvent *event = new MapEditEvent();
		event->type = type;
		event->p = p;
		event->n = n;
		event->modified_blocks = modified_blocks;
		return event;
	}

	VoxelArea getArea()
	{
		switch(type){
		case MEET_ADDNODE:
			return VoxelArea(p);
		case MEET_REMOVENODE:
			return VoxelArea(p);
		case MEET_SWAPNODE:
			return VoxelArea(p);
		case MEET_BLOCK_NODE_METADATA_CHANGED:
		{
			v3s16 np1 = p*MAP_BLOCKSIZE;
			v3s16 np2 = np1 + v3s16(1,1,1)*MAP_BLOCKSIZE - v3s16(1,1,1);
			return VoxelArea(np1, np2);
		}
		case MEET_OTHER:
		{
			VoxelArea a;
			for(std::set<v3s16>::iterator
					i = modified_blocks.begin();
					i != modified_blocks.end(); ++i)
			{
				v3s16 p = *i;
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
	virtual void onMapEditEvent(MapEditEvent *event) = 0;
};

class Map /*: public NodeContainer*/
{
public:
	Map(IGameDef *gamedef, Circuit* circuit = nullptr);
	virtual ~Map();

	/*virtual u16 nodeContainerId() const
	{
		return NODECONTAINER_ID_MAP;
	}*/

	virtual s32 mapType() const
	{
		return MAPTYPE_BASE;
	}

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
	void dispatchEvent(MapEditEvent *event);

	// Returns InvalidPositionException if not found
	MapBlock * getBlockNoCreate(v3s16 p);
	// Returns NULL if not found
	MapBlock * getBlockNoCreateNoEx(v3s16 p);

	/* Server overrides */
	virtual MapBlock * emergeBlock(v3s16 p, bool allow_generate=true)
	{ return getBlockNoCreateNoEx(p); }

	// Returns InvalidPositionException if not found
	bool isNodeUnderground(v3s16 p);

	bool isValidPosition(v3s16 p);

	// throws InvalidPositionException if not found
	MapNode getNode(v3s16 p);

	// throws InvalidPositionException if not found
	void setNode(v3s16 p, MapNode & n);

	// Returns a CONTENT_IGNORE node if not found
	MapNode getNodeNoEx(v3s16 p);
	MapNode getNodeNoLock(v3s16 p);

	void unspreadLight(enum LightBank bank,
			std::map<v3s16, u8> & from_nodes,
			std::set<v3s16> & light_sources,
			std::map<v3s16, MapBlock*> & modified_blocks);

	void unLightNeighbors(enum LightBank bank,
			v3s16 pos, u8 lightwas,
			std::set<v3s16> & light_sources,
			std::map<v3s16, MapBlock*> & modified_blocks);

	void spreadLight(enum LightBank bank,
			std::set<v3s16> & from_nodes,
			std::map<v3s16, MapBlock*> & modified_blocks, int recursive = 0);

	void lightNeighbors(enum LightBank bank,
			v3s16 pos,
			std::map<v3s16, MapBlock*> & modified_blocks);

	v3s16 getBrightestNeighbour(enum LightBank bank, v3s16 p);

	s16 propagateSunlight(v3s16 start,
			std::map<v3s16, MapBlock*> & modified_blocks);

	u32 updateLighting(enum LightBank bank,
			shared_map<v3s16, MapBlock*>  & a_blocks,
			std::map<v3s16, MapBlock*> & modified_blocks, int max_cycle_ms = 0);

	u32 updateLighting(shared_map<v3s16, MapBlock*>  & a_blocks,
			std::map<v3s16, MapBlock*> & modified_blocks, int max_cycle_ms = 0);

	u32 updateLighting_last[2];

	/*
		These handle lighting but not faces.
	*/
	void addNodeAndUpdate(v3s16 p, MapNode n,
			std::map<v3s16, MapBlock*> &modified_blocks,
			bool remove_metadata = true);
	void removeNodeAndUpdate(v3s16 p,
			std::map<v3s16, MapBlock*> &modified_blocks);

	/*
		Wrappers for the latter ones.
		These emit events.
		Return true if succeeded, false if not.
	*/
	bool addNodeWithEvent(v3s16 p, MapNode n, bool remove_metadata = true);
	bool removeNodeWithEvent(v3s16 p);

	/*
		Takes the blocks at the edges into account
	*/
	bool getDayNightDiff(v3s16 blockpos);

	//core::aabbox3d<s16> getDisplayedBlockArea();

	//bool updateChangedVisibleArea();

	// Call these before and after saving of many blocks
	virtual void beginSave() {return;};
	virtual void endSave() {return;};

	virtual s32 save(ModifiedState save_level, bool breakable){assert(0); return 0;};

	// Server implements this.
	// Client leaves it as no-op.
	virtual bool saveBlock(MapBlock *block) { return false; };

	/*
		Updates usage timers and unloads unused blocks and sectors.
		Saves modified blocks before unloading on MAPTYPE_SERVER.
	*/
	u32 timerUpdate(float uptime, float unload_timeout, int max_cycle_ms = 100,
			std::list<v3s16> *unloaded_blocks=NULL);

	/*
		Unloads all blocks with a zero refCount().
		Saves modified blocks before unloading on MAPTYPE_SERVER.
	*/
	void unloadUnreferencedBlocks(std::list<v3s16> *unloaded_blocks=NULL);

	// For debug printing. Prints "Map: ", "ServerMap: " or "ClientMap: "
	virtual void PrintInfo(std::ostream &out);

	u32 transformLiquids(Server *m_server, std::map<v3s16, MapBlock*> & modified_blocks, shared_map<v3s16, MapBlock*> & lighting_modified_blocks, int max_cycle_ms);
	u32 transformLiquidsFinite(Server *m_server, std::map<v3s16, MapBlock*> & modified_blocks, shared_map<v3s16, MapBlock*> & lighting_modified_blocks, int max_cycle_ms);
	/*
		Node metadata
		These are basically coordinate wrappers to MapBlock
	*/

	NodeMetadata* getNodeMetadata(v3s16 p);

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
	void setNodeTimer(v3s16 p, NodeTimer t);
	void removeNodeTimer(v3s16 p);

	/*
		Misc.
	*/
	//DELME std::map<v2s16, MapSector*> *getSectorsPtr(){return &m_sectors;}

	/*
		Variables
	*/

	void transforming_liquid_push_back(v3s16 & p);
	u32 transforming_liquid_size();
	u32 m_liquid_step_flow;

	virtual s16 getHeat(v3s16 p, bool no_random = 0);
	virtual s16 getHumidity(v3s16 p, bool no_random = 0);

	virtual int getSurface(v3s16 basepos, int searchup, bool walkable_only) {
		return basepos.Y -1;
	}

	Circuit* getCircuit();
	INodeDefManager* getNodeDefManager();


// from old mapsector:
	MapBlock *m_block_cache;
	v3s16 m_block_cache_p;
	try_shared_mutex m_block_cache_mutex;
	typedef shared_unordered_map<v3s16, MapBlock*, v3s16Hash, v3s16Equal> m_blocks_type;
	m_blocks_type m_blocks;
	//MapBlock * getBlockNoCreateNoEx(v3s16 & p);
	MapBlock * createBlankBlockNoInsert(v3s16 & p);
	MapBlock * createBlankBlock(v3s16 & p);
	void insertBlock(MapBlock *block);
	void deleteBlock(MapBlock *block, bool now = 0);
	std::map<MapBlock *, int> * m_blocks_delete;
	std::map<MapBlock *, int> m_blocks_delete_1, m_blocks_delete_2;
	//void getBlocks(std::list<MapBlock*> &dest);
	MapBlock *getBlockBuffered(v3s16 & p);

protected:
	friend class LuaVoxelManip;

	IGameDef *m_gamedef;
	Circuit* m_circuit;

	std::set<MapEventReceiver*> m_event_receivers;

	//std::map<v2s16, MapSector*> m_sectors;
	u32 m_blocks_update_last;
	u32 m_blocks_save_last;

	// Queued transforming water nodes
public:
	UniqueQueue<v3s16> m_transforming_liquid;
	//JMutex m_transforming_liquid_mutex;
protected:
	//JMutex m_update_lighting_mutex;
};

/*
	ServerMap

	This is the only map class that is able to generate map.
*/

class ServerMap : public Map
{
public:
	/*
		savedir: directory to which map data should be saved
	*/
	ServerMap(std::string savedir, IGameDef *gamedef, EmergeManager *emerge, Circuit* m_circuit);
	~ServerMap();

	s32 mapType() const
	{
		return MAPTYPE_SERVER;
	}

	/*
		Blocks are generated by using these and makeBlock().
	*/
	bool initBlockMake(BlockMakeData *data, v3s16 blockpos);
	void finishBlockMake(BlockMakeData *data,
			std::map<v3s16, MapBlock*> &changed_blocks);

	/*
		Get a block from somewhere.
		- Memory
		- Create blank
	*/
	MapBlock * createBlock(v3s16 p);

	/*
		Forcefully get a block from somewhere.
		- Memory
		- Load from disk
		- Create blank filled with CONTENT_IGNORE

	*/
	MapBlock * emergeBlock(v3s16 p, bool create_blank=true);

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
	s16 findGroundLevel(v2s16 p2d, bool cacheBlocks);

	/*
		Misc. helper functions for fiddling with directory and file
		names when saving
	*/
	void createDirs(std::string path);
	// dirname: final directory name

	/*
		Database functions
	*/
	// Verify we can read/write to the database
	void verifyDatabase();

	// Call these before and after saving of blocks
	void beginSave();
	void endSave();

	s32 save(ModifiedState save_level, bool breakable = 0);
	void listAllLoadableBlocks(std::list<v3s16> &dst);
	void listAllLoadedBlocks(std::list<v3s16> &dst);
	// Saves map seed and possibly other stuff
	void saveMapMeta();
	void loadMapMeta();

	bool saveBlock(MapBlock *block, Database *db);
	bool saveBlock(MapBlock *block);
	MapBlock* loadBlock(v3s16 p);

	// For debug printing
	virtual void PrintInfo(std::ostream &out);

	bool isSavingEnabled(){ return m_map_saving_enabled; }

	u64 getSeed();
	s16 getWaterLevel();

	virtual s16 updateBlockHeat(ServerEnvironment *env, v3s16 p, MapBlock *block = nullptr, std::map<v3s16, s16> *cache = nullptr);
	virtual s16 updateBlockHumidity(ServerEnvironment *env, v3s16 p, MapBlock *block = nullptr, std::map<v3s16, s16> *cache = nullptr);

	//getSurface level starting on basepos.y up to basepos.y + searchup
	//returns basepos.y -1 if no surface has been found
	// (due to limited data range of basepos.y this will always give a unique
	// return value as long as minetest is compiled at least on 32bit architecture)
	int getSurface(v3s16 basepos, int searchup, bool walkable_only);

private:
	// Emerge manager
	EmergeManager *m_emerge;

public:
	std::string m_savedir;
private:
	bool m_map_saving_enabled;

#if 0
	// Chunk size in MapSectors
	// If 0, chunks are disabled.
	s16 m_chunksize;
	// Chunks
	core::map<v2s16, MapChunk*> m_chunks;
#endif

	/*
		Metadata is re-written on disk only if this is true.
		This is reset to false when written on disk.
	*/
	bool m_map_metadata_changed;
	Database *dbase;
};


#define VMANIP_BLOCK_DATA_INEXIST     1
#define VMANIP_BLOCK_CONTAINS_CIGNORE 2

class ManualMapVoxelManipulator : public VoxelManipulator
{
public:
	ManualMapVoxelManipulator(Map *map);
	virtual ~ManualMapVoxelManipulator();

	virtual void clear()
	{
		VoxelManipulator::clear();
		m_loaded_blocks.clear();
	}

	void setMap(Map *map)
	{m_map = map;}

	void initialEmerge(v3s16 blockpos_min, v3s16 blockpos_max,
			bool load_if_inexistent = true);

	// This is much faster with big chunks of generated data
	void blitBackAll(std::map<v3s16, MapBlock*> * modified_blocks,
			bool overwrite_generated = true);

	bool replace_generated;
protected:
	bool m_create_area;
	Map *m_map;
	/*
		key = blockpos
		value = flags describing the block
	*/
	std::map<v3s16, u8> m_loaded_blocks;
};

#endif
