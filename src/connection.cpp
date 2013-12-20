/*
Minetest
Copyright (C) 2013 celeron55, Perttu Ahola <celeron55@gmail.com>

This program is free software; you can redistribute it and/or modify
it under the terms of the GNU Lesser General Public License as published by
the Free Software Foundation; either version 2.1 of the License, or
(at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU Lesser General Public License for more details.

You should have received a copy of the GNU Lesser General Public License along
with this program; if not, write to the Free Software Foundation, Inc.,
51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
*/

#include "connection.h"
#include "main.h"
#include "serialization.h"
#include "log.h"
#include "porting.h"
#include "util/serialize.h"
#include "util/numeric.h"
#include "util/string.h"
#include "settings.h"

namespace con
{

/*
	Connection
*/

Connection::Connection(u32 protocol_id, u32 max_packet_size, float timeout,
		bool ipv6):
	m_protocol_id(protocol_id),
	m_max_packet_size(max_packet_size),
	m_timeout(timeout),
	m_enet_host(0),
	m_peer_id(0),
	m_bc_peerhandler(NULL),
	m_bc_receive_timeout(0)
{
	Start();
}

Connection::Connection(u32 protocol_id, u32 max_packet_size, float timeout,
		bool ipv6, PeerHandler *peerhandler):
	m_protocol_id(protocol_id),
	m_max_packet_size(max_packet_size),
	m_timeout(timeout),
	m_enet_host(0),
	m_peer_id(0),
	m_bc_peerhandler(peerhandler),
	m_bc_receive_timeout(0)
{
	Start();
}


Connection::~Connection()
{
	Stop();
}

/* Internal stuff */

void * Connection::Thread()
{
	ThreadStarted();
	log_register_thread("Connection");

	while(!StopRequested())
	{
		while(!m_command_queue.empty()){
			ConnectionCommand c = m_command_queue.pop_front();
			processCommand(c);
		}

		receive();

		sleep_ms(10);
	}

	return NULL;
}

void Connection::putEvent(ConnectionEvent &e)
{
	assert(e.type != CONNEVENT_NONE);
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
				<<c.port<<std::endl;
		serve(c.port);
		return;
	case CONNCMD_CONNECT:
		dout_con<<getDesc()<<" processing CONNCMD_CONNECT"<<std::endl;
		connect(c.address);
		return;
	case CONNCMD_DISCONNECT:
		dout_con<<getDesc()<<" processing CONNCMD_DISCONNECT"<<std::endl;
		disconnect();
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
	if (!m_enet_host)
		return;
	ENetEvent event;
	while (enet_host_service(m_enet_host, & event, 0) > 0)
	{
		switch (event.type)
		{
		case ENET_EVENT_TYPE_CONNECT:
			{
				JMutexAutoLock peerlock(m_peers_mutex);
				u16 peer_id = PEER_ID_SERVER + 1;
				if (m_peers.size() > 0)
					// TODO: fix this shit
					peer_id = m_peers.rbegin()->first + 1;
				m_peers[peer_id] = event.peer;

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
	}
}

// host
void Connection::serve(u16 port)
{
	ENetAddress *address = new ENetAddress;
	address->host = ENET_HOST_ANY;
	address->port = port;

	std::cout << "creating enet host" << std::endl;
	m_enet_host = enet_host_create(address, g_settings->getU16("max_users"), CHANNEL_COUNT, 0, 0);
	if (m_enet_host == NULL) {
		puts("Server creation failed.");
		assert(0);
	}
}

// peer
void Connection::connect(Address addr)
{
	JMutexAutoLock peerlock(m_peers_mutex);
	std::map<u16, ENetPeer*>::iterator node = m_peers.find(PEER_ID_SERVER);
	if(node != m_peers.end()){
		throw ConnectionException("Already connected to a server");
	}

	m_enet_host = enet_host_create(NULL, 1, 0, 0, 0);
	ENetAddress *address = new ENetAddress;
	// TODO: ipv6
	address->host = addr.getAddress().sin_addr.s_addr;
	address->port = addr.getPort();
	ENetPeer *peer = enet_host_connect(m_enet_host, address, CHANNEL_COUNT, 0);
	peer->data = new u16;
	*((u16*)peer->data) = PEER_ID_SERVER;

	m_peers[PEER_ID_SERVER] = peer;

	ENetEvent event;
	if (enet_host_service (m_enet_host, & event, 5000) > 0 &&
			event.type == ENET_EVENT_TYPE_CONNECT) {
		// Create event
		ConnectionEvent e;
		e.peerAdded(PEER_ID_SERVER);
		putEvent(e);
	}
	else {
		/* Either the 5 seconds are up or a disconnect event was */
		/* received. Reset the peer in the event the 5 seconds   */
		/* had run out without any significant event.            */
		enet_peer_reset(m_peer);
	}

	m_peer_id = PEER_ID_INEXISTENT;
}

void Connection::disconnect()
{
	JMutexAutoLock peerlock(m_peers_mutex);
	for (std::map<u16, ENetPeer*>::iterator i = m_peers.begin();
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
	dout_con<<getDesc()<<" sending to peer_id="<<peer_id<<std::endl;
	{
		JMutexAutoLock peerlock(m_peers_mutex);
		if (m_peers.find(peer_id) == m_peers.end())
			return;
	}

	assert(channelnum < CHANNEL_COUNT);

	ENetPacket *packet = enet_packet_create(*data, data.getSize(), reliable ? ENET_PACKET_FLAG_RELIABLE : 0);

	ENetPeer *peer = getPeer(peer_id);
	enet_peer_send(peer, channelnum, packet);
}

ENetPeer* Connection::getPeer(u16 peer_id)
{
	std::map<u16, ENetPeer*>::iterator node;
	{
		JMutexAutoLock peerlock(m_peers_mutex);
		node = m_peers.find(peer_id);
	}

	if(node == m_peers.end())
		return NULL;

	return node->second;
}

bool Connection::deletePeer(u16 peer_id, bool timeout)
{
	JMutexAutoLock peerlock(m_peers_mutex);
	if(m_peers.find(peer_id) == m_peers.end())
		return false;

	// Create event
	ConnectionEvent e;
	e.peerRemoved(peer_id, timeout);
	putEvent(e);

	// delete m_peers[peer_id]; -- enet should handle this
	m_peers.erase(peer_id);
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
	return m_event_queue.pop_front();
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

void Connection::Serve(unsigned short port)
{
	ConnectionCommand c;
	c.serve(port);
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
	JMutexAutoLock peerlock(m_peers_mutex);

	std::map<u16, ENetPeer*>::iterator node = m_peers.find(PEER_ID_SERVER);
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

u32 Connection::Receive(u16 &peer_id, SharedBuffer<u8> &data)
{
	for(;;){
		ConnectionEvent e = waitEvent(m_bc_receive_timeout);
		if(e.type != CONNEVENT_NONE)
			dout_con<<getDesc()<<": Receive: got event: "
					<<e.describe()<<std::endl;
		switch(e.type){
		case CONNEVENT_NONE:
			throw NoIncomingDataException("No incoming data");
		case CONNEVENT_DATA_RECEIVED:
			peer_id = e.peer_id;
			data = SharedBuffer<u8>(e.data);
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
		}
	}
	throw NoIncomingDataException("No incoming data");
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

Address Connection::GetPeerAddress(u16 peer_id)
{
	// lol that's not going to end well
	//JMutexAutoLock peerlock(m_peers_mutex);
	//return getPeer(peer_id)->address;
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

} // namespace

