// Luanti
// SPDX-License-Identifier: LGPL-2.1-or-later
// Copyright (C) 2010-2017 celeron55, Perttu Ahola <celeron55@gmail.com>

#pragma once

#include "server/fm_key_value_cached.h"
//#include "server/abmhandler.h"
#include "threading/concurrent_set.h"

#include <set>
#include <utility>

#include "activeobject.h"
#include "environment.h"
#include "servermap.h"
#include "settings.h"
#include "server/activeobjectmgr.h"
#include "server/blockmodifier.h"
#include "util/numeric.h"
#include <atomic>
#include <cstddef>
#include <cstdint>
#include <mutex>
#include "util/metricsbackend.h"

class IGameDef;
struct GameParams;
class RemotePlayer;
class PlayerDatabase;
class AuthDatabase;
class PlayerSAO;
class ServerEnvironment;
struct StaticObject;
class ServerActiveObject;
class Server;
class ServerScripting;
enum AccessDeniedCode : u8;
typedef u16 session_t;

/*
	List of active blocks, used by ServerEnvironment
*/

class ActiveBlockList
{
public:
	void update(std::vector<PlayerSAO*> &active_players,
		s16 active_block_range,
		s16 active_object_range,
		std::set<v3bpos_t> &blocks_removed,
		std::set<v3bpos_t> &blocks_added,
		std::set<v3bpos_t> &extra_blocks_added);

	bool contains(v3bpos_t p) const {
		return (m_list.find(p) != m_list.end());
	}

	auto size() const {
		return m_list.size();
	}

	void clear() {
		m_list.clear();
	}

	/// @return true if block was newly added
	bool add(v3bpos_t p) {
		if (m_list.insert(p).second) {
			m_abm_list.insert(p);
			return true;
		}
		return false;
	}

	void remove(v3bpos_t p) {
		m_list.erase(p);
		m_abm_list.erase(p);
	}

	// list of all active blocks
	//std::set<v3s16> m_list;
	maybe_concurrent_set<v3pos_t> m_list;
	// list of blocks for ABM processing
	// subset of `m_list` that does not contain view cone affected blocks
	std::set<v3bpos_t> m_abm_list;
	// list of blocks that are always active, not modified by this class
	std::set<v3bpos_t> m_forceloaded_list;
};

/*
	ServerEnvironment::m_on_mapblocks_changed_receiver
*/
struct OnMapblocksChangedReceiver : public MapEventReceiver {
	std::unordered_set<v3bpos_t> modified_blocks;
	bool receiving = false;

	void onMapEditEvent(const MapEditEvent &event) override;
};

/*
	Operation mode for ServerEnvironment::clearObjects()
*/
enum ClearObjectsMode {
	// Load and go through every mapblock, clearing objects
		CLEAR_OBJECTS_MODE_FULL,

	// Clear objects immediately in loaded mapblocks;
	// clear objects in unloaded mapblocks only when the mapblocks are next activated.
		CLEAR_OBJECTS_MODE_QUICK,
};

class ServerEnvironment final : public Environment
{
public:
	ServerEnvironment(std::unique_ptr<ServerMap> map, Server *server, MetricsBackend *mb);
	~ServerEnvironment();

	void init();

	Map & getMap();

	ServerMap & getServerMap();

	//TODO find way to remove this fct!
	ServerScripting* getScriptIface()
	{ return m_script; }

	Server *getGameDef()
	{ return m_server; }

	float getSendRecommendedInterval()
	{ return m_recommended_send_interval; }

	// Save players
	void saveLoadedPlayers(bool force = false);
	void savePlayer(RemotePlayer *player);
	std::unique_ptr<PlayerSAO> loadPlayer(RemotePlayer *player, session_t peer_id);
	void addPlayer(RemotePlayer *player);
	void removePlayer(RemotePlayer *player);
	bool removePlayerFromDatabase(const std::string &name);

	/*
		Save and load time of day and game timer
	*/
	void saveMeta();
	void loadMeta();

	u32 addParticleSpawner(float exptime);
	u32 addParticleSpawner(float exptime, u16 attached_id);
	void deleteParticleSpawner(u32 id, bool remove_from_object = true);

	/*
		External ActiveObject interface
		-------------------------------------------
	*/

	ServerActiveObject* getActiveObject(u16 id, bool removed = false)
	{
		const auto obj = m_ao_manager.getActiveObject(id).get();
		if (!removed && (!obj || obj->isGone()))
			return nullptr;
		return obj;
	}

	/*
		Add an active object to the environment.
		Environment handles deletion of object.
		Object may be deleted by environment immediately.
		If id of object is 0, assigns a free id to it.
		Returns the id of the object.
		Returns 0 if not added and thus deleted.
	*/
	u16 addActiveObject(std::shared_ptr<ServerActiveObject> object);

	void invalidateActiveObjectObserverCaches();

	/*
		Find out what new objects have been added to
		inside a radius around a position
	*/
	void getAddedActiveObjects(PlayerSAO *playersao, s16 radius,
		s16 player_radius,
		const concurrent_set<u16> &current_objects,
		std::vector<u16> &added_objects);

	/*
		Find out what new objects have been removed from
		inside a radius around a position
	*/
	void getRemovedActiveObjects(PlayerSAO *playersao, s16 radius,
		s16 player_radius,
		const concurrent_set<u16> &current_objects,
		std::vector<std::pair<bool /* gone? */, u16>> &removed_objects);

	/*
		Get the next message emitted by some active object.
		Returns false if no messages are available, true otherwise.
	*/
	bool getActiveObjectMessage(ActiveObjectMessage *dest);

	virtual void getSelectedActiveObjects(
		const core::line3d<opos_t> &shootline_on_map,
		std::vector<PointedThing> &objects,
		const std::optional<Pointabilities> &pointabilities
	);

	/*
		Force a block to become active. It will probably be deactivated
		the next time active blocks are re-calculated.
	*/
	void forceActivateBlock(MapBlock *block);

	/*
		{Active,Loading}BlockModifiers
		-------------------------------------------
	*/

	void addActiveBlockModifier(ActiveBlockModifier *abm);
	void addLoadingBlockModifierDef(LoadingBlockModifierDef *lbm);

	/*
		Other stuff
		-------------------------------------------
	*/

	// Script-aware node setters
	bool setNode(v3pos_t p, const MapNode &n, s16 fast = 0, bool important = false);
	bool removeNode(v3pos_t p, s16 fast = 0, bool important = false);
	bool swapNode(v3pos_t p, const MapNode &n, s16 fast = 0);

	// Find the daylight value at pos with a Depth First Search
	u8 findSunlight(v3pos_t pos) const;

	void updateObjectPos(u16 id, v3opos_t pos)
	{
		return m_ao_manager.updateObjectPos(id, pos);
	}

	// Find all active objects inside a radius around a point
	void getObjectsInsideRadius(std::vector<ServerActiveObjectPtr> &objects, const v3opos_t &pos, float radius,
			const std::function<bool(const ServerActiveObjectPtr &obj)> &include_obj_cb)
	{
		return m_ao_manager.getObjectsInsideRadius(pos, radius, objects, include_obj_cb);
	}

	// Find all active objects inside a box
	void getObjectsInArea(std::vector<ServerActiveObjectPtr> &objects, const aabb3o &box,
			const std::function<bool(const ServerActiveObjectPtr &obj)> &include_obj_cb)
	{
		return m_ao_manager.getObjectsInArea(box, objects, include_obj_cb);
	}

	// Clear objects, loading and going through every MapBlock
	void clearObjects(ClearObjectsMode mode);

	// to be called before destructor
	void deactivateBlocksAndObjects();

	// This makes stuff happen
	void step(f32 dtime, double uptime={}, unsigned int max_cycle_ms={});

	u32 getGameTime() const { return m_game_time; }

	void reportMaxLagEstimate(float f) { m_max_lag_estimate = f; }
	float getMaxLagEstimate() const { return m_max_lag_estimate; }

	std::set<v3bpos_t>* getForceloadedBlocks() { return &m_active_blocks.m_forceloaded_list; }

	// Sorted by how ready a mapblock is
	enum BlockStatus {
		BS_UNKNOWN,
		BS_EMERGING,
		BS_LOADED,
		BS_ACTIVE // always highest value
	};
	BlockStatus getBlockStatus(v3bpos_t blockpos);

	// Sets the static object status all the active objects in the specified block
	// This is only really needed for deleting blocks from the map
	void setStaticForActiveObjectsInBlock(v3bpos_t blockpos,
		bool static_exists, v3bpos_t static_block=v3bpos_t(0,0,0));

	RemotePlayer *getPlayer(const session_t peer_id);
	RemotePlayer *getPlayer(const std::string &name, bool match_invalid_peer = false);
	const std::vector<RemotePlayer *> getPlayers() const { return m_players; }
	u32 getPlayerCount() const { return m_players.size(); }

	static bool migratePlayersDatabase(const GameParams &game_params,
			const Settings &cmd_args);

	AuthDatabase *getAuthDatabase() { return m_auth_database; }
	static bool migrateAuthDatabase(const GameParams &game_params,
			const Settings &cmd_args);

// freeminer

public:
	KeyValueStorage &getKeyValueStorage(std::string name = "key_value_storage");
	KeyValueStorage &getPlayerStorage() { return getKeyValueStorage("players"); };
	std::shared_ptr<epixel::ItemSAO> spawnItemActiveObject(const std::string &itemName, v3opos_t pos,
			const ItemStack& items);

	std::shared_ptr<epixel::FallingSAO> spawnFallingActiveObject(const std::string &nodeName, v3opos_t pos,
			const MapNode &n, int fast = 2);
private:

	// is weather active in this environment?
public:
	bool m_use_weather = true;
	bool m_use_weather_biome = true;
	bool m_more_threads = true;
public:
	ABMHandler m_abmhandler;
	uint8_t analyzeBlock(MapBlockPtr block);
private:
	IntervalLimiter m_analyze_blocks_interval;
	IntervalLimiter m_abm_random_interval;
	std::list<v3pos_t> m_abm_random_blocks;
public:
	size_t blockStep(MapBlockPtr block, float dtime = 0, uint8_t activate = 0);
	int analyzeBlocks(float dtime, unsigned int max_cycle_ms);
	u32 m_game_time_start = 0;

public:
	size_t nodeUpdate(const v3pos_t &pos, u8 recursion_limit = 5, u8 fast = 2, bool destroy = false);

private:
	struct nodeUpdatePos
	{
		v3pos_t pos;
		u8 recursion_limit{5};
		int fast{2};
		bool destroy{false};
	};
	size_t nodeUpdateReal(const v3pos_t &pos, u8 recursion_limit = 5, u8 fast = 2, bool destroy = false);
private:
	void handleNodeDrops(const ContentFeatures &f, v3f pos, PlayerSAO* player=NULL);

/*
	void contrib_player_globalstep(RemotePlayer *player, float dtime);
	void contrib_lookupitemtogather(RemotePlayer* player, v3f playerPos,
			Inventory* inv, ServerActiveObject* obj);
*/
	void contrib_globalstep(const float dtime);
	bool checkAttachedNode(v3pos_t pos, MapNode n, const ContentFeatures &f);
/*
	void explodeNode(const v3s16 pos);
*/

	std::deque<nodeUpdatePos> m_nodeupdate_queue;
	std::mutex m_nodeupdate_queue_mutex;
	// Circuit manager
	Circuit m_circuit;
	// Key-value storage
public:
	std::unordered_map<std::string, KeyValueStorage> m_key_value_storage;
private:
	std::vector<u16> objects_to_remove;
	//std::vector<ServerActiveObject*> objects_to_delete;
	//loop breakers
	u32 m_active_objects_last = 0;
	u32 m_active_block_abm_last = 0;
	float m_active_block_abm_dtime = 0;
	float m_active_block_abm_dtime_counter = 0;
	u32 m_active_block_timer_last = 0;
	std::set<v3bpos_t> m_blocks_added;
	u32 m_blocks_added_last = 0;
	u32 m_active_block_analyzed_last = 0;
	std::mutex m_max_lag_estimate_mutex;
public:
	KeyValueCached blocks_with_abm;
	size_t abm_world_last = 0;
	size_t world_merge_last = 0;
//end of freeminer



	/**
	 * called if env_meta.txt doesn't exist (e.g. new world)
	 */
	void loadDefaultMeta();

	static PlayerDatabase *openPlayerDatabase(const std::string &name,
			const std::string &savedir, const Settings &conf);
	static AuthDatabase *openAuthDatabase(const std::string &name,
			const std::string &savedir, const Settings &conf);

	void activateBlock(MapBlock *block);

	/*
		Internal ActiveObject interface
		-------------------------------------------
	*/

	/*
		Add an active object to the environment.

		Called by addActiveObject.

		Object may be deleted by environment immediately.
		If id of object is 0, assigns a free id to it.
		Returns the id of the object.
		Returns 0 if not added and thus deleted.
	*/
	u16 addActiveObjectRaw(std::shared_ptr<ServerActiveObject> object,
			const StaticObject *from_static, u32 dtime_s);

	/*
		Remove all objects that satisfy (isGone() && m_known_by_count==0)
	*/
	void removeRemovedObjects(u32 max_cycle_ms = 1000);

	/*
		Convert stored objects from block to active
	*/
	void activateObjects(MapBlock *block, u32 dtime_s);

	/*
		Convert objects that are not in active blocks to static.

		If m_known_by_count != 0, active object is not deleted, but static
		data is still updated.

		If force_delete is set, active object is deleted nevertheless. It
		shall only be set so in the destructor of the environment.
	*/
	void deactivateFarObjects(bool force_delete);

	/*
		A few helpers used by the three above methods
	*/
	void deleteStaticFromBlock(
			ServerActiveObject *obj, u16 id, u32 mod_reason, bool no_emerge);
	bool saveStaticToBlock(v3bpos_t blockpos, u16 store_id,
			ServerActiveObject *obj, const StaticObject &s_obj, u32 mod_reason);

	void processActiveObjectRemove(ServerActiveObjectPtr obj);

	/*
		Member variables
	*/

	// The map
	std::unique_ptr<ServerMap> m_map;
	// Lua state
	ServerScripting* m_script;
	// Server definition
	Server *m_server;
	// Active Object Manager
	server::ActiveObjectMgr m_ao_manager;
	// on_mapblocks_changed map event receiver
	OnMapblocksChangedReceiver m_on_mapblocks_changed_receiver;
	// Outgoing network message buffer for active objects
public:
	Queue<ActiveObjectMessage> m_active_object_messages;
private:	
	// Some timers
	float m_send_recommended_timer = 0.0f;
	IntervalLimiter m_object_management_interval;
	// List of active blocks
	ActiveBlockList m_active_blocks;
	int m_fast_active_block_divider = 1;
	IntervalLimiter m_active_blocks_mgmt_interval;
	IntervalLimiter m_active_block_modifier_interval;
	IntervalLimiter m_active_blocks_nodemetadata_interval;
	// Whether the variables below have been read from file yet
	bool m_meta_loaded = false;
	// Time from the beginning of the game in seconds.
	// Incremented in step().
	std::atomic_uint32_t m_game_time {0};
	// A helper variable for incrementing the latter
	float m_game_time_fraction_counter = 0.0f;
	// Time of last clearObjects call (game time).
	// When a mapblock older than this is loaded, its objects are cleared.
	u32 m_last_clear_objects_time = 0;
	// Active block modifiers
public:
	std::vector<ABMWithState> m_abms;
private:
	LBMManager m_lbm_mgr;
	// An interval for generally sending object positions and stuff
	std::atomic<float> m_recommended_send_interval {0.1f};
	// Estimate for general maximum lag as determined by server.
	// Can raise to high values like 15s with eg. map generation mods.
	std::atomic<float> m_max_lag_estimate {0.1f};


	/*
	 * TODO: Add a callback function so these can be updated when a setting
	 *       changes.
	 */
	float m_cache_active_block_mgmt_interval;
	float m_cache_abm_interval;
	float m_cache_nodetimer_interval;
	float m_cache_abm_time_budget;

	// peer_ids in here should be unique, except that there may be many 0s
	concurrent_vector<RemotePlayer*> m_players;

	PlayerDatabase *m_player_database = nullptr;
	AuthDatabase *m_auth_database = nullptr;

	// Particles
	IntervalLimiter m_particle_management_interval;
	std::unordered_map<u32, float> m_particle_spawners;
	u32 m_particle_spawners_id_last_used = 0;
	std::unordered_map<u32, u16> m_particle_spawner_attachments;

	// Environment metrics
	MetricCounterPtr m_step_time_counter;
	MetricGaugePtr m_active_block_gauge;
	MetricGaugePtr m_active_object_gauge;

	std::unique_ptr<ServerActiveObject> createSAO(ActiveObjectType type, v3opos_t pos,
			const std::string &data);
};
