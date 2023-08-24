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

#pragma once

#include <atomic>
#include <cstdint>
#include <mutex>
#include <set>
#include "irr_v3d.h"
#include "mapnode.h"
#include "exceptions.h"
#include "constants.h"
#include "staticobject.h"
#include "nodemetadata.h"
#include "nodetimer.h"
#include "modifiedstate.h"
#include "util/numeric.h" // getContainerPos
#include "threading/lock.h"
#include "settings.h"
#include "mapgen/mapgen.h"

class Map;
class NodeMetadataList;
class IGameDef;
class MapBlockMesh;
class VoxelManipulator;
class Circuit;
class ServerEnvironment;
struct ActiveABM;

#define BLOCK_TIMESTAMP_UNDEFINED 0xffffffff

// fm:
static MapNode ignoreNode{CONTENT_IGNORE};

struct abm_trigger_one {
	ActiveABM * abm;
	v3pos_t pos;
	content_t content;
	u32 active_object_count;
	u32 active_object_count_wider;
	v3pos_t neighbor_pos;
	bool activate;
};

////
//// MapBlock modified reason flags
////

#define MOD_REASON_INITIAL                   (1 << 0)
#define MOD_REASON_REALLOCATE                (1 << 1)
#define MOD_REASON_SET_IS_UNDERGROUND        (1 << 2)
#define MOD_REASON_SET_LIGHTING_COMPLETE     (1 << 3)
#define MOD_REASON_SET_GENERATED             (1 << 4)
#define MOD_REASON_SET_NODE                  (1 << 5)
#define MOD_REASON_SET_NODE_NO_CHECK         (1 << 6)
#define MOD_REASON_SET_TIMESTAMP             (1 << 7)
#define MOD_REASON_REPORT_META_CHANGE        (1 << 8)
#define MOD_REASON_CLEAR_ALL_OBJECTS         (1 << 9)
#define MOD_REASON_BLOCK_EXPIRED             (1 << 10)
#define MOD_REASON_ADD_ACTIVE_OBJECT_RAW     (1 << 11)
#define MOD_REASON_REMOVE_OBJECTS_REMOVE     (1 << 12)
#define MOD_REASON_REMOVE_OBJECTS_DEACTIVATE (1 << 13)
#define MOD_REASON_TOO_MANY_OBJECTS          (1 << 14)
#define MOD_REASON_STATIC_DATA_ADDED         (1 << 15)
#define MOD_REASON_STATIC_DATA_REMOVED       (1 << 16)
#define MOD_REASON_STATIC_DATA_CHANGED       (1 << 17)
#define MOD_REASON_EXPIRE_DAYNIGHTDIFF       (1 << 18)
#define MOD_REASON_VMANIP                    (1 << 19)
#define MOD_REASON_UNKNOWN                   (1 << 20)

////
//// MapBlock itself
////

class MapBlock
: public locker<>
{
public:
	MapBlock(Map *parent, v3s16 pos, IGameDef *gamedef);
	~MapBlock();

	/*virtual u16 nodeContainerId() const
	{
		return NODECONTAINER_ID_MAPBLOCK;
	}*/

	Map *getParent()
	{
		return m_parent;
	}

	// Any server-modding code can "delete" arbitrary blocks (i.e. with
	// core.delete_area), which makes them orphan. Avoid using orphan blocks for
	// anything.
	bool isOrphan() const
	{
		return !m_parent;
	}

	void makeOrphan()
	{
		m_parent = nullptr;
	}

	void reallocate()
	{
		auto lock = lock_unique_rec();
#if defined(__GNUC__) && !defined(__clang__)
#pragma GCC diagnostic push
#if __GNUC__ > 7
#pragma GCC diagnostic ignored "-Wclass-memaccess"
#endif
#endif
		if constexpr(!CONTENT_IGNORE) {
			memset(data, 0, nodecount * sizeof(MapNode));
#if defined(__GNUC__) && !defined(__clang__)
#pragma GCC diagnostic pop
#endif

		} else
		for (u32 i = 0; i < nodecount; i++)
			data[i] = ignoreNode;

		//raiseModified(MOD_STATE_WRITE_NEEDED, MOD_REASON_REALLOCATE);
	}

	/*
		Flags
	*/

	enum modified_light {modified_light_no = 0, modified_light_yes};
	void raiseModified(u32 mod, modified_light light = modified_light_no, bool important = false);
	
	MapNode* getData()
	{
		return data;
	}

	////
	//// Modification tracking methods
	////
	void raiseModified(u32 mod, u32 reason, bool important = false)
	{
		raiseModified(mod, modified_light_no, important);
#ifdef WTFdebug
		if (mod > m_modified) {
			m_modified = mod;
			m_modified_reason = reason;
			if (m_modified >= MOD_STATE_WRITE_AT_UNLOAD)
				m_disk_timestamp = m_timestamp;
		} else if (mod == m_modified) {
			m_modified_reason |= reason;
		}
#endif
		if (mod == MOD_STATE_WRITE_NEEDED)
			contents_cached = false;
	}

	inline u32 getModified()
	{
		return m_modified;
	}

	std::string getModifiedReasonString();

	inline void resetModified()
	{
		m_modified = MOD_STATE_CLEAN;
		m_modified_reason = 0;
	}

	////
	//// Flags
	////

	// is_underground getter/setter
	inline bool getIsUnderground()
	{
		return is_underground;
	}

	inline void setIsUnderground(bool a_is_underground)
	{
		is_underground = a_is_underground;
/*
		raiseModified(MOD_STATE_WRITE_NEEDED, MOD_REASON_SET_IS_UNDERGROUND);
*/
	}

	inline void setLightingComplete(u16 newflags)
	{
/*
		if (newflags != m_lighting_complete) {
*/
			m_lighting_complete = newflags;
/*
			raiseModified(MOD_STATE_WRITE_NEEDED, MOD_REASON_SET_LIGHTING_COMPLETE);
		}
*/
	}

	inline u16 getLightingComplete()
	{
		return m_lighting_complete;
	}

	inline void setLightingComplete(LightBank bank, u8 direction,
		bool is_complete)
	{
		assert(direction >= 0 && direction <= 5);
		if (bank == LIGHTBANK_NIGHT) {
			direction += 6;
		}
		u16 newflags = m_lighting_complete;
		if (is_complete) {
			newflags |= 1 << direction;
		} else {
			newflags &= ~(1 << direction);
		}
		setLightingComplete(newflags);
	}

	inline bool isLightingComplete(LightBank bank, u8 direction)
	{
		assert(direction >= 0 && direction <= 5);
		if (bank == LIGHTBANK_NIGHT) {
			direction += 6;
		}
		return (m_lighting_complete & (1 << direction)) != 0;
	}

	inline void setLightingExpired(bool expired)
	{
		m_lighting_expired = expired;
	}

	inline bool getLightingExpired() const
	{
		return m_lighting_expired;
	}


	inline bool isGenerated()
	{
		return m_generated;
	}

	inline void setGenerated(bool b)
	{
		if (b != m_generated) {
/*
			raiseModified(MOD_STATE_WRITE_NEEDED, MOD_REASON_SET_GENERATED);
*/
			m_generated = b;
		}
	}

	////
	//// Position stuff
	////

	inline v3bpos_t getPos() const
	{
		return m_pos;
	}

	inline v3s16 getPosRelative()
	{
		return m_pos_relative;
	}

	inline core::aabbox3d<s16> getBox()
	{
		return core::aabbox3d<s16>(getPosRelative(),
				getPosRelative()
				+ v3s16(MAP_BLOCKSIZE, MAP_BLOCKSIZE, MAP_BLOCKSIZE)
				- v3s16(1,1,1));
	}

	////
	//// Regular MapNode get-setters
	////

	inline bool isValidPosition(s16 x, s16 y, s16 z)
	{
		return x >= 0 && x < MAP_BLOCKSIZE
			&& y >= 0 && y < MAP_BLOCKSIZE
			&& z >= 0 && z < MAP_BLOCKSIZE;
	}

	inline bool isValidPosition(v3s16 p)
	{
		return isValidPosition(p.X, p.Y, p.Z);
	}

	inline MapNode getNode(v3pos_t p, bool *valid_position)
	{
		*valid_position = isValidPosition(p.X, p.Y, p.Z);

		if (!*valid_position)
			return ignoreNode;

		auto lock = lock_shared_rec();
		return data[p.Z * zstride + p.Y * ystride + p.X];
	}

	MapNode getNodeNoEx(v3pos_t p);

	MapNode getNode(v3pos_t p)
	{
		return getNodeNoEx(p);
	}

	MapNode getNodeTry(v3pos_t p)
	{
		auto lock = try_lock_shared_rec();
		if (!lock->owns_lock())
			return ignoreNode;
		return getNodeNoLock(p);
/*
		bool is_valid;
		return getNode(p.X, p.Y, p.Z, &is_valid);
*/
	}

/*
	inline void setNode(s16 x, s16 y, s16 z, MapNode n)
	{
		if (!isValidPosition(x, y, z))
			throw InvalidPositionException();

		data[z * zstride + y * ystride + x] = n;
		raiseModified(MOD_STATE_WRITE_NEEDED, MOD_REASON_SET_NODE);
	}
*/

	void setNode(v3pos_t p, MapNode& n);

	MapNode getNodeNoLock(v3pos_t p)
	{
		return data[p.Z*zstride + p.Y*ystride + p.X];
	}

	inline void setNodeNoLock(v3pos_t p, MapNode n, bool important = false)
	{
		data[p.Z * zstride + p.Y * ystride + p.X] = n;
		raiseModified(MOD_STATE_WRITE_NEEDED, MOD_REASON_SET_NODE_NO_CHECK, important);
	}

	////
	//// Non-checking variants of the above
	////

	inline MapNode getNodeNoCheck(s16 x, s16 y, s16 z)
	{
		auto lock = lock_shared_rec();
		return data[z * zstride + y * ystride + x];
	}

	inline MapNode getNodeNoCheck(v3s16 p)
	{
		return getNodeNoCheck(p.X, p.Y, p.Z);
	}

	inline void setNodeNoCheck(s16 x, s16 y, s16 z, MapNode n)
	{
        auto lock = lock_unique_rec();

		data[z * zstride + y * ystride + x] = n;
		raiseModified(MOD_STATE_WRITE_NEEDED, MOD_REASON_SET_NODE_NO_CHECK);
	}

	inline void setNodeNoCheck(v3pos_t p, MapNode n, bool important = false)
	{
		auto lock = lock_unique_rec();

		data[p.Z * zstride + p.Y * ystride + p.X] = n;
		raiseModified(MOD_STATE_WRITE_NEEDED, MOD_REASON_SET_NODE_NO_CHECK, important);
	}

	// These functions consult the parent container if the position
	// is not valid on this MapBlock.
	bool isValidPositionParent(v3s16 p);
	MapNode getNodeParent(v3s16 p, bool *is_valid_position = NULL);

	// Copies data to VoxelManipulator to getPosRelative()
	void copyTo(VoxelManipulator &dst);

	// Copies data from VoxelManipulator getPosRelative()
	void copyFrom(VoxelManipulator &dst);

	// Update day-night lighting difference flag.
	// Sets m_day_night_differs to appropriate value.
	// These methods don't care about neighboring blocks.
	void actuallyUpdateDayNightDiff();

	// Call this to schedule what the previous function does to be done
	// when the value is actually needed.
	void expireDayNightDiff();

	inline bool getDayNightDiff()
	{
		if (m_day_night_differs_expired)
			actuallyUpdateDayNightDiff();
		return m_day_night_differs;
	}

	bool onObjectsActivation();
	bool saveStaticObject(u16 id, const StaticObject &obj, u32 reason);

	void step(float dtime, const std::function<bool(v3s16, MapNode, f32)> &on_timer_cb);

	////
	//// Timestamp (see m_timestamp)
	////

	// NOTE: BLOCK_TIMESTAMP_UNDEFINED=0xffffffff means there is no timestamp.

	inline void setTimestamp(u32 time)
	{
		m_timestamp = time;
		raiseModified(MOD_STATE_WRITE_AT_UNLOAD, MOD_REASON_SET_TIMESTAMP);
	}

	inline void setTimestampNoChangedFlag(u32 time)
	{
		m_timestamp = time;
	}

	inline u32 getTimestamp()
	{
		return m_timestamp;
	}

	inline u32 getDiskTimestamp()
	{
		return m_disk_timestamp;
	}

	////
	//// Usage timer (see m_usage_timer)
	////

	inline void resetUsageTimer()
	{
		std::lock_guard<std::mutex> lock(m_usage_timer_mutex);
		m_usage_timer = 0;
		usage_timer_multiplier = 1;
	}

	void incrementUsageTimer(float dtime);

	inline float getUsageTimer()
	{
		std::lock_guard<std::mutex> lock(m_usage_timer_mutex);
		return m_usage_timer;
	}

	////
	//// Reference counting (see m_refcount)
	////

	inline void refGrab()
	{
		m_refcount++;
	}

	inline void refDrop()
	{
		m_refcount--;
	}

	inline int refGet()
	{
		return m_refcount;
	}

	////
	//// Node Timers
	////

	inline NodeTimer getNodeTimer(v3s16 p)
	{
		return m_node_timers.get(p);
	}

	inline void removeNodeTimer(v3s16 p)
	{
		m_node_timers.remove(p);
	}

	inline void setNodeTimer(const NodeTimer &t)
	{
		m_node_timers.set(t);
	}

	inline void clearNodeTimers()
	{
		m_node_timers.clear();
	}

	////
	//// Serialization
	///

	// These don't write or read version by itself
	// Set disk to true for on-disk format, false for over-the-network format
	// Precondition: version >= SER_FMT_VER_LOWEST_WRITE
	void serialize(std::ostream &result, u8 version, bool disk, int compression_level, bool use_content_only = false);
	// If disk == true: In addition to doing other things, will add
	// unknown blocks from id-name mapping to wndef
	bool deSerialize(std::istream &is, u8 version, bool disk);

	void serializeNetworkSpecific(std::ostream &os);
	void deSerializeNetworkSpecific(std::istream &is);


//fm:
	void pushElementsToCircuit(Circuit* circuit);

	typedef std::shared_ptr<MapBlockMesh> mesh_type;

#if BUILD_CLIENT // Only on client
	MapBlock::mesh_type getMesh(int step);
	void setMesh(MapBlock::mesh_type & rmesh);
#endif
//===


	bool storeActiveObject(u16 id);
	// clearObject and return removed objects count
	u32 clearObjects();

private:
	/*
		Private methods
	*/

	void deSerialize_pre22(std::istream &is, u8 version, bool disk);

public:
	/*
		Public member variables
	*/

#if BUILD_CLIENT // Only on client
private:
	mesh_type mesh = nullptr;
	mesh_type mesh2 = nullptr, mesh4 = nullptr, mesh8 = nullptr, mesh16 = nullptr;
	int16_t m_mesh_size_16 = -1, m_mesh_size_8 = -1, m_mesh_size_4 = -1, m_mesh_size_2 = -1, m_mesh_size = -1;
public:	
	mesh_type mesh_old = nullptr;
	//std::atomic_int mesh_size {-1};
#endif

	NodeMetadataList m_node_metadata;
	StaticObjectList m_static_objects;
	
	std::atomic_short heat {0};
	std::atomic_short humidity {0};
	std::atomic_short heat_add {0};
	std::atomic_short humidity_add {0};
	std::atomic_ulong heat_last_update {0};
	u32 humidity_last_update = 0;
	float m_uptime_timer_last = 0;
	std::atomic_short usage_timer_multiplier {1};

	// Last really changed time (need send to client)
	std::atomic_uint m_changed_timestamp {0};
	u32 m_next_analyze_timestamp = 0;
	typedef std::list<abm_trigger_one> abm_triggers_type;
	std::unique_ptr<abm_triggers_type> abm_triggers;
	std::mutex abm_triggers_mutex;
	void abmTriggersRun(ServerEnvironment * m_env, u32 time, bool activate = false);
	u32 m_abm_timestamp = 0;;

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
	std::atomic<content_t> content_only = CONTENT_IGNORE;
	u8 content_only_param1 = 0, content_only_param2 = 0;
	bool analyzeContent();
	//std::atomic_short lighting_broken {0};
	void setMeshSize(int step, int32_t size);
	int32_t getMeshSize(int step);

	static const u32 ystride = MAP_BLOCKSIZE;
	static const u32 zstride = MAP_BLOCKSIZE * MAP_BLOCKSIZE;

	static const u32 nodecount = MAP_BLOCKSIZE * MAP_BLOCKSIZE * MAP_BLOCKSIZE;

	//// ABM optimizations ////
	// Cache of content types
	std::unordered_set<content_t> contents;
	// True if content types are cached
	std::atomic_bool contents_cached {false};
	// True if we never want to cache content types for this block
	bool do_not_cache_contents = false;
	// marks the sides which are opaque: 00+Z-Z+Y-Y+X-X
	u8 solid_sides {0};

private:
	/*
		Private member variables
	*/

	// NOTE: Lots of things rely on this being the Map
	Map *m_parent;
	// Position in blocks on parent
	v3s16 m_pos;

	/* This is the precalculated m_pos_relative value
	* This caches the value, improving performance by removing 3 s16 multiplications
	* at runtime on each getPosRelative call
	* For a 5 minutes runtime with valgrind this removes 3 * 19M s16 multiplications
	* The gain can be estimated in Release Build to 3 * 100M multiply operations for 5 mins
	*/
	v3s16 m_pos_relative;

	IGameDef *m_gamedef;

	/*
		- On the server, this is used for telling whether the
		  block has been modified from the one on disk.
		- On the client, this is used for nothing.
	*/
	std::atomic_uint8_t m_modified = MOD_STATE_CLEAN;
	u32 m_modified_reason = MOD_REASON_INITIAL;

	/*
		When propagating sunlight and the above block doesn't exist,
		sunlight is assumed if this is false.

		In practice this is set to true if the block is completely
		undeground with nothing visible above the ground except
		caves.
	*/
	std::atomic_bool is_underground = false;

	/*
		Set to true if changes has been made that make the old lighting
		values wrong but the lighting hasn't been actually updated.

		If this is false, lighting is exactly right.
		If this is true, lighting might be wrong or right.
	*/

	std::atomic_bool m_lighting_expired {false};

	/*!
	 * Each bit indicates if light spreading was finished
	 * in a direction. (Because the neighbor could also be unloaded.)
	 * Bits (most significant first):
	 * nothing,  nothing,  nothing,  nothing,
	 * night X-, night Y-, night Z-, night Z+, night Y+, night X+,
	 * day X-,   day Y-,   day Z-,   day Z+,   day Y+,   day X+.
	*/
	std::atomic_short m_lighting_complete {static_cast<short>(0xFFFF)};

	// Whether day and night lighting differs
	bool m_day_night_differs = false;
	std::atomic_bool m_day_night_differs_expired {true};

	std::atomic_bool m_generated {false};

	/*
		When block is removed from active blocks, this is set to gametime.
		Value BLOCK_TIMESTAMP_UNDEFINED=0xffffffff means there is no timestamp.
	*/
	std::atomic_uint m_timestamp {BLOCK_TIMESTAMP_UNDEFINED};
	// The on-disk (or to-be on-disk) timestamp value
	std::atomic_uint m_disk_timestamp = BLOCK_TIMESTAMP_UNDEFINED;

	/*
		When the block is accessed, this is set to 0.
		Map will unload the block when this reaches a timeout.
	*/
	std::mutex m_usage_timer_mutex;
	float m_usage_timer = 0;

	/*
		Reference count; currently used for determining if this block is in
		the list of blocks to be drawn.
	*/
	std::atomic_int m_refcount {0};

	MapNode data[nodecount];

public:
	NodeTimerList m_node_timers;
};

typedef std::vector<MapBlock*> MapBlockVect;

inline bool objectpos_over_limit(v3f p)
{
	const float max_limit_bs = (MAX_MAP_GENERATION_LIMIT + 0.5f) * BS;
	return p.X < -max_limit_bs ||
		p.X >  max_limit_bs ||
		p.Y < -max_limit_bs ||
		p.Y >  max_limit_bs ||
		p.Z < -max_limit_bs ||
		p.Z >  max_limit_bs;
}

inline bool blockpos_over_max_limit(v3s16 p)
{
	const s16 max_limit_bp = MAX_MAP_GENERATION_LIMIT / MAP_BLOCKSIZE;
	return p.X < -max_limit_bp ||
		p.X >  max_limit_bp ||
		p.Y < -max_limit_bp ||
		p.Y >  max_limit_bp ||
		p.Z < -max_limit_bp ||
		p.Z >  max_limit_bp;
}

/*
	Returns the position of the block where the node is located
*/
inline v3s16 getNodeBlockPos(v3s16 p)
{
	return v3bpos_t(p.X >> MAP_BLOCKP, p.Y >> MAP_BLOCKP, p.Z >> MAP_BLOCKP);
/*
	return getContainerPos(p, MAP_BLOCKSIZE);
*/
}

inline void getNodeBlockPosWithOffset(v3s16 p, v3s16 &block, v3s16 &offset)
{
	getContainerPosWithOffset(p, MAP_BLOCKSIZE, block, offset);
}

/*
	Get a quick string to describe what a block actually contains
*/
std::string analyze_block(MapBlock *block);

//typedef std::shared_ptr<MapBlock> MapBlockP;
typedef MapBlock * MapBlockP;

