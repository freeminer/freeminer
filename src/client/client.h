/*
client.h
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

#include "clientenvironment.h"
#include "irrlichttypes_extrabloated.h"
#include <ostream>
#include <map>
#include <set>
#include <vector>
#include <unordered_set>
#include "clientobject.h"
#include "gamedef.h"
#include "inventorymanager.h"
#include "localplayer.h"
#include "client/hud.h"
#include "particles.h"
<<<<<<< HEAD:src/client.h

#include "threading/thread_pool.h"
#include "util/unordered_map_hash.h"
#include "msgpack_fix.h"

#include "network/networkpacket.h"
=======
#include "mapnode.h"
#include "tileanimation.h"
#include "mesh_generator_thread.h"
#include "network/address.h"
#include "network/peerhandler.h"
#include <fstream>
>>>>>>> 5.5.0:src/client/client.h

#define CLIENT_CHAT_MESSAGE_LIMIT_PER_10S 10.0f

struct ClientEvent;
struct MeshMakeData;
struct ChatMessage;
class MapBlockMesh;
class RenderingEngine;
class IWritableTextureSource;
class IWritableShaderSource;
class IWritableItemDefManager;
class ISoundManager;
class NodeDefManager;
//class IWritableCraftDefManager;
class ClientMediaDownloader;
class SingleMediaDownloader;
struct MapDrawControl;
class ModChannelMgr;
class MtEventManager;
struct PointedThing;
<<<<<<< HEAD:src/client.h
class Database;
class Server;
class Mapper;
=======
class MapDatabase;
class Minimap;
>>>>>>> 5.5.0:src/client/client.h
struct MinimapMapblock;
class ChatBackend;
class Camera;
<<<<<<< HEAD:src/client.h

/*
struct QueuedMeshUpdate
{
	v3s16 p;
	MeshMakeData *data;
	bool ack_block_to_server;

	QueuedMeshUpdate();
	~QueuedMeshUpdate();
};
*/
=======
class NetworkPacket;
namespace con {
class Connection;
}
>>>>>>> 5.5.0:src/client/client.h

enum LocalClientState {
	LC_Created,
	LC_Init,
	LC_Ready
};

/*
<<<<<<< HEAD:src/client.h
	A thread-safe queue of mesh update tasks
*/
class MeshUpdateQueue
{
public:
	MeshUpdateQueue();

	~MeshUpdateQueue();

	unsigned int addBlock(v3POS p, std::shared_ptr<MeshMakeData> data, bool urgent);
	std::shared_ptr<MeshMakeData> pop();

	concurrent_unordered_map<v3s16, bool, v3POSHash, v3POSEqual> m_process;

private:
	concurrent_map<unsigned int, unordered_map_v3POS<std::shared_ptr<MeshMakeData>>> m_queue;
	unordered_map_v3POS<unsigned int> m_ranges;
};

struct MeshUpdateResult
{
	v3s16 p;
	MapBlock::mesh_type mesh;

	MeshUpdateResult(v3POS & p_, MapBlock::mesh_type mesh_):
		p(p_),
		mesh(mesh_)
	{
	}
};

class MeshUpdateThread : public UpdateThread
{
private:
	MeshUpdateQueue m_queue_in;
 
protected:
	virtual void doUpdate();

public:

	MeshUpdateThread() : UpdateThread("Mesh") {}

	void enqueueUpdate(v3s16 p, std::shared_ptr<MeshMakeData> data,
			bool urgent);

	MutexedQueue<MeshUpdateResult> m_queue_out;

	v3s16 m_camera_offset;
	int id;
};

enum ClientEventType
{
	CE_NONE,
	CE_PLAYER_DAMAGE,
	CE_PLAYER_FORCE_MOVE,
	CE_DEATHSCREEN,
	CE_SHOW_FORMSPEC,
	CE_SPAWN_PARTICLE,
	CE_ADD_PARTICLESPAWNER,
	CE_DELETE_PARTICLESPAWNER,
	CE_HUDADD,
	CE_HUDRM,
	CE_HUDCHANGE,
	CE_SET_SKY,
	CE_OVERRIDE_DAY_NIGHT_RATIO,
};

struct ClientEvent
{
	ClientEventType type;
	union{
		//struct{
		//} none;
		struct{
			u8 amount;
		} player_damage;
		struct{
			f32 pitch;
			f32 yaw;
		} player_force_move;
		struct{
			bool set_camera_point_target;
			f32 camera_point_target_x;
			f32 camera_point_target_y;
			f32 camera_point_target_z;
		} deathscreen;
		struct{
			std::string *formspec;
			std::string *formname;
		} show_formspec;
		//struct{
		//} textures_updated;
		struct{
			v3f *pos;
			v3f *vel;
			v3f *acc;
			f32 expirationtime;
			f32 size;
			bool collisiondetection;
			bool collision_removal;
			bool vertical;
			std::string *texture;
		} spawn_particle;
		struct{
			u16 amount;
			f32 spawntime;
			v3f *minpos;
			v3f *maxpos;
			v3f *minvel;
			v3f *maxvel;
			v3f *minacc;
			v3f *maxacc;
			f32 minexptime;
			f32 maxexptime;
			f32 minsize;
			f32 maxsize;
			bool collisiondetection;
			bool collision_removal;
			u16 attached_id;
			bool vertical;
			std::string *texture;
			u32 id;
		} add_particlespawner;
		struct{
			u32 id;
		} delete_particlespawner;
		struct{
			u32 id;
			u8 type;
			v2f *pos;
			std::string *name;
			v2f *scale;
			std::string *text;
			u32 number;
			u32 item;
			u32 dir;
			v2f *align;
			v2f *offset;
			v3f *world_pos;
			v2s32 * size;
		} hudadd;
		struct{
			u32 id;
		} hudrm;
		struct{
			u32 id;
			HudElementStat stat;
			v2f *v2fdata;
			std::string *sdata;
			u32 data;
			v3f *v3fdata;
			v2s32 * v2s32data;
		} hudchange;
		struct{
			video::SColor *bgcolor;
			std::string *type;
			std::vector<std::string> *params;
		} set_sky;
		struct{
			bool do_override;
			float ratio_f;
		} override_day_night_ratio;
	};
};

/*
=======
>>>>>>> 5.5.0:src/client/client.h
	Packet counter
*/

class PacketCounter
{
public:
	PacketCounter() = default;

	void add(u16 command)
	{
		auto n = m_packets.find(command);
		if (n == m_packets.end())
			m_packets[command] = 1;
		else
			n->second++;
	}

	void clear()
	{
		m_packets.clear();
	}

<<<<<<< HEAD:src/client.h
	void print(std::ostream &o)
	{
		for(std::map<u16, u16>::iterator
				i = m_packets.begin();
				i != m_packets.end(); ++i)
		{
			if (i->second)
			o<<"cmd "<<i->first
					<<" count "<<i->second
					<<std::endl;
		}
	}
=======
	u32 sum() const;
	void print(std::ostream &o) const;
>>>>>>> 5.5.0:src/client/client.h

private:
	// command, count
	std::map<u16, u32> m_packets;
};

class ClientScripting;
class GameUI;

class Client : public con::PeerHandler, public InventoryManager, public IGameDef
{
public:
	/*
		NOTE: Nothing is thread-safe here.
	*/

	Client(
			const char *playername,
<<<<<<< HEAD:src/client.h
			std::string password,
			bool is_simple_singleplayer_game,
=======
			const std::string &password,
			const std::string &address_name,
>>>>>>> 5.5.0:src/client/client.h
			MapDrawControl &control,
			IWritableTextureSource *tsrc,
			IWritableShaderSource *shsrc,
			IWritableItemDefManager *itemdef,
			NodeDefManager *nodedef,
			ISoundManager *sound,
			MtEventManager *event,
			RenderingEngine *rendering_engine,
			bool ipv6,
			GameUI *game_ui
	);

	~Client();
	DISABLE_CLASS_COPY(Client);

	// Load local mods into memory
	void scanModSubfolder(const std::string &mod_name, const std::string &mod_path,
				std::string mod_subpath);
	inline void scanModIntoMemory(const std::string &mod_name, const std::string &mod_path)
	{
		scanModSubfolder(mod_name, mod_path, "");
	}

	/*
	 request all threads managed by client to be stopped
	 */
	void Stop();

	/*
		The name of the local player should already be set when
		calling this, as it is sent in the initialization.
	*/
	void connect(Address address, bool is_local_server);

	/*
		Stuff that references the environment is valid only as
		long as this is not called. (eg. Players)
		If this throws a PeerNotFoundException, the connection has
		timed out.
	*/
	void step(float dtime);

	/*
	 * Command Handlers
	 */

	void handleCommand(NetworkPacket* pkt);

	void handleCommand_Null(NetworkPacket* pkt) {};
	void handleCommand_Deprecated(NetworkPacket* pkt);
	void handleCommand_Hello(NetworkPacket* pkt);
	void handleCommand_AuthAccept(NetworkPacket* pkt);
	void handleCommand_AcceptSudoMode(NetworkPacket* pkt);
	void handleCommand_DenySudoMode(NetworkPacket* pkt);
	void handleCommand_AccessDenied(NetworkPacket* pkt);
	void handleCommand_RemoveNode(NetworkPacket* pkt);
	void handleCommand_AddNode(NetworkPacket* pkt);
	void handleCommand_NodemetaChanged(NetworkPacket *pkt);
	void handleCommand_BlockData(NetworkPacket* pkt);
	void handleCommand_Inventory(NetworkPacket* pkt);
	void handleCommand_TimeOfDay(NetworkPacket* pkt);
	void handleCommand_ChatMessage(NetworkPacket *pkt);
	void handleCommand_ActiveObjectRemoveAdd(NetworkPacket* pkt);
	void handleCommand_ActiveObjectMessages(NetworkPacket* pkt);
	void handleCommand_Movement(NetworkPacket* pkt);
	void handleCommand_Fov(NetworkPacket *pkt);
	void handleCommand_HP(NetworkPacket* pkt);
	void handleCommand_Breath(NetworkPacket* pkt);
	void handleCommand_MovePlayer(NetworkPacket* pkt);
<<<<<<< HEAD:src/client.h
	void handleCommand_PunchPlayer(NetworkPacket* pkt);
=======
>>>>>>> 5.5.0:src/client/client.h
	void handleCommand_DeathScreen(NetworkPacket* pkt);
	void handleCommand_AnnounceMedia(NetworkPacket* pkt);
	void handleCommand_Media(NetworkPacket* pkt);
	void handleCommand_NodeDef(NetworkPacket* pkt);
	void handleCommand_ItemDef(NetworkPacket* pkt);
	void handleCommand_PlaySound(NetworkPacket* pkt);
	void handleCommand_StopSound(NetworkPacket* pkt);
	void handleCommand_FadeSound(NetworkPacket *pkt);
	void handleCommand_Privileges(NetworkPacket* pkt);
	void handleCommand_InventoryFormSpec(NetworkPacket* pkt);
	void handleCommand_DetachedInventory(NetworkPacket* pkt);
	void handleCommand_ShowFormSpec(NetworkPacket* pkt);
	void handleCommand_SpawnParticle(NetworkPacket* pkt);
	void handleCommand_AddParticleSpawner(NetworkPacket* pkt);
	void handleCommand_DeleteParticleSpawner(NetworkPacket* pkt);
	void handleCommand_HudAdd(NetworkPacket* pkt);
	void handleCommand_HudRemove(NetworkPacket* pkt);
	void handleCommand_HudChange(NetworkPacket* pkt);
	void handleCommand_HudSetFlags(NetworkPacket* pkt);
	void handleCommand_HudSetParam(NetworkPacket* pkt);
	void handleCommand_HudSetSky(NetworkPacket* pkt);
	void handleCommand_HudSetSun(NetworkPacket* pkt);
	void handleCommand_HudSetMoon(NetworkPacket* pkt);
	void handleCommand_HudSetStars(NetworkPacket* pkt);
	void handleCommand_CloudParams(NetworkPacket* pkt);
	void handleCommand_OverrideDayNightRatio(NetworkPacket* pkt);
	void handleCommand_LocalPlayerAnimations(NetworkPacket* pkt);
	void handleCommand_EyeOffset(NetworkPacket* pkt);
	void handleCommand_UpdatePlayerList(NetworkPacket* pkt);
	void handleCommand_ModChannelMsg(NetworkPacket *pkt);
	void handleCommand_ModChannelSignal(NetworkPacket *pkt);
	void handleCommand_SrpBytesSandB(NetworkPacket *pkt);
	void handleCommand_FormspecPrepend(NetworkPacket *pkt);
	void handleCommand_CSMRestrictionFlags(NetworkPacket *pkt);
	void handleCommand_PlayerSpeed(NetworkPacket *pkt);
	void handleCommand_MediaPush(NetworkPacket *pkt);
	void handleCommand_MinimapModes(NetworkPacket *pkt);

	void ProcessData(NetworkPacket *pkt);

<<<<<<< HEAD:src/client.h
	// Returns true if something was received
	bool AsyncProcessPacket();
	bool AsyncProcessData();
/*
	void Send(u16 channelnum, SharedBuffer<u8> data, bool reliable);
*/
	void Send(u16 channelnum, const msgpack::sbuffer &data, bool reliable);
=======
>>>>>>> 5.5.0:src/client/client.h
	void Send(NetworkPacket* pkt);

	void interact(InteractAction action, const PointedThing &pointed);

	void sendNodemetaFields(v3s16 p, const std::string &formname,
		const StringMap &fields);
	void sendInventoryFields(const std::string &formname,
		const StringMap &fields);
	void sendInventoryAction(InventoryAction *a);
<<<<<<< HEAD:src/client.h
	void sendChatMessage(const std::string &message);
=======
	void sendChatMessage(const std::wstring &message);
	void clearOutChatQueue();
>>>>>>> 5.5.0:src/client/client.h
	void sendChangePassword(const std::string &oldpassword,
		const std::string &newpassword);
	void sendDamage(u16 damage);
	void sendRespawn();
	void sendReady();
	void sendHaveMedia(const std::vector<u32> &tokens);

	ClientEnvironment& getEnv() { return m_env; }
	ITextureSource *tsrc() { return getTextureSource(); }
	ISoundManager *sound() { return getSoundManager(); }
	static const std::string &getBuiltinLuaPath();
	static const std::string &getClientModsLuaPath();

	const std::vector<ModSpec> &getMods() const override;
	const ModSpec* getModSpec(const std::string &modname) const override;

	// Causes urgent mesh updates (unlike Map::add/removeNodeWithEvent)
<<<<<<< HEAD:src/client.h
	void removeNode(v3s16 p, int fast = 0);
	void addNode(v3s16 p, MapNode n, bool remove_metadata = true, int fast = 0);

	void setPlayerControl(PlayerControl &control);

	void selectPlayerItem(u16 item);
	u16 getPlayerItem() const
	{ return m_playeritem; }
	u16 getPreviousPlayerItem() const
	{ return m_previous_playeritem; }

=======
	void removeNode(v3s16 p);

	// helpers to enforce CSM restrictions
	MapNode CSMGetNode(v3s16 p, bool *is_valid_position);
	int CSMClampRadius(v3s16 pos, int radius);
	v3s16 CSMClampPos(v3s16 pos);

	void addNode(v3s16 p, MapNode n, bool remove_metadata = true);

	void setPlayerControl(PlayerControl &control);

>>>>>>> 5.5.0:src/client/client.h
	// Returns true if the inventory of the local player has been
	// updated from the server. If it is true, it is set to false.
	bool updateWieldedItem();

	/* InventoryManager interface */
	Inventory* getInventory(const InventoryLocation &loc) override;
	void inventoryAction(InventoryAction *a) override;

	// Send the item number 'item' as player item to the server
	void setPlayerItem(u16 item);

	const std::list<std::string> &getConnectedPlayerNames()
	{
		return m_env.getPlayerNames();
	}

	float getAnimationTime();

	int getCrackLevel();
	v3s16 getCrackPos();
	void setCrack(int level, v3s16 pos);

	u16 getHP();

	bool checkPrivilege(const std::string &priv) const
	{ return (m_privileges.count(priv) != 0); }

<<<<<<< HEAD:src/client.h
	bool getChatMessage(std::string &message);
	void typeChatMessage(const std::string& message);
=======
	const std::unordered_set<std::string> &getPrivilegeList() const
	{ return m_privileges; }

	bool getChatMessage(std::wstring &message);
	void typeChatMessage(const std::wstring& message);
>>>>>>> 5.5.0:src/client/client.h

	u64 getMapSeed(){ return m_map_seed; }

	void addUpdateMeshTask(v3s16 blockpos, bool urgent=false, int step = 0);
	// Including blocks at appropriate edges
	void addUpdateMeshTaskWithEdge(v3POS blockpos, bool urgent = false);
	void addUpdateMeshTaskForNode(v3s16 nodepos, bool urgent=false);

	void updateMeshTimestampWithEdge(v3s16 blockpos);

	void updateCameraOffset(v3s16 camera_offset)
	{ m_mesh_update_thread.m_camera_offset = camera_offset; }

	bool hasClientEvents() const { return !m_client_event_queue.empty(); }
	// Get event from queue. If queue is empty, it triggers an assertion failure.
	ClientEvent * getClientEvent();

	bool accessDenied() const { return m_access_denied; }

	bool reconnectRequested() const { return m_access_denied_reconnect; }

	void setFatalError(const std::string &reason)
	{
		m_access_denied = true;
		m_access_denied_reason = reason;
	}
	inline void setFatalError(const LuaError &e)
	{
		setFatalError(std::string("Lua: ") + e.what());
	}

	// Renaming accessDeniedReason to better name could be good as it's used to
	// disconnect client when CSM failed.
	const std::string &accessDeniedReason() const { return m_access_denied_reason; }

	bool itemdefReceived() const
	{ return m_itemdef_received; }
	bool nodedefReceived() const
	{ return m_nodedef_received; }
	bool mediaReceived() const
	{ return !m_media_downloader; }
	bool activeObjectsReceived() const
	{ return m_activeobjects_received; }

	u16 getProtoVersion()
	{ return m_proto_ver; }

	void confirmRegistration();
	bool m_is_registration_confirmation_state = false;
	bool m_simple_singleplayer_mode;

	float mediaReceiveProgress();

	void afterContentReceived();
	void showUpdateProgressTexture(void *args, u32 progress, u32 max_progress);

	float getRTT();
	float getCurRate();

	Minimap* getMinimap() { return m_minimap; }
	void setCamera(Camera* camera) { m_camera = camera; }

	Camera* getCamera () { return m_camera; }
	scene::ISceneManager *getSceneManager();

	bool shouldShowMinimap() const;

	// IGameDef interface
	IItemDefManager* getItemDefManager() override;
	const NodeDefManager* getNodeDefManager() override;
	ICraftDefManager* getCraftDefManager() override;
	ITextureSource* getTextureSource();
	virtual IWritableShaderSource* getShaderSource();
	u16 allocateUnknownNodeId(const std::string &name) override;
	virtual ISoundManager* getSoundManager();
	MtEventManager* getEventManager();
	virtual ParticleManager* getParticleManager();
	bool checkLocalPrivilege(const std::string &priv)
	{ return checkPrivilege(priv); }
	virtual scene::IAnimatedMesh* getMesh(const std::string &filename, bool cache = false);
	const std::string* getModFile(std::string filename);
	ModMetadataDatabase *getModStorageDatabase() override { return m_mod_storage_database; }

	bool registerModStorage(ModMetadata *meta) override;
	void unregisterModStorage(const std::string &name) override;

	// Migrates away old files-based mod storage if necessary
	void migrateModStorage();

	// The following set of functions is used by ClientMediaDownloader
	// Insert a media file appropriately into the appropriate manager
	bool loadMedia(const std::string &data, const std::string &filename,
		bool from_media_push = false);

	// Send a request for conventional media transfer
	void request_media(const std::vector<std::string> &file_requests);

	LocalClientState getState() { return m_state; }

<<<<<<< HEAD:src/client.h
	void makeScreenshot(const std::string & name = "screenshot_", IrrlichtDevice *device = nullptr);
	
	ChatBackend *chat_backend;
=======
	void makeScreenshot();
>>>>>>> 5.5.0:src/client/client.h

	inline void pushToChatQueue(ChatMessage *cec)
	{
		m_chat_queue.push(cec);
	}

	ClientScripting *getScript() { return m_script; }
	const bool modsLoaded() const { return m_mods_loaded; }

	void pushToEventQueue(ClientEvent *event);

	void showMinimap(bool show = true);

	const Address getServerAddress();

	const std::string &getAddressName() const
	{
		return m_address_name;
	}

	inline u64 getCSMRestrictionFlags() const
	{
		return m_csm_restriction_flags;
	}

	inline bool checkCSMRestrictionFlag(CSMRestrictionFlags flag) const
	{
		return m_csm_restriction_flags & flag;
	}

	bool joinModChannel(const std::string &channel) override;
	bool leaveModChannel(const std::string &channel) override;
	bool sendModChannelMessage(const std::string &channel,
			const std::string &message) override;
	ModChannel *getModChannel(const std::string &channel) override;

	const std::string &getFormspecPrepend() const
	{
		return m_env.getLocalPlayer()->formspec_prepend;
	}
private:
	void loadMods();

	// Virtual methods from con::PeerHandler
<<<<<<< HEAD:src/client.h
	void peerAdded(u16 peer_id);
	void deletingPeer(u16 peer_id, bool timeout);
=======
	void peerAdded(con::Peer *peer) override;
	void deletingPeer(con::Peer *peer, bool timeout) override;
>>>>>>> 5.5.0:src/client/client.h

	void initLocalMapSaving(const Address &address,
			const std::string &hostname,
			bool is_local_server);

	void ReceiveAll();
<<<<<<< HEAD:src/client.h
	bool Receive();
=======
>>>>>>> 5.5.0:src/client/client.h

	void sendPlayerPos();

	void deleteAuthData();
	// helper method shared with clientpackethandler
	static AuthMechanism choseAuthMech(const u32 mechs);

<<<<<<< HEAD:src/client.h
	void sendLegacyInit(const std::string &playerName, const std::string &playerPassword);
=======
>>>>>>> 5.5.0:src/client/client.h
	void sendInit(const std::string &playerName);
	void promptConfirmRegistration(AuthMechanism chosen_auth_mechanism);
	void startAuth(AuthMechanism chosen_auth_mechanism);
	void sendDeletedBlocks(std::vector<v3s16> &blocks);
	void sendGotBlocks(const std::vector<v3s16> &blocks);
	void sendRemovedSounds(std::vector<s32> &soundList);

	// Helper function
	inline std::string getPlayerName()
	{ return m_env.getLocalPlayer()->getName(); }

	bool canSendChatMessage() const;

	float m_packetcounter_timer = 0.0f;
	float m_connection_reinit_timer = 0.1f;
	float m_avg_rtt_timer = 0.0f;
	float m_playerpos_send_timer = 0.0f;
	IntervalLimiter m_map_timer_and_unload_interval;

	IWritableTextureSource *m_tsrc;
	IWritableShaderSource *m_shsrc;
	IWritableItemDefManager *m_itemdef;
	NodeDefManager *m_nodedef;
	ISoundManager *m_sound;
	MtEventManager *m_event;
	RenderingEngine *m_rendering_engine;

	MeshUpdateThread m_mesh_update_thread;
private:
	ClientEnvironment m_env;
	ParticleManager m_particle_manager;
<<<<<<< HEAD:src/client.h
public:
	con::Connection m_con;
private:
	IrrlichtDevice *m_device;
	Camera *m_camera;
	Mapper *m_mapper;
	bool m_minimap_disabled_by_server;
=======
	std::unique_ptr<con::Connection> m_con;
	std::string m_address_name;
	Camera *m_camera = nullptr;
	Minimap *m_minimap = nullptr;
	bool m_minimap_disabled_by_server = false;

>>>>>>> 5.5.0:src/client/client.h
	// Server serialization version
	u8 m_server_ser_ver;

	// Used version of the protocol with server
	// Values smaller than 25 only mean they are smaller than 25,
	// and aren't accurate. We simply just don't know, because
	// the server didn't send the version back then.
	// If 0, server init hasn't been received yet.
	u16 m_proto_ver = 0;

<<<<<<< HEAD:src/client.h
	u16 m_playeritem;
	u16 m_previous_playeritem;
	bool m_inventory_updated;
	Inventory *m_inventory_from_server;
	float m_inventory_from_server_age;
	PacketCounter m_packetcounter;
	// Block mesh animation parameters
	float m_animation_time;
	std::atomic_int m_crack_level;
=======
	bool m_update_wielded_item = false;
	Inventory *m_inventory_from_server = nullptr;
	float m_inventory_from_server_age = 0.0f;
	PacketCounter m_packetcounter;
	// Block mesh animation parameters
	float m_animation_time = 0.0f;
	int m_crack_level = -1;
>>>>>>> 5.5.0:src/client/client.h
	v3s16 m_crack_pos;
	// 0 <= m_daynight_i < DAYNIGHT_CACHE_COUNT
	//s32 m_daynight_i;
	//u32 m_daynight_ratio;
<<<<<<< HEAD:src/client.h
	Queue<std::string> m_chat_queue; // todo: convert to std::queue
=======
	std::queue<std::wstring> m_out_chat_queue;
	u32 m_last_chat_message_sent;
	float m_chat_message_allowance = 5.0f;
	std::queue<ChatMessage *> m_chat_queue;
>>>>>>> 5.5.0:src/client/client.h

	// The authentication methods we can use to enter sudo mode (=change password)
	u32 m_sudo_auth_methods;

	// The seed returned by the server in TOCLIENT_INIT is stored here
	u64 m_map_seed = 0;

	// Auth data
	std::string m_playername;
	std::string m_password;
	bool is_simple_singleplayer_game;
	// If set, this will be sent (and cleared) upon a TOCLIENT_ACCEPT_SUDO_MODE
	std::string m_new_password;
	// Usable by auth mechanisms.
	AuthMechanism m_chosen_auth_mech;
	void *m_auth_data = nullptr;

<<<<<<< HEAD:src/client.h
	bool m_access_denied;
	bool m_access_denied_reconnect;
	std::string m_access_denied_reason;
	Queue<ClientEvent> m_client_event_queue;
	//std::queue<ClientEvent> m_client_event_queue;
	bool m_itemdef_received;
	bool m_nodedef_received;
=======
	bool m_access_denied = false;
	bool m_access_denied_reconnect = false;
	std::string m_access_denied_reason = "";
	std::queue<ClientEvent *> m_client_event_queue;
	bool m_itemdef_received = false;
	bool m_nodedef_received = false;
	bool m_activeobjects_received = false;
	bool m_mods_loaded = false;

	std::vector<std::string> m_remote_media_servers;
	// Media downloader, only exists during init
>>>>>>> 5.5.0:src/client/client.h
	ClientMediaDownloader *m_media_downloader;
	// Set of media filenames pushed by server at runtime
	std::unordered_set<std::string> m_media_pushed_files;
	// Pending downloads of dynamic media (key: token)
	std::vector<std::pair<u32, std::shared_ptr<SingleMediaDownloader>>> m_pending_media_downloads;

	// time_of_day speed approximation for old protocol
	bool m_time_of_day_set = false;
	float m_last_time_of_day_f = -1.0f;
	float m_time_of_day_update_timer = 0.0f;

	// An interval for generally sending object positions and stuff
	float m_recommended_send_interval = 0.1f;

	// Sounds
	float m_removed_sounds_check_timer = 0.0f;
	// Mapping from server sound ids to our sound ids
	std::unordered_map<s32, int> m_sounds_server_to_client;
	// And the other way!
	std::unordered_map<int, s32> m_sounds_client_to_server;
	// Relation of client id to object id
	std::unordered_map<int, u16> m_sounds_to_objects;

	// Privileges
	std::unordered_set<std::string> m_privileges;

	// Detached inventories
	// key = name
<<<<<<< HEAD:src/client.h
	UNORDERED_MAP<std::string, Inventory*> m_detached_inventories;
	double m_uptime;
	bool m_simple_singleplayer_mode;
	float m_timelapse_timer;
public:
	bool use_weather = false;
	unsigned int overload = 0;
	void sendDrawControl();
private:
=======
	std::unordered_map<std::string, Inventory*> m_detached_inventories;
>>>>>>> 5.5.0:src/client/client.h

	// Storage for mesh data for creating multiple instances of the same mesh
	StringMap m_mesh_data;

	// own state
	LocalClientState m_state;

	GameUI *m_game_ui;

	// Used for saving server map to disk client-side
	MapDatabase *m_localdb = nullptr;
	IntervalLimiter m_localdb_save_interval;
	u16 m_cache_save_interval;
	Server *m_localserver;

	// Client modding
	ClientScripting *m_script = nullptr;
	std::unordered_map<std::string, ModMetadata *> m_mod_storages;
	ModMetadataDatabase *m_mod_storage_database = nullptr;
	float m_mod_storage_save_timer = 10.0f;
	std::vector<ModSpec> m_mods;
	StringMap m_mod_vfs;

	bool m_shutdown = false;

	// CSM restrictions byteflag
	u64 m_csm_restriction_flags = CSMRestrictionFlags::CSM_RF_NONE;
	u32 m_csm_restriction_noderange = 8;

	std::unique_ptr<ModChannelMgr> m_modchannel_mgr;
};
