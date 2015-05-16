/*
clientiface.h
Copyright (C) 2010-2014 celeron55, Perttu Ahola <celeron55@gmail.com>
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
#ifndef _CLIENTIFACE_H_
#define _CLIENTIFACE_H_

#include "irr_v3d.h"                   // for irrlicht datatypes

#include "constants.h"
#include "serialization.h"             // for SER_FMT_VER_INVALID
#include "jthread/jmutex.h"
#include "util/concurrent_map.h"
#include "util/concurrent_unordered_map.h"
#include "util/unordered_map_hash.h"
#include "network/networkpacket.h"

#include <list>
#include <vector>
#include <map>
#include <set>

#include "msgpack_fix.h"

class MapBlock;
class ServerEnvironment;
class EmergeManager;

/*
 * State Transitions

      Start
  (peer connect)
        |
        v
      /-----------------\
      |                 |
      |    Created      |
      |                 |
      \-----------------/
               |                  depending of the incoming packet
               +---------------------------------------
               v                                      |
+-----------------------------+        +-----------------------------+
|IN:                          |        |IN:                          |
| TOSERVER_INIT_LEGACY        |-----   | TOSERVER_INIT               |      invalid playername,
+-----------------------------+    |   +-----------------------------+  password (for _LEGACY),
               |                   |                  |                       or denied by mod
               | Auth ok           -------------------+---------------------------------
               |                                      |                                |
+-----------------------------+        +-----------------------------+                 |
|OUT:                         |        |OUT:                         |                 |
| TOCLIENT_INIT_LEGACY        |        | TOCLIENT_HELLO              |                 |
+-----------------------------+        +-----------------------------+                 |
               |                                      |                                |
               |                                      |                                |
               v                                      v                                |
      /-----------------\                    /-----------------\                       |
      |                 |                    |                 |                       |
      |  AwaitingInit2  |<---------          |    HelloSent    |                       |
      |                 |         |          |                 |                       |
      \-----------------/         |          \-----------------/                       |
               |                  |                   |                                |
+-----------------------------+   |    *-----------------------------*     Auth fails  |
|IN:                          |   |    |Authentication, depending on |-----------------+
| TOSERVER_INIT2              |   |    | packet sent by client       |                 |
+-----------------------------+   |    *-----------------------------*                 |
               |                  |                   |                                |
               |                  |                   | Authentication                 |
               v                  |                   |  successful                    |
      /-----------------\         |                   v                                |
      |                 |         |    +-----------------------------+                 |
      |    InitDone     |         |    |OUT:                         |                 |
      |                 |         |    | TOCLIENT_AUTH_ACCEPT        |                 |
      \-----------------/         |    +-----------------------------+                 |
               |                  |                   |                                |
+-----------------------------+   ---------------------                                |
|OUT:                         |                                                        |
| TOCLIENT_MOVEMENT           |                                                        |
| TOCLIENT_ITEMDEF            |                                                        |
| TOCLIENT_NODEDEF            |                                                        |
| TOCLIENT_ANNOUNCE_MEDIA     |                                                        |
| TOCLIENT_DETACHED_INVENTORY |                                                        |
| TOCLIENT_TIME_OF_DAY        |                                                        |
+-----------------------------+                                                        |
               |                                                                       |
               |                                                                       |
               |      -----------------------------                                    |
               v      |                           |                                    |
      /-----------------\                         v                                    |
      |                 |             +-----------------------------+                  |
      | DefinitionsSent |             |IN:                          |                  |
      |                 |             | TOSERVER_REQUEST_MEDIA      |                  |
      \-----------------/             | TOSERVER_RECEIVED_MEDIA     |                  |
               |                      +-----------------------------+                  |
               |      ^                           |                                    |
               |      -----------------------------                                    |
               |                                                                       |
+-----------------------------+                        --------------------------------+
|IN:                          |                        |                               |
| TOSERVER_CLIENT_READY       |                        v                               |
+-----------------------------+        +-------------------------------+               |
               |                       |OUT:                           |               |
               v                       | TOCLIENT_ACCESS_DENIED_LEGAGY |               |
+-----------------------------+        +-------------------------------+               |
|OUT:                         |                        |                               |
| TOCLIENT_MOVE_PLAYER        |                        v                               |
| TOCLIENT_PRIVILEGES         |                /-----------------\                     |
| TOCLIENT_INVENTORY_FORMSPEC |                |                 |                     |
| UpdateCrafting              |                |     Denied      |                     |
| TOCLIENT_INVENTORY          |                |                 |                     |
| TOCLIENT_HP (opt)           |                \-----------------/                     |
| TOCLIENT_BREATH             |                                                        |
| TOCLIENT_DEATHSCREEN        |                                                        |
+-----------------------------+                                                        |
              |                                                                        |
              v                                                                        |
      /-----------------\      async mod action (ban, kick)                            |
      |                 |---------------------------------------------------------------
 ---->|     Active      |
 |    |                 |----------------------------------------------
 |    \-----------------/      timeout                                |
 |       |           |                                  +-----------------------------+
 |       |           |                                  |OUT:                         |
 |       |           |                                  | TOCLIENT_DISCONNECT         |
 |       |           |                                  +-----------------------------+
 |       |           |                                                |
 |       |           |                                                v
 |       |  +-----------------------------+                    /-----------------\
 |       |  |IN:                          |                    |                 |
 |       |  | TOSERVER_DISCONNECT         |------------------->|  Disconnecting  |
 |       |  +-----------------------------+                    |                 |
 |       |                                                     \-----------------/
 |       | any auth packet which was
 |       | allowed in TOCLIENT_AUTH_ACCEPT
 |       v
 |    *-----------------------------* Auth      +-------------------------------+
 |    |Authentication, depending on | succeeds  |OUT:                           |
 |    | packet sent by client       |---------->| TOCLIENT_ACCEPT_SUDO_MODE     |
 |    *-----------------------------*           +-------------------------------+
 |                  |                                            |
 |                  | Auth fails                        /-----------------\
 |                  v                                   |                 |
 |    +-------------------------------+                 |    SudoMode     |
 |    |OUT:                           |                 |                 |
 |    | TOCLIENT_DENY_SUDO_MODE       |                 \-----------------/
 |    +-------------------------------+                          |
 |                  |                                            v
 |                  |                               +-----------------------------+
 |                  |    sets password accordingly  |IN:                          |
 -------------------+-------------------------------| TOSERVER_FIRST_SRP          |
                                                   +-----------------------------+

*/
namespace con {
	class Connection;
}

#define CI_ARRAYSIZE(a) (sizeof(a) / sizeof((a)[0]))

enum ClientState
{
	CS_Invalid,
	CS_Disconnecting,
	CS_Denied,
	CS_Created,
	CS_AwaitingInit2,
	CS_HelloSent,
	CS_InitDone,
	CS_DefinitionsSent,
	CS_Active,
	CS_SudoMode
};

enum ClientStateEvent
{
	CSE_Hello,
	CSE_AuthAccept,
	CSE_InitLegacy,
	CSE_GotInit2,
	CSE_SetDenied,
	CSE_SetDefinitionsSent,
	CSE_SetClientReady,
	CSE_SudoSuccess,
	CSE_SudoLeave,
	CSE_Disconnect
};

/*
	Used for queueing and sorting block transfers in containers

	Lower priority number means higher priority.
*/
struct PrioritySortedBlockTransfer
{
	PrioritySortedBlockTransfer(float a_priority, v3s16 a_pos, u16 a_peer_id)
	{
		priority = a_priority;
		pos = a_pos;
		peer_id = a_peer_id;
	}
	bool operator < (const PrioritySortedBlockTransfer &other) const
	{
		return priority < other.priority;
	}
	float priority;
	v3s16 pos;
	u16 peer_id;
};

class RemoteClient
{
public:
	// peer_id=0 means this client has no associated peer
	// NOTE: If client is made allowed to exist while peer doesn't,
	//       this has to be set to 0 when there is no peer.
	//       Also, the client must be moved to some other container.
	u16 peer_id;
	// The serialization version to use with the client
	u8 serialization_version;
	//
	std::atomic_ushort net_proto_version;
	u16 net_proto_version_fm;

	std::atomic_int m_nearest_unsent_reset;
	std::atomic_int wanted_range;
	std::atomic_int range_all;
	std::atomic_int farmesh;
	float fov;
	bool block_overflow;

	ServerEnvironment *m_env;

	/* Authentication information */
	std::string enc_pwd;
	AuthMechanism chosen_mech;
	void * auth_data;
	u32 allowed_auth_mechs;
	u32 allowed_sudo_mechs;

	bool isSudoMechAllowed(AuthMechanism mech)
	{ return allowed_sudo_mechs & mech; }
	bool isMechAllowed(AuthMechanism mech)
	{ return allowed_auth_mechs & mech; }

	RemoteClient(ServerEnvironment *env):
		peer_id(PEER_ID_INEXISTENT),
		serialization_version(SER_FMT_VER_INVALID),
		m_env(env),
		chosen_mech(AUTH_MECHANISM_NONE),
		auth_data(NULL),
		m_time_from_building(9999),
		m_pending_serialization_version(SER_FMT_VER_INVALID),
		m_state(CS_Created),
		m_nearest_unsent_reset_timer(0.0),
		m_nothing_to_send_pause_timer(0.0),
		m_name(""),
		m_version_major(0),
		m_version_minor(0),
		m_version_patch(0),
		m_full_version("unknown"),
		m_supported_compressions(0),
		m_connection_time(getTime(PRECISION_SECONDS))
	{
		net_proto_version = 0;
		net_proto_version_fm = 0;
		m_nearest_unsent_d = 0;
		m_nearest_unsent_reset = 0;

		wanted_range = 9 * MAP_BLOCKSIZE;
		range_all = 0;
		farmesh = 0;
		fov = 72; // g_settings->getFloat("fov");
		block_overflow = 0;
	}
	~RemoteClient()
	{
	}

	/*
		Finds block that should be sent next to the client.
		Environment should be locked when this is called.
		dtime is used for resetting send radius at slow interval
	*/
	int GetNextBlocks(ServerEnvironment *env, EmergeManager* emerge,
			float dtime, double m_uptime, std::vector<PrioritySortedBlockTransfer> &dest);

	void SentBlock(v3s16 p, double time);

	void SetBlockNotSent(v3s16 p);
	void SetBlocksNotSent(std::map<v3s16, MapBlock*> &blocks);
	void SetBlockDeleted(v3s16 p);

	/**
	 * tell client about this block being modified right now.
	 * this information is required to requeue the block in case it's "on wire"
	 * while modification is processed by server
	 * @param p position of modified block
	 */
	void ResendBlockIfOnWire(v3s16 p);

	s32 SendingCount()
	{
		return 0; //return m_blocks_sending.size();
	}

	// Increments timeouts and removes timed-out blocks from list
	// NOTE: This doesn't fix the server-not-sending-block bug
	//       because it is related to emerging, not sending.
	//void RunSendingTimeouts(float dtime, float timeout);

	void PrintInfo(std::ostream &o)
	{
		o<<"RemoteClient "<<peer_id<<": "
				<<"m_blocks_sent.size()="<<m_blocks_sent.size()
				<<", m_nearest_unsent_d="<<m_nearest_unsent_d
				<<", wanted_range="<<wanted_range
				<<std::endl;
	}

	// Time from last placing or removing blocks
	float m_time_from_building;

	/*
		List of active objects that the client knows of.
		Value is dummy.
	*/
	maybe_concurrent_unordered_map<u16, bool> m_known_objects;

	ClientState getState()
		{ return m_state; }

	std::string getName()
		{ return m_name; }

	void setName(std::string name)
		{ m_name = name; }

	/* update internal client state */
	void notifyEvent(ClientStateEvent event);

	/* set expected serialization version */
	void setPendingSerializationVersion(u8 version)
		{ m_pending_serialization_version = version; }

	void setSupportedCompressionModes(u8 byteFlag)
		{ m_supported_compressions = byteFlag; }

	void confirmSerializationVersion()
		{ serialization_version = m_pending_serialization_version; }

	/* get uptime */
	u32 uptime();

	/* set version information */
	void setVersionInfo(u8 major, u8 minor, u8 patch, std::string full) {
		m_version_major = major;
		m_version_minor = minor;
		m_version_patch = patch;
		m_full_version = full;
	}

	/* read version information */
	u8 getMajor() { return m_version_major; }
	u8 getMinor() { return m_version_minor; }
	u8 getPatch() { return m_version_patch; }
	std::string getVersion() { return m_full_version; }
private:
	// Version is stored in here after INIT before INIT2
	u8 m_pending_serialization_version;

	/* current state of client */
	ClientState m_state;

	/*
		Blocks that have been sent to client.
		- These don't have to be sent again.
		- A block is cleared from here when client says it has
		  deleted it from it's memory

		Key is position, value is dummy.
		No MapBlock* is stored here because the blocks can get deleted.
	*/
	concurrent_unordered_map<v3POS, unsigned int, v3POSHash, v3POSEqual> m_blocks_sent;

public:
	std::atomic_int m_nearest_unsent_d;
private:

	v3s16 m_last_center;
	v3f   m_last_direction;
	float m_nearest_unsent_reset_timer;

	// CPU usage optimization
	float m_nothing_to_send_pause_timer;

	/*
		name of player using this client
	*/
	std::string m_name;

	/*
		client information
	 */
	u8 m_version_major;
	u8 m_version_minor;
	u8 m_version_patch;

	std::string m_full_version;

	u8 m_supported_compressions;

	/*
		time this client was created
	 */
	const u32 m_connection_time;
};

class ClientInterface {
public:

	friend class Server;

	ClientInterface(con::Connection* con);
	~ClientInterface();

	/* run sync step */
	void step(float dtime);

	/* get list of active client id's */
	std::vector<u16> getClientIDs(ClientState min_state=CS_Active);

	/* get list of client player names */
	std::vector<std::string> getPlayerNames();

	/* send message to client */
	void send(u16 peer_id, u8 channelnum, NetworkPacket* pkt, bool reliable);

	/* send message to client */
	void send(u16 peer_id, u8 channelnum, const msgpack::sbuffer &data, bool reliable);

	void send(u16 peer_id, u8 channelnum, SharedBuffer<u8> data, bool reliable); //todo: delete

	/* send to all clients */
	void sendToAll(u16 channelnum, SharedBuffer<u8> data, bool reliable);
	void sendToAll(u16 channelnum, msgpack::sbuffer const &buffer, bool reliable);
	void sendToAll(u16 channelnum, NetworkPacket* pkt, bool reliable);

	/* delete a client */
	void DeleteClient(u16 peer_id);

	/* create client */
	void CreateClient(u16 peer_id);

	std::shared_ptr<RemoteClient> getClient(u16 peer_id,  ClientState state_min=CS_Active);

	/* get a client by peer_id */
	RemoteClient* getClientNoEx(u16 peer_id,  ClientState state_min=CS_Active);

	/* get client by peer_id (make sure you have list lock before!*/
	RemoteClient* lockedGetClientNoEx(u16 peer_id,  ClientState state_min=CS_Active);

	/* get state of client by id*/
	ClientState getClientState(u16 peer_id);

	/* set client playername */
	void setPlayerName(u16 peer_id,std::string name);

	/* get protocol version of client */
	u16 getProtocolVersion(u16 peer_id);

	/* set client version */
	void setClientVersion(u16 peer_id, u8 major, u8 minor, u8 patch, std::string full);

	/* event to update client state */
	void event(u16 peer_id, ClientStateEvent event);

	/* Set environment. Do not call this function if environment is already set */
	void setEnv(ServerEnvironment *env)
	{
		assert(m_env == NULL); // pre-condition
		m_env = env;
	}

	static std::string state2Name(ClientState state);

protected:
	//mt compat
	void Lock()
		{  }
	void Unlock()
		{  }


public:
	std::vector<std::shared_ptr<RemoteClient>> getClientList() {
		std::vector<std::shared_ptr<RemoteClient>> clients;
		auto lock = m_clients.lock_shared_rec();
		for(auto & ir : m_clients) {
			auto c = ir.second;
			if (c)
				clients.emplace_back(c);
		}
		return clients;
	}

private:
	/* update internal player list */
	void UpdatePlayerList();

	// Connection
	con::Connection* m_con;
	// Connected clients (behind the con mutex)
	concurrent_map<u16, std::shared_ptr<RemoteClient>> m_clients;
	std::vector<std::string> m_clients_names; //for announcing masterserver

	// Environment
	ServerEnvironment *m_env;
	//JMutex m_env_mutex;

	float m_print_info_timer;

	static const char *statenames[];
};

#endif
