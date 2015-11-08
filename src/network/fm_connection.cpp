/*
connection.cpp
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

#include "network/fm_connection.h"
#include "serialization.h"
#include "log.h"
#include "porting.h"
#include "util/serialize.h"
#include "util/numeric.h"
#include "util/string.h"
#include "settings.h"
#include "profiler.h"

namespace con
{

//very ugly windows hack
#if ( defined(_MSC_VER) || defined(__MINGW32__) ) && defined(ENET_IPV6)

#include <winsock2.h>
#include <ws2tcpip.h>

int inet_pton(int af, const char *src, void *dst)
{
  struct sockaddr_storage ss;
  int size = sizeof(ss);
  char src_copy[INET6_ADDRSTRLEN+1];

  ZeroMemory(&ss, sizeof(ss));
  /* stupid non-const API */
  strncpy (src_copy, src, INET6_ADDRSTRLEN+1);
  src_copy[INET6_ADDRSTRLEN] = 0;

  if (WSAStringToAddress(src_copy, af, NULL, (struct sockaddr *)&ss, &size) == 0) {
    switch(af) {
      case AF_INET:
    *(struct in_addr *)dst = ((struct sockaddr_in *)&ss)->sin_addr;
    return 1;
      case AF_INET6:
    *(struct in6_addr *)dst = ((struct sockaddr_in6 *)&ss)->sin6_addr;
    return 1;
    }
  }
  return 0;
}

const char *inet_ntop(int af, const void *src, char *dst, socklen_t size)
{
  struct sockaddr_storage ss;
  unsigned long s = size;

  ZeroMemory(&ss, sizeof(ss));
  ss.ss_family = af;

  switch(af) {
    case AF_INET:
      ((struct sockaddr_in *)&ss)->sin_addr = *(struct in_addr *)src;
      break;
    case AF_INET6:
      ((struct sockaddr_in6 *)&ss)->sin6_addr = *(struct in6_addr *)src;
      break;
    default:
      return NULL;
  }
  /* cannot direclty use &size because of strict aliasing rules */
  return (WSAAddressToString((struct sockaddr *)&ss, sizeof(ss), NULL, dst, &s) == 0)?
          dst : NULL;
}
#endif

/*
	Connection
*/

Connection::Connection(u32 protocol_id, u32 max_packet_size, float timeout,
		bool ipv6, PeerHandler *peerhandler):
	m_protocol_id(protocol_id),
	m_max_packet_size(max_packet_size),
	m_timeout(timeout),
	m_enet_host(0),
	m_peer_id(0),
	m_bc_peerhandler(peerhandler),
	m_last_recieved(0),
	m_last_recieved_warn(0),
	timeout_mul(0)
{
	timeout_mul = g_settings->getU16("timeout_mul");
	if (!timeout_mul)
		timeout_mul = 1;
	start();
}


Connection::~Connection()
{
	join();
	if(m_enet_host)
		enet_host_destroy(m_enet_host);
	m_enet_host = nullptr;
}

/* Internal stuff */

void * Connection::run()
{
	reg("Connection");

	while(!stopRequested())
	{
		while(!m_command_queue.empty()){
			ConnectionCommand c = m_command_queue.pop_frontNoEx();
			processCommand(c);
		}
		receive();
	}

	return nullptr;
}

void Connection::putEvent(ConnectionEvent &e)
{
	//if(e.type == CONNEVENT_NONE) return;
	m_event_queue.push_back(e);
}

void Connection::processCommand(ConnectionCommand &c)
{
	switch(c.type){
	case CONNCMD_NONE:
		dout_con<<getDesc()<<" processing CONNCMD_NONE"<<std::endl;
		return;
	case CONNCMD_SERVE:
		dout_con<<getDesc()<<" processing CONNCMD_SERVE port="
				<<c.address.getPort()<<std::endl;
		serve(c.address);
		return;
	case CONNCMD_CONNECT:
		dout_con<<getDesc()<<" processing CONNCMD_CONNECT"<<std::endl;
		connect(c.address);
		return;
	case CONNCMD_DISCONNECT:
		dout_con<<getDesc()<<" processing CONNCMD_DISCONNECT"<<std::endl;
		disconnect();
		return;
	case CONNCMD_DISCONNECT_PEER:
		dout_con<<getDesc()<<" processing CONNCMD_DISCONNECT"<<std::endl;
		deletePeer(c.peer_id, false); // its correct ?
		//DisconnectPeer(c.peer_id);
		return;
	case CONNCMD_SEND:
		dout_con<<getDesc()<<" processing CONNCMD_SEND"<<std::endl;
		send(c.peer_id, c.channelnum, c.data, c.reliable);
		return;
	case CONNCMD_SEND_TO_ALL:
		dout_con<<getDesc()<<" processing CONNCMD_SEND_TO_ALL"<<std::endl;
		sendToAll(c.channelnum, c.data, c.reliable);
		return;
	case CONNCMD_DELETE_PEER:
		dout_con<<getDesc()<<" processing CONNCMD_DELETE_PEER"<<std::endl;
		deletePeer(c.peer_id, false);
		return;
	}
}

// Receive packets from the network and buffers and create ConnectionEvents
void Connection::receive()
{
	if (!m_enet_host) {
		return;
	}
	ENetEvent event;
	int ret = enet_host_service(m_enet_host, & event, 10);
	if (ret > 0)
	{
		m_last_recieved = porting::getTimeMs();
		m_last_recieved_warn = 0;
		switch (event.type)
		{
		case ENET_EVENT_TYPE_CONNECT:
			{
				//MutexAutoLock peerlock(m_peers_mutex);
				u16 peer_id = 0;
				static u16 last_try = PEER_ID_SERVER + 1;
				if (m_peers.size() > 0) {
					for (int i = 0; i < 1000; ++i) {
						if (last_try > 30000)
							last_try = PEER_ID_SERVER;
						++last_try;
						if (!m_peers.count(last_try)) {
							peer_id = last_try;
							break;
						}
					}
				} else {
					peer_id = last_try;
				}
				if (!peer_id)
					last_try = peer_id = m_peers.rbegin()->first + 1;

				m_peers.set(peer_id, event.peer);
				m_peers_address.set(peer_id, Address(event.peer->address.host, event.peer->address.port));

				event.peer->data = new u16;
				*((u16*)event.peer->data) = peer_id;

				// Create peer addition event
				ConnectionEvent e;
				e.peerAdded(peer_id);
				putEvent(e);
			}
			break;
		case ENET_EVENT_TYPE_RECEIVE:
			{
				ConnectionEvent e;
				SharedBuffer<u8> resultdata(event.packet->data, event.packet->dataLength);
				e.dataReceived(*(u16*)event.peer->data, resultdata);
				putEvent(e);
			}

			/* Clean up the packet now that we're done using it. */
			enet_packet_destroy (event.packet);
			break;
		case ENET_EVENT_TYPE_DISCONNECT:
			deletePeer(*((u16*)event.peer->data), false);

			/* Reset the peer's client information. */
			delete (u16*)event.peer->data;

			break;
		case ENET_EVENT_TYPE_NONE:
			break;
		}
	} else if (ret < 0) {
		infostream<<"recieve enet_host_service failed = "<< ret << std::endl;
		if (m_peers.count(PEER_ID_SERVER))
			deletePeer(PEER_ID_SERVER,  false);
	} else { //0
		if (m_peers.count(PEER_ID_SERVER) && m_last_recieved) { //ugly fix. todo: fix enet and remove
			unsigned int time = porting::getTimeMs();
			const unsigned int t1 = 10000, t2 = 30000 * timeout_mul, t3 = 60000 * timeout_mul;
			unsigned int wait = time - m_last_recieved;
			if (wait > t3 && m_last_recieved_warn > t2) {
				errorstream<<"connection lost [60s], disconnecting."<<std::endl;
#if defined(__has_feature)
#if __has_feature(thread_sanitizer) || __has_feature(address_sanitizer)
				if (0)
#endif
#endif
				{
					deletePeer(PEER_ID_SERVER,  false);
				}
				m_last_recieved_warn = 0;
				m_last_recieved = 0;
			} else if (wait > t2 && m_last_recieved_warn > t1 && m_last_recieved_warn < t2) {
				errorstream<<"connection lost [30s]!"<<std::endl;
				m_last_recieved_warn = time - m_last_recieved;
			} else if (wait > t1 && m_last_recieved_warn < t1) {
				errorstream<<"connection lost [10s]? ping."<<std::endl;
				enet_peer_ping(m_peers.get(PEER_ID_SERVER));
				m_last_recieved_warn = wait;
			}
		}
	}
}

// host
void Connection::serve(Address bind_addr)
{
	ENetAddress address;
#if defined(ENET_IPV6)
	address.host = bind_addr.getAddress6().sin6_addr; // in6addr_any;
#else
	address.host = bind_addr.getAddress().sin_addr.s_addr; // ENET_HOST_ANY;
#endif
	address.port = bind_addr.getPort(); // fmtodo

	m_enet_host = enet_host_create(&address, g_settings->getU16("max_users"), CHANNEL_COUNT, 0, 0);
	if (m_enet_host == NULL) {
		ConnectionEvent ev(CONNEVENT_BIND_FAILED);
		putEvent(ev);
	}
}

// peer
void Connection::connect(Address addr)
{
	m_last_recieved = porting::getTimeMs();
	//MutexAutoLock peerlock(m_peers_mutex);
	//m_peers.lock_unique_rec();
	auto node = m_peers.find(PEER_ID_SERVER);
	if(node != m_peers.end()){
		//throw ConnectionException("Already connected to a server");
		ConnectionEvent ev(CONNEVENT_CONNECT_FAILED);
		putEvent(ev);
	}

	m_enet_host = enet_host_create(NULL, 1, 0, 0, 0);
	ENetAddress address;
#if defined(ENET_IPV6)
	if (!addr.isIPv6())
		inet_pton (AF_INET6, ("::ffff:"+addr.serializeString()).c_str(), &address.host);
	else
		address.host = addr.getAddress6().sin6_addr;
#else
	if (addr.isIPv6()) {
		//throw ConnectionException("Cant connect to ipv6 address");
		ConnectionEvent ev(CONNEVENT_CONNECT_FAILED);
		putEvent(ev);
	} else {
		address.host = addr.getAddress().sin_addr.s_addr;
	}
#endif

	address.port = addr.getPort();
	ENetPeer *peer = enet_host_connect(m_enet_host, &address, CHANNEL_COUNT, 0);
	peer->data = new u16;
	*((u16*)peer->data) = PEER_ID_SERVER;

	ENetEvent event;
	int ret = enet_host_service (m_enet_host, & event, 5000);
	if (ret > 0 && event.type == ENET_EVENT_TYPE_CONNECT) {
		m_peers.set(PEER_ID_SERVER, peer);
		m_peers_address.set(PEER_ID_SERVER, addr);
	} else {
		errorstream<<"connect enet_host_service ret="<<ret<<std::endl;
		if (ret == 0) {
			ConnectionEvent ev(CONNEVENT_CONNECT_FAILED);
			putEvent(ev);
		}

		/* Either the 5 seconds are up or a disconnect event was */
		/* received. Reset the peer in the event the 5 seconds   */
		/* had run out without any significant event.            */
		enet_peer_reset(peer);
	}
}

void Connection::disconnect()
{
	//MutexAutoLock peerlock(m_peers_mutex);
	m_peers.lock_shared_rec();
	for (auto i = m_peers.begin();
			i != m_peers.end(); ++i)
		enet_peer_disconnect(i->second, 0);
}

void Connection::sendToAll(u8 channelnum, SharedBuffer<u8> data, bool reliable)
{
	ENetPacket *packet = enet_packet_create(*data, data.getSize(), reliable ? ENET_PACKET_FLAG_RELIABLE : 0);
	enet_host_broadcast(m_enet_host, 0, packet);
}

void Connection::send(u16 peer_id, u8 channelnum,
		SharedBuffer<u8> data, bool reliable)
{
	{
		//MutexAutoLock peerlock(m_peers_mutex);
		if (m_peers.find(peer_id) == m_peers.end())
			return;
	}
	//dout_con<<getDesc()<<" sending to peer_id="<<peer_id<<std::endl;

	assert(channelnum < CHANNEL_COUNT);

	ENetPacket *packet = enet_packet_create(*data, data.getSize(), reliable ? ENET_PACKET_FLAG_RELIABLE : 0);

	ENetPeer *peer = getPeer(peer_id);
	if(!peer) {
		deletePeer(peer_id, false);
		return;
	}
	if (enet_peer_send(peer, channelnum, packet) < 0) {
		infostream<<"enet_peer_send failed peer="<<peer_id<<" reliable="<<reliable<< " size="<<data.getSize()<<std::endl;
		if (reliable)
			deletePeer(peer_id, false);
		return;
	}
}

ENetPeer* Connection::getPeer(u16 peer_id)
{
	auto node = m_peers.find(peer_id);

	if(node == m_peers.end())
		return NULL;

	return node->second;
}

bool Connection::deletePeer(u16 peer_id, bool timeout)
{
	//MutexAutoLock peerlock(m_peers_mutex);
	if(m_peers.find(peer_id) == m_peers.end())
		return false;

	// Create event
	ConnectionEvent e;
	e.peerRemoved(peer_id, timeout);
	putEvent(e);

	// delete m_peers[peer_id]; -- enet should handle this
	m_peers.erase(peer_id);
	m_peers_address.erase(peer_id);
	return true;
}

/* Interface */

ConnectionEvent Connection::getEvent()
{
	if(m_event_queue.empty()){
		ConnectionEvent e;
		e.type = CONNEVENT_NONE;
		return e;
	}
	return m_event_queue.pop_frontNoEx();
}

ConnectionEvent Connection::waitEvent(u32 timeout_ms)
{
	try{
		return m_event_queue.pop_front(timeout_ms);
	} catch(ItemNotFoundException &ex){
		ConnectionEvent e;
		e.type = CONNEVENT_NONE;
		return e;
	}
}

void Connection::putCommand(ConnectionCommand &c)
{
	m_command_queue.push_back(c);
}

void Connection::Serve(Address bind_address)
{
	ConnectionCommand c;
	c.serve(bind_address);
	putCommand(c);
}

void Connection::Connect(Address address)
{
	ConnectionCommand c;
	c.connect(address);
	putCommand(c);
}

bool Connection::Connected()
{
	//MutexAutoLock peerlock(m_peers_mutex);

	auto node = m_peers.find(PEER_ID_SERVER);
	if(node == m_peers.end())
		return false;

	// TODO: why do we even need to know our peer id?
	if (!m_peer_id)
		m_peer_id = 2;

	if(m_peer_id == PEER_ID_INEXISTENT)
		return false;

	return true;
}

void Connection::Disconnect()
{
	ConnectionCommand c;
	c.disconnect();
	putCommand(c);
}

u32 Connection::Receive(NetworkPacket* pkt, int timeout)
{
	for(;;){
		ConnectionEvent e = waitEvent(timeout);
		if(e.type != CONNEVENT_NONE)
			dout_con<<getDesc()<<": Receive: got event: "
					<<e.describe()<<std::endl;
		switch(e.type){
		case CONNEVENT_NONE:
			//throw NoIncomingDataException("No incoming data");
			return 0;
		case CONNEVENT_DATA_RECEIVED:
			if (e.data.getSize() < 2) {
				continue;
			}
			pkt->putRawPacket(*e.data, e.data.getSize(), e.peer_id);
			return e.data.getSize();
		case CONNEVENT_PEER_ADDED: {
			if(m_bc_peerhandler)
				m_bc_peerhandler->peerAdded(e.peer_id);
			continue; }
		case CONNEVENT_PEER_REMOVED: {
			if(m_bc_peerhandler)
				m_bc_peerhandler->deletingPeer(e.peer_id, e.timeout);
			continue; }
		case CONNEVENT_BIND_FAILED:
			throw ConnectionBindFailed("Failed to bind socket "
					"(port already in use?)");
		case CONNEVENT_CONNECT_FAILED:
			throw ConnectionException("Failed to connect");
		}
	}
	return 0;
	//throw NoIncomingDataException("No incoming data");
}

void Connection::SendToAll(u8 channelnum, SharedBuffer<u8> data, bool reliable)
{
	assert(channelnum < CHANNEL_COUNT);

	ConnectionCommand c;
	c.sendToAll(channelnum, data, reliable);
	putCommand(c);
}

void Connection::Send(u16 peer_id, u8 channelnum,
		SharedBuffer<u8> data, bool reliable)
{
	assert(channelnum < CHANNEL_COUNT);

	ConnectionCommand c;
	c.send(peer_id, channelnum, data, reliable);
	putCommand(c);
}

void Connection::Send(u16 peer_id, u8 channelnum, const msgpack::sbuffer &buffer, bool reliable) {
	SharedBuffer<u8> data((unsigned char*)buffer.data(), buffer.size());
	Send(peer_id, channelnum, data, reliable);
}

Address Connection::GetPeerAddress(u16 peer_id)
{
	auto lock = m_peers_address.lock_unique_rec();
	if (!m_peers_address.count(peer_id))
		return Address();
	return m_peers_address.get(peer_id);
/*
	auto a = Address(0, 0, 0, 0, 0);
	if (!m_peers.get(peer_id))
		return a;
	a.setPort(m_peers.get(peer_id)->address.port);
	a.setAddress(m_peers.get(peer_id)->address.host);
	return a;
*/
}

void Connection::DeletePeer(u16 peer_id)
{
	ConnectionCommand c;
	c.deletePeer(peer_id);
	putCommand(c);
}

void Connection::PrintInfo(std::ostream &out)
{
	out<<getDesc()<<": ";
}

void Connection::PrintInfo()
{
	PrintInfo(dout_con);
}

std::string Connection::getDesc()
{
	return "";
	//return std::string("con(")+itos(m_socket.GetHandle())+"/"+itos(m_peer_id)+")";
}
float Connection::getPeerStat(u16 peer_id, rtt_stat_type type)
{
	return 0;
}


void Connection::DisconnectPeer(u16 peer_id)
{
	ConnectionCommand discon;
	discon.disconnect_peer(peer_id);
	putCommand(discon);
}

bool parse_msgpack_packet(char *data, u32 datasize, MsgpackPacket *packet, int *command, msgpack::unpacked *msg) {
	try {
		//msgpack::unpacked msg;
		msgpack::unpack(msg, data, datasize);
		msgpack::object obj = msg->get();
		*packet = obj.as<MsgpackPacket>();

		*command = (*packet)[MSGPACK_COMMAND].as<int>();
	}
	catch (msgpack::type_error e) {
		verbosestream<<"parse_msgpack_packet: msgpack::type_error : "<<e.what()<<" datasize="<<datasize<<std::endl;
		return false;
	}
	catch (msgpack::unpack_error e) {
		verbosestream<<"parse_msgpack_packet: msgpack::unpack_error : "<<e.what()<<" datasize="<<datasize<<std::endl;
		//verbosestream<<"bad data:["<< std::string(data, datasize) <<"]"<<std::endl;
		return false;
	} catch (std::exception &e) {
		errorstream<<"parse_msgpack_packet: exception: "<<e.what()<<" datasize="<<datasize<<std::endl;
		return false;
	} catch (...) {
		errorstream<<"parse_msgpack_packet: Ooops..."<<std::endl;
		return false;
	}

	return true;
}

} // namespace

