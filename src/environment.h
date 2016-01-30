/*
environment.h
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

#ifndef ENVIRONMENT_HEADER
#define ENVIRONMENT_HEADER

/*
	This class is the game's environment.
	It contains:
	- The map
	- Players
	- Other objects
	- The current time in the game
	- etc.
*/

#include <set>
#include <list>
#include <queue>
#include <map>
#include "irr_v3d.h"
#include "activeobject.h"
#include "util/numeric.h"
#include "mapnode.h"
#include "mapblock.h"

//fm:
#include "network/connection.h"
#include "fm_bitset.h"
#include "util/concurrent_unordered_map.h"
#include "util/concurrent_vector.h"
#include <unordered_set>
#include "util/container.h" // Queue
#include <array>
#include "circuit.h"
#include "key_value_storage.h"
#include <unordered_set>
//--

#include "threading/mutex.h"
#include "threading/atomic.h"
#include "network/networkprotocol.h" // for AccessDeniedCode

class ServerEnvironment;
class ActiveBlockModifier;
class ServerActiveObject;
class ITextureSource;
class IGameDef;
class Map;
class ServerMap;
class ClientMap;
class GameScripting;
class Player;
class RemotePlayer;

struct ItemStack;
class PlayerSAO;

namespace epixel
{
class ItemSAO;
class FallingSAO;
}

class Environment
{
public:
	// Environment will delete the map passed to the constructor
	Environment();
	virtual ~Environment();

	/*
		Step everything in environment.
		- Move players
		- Step mobs
		- Run timers of map
	*/
	virtual void step(f32 dtime, float uptime, unsigned int max_cycle_ms) = 0;

	virtual Map & getMap() = 0;

	virtual void addPlayer(Player *player);
	void removePlayer(Player *player);
	Player * getPlayer(u16 peer_id);
	Player * getPlayer(const std::string &name);
	std::vector<Player*> getPlayers();
	std::vector<Player*> getPlayers(bool ignore_disconnected);

	u32 getDayNightRatio();

	// 0-23999
	virtual void setTimeOfDay(u32 time);
	u32 getTimeOfDay();
	float getTimeOfDayF();

	void stepTimeOfDay(float dtime);

	void setTimeOfDaySpeed(float speed);
	float getTimeOfDaySpeed();

	void setDayNightRatioOverride(bool enable, u32 value);

	// counter used internally when triggering ABMs
	std::atomic_uint m_added_objects;

protected:
	// peer_ids in here should be unique, except that there may be many 0s
	concurrent_vector<Player*> m_players;

	GenericAtomic<float> m_time_of_day_speed;

	/*
	 * Below: values managed by m_time_lock
	*/
	// Time of day in milli-hours (0-23999); determines day and night
	u32 m_time_of_day;
	// Time of day in 0...1
	float m_time_of_day_f;
	// Stores the skew created by the float -> u32 conversion
	// to be applied at next conversion, so that there is no real skew.
	float m_time_conversion_skew;
	// Overriding the day-night ratio is useful for custom sky visuals
	bool m_enable_day_night_ratio_override;
	u32 m_day_night_ratio_override;
	/*
	 * Above: values managed by m_time_lock
	*/

	/* TODO: Add a callback function so these can be updated when a setting
	 *       changes.  At this point in time it doesn't matter (e.g. /set
	 *       is documented to change server settings only)
	 *
	 * TODO: Local caching of settings is not optimal and should at some stage
	 *       be updated to use a global settings object for getting thse values
	 *       (as opposed to the this local caching). This can be addressed in
	 *       a later release.
	 */
	bool m_cache_enable_shaders;

private:
	Mutex m_time_lock;

	DISABLE_CLASS_COPY(Environment);
};

/*
	Active block modifier interface.

	These are fed into ServerEnvironment at initialization time;
	ServerEnvironment handles deleting them.
*/

class ActiveBlockModifier
{
public:
	ActiveBlockModifier(){};
	virtual ~ActiveBlockModifier(){};

	// Set of contents to trigger on
	virtual std::set<std::string> getTriggerContents()=0;
	// Set of required neighbors (trigger doesn't happen if none are found)
	// Empty = do not check neighbors
	virtual std::set<std::string> getRequiredNeighbors(bool activate)
	{ return std::set<std::string>(); }
	// Maximum range to neighbors
	virtual u32 getNeighborsRange()
	{ return 1; };
	// Trigger interval in seconds
	virtual float getTriggerInterval() = 0;
	// Random chance of (1 / return value), 0 is disallowed
	virtual u32 getTriggerChance() = 0;
	// Whether to modify chance to simulate time lost by an unnattended block
	virtual bool getSimpleCatchUp() = 0;
	// This is called usually at interval for 1/chance of the nodes
	//virtual void trigger(ServerEnvironment *env, v3s16 p, MapNode n){};
	//virtual void trigger(ServerEnvironment *env, v3s16 p, MapNode n, MapNode neighbor){};
	virtual void trigger(ServerEnvironment *env, v3s16 p, MapNode n,
			u32 active_object_count, u32 active_object_count_wider, MapNode neighbor, bool activate = false){};
};

struct ABMWithState
{
	ActiveBlockModifier *abm;
	float interval;
	float chance;
	float timer;
	int neighbors_range;
	bool simple_catchup;
	std::unordered_set<content_t> trigger_ids;
	FMBitset required_neighbors, required_neighbors_activate;

	ABMWithState(ActiveBlockModifier *abm_, ServerEnvironment *senv);
};

/*
	List of active blocks, used by ServerEnvironment
*/

class ActiveBlockList
{
public:
	void update(std::vector<v3s16> &active_positions,
			s16 radius,
			std::set<v3s16> &blocks_removed,
			std::set<v3s16> &blocks_added);

	bool contains(v3s16 p){
		return (m_list.find(p) != m_list.end());
	}

	void clear(){
		m_list.clear();
	}

	maybe_concurrent_unordered_map<v3POS, bool, v3POSHash, v3POSEqual> m_list;
	std::set<v3s16> m_forceloaded_list;

private:
};

struct ActiveABM
{
	ActiveABM()
	{}
	ABMWithState *abmws;
	int chance;
};

class ABMHandler
{
private:
	ServerEnvironment *m_env;
	std::array<std::vector<ActiveABM> *, CONTENT_ID_CAPACITY> m_aabms;
	std::list<std::vector<ActiveABM>*> m_aabms_list;
	bool m_aabms_empty;
public:
	ABMHandler(ServerEnvironment *env);
	void init(std::vector<ABMWithState> &abms);
	~ABMHandler();
	u32 countObjects(MapBlock *block, ServerMap * map, u32 &wider);
	void apply(MapBlock *block, bool activate = false);

};

/*
	The server-side environment.

	This is not thread-safe. Server uses an environment mutex.
*/

class ServerEnvironment : public Environment
{
public:
	ServerEnvironment(ServerMap *map, GameScripting *scriptIface,
	                  IGameDef *gamedef, const std::string &path_world);
	~ServerEnvironment();

	Map & getMap();

	ServerMap & getServerMap();

	//TODO find way to remove this fct!
	GameScripting* getScriptIface()
		{ return m_script; }

	IGameDef *getGameDef()
		{ return m_gamedef; }

	float getSendRecommendedInterval()
		{ return m_recommended_send_interval; }

	//Player * getPlayer(u16 peer_id) { return Environment::getPlayer(peer_id); };
	//Player * getPlayer(const std::string &name);

	KeyValueStorage &getKeyValueStorage(std::string name = "key_value_storage");
	KeyValueStorage &getPlayerStorage() { return getKeyValueStorage("players"); };

	void kickAllPlayers(AccessDeniedCode reason,
		const std::string &str_reason, bool reconnect);
	// Save players
	void saveLoadedPlayers();
	void savePlayer(RemotePlayer *player);
	Player *loadPlayer(const std::string &playername);

	/*
		Save and load time of day and game timer
	*/
	void saveMeta();
	void loadMeta();

	/*
		External ActiveObject interface
		-------------------------------------------
	*/

	ServerActiveObject* getActiveObject(u16 id, bool removed = false);

	/*
		Add an active object to the environment.
		Environment handles deletion of object.
		Object may be deleted by environment immediately.
		If id of object is 0, assigns a free id to it.
		Returns the id of the object.
		Returns 0 if not added and thus deleted.
	*/
	u16 addActiveObject(ServerActiveObject *object);

	epixel::ItemSAO* spawnItemActiveObject(const std::string &itemName, v3f pos,
			const ItemStack& items);

	epixel::FallingSAO *spawnFallingActiveObject(const std::string &nodeName, v3f pos,
			const MapNode n, int fast = 2);

	/*
		Add an active object as a static object to the corresponding
		MapBlock.
		Caller allocates memory, ServerEnvironment frees memory.
		Return value: true if succeeded, false if failed.
		(note:  not used, pending removal from engine)
	*/
	//bool addActiveObjectAsStatic(ServerActiveObject *object);

	/*
		Find out what new objects have been added to
		inside a radius around a position
	*/
	void getAddedActiveObjects(Player *player, s16 radius,
			s16 player_radius,
			maybe_concurrent_unordered_map<u16, bool> &current_objects,
			std::queue<u16> &added_objects);

	/*
		Find out what new objects have been removed from
		inside a radius around a position
	*/
	void getRemovedActiveObjects(Player* player, s16 radius,
			s16 player_radius,
			maybe_concurrent_unordered_map<u16, bool> &current_objects,
			std::queue<u16> &removed_objects);

	/*
		Get the next message emitted by some active object.
		Returns a message with id=0 if no messages are available.
	*/
	ActiveObjectMessage getActiveObjectMessage();

	/*
		Activate objects and dynamically modify for the dtime determined
		from timestamp and additional_dtime
	*/
	void activateBlock(MapBlock *block, u32 additional_dtime=0);

	/*
		ActiveBlockModifiers
		-------------------------------------------
	*/

	void addActiveBlockModifier(ActiveBlockModifier *abm);

	/*
		Other stuff
		-------------------------------------------
	*/

	// Script-aware node setters
	bool setNode(v3s16 p, const MapNode &n, s16 fast = 0);
	bool removeNode(v3s16 p, s16 fast = 0);
	bool swapNode(v3s16 p, const MapNode &n);

	// Find all active objects inside a radius around a point
	void getObjectsInsideRadius(std::vector<u16> &objects, v3f pos, float radius);

	// Clear all objects, loading and going through every MapBlock
	void clearAllObjects();

	// This makes stuff happen
	void step(f32 dtime, float uptime, unsigned int max_cycle_ms);

	//check if there's a line of sight between two positions
	bool line_of_sight(v3f pos1, v3f pos2, float stepsize=1.0, v3s16 *p=NULL);

	u32 getGameTime() { return m_game_time; }

	void reportMaxLagEstimate(float f) { std::unique_lock<std::mutex> lock(m_max_lag_estimate_mutex); m_max_lag_estimate = f; }
	float getMaxLagEstimate() { std::unique_lock<std::mutex> lock(m_max_lag_estimate_mutex); return m_max_lag_estimate; }

	// is weather active in this environment?
	bool m_use_weather;
	bool m_use_weather_biome;
	bool m_more_threads;
	ABMHandler m_abmhandler;
	void analyzeBlock(MapBlock * block);
	IntervalLimiter m_analyze_blocks_interval;
	IntervalLimiter m_abm_random_interval;
	std::list<v3POS> m_abm_random_blocks;
	int analyzeBlocks(float dtime, unsigned int max_cycle_ms);

	std::set<v3s16>* getForceloadedBlocks() { return &m_active_blocks.m_forceloaded_list; };

	u32 m_game_time_start;

	// Sets the static object status all the active objects in the specified block
	// This is only really needed for deleting blocks from the map
	void setStaticForActiveObjectsInBlock(v3s16 blockpos,
		bool static_exists, v3s16 static_block=v3s16(0,0,0));

	void nodeUpdate(const v3s16 pos, u16 recursion_limit = 5, int fast = 2, bool destroy = false);
	void handleNodeDrops(const ContentFeatures &f, v3f pos, PlayerSAO* player=NULL);
private:

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
	u16 addActiveObjectRaw(ServerActiveObject *object, bool set_changed, u32 dtime_s);

	/*
		Remove all objects that satisfy (m_removed && m_known_by_count==0)
	*/
	void removeRemovedObjects(unsigned int max_cycle_ms = 1000);

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
	void contrib_player_globalstep(RemotePlayer *player, float dtime);
	void contrib_lookupitemtogather(RemotePlayer* player, v3f playerPos,
			Inventory* inv, ServerActiveObject* obj);
*/
	void contrib_globalstep(const float dtime);
	bool checkAttachedNode(v3s16 pos, MapNode n, const ContentFeatures &f);
/*
	void explodeNode(const v3s16 pos);
*/

	std::deque<v3s16> m_nodeupdate_queue;

	/*
		Member variables
	*/

	// The map
	ServerMap *m_map;
	// Lua state
	GameScripting* m_script;
	// Game definition
	IGameDef *m_gamedef;

	// Circuit manager
	Circuit m_circuit;
	// Key-value storage
public:
	std::unordered_map<std::string, KeyValueStorage> m_key_value_storage;
private:

	// World path
	const std::string m_path_world;
	// Active object list
	maybe_concurrent_map<u16, ServerActiveObject*> m_active_objects;
	std::vector<u16> objects_to_remove;
	std::vector<ServerActiveObject*> objects_to_delete;
	// Outgoing network message buffer for active objects
public:
	Queue<ActiveObjectMessage> m_active_object_messages;
private:
	// Some timers
	float m_send_recommended_timer;
	IntervalLimiter m_object_management_interval;
	// List of active blocks
	ActiveBlockList m_active_blocks;
	IntervalLimiter m_active_blocks_management_interval;
	IntervalLimiter m_active_block_modifier_interval;
	IntervalLimiter m_active_blocks_nodemetadata_interval;
	//loop breakers
	u32 m_active_objects_last;
	u32 m_active_block_abm_last;
	float m_active_block_abm_dtime;
	float m_active_block_abm_dtime_counter;
	u32 m_active_block_timer_last;
	std::set<v3s16> m_blocks_added;
	u32 m_blocks_added_last;
	u32 m_active_block_analyzed_last;
	// Time from the beginning of the game in seconds.
	// Incremented in step().
	std::atomic_uint m_game_time;
	std::mutex m_max_lag_estimate_mutex;
	// A helper variable for incrementing the latter
	float m_game_time_fraction_counter;
public:
	std::vector<ABMWithState> m_abms;
private:
	// An interval for generally sending object positions and stuff
	float m_recommended_send_interval;
	// Estimate for general maximum lag as determined by server.
	// Can raise to high values like 15s with eg. map generation mods.
	float m_max_lag_estimate;
};

#ifndef SERVER

#include "clientobject.h"
#include "content_cao.h"

class ClientSimpleObject;

/*
	The client-side environment.

	This is not thread-safe.
	Must be called from main (irrlicht) thread (uses the SceneManager)
	Client uses an environment mutex.
*/

enum ClientEnvEventType
{
	CEE_NONE,
	CEE_PLAYER_DAMAGE,
	CEE_PLAYER_BREATH
};

struct ClientEnvEvent
{
	ClientEnvEventType type;
	union {
		//struct{
		//} none;
		struct{
			u8 amount;
			bool send_to_server;
		} player_damage;
		struct{
			u16 amount;
		} player_breath;
	};
};

class ClientEnvironment : public Environment
{
public:
	ClientEnvironment(ClientMap *map, scene::ISceneManager *smgr,
			ITextureSource *texturesource, IGameDef *gamedef,
			IrrlichtDevice *device);
	~ClientEnvironment();

	Map & getMap();
	ClientMap & getClientMap();

	IGameDef *getGameDef()
	{ return m_gamedef; }

	void step(f32 dtime, float uptime, unsigned int max_cycle_ms);

	virtual void addPlayer(Player *player);
	LocalPlayer * getLocalPlayer();

	/*
		ClientSimpleObjects
	*/

	void addSimpleObject(ClientSimpleObject *simple);

	/*
		ActiveObjects
	*/

	GenericCAO* getGenericCAO(u16 id);
	ClientActiveObject* getActiveObject(u16 id);

	/*
		Adds an active object to the environment.
		Environment handles deletion of object.
		Object may be deleted by environment immediately.
		If id of object is 0, assigns a free id to it.
		Returns the id of the object.
		Returns 0 if not added and thus deleted.
	*/
	u16 addActiveObject(ClientActiveObject *object);

	void addActiveObject(u16 id, u8 type, const std::string &init_data);
	void removeActiveObject(u16 id);

	void processActiveObjectMessage(u16 id, const std::string &data);

	/*
		Callbacks for activeobjects
	*/

	void damageLocalPlayer(u8 damage, bool handle_hp=true);
	void updateLocalPlayerBreath(u16 breath);

	/*
		Client likes to call these
	*/

	// Get all nearby objects
	void getActiveObjects(v3f origin, f32 max_d,
			std::vector<DistanceSortedActiveObject> &dest);

	// Get event from queue. CEE_NONE is returned if queue is empty.
	ClientEnvEvent getClientEvent();

	u16 attachement_parent_ids[USHRT_MAX + 1];

	std::list<std::string> getPlayerNames()
	{ return m_player_names; }
	void addPlayerName(std::string name)
	{ m_player_names.push_back(name); }
	void removePlayerName(std::string name)
	{ m_player_names.remove(name); }
	void updateCameraOffset(v3s16 camera_offset)
	{ m_camera_offset = camera_offset; }
	v3s16 getCameraOffset()
	{ return m_camera_offset; }

private:
	ClientMap *m_map;
	scene::ISceneManager *m_smgr;
	ITextureSource *m_texturesource;
	IGameDef *m_gamedef;
	IrrlichtDevice *m_irr;
	std::map<u16, ClientActiveObject*> m_active_objects;
	u32 m_active_objects_client_last;
	u32 m_move_max_loop;
	std::vector<ClientSimpleObject*> m_simple_objects;
	std::queue<ClientEnvEvent> m_client_event_queue;
	IntervalLimiter m_active_object_light_update_interval;
	IntervalLimiter m_lava_hurt_interval;
	IntervalLimiter m_drowning_interval;
	IntervalLimiter m_breathing_interval;
	std::list<std::string> m_player_names;
	v3s16 m_camera_offset;
};

#endif

#endif

