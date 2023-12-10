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

#include "address.h"
#include "msgpack_fix.h"
#include "network/connection.h"
#include "networkprotocol.h"
#include "peerhandler.h"
#include "util/pointer.h"

class NetworkPacket;

namespace con
{
class Connection;
class PeerHandler;
}
namespace con_sctp
{
class Connection;
}
namespace con_enet
{
class Connection;
}
namespace con_ws
{
class Connection;
}
namespace con_ws_sctp
{
class Connection;
}
namespace con_multi
{
class Connection
{
public:
	Connection(u32 protocol_id, u32 max_packet_size, float timeout, bool ipv6,
			con::PeerHandler *peerhandler = nullptr);
	~Connection();

	void Serve(Address bind_addr);
	void Connect(Address address);
	bool Connected();
	void Disconnect();
	u32 Receive(NetworkPacket *pkt, int timeout = 1);
	bool TryReceive(NetworkPacket *pkt);
	void SendToAll(u8 channelnum, SharedBuffer<u8> data, bool reliable);
	void Send(session_t peer_id, u8 channelnum, const msgpack::sbuffer &buffer,
			bool reliable);
	void Send(session_t peer_id, u8 channelnum, NetworkPacket *pkt, bool reliable);
	void DeletePeer(session_t peer_id);
	Address GetPeerAddress(session_t peer_id);
	float getPeerStat(session_t peer_id, con::rtt_stat_type type);
	float getLocalStat(con::rate_stat_type type);
	void DisconnectPeer(session_t peer_id);
	size_t events_size();

private:
#if USE_SCTP
	std::shared_ptr<con_sctp::Connection> m_con_sctp;
#endif
#if USE_WEBSOCKET
	std::shared_ptr<con_ws::Connection> m_con_ws;
#endif
#if USE_WEBSOCKET_SCTP
	std::shared_ptr<con_ws_sctp::Connection> m_con_ws_sctp;
#endif
#if USE_ENET
	std::shared_ptr<con_enet::Connection> m_con_enet;
#endif
#if MINETEST_TRANSPORT
	std::shared_ptr<con::Connection> m_con;
#endif
	bool dummy;
};

} // namespace
