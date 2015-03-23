/*
mapblock.h
Copyright (C) 2013 celeron55, Perttu Ahola <celeron55@gmail.com>
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

#ifndef MAPBLOCK_HEADER
#define MAPBLOCK_HEADER

#include <set>
#include "debug.h"
#include "irr_v3d.h"
#include "mapnode.h"
#include "exceptions.h"
#include "constants.h"
#include "staticobject.h"
#include "nodemetadata.h"
#include "nodetimer.h"
#include "modifiedstate.h"
#include "util/numeric.h" // getContainerPos
#include "util/lock.h"

class Map;
class NodeMetadataList;
class IGameDef;
class MapBlockMesh;
class VoxelManipulator;
class Circuit;
class ServerEnvironment;
struct ActiveABM;

#define BLOCK_TIMESTAMP_UNDEFINED 0xffffffff

/*// Named by looking towards z+
enum{
	FACE_BACK=0,
	FACE_TOP,
	FACE_RIGHT,
	FACE_FRONT,
	FACE_BOTTOM,
	FACE_LEFT
};*/

// NOTE: If this is enabled, set MapBlock to be initialized with
//       CONTENT_IGNORE.
/*enum BlockGenerationStatus
{
	// Completely non-generated (filled with CONTENT_IGNORE).
	BLOCKGEN_UNTOUCHED=0,
	// Trees or similar might have been blitted from other blocks to here.
	// Otherwise, the block contains CONTENT_IGNORE
	BLOCKGEN_FROM_NEIGHBORS=2,
	// Has been generated, but some neighbors might put some stuff in here
	// when they are generated.
	// Does not contain any CONTENT_IGNORE
	BLOCKGEN_SELF_GENERATED=4,
	// The block and all its neighbors have been generated
	BLOCKGEN_FULLY_GENERATED=6
};*/

#if 0
enum
{
	NODECONTAINER_ID_MAPBLOCK,
	NODECONTAINER_ID_MAPSECTOR,
	NODECONTAINER_ID_MAP,
	NODECONTAINER_ID_MAPBLOCKCACHE,
	NODECONTAINER_ID_VOXELMANIPULATOR,
};

class NodeContainer
{
public:
	virtual bool isValidPosition(v3s16 p) = 0;
	virtual MapNode getNode(v3s16 p) = 0;
	virtual void setNode(v3s16 p, MapNode & n) = 0;
	virtual u16 nodeContainerId() const = 0;

	MapNode getNodeNoEx(v3s16 p)
	{
		try{
			return getNode(p);
		}
		catch(InvalidPositionException &e){
			return MapNode(CONTENT_IGNORE);
		}
	}
};
#endif

struct abm_trigger_one {
	ActiveABM * abm;
	v3POS pos;
	content_t content;
	u32 active_object_count;
	u32 active_object_count_wider;
	v3POS neighbor_pos;
	bool activate;
};

/*
	MapBlock itself
*/

class MapBlock /*: public NodeContainer*/
: public maybe_locker
{
public:
	MapBlock(Map *parent, v3s16 pos, IGameDef *gamedef, bool dummy=false);
	~MapBlock();
	
	/*virtual u16 nodeContainerId() const
	{
		return NODECONTAINER_ID_MAPBLOCK;
	}*/
	
	Map * getParent()
	{
		return m_parent;
	}

	void reallocate()
	{
		auto lock = lock_unique_rec();
		if(data != NULL)
			delete data;
		u32 l = MAP_BLOCKSIZE * MAP_BLOCKSIZE * MAP_BLOCKSIZE;
		data = reinterpret_cast<MapNode*>( ::operator new(l * sizeof(MapNode)));
		memset(data, 0, l * sizeof(MapNode));
	}

	/*
		Flags
	*/

	bool isDummy()
	{
		return false;
		//auto lock = lock_shared_rec();
		//return (data == NULL);
	}
	void unDummify()
	{
		reallocate();
	}
	
	// m_modified methods
	void raiseModified(u32 mod);
	void raiseModified(u32 mod, const std::string &reason)
	{
		raiseModified(mod);

#ifdef WTFdebug
		if(mod >= MOD_STATE_WRITE_NEEDED && m_timestamp != BLOCK_TIMESTAMP_UNDEFINED) {
			m_changed_timestamp = m_timestamp;
		}
		if(mod > m_modified){
			m_modified = mod;
			m_modified_reason = reason;
			m_modified_reason_too_long = false;

			if(m_modified >= MOD_STATE_WRITE_AT_UNLOAD){
				m_disk_timestamp = m_timestamp;
			}
		} else if(mod == m_modified){
			if(!m_modified_reason_too_long){
				if(m_modified_reason.size() < 40)
					m_modified_reason += ", " + reason;
				else{
					m_modified_reason += "...";
					m_modified_reason_too_long = true;
				}
			}
		}
#endif
	}
	void raiseModified(u32 mod, const char *reason)
	{
		raiseModified(mod);
#ifdef WTFdebug
		if (mod > m_modified){
			m_modified = mod;
			m_modified_reason = reason;
			m_modified_reason_too_long = false;

			if (m_modified >= MOD_STATE_WRITE_AT_UNLOAD){
				m_disk_timestamp = m_timestamp;
			}
		}
		else if (mod == m_modified){
			if (!m_modified_reason_too_long){
				if (m_modified_reason.size() < 40)
					m_modified_reason += ", " + std::string(reason);
				else{
					m_modified_reason += "...";
					m_modified_reason_too_long = true;
				}
			}
		}
#endif
	}

	u32 getModified()
	{
		return m_modified;
	}
	std::string getModifiedReason()
	{
#ifdef WTFdebug
		return m_modified_reason;
#else
		return std::string();
#endif
	}
	void resetModified()
	{
		m_modified = MOD_STATE_CLEAN;
#ifdef WTFdebug
		m_modified_reason = "none";
		m_modified_reason_too_long = false;
#endif
	}
	
	// is_underground getter/setter
	bool getIsUnderground()
	{
		return is_underground;
	}
	void setIsUnderground(bool a_is_underground)
	{
		is_underground = a_is_underground;
/*
		raiseModified(MOD_STATE_WRITE_NEEDED, "setIsUnderground");
*/
	}

	void setLightingExpired(bool expired)
	{
//		if(expired != m_lighting_expired){
			m_lighting_expired = expired;
/*
			raiseModified(MOD_STATE_WRITE_NEEDED, "setLightingExpired");
*/
//		}
	}
	bool getLightingExpired()
	{
		return m_lighting_expired;
	}

	bool isGenerated()
	{
		return m_generated;
	}
	void setGenerated(bool b)
	{
		if(b != m_generated){
/*
			raiseModified(MOD_STATE_WRITE_NEEDED, "setGenerated");
*/
			m_generated = b;
		}
	}

	bool isValid()
	{
		//if(m_lighting_expired)
		//	return false;
		if(data == NULL)
			return false;
		return true;
	}

	/*
		Position stuff
	*/

	v3s16 getPos()
	{
		return m_pos;
	}
		
	v3s16 getPosRelative()
	{
		return m_pos * MAP_BLOCKSIZE;
	}
		
	core::aabbox3d<s16> getBox()
	{
		return core::aabbox3d<s16>(getPosRelative(),
				getPosRelative()
				+ v3s16(MAP_BLOCKSIZE, MAP_BLOCKSIZE, MAP_BLOCKSIZE)
				- v3s16(1,1,1));
	}

	/*
		Regular MapNode get-setters
	*/
	
	bool isValidPosition(s16 x, s16 y, s16 z)
	{
		return data != NULL
				&& x >= 0 && x < MAP_BLOCKSIZE
				&& y >= 0 && y < MAP_BLOCKSIZE
				&& z >= 0 && z < MAP_BLOCKSIZE;
	}

	bool isValidPosition(v3s16 p)
	{
		return isValidPosition(p.X, p.Y, p.Z);
	}

	MapNode getNode(v3POS p, bool *valid_position)
	{
		*valid_position = isValidPosition(p.X, p.Y, p.Z);

		if (!*valid_position)
			return MapNode(CONTENT_IGNORE);

		auto lock = lock_shared_rec();
		return data[p.Z*MAP_BLOCKSIZE*MAP_BLOCKSIZE + p.Y*MAP_BLOCKSIZE + p.X];
	}

	MapNode getNode(v3s16 p)
	{
		return getNodeNoEx(p);
	}

	MapNode getNodeTry(v3s16 p)
	{
		auto lock = try_lock_shared_rec();
		if (!lock->owns_lock())
			return MapNode(CONTENT_IGNORE);
		return getNodeNoLock(p);
	}

	MapNode getNodeNoLock(v3POS p)
	{
		if (!data)
			return MapNode(CONTENT_IGNORE);
		return data[p.Z*MAP_BLOCKSIZE*MAP_BLOCKSIZE + p.Y*MAP_BLOCKSIZE + p.X];
	}

	MapNode getNodeNoEx(v3POS p);

	void setNode(v3s16 p, MapNode & n);

	/*
		Non-checking variants of the above
	*/

	MapNode getNodeNoCheck(s16 x, s16 y, s16 z, bool *valid_position)
	{
		*valid_position = data != NULL;
		if(!valid_position)
			return MapNode(CONTENT_IGNORE);

		auto lock = lock_shared_rec();
		return data[z*MAP_BLOCKSIZE*MAP_BLOCKSIZE + y*MAP_BLOCKSIZE + x];
	}
	
	MapNode getNodeNoCheck(v3s16 p, bool *valid_position)
	{
		return getNodeNoCheck(p.X, p.Y, p.Z, valid_position);
	}
	
	void setNodeNoCheck(v3s16 p, MapNode & n);

	/*
		These functions consult the parent container if the position
		is not valid on this MapBlock.
	*/
	bool isValidPositionParent(v3s16 p);
	MapNode getNodeParent(v3s16 p, bool *is_valid_position = NULL);

	void drawbox(s16 x0, s16 y0, s16 z0, s16 w, s16 h, s16 d, MapNode node)
	{
		for(u16 z=0; z<d; z++)
			for(u16 y=0; y<h; y++)
				for(u16 x=0; x<w; x++)
					setNode(v3s16(x0+x, y0+y, z0+z), node);
	}

	// See comments in mapblock.cpp
	bool propagateSunlight(std::set<v3s16> & light_sources,
			bool remove_light=false, bool *black_air_left=NULL);
	
	// Copies data to VoxelManipulator to getPosRelative()
	void copyTo(VoxelManipulator &dst);
	// Copies data from VoxelManipulator getPosRelative()
	void copyFrom(VoxelManipulator &dst);

	/*
		Update day-night lighting difference flag.
		Sets m_day_night_differs to appropriate value.
		These methods don't care about neighboring blocks.
	*/
	void actuallyUpdateDayNightDiff();
	/*
		Call this to schedule what the previous function does to be done
		when the value is actually needed.
	*/
	void expireDayNightDiff();

	bool getDayNightDiff()
	{
		if(m_day_night_differs_expired)
			actuallyUpdateDayNightDiff();
		return m_day_night_differs;
	}

	/*
		Miscellaneous stuff
	*/
	
	/*
		Tries to measure ground level.
		Return value:
			-1 = only air
			-2 = only ground
			-3 = random fail
			0...MAP_BLOCKSIZE-1 = ground level
	*/
	s16 getGroundLevel(v2s16 p2d);

	/*
		Timestamp (see m_timestamp)
		NOTE: BLOCK_TIMESTAMP_UNDEFINED=0xffffffff means there is no timestamp.
	*/
	void setTimestamp(u32 time)
	{
		m_timestamp = time;
		raiseModified(MOD_STATE_WRITE_AT_UNLOAD, "setTimestamp");
	}
	void setTimestampNoChangedFlag(u32 time)
	{
		m_timestamp = time;
	}
	u32 getTimestamp()
	{
		return m_timestamp;
	}
	u32 getDiskTimestamp()
	{
		return m_disk_timestamp;
	}
	
	/*
		See m_usage_timer
	*/
	void resetUsageTimer()
	{
		auto lock = lock_unique_rec();
		m_usage_timer = 0;
	}
	float getUsageTimer()
	{
		auto lock = lock_shared_rec();
		return m_usage_timer;
	}
	void incrementUsageTimer(float dtime);

	/*
		See m_refcount
	*/
	void refGrab()
	{
		m_refcount++;
	}
	void refDrop()
	{
		m_refcount--;
	}
	int refGet()
	{
		return m_refcount;
	}
	
	/*
		Node Timers
	*/
	// Get timer
	NodeTimer getNodeTimer(v3s16 p){ 
		return m_node_timers.get(p);
	}
	// Deletes timer
	void removeNodeTimer(v3s16 p){
		m_node_timers.remove(p);
	}
	// Deletes old timer and sets a new one
	void setNodeTimer(v3s16 p, NodeTimer t){
		m_node_timers.set(p,t);
	}
	// Deletes all timers
	void clearNodeTimers(){
		m_node_timers.clear();
	}

	/*
		Serialization
	*/
	
	// These don't write or read version by itself
	// Set disk to true for on-disk format, false for over-the-network format
	// Precondition: version >= SER_FMT_CLIENT_VER_LOWEST
	void serialize(std::ostream &os, u8 version, bool disk);
	// If disk == true: In addition to doing other things, will add
	// unknown blocks from id-name mapping to wndef
	bool deSerialize(std::istream &is, u8 version, bool disk);

	void serializeNetworkSpecific(std::ostream &os, u16 net_proto_version);
	void deSerializeNetworkSpecific(std::istream &is);

	void pushElementsToCircuit(Circuit* circuit);

#ifndef SERVER // Only on client
	typedef std::shared_ptr<MapBlockMesh> mesh_type;

	MapBlock::mesh_type getMesh(int step = 1);
	void setMesh(MapBlock::mesh_type & rmesh);
#endif

private:
	/*
		Private methods
	*/

	void deSerialize_pre22(std::istream &is, u8 version, bool disk);

	/*
		Used only internally, because changes can't be tracked
	*/

	MapNode & getNodeRef(s16 x, s16 y, s16 z)
	{
		if(data == NULL)
			throw InvalidPositionException("getNodeRef data=NULL");
		if(x < 0 || x >= MAP_BLOCKSIZE) throw InvalidPositionException("getNodeRef x out of size");
		if(y < 0 || y >= MAP_BLOCKSIZE) throw InvalidPositionException("getNodeRef y out of size");
		if(z < 0 || z >= MAP_BLOCKSIZE) throw InvalidPositionException("getNodeRef z out of size");
		return data[z*MAP_BLOCKSIZE*MAP_BLOCKSIZE + y*MAP_BLOCKSIZE + x];
	}
	MapNode & getNodeRef(v3s16 &p)
	{
		return getNodeRef(p.X, p.Y, p.Z);
	}

public:
	/*
		Public member variables
	*/

#ifndef SERVER // Only on client
	mesh_type mesh, mesh_old;
	mesh_type mesh2, mesh4, mesh8, mesh16;
	unsigned int mesh_size;
#endif
	
	NodeMetadataList m_node_metadata;
	NodeTimerList m_node_timers;
	StaticObjectList m_static_objects;
	
	std::atomic_short heat;
	std::atomic_short humidity;
	u32 heat_last_update;
	u32 humidity_last_update;
	float m_uptime_timer_last;

	// Last really changed time (need send to client)
	std::atomic_uint m_changed_timestamp;
	u32 m_next_analyze_timestamp;
	typedef std::list<abm_trigger_one> abm_triggers_type;
	std::unique_ptr<abm_triggers_type> abm_triggers;
	std::mutex abm_triggers_mutex;
	void abmTriggersRun(ServerEnvironment * m_env, u32 time, bool activate = false);
	u32 m_abm_timestamp;

	u32 getActualTimestamp() {
		u32 block_timestamp = 0;
		if (m_changed_timestamp && m_changed_timestamp != BLOCK_TIMESTAMP_UNDEFINED) {
			block_timestamp = m_changed_timestamp;
		} else if (m_disk_timestamp && m_disk_timestamp != BLOCK_TIMESTAMP_UNDEFINED) {
			block_timestamp = m_disk_timestamp;
		}
		return block_timestamp;
	}

	// Set to content type of a node if the block consists solely of nodes of one type, otherwise set to CONTENT_IGNORE
	content_t content_only;
	content_t analyzeContent() {
		auto lock = lock_shared_rec();
		content_only = data[0].param0;
		for (int i = 1; i<MAP_BLOCKSIZE*MAP_BLOCKSIZE*MAP_BLOCKSIZE; ++i) {
			if (data[i].param0 != content_only) {
				content_only = CONTENT_IGNORE;
				break;
			}
		}
		return content_only;
	}

private:
	/*
		Private member variables
	*/

	// NOTE: Lots of things rely on this being the Map
	Map *m_parent;
	// Position in blocks on parent
	v3s16 m_pos;

	IGameDef *m_gamedef;
	
	/*
		If NULL, block is a dummy block.
		Dummy blocks are used for caching not-found-on-disk blocks.
	*/
	MapNode * data;

	/*
		- On the server, this is used for telling whether the
		  block has been modified from the one on disk.
		- On the client, this is used for nothing.
	*/
	u32 m_modified;

#ifdef WTFdebug
	std::string m_modified_reason;
	bool m_modified_reason_too_long;
#endif

	/*
		When propagating sunlight and the above block doesn't exist,
		sunlight is assumed if this is false.

		In practice this is set to true if the block is completely
		undeground with nothing visible above the ground except
		caves.
	*/
	bool is_underground;

	/*
		Set to true if changes has been made that make the old lighting
		values wrong but the lighting hasn't been actually updated.

		If this is false, lighting is exactly right.
		If this is true, lighting might be wrong or right.
	*/
	std::atomic_bool m_lighting_expired;
	
	// Whether day and night lighting differs
	bool m_day_night_differs;
	std::atomic_bool m_day_night_differs_expired;

	bool m_generated;
	
	/*
		When block is removed from active blocks, this is set to gametime.
		Value BLOCK_TIMESTAMP_UNDEFINED=0xffffffff means there is no timestamp.
	*/
	std::atomic_uint m_timestamp;
	// The on-disk (or to-be on-disk) timestamp value
	u32 m_disk_timestamp;

	/*
		When the block is accessed, this is set to 0.
		Map will unload the block when this reaches a timeout.
	*/
	float m_usage_timer;

	/*
		Reference count; currently used for determining if this block is in
		the list of blocks to be drawn.
	*/
	std::atomic_int m_refcount;
};

typedef std::vector<MapBlock*> MapBlockVect;

inline bool blockpos_over_limit(v3s16 p)
{
	return
	  (p.X < -MAP_GENERATION_LIMIT / MAP_BLOCKSIZE
	|| p.X >  MAP_GENERATION_LIMIT / MAP_BLOCKSIZE
	|| p.Y < -MAP_GENERATION_LIMIT / MAP_BLOCKSIZE
	|| p.Y >  MAP_GENERATION_LIMIT / MAP_BLOCKSIZE
	|| p.Z < -MAP_GENERATION_LIMIT / MAP_BLOCKSIZE
	|| p.Z >  MAP_GENERATION_LIMIT / MAP_BLOCKSIZE);
}

/*
	Returns the position of the block where the node is located
*/
inline v3s16 getNodeBlockPos(const v3s16 &p)
{
	return v3s16(p.X >> MAP_BLOCKP, p.Y >> MAP_BLOCKP, p.Z >> MAP_BLOCKP);
/*
	return getContainerPos(p, MAP_BLOCKSIZE);
*/
}

inline void getNodeBlockPosWithOffset(const v3s16 &p, v3s16 &block, v3s16 &offset)
{
	getContainerPosWithOffset(p, MAP_BLOCKSIZE, block, offset);
}

inline void getNodeSectorPosWithOffset(const v2s16 &p, v2s16 &block, v2s16 &offset)
{
	getContainerPosWithOffset(p, MAP_BLOCKSIZE, block, offset);
}

/*
	Get a quick string to describe what a block actually contains
*/
std::string analyze_block(MapBlock *block);

//typedef std::shared_ptr<MapBlock> MapBlockP;
typedef MapBlock * MapBlockP;

#endif

