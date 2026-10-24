/*
Copyright (C) 2023 proller <proler@gmail.com>
SPDX-License-Identifier: GPL-3.0-or-later
*/
#include "network/ws_sctp/internal.h"
#if USE_WEBSOCKET_SCTP
#include <atomic>
#include <mutex>
#include <unordered_map>

namespace con_ws_sctp
{
namespace
{
struct Output
{
	std::weak_ptr<ws_transport::Transport> transport;
	ws_transport::Transport::Id websocket;
};
std::mutex output_mutex;
std::unordered_map<void *, Output> outputs;
std::atomic<uintptr_t> next_address{1};
void *registerOutput(const std::shared_ptr<ws_transport::Transport> &transport,
		ws_transport::Transport::Id websocket)
{
	// Never recycle opaque AF_CONN addresses: late stack callbacks cannot target
	// a newer connection, and no dangling application pointer is dereferenced.
	void *address = reinterpret_cast<void *>(next_address.fetch_add(1));
	std::lock_guard lock(output_mutex);
	outputs.emplace(address, Output{transport, websocket});
	return address;
}
}
int connectionOutput(void *address, void *data, size_t size, uint8_t, uint8_t)
{
	std::shared_ptr<ws_transport::Transport> transport;
	ws_transport::Transport::Id websocket;
	{
		std::lock_guard lock(output_mutex);
		auto it = outputs.find(address);
		if (it == outputs.end())
			return EINVAL;
		transport = it->second.transport.lock();
		websocket = it->second.websocket;
	}
	return transport ? transport->send(websocket, data, size) : EINVAL;
}
Connection::Connection(u32 protocol_id, u32 max_packet_size, float timeout, bool ipv6,
		con::PeerHandler *peerhandler) :
		con_sctp::Connection(max_packet_size, timeout, ipv6, peerhandler, false)
{
	(void)protocol_id;
	m_ipv6 = ipv6;
	domain = AF_CONN;
	sctp_conn_output = connectionOutput;
	m_transport = std::make_shared<ws_transport::Transport>();
	start();
}
Connection::~Connection()
{
	join();
	disconnect();
	finish_sctp();
}
void Connection::processCommand(con::ConnectionCommandPtr command)
{
	switch (command->type) {
	case con::CONNCMD_CONNECT:
		stopWebSockets();
		m_transport = std::make_shared<ws_transport::Transport>();
		m_connecting = true;
		m_destination = command->address;
		if (!m_transport->connect(m_destination)) {
			m_connecting = false;
			putEvent(con::ConnectionEvent::connectFailed());
		}
		return;
	case con::CONNCMD_DISCONNECT:
		disconnect();
		return;
	default:
		con_sctp::Connection::processCommand(command);
		return;
	}
}
void Connection::serve(const Address &address)
{
	m_port = address.getPort();
	const Address listener =
			m_ipv6 && address.isAny()
					? Address(static_cast<const IPv6AddressBytes *>(nullptr),
							  address.getPort())
					: address;
	if (!m_transport->listen(listener))
		putEvent(con::ConnectionEvent::bindFailed());
}
bool Connection::openPeer(const ws_transport::Transport::Event &event)
{
	// Do not eagerly initialize the shared stack with UDP disabled just because
	// multi mode constructs an unused WebSocket transport.
	sctp_setup(0);
	auto peer = std::make_shared<Peer>();
	peer->websocket = event.id;
	if (event.client)
		peer->id = PEER_ID_SERVER;
	else {
		for (size_t i = 0; i <= LAST_PEER_ID - FIRST_PEER_ID; ++i) {
			const auto id = m_next_id++;
			if (m_next_id > LAST_PEER_ID)
				m_next_id = FIRST_PEER_ID;
			if (!m_reserved_ids.count(id)) {
				peer->id = id;
				break;
			}
		}
		if (peer->id == PEER_ID_INEXISTENT)
			return false;
	}
	m_reserved_ids.insert(peer->id);
	peer->address = registerOutput(m_transport, event.id);
	usrsctp_register_address(peer->address);
	m_websockets.emplace(event.id, peer);
	setPeerAddress(peer->id, event.client ? m_destination : event.address);
	auto *socket = usrsctp_socket(
			AF_CONN, SOCK_STREAM, IPPROTO_SCTP, nullptr, nullptr, 0, nullptr);
	if (!socket)
		return false;
	if (event.client)
		peer->socket = socket;
	else
		peer->listener = socket;
	sock_setup(socket);
	sockaddr_conn address = {};
	address.sconn_family = AF_CONN;
#ifdef HAVE_SCONN_LEN
	address.sconn_len = sizeof(address);
#endif
	address.sconn_port = htons(event.client ? 0 : m_port);
	address.sconn_addr = event.client ? nullptr : peer->address;
	if (usrsctp_bind(socket, reinterpret_cast<sockaddr *>(&address), sizeof(address)) < 0)
		return false;
	if (!event.client)
		return usrsctp_listen(socket, 1) == 0;
	address.sconn_addr = peer->address;
	address.sconn_port = htons(m_destination.getPort());
	m_peers.insert_or_assign(peer->id, socket);
	if (usrsctp_connect(socket, reinterpret_cast<sockaddr *>(&address), sizeof(address)) <
					0 &&
			errno != EINPROGRESS && errno != EAGAIN && errno != EWOULDBLOCK)
		return false;
	return true;
}
void Connection::closePeer(const std::shared_ptr<Peer> &peer, bool timeout)
{
	if (peer->address) {
		std::lock_guard lock(output_mutex);
		outputs.erase(peer->address);
	}
	m_reserved_ids.erase(peer->id);
	clearPeerState(peer->id);
	m_peers.erase(peer->id);
	if (peer->socket)
		usrsctp_close(peer->socket);
	if (peer->listener)
		usrsctp_close(peer->listener);
	if (peer->address)
		usrsctp_deregister_address(peer->address);
	peer->socket = peer->listener = nullptr;
	peer->address = nullptr;
	if (peer->added)
		putEvent(con::ConnectionEvent::peerRemoved(peer->id, timeout, {}));
}
bool Connection::deletePeer(session_t id, bool timeout)
{
	for (auto it = m_websockets.begin(); it != m_websockets.end(); ++it) {
		if (it->second->id != id)
			continue;
		auto peer = it->second;
		m_websockets.erase(it);
		closePeer(peer, timeout);
		m_transport->close(peer->websocket);
		return true;
	}
	return false;
}
void Connection::disconnect()
{
	stopWebSockets();
	con_sctp::Connection::disconnect();
}
void Connection::onAssociationChange(session_t id, const sctp_assoc_change *change)
{
	if (id != PEER_ID_SERVER)
		return;
	m_connecting = false;
	if (change->sac_state == SCTP_COMM_UP || change->sac_state == SCTP_RESTART) {
		for (auto &[websocket, peer] : m_websockets)
			if (peer->id == id)
				peer->added = true;
	}
}
void Connection::stopWebSockets()
{
	for (auto &[id, peer] : m_websockets)
		closePeer(peer, false);
	m_websockets.clear();
	if (m_transport)
		m_transport->stop();
	m_connecting = false;
}
int Connection::receive()
{
	int progress = 0;
	for (auto &event : m_transport->poll()) {
		++progress;
		if (event.type == ws_transport::Transport::Type::Open) {
			if (!openPeer(event)) {
				auto it = m_websockets.find(event.id);
				if (it != m_websockets.end()) {
					closePeer(it->second, false);
					m_websockets.erase(it);
				}
				m_transport->close(event.id);
				if (event.client) {
					m_connecting = false;
					putEvent(con::ConnectionEvent::connectFailed());
				}
			}
		} else if (event.type == ws_transport::Transport::Type::Message) {
			auto it = m_websockets.find(event.id);
			if (it != m_websockets.end())
				usrsctp_conninput(
						it->second->address, event.data.data(), event.data.size(), 0);
		} else {
			const bool connecting = event.client && m_connecting;
			auto it = m_websockets.find(event.id);
			if (it != m_websockets.end()) {
				auto peer = it->second;
				m_websockets.erase(it);
				closePeer(peer, false);
			}
			if (connecting) {
				m_connecting = false;
				putEvent(con::ConnectionEvent::connectFailed());
			}
			m_transport->close(event.id);
		}
	}
	for (auto &[id, peer] : m_websockets) {
		if (!peer->listener || peer->socket)
			continue;
		peer->socket = usrsctp_accept(peer->listener, nullptr, nullptr);
		if (!peer->socket)
			continue;
		// Accepted sockets inherit their listener's negotiated SCTP options.
		usrsctp_set_non_blocking(peer->socket, 1);
		m_peers.insert_or_assign(peer->id, peer->socket);
		peer->added = true;
		putEvent(con::ConnectionEvent::peerAdded(peer->id, {}));
		++progress;
	}
	return progress + con_sctp::Connection::receive();
}
}
#endif
