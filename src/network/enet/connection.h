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

#include <enet/enet.h>
//#include "network/fm_connection_multi.h"
#include "network/connection.h"
#include "network/mtp/impl.h"
#include "network/mtp/internal.h"
#include "network/networkprotocol.h"
#include "threading/thread_vector.h"
#include "threading/concurrent_map.h"
#include "threading/concurrent_unordered_map.h"
#include "msgpack_fix.h"

// #define CHANNEL_COUNT 3

namespace con
{
//using namespace con;
class ConnectionEnet final : public IConnection, public thread_vector
{
public:
	//friend con_multi::Connection;
	ConnectionEnet(u32 max_packet_size, float timeout, bool ipv6,
			con::PeerHandler *peerhandler = nullptr);
	~ConnectionEnet();
	void *run() override;

	/* Interface */

private:
	// ConnectionEvent getEvent();
	// ConnectionEvent waitEvent(u32 timeout_ms);
	ConnectionEventPtr waitEvent(u32 timeout_ms);
	void putCommand(ConnectionCommandPtr c);

public:
	void Serve(Address bind_addr) override;
	void Connect(Address address) override;
	bool Connected() override;
	void Disconnect() override;
	bool ReceiveTimeoutMs(NetworkPacket *pkt, u32 timeout = 1) override;
	//bool TryReceive(NetworkPacket *pkt);

	//void SendToAll(u8 channelnum, SharedBuffer<u8> data, bool reliable);
	void Send(
			session_t peer_id, u8 channelnum, NetworkPacket *pkt, bool reliable) override;
	void Send(u16 peer_id, u8 channelnum, SharedBuffer<u8> data, bool reliable);
	//void Send(u16 peer_id, u8 channelnum, const msgpack::sbuffer &buffer, bool reliable);
	//session_t GetPeerID() const { return m_peer_id; }
	//void DeletePeer(u16 peer_id);
	Address GetPeerAddress(session_t peer_id) override;
	float getPeerStat(u16 peer_id, con::rtt_stat_type type) override;
	float getLocalStat(con::rate_stat_type type) override;

	void DisconnectPeer(u16 peer_id) override;
	size_t events_size() override;

private:
	void putEvent(ConnectionEventPtr e);
	// void putEvent(ConnectionEvent &e);
	// void processCommand(ConnectionCommand &c);
	void processCommand(ConnectionCommandPtr c);
	void send(float dtime);
	int receive();
	void runTimeouts(float dtime);
	void serve(Address address);
	void connect(Address address);
	void disconnect();
	void sendToAll(u8 channelnum, SharedBuffer<u8> data, bool reliable);
	void send(u16 peer_id, u8 channelnum, SharedBuffer<u8> data, bool reliable);

protected:
	ENetPeer *getPeer(u16 peer_id);

private:
	bool deletePeer(u16 peer_id, bool timeout);

	MutexedQueue<ConnectionEventPtr> m_event_queue;
	MutexedQueue<ConnectionCommandPtr> m_command_queue;

	//u32 m_protocol_id {PROTOCOL_ID};
	u32 m_max_packet_size;
	float m_timeout;
	ENetHost *m_enet_host;
	// ENetPeer *m_peer;
	u16 m_peer_id;

	concurrent_map<u16, ENetPeer *> m_peers;
	concurrent_unordered_map<u16, Address> m_peers_address;
	// std::mutex m_peers_mutex;

	// Backwards compatibility
	con::PeerHandler *m_bc_peerhandler;
	unsigned int m_last_recieved;
	unsigned int m_last_recieved_warn;

	void SetPeerID(u16 id) { m_peer_id = id; }
	//u32 GetProtocolID() { return m_protocol_id; }
	void PrintInfo(std::ostream &out);
	void PrintInfo();
	std::string getDesc();
	unsigned int timeout_mul;

	session_t m_next_remote_peer_id = PEER_ENET_MIN;
};

} // namespace
