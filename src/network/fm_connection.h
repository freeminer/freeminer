/*
connection.h
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

#ifndef CONNECTION_ENET_HEADER
#define CONNECTION_ENET_HEADER

#include "irrlichttypes_bloated.h"
#include "socket.h"
#include "exceptions.h"
#include "constants.h"
#include "network/networkpacket.h"
#include "util/pointer.h"
#include "util/container.h"
#include "util/thread.h"
#include <iostream>
#include <fstream>
#include <list>
#include <map>

#include "enet/enet.h"
#include "../msgpack_fix.h"
#include "util/msgpack_serialize.h"
#include "threading/concurrent_map.h"
#include "../threading/concurrent_unordered_map.h"

#define CHANNEL_COUNT 3

extern std::ostream *dout_con_ptr;
extern std::ostream *derr_con_ptr;
#define dout_con (*dout_con_ptr)
#define derr_con (*derr_con_ptr)

namespace con
{

/*
	Exceptions
*/
class NotFoundException : public BaseException
{
public:
	NotFoundException(const char *s):
		BaseException(s)
	{}
};

class PeerNotFoundException : public BaseException
{
public:
	PeerNotFoundException(const char *s):
		BaseException(s)
	{}
};

class ConnectionException : public BaseException
{
public:
	ConnectionException(const char *s):
		BaseException(s)
	{}
};

class ConnectionBindFailed : public BaseException
{
public:
	ConnectionBindFailed(const char *s):
		BaseException(s)
	{}
};

/*class ThrottlingException : public BaseException
{
public:
	ThrottlingException(const char *s):
		BaseException(s)
	{}
};*/

class InvalidIncomingDataException : public BaseException
{
public:
	InvalidIncomingDataException(const char *s):
		BaseException(s)
	{}
};

class InvalidOutgoingDataException : public BaseException
{
public:
	InvalidOutgoingDataException(const char *s):
		BaseException(s)
	{}
};

class NoIncomingDataException : public BaseException
{
public:
	NoIncomingDataException(const char *s):
		BaseException(s)
	{}
};

class ProcessedSilentlyException : public BaseException
{
public:
	ProcessedSilentlyException(const char *s):
		BaseException(s)
	{}
};

class Connection;

enum PeerChangeType
{
	PEER_ADDED,
	PEER_REMOVED
};
struct PeerChange
{
	PeerChangeType type;
	u16 peer_id;
	bool timeout;
};

class PeerHandler
{
public:
	PeerHandler()
	{
	}
	virtual ~PeerHandler()
	{
	}
	
	/*
		This is called after the Peer has been inserted into the
		Connection's peer container.
	*/
	virtual void peerAdded(u16 peer_id) = 0;
	/*
		This is called before the Peer has been removed from the
		Connection's peer container.
	*/
	virtual void deletingPeer(u16 peer_id, bool timeout) = 0;
};

/*mt compat*/
typedef enum rtt_stat_type {
	MIN_RTT,
	MAX_RTT,
	AVG_RTT,
	MIN_JITTER,
	MAX_JITTER,
	AVG_JITTER
} rtt_stat_type;

enum ConnectionEventType{
	CONNEVENT_NONE,
	CONNEVENT_DATA_RECEIVED,
	CONNEVENT_PEER_ADDED,
	CONNEVENT_PEER_REMOVED,
	CONNEVENT_BIND_FAILED,
	CONNEVENT_CONNECT_FAILED,
};

struct ConnectionEvent
{
	enum ConnectionEventType type;
	u16 peer_id;
	Buffer<u8> data;
	bool timeout;
	Address address;

	ConnectionEvent(ConnectionEventType type_=CONNEVENT_NONE): type(type_) {}

	std::string describe()
	{
		switch(type){
		case CONNEVENT_NONE:
			return "CONNEVENT_NONE";
		case CONNEVENT_DATA_RECEIVED:
			return "CONNEVENT_DATA_RECEIVED";
		case CONNEVENT_PEER_ADDED:
			return "CONNEVENT_PEER_ADDED";
		case CONNEVENT_PEER_REMOVED:
			return "CONNEVENT_PEER_REMOVED";
		case CONNEVENT_BIND_FAILED:
			return "CONNEVENT_BIND_FAILED";
		case CONNEVENT_CONNECT_FAILED:
			return "CONNEVENT_CONNECT_FAILED";
		}
		return "Invalid ConnectionEvent";
	}
	
	void dataReceived(u16 peer_id_, SharedBuffer<u8> data_)
	{
		type = CONNEVENT_DATA_RECEIVED;
		peer_id = peer_id_;
		data = data_;
	}
	void peerAdded(u16 peer_id_)
	{
		type = CONNEVENT_PEER_ADDED;
		peer_id = peer_id_;
		// address = address_;
	}
	void peerRemoved(u16 peer_id_, bool timeout_)
	{
		type = CONNEVENT_PEER_REMOVED;
		peer_id = peer_id_;
		timeout = timeout_;
		// address = address_;
	}
	void bindFailed()
	{
		type = CONNEVENT_BIND_FAILED;
	}
};

enum ConnectionCommandType{
	CONNCMD_NONE,
	CONNCMD_SERVE,
	CONNCMD_CONNECT,
	CONNCMD_DISCONNECT,
	CONNCMD_DISCONNECT_PEER,
	CONNCMD_SEND,
	CONNCMD_SEND_TO_ALL,
	CONNCMD_DELETE_PEER,
};

struct ConnectionCommand
{
	enum ConnectionCommandType type;
	Address address;
	u16 peer_id;
	u8 channelnum;
	Buffer<u8> data;
	bool reliable;
	
	ConnectionCommand(): type(CONNCMD_NONE) {}

	void serve(Address address_)
	{
		type = CONNCMD_SERVE;
		address = address_;
	}
	void connect(Address address_)
	{
		type = CONNCMD_CONNECT;
		address = address_;
	}
	void disconnect()
	{
		type = CONNCMD_DISCONNECT;
	}
	void send(u16 peer_id_, u8 channelnum_,
			SharedBuffer<u8> data_, bool reliable_)
	{
		type = CONNCMD_SEND;
		peer_id = peer_id_;
		channelnum = channelnum_;
		data = data_;
		reliable = reliable_;
	}
	void sendToAll(u8 channelnum_, SharedBuffer<u8> data_, bool reliable_)
	{
		type = CONNCMD_SEND_TO_ALL;
		channelnum = channelnum_;
		data = data_;
		reliable = reliable_;
	}
	void deletePeer(u16 peer_id_)
	{
		type = CONNCMD_DELETE_PEER;
		peer_id = peer_id_;
	}
	void disconnect_peer(u16 peer_id_)
	{
		type = CONNCMD_DISCONNECT_PEER;
		peer_id = peer_id_;
	}
};

class Connection: public thread_pool
{
public:
	Connection(u32 protocol_id, u32 max_packet_size, float timeout, bool ipv6,
			PeerHandler *peerhandler = nullptr);
	~Connection();
	void * run();

	/* Interface */

	ConnectionEvent getEvent();
	ConnectionEvent waitEvent(u32 timeout_ms);
	void putCommand(ConnectionCommand &c);

	void Serve(Address bind_addr);
	void Connect(Address address);
	bool Connected();
	void Disconnect();
	u32 Receive(NetworkPacket* pkt, int timeout = 1);
	void SendToAll(u8 channelnum, SharedBuffer<u8> data, bool reliable);
	void Send(u16 peer_id, u8 channelnum, SharedBuffer<u8> data, bool reliable);
	void Send(u16 peer_id, u8 channelnum, const msgpack::sbuffer &buffer, bool reliable);
	u16 GetPeerID(){ return m_peer_id; }
	void DeletePeer(u16 peer_id);
	Address GetPeerAddress(u16 peer_id);
	float getPeerStat(u16 peer_id, rtt_stat_type type);
	void DisconnectPeer(u16 peer_id);

private:
	void putEvent(ConnectionEvent &e);
	void processCommand(ConnectionCommand &c);
	void send(float dtime);
	void receive();
	void runTimeouts(float dtime);
	void serve(Address address);
	void connect(Address address);
	void disconnect();
	void sendToAll(u8 channelnum, SharedBuffer<u8> data, bool reliable);
	void send(u16 peer_id, u8 channelnum, SharedBuffer<u8> data, bool reliable);
	ENetPeer* getPeer(u16 peer_id);
	bool deletePeer(u16 peer_id, bool timeout);

	MutexedQueue<ConnectionEvent> m_event_queue;
	MutexedQueue<ConnectionCommand> m_command_queue;

	u32 m_protocol_id;
	u32 m_max_packet_size;
	float m_timeout;
	ENetHost *m_enet_host;
	//ENetPeer *m_peer;
	u16 m_peer_id;

	concurrent_map<u16, ENetPeer*> m_peers;
	concurrent_unordered_map<u16, Address> m_peers_address;
	//Mutex m_peers_mutex;

	// Backwards compatibility
	PeerHandler *m_bc_peerhandler;
	unsigned int m_last_recieved;
	unsigned int m_last_recieved_warn;

	void SetPeerID(u16 id){ m_peer_id = id; }
	u32 GetProtocolID(){ return m_protocol_id; }
	void PrintInfo(std::ostream &out);
	void PrintInfo();
	std::string getDesc();
	unsigned int timeout_mul;
};


bool parse_msgpack_packet(char *data, u32 datasize, MsgpackPacket *packet, int *command, msgpack::unpacked *msg);
} // namespace

#endif

