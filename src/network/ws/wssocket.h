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

#include <list>
#include "network/address.h"

#include <websocketpp/server.hpp>
#include <websocketpp/client.hpp>

#define USE_SSL 1

#if USE_SSL
#include <websocketpp/config/asio.hpp>
#include <websocketpp/config/asio_client.hpp>
#else
#include <websocketpp/config/asio_no_tls.hpp>
#include <websocketpp/config/asio_no_tls_client.hpp>
#endif

//extern bool socket_enable_debug_output;

class WSSocket
{
public:
	WSSocket() = default;

	WSSocket(bool ipv6);
	~WSSocket();
	void Bind(Address addr);
	bool Connect(const Address &addr);

	bool init(bool ipv6, bool noExceptions = false);

	void Send(const Address &destination, const void *data, int size);
	// Returns -1 if there is no data
	int Receive(Address &sender, void *data, int size);
	int GetHandle(); // For debugging purposes only
	void setTimeoutMs(int timeout_ms);
	// Returns true if there is data, false if timeout occurred
	bool WaitData(int timeout_ms);

	using hdl_list = std::map<websocketpp::connection_hdl, Address,
			std::owner_less<websocketpp::connection_hdl>>;

#if USE_SSL
	using ws_server_t = websocketpp::server<websocketpp::config::asio_tls>;
	using ws_client_t = websocketpp::client<websocketpp::config::asio_tls>;
	typedef websocketpp::lib::shared_ptr<boost::asio::ssl::context> context_ptr;
#else
	using ws_server_t = websocketpp::server<websocketpp::config::asio>;
	using ws_client_t = websocketpp::client<websocketpp::config::asio>;
#endif

	typedef ws_server_t::message_ptr message_ptr;
	typedef ws_client_t::message_ptr client_message_ptr;

	ws_server_t server;
	ws_client_t client;
	hdl_list hdls;
	bool ws_serve = false;
	bool ws_client = false;
	websocketpp::connection_hdl client_hdl;
	Address client_address;

	void on_http(const websocketpp::connection_hdl &hdl);
	void on_fail(const websocketpp::connection_hdl &hdl);
	void on_close(const websocketpp::connection_hdl &hdl);
	void on_open(const websocketpp::connection_hdl &hdl);
	void on_message(const websocketpp::connection_hdl &hdl, const message_ptr &msg);
	void on_client_message(
			const websocketpp::connection_hdl &hdl, const client_message_ptr &msg);
	void on_client_open(const websocketpp::connection_hdl &hdl);
	void on_client_fail(const websocketpp::connection_hdl &hdl);
	void on_client_close(const websocketpp::connection_hdl &hdl);
#if USE_SSL
	context_ptr on_tls_init(const websocketpp::connection_hdl &hdl);
	context_ptr on_client_tls_init(const websocketpp::connection_hdl &hdl);
#endif

private:
	struct queue_item
	{
		Address address;
		std::string data;
	};
	std::list<queue_item> incoming_queue;

	int m_timeout_ms = -1;
};
