// Luanti
// SPDX-License-Identifier: LGPL-2.1-or-later
// Copyright (C) 2013 celeron55, Perttu Ahola <celeron55@gmail.com>

#pragma once

#include "config.h"

#include <atomic>
#include <vector>
#include "irr_v3d.h"
#include "mapnode.h"
#include "exceptions.h"
#include "constants.h"
#include "staticobject.h"
#include "nodemetadata.h"
#include "nodetimer.h"
#include "modifiedstate.h"
#include "util/numeric.h" // getContainerPos
#include "settings.h"

class Circuit;
class ServerEnvironment;
struct ActiveABM;

class Map;
class NodeMetadataList;
class IGameDef;
class MapBlockMesh;
class VoxelManipulator;

#define BLOCK_TIMESTAMP_UNDEFINED 0xffffffff

// fm:
static MapNode ignoreNode{CONTENT_IGNORE};
constexpr auto LODMESH_STEP_MAX {8}; // 4+1
constexpr auto FARMESH_STEP_MAX {16};


struct abm_trigger_one {
	ActiveABM * abm;
	v3pos_t pos;
	content_t content;
	u32 active_object_count;
	u32 active_object_count_wider;
	v3pos_t neighbor_pos;
	uint8_t activate;
};

////
//// MapBlock modified reason flags
////

enum ModReason : u32 {
	MOD_REASON_REALLOCATE                 = 1 << 0,
	MOD_REASON_SET_IS_UNDERGROUND         = 1 << 1,
	MOD_REASON_SET_LIGHTING_COMPLETE      = 1 << 2,
	MOD_REASON_SET_GENERATED              = 1 << 3,
	MOD_REASON_SET_NODE                   = 1 << 4,
	MOD_REASON_SET_TIMESTAMP              = 1 << 5,
	MOD_REASON_REPORT_META_CHANGE         = 1 << 6,
	MOD_REASON_CLEAR_ALL_OBJECTS          = 1 << 7,
	MOD_REASON_BLOCK_EXPIRED              = 1 << 8,
	MOD_REASON_ADD_ACTIVE_OBJECT_RAW      = 1 << 9,
	MOD_REASON_REMOVE_OBJECTS_REMOVE      = 1 << 10,
	MOD_REASON_REMOVE_OBJECTS_DEACTIVATE  = 1 << 11,
	MOD_REASON_TOO_MANY_OBJECTS           = 1 << 12,
	MOD_REASON_STATIC_DATA_ADDED          = 1 << 13,
	MOD_REASON_STATIC_DATA_REMOVED        = 1 << 14,
	MOD_REASON_STATIC_DATA_CHANGED        = 1 << 15,
	MOD_REASON_EXPIRE_IS_AIR              = 1 << 16,
	MOD_REASON_VMANIP                     = 1 << 17,
	MOD_REASON_UNKNOWN                    = 1 << 18,
};

////
//// MapBlock itself
////

class MapBlock
: public locker<> // TODO: find all deadlocks and change to shared_locker
{
public:
	MapBlock(v3s16 pos, IGameDef *gamedef);
	~MapBlock();

	// Any server-modding code can "delete" arbitrary blocks (i.e. with
	// core.delete_area), which makes them orphan. Avoid using orphan blocks for
	// anything.
	bool isOrphan() const
	{
		return m_orphan;
	}

	void makeOrphan()
	{
		m_orphan = true;
	}

	void reallocate()
	{
		const auto lock = lock_unique_rec();
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

	MapNode* getData()
	{
		return data;
	}

	////
	//// Modification tracking methods
	////
	void raiseModified(u32 mod, u32 reason=MOD_REASON_UNKNOWN, bool important = true)
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
			contents.clear();
	}

	inline u32 getModified()
	{
		return m_modified;
	}

/*
	inline u32 getModifiedReason()
	{
		return m_modified_reason;
	}
*/

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
			raiseModified(MOD_STATE_WRITE_AT_UNLOAD, MOD_REASON_SET_LIGHTING_COMPLETE);
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
		assert(direction <= 5);
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
		assert(direction <= 5);
		if (bank == LIGHTBANK_NIGHT) {
			direction += 6;
		}
		return (m_lighting_complete & (1 << direction)) != 0;
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

	inline core::aabbox3d<s16> getBox() {
		return getBox(getPosRelative());
	}

	static inline core::aabbox3d<s16> getBox(const v3s16 &pos_relative)
	{
		return core::aabbox3d<s16>(pos_relative,
				pos_relative
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

		const auto lock = lock_shared_rec();
		return data[p.Z * zstride + p.Y * ystride + p.X];
	}

	MapNode getNodeNoEx(v3pos_t p);

	MapNode getNode(v3pos_t p)
	{
		return getNodeNoEx(p);
	}

/*
	inline void setNode(s16 x, s16 y, s16 z, MapNode n)
	{
		if (!isValidPosition(x, y, z))
			throw InvalidPositionException();

		data[z * zstride + y * ystride + x] = n;
		raiseModified(MOD_STATE_WRITE_NEEDED, MOD_REASON_SET_NODE);
	}

	inline void setNode(v3s16 p, MapNode n)
	{
		setNode(p.X, p.Y, p.Z, n);
	}
*/

	void setNode(const v3pos_t& p, const MapNode& n, bool important = false);

	////
	//// Non-checking variants of the above
	////

	inline MapNode getNodeNoCheck(s16 x, s16 y, s16 z)
	{
		const auto lock = lock_shared_rec();
		return data[z * zstride + y * ystride + x];
	}

	inline MapNode getNodeNoCheck(v3s16 p)
	{
		return getNodeNoCheck(p.X, p.Y, p.Z);
	}

	inline void setNodeNoCheck(s16 x, s16 y, s16 z, MapNode n)
	{
        const auto lock = lock_unique_rec();

		data[z * zstride + y * ystride + x] = n;
		raiseModified(MOD_STATE_WRITE_NEEDED, MOD_REASON_SET_NODE, false);
	}

	inline void setNodeNoCheck(v3pos_t p, MapNode n, bool important = false)
	{
		const auto lock = lock_unique_rec();

		data[p.Z * zstride + p.Y * ystride + p.X] = n;
		raiseModified(MOD_STATE_WRITE_NEEDED, MOD_REASON_SET_NODE, important);
	}

	// Copies data to VoxelManipulator to getPosRelative()
	void copyTo(VoxelManipulator &dst);

	// Copies data from VoxelManipulator to getPosRelative()
	void copyFrom(const VoxelManipulator &src);

	// Update is air flag.
	// Sets m_is_air to appropriate value.
	void actuallyUpdateIsAir();

	// Call this to schedule what the previous function does to be done
	// when the value is actually needed.
	void expireIsAirCache();

	inline bool isAir()
	{
		if (m_is_air_expired)
			actuallyUpdateIsAir();
		return m_is_air;
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
		assert(m_refcount < SHRT_MAX);
		m_refcount++;
	}

	inline void refDrop()
	{
		assert(m_refcount > 0);
		m_refcount--;
	}

	inline short refGet()
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
	/*
		Flags
	*/

	enum modified_light {modified_light_no = 0, modified_light_yes};
	void raiseModified(u32 mod, modified_light light = modified_light_no, bool important = false);


	void pushElementsToCircuit(Circuit* circuit);

	void fill(const MapNode & n) {
		for (u32 i = 0; i < nodecount; ++i)
			data[i] = n;
	}

	using mesh_type = std::shared_ptr<MapBlockMesh>;

#if CHECK_CLIENT_BUILD() // Only on client
	const MapBlock::mesh_type getLodMesh(block_step_t step, bool allow_other = false);
	void setLodMesh(const MapBlock::mesh_type &rmesh);
	const MapBlock::mesh_type getFarMesh(block_step_t step);
	void setFarMesh(const MapBlock::mesh_type &rmesh, block_step_t step);
	std::mutex far_mutex;
	uint32_t mesh_requested_timestamp{};
	block_step_t mesh_requested_step{};

private:
	std::array<MapBlock::mesh_type, LODMESH_STEP_MAX + 1> m_lod_mesh;
	std::array<MapBlock::mesh_type, FARMESH_STEP_MAX + 1> m_far_mesh;
	MapBlock::mesh_type delete_mesh;

public:
#endif

	block_step_t far_step{};
	uint32_t far_make_mesh_timestamp{static_cast<uint32_t>(-1)};
	std::atomic_uint32_t far_iteration{};
	std::atomic_bool creating_far_mesh{};
	std::atomic_short heat{};
	std::atomic_short humidity{};
	std::atomic_short heat_add{};
	std::atomic_short humidity_add{};
	std::atomic_ulong heat_last_update{};
	std::atomic_uint32_t humidity_last_update{};
	float m_uptime_timer_last{};
	std::atomic_short usage_timer_multiplier{1};

	// Last really changed time (need send to client)
	std::atomic_uint m_changed_timestamp{};
	uint32_t m_next_analyze_timestamp{};
	typedef std::list<abm_trigger_one> abm_triggers_type;
	std::unique_ptr<abm_triggers_type> abm_triggers;
	std::mutex abm_triggers_mutex;
	size_t abmTriggersRun(ServerEnvironment *m_env, u32 time, uint8_t activate = 0);
	uint32_t m_abm_timestamp{};

	u32 getActualTimestamp()
	{
		u32 block_timestamp = 0;
		if (m_changed_timestamp && m_changed_timestamp != BLOCK_TIMESTAMP_UNDEFINED) {
			block_timestamp = m_changed_timestamp;
		} else if (m_disk_timestamp && m_disk_timestamp != BLOCK_TIMESTAMP_UNDEFINED) {
			block_timestamp = m_disk_timestamp;
		}
		return block_timestamp;
	}

	// Set to content type of a node if the block consists solely of nodes of one type, otherwise set to CONTENT_IGNORE
	std::atomic<content_t> content_only{CONTENT_IGNORE};
	u8 content_only_param1{}, content_only_param2{};
	bool analyzeContent();
	std::mutex m_usage_timer_mutex;

	/*
		Set to true if changes has been made that make the old lighting
		values wrong but the lighting hasn't been actually updated.

		If this is false, lighting is exactly right.
		If this is true, lighting might be wrong or right.
	*/

	std::atomic_bool m_lighting_expired {false};

	MapNode getNodeTry(const v3pos_t &p);

	MapNode &getNodeRef(const v3pos_t &p);

	MapNode &getNodeNoLock(v3pos_t p)
	{
		return data[p.Z*zstride + p.Y*ystride + p.X];
	}

	void setNodeNoLock(v3pos_t p, MapNode n, bool important = false);

	//===

	bool storeActiveObject(u16 id);
	// clearObject and return removed objects count
	u32 clearObjects();

	static const u32 ystride = MAP_BLOCKSIZE;
	static const u32 zstride = MAP_BLOCKSIZE * MAP_BLOCKSIZE;

	static const u32 nodecount = MAP_BLOCKSIZE * MAP_BLOCKSIZE * MAP_BLOCKSIZE;

private:
	/*
		Private methods
	*/

	void deSerialize_pre22(std::istream &is, u8 version, bool disk);

	/*
	 * PLEASE NOTE: When adding something here be mindful of position and size
	 * of member variables! This is also the reason for the weird public-private
	 * interleaving.
	 * If in doubt consult `pahole` to see the effects.
	 */

public:
#if CHECK_CLIENT_BUILD() // Only on client
/*
	MapBlockMesh *mesh = nullptr;
*/
	// marks the sides which are opaque: 00+Z-Z+Y-Y+X-X
	u8 solid_sides = 0;
#endif

private:
	// see isOrphan()
	bool m_orphan = false;

	// Position in blocks on parent
	v3s16 m_pos;

	/* Precalculated m_pos_relative value
	 * This caches the value, improving performance by removing 3 s16 multiplications
	 * at runtime on each getPosRelative call.
	 * For a 5 minutes runtime with valgrind this removes 3 * 19M s16 multiplications.
	 * The gain can be estimated in Release Build to 3 * 100M multiply operations for 5 mins.
	 */
	v3s16 m_pos_relative;

	/*
		Reference count; currently used for determining if this block is in
		the list of blocks to be drawn.
	*/
	std::atomic_short m_refcount = 0;

	/*
	 * Note that this is not an inline array because that has implications for
	 * heap fragmentation (the array is exactly 16K), CPU caches and/or
	 * optimizability of algorithms working on this array.
	 */
	MapNode *const data; // of `nodecount` elements

	// provides the item and node definitions
	IGameDef *m_gamedef;

	/*
		When the block is accessed, this is set to 0.
		Map will unload the block when this reaches a timeout.
	*/
	float m_usage_timer = 0;

public:
	//// ABM optimizations ////
	// True if we never want to cache content types for this block
	bool do_not_cache_contents = false;
	// Cache of content types
	// This is actually a set but for the small sizes we have a vector should be
	// more efficient.
	// Can be empty, in which case nothing was cached yet.
	std::vector<content_t> contents;

private:
	// Whether day and night lighting differs
	bool m_is_air = false;
	bool m_is_air_expired = true;

	/*
		- On the server, this is used for telling whether the
		  block has been modified from the one on disk.
		- On the client, this is used for nothing.
	*/
	std::atomic_uint16_t m_modified = MOD_STATE_CLEAN;
	u32 m_modified_reason = 0;

	/*
		When block is removed from active blocks, this is set to gametime.
		Value BLOCK_TIMESTAMP_UNDEFINED=0xffffffff means there is no timestamp.
	*/
	std::atomic_uint32_t m_timestamp = BLOCK_TIMESTAMP_UNDEFINED;
	// The on-disk (or to-be on-disk) timestamp value
	std::atomic_uint32_t m_disk_timestamp = BLOCK_TIMESTAMP_UNDEFINED;

	/*!
	 * Each bit indicates if light spreading was finished
	 * in a direction. (Because the neighbor could also be unloaded.)
	 * Bits (most significant first):
	 * nothing,  nothing,  nothing,  nothing,
	 * night X-, night Y-, night Z-, night Z+, night Y+, night X+,
	 * day X-,   day Y-,   day Z-,   day Z+,   day Y+,   day X+.
	*/
	std::atomic_short m_lighting_complete {static_cast<short>(0xFFFF)};

	// Whether mapgen has generated the content of this block (persisted)
	std::atomic_bool m_generated = false;

	/*
		When propagating sunlight and the above block doesn't exist,
		sunlight is assumed if this is false.

		In practice this is set to true if the block is completely
		undeground with nothing visible above the ground except
		caves.
	*/
	std::atomic_bool is_underground = false;

public:
	NodeMetadataList m_node_metadata;
	StaticObjectList m_static_objects;

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

using MapBlockPtr = std::shared_ptr<MapBlock>;
// using MapBlockPtr = MapBlock *;

