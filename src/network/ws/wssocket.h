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

#define USE_SSL 1

#if USE_SSL
#include <websocketpp/config/asio.hpp>
#else
#include <websocketpp/config/asio_no_tls.hpp>
#endif

//extern bool socket_enable_debug_output;

class WSSocket
{
public:
	WSSocket() = default;

	WSSocket(bool ipv6);
	~WSSocket();
	void Bind(Address addr);

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
	//typedef websocketpp::config::asio_tls_client::message_type::ptr message_ptr;
	typedef websocketpp::lib::shared_ptr<boost::asio::ssl::context> context_ptr;
#else
	using ws_server_t = websocketpp::server<websocketpp::config::asio>;
#endif

	typedef ws_server_t::message_ptr message_ptr;

	ws_server_t server;
	hdl_list hdls;
	bool ws_serve = false;
	void on_http(const websocketpp::connection_hdl &hdl);
	void on_fail(const websocketpp::connection_hdl &hdl);
	void on_close(const websocketpp::connection_hdl &hdl);
	void on_open(const websocketpp::connection_hdl &hdl);
	void on_message(const websocketpp::connection_hdl &hdl, const message_ptr &msg);
#if USE_SSL
	context_ptr on_tls_init(const websocketpp::connection_hdl &hdl);
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
