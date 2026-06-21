/*
Copyright (C) 2023 proller <proler@gmail.com>
*/

#include "network/sctp/internal.h"

#if USE_SCTP

namespace con_sctp
{

struct socket *Connection::getPeer(session_t peer_id)
{
	auto node = m_peers.find(peer_id);

	if (node == m_peers.end())
		return NULL;

	return node->second;
}

bool Connection::deletePeer(session_t peer_id, bool timeout)
{
	cs << "Connection::deletePeer " << peer_id << ", " << timeout << std::endl;
	Address peer_address;
	// any peer has a primary address this never fails!
	// peer->getAddress(MTP_PRIMARY, peer_address);

	if (!peer_id) {
		if (sock) {
			usrsctp_close(sock);
			sock = nullptr;

			putEvent(ConnectionEvent::peerRemoved(peer_id, timeout, {}));

			return true;
		} else {
			return false;
		}
	}
	if (m_peers.find(peer_id) == m_peers.end())
		return false;

	// Create event
	putEvent(ConnectionEvent::peerRemoved(peer_id, timeout, {}));

	{
		const auto lock = m_peers.lock_unique_rec();
		auto sock = m_peers.get(peer_id);
		if (sock)
			usrsctp_close(sock);

		// delete m_peers[peer_id]; -- enet should handle this
		m_peers.erase(peer_id);
	}
	m_peers_address.erase(peer_id);
	return true;
}

/* Interface */
/*
ConnectionEvent Connection::getEvent() {
	if(m_event_queue.empty()) {
		ConnectionEvent e;
		e.type = CONNEVENT_NONE;
		return e;
	}
	return m_event_queue.pop_frontNoEx();
}
*/

ConnectionEventPtr Connection::waitEvent(u32 timeout_ms)
{
	try {
		return m_event_queue.pop_front(timeout_ms);
	} catch (const ItemNotFoundException &ex) {
		return ConnectionEvent::create(CONNEVENT_NONE);
	}
}

void Connection::putCommand(ConnectionCommandPtr c)
{
	// TODO? if (!m_shutting_down)
	{
		m_command_queue.push_back(c);
		// m_sendThread->Trigger();
	}
}

void Connection::Serve(Address bind_addr)
{
	putCommand(ConnectionCommand::serve(bind_addr));
}

void Connection::Connect(Address address)
{
	putCommand(ConnectionCommand::connect(address));
}

bool Connection::Connected()
{
	auto node = m_peers.find(PEER_ID_SERVER);
	if (node == m_peers.end())
		return false;

	// TODO: why do we even need to know our peer id?
	if (!m_peer_id)
		m_peer_id = 2;

	if (m_peer_id == PEER_ID_INEXISTENT)
		return false;

	return true;
}

void Connection::Disconnect()
{
	putCommand(ConnectionCommand::disconnect());
}

u32 Connection::Receive(NetworkPacket *pkt, int timeout)
{
	for (;;) {
		auto e = waitEvent(timeout);
		if (e->type != CONNEVENT_NONE)
			dout_con << getDesc() << ": Receive: got event: " << e->describe()
					 << std::endl;
		switch (e->type) {
		case CONNEVENT_NONE:
			// throw NoIncomingDataException("No incoming data");
			return 0;
		case CONNEVENT_DATA_RECEIVED:
			if (e->data.getSize() < 2) {
				continue;
			}
			pkt->putRawPacket(*e->data, e->data.getSize(), e->peer_id);
			return e->data.getSize();
		case CONNEVENT_PEER_ADDED: {
			if (m_bc_peerhandler)
				m_bc_peerhandler->peerAdded(e->peer_id);
			continue;
		}
		case CONNEVENT_PEER_REMOVED: {
			if (m_bc_peerhandler)
				m_bc_peerhandler->deletingPeer(e->peer_id, e->timeout);
			continue;
		}
		case CONNEVENT_BIND_FAILED:
			throw ConnectionBindFailed("Failed to bind socket "
									   "(port already in use?)");
		case CONNEVENT_CONNECT_FAILED:
			throw ConnectionException("Failed to connect");
		}
	}
	return 0;
}

bool Connection::ReceiveTimeoutMs(NetworkPacket *pkt, u32 timeout_ms)
{
	return Receive(pkt, timeout_ms) != 0;
}

bool Connection::TryReceive(NetworkPacket *pkt)
{
	return Receive(pkt, 0);
}

/*
void Connection::SendToAll(u8 channelnum, SharedBuffer<u8> data, bool reliable) {
	assert(channelnum < CHANNEL_COUNT);

	ConnectionCommand c;
	c.sendToAll(channelnum, data, reliable);
	putCommand(c);
}
*/

void Connection::Send(session_t peer_id, u8 channelnum, NetworkPacket *pkt, bool reliable)
{
	assert(channelnum < CHANNEL_COUNT); // Pre-condition

	putCommand(ConnectionCommand::send(peer_id, channelnum, pkt, reliable));
}

void Connection::Send(
		session_t peer_id, u8 channelnum, SharedBuffer<u8> data, bool reliable)
{
	assert(channelnum < CHANNEL_COUNT); // Pre-condition

	putCommand(ConnectionCommand::send(peer_id, channelnum, data, reliable));
}

void Connection::Send(
		session_t peer_id, u8 channelnum, const msgpack::sbuffer &buffer, bool reliable)
{
	SharedBuffer<u8> data((unsigned char *)buffer.data(), buffer.size());
	Send(peer_id, channelnum, data, reliable);
}

Address Connection::GetPeerAddress(session_t peer_id)
{
	if (!m_peers_address.count(peer_id))
		return Address();
	return m_peers_address.get(peer_id);
}

/*
void Connection::DeletePeer(u16 peer_id) {
	ConnectionCommand c;
	c.deletePeer(peer_id);
	putCommand(c);
}
*/

void Connection::PrintInfo(std::ostream &out)
{
	out << getDesc() << ": ";
}

void Connection::PrintInfo()
{
	PrintInfo(dout_con);
}

std::string Connection::getDesc()
{
	return "";
	// return std::string("con(")+itos(m_socket.GetHandle())+"/"+itos(m_peer_id)+")";
}

float Connection::getPeerStat(session_t peer_id, rtt_stat_type type)
{
	return 0;
}

float Connection::getLocalStat(con::rate_stat_type type)
{
	return 0;
}

void Connection::DisconnectPeer(session_t peer_id)
{
	putCommand(ConnectionCommand::disconnect_peer(peer_id));
}

} // namespace

#endif
