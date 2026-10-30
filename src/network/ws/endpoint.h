// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include "log.h"
#include "settings.h"
#include <websocketpp/common/asio.hpp>
#include <websocketpp/client.hpp>
#include <websocketpp/server.hpp>
namespace ws_transport
{
template <class Endpoint>
void initEndpoint(Endpoint &endpoint)
{
	endpoint.set_error_channels(websocketpp::log::elevel::none);
	endpoint.set_access_channels(websocketpp::log::alevel::none);
	endpoint.set_open_handshake_timeout(30000);
	endpoint.set_close_handshake_timeout(30000);
	endpoint.set_pong_timeout(30000);
	endpoint.set_max_message_size(16 * 1024 * 1024);
	endpoint.init_asio();
	// Game datagrams should not wait for Nagle/delayed-ACK coalescing.
	endpoint.set_tcp_post_init_handler([&endpoint](websocketpp::connection_hdl hdl) {
		websocketpp::lib::asio::error_code ec;
		endpoint.get_con_from_hdl(hdl)->get_raw_socket().set_option(
				websocketpp::lib::asio::ip::tcp::no_delay(true), ec);
		if (ec)
			warningstream << "WebSocket TCP_NODELAY: " << ec.message() << '\n';
	});
}
template <class Endpoint>
void initServer(Endpoint &endpoint)
{
	initEndpoint(endpoint);
	endpoint.set_listen_backlog(100);
	endpoint.set_max_http_body_size(500000000);
	endpoint.set_reuse_addr(true);
}
}
