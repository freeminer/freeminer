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
#include "network/fm_connection_sctp.h"
#include "network/networkprotocol.h"
#include "network/peerhandler.h"
#include "threading/concurrent_map.h"
#include "threading/thread_vector.h"
#include "util/container.h"
#include "util/pointer.h"

#include <memory>
#include <unordered_map>

#include "external/usrsctp/usrsctplib/usrsctp.h"

#include <websocketpp/server.hpp>

#define USE_SSL 1

#if USE_SSL
#include <websocketpp/config/asio.hpp>
#else
#include <websocketpp/config/asio_no_tls.hpp>
#endif

// #define CHANNEL_COUNT 3

// class NetworkPacket;

namespace con
{
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
	friend class con_multi::Connection;

	Connection(u32 protocol_id, u32 max_packet_size, float timeout, bool ipv6,
			con::PeerHandler *peerhandler = nullptr);
	~Connection();

private:
	int receive() override;
	void serve(Address address) override;

protected:
	websocketpp::connection_hdl getPeer(session_t peer_id);

private:
	concurrent_map<u16, websocketpp::connection_hdl> m_peers_ws;

public:
	struct ulp_info_holder
	{
		Connection *self{};
		websocketpp::connection_hdl hdl{};
	};
private:
	struct ws_peer
	{
		session_t peer_id{};
		int fd[2]{};
		struct socket *sock{};
		std::unique_ptr<ulp_info_holder> ulp_info{};
	};
	bool ws_serve = false;
	using hdl_list = std::map<websocketpp::connection_hdl, std::shared_ptr<ws_peer>,
			std::owner_less<websocketpp::connection_hdl>>;

	//struct socket *sctp_server_sock{};


#if USE_SSL
	using ws_server_t = websocketpp::server<websocketpp::config::asio_tls>;
	//typedef websocketpp::config::asio_tls_client::message_type::ptr message_ptr;
	typedef websocketpp::lib::shared_ptr<boost::asio::ssl::context> context_ptr;
#else
	using ws_server_t = websocketpp::server<websocketpp::config::asio>;
#endif

public:
	ws_server_t Server;
private:
	hdl_list hdls;
	std::shared_mutex peersMutex;

	typedef ws_server_t::message_ptr message_ptr;

public:
	void on_http(websocketpp::connection_hdl hdl);
	void on_fail(websocketpp::connection_hdl hdl);
	void on_close(websocketpp::connection_hdl hdl);
	void on_open(websocketpp::connection_hdl hdl);
	void on_message(websocketpp::connection_hdl hdl, message_ptr msg);
#if USE_SSL
	context_ptr on_tls_init(const websocketpp::connection_hdl &hdl);
#endif
};

} // namespace
