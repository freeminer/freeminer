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
#include "network/sctp/connection.h"
#include "network/networkprotocol.h"
#include "network/peerhandler.h"
#include "threading/concurrent_map.h"
#include "threading/thread_vector.h"
#include "util/container.h"
#include "util/pointer.h"

#include <atomic>
#include <memory>
#include <mutex>
#include <unordered_map>
#include <vector>

#include "external/usrsctp/usrsctplib/usrsctp.h"

#include <chrono>
#include <websocketpp/client.hpp>
#include <websocketpp/server.hpp>

#ifndef USE_WS_SCTP_SSL
#define USE_WS_SCTP_SSL 0
#endif

#if USE_WS_SCTP_SSL
#include <websocketpp/config/asio.hpp>
#include <websocketpp/config/asio_client.hpp>
#else
#include <websocketpp/config/asio_no_tls.hpp>
#include <websocketpp/config/asio_no_tls_client.hpp>
#endif

// #define CHANNEL_COUNT 3

// class NetworkPacket;

namespace con
{
class ConnectionMulti;
class PeerHandler;
}
namespace con_multi
{
class Connection;
}
namespace con_ws_sctp
{
using namespace con;

class Connection : public con_sctp::Connection
{
public:
	friend class con::ConnectionMulti;
	friend class con_multi::Connection;

	Connection(u32 protocol_id, u32 max_packet_size, float timeout, bool ipv6,
			con::PeerHandler *peerhandler = nullptr);
	~Connection();
	void Connect(Address address) override;
	bool Connected() override;
	void Disconnect() override;
	void DisconnectPeer(session_t peer_id) override;

private:
	void processCommand(ConnectionCommandPtr c) override;
	int receive() override;
	void serve(const Address &address) override;
	bool deletePeer(session_t peer_id, bool timeout = 0) override;
	void onAssociationChange(
			session_t peer_id, const struct sctp_assoc_change *sac) override;

protected:
	websocketpp::connection_hdl getPeer(session_t peer_id);

private:
	concurrent_map<session_t, websocketpp::connection_hdl> m_peers_ws;

public:
	struct ulp_info_holder
	{
		ulp_info_holder(Connection *self_, websocketpp::connection_hdl hdl_, bool client_) :
			self(self_), hdl(std::move(hdl_)), client(client_)
		{
		}

		std::atomic<Connection *> self{};
		websocketpp::connection_hdl hdl{};
		bool client{};
		std::atomic_bool active{true};
	};
private:
	struct ws_peer
	{
		session_t peer_id{};
		struct socket *listen_sock{};
		struct socket *sock{};
		std::unique_ptr<ulp_info_holder> ulp_info{};
		bool registered{};
		bool peer_added{};
	};
	struct retired_ulp_info
	{
		std::unique_ptr<ulp_info_holder> ulp_info{};
		std::chrono::steady_clock::time_point retired_at{};
	};
	bool ws_serve = false;
	bool ws_client = false;
	std::atomic_bool ws_client_open{false};
	std::atomic_bool ws_sctp_connected{false};
	bool ws_ipv6 = false;
	u16 ws_sctp_port = 0;
	session_t m_next_ws_peer_id = PEER_SCTP_MIN;
	using hdl_list = std::map<websocketpp::connection_hdl, std::shared_ptr<ws_peer>,
			std::owner_less<websocketpp::connection_hdl>>;

	//struct socket *sctp_server_sock{};


#if USE_WS_SCTP_SSL
	using ws_server_t = websocketpp::server<websocketpp::config::asio_tls>;
	using ws_client_t = websocketpp::client<websocketpp::config::asio_tls_client>;
	//typedef websocketpp::config::asio_tls_client::message_type::ptr message_ptr;
	typedef websocketpp::lib::shared_ptr<boost::asio::ssl::context> context_ptr;
#else
	using ws_server_t = websocketpp::server<websocketpp::config::asio>;
	using ws_client_t = websocketpp::client<websocketpp::config::asio_client>;
#endif

public:
	ws_server_t Server;
	ws_client_t Client;
private:
	hdl_list hdls;
	std::shared_ptr<ws_peer> client_peer;
	websocketpp::connection_hdl client_hdl;
	Address client_address;
	std::shared_mutex peersMutex;
	std::mutex retiredPeersMutex;
	std::vector<retired_ulp_info> retiredUlpInfo;

	typedef ws_server_t::message_ptr message_ptr;
	typedef ws_client_t::message_ptr client_message_ptr;

private:
	session_t nextPeerId();
	std::shared_ptr<ws_peer> makePeer(
			websocketpp::connection_hdl hdl, session_t peer_id, bool client);
	bool setupListenSocket(const std::shared_ptr<ws_peer> &peer);
	bool setupClientSocket(const std::shared_ptr<ws_peer> &peer, const Address &address);
	void connectWebSocket(const Address &address);
	void disconnectWebSocket();
	void closePeer(const std::shared_ptr<ws_peer> &peer, bool timeout);
	void closeWebSocketState();
	void cleanupRetiredUlpInfo(bool force = false);
	int acceptPeers();
	void pumpWebSockets();

public:
	void on_http(websocketpp::connection_hdl hdl);
	void on_fail(websocketpp::connection_hdl hdl);
	void on_close(websocketpp::connection_hdl hdl);
	void on_open(websocketpp::connection_hdl hdl);
	void on_message(websocketpp::connection_hdl hdl, message_ptr msg);
	void on_client_open(websocketpp::connection_hdl hdl);
	void on_client_fail(websocketpp::connection_hdl hdl);
	void on_client_close(websocketpp::connection_hdl hdl);
	void on_client_message(websocketpp::connection_hdl hdl, client_message_ptr msg);
#if USE_WS_SCTP_SSL
	context_ptr on_tls_init(const websocketpp::connection_hdl &hdl);
	context_ptr on_client_tls_init(const websocketpp::connection_hdl &hdl);
#endif
};

} // namespace
