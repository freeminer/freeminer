/*
Copyright (C) 2023 proller <proler@gmail.com>
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

#include "constants.h"
#include "exceptions.h"
#include "msgpack_fix.h"
#include "network/address.h"
#include "network/connection.h"
#include "network/networkprotocol.h"
#include "network/peerhandler.h"
#include "threading/concurrent_map.h"
#include "threading/concurrent_unordered_map.h"
#include "threading/thread_vector.h"
#include "util/container.h"
#include "util/pointer.h"


#define CHANNEL_COUNT 3

struct socket;
union sctp_sockstore;
struct sctp_rcvinfo;
struct sctp_assoc_change;
class NetworkPacket;

namespace con
{
class PeerHandler;
}
namespace con_multi
{
class Connection;
}
namespace con_sctp
{
using namespace con;

class Connection : public thread_vector
{
public:
	friend class con_multi::Connection;

	Connection(u32 max_packet_size, float timeout, bool ipv6,
			con::PeerHandler *peerhandler = nullptr);
	~Connection();
	void *run();

	/* Interface */

	// ConnectionEvent getEvent();
	ConnectionEventPtr waitEvent(u32 timeout_ms);
	void putCommand(ConnectionCommandPtr c);

	void Serve(Address bind_addr);
	void Connect(Address address);
	bool Connected();
	void Disconnect();
	u32 Receive(NetworkPacket *pkt, int timeout = 1);
	bool TryReceive(NetworkPacket *pkt);

	void SendToAll(u8 channelnum, SharedBuffer<u8> data, bool reliable);
	void Send(session_t peer_id, u8 channelnum, SharedBuffer<u8> data, bool reliable);
	void Send(session_t peer_id, u8 channelnum, const msgpack::sbuffer &buffer,
			bool reliable);
	void Send(session_t peer_id, u8 channelnum, NetworkPacket *pkt, bool reliable);
	session_t GetPeerID() { return m_peer_id; }
	Address GetPeerAddress(session_t peer_id) override;
	float getPeerStat(session_t peer_id, rtt_stat_type type);
	float getLocalStat(rate_stat_type type);

	void DisconnectPeer(session_t peer_id);
	size_t events_size() { return m_event_queue.size(); }

protected:
	void putEvent(ConnectionEventPtr e);
	void processCommand(ConnectionCommandPtr c);
	void send(float dtime);
	virtual int receive();
	void runTimeouts(float dtime);
	virtual void serve(const Address & address);
	void connect_addr(const Address & address);
	void connect_conn(const Address & address);
	void disconnect();
	void sendToAll(u8 channelnum, SharedBuffer<u8> data, bool reliable);
	void send(session_t peer_id, u8 channelnum, SharedBuffer<u8> data, bool reliable);

protected:
	struct socket *getPeer(session_t peer_id);

	bool deletePeer(session_t peer_id, bool timeout = 0);
private:

	MutexedQueue<ConnectionEventPtr> m_event_queue;
	MutexedQueue<ConnectionCommandPtr> m_command_queue;

	u32 m_protocol_id;
	u32 m_max_packet_size;
	float m_timeout = 0;
	// struct sctp_udpencaps encaps;
protected:
	struct socket *sock = nullptr;
private:
	session_t m_peer_id;
	session_t m_next_remote_peer_id = PEER_SCTP_MIN;

protected:
	concurrent_map<session_t, struct socket *> m_peers;
private:
	concurrent_unordered_map<session_t, Address> m_peers_address;
	concurrent_unordered_map<session_t, int[2]> m_peers_fd;

	// Backwards compatibility
	PeerHandler *m_bc_peerhandler;
	unsigned int m_last_recieved;
	int m_last_recieved_warn;

	void SetPeerID(const session_t id) { m_peer_id = id; }
	//u32 GetProtocolID() { return m_protocol_id; }
	void PrintInfo(std::ostream &out);
	void PrintInfo();

protected:
	std::string getDesc();
private:

	bool sock_listen = false, sock_connect = false, sctp_inited_by_me = false;
	static bool sctp_inited;
protected:
/*
#ifdef __EMSCRIPTEN__
	const int domain = AF_CONN;
	//using sockaddr_t = sockaddr_conn;
#else
	int domain = AF_INET6;
	//using sockaddr_t = sockaddr_in6;
#endif
*/
	std::thread handle_packets_thread;

	int domain = AF_INET6;
	//bool use_conn = false;
	int (*sctp_conn_output)(void *addr, void *buffer, size_t length, uint8_t tos, uint8_t set_df) = nullptr;
	int (*server_send_cb)(struct socket *sock, uint32_t sb_free, void *ulp_info) = nullptr;
	int (*client_send_cb)(struct socket *sock, uint32_t sb_free, void *ulp_info) = nullptr;

	std::pair<int, bool> recv(session_t peer_id, struct socket *sock);
	void sock_setup(/*session_t peer_id,*/ struct socket *sock);
	void sctp_setup(u16 port = 9899);
	struct socket *sctp_server_sock{};
private:
	std::unordered_map<session_t, std::array<std::string, 10>> recv_buf;

	void handle_association_change_event(
			session_t peer_id, const struct sctp_assoc_change *sac);
	/*
		int sctp_recieve_callback(struct socket *sock, union sctp_sockstore addr, void
	   *data, size_t datalen, struct sctp_rcvinfo, int flags, void *ulp_info);
	*/
};

} // namespace
