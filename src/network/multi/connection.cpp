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

#include "network/multi/connection.h"
#include "connection.h"
#include "network/networkpacket.h"
#include "settings.h"
#include "config.h"

#if USE_SCTP
#include "fm_connection_sctp.h"
#endif
#if USE_WEBSOCKET
#include "network/ws/impl.h"
#endif
#if USE_WEBSOCKET_SCTP
#include "fm_connection_websocket_sctp.h"
#endif
#if USE_ENET
#include "network/enet/connection.h"
#endif
#if MINETEST_TRANSPORT
#include "network/mtp/impl.h"
#endif

namespace con
{

ConnectionMulti::ConnectionMulti(
		u32 max_packet_size, float timeout, bool ipv6, con::PeerHandler *peerhandler) :
#if USE_SCTP
		m_con_sctp(std::make_shared<con_sctp::Connection>(
				max_packet_size, timeout, ipv6, peerhandler)),
#endif
#if USE_WEBSOCKET
		m_con_ws(std::make_shared<con_ws::Connection>(
				max_packet_size, timeout, ipv6, peerhandler)),
#endif
#if USE_WEBSOCKET_SCTP
		m_con_ws_sctp(std::make_shared<con_ws_sctp::Connection>(
				max_packet_size, timeout, ipv6, peerhandler)),
#endif
#if USE_ENET
		m_con_enet(std::make_shared<ConnectionEnet>(
				max_packet_size, timeout, ipv6, peerhandler)),
#endif
#if MINETEST_TRANSPORT
		m_con(std::make_shared<con::Connection>(
				max_packet_size, timeout, ipv6, peerhandler)),
#endif
		dummy{}
{
}

ConnectionMulti::~ConnectionMulti()
{
}

void ConnectionMulti::Serve(Address bind_address)
{
	infostream << "Multi serving at " << bind_address.serializeString() << ":"
			   << std::to_string(bind_address.getPort()) << std::endl;

#if USE_SCTP
	if (m_con_sctp) {
		auto addr = bind_address;
		u16 port = 0;
		if (!g_settings->getU16NoEx("port_sctp", port)) {
			port = addr.getPort() + 100;
		}
		addr.setPort(port);
		m_con_sctp->Serve(addr);
	}
#endif
#if USE_WEBSOCKET
	if (m_con_ws) {
		auto addr = bind_address;
		u16 port = 0;
		if (!g_settings->getU16NoEx("port_wss", port)) {
			port = addr.getPort();
		}
		addr.setPort(port); // same tcp
		m_con_ws->Serve(addr);
	}
#endif
#if USE_WEBSOCKET_SCTP
	if (m_con_ws_sctp) {
		auto addr = bind_address;
		u16 port = 0;
		if (!g_settings->getU16NoEx("port_sctp_wss", port)) {
			port = addr.getPort() + 100;
		}
		addr.setPort(port); // same tcp
		m_con_ws_sctp->Serve(addr);
	}
#endif
#if USE_ENET
	if (m_con_enet) {
		auto addr = bind_address;
		u16 port = 0;
		if (!g_settings->getU16NoEx("port_enet", port)) {
			port = addr.getPort() + 200;
		}
		addr.setPort(port);
		m_con_enet->Serve(addr);
	}
#endif
#if MINETEST_TRANSPORT
	if (m_con)
		m_con->Serve(bind_address);
#endif
}

void ConnectionMulti::Connect(Address address)
{
	const auto remote_proto = g_settings->get("remote_proto");

	actionstream << "Multi connect to " << address.serializeString() << ":"
				 << std::to_string(address.getPort()) << " with " << remote_proto << '\n';

#if USE_SCTP
	if (m_con_sctp && remote_proto == "sctp") {
		connected_to = sctp;
		m_con_sctp->Connect(address);
	}
#endif
#if USE_ENET
	if (m_con_enet && remote_proto == "enet") {
		connected_to = proto_name::enet;
		m_con_enet->Connect(address);
	}
#endif
#if MINETEST_TRANSPORT
	if (m_con && (remote_proto == "mt" || remote_proto.empty())) {
		connected_to = proto_name::minetest;
		m_con->Connect(address);
	}
#endif
}

bool ConnectionMulti::Connected()
{
#if USE_SCTP
	if (m_con_sctp)
		if (auto c = m_con_sctp->Connected(); c)
			return c;
#endif
#if USE_ENET
	if (m_con_enet)
		if (auto c = m_con_enet->Connected(); c)
			return c;
#endif
#if MINETEST_TRANSPORT
	if (m_con)
		if (auto c = m_con->Connected(); c)
			return c;
	return false;
#endif
}

void ConnectionMulti::Disconnect()
{
#if USE_SCTP
	if (m_con_sctp)
		m_con_sctp->Disconnect();
#endif
#if USE_ENET
	if (m_con_enet)
		m_con_enet->Disconnect();
#endif
#if MINETEST_TRANSPORT
	if (m_con)
		m_con->Disconnect();
#endif
	connected_to = proto_name::none;
}

bool ConnectionMulti::ReceiveTimeoutMs(NetworkPacket *pkt, u32 timeout_ms)
{
	u32 ret = 0;
	for (u32 i = 0; !i || (i < timeout_ms/10); ++i) {
		const u32 timeout = i ? 10 : 0;
		//for (const auto &timeout : {u32(0), u32(1)}) {
#if USE_SCTP
		if (m_con_sctp)
			ret += m_con_sctp->ReceiveTimeoutMs(pkt, timeout);
		if (ret)
			return ret;
#endif
#if USE_WEBSOCKET
		if (m_con_ws)
			ret += m_con_ws->ReceiveTimeoutMs(pkt, timeout);
		if (ret)
			return ret;
#endif
#if USE_WEBSOCKET_SCTP
		if (m_con_ws_sctp)
			ret += m_con_ws_sctp->ReceiveTimeoutMs(pkt, timeout);
		if (ret)
			return ret;
#endif
#if USE_ENET
		if (m_con_enet)
			ret += m_con_enet->ReceiveTimeoutMs(pkt, timeout);
		if (ret)
			return ret;
#endif
#if MINETEST_TRANSPORT
		if (m_con)
			ret += m_con->ReceiveTimeoutMs(pkt, timeout);
#endif
		if (ret)
			return ret;
	}
	return ret;
}

/*
bool ConnectionMulti::TryReceive(NetworkPacket *pkt)
{
	return ReceiveTimeoutMs(pkt, 0);
}
*/

void ConnectionMulti::Send(
		session_t peer_id, u8 channelnum, NetworkPacket *pkt, bool reliable)
{
	// TODO send to one
#if USE_SCTP
	if (m_con_sctp && ((peer_id >= PEER_SCTP_MIN && peer_id <= PEER_SCTP_MAX) ||
							  (connected_to == sctp && peer_id == PEER_ID_SERVER)))
		m_con_sctp->Send(peer_id, channelnum, pkt, reliable);
#endif
#if USE_WEBSOCKET
	if (m_con_ws &&
			((peer_id >= PEER_WS_MIN && peer_id <= PEER_WS_MAX) ||
					(connected_to == proto_name::websocket && peer_id == PEER_ID_SERVER)))
		m_con_ws->Send(peer_id, channelnum, pkt, reliable);
#endif
#if USE_WEBSOCKET_SCTP
	if (m_con_ws_sctp && m_con_ws_sctp->getPeer(peer_id).lock().get())
		m_con_ws_sctp->Send(peer_id, channelnum, pkt, reliable);
#endif
#if USE_ENET
	if (m_con_enet &&
			((peer_id >= PEER_ENET_MIN && peer_id <= PEER_ENET_MAX) ||
					(connected_to == proto_name::enet && peer_id == PEER_ID_SERVER)))
		m_con_enet->Send(peer_id, channelnum, pkt, reliable);
#endif
#if MINETEST_TRANSPORT
	if (m_con &&
			((peer_id >= PEER_MINETEST_MIN && peer_id <= PEER_MINETEST_MAX) ||
					(connected_to == proto_name::minetest && peer_id == PEER_ID_SERVER)))
		m_con->Send(peer_id, channelnum, pkt, reliable);
#endif
}

#if 0
void ConnectionMulti::Send(
		session_t peer_id, u8 channelnum, const msgpack::sbuffer &buffer, bool reliable)
{
	// TODO send to one
#if USE_SCTP
	if (m_con_sctp && ((peer_id >= PEER_SCTP_MIN && peer_id <= PEER_SCTP_MAX) ||
							  (connected_to == sctp && peer_id == PEER_ID_SERVER)))
		m_con_sctp->Send(peer_id, channelnum, buffer, reliable);
#endif

#if USE_WEBSOCKET_SCTP
	if (m_con_ws_sctp)
		m_con_ws_sctp->Send(peer_id, channelnum, buffer, reliable);
#endif
#if USE_ENET
	if (m_con_enet &&
			((peer_id >= PEER_ENET_MIN && peer_id <= PEER_ENET_MAX) ||
					(connected_to == proto_name::enet && peer_id == PEER_ID_SERVER)))
		m_con_enet->Send(peer_id, channelnum, buffer, reliable);
#endif
#if MINETEST_TRANSPORT
		//	if (m_con && (peer_id >= PEER_MINETEST_MIN && peer_id <= PEER_MINETEST_MAX) ||
		// peer_id == PEER_ID_SERVER) 		m_con->Send(peer_id, channelnum, buffer,
		// reliable);
#endif
}
#endif

Address ConnectionMulti::GetPeerAddress(session_t peer_id)
{
#if USE_SCTP
	if (m_con_sctp && ((peer_id >= PEER_SCTP_MIN && peer_id <= PEER_SCTP_MAX) ||
							  (connected_to == sctp && peer_id == PEER_ID_SERVER)))
		return m_con_sctp->GetPeerAddress(peer_id);
#endif
#if USE_WEBSOCKET
	try {
		if (m_con_ws && ((peer_id >= PEER_WS_MIN && peer_id <= PEER_WS_MAX) ||
								(connected_to == proto_name::websocket &&
										peer_id == PEER_ID_SERVER)))
			return m_con_ws->GetPeerAddress(peer_id);
	} catch (...) {
	}
#endif
#if USE_WEBSOCKET_SCTP
	try {
		if (m_con_ws_sctp && m_con_ws_sctp->getPeer(peer_id).lock().get())
			return m_con_ws_sctp->GetPeerAddress(peer_id);
	} catch (...) {
	}
#endif
#if USE_ENET
	try {
		if (m_con_enet &&
				((peer_id >= PEER_ENET_MIN && peer_id <= PEER_ENET_MAX) ||
						(connected_to == proto_name::enet && peer_id == PEER_ID_SERVER)))
			return m_con_enet->GetPeerAddress(peer_id);
	} catch (...) {
	}
#endif
#if MINETEST_TRANSPORT
	try {
		if (m_con && ((peer_id >= PEER_MINETEST_MIN && peer_id <= PEER_MINETEST_MAX) ||
							 (connected_to == proto_name::minetest &&
									 peer_id == PEER_ID_SERVER)))
			return m_con->GetPeerAddress(peer_id);
	} catch (...) {
	}
#endif
	throw con::PeerNotFoundException("No address for peer found!");
}

float ConnectionMulti::getPeerStat(session_t peer_id, con::rtt_stat_type type)
{
#if USE_SCTP
	if (m_con_sctp && ((peer_id >= PEER_SCTP_MIN && peer_id <= PEER_SCTP_MAX) ||
							  (connected_to == sctp && peer_id == PEER_ID_SERVER)))
		return m_con_sctp->getPeerStat(peer_id, type);
#endif
#if USE_ENET
	if (m_con_enet &&
			((peer_id >= PEER_ENET_MIN && peer_id <= PEER_ENET_MAX) ||
					(connected_to == proto_name::enet && peer_id == PEER_ID_SERVER)))
		return m_con_enet->getPeerStat(peer_id, type);
#endif
#if MINETEST_TRANSPORT
	if (m_con &&
			((peer_id >= PEER_MINETEST_MIN && peer_id <= PEER_MINETEST_MAX) ||
					(connected_to == proto_name::minetest && peer_id == PEER_ID_SERVER)))
		return m_con->getPeerStat(peer_id, type);
#endif
	return {};
}

float ConnectionMulti::getLocalStat(con::rate_stat_type type)
{
#if MINETEST_TRANSPORT
	if (m_con)
		return m_con->getLocalStat(type);
#endif
	return {};
}

void ConnectionMulti::DisconnectPeer(session_t peer_id)
{
#if USE_SCTP
	if (m_con_sctp && ((peer_id >= PEER_SCTP_MIN && peer_id <= PEER_SCTP_MAX) ||
							  (connected_to == sctp && peer_id == PEER_ID_SERVER)))
		return m_con_sctp->DisconnectPeer(peer_id);
#endif
#if USE_WEBSOCKET
	if (m_con_ws &&
			((peer_id >= PEER_WS_MIN && peer_id <= PEER_WS_MAX) ||
					(connected_to == proto_name::websocket && peer_id == PEER_ID_SERVER)))
		return m_con_ws->DisconnectPeer(peer_id);
#endif
#if USE_WEBSOCKET_SCTP
	if (m_con_ws_sctp && m_con_ws_sctp->getPeer(peer_id).lock().get())
		return m_con_ws_sctp->DisconnectPeer(peer_id);
#endif
#if USE_ENET
	if (m_con_enet &&
			((peer_id >= PEER_ENET_MIN && peer_id <= PEER_ENET_MAX) ||
					(connected_to == proto_name::enet && peer_id == PEER_ID_SERVER)))
		return m_con_enet->DisconnectPeer(peer_id);
#endif
#if MINETEST_TRANSPORT
	if (m_con &&
			((peer_id >= PEER_MINETEST_MIN && peer_id <= PEER_MINETEST_MAX) ||
					(connected_to == proto_name::minetest && peer_id == PEER_ID_SERVER)))
		return m_con->DisconnectPeer(peer_id);
#endif
}

size_t ConnectionMulti::events_size()
{
	size_t ret = 0;
#if USE_SCTP
	if (m_con_sctp)
		ret += m_con_sctp->events_size();
#endif
#if USE_WEBSOCKET
	if (m_con_ws)
		ret += m_con_ws->events_size();
#endif
#if USE_WEBSOCKET_SCTP
	if (m_con_ws_sctp)
		ret += m_con_ws_sctp->events_size();
#endif
#if USE_ENET
	if (m_con_enet)
		ret += m_con_enet->events_size();
#endif
#if MINETEST_TRANSPORT
	if (m_con)
		ret += m_con->events_size();
#endif
	return ret;
}

} // namespace
