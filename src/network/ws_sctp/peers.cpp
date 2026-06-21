/*
Copyright (C) 2023 proller <proler@gmail.com>
*/

#include "network/ws_sctp/internal.h"

#if USE_WEBSOCKET_SCTP

namespace con_ws_sctp
{

static constexpr auto WS_SCTP_RETIRED_ULP_TTL = std::chrono::seconds(30);
static constexpr size_t WS_SCTP_RETIRED_ULP_MAX = 4096;

session_t Connection::nextPeerId()
{
	for (size_t i = 0; i <= PEER_SCTP_MAX - PEER_SCTP_MIN; ++i) {
		session_t peer_id = m_next_ws_peer_id++;
		if (m_next_ws_peer_id > PEER_SCTP_MAX)
			m_next_ws_peer_id = PEER_SCTP_MIN;
		if (!m_peers.count(peer_id) && !m_peers_ws.count(peer_id))
			return peer_id;
	}
	return PEER_ID_INEXISTENT;
}

std::shared_ptr<Connection::ws_peer> Connection::makePeer(
		websocketpp::connection_hdl hdl, session_t peer_id, bool client)
{
	auto peer = std::make_shared<ws_peer>();
	peer->peer_id = peer_id;
	peer->ulp_info = std::make_unique<ulp_info_holder>(this, std::move(hdl), client);
	usrsctp_register_address(peer->ulp_info.get());
	peer->registered = true;
	return peer;
}

bool Connection::setupListenSocket(const std::shared_ptr<ws_peer> &peer)
{
	if (!peer || !peer->ulp_info)
		return false;

	if ((peer->listen_sock = usrsctp_socket(AF_CONN, SOCK_STREAM, IPPROTO_SCTP, NULL,
				 NULL, 0, NULL)) == NULL) {
		perror("usrsctp_socket");
		return false;
	}
	sock_setup(peer->listen_sock);

	struct sockaddr_conn sconn = {};
	sconn.sconn_family = AF_CONN;
#ifdef HAVE_SCONN_LEN
	sconn.sconn_len = sizeof(struct sockaddr_conn);
#endif
	sconn.sconn_port = htons(ws_sctp_port);
	sconn.sconn_addr = peer->ulp_info.get();
	if (usrsctp_bind(peer->listen_sock, (struct sockaddr *)&sconn, sizeof(sconn)) < 0) {
		perror("usrsctp_bind");
		return false;
	}
	if (usrsctp_listen(peer->listen_sock, 1) < 0) {
		perror("usrsctp_listen");
		return false;
	}
	return true;
}

bool Connection::setupClientSocket(
		const std::shared_ptr<ws_peer> &peer, const Address &address)
{
	if (!peer || !peer->ulp_info)
		return false;

	if ((peer->sock = usrsctp_socket(AF_CONN, SOCK_STREAM, IPPROTO_SCTP, NULL,
				 NULL, 0, NULL)) == NULL) {
		perror("usrsctp_socket");
		return false;
	}
	sock_setup(peer->sock);

	struct sockaddr_conn sconn = {};
	sconn.sconn_family = AF_CONN;
#ifdef HAVE_SCONN_LEN
	sconn.sconn_len = sizeof(struct sockaddr_conn);
#endif
	sconn.sconn_port = htons(0);
	sconn.sconn_addr = NULL;
	if (usrsctp_bind(peer->sock, (struct sockaddr *)&sconn, sizeof(sconn)) < 0) {
		perror("usrsctp_bind");
		return false;
	}

	sconn = {};
	sconn.sconn_family = AF_CONN;
#ifdef HAVE_SCONN_LEN
	sconn.sconn_len = sizeof(struct sockaddr_conn);
#endif
	sconn.sconn_port = htons(address.getPort());
	sconn.sconn_addr = peer->ulp_info.get();
	if (usrsctp_connect(peer->sock, (struct sockaddr *)&sconn, sizeof(sconn)) < 0 &&
			errno != EINPROGRESS && errno != EWOULDBLOCK) {
		perror("usrsctp_connect");
		return false;
	}

	m_peers.insert_or_assign(PEER_ID_SERVER, peer->sock);
	m_peers_ws.insert_or_assign(PEER_ID_SERVER, peer->ulp_info->hdl);
	return true;
}

void Connection::closePeer(const std::shared_ptr<ws_peer> &peer, bool timeout)
{
	if (!peer)
		return;

	if (peer->peer_added) {
		putEvent(ConnectionEvent::peerRemoved(peer->peer_id, timeout, {}));
		peer->peer_added = false;
	}
	m_peers_ws.erase(peer->peer_id);
	m_peers.erase(peer->peer_id);

	if (peer->ulp_info) {
		peer->ulp_info->active.store(false, std::memory_order_release);
		peer->ulp_info->self.store(nullptr, std::memory_order_release);
	}

	if (peer->sock) {
		usrsctp_close(peer->sock);
		peer->sock = nullptr;
	}
	if (peer->listen_sock) {
		usrsctp_close(peer->listen_sock);
		peer->listen_sock = nullptr;
	}
	if (peer->registered && peer->ulp_info) {
		usrsctp_deregister_address(peer->ulp_info.get());
		peer->registered = false;
	}
	if (peer->ulp_info) {
		const auto lock = std::unique_lock(retiredPeersMutex);
		retiredUlpInfo.push_back(
				{std::move(peer->ulp_info), std::chrono::steady_clock::now()});
	}
}

void Connection::cleanupRetiredUlpInfo(bool force)
{
	const auto lock = std::unique_lock(retiredPeersMutex);
	if (force) {
		retiredUlpInfo.clear();
		return;
	}

	const auto expire_before = std::chrono::steady_clock::now() - WS_SCTP_RETIRED_ULP_TTL;
	retiredUlpInfo.erase(
			std::remove_if(retiredUlpInfo.begin(), retiredUlpInfo.end(),
					[expire_before](const retired_ulp_info &info) {
						return info.retired_at < expire_before;
					}),
			retiredUlpInfo.end());

	if (retiredUlpInfo.size() > WS_SCTP_RETIRED_ULP_MAX) {
		const size_t excess = retiredUlpInfo.size() - WS_SCTP_RETIRED_ULP_MAX;
		retiredUlpInfo.erase(retiredUlpInfo.begin(), retiredUlpInfo.begin() + excess);
	}
}

void Connection::closeWebSocketState()
{
	websocketpp::lib::error_code ec;
	std::vector<std::pair<websocketpp::connection_hdl, std::shared_ptr<ws_peer>>> peers;
	{
		const auto lock = std::unique_lock(peersMutex);
		for (auto &entry : hdls)
			peers.emplace_back(entry.first, entry.second);
		hdls.clear();
	}

	for (const auto &entry : peers) {
		ec.clear();
		Server.close(entry.first, websocketpp::close::status::going_away,
				"disconnect", ec);
		closePeer(entry.second, false);
	}

	if (client_peer) {
		ec.clear();
		Client.close(client_hdl, websocketpp::close::status::going_away,
				"disconnect", ec);
		closePeer(client_peer, false);
		client_peer.reset();
	}

	ec.clear();
	if (ws_serve)
		Server.stop_listening(ec);
	Server.stop();
	Client.stop();
	ws_serve = false;
	ws_client = false;
	ws_client_open.store(false, std::memory_order_release);
	ws_sctp_connected.store(false, std::memory_order_release);
}

int Connection::acceptPeers()
{
	int n = 0;
	std::vector<std::shared_ptr<ws_peer>> peers;
	{
		const auto lock = std::shared_lock(peersMutex);
		for (const auto &entry : hdls)
			peers.push_back(entry.second);
	}

	for (const auto &peer : peers) {
		if (!peer || !peer->listen_sock || peer->sock)
			continue;

		usrsctp_set_non_blocking(peer->listen_sock, 1);
		struct socket *conn_sock = nullptr;
		if ((conn_sock = usrsctp_accept(peer->listen_sock, NULL, NULL)) == NULL) {
			if (errno != EWOULDBLOCK && errno != EAGAIN)
				perror("usrsctp_accept");
			continue;
		}

		sock_setup(conn_sock);
		peer->sock = conn_sock;
		peer->peer_added = true;
		m_peers.insert_or_assign(peer->peer_id, conn_sock);
		m_peers_ws.insert_or_assign(peer->peer_id, peer->ulp_info->hdl);
		putEvent(ConnectionEvent::peerAdded(peer->peer_id, {}));
		++n;
	}
	return n;
}

bool Connection::deletePeer(session_t peer_id, bool timeout)
{
	if (peer_id == PEER_ID_SERVER && client_peer) {
		websocketpp::lib::error_code ec;
		Client.close(client_hdl, websocketpp::close::status::going_away,
				"peer removed", ec);
		closePeer(client_peer, timeout);
		client_peer.reset();
		ws_client_open.store(false, std::memory_order_release);
		ws_sctp_connected.store(false, std::memory_order_release);
		return true;
	}

	auto hdl_node = m_peers_ws.find(peer_id);
	if (hdl_node != m_peers_ws.end()) {
		std::shared_ptr<ws_peer> peer;
		websocketpp::connection_hdl hdl = hdl_node->second;
		{
			const auto lock = std::unique_lock(peersMutex);
			auto it = hdls.find(hdl);
			if (it != hdls.end()) {
				peer = it->second;
				hdls.erase(it);
			}
		}

		websocketpp::lib::error_code ec;
		Server.close(hdl, websocketpp::close::status::going_away,
				"peer removed", ec);
		closePeer(peer, timeout);
		return true;
	}

	return con_sctp::Connection::deletePeer(peer_id, timeout);
}


websocketpp::connection_hdl Connection::getPeer(session_t peer_id)
{
	auto node = m_peers_ws.find(peer_id);

	if (node == m_peers_ws.end())
		return {};

	return node->second;
}

} // namespace

#endif
