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
#include "network/networkexceptions.h"
#include "network/networkpacket.h"
#include "settings.h"
#include "config.h"
#include <chrono>

#if USE_SCTP
#include "network/sctp/connection.h"
#endif
#if USE_ENET
#include "network/enet/connection.h"
#endif
#if USE_WEBSOCKET
#include "network/ws/impl.h"
#endif
#if USE_WEBSOCKET_SCTP
#include "network/mtp/internal.h"
#include "network/ws_sctp/connection.h"
#endif
#if MINETEST_TRANSPORT
#include "network/mtp/impl.h"
#endif

namespace con
{

ConnectionMulti::ConnectionMulti(
		u32 max_packet_size, float timeout, bool ipv6, con::PeerHandler *peerhandler) :
		m_max_packet_size(max_packet_size), m_timeout(timeout), m_ipv6(ipv6),
		m_peerhandler(peerhandler)
{
}

IConnection *ConnectionMulti::ensureConnection(proto_name protocol)
{
	auto &connection = m_connections[static_cast<size_t>(protocol)];
	if (connection)
		return connection.get();
	switch (protocol) {
#if USE_SCTP
	case proto_name::sctp:
		connection = std::make_shared<con_sctp::Connection>(
				m_max_packet_size, m_timeout, m_ipv6, m_peerhandler);
		break;
#endif
#if USE_ENET
	case proto_name::enet:
		connection = std::make_shared<ConnectionEnet>(
				m_max_packet_size, m_timeout, m_ipv6, m_peerhandler);
		break;
#endif
#if USE_WEBSOCKET
	case proto_name::websocket:
		connection = std::make_shared<con_ws::Connection>(
				100000, m_timeout, m_ipv6, m_peerhandler);
		break;
#endif
#if USE_WEBSOCKET_SCTP
	case proto_name::websocket_stcp:
		connection = std::make_shared<con_ws_sctp::Connection>(
				PROTOCOL_ID, m_max_packet_size, m_timeout, m_ipv6, m_peerhandler);
		break;
#endif
#if MINETEST_TRANSPORT
	case proto_name::minetest:
		connection = std::make_shared<con::Connection>(
				m_max_packet_size, m_timeout, m_ipv6, m_peerhandler);
		break;
#endif
	default:
		return nullptr;
	}
	connection->setReceiveSignal(m_receive_signal);
	return connection.get();
}

ConnectionMulti::~ConnectionMulti()
{
}

ConnectionMulti::proto_name ConnectionMulti::getProtocol(std::string_view name)
{
	if (name == "sctp")
		return proto_name::sctp;
	if (name == "enet")
		return proto_name::enet;
	if (name == "mt_ws" || name == "ws")
		return proto_name::websocket;
	if (name == "mt_ws_sctp" || name == "ws_sctp")
		return proto_name::websocket_stcp;
	if (name == "mt" || name.empty())
		return proto_name::minetest;
	return proto_name::none;
}

IConnection *ConnectionMulti::getConnection(proto_name protocol) const
{
	return m_connections[static_cast<size_t>(protocol)].get();
}

IConnection *ConnectionMulti::getConnection(session_t peer_id)
{
	if (peer_id == PEER_ID_SERVER)
		return getConnection(connected_to);

#if USE_WEBSOCKET_SCTP
	// WebSocket-SCTP owns a separate peer map and session range.
	auto connection = static_cast<con_ws_sctp::Connection *>(
			getConnection(proto_name::websocket_stcp));
	if (connection && connection->hasPeer(peer_id))
		return connection;
#endif

	if (peer_id >= PEER_SCTP_MIN && peer_id <= PEER_SCTP_MAX) {
		if (auto connection = getConnection(proto_name::sctp))
			return connection;
	}
	if (peer_id >= PEER_ENET_MIN && peer_id <= PEER_ENET_MAX)
		return getConnection(proto_name::enet);
	if (peer_id >= PEER_WS_MIN && peer_id <= PEER_WS_MAX)
		return getConnection(proto_name::websocket);
	if (peer_id >= PEER_MINETEST_MIN && peer_id <= PEER_MINETEST_MAX)
		return getConnection(proto_name::minetest);
	return {};
}

void ConnectionMulti::Serve(Address bind_address)
{
	m_serving = true;
	infostream << "Multi serving at " << bind_address.serializeString() << ":"
			   << std::to_string(bind_address.getPort()) << std::endl;

	if (auto connection = ensureConnection(proto_name::sctp)) {
		auto addr = bind_address;
		u16 port = 0;
		if (!g_settings->getU16NoEx("port_sctp", port)) {
			port = addr.getPort() + 100;
		}
		addr.setPort(port);
		connection->Serve(addr);
	}
	if (auto connection = ensureConnection(proto_name::enet)) {
		auto addr = bind_address;
		u16 port = 0;
		if (!g_settings->getU16NoEx("port_enet", port)) {
			port = addr.getPort() + 200;
		}
		addr.setPort(port);
		connection->Serve(addr);
	}
	if (auto connection = ensureConnection(proto_name::websocket)) {
		auto addr = bind_address;
		u16 port = 0;
		if (!g_settings->getU16NoEx("port_wss", port)) {
			port = addr.getPort();
		}
		addr.setPort(port); // same tcp
		connection->Serve(addr);
	}
	if (auto connection = ensureConnection(proto_name::websocket_stcp)) {
		auto addr = bind_address;
		u16 port = 0;
		if (!g_settings->getU16NoEx("port_sctp_wss", port)) {
			port = addr.getPort() + 100;
		}
		addr.setPort(port); // same tcp
		connection->Serve(addr);
	}
	if (auto connection = ensureConnection(proto_name::minetest))
		connection->Serve(bind_address);
}

void ConnectionMulti::Connect(Address address)
{
	ConnectToHost(address, {});
}

void ConnectionMulti::ConnectToHost(Address address, const std::string &address_name)
{
	const auto remote_proto = g_settings->get("remote_proto");

	actionstream << "Multi connect to " << address.serializeString() << ":"
				 << std::to_string(address.getPort()) << " with " << remote_proto << '\n';

	const auto protocol = getProtocol(remote_proto);
	if (auto connection = ensureConnection(protocol)) {
		connected_to = protocol;
		connection->ConnectToHost(address, address_name);
	}
}

bool ConnectionMulti::Connected()
{
	if (!m_serving) {
		auto *connection = getConnection(connected_to);
		return connection && connection->Connected();
	}
	for (const auto &connection : m_connections) {
		if (connection && connection->Connected())
			return true;
	}
	return false;
}

void ConnectionMulti::Disconnect()
{
	for (const auto &connection : m_connections) {
		if (connection)
			connection->Disconnect();
	}
	connected_to = proto_name::none;
}

std::string_view ConnectionMulti::getProtocolName(session_t peer_id)
{
	auto connection = getConnection(peer_id);
	if (!connection)
		return "unknown";
	if (connection == getConnection(proto_name::sctp))
		return "sctp";
	if (connection == getConnection(proto_name::enet))
		return "enet";
	if (connection == getConnection(proto_name::websocket))
		return "ws";
	if (connection == getConnection(proto_name::websocket_stcp))
		return "ws_sctp";
	if (connection == getConnection(proto_name::minetest))
		return "mt";
	return "unknown";
}

bool ConnectionMulti::ReceiveTimeoutMs(NetworkPacket *pkt, u32 timeout_ms)
{
	if (!m_serving) {
		if (auto *connection = getConnection(connected_to))
			return connection->ReceiveTimeoutMs(pkt, timeout_ms);
		// Disconnect clears the selected protocol, but queued events still
		// need to reach their peer handlers.
	}

	using Clock = std::chrono::steady_clock;
	const auto deadline = Clock::now() + std::chrono::milliseconds(timeout_ms);
	for (;;) {
		m_receive_signal->wait(0);
		// Scan without blocking; waiting on one transport would delay every
		// other one. Rotate the first slot so a busy backend cannot starve peers.
		for (size_t i = 0; i < m_connections.size(); ++i) {
			const size_t index = (m_receive_cursor + i) % m_connections.size();
			const auto &connection = m_connections[index];
			if (connection && connection->ReceiveTimeoutMs(pkt, 0)) {
				m_receive_cursor = (index + 1) % m_connections.size();
				return true;
			}
		}
		const auto now = Clock::now();
		if (now >= deadline)
			return false;
		const auto remaining =
				std::chrono::ceil<std::chrono::milliseconds>(deadline - now);
		m_receive_signal->wait(static_cast<unsigned>(remaining.count()));
	}
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
	if (auto connection = getConnection(peer_id))
		connection->Send(peer_id, channelnum, pkt, reliable);
}

Address ConnectionMulti::GetPeerAddress(session_t peer_id)
{
	try {
		if (auto connection = getConnection(peer_id))
			return connection->GetPeerAddress(peer_id);
	} catch (...) {
	}
	throw con::PeerNotFoundException("No address for peer found!");
}

float ConnectionMulti::getPeerStat(session_t peer_id, con::rtt_stat_type type)
{
	if (auto connection = getConnection(peer_id))
		return connection->getPeerStat(peer_id, type);
	return {};
}

float ConnectionMulti::getLocalStat(con::rate_stat_type type)
{
	if (auto connection = getConnection(connected_to))
		return connection->getLocalStat(type);
	return {};
}

void ConnectionMulti::DisconnectPeer(session_t peer_id)
{
	if (auto connection = getConnection(peer_id))
		connection->DisconnectPeer(peer_id);
}

size_t ConnectionMulti::events_size()
{
	if (!m_serving) {
		if (auto *connection = getConnection(connected_to))
			return connection->events_size();
	}
	size_t ret = 0;
	for (const auto &connection : m_connections) {
		if (connection)
			ret += connection->events_size();
	}
	return ret;
}

} // namespace
