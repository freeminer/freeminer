/*
server.h
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

#include "irr_v3d.h"
#include "map.h"
#include "hud.h"
#include "gamedef.h"
#include "serialization.h" // For SER_FMT_VER_INVALID
#include "content/mods.h"
#include "inventorymanager.h"
#include "content/subgames.h"
#include "network/peerhandler.h"
#include "network/address.h"
#include "util/numeric.h"
#include "util/thread.h"
#include "util/basic_macros.h"
#include "util/metricsbackend.h"
#include "serverenvironment.h"
#include "clientiface.h"
#include "chatmessage.h"
#include "sound.h"
#include "translation.h"
#include <memory>
#include <string>
#include <list>
#include <map>
#include <vector>


//fm:
#include "stat.h"
#include "network/fm_lan.h"
#include <unordered_set>
class Circuit;
class Stat;
class MapThread;
class SendBlocksThread;
class SendFarBlocksThread;
class LiquidThread;
class EnvThread;
class AbmThread;
class AbmWorldThread;
class WorldMergeThread;


class ClientNotFoundException : public BaseException
{
public:
	ClientNotFoundException(const char *s):
		BaseException(s)
	{}
};



class ChatEvent;
struct ChatEventChat;
struct ChatInterface;
class IWritableItemDefManager;
class NodeDefManager;
class IWritableCraftDefManager;
class BanManager;
class EventManager;
class Inventory;
class ModChannelMgr;
class RemotePlayer;
class PlayerSAO;
struct PlayerHPChangeReason;
class IRollbackManager;
struct RollbackAction;
class EmergeManager;
class ServerScripting;
class ServerEnvironment;
struct SoundSpec;
struct CloudParams;
struct SkyboxParams;
struct SunParams;
struct MoonParams;
struct StarParams;
struct Lighting;
class ServerThread;
class ServerModManager;
class ServerInventoryManager;
struct PackedValue;
struct ParticleParameters;
struct ParticleSpawnerParameters;

enum ClientDeletionReason {
	CDR_LEAVE,
	CDR_TIMEOUT,
	CDR_DENY
};

struct MediaInfo
{
	std::string path;
	std::string sha1_digest; // base64-encoded
	bool no_announce; // true: not announced in TOCLIENT_ANNOUNCE_MEDIA (at player join)

	MediaInfo(const std::string &path_="",
	          const std::string &sha1_digest_=""):
		path(path_),
		sha1_digest(sha1_digest_),
		no_announce(false)
	{
	}
};

// Combines the pure sound (SoundSpec) with positional information
struct ServerPlayingSound
{
	SoundLocation type = SoundLocation::Local;

	float gain = 1.0f; // for amplification of the base sound
	float max_hear_distance = 32 * BS;
	v3f pos;
	u16 object = 0;
	std::string to_player;
	std::string exclude_player;

	v3f getPos(ServerEnvironment *env, bool *pos_exists) const;

	SoundSpec spec;

	std::unordered_set<session_t> clients; // peer ids
};

struct MinimapMode {
	MinimapType type = MINIMAP_TYPE_OFF;
	std::string label;
	u16 size = 0;
	std::string texture;
	u16 scale = 1;
};

// structure for everything getClientInfo returns, for convenience
struct ClientInfo {
	ClientState state;
	Address addr;
	u32 uptime;
	u8 ser_vers;
	u16 prot_vers;
	u8 major, minor, patch;
	std::string vers_string, lang_code;
};

class Server : public con::PeerHandler, public MapEventReceiver,
		public IGameDef
{
public:
	/*
		NOTE: Every public method should be thread-safe
	*/

	Server(
		const std::string &path_world,
		const SubgameSpec &gamespec,
		bool simple_singleplayer_mode,
		Address bind_addr,
		bool dedicated,
		ChatInterface *iface = nullptr,
		std::string *shutdown_errmsg = nullptr
	);
	~Server();
	DISABLE_CLASS_COPY(Server);

	void start();
	void stop();
	// This is mainly a way to pass the time to the server.
	// Actual processing is done in another thread.
	void step(float dtime);
	// This is run by ServerThread and does the actual processing

	void AsyncRunStep( float dtime, bool initial_step=false);
	u16 Receive(int ms = 10);
	PlayerSAO* StageTwoClientInit(session_t peer_id);

	/*
	 * Command Handlers
	 */

	void handleCommand(NetworkPacket* pkt);

	void handleCommand_Null(NetworkPacket* pkt) {};
	void handleCommand_Deprecated(NetworkPacket* pkt);
	void handleCommand_Init(NetworkPacket* pkt);
	void handleCommand_Init2(NetworkPacket* pkt);
	void handleCommand_ModChannelJoin(NetworkPacket *pkt);
	void handleCommand_ModChannelLeave(NetworkPacket *pkt);
	void handleCommand_ModChannelMsg(NetworkPacket *pkt);
	void handleCommand_RequestMedia(NetworkPacket* pkt);
	void handleCommand_ClientReady(NetworkPacket* pkt);
	void handleCommand_GotBlocks(NetworkPacket* pkt);
	void handleCommand_PlayerPos(NetworkPacket* pkt);
	void handleCommand_DeletedBlocks(NetworkPacket* pkt);
	void handleCommand_InventoryAction(NetworkPacket* pkt);
	void handleCommand_ChatMessage(NetworkPacket* pkt);
	void handleCommand_Damage(NetworkPacket* pkt);
	void handleCommand_PlayerItem(NetworkPacket* pkt);
	void handleCommand_Respawn(NetworkPacket* pkt);
	void handleCommand_Interact(NetworkPacket* pkt);
	void handleCommand_RemovedSounds(NetworkPacket* pkt);
	void handleCommand_NodeMetaFields(NetworkPacket* pkt);
	void handleCommand_InventoryFields(NetworkPacket* pkt);
	void handleCommand_FirstSrp(NetworkPacket* pkt);
	void handleCommand_SrpBytesA(NetworkPacket* pkt);
	void handleCommand_SrpBytesM(NetworkPacket* pkt);
	void handleCommand_HaveMedia(NetworkPacket *pkt);
	void handleCommand_UpdateClientInfo(NetworkPacket *pkt);

	void ProcessData(NetworkPacket *pkt);

	void Send(NetworkPacket *pkt);
	void Send(session_t peer_id, NetworkPacket *pkt);

	// Helper for handleCommand_PlayerPos and handleCommand_Interact
	void process_PlayerPos(RemotePlayer *player, PlayerSAO *playersao,
		NetworkPacket *pkt);

	// Both setter and getter need no envlock,
	// can be called freely from threads
	void setTimeOfDay(u32 time);

	/*
		Shall be called with the environment locked.
		This is accessed by the map, which is inside the environment,
		so it shouldn't be a problem.
	*/
	void onMapEditEvent(const MapEditEvent &event);

	// Connection must be locked when called
	std::string getStatusString();
	inline double getUptime() const { return m_uptime_counter->get(); }

	// read shutdown state
	inline bool isShutdownRequested() const { return m_shutdown_state.is_requested; }

	// request server to shutdown
	void requestShutdown(const std::string &msg, bool reconnect, float delay = 0.0f);

	// Returns -1 if failed, sound handle on success
	// Envlock
	s32 playSound(ServerPlayingSound &params, bool ephemeral=false);
	void stopSound(s32 handle);
	void fadeSound(s32 handle, float step, float gain);

	// Envlock
	std::set<std::string> getPlayerEffectivePrivs(const std::string &name);
	bool checkPriv(const std::string &name, const std::string &priv);
	void reportPrivsModified(const std::string &name=""); // ""=all
	void reportInventoryFormspecModified(const std::string &name);
	void reportFormspecPrependModified(const std::string &name);

	void setIpBanned(const std::string &ip, const std::string &name);
	void unsetIpBanned(const std::string &ip_or_name);
	std::string getBanDescription(const std::string &ip_or_name);

	void notifyPlayer(const char *name, const std::wstring &msg);
	void notifyPlayers(const std::wstring &msg);

	void spawnParticle(const std::string &playername,
		const ParticleParameters &p);

	u32 addParticleSpawner(const ParticleSpawnerParameters &p,
		ServerActiveObject *attached, const std::string &playername);

	void deleteParticleSpawner(const std::string &playername, u32 id);

	bool dynamicAddMedia(std::string filepath, u32 token,
		const std::string &to_player, bool ephemeral);

	ServerInventoryManager *getInventoryMgr() const { return m_inventory_mgr.get(); }
	void sendDetachedInventory(Inventory *inventory, const std::string &name, session_t peer_id);

	// Envlock and conlock should be locked when using scriptapi
	ServerScripting *getScriptIface(){ return m_script; }

	// actions: time-reversed list
	// Return value: success/failure
	bool rollbackRevertActions(const std::list<RollbackAction> &actions,
			std::list<std::string> *log);

	// IGameDef interface
	// Under envlock
	virtual IItemDefManager* getItemDefManager();
	virtual const NodeDefManager* getNodeDefManager();
	virtual ICraftDefManager* getCraftDefManager();
	virtual u16 allocateUnknownNodeId(const std::string &name);
	IRollbackManager *getRollbackManager() { return m_rollback; }
	virtual EmergeManager *getEmergeManager() { return m_emerge; }
	virtual ModStorageDatabase *getModStorageDatabase() { return m_mod_storage_database; }

	IWritableItemDefManager* getWritableItemDefManager();
	NodeDefManager* getWritableNodeDefManager();
	IWritableCraftDefManager* getWritableCraftDefManager();

	virtual const std::vector<ModSpec> &getMods() const;
	virtual const ModSpec* getModSpec(const std::string &modname) const;
	virtual const SubgameSpec* getGameSpec() const { return &m_gamespec; }
	static std::string getBuiltinLuaPath();
	virtual std::string getWorldPath() const { return m_path_world; }

	inline bool isSingleplayer() const
			{ return m_simple_singleplayer_mode; }

	inline void setAsyncFatalError(const std::string &error)
			{ m_async_fatal_error.set(error); }
	inline void setAsyncFatalError(const LuaError &e)
	{
		setAsyncFatalError(std::string("Lua: ") + e.what());
	}

	// Not thread-safe.
	void addShutdownError(const ModError &e);

	bool showFormspec(const char *name, const std::string &formspec, const std::string &formname);
	Map & getMap() { return m_env->getMap(); }
	ServerEnvironment & getEnv() { return *m_env; }
	v3opos_t findSpawnPos(const std::string &player_name);

	u32 hudAdd(RemotePlayer *player, HudElement *element);
	bool hudRemove(RemotePlayer *player, u32 id);
	bool hudChange(RemotePlayer *player, u32 id, HudElementStat stat, void *value);
	bool hudSetFlags(RemotePlayer *player, u32 flags, u32 mask);
	bool hudSetHotbarItemcount(RemotePlayer *player, s32 hotbar_itemcount);
	void hudSetHotbarImage(RemotePlayer *player, const std::string &name, int items = 0);
	void hudSetHotbarSelectedImage(RemotePlayer *player, const std::string &name);

	Address getPeerAddress(session_t peer_id);

	void setLocalPlayerAnimations(RemotePlayer *player, v2s32 animation_frames[4],
			f32 frame_speed);
	void setPlayerEyeOffset(RemotePlayer *player, const v3f &first, const v3f &third, const v3f &third_front);

	void setSky(RemotePlayer *player, const SkyboxParams &params);
	void setSun(RemotePlayer *player, const SunParams &params);
	void setMoon(RemotePlayer *player, const MoonParams &params);
	void setStars(RemotePlayer *player, const StarParams &params);

	void setClouds(RemotePlayer *player, const CloudParams &params);

	void overrideDayNightRatio(RemotePlayer *player, bool do_override, float brightness);

	void setLighting(RemotePlayer *player, const Lighting &lighting);

	void RespawnPlayer(session_t peer_id);

	/* con::PeerHandler implementation. */
	void peerAdded(session_t peer_id);
	void deletingPeer(session_t peer_id, bool timeout);

	void DenySudoAccess(session_t peer_id);
	void DenyAccess(session_t peer_id, AccessDeniedCode reason,
		const std::string &custom_reason = "", bool reconnect = false);
	void acceptAuth(session_t peer_id, bool forSudoMode);
	void DisconnectPeer(session_t peer_id);
	bool getClientConInfo(session_t peer_id, con::rtt_stat_type type, float *retval);
	bool getClientInfo(session_t peer_id, ClientInfo &ret);
	const ClientDynamicInfo *getClientDynamicInfo(session_t peer_id);

	void printToConsoleOnly(const std::string &text);

	void HandlePlayerHPChange(PlayerSAO *sao, const PlayerHPChangeReason &reason);
	void SendPlayerHP(PlayerSAO *sao, bool effect);
	void SendPlayerBreath(PlayerSAO *sao);
	void SendInventory(PlayerSAO *playerSAO, bool incremental);
	void SendMovePlayer(session_t peer_id);
	void SendPlayerSpeed(session_t peer_id, const v3f &added_vel);
	void SendPlayerFov(session_t peer_id);

	void SendMinimapModes(session_t peer_id,
			std::vector<MinimapMode> &modes,
			size_t wanted_mode);

	void sendDetachedInventories(session_t peer_id, bool incremental);

	bool joinModChannel(const std::string &channel);
	bool leaveModChannel(const std::string &channel);
	bool sendModChannelMessage(const std::string &channel, const std::string &message);
	ModChannel *getModChannel(const std::string &channel);

	// Send block to specific player only
	bool SendBlock(session_t peer_id, const v3bpos_t &blockpos);

	// Get or load translations for a language
	Translations *getTranslationLanguage(const std::string &lang_code);

	static ModStorageDatabase *openModStorageDatabase(const std::string &world_path);

	static ModStorageDatabase *openModStorageDatabase(const std::string &backend,
			const std::string &world_path, const Settings &world_mt);

	static bool migrateModStorageDatabase(const GameParams &game_params,
			const Settings &cmd_args);

	// Lua files registered for init of async env, pair of modname + path
	std::vector<std::pair<std::string, std::string>> m_async_init_files;

	// Data transferred into other Lua envs at init time
	std::unique_ptr<PackedValue> m_lua_globals_data;

	// Bind address
	Address m_bind_addr;

	// Environment mutex (envlock)
	//std::mutex m_env_mutex;

private:
	friend class EmergeThread;
	friend class RemoteClient;
	friend class TestServerShutdownState;

	struct ShutdownState {
		friend class TestServerShutdownState;
		public:
			bool is_requested = false;
			bool should_reconnect = false;
			std::string message;

			void reset();
			void trigger(float delay, const std::string &msg, bool reconnect);
			void tick(float dtime, Server *server);
			std::wstring getShutdownTimerMessage() const;
			bool isTimerRunning() const { return m_timer > 0.0f; }
		private:
			float m_timer = 0.0f;
	};

	struct PendingDynamicMediaCallback {
		std::string filename; // only set if media entry and file is to be deleted
		float expiry_timer;
		std::unordered_set<session_t> waiting_players;
	};

	// The standard library does not implement std::hash for pairs so we have this:
	struct SBCHash {
		size_t operator() (const std::pair<v3bpos_t, u16> &p) const {
			return std::hash<v3bpos_t>()(p.first) ^ p.second;
		}
	};

	typedef std::unordered_map<std::pair<v3bpos_t, u16>, std::string, SBCHash> SerializedBlockCache;

	void init();

	void SendMovement(session_t peer_id);
	void SendHP(session_t peer_id, u16 hp, bool effect);
	void SendBreath(session_t peer_id, u16 breath);
	void SendAccessDenied(session_t peer_id, AccessDeniedCode reason,
		const std::string &custom_reason, bool reconnect = false);
	void SendAccessDenied_Legacy(session_t peer_id, const std::wstring &reason);
	void SendDeathscreen(session_t peer_id, bool set_camera_point_target,
		v3f camera_point_target);
	void SendItemDef(session_t peer_id, IItemDefManager *itemdef, u16 protocol_version);
	void SendNodeDef(session_t peer_id, const NodeDefManager *nodedef,
		u16 protocol_version);


	virtual void SendChatMessage(session_t peer_id, const ChatMessage &message);
	void SendTimeOfDay(session_t peer_id, u16 time, f32 time_speed);

	void SendLocalPlayerAnimations(session_t peer_id, v2s32 animation_frames[4],
		f32 animation_speed);
	void SendEyeOffset(session_t peer_id, v3f first, v3f third, v3f third_front);
	void SendPlayerPrivileges(session_t peer_id);
	void SendPlayerInventoryFormspec(session_t peer_id);
	void SendPlayerFormspecPrepend(session_t peer_id);
	void SendShowFormspecMessage(session_t peer_id, const std::string &formspec,
		const std::string &formname);
	void SendHUDAdd(session_t peer_id, u32 id, HudElement *form);
	void SendHUDRemove(session_t peer_id, u32 id);
	void SendHUDChange(session_t peer_id, u32 id, HudElementStat stat, void *value);
	void SendHUDSetFlags(session_t peer_id, u32 flags, u32 mask);
	void SendHUDSetParam(session_t peer_id, u16 param, const std::string &value);
	void SendSetSky(session_t peer_id, const SkyboxParams &params);
	void SendSetSun(session_t peer_id, const SunParams &params);
	void SendSetMoon(session_t peer_id, const MoonParams &params);
	void SendSetStars(session_t peer_id, const StarParams &params);
	void SendCloudParams(session_t peer_id, const CloudParams &params);
	void SendOverrideDayNightRatio(session_t peer_id, bool do_override, float ratio);
	void SendSetLighting(session_t peer_id, const Lighting &lighting);
	void broadcastModChannelMessage(const std::string &channel,
			const std::string &message, session_t from_peer);

	/*
		Send a node removal/addition event to all clients except ignore_id.
		Additionally, if far_players!=NULL, players further away than
		far_d_nodes are ignored and their peer_ids are added to far_players
	*/
	// Envlock and conlock should be locked when calling these
	void sendRemoveNode(v3pos_t p, std::unordered_set<u16> *far_players = nullptr,
			float far_d_nodes = 100);
	void sendAddNode(v3pos_t p, MapNode n,
			std::unordered_set<u16> *far_players = nullptr,
			float far_d_nodes = 100, bool remove_metadata = true);
	void sendNodeChangePkt(u16 command, const MapNode& n, v3pos_t p_int,
			float far_d_nodes, std::unordered_set<u16> *far_players, bool remove_metadata = true);

	void sendMetadataChanged(const std::unordered_set<v3pos_t> &positions,
			float far_d_nodes = 100);

	// Environment and Connection must be locked when called
	// `cache` may only be very short lived! (invalidation not handeled)
	void SendBlockNoLock(session_t peer_id, MapBlock *block, u8 ver,
		u16 net_proto_version, SerializedBlockCache *cache = nullptr);

	// Sends blocks to clients (locks env and con on its own)
public:
	int SendBlocks(float dtime);
private:

	size_t addMediaFile(const std::string &filename, const std::string &filepath,
			std::string *filedata = nullptr, std::string *digest = nullptr);
	void fillMediaCache();
	void sendMediaAnnouncement(session_t peer_id, const std::string &lang_code);
	void sendRequestedMedia(session_t peer_id,
			const std::vector<std::string> &tosend);
	void stepPendingDynMediaCallbacks(float dtime);

	// Adds a ParticleSpawner on peer with peer_id (PEER_ID_INEXISTENT == all)
	void SendAddParticleSpawner(session_t peer_id, u16 protocol_version,
		const ParticleSpawnerParameters &p, u16 attached_id, u32 id);

	void SendDeleteParticleSpawner(session_t peer_id, u32 id);

	// Spawns particle on peer with peer_id (PEER_ID_INEXISTENT == all)
	void SendSpawnParticle(session_t peer_id, u16 protocol_version,
		const ParticleParameters &p);

	void SendActiveObjectRemoveAdd(RemoteClient *client, PlayerSAO *playersao);
//mt compat:
	void SendActiveObjectMessages(session_t peer_id, const std::string &datas,
		bool reliable = true);
	void SendCSMRestrictionFlags(session_t peer_id);

	/*
		Something random
	*/

	void HandlePlayerDeath(PlayerSAO* sao, const PlayerHPChangeReason &reason);
	void DeleteClient(session_t peer_id, ClientDeletionReason reason);
	void UpdateCrafting(RemotePlayer *player);
	bool checkInteractDistance(RemotePlayer *player, const f32 d, const std::string &what);

	void handleChatInterfaceEvent(ChatEvent *evt);

	// This returns the answer to the sender of wmessage, or "" if there is none
	std::wstring handleChat(const std::string &name, std::wstring wmessage_input,
		bool check_shout_priv = false, RemotePlayer *player = nullptr);
	void handleAdminChat(const ChatEventChat *evt);

	// When called, connection mutex should be locked
	RemoteClient* getClient(session_t peer_id, ClientState state_min = CS_Active);
	RemoteClient* getClientNoEx(session_t peer_id, ClientState state_min = CS_Active);

	// When called, environment mutex should be locked
	std::string getPlayerName(session_t peer_id);
	PlayerSAO *getPlayerSAO(session_t peer_id);

	/*
		Get a player from memory or creates one.
		If player is already connected, return NULL
		Does not verify/modify auth info and password.

		Call with env and con locked.
	*/
	PlayerSAO *emergePlayer(const char *name, session_t peer_id, u16 proto_version);

	void handlePeerChanges();

	/*
		Variables
	*/
	// World directory
	std::string m_path_world;
	// Subgame specification
	SubgameSpec m_gamespec;
	// If true, do not allow multiple players and hide some multiplayer
	// functionality
	bool m_simple_singleplayer_mode;
	u16 m_max_chatmessage_length;
	// For "dedicated" server list flag
	bool m_dedicated;
	Settings *m_game_settings = nullptr;

	// Thread can set; step() will throw as ServerError
	MutexedVariable<std::string> m_async_fatal_error;

	// Some timers
	float m_liquid_transform_timer = 0.0f;
	float m_liquid_transform_every = 1.0f;
	float m_masterserver_timer = 0.0f;
	float m_emergethread_trigger_timer = 0.0f;
	float m_savemap_timer = 0.0f;
	IntervalLimiter m_map_timer_and_unload_interval;

	// Environment
	ServerEnvironment *m_env = nullptr;

	// Reference to the server map until ServerEnvironment is initialized
	// after that this variable must be a nullptr
	ServerMap *m_startup_server_map = nullptr;

public:
	// server connection
	std::shared_ptr<con_use::Connection> m_con;

private:

	// Ban checking
	BanManager *m_banmanager = nullptr;

	// Rollback manager (behind m_env_mutex)
	IRollbackManager *m_rollback = nullptr;

public:
	// Emerge manager
	EmergeManager *m_emerge = nullptr;
private:

	// Scripting
	// Envlock and conlock should be locked when using Lua
	ServerScripting *m_script = nullptr;

	// Item definition manager
	IWritableItemDefManager *m_itemdef;

	// Node definition manager
	NodeDefManager *m_nodedef;

	// Craft definition manager
	IWritableCraftDefManager *m_craftdef;

	// Mods
	std::unique_ptr<ServerModManager> m_modmgr;

	std::unordered_map<std::string, Translations> server_translations;

	/*
		Threads
	*/
	// A buffer for time steps
	// step() increments and AsyncRunStep() run by m_thread reads it.
public:
	float m_step_dtime = 0.0f;
private:
	std::mutex m_step_dtime_mutex;

	// The server mainly operates in this thread
	ServerThread *m_thread = nullptr;

	/*
		Time related stuff
	*/
	// Timer for sending time of day over network
	float m_time_of_day_send_timer = 0.0f;

public:
	/*
	 	Client interface
	*/
	ClientInterface m_clients;

private:
	/*
		Peer change queue.
		Queues stuff from peerAdded() and deletingPeer() to
		handlePeerChanges()
	*/
	Queue<con::PeerChange> m_peer_change_queue;

	std::unordered_map<session_t, std::string> m_formspec_state_data;

	/*
		Random stuff
	*/

	ShutdownState m_shutdown_state;

	ChatInterface *m_admin_chat;
	std::string m_admin_nick;

	// If a mod error occurs while shutting down, the error message will be
	// written into this.
	std::string *const m_shutdown_errmsg;

	/*
		Map edit event queue. Automatically receives all map edits.
		The constructor of this class registers us to receive them through
		onMapEditEvent

		NOTE: Should these be moved to actually be members of
		ServerEnvironment?
	*/

	/*
		Queue of map edits from the environment for sending to the clients
		This is behind m_env_mutex
	*/
	Queue<MapEditEvent*> m_unsent_map_edit_queue;
	/*
		If a non-empty area, map edit events contained within are left
		unsent. Done at map generation time to speed up editing of the
		generated area, as it will be sent anyway.
		This is behind m_env_mutex
	*/
	VoxelArea m_ignore_map_edit_events_area;

	// media files known to server
	std::unordered_map<std::string, MediaInfo> m_media;

	// pending dynamic media callbacks, clients inform the server when they have a file fetched
	std::unordered_map<u32, PendingDynamicMediaCallback> m_pending_dyn_media;
	float m_step_pending_dyn_media_timer = 0.0f;

	/*
		Sounds
	*/
	std::unordered_map<s32, ServerPlayingSound> m_playing_sounds;
	s32 m_playing_sounds_id_last_used = 0; // positive values only
	s32 nextSoundId();

	ModStorageDatabase *m_mod_storage_database = nullptr;
	float m_mod_storage_save_timer = 10.0f;





	// freeminer:
private:
	int save(float dtime, float dedicated_server_step = 0.1, bool breakable = false);

	//fmtodo: remove:
	void DenyAccess(session_t peer_id, const std::string &reason);

	void SetBlocksNotSent();
	void SendFreeminerInit(session_t peer_id, u16 protocol_version);
	void SendActiveObjectMessages(
			session_t peer_id, const ActiveObjectMessages &datas, bool reliable = true);
	void SendBlockFm(session_t peer_id, MapBlockP block, u8 ver, u16 net_proto_version,
			SerializedBlockCache *cache = nullptr);

	float m_liquid_send_timer{};
	float m_liquid_send_interval{1};

public:
	lan_adv lan_adv_server;
	int m_autoexit{};
	//concurrent_map<v3POS, MapBlock*> m_modified_blocks;
	//concurrent_map<v3POS, MapBlock*> m_lighting_modified_blocks;
	bool m_more_threads{};
	unsigned int overload{};

	int AsyncRunMapStep(
			float dtime, float dedicated_server_step = 0.1, bool async = true);
	void deleteDetachedInventory(const std::string &name);
	void maintenance_start();
	void maintenance_end();
	int maintenance_status{};
	void SendPunchPlayer(session_t peer_id, v3f speed);

	void handleCommand_Drawcontrol(NetworkPacket *pkt);
	void handleCommand_GetBlocks(NetworkPacket *pkt);
	void handleCommand_InitFm(NetworkPacket *pkt);
	ServerMap::far_dbases_t far_dbases;
	uint32_t SendFarBlocks(float dtime);

	Stat stat;

	std::unique_ptr<MapThread> m_map_thread;
	std::unique_ptr<SendBlocksThread> m_sendblocks_thead;
	std::unique_ptr<SendFarBlocksThread> m_sendfarblocks_thead;
	std::unique_ptr<LiquidThread> m_liquid;
	std::unique_ptr<EnvThread> m_env_thread;
	std::unique_ptr<AbmThread> m_abm_thread;
	std::unique_ptr<AbmWorldThread> m_abm_world_thread;
	std::unique_ptr<WorldMergeThread> m_world_merge_thread;
	// ==



	// CSM restrictions byteflag
	u64 m_csm_restriction_flags = CSMRestrictionFlags::CSM_RF_NONE;
	u32 m_csm_restriction_noderange = 8;

	// ModChannel manager
	std::unique_ptr<ModChannelMgr> m_modchannel_mgr;

	// Inventory manager
	std::unique_ptr<ServerInventoryManager> m_inventory_mgr;

	// Global server metrics backend
	std::unique_ptr<MetricsBackend> m_metrics_backend;

	// Server metrics
	MetricCounterPtr m_uptime_counter;
	MetricGaugePtr m_player_gauge;
	MetricGaugePtr m_timeofday_gauge;
	MetricGaugePtr m_lag_gauge;
	MetricCounterPtr m_aom_buffer_counter[2]; // [0] = rel, [1] = unrel
	MetricCounterPtr m_packet_recv_counter;
	MetricCounterPtr m_packet_recv_processed_counter;
	MetricCounterPtr m_map_edit_event_counter;
};

/*
	Runs a simple dedicated server loop.

	Shuts down when kill is set to true.
*/
void dedicated_server_loop(Server &server, bool &kill);


// fm:
MapDatabase *GetFarDatabase(MapDatabase *dbase, ServerMap::far_dbases_t &far_dbases,
		const std::string &savedir, MapBlock::block_step_t step);
MapBlockP loadBlockNoStore(Map *smap, MapDatabase *dbase, const v3bpos_t &pos);
// ==
