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

#ifndef CLIENT_HEADER
#define CLIENT_HEADER

#include "connection.h"
#include "environment.h"
#include "irrlichttypes_extrabloated.h"
#include "jthread/jmutex.h"
#include <ostream>
#include <map>
#include <set>
#include <vector>
#include "clientobject.h"
#include "gamedef.h"
#include "inventorymanager.h"
#include "localplayer.h"
#include "hud.h"
#include "particles.h"
#include "util/thread_pool.h"
#include "util/unordered_map_hash.h"

struct MeshMakeData;
class MapBlockMesh;
class IWritableTextureSource;
class IWritableShaderSource;
class IWritableItemDefManager;
class IWritableNodeDefManager;
//class IWritableCraftDefManager;
class ClientMediaDownloader;
struct MapDrawControl;
class MtEventManager;
struct PointedThing;

enum LocalClientState {
	LC_Created,
	LC_Init,
	LC_Ready
};

/*
	A thread-safe queue of mesh update tasks
*/
class MeshUpdateQueue
{
public:
	MeshUpdateQueue();

	~MeshUpdateQueue();

	void addBlock(v3s16 p, std::shared_ptr<MeshMakeData> data, bool urgent);
	std::shared_ptr<MeshMakeData> pop();

	shared_unordered_map<v3s16, bool, v3s16Hash, v3s16Equal> m_process;
private:
	shared_map<unsigned int, std::unordered_map<v3s16, std::shared_ptr<MeshMakeData>, v3s16Hash, v3s16Equal>> m_queue;
	std::unordered_map<v3s16, unsigned int, v3s16Hash, v3s16Equal> m_ranges;
};

struct MeshUpdateResult
{
	v3s16 p;
	std::shared_ptr<MapBlockMesh> mesh;

	MeshUpdateResult(v3s16 & p_, std::shared_ptr<MapBlockMesh> mesh_):
		p(p_),
		mesh(mesh_)
	{
	}
};

class MeshUpdateThread : public thread_pool
{
public:

	MeshUpdateThread(IGameDef *gamedef, int id_ = 0):
		m_gamedef(gamedef)
		,id(id_)
	{
	}

	void * Thread();

	MeshUpdateQueue m_queue_in;

	MutexedQueue<MeshUpdateResult> m_queue_out;

	IGameDef *m_gamedef;
	
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
		struct{
		} none;
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
		struct{
		} textures_updated;
		struct{
			v3f *pos;
			v3f *vel;
			v3f *acc;
			f32 expirationtime;
			f32 size;
			bool collisiondetection;
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
	Packet counter
*/

class PacketCounter
{
public:
	PacketCounter()
	{
	}

	void add(u16 command)
	{
		std::map<u16, u16>::iterator n = m_packets.find(command);
		if(n == m_packets.end())
		{
			m_packets[command] = 1;
		}
		else
		{
			n->second++;
		}
	}

	void clear()
	{
		for(std::map<u16, u16>::iterator
				i = m_packets.begin();
				i != m_packets.end(); ++i)
		{
			i->second = 0;
		}
	}

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

private:
	// command, count
	std::map<u16, u16> m_packets;
};

class Client : public con::PeerHandler, public InventoryManager, public IGameDef
{
public:
	/*
		NOTE: Nothing is thread-safe here.
	*/

	Client(
			IrrlichtDevice *device,
			const char *playername,
			std::string password,
			MapDrawControl &control,
			IWritableTextureSource *tsrc,
			IWritableShaderSource *shsrc,
			IWritableItemDefManager *itemdef,
			IWritableNodeDefManager *nodedef,
			ISoundManager *sound,
			MtEventManager *event,
			bool ipv6
			,bool simple_singleplayer_mode
	);
	
	~Client();

	/*
	 request all threads managed by client to be stopped
	 */
	void Stop();

	/*
		The name of the local player should already be set when
		calling this, as it is sent in the initialization.
	*/
	void connect(Address address);

	/*
		Stuff that references the environment is valid only as
		long as this is not called. (eg. Players)
		If this throws a PeerNotFoundException, the connection has
		timed out.
	*/
	void step(float dtime);

	void ProcessData(u8 *data, u32 datasize, u16 sender_peer_id);
	// Returns true if something was received
	bool AsyncProcessPacket();
	bool AsyncProcessData();
	void Send(u16 channelnum, SharedBuffer<u8> data, bool reliable);

	void interact(u8 action, const PointedThing& pointed);

	void sendNodemetaFields(v3s16 p, const std::string &formname,
			const std::map<std::string, std::string> &fields);
	void sendInventoryFields(const std::string &formname,
			const std::map<std::string, std::string> &fields);
	void sendInventoryAction(InventoryAction *a);
	void sendChatMessage(const std::wstring &message);
	void sendChangePassword(const std::wstring &oldpassword,
	                        const std::wstring &newpassword);
	void sendDamage(u8 damage);
	void sendBreath(u16 breath);
	void sendRespawn();
	void sendReady();

	ClientEnvironment& getEnv()
	{ return m_env; }
	
	// Causes urgent mesh updates (unlike Map::add/removeNodeWithEvent)
	void removeNode(v3s16 p);
	void addNode(v3s16 p, MapNode n, bool remove_metadata = true);
	
	void setPlayerControl(PlayerControl &control);

	void selectPlayerItem(u16 item);
	u16 getPlayerItem() const
	{ return m_playeritem; }

	// Returns true if the inventory of the local player has been
	// updated from the server. If it is true, it is set to false.
	bool getLocalInventoryUpdated();
	// Copies the inventory of the local player to parameter
	void getLocalInventory(Inventory &dst);
	
	/* InventoryManager interface */
	Inventory* getInventory(const InventoryLocation &loc);
	void inventoryAction(InventoryAction *a);

	// Gets closest object pointed by the shootline
	// Returns NULL if not found
	ClientActiveObject * getSelectedActiveObject(
			f32 max_d,
			v3f from_pos_f_on_map,
			core::line3d<f32> shootline_on_map
	);

	std::list<std::string> getConnectedPlayerNames();

	float getAnimationTime();

	int getCrackLevel();
	void setCrack(int level, v3s16 pos);

	u16 getHP();
	u16 getBreath();

	bool checkPrivilege(const std::string &priv)
	{ return (m_privileges.count(priv) != 0); }

	bool getChatMessage(std::wstring &message);
	void typeChatMessage(const std::wstring& message);

	u64 getMapSeed(){ return m_map_seed; }

	void addUpdateMeshTask(v3s16 blockpos, bool urgent=false);
	// Including blocks at appropriate edges
	void addUpdateMeshTaskWithEdge(v3s16 blockpos, bool urgent=false);
	void addUpdateMeshTaskForNode(v3s16 nodepos, bool urgent=false);

	void updateMeshTimestampWithEdge(v3s16 blockpos);

	void updateCameraOffset(v3s16 camera_offset)
	{ m_mesh_update_thread.m_camera_offset = camera_offset; }

	// Get event from queue. CE_NONE is returned if queue is empty.
	ClientEvent getClientEvent();
	
	bool accessDenied()
	{ return m_access_denied; }

	std::wstring accessDeniedReason()
	{ return m_access_denied_reason; }

	bool itemdefReceived()
	{ return m_itemdef_received; }
	bool nodedefReceived()
	{ return m_nodedef_received; }
	bool mediaReceived()
	{ return m_media_downloader == NULL; }

	float mediaReceiveProgress();

	void afterContentReceived(IrrlichtDevice *device, gui::IGUIFont* font);

	float getRTT(void);
	float getCurRate(void);
	float getAvgRate(void);

	// IGameDef interface
	virtual IItemDefManager* getItemDefManager();
	virtual INodeDefManager* getNodeDefManager();
	virtual ICraftDefManager* getCraftDefManager();
	virtual ITextureSource* getTextureSource();
	virtual IShaderSource* getShaderSource();
	virtual u16 allocateUnknownNodeId(const std::string &name);
	virtual ISoundManager* getSoundManager();
	virtual MtEventManager* getEventManager();
	virtual bool checkLocalPrivilege(const std::string &priv)
	{ return checkPrivilege(priv); }
	virtual scene::IAnimatedMesh* getMesh(const std::string &filename);

	// The following set of functions is used by ClientMediaDownloader
	// Insert a media file appropriately into the appropriate manager
	bool loadMedia(const std::string &data, const std::string &filename);
	// Send a request for conventional media transfer
	void request_media(const std::list<std::string> &file_requests);
	// Send a notification that no conventional media transfer is needed
	void received_media();

	LocalClientState getState() { return m_state; }

private:

	// Virtual methods from con::PeerHandler
	void peerAdded(con::Peer *peer);
	void deletingPeer(con::Peer *peer, bool timeout);
	
	void ReceiveAll();
	void Receive();
	
	void sendPlayerPos();
	// Send the item number 'item' as player item to the server
	void sendPlayerItem(u16 item);
	
	float m_packetcounter_timer;
	float m_connection_reinit_timer;
	float m_avg_rtt_timer;
	float m_playerpos_send_timer;
	float m_ignore_damage_timer; // Used after server moves player
	IntervalLimiter m_map_timer_and_unload_interval;

	IWritableTextureSource *m_tsrc;
	IWritableShaderSource *m_shsrc;
	IWritableItemDefManager *m_itemdef;
	IWritableNodeDefManager *m_nodedef;
	ISoundManager *m_sound;
	MtEventManager *m_event;

public:
	MeshUpdateThread m_mesh_update_thread;
private:
	ClientEnvironment m_env;
public:
	con::Connection m_con;
private:
	IrrlichtDevice *m_device;
	// Server serialization version
	u8 m_server_ser_ver;
	u16 m_playeritem;
	bool m_inventory_updated;
	Inventory *m_inventory_from_server;
	float m_inventory_from_server_age;
	std::set<v3s16> m_active_blocks;
	PacketCounter m_packetcounter;
	// Block mesh animation parameters
	float m_animation_time;
	int m_crack_level;
	v3s16 m_crack_pos;
	// 0 <= m_daynight_i < DAYNIGHT_CACHE_COUNT
	//s32 m_daynight_i;
	//u32 m_daynight_ratio;
	Queue<std::wstring> m_chat_queue;
	// The seed returned by the server in TOCLIENT_INIT is stored here
	u64 m_map_seed;
	std::string m_password;
	bool m_access_denied;
	std::wstring m_access_denied_reason;
	Queue<ClientEvent> m_client_event_queue;
	bool m_itemdef_received;
	bool m_nodedef_received;
	ClientMediaDownloader *m_media_downloader;

	// time_of_day speed approximation for old protocol
	bool m_time_of_day_set;
	float m_last_time_of_day_f;
	float m_time_of_day_update_timer;

	// An interval for generally sending object positions and stuff
	float m_recommended_send_interval;

	// Sounds
	float m_removed_sounds_check_timer;
	// Mapping from server sound ids to our sound ids
	std::map<s32, int> m_sounds_server_to_client;
	// And the other way!
	std::map<int, s32> m_sounds_client_to_server;
	// And relations to objects
	std::map<int, u16> m_sounds_to_objects;

	// Privileges
	std::set<std::string> m_privileges;

	// Detached inventories
	// key = name
	std::map<std::string, Inventory*> m_detached_inventories;
	double m_uptime;
	bool m_simple_singleplayer_mode;

	// Storage for mesh data for creating multiple instances of the same mesh
	std::map<std::string, std::string> m_mesh_data;

	// own state
	LocalClientState m_state;
};

#endif // !CLIENT_HEADER

