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

#include <memory>
#include "network/address.h"
#include "network/connection.h"
#include "network/networkprotocol.h"
#include "network/peerhandler.h"
#include "config.h"

class NetworkPacket;

namespace con
{
class PeerHandler;
}

namespace con
{

class ConnectionMulti final : public IConnection
{
public:
	ConnectionMulti(u32 max_packet_size, float timeout, bool ipv6,
			con::PeerHandler *peerhandler = nullptr);
	~ConnectionMulti();

	void Serve(Address bind_addr) override;
	void Connect(Address address) override;
	bool Connected() override;
	void Disconnect() override;
	bool ReceiveTimeoutMs(NetworkPacket *pkt, u32 timeout_ms) override;
	void Send(
			session_t peer_id, u8 channelnum, NetworkPacket *pkt, bool reliable) override;
	Address GetPeerAddress(session_t peer_id) override;
	float getPeerStat(session_t peer_id, con::rtt_stat_type type) override;
	float getLocalStat(con::rate_stat_type type) override;
	void DisconnectPeer(session_t peer_id) override;
	size_t events_size() override;

private:
#if USE_SCTP
	std::shared_ptr<IConnection> m_con_sctp;
#endif
#if USE_WEBSOCKET
	std::shared_ptr<IConnection> m_con_ws;
#endif
#if USE_WEBSOCKET_SCTP
	std::shared_ptr<IConnection> m_con_ws_sctp;
#endif
#if USE_ENET
	std::shared_ptr<IConnection> m_con_enet;
#endif
#if MINETEST_TRANSPORT
	std::shared_ptr<IConnection> m_con;
#endif
	bool dummy;
	enum class proto_name
	{
		none = 0,
		minetest,
		sctp,
		websocket,
		websocket_stcp,
		enet,
	};
	proto_name connected_to = proto_name::none;
};

} // namespace
