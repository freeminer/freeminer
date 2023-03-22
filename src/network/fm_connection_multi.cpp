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

#include "fm_connection_multi.h"
#include "connection.h"
#include "fm_connection_sctp.h"

namespace con_multi
{

Connection::Connection(u32 protocol_id, u32 max_packet_size, float timeout, bool ipv6,
		con::PeerHandler *peerhandler) :
#if USE_SCTP
		m_con_sctp(std::make_shared<con_sctp::Connection>(
				PROTOCOL_ID, max_packet_size, timeout, ipv6, peerhandler)),
#endif
#if MINETEST_TRANSPORT
		m_con(std::make_shared<con::Connection>(
				PROTOCOL_ID, max_packet_size, timeout, ipv6, peerhandler)),
#endif
		dummy{}
{
}

Connection::~Connection()
{
}

void Connection::Serve(Address bind_addr)
{
#if MINETEST_TRANSPORT
	if (m_con)
		m_con->Serve(bind_addr);
#endif
#if USE_SCTP
	if (m_con_sctp) {
		auto addr_sctp = bind_addr;
		addr_sctp.setPort(addr_sctp.getPort() + 1);
		m_con_sctp->Serve(addr_sctp);
	}
#endif
}

void Connection::Connect(Address address)
{
#if USE_SCTP
	if (m_con_sctp)
		m_con_sctp->Connect(address);
#endif
#if MINETEST_TRANSPORT
	if (m_con)
		m_con->Connect(address);
#endif
}

bool Connection::Connected()
{
#if USE_SCTP
	if (m_con_sctp)
		if (auto c = m_con_sctp->Connected(); c)
			return c;
#endif
#if MINETEST_TRANSPORT
	if (m_con)
		if (auto c = m_con->Connected(); c)
			return c;
	return false;
#endif
}

void Connection::Disconnect()
{
#if MINETEST_TRANSPORT
	if (m_con)
		m_con->Disconnect();
#endif
#if USE_SCTP
	if (m_con_sctp)
		m_con_sctp->Disconnect();
#endif
}

u32 Connection::Receive(NetworkPacket *pkt, int timeout)
{
	u32 ret = 0;
#if USE_SCTP
	if (m_con_sctp)
		ret += m_con_sctp->Receive(pkt, timeout / 2);
	if (ret)
		return ret;
#endif
#if MINETEST_TRANSPORT
	if (m_con)
		ret += m_con->Receive(pkt, timeout / 2);
#endif
	return ret;
}

bool Connection::TryReceive(NetworkPacket *pkt)
{
	return Receive(pkt, 0);
}

void Connection::Send(session_t peer_id, u8 channelnum, NetworkPacket *pkt, bool reliable)
{
	// TODO send to one
#if MINETEST_TRANSPORT
	if (m_con && &m_con.get()->getPeerNoEx(peer_id))
		m_con->Send(peer_id, channelnum, pkt, reliable);
#endif
#if USE_SCTP
	if (m_con_sctp && m_con_sctp->getPeer(peer_id))
		m_con_sctp->Send(peer_id, channelnum, pkt, reliable);
#endif
}

void Connection::Send(
		session_t peer_id, u8 channelnum, const msgpack::sbuffer &buffer, bool reliable)
{
	// TODO send to one
#if MINETEST_TRANSPORT
	//	if (m_con)
	//		m_con->Send(peer_id, channelnum, buffer, reliable);
#endif
#if USE_SCTP
	if (m_con_sctp)
		m_con_sctp->Send(peer_id, channelnum, buffer, reliable);
#endif
}

Address Connection::GetPeerAddress(session_t peer_id)
{
#if MINETEST_TRANSPORT
	if (m_con && &m_con.get()->getPeerNoEx(peer_id))
		return m_con->GetPeerAddress(peer_id);
#endif
#if USE_SCTP
	if (m_con_sctp && m_con_sctp->getPeer(peer_id))
		return m_con_sctp->GetPeerAddress(peer_id);
#endif
	return {};
}

float Connection::getPeerStat(session_t peer_id, con::rtt_stat_type type)
{
#if MINETEST_TRANSPORT
	if (m_con && &m_con.get()->getPeerNoEx(peer_id))
		return m_con->getPeerStat(peer_id, type);
#endif
#if USE_SCTP
	if (m_con_sctp && m_con_sctp->getPeer(peer_id))
		return m_con_sctp->getPeerStat(peer_id, type);
#endif
	return {};
}

float Connection::getLocalStat(con::rate_stat_type type)
{
#if MINETEST_TRANSPORT
	if (m_con)
		return m_con->getLocalStat(type);
#endif
	return {};
}

void Connection::DisconnectPeer(session_t peer_id)
{
#if MINETEST_TRANSPORT
	if (m_con && &m_con.get()->getPeerNoEx(peer_id))
		return m_con->DisconnectPeer(peer_id);
#endif
#if USE_SCTP
	if (m_con_sctp && m_con_sctp->getPeer(peer_id))
		return m_con_sctp->DisconnectPeer(peer_id);
#endif
}

size_t Connection::events_size()
{
	size_t ret = 0;
#if MINETEST_TRANSPORT
	if (m_con)
		ret += m_con->events_size();
#endif
#if USE_SCTP
	if (m_con_sctp)
		ret += m_con_sctp->events_size();
#endif
	return ret;
}

} // namespace
