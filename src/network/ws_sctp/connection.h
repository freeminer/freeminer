/*
Copyright (C) 2023 proller <proler@gmail.com>
SPDX-License-Identifier: GPL-3.0-or-later
*/
#pragma once
#include "network/sctp/connection.h"
#include "network/ws/transport.h"
#include "external/usrsctp/usrsctplib/usrsctp.h"
#include <map>
#include <memory>
#include <unordered_set>

namespace con_ws_sctp
{
// One AF_CONN association per binary WebSocket. No emsocket datagram framing.
int connectionOutput(void *address, void *data, size_t size, uint8_t tos, uint8_t df);
class Connection : public con_sctp::Connection
{
public:
	Connection(u32 protocol_id, u32 max_packet_size, float timeout, bool ipv6,
			con::PeerHandler *peerhandler = nullptr);
	~Connection() override;
	bool hasPeer(session_t id) { return m_peers.count(id) != 0; }

private:
	// Keep WebSocket-SCTP sessions distinct from native SCTP in multi mode.
	static constexpr session_t FIRST_PEER_ID = PEER_SCTP_MAX + 3;
	static constexpr session_t LAST_PEER_ID =
			FIRST_PEER_ID + PEER_SCTP_MAX - PEER_SCTP_MIN;
	struct Peer
	{
		session_t id = PEER_ID_INEXISTENT;
		ws_transport::Transport::Id websocket = 0;
		void *address = nullptr;
		struct socket *listener = nullptr;
		struct socket *socket = nullptr;
		bool added = false;
	};
	std::shared_ptr<ws_transport::Transport> m_transport;
	std::map<ws_transport::Transport::Id, std::shared_ptr<Peer>> m_websockets;
	std::unordered_set<session_t> m_reserved_ids;
	Address m_destination;
	u16 m_port = 0;
	session_t m_next_id = FIRST_PEER_ID;
	bool m_ipv6 = false;

	bool m_connecting = false;
	void processCommand(con::ConnectionCommandPtr command) override;
	int receive() override;
	void serve(const Address &address) override;
	bool deletePeer(session_t id, bool timeout = false) override;
	void stopWebSockets();
	void disconnect() override;
	void onAssociationChange(session_t id, const sctp_assoc_change *change) override;
	void closePeer(const std::shared_ptr<Peer> &peer, bool timeout);
	bool openPeer(const ws_transport::Transport::Event &event);
};
}
