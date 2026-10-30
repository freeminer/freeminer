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

// Todo: pass disconnect to Connection (need to change api)

/* 
openssl genrsa > privkey.pem                             
openssl req -new -x509 -key privkey.pem > fullchain.pem  
*/

#include "config.h"
#if USE_WEBSOCKET

#include "wssocket.h"
#include "endpoint.h"
#include "transport.h"
#include "http.h"
#if USE_SSL
#include "tls.h"
#endif
#include "constants.h"
#include "filesys.h"
#include "log.h"
#include "../server/serverlist.h"
#include "settings.h"
#include "util/numeric.h"
#include "util/string.h"
#include <string>
#include <future>
#include <chrono>
#include <boost/asio/post.hpp>

#ifdef _WIN32
// Without this some of the network functions are not found on mingw
#ifndef _WIN32_WINNT
#define _WIN32_WINNT 0x0501
#endif
#include <windows.h>
#include <winsock2.h>
#include <ws2tcpip.h>
#define LAST_SOCKET_ERR() WSAGetLastError()
#define SOCKET_ERR_STR(e) itos(e)
typedef int socklen_t;
#else
#include <cerrno>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <fcntl.h>
#include <netdb.h>
#include <unistd.h>
#include <arpa/inet.h>
#define LAST_SOCKET_ERR() (errno)
#define SOCKET_ERR_STR(e) strerror(e)
#endif

// Custom logger
//#include <websocketpp/logger/syslog.hpp>
#include <websocketpp/server.hpp>

#define WS_DEBUG 0
// auto & cs = verbosestream; //errorstream; // remove after debug
#if WS_DEBUG
auto &cs = errorstream; // remove after debug
#else
auto &cs = tracestream; // remove after debug
#endif

void WSSocket::on_http(const websocketpp::connection_hdl &hdl)
{
	ws_transport::serveHttp(server, hdl);
}

void WSSocket::on_fail(const websocketpp::connection_hdl &hdl)
{
	ws_server_t::connection_ptr con = server.get_con_from_hdl(hdl);

	cs << "Fail handler: " << con->get_ec() << " " << con->get_ec().message() << '\n';
	//auto ec = websocketpp::get_transport_ec();
}

void WSSocket::on_close(const websocketpp::connection_hdl &hdl)
{
	cs << "Close handler" << '\n';

	// auto ws = hdls.at(hdl);

	// deletePeer(ws->peer_id, 0);
	// cancel pending service operations when the browser disconnects
	if (auto it = m_proxies.find(hdl); it != m_proxies.end()) {
		it->second->stop();
		m_proxies.erase(it);
	}
	// ===
	if (m_datagrams)
		m_datagrams->remove(hdl);
	hdls.erase(hdl);
}

void WSSocket::on_open(const websocketpp::connection_hdl &)
{
	cs << "open handler" << '\n';
}

// SPDX-License-Identifier: GPL-3.0-or-later
// Native client for emsocket's address assignment and encapsulated UDP protocol.
// Included by wssocket.cpp.

void WSSocket::on_client_open(const websocketpp::connection_hdl &hdl)
{
	websocketpp::lib::error_code ec;
	client.send(hdl, "NEWADDR", websocketpp::frame::opcode::text, ec);
	if (ec)
		errorstream << "WS address request failed: " << ec.message() << '\n';
}

void WSSocket::on_client_fail(const websocketpp::connection_hdl &hdl)
{
	auto con = client.get_con_from_hdl(hdl);
	errorstream << "WS connection failed: " << con->get_ec().message();
	if (const auto status = con->get_response_code(); status != 0)
		errorstream << " (HTTP " << status << " " << con->get_response_msg() << ")";
	errorstream << '\n';
	m_client_bound = false;
	m_client_pending.clear();
	m_client_pending_bytes = 0;
}

void WSSocket::on_client_close(const websocketpp::connection_hdl &hdl)
{
	auto con = client.get_con_from_hdl(hdl);
	errorstream << "WS connection closed: " << con->get_remote_close_code() << " "
				<< con->get_remote_close_reason() << '\n';
	m_client_bound = false;
	m_client_pending.clear();
	m_client_pending_bytes = 0;
}

void WSSocket::on_client_message(
		const websocketpp::connection_hdl &hdl, const client_message_ptr &msg)
{
	const auto &data = msg->get_payload();
	if (msg->get_opcode() == websocketpp::frame::opcode::text) {
		websocketpp::lib::error_code ec;
		if (!m_client_bound && !m_client_binding && data.starts_with("ADDR ")) {
			std::istringstream input(data);
			std::string command, address, passcode, joincode, extra;
			if (!(input >> command >> address >> passcode >> joincode) ||
					(input >> extra) || passcode.size() != 32) {
				errorstream << "WS server returned an invalid address assignment\n";
				return;
			}
			m_client_binding = true;
			client.send(hdl, "BIND " + passcode + " UDP 49152",
					websocketpp::frame::opcode::text, ec);
		} else if (m_client_binding && data == "BIND OK") {
			m_client_binding = false;
			m_client_bound = true;
			infostream << "WS datagram handshake completed\n";
			auto pending = std::move(m_client_pending);
			m_client_pending.clear();
			m_client_pending_bytes = 0;
			for (const auto &payload : pending)
				sendClientPacket(payload);
		} else {
			errorstream << "WS server rejected the datagram handshake\n";
			client.close(hdl, websocketpp::close::status::protocol_error,
					"Unexpected datagram handshake response", ec);
		}
		if (ec)
			errorstream << "WS datagram handshake failed: " << ec.message() << '\n';
		return;
	}
	ws_transport::Datagram packet;
	if (!m_client_bound || !packet.parse(data)) {
		errorstream << "WS server returned an invalid datagram\n";
		return;
	}
	{
		std::lock_guard lock(m_incoming_mutex);
		incoming_queue.push_back({client_address, std::move(packet.payload)});
	}
	m_incoming_ready.notify_one();
}

void WSSocket::sendClientPacket(const std::string &payload)
{
	if (!m_client_bound) {
		// The game can send before the asynchronous WebSocket handshake completes.
		if (m_client_pending_bytes + payload.size() <= 1024 * 1024) {
			m_client_pending.push_back(payload);
			m_client_pending_bytes += payload.size();
		}
		return;
	}
	// This listener's game route is also available through its synthetic address,
	// including when the public WebSocket address is translated by NAT.
	const ws_transport::tcp::endpoint destination(
			ws_transport::asio::ip::make_address("10.0.0.1"), client_address.getPort());
	const auto data = ws_transport::Datagram::encode(destination, payload);
	if (data.empty())
		return;
	websocketpp::lib::error_code ec;
	client.send(client_hdl, data, websocketpp::frame::opcode::binary, ec);
	if (ec)
		errorstream << "WS datagram send failed: " << ec.message() << '\n';
}

void WSSocket::on_message(const websocketpp::connection_hdl &hdl, const message_ptr &msg)
{
	// emsocket service connections must never enter the game packet queue
	if (auto it = m_proxies.find(hdl); it != m_proxies.end()) {
		it->second->receive(msg->get_payload());
		return;
	}
	const bool text = msg->get_opcode() == websocketpp::frame::opcode::text;
	if (m_datagrams && m_datagrams->receive(hdl, msg->get_payload(), text))
		return;
	if (text && msg->get_payload().starts_with("PROXY ")) {
		bool enabled = false;
		g_settings->getBoolNoEx("ws_proxy_enable", enabled);
		auto proxy = std::make_shared<proxy_t>(server, hdl, enabled);
		m_proxies.emplace(hdl, proxy);
		proxy->start(msg->get_payload());
		return;
	}
	websocketpp::lib::error_code ec;
	server.close(hdl, websocketpp::close::status::protocol_error,
			"Expected NEWADDR, BIND or PROXY", ec);
}

WSSocket::WSSocket(bool ipv6)
{
	init(ipv6, false);
}

bool WSSocket::init(bool ipv6, bool noExceptions)
{
	setTimeoutMs(0);

	ws_transport::initServer(server);

	server.set_open_handler(websocketpp::lib::bind(
			&WSSocket::on_open, this, websocketpp::lib::placeholders::_1));
	server.set_close_handler(websocketpp::lib::bind(
			&WSSocket::on_close, this, websocketpp::lib::placeholders::_1));
	server.set_message_handler(websocketpp::lib::bind(&WSSocket::on_message, this,
			websocketpp::lib::placeholders::_1, websocketpp::lib::placeholders::_2));
	// Server.set_message_handler(websocketpp::lib::bind(&Connection::on_message,
	// this, websocketpp::lib::placeholders::_1, websocketpp::lib::placeholders::_2));
	server.set_http_handler(websocketpp::lib::bind(
			&WSSocket::on_http, this, websocketpp::lib::placeholders::_1));
#if USE_SSL
	server.set_tls_init_handler(
			bind(&WSSocket::on_tls_init, this, websocketpp::lib::placeholders::_1));
#endif
	// Server.set_timer(long duration, timer_handler callback);
	//}

	return true;
}
#if USE_SSL
WSSocket::context_ptr WSSocket::on_tls_init(const websocketpp::connection_hdl &)
{
	return ws_transport::serverTlsContext();
}
WSSocket::context_ptr WSSocket::on_client_tls_init(const websocketpp::connection_hdl &)
{
	return ws_transport::clientTlsContext();
}
#endif

WSSocket::~WSSocket()
{
	if (m_server_thread.joinable()) {
		server.stop();
		m_server_thread.join();
	}
	if (m_client_thread.joinable()) {
		client.stop();
		m_client_thread.join();
	}
	// cancel cached game address resolution before destroying the endpoint
	if (m_game_router)
		m_game_router->stop();
	// cancel service callbacks before the endpoint is destroyed
	for (auto &[hdl, proxy] : m_proxies)
		proxy->stop();
}

bool WSSocket::Connect(const Address &addr)
{
	try {
		ws_transport::initEndpoint(client);

		// Set up client handlers
		client.set_open_handler(websocketpp::lib::bind(
				&WSSocket::on_client_open, this, websocketpp::lib::placeholders::_1));
		client.set_fail_handler(websocketpp::lib::bind(
				&WSSocket::on_client_fail, this, websocketpp::lib::placeholders::_1));
		client.set_close_handler(websocketpp::lib::bind(
				&WSSocket::on_client_close, this, websocketpp::lib::placeholders::_1));
		client.set_message_handler(websocketpp::lib::bind(&WSSocket::on_client_message,
				this, websocketpp::lib::placeholders::_1,
				websocketpp::lib::placeholders::_2));

#if USE_SSL
		client.set_tls_init_handler(bind(
				&WSSocket::on_client_tls_init, this, websocketpp::lib::placeholders::_1));
#endif

		const std::string uri = ws_transport::makeUri(addr, USE_SSL);

		// Create connection
		websocketpp::lib::error_code ec;
		auto con = client.get_connection(uri, ec);
		if (ec) {
			errorstream << "Could not create connection to " << uri << ": "
						<< ec.message() << std::endl;
			return false;
		}

		// Store the connection handle and address
		client_hdl = con->get_handle();
		client_address = addr;
		ws_client = true;

		// Connect
		client.connect(con);

		// Run the client io_service in a separate thread
		m_client_thread = std::thread([this]() {
			try {
				client.run();
			} catch (const std::exception &e) {
				errorstream << "Client run error: " << e.what() << std::endl;
			}
		});

		return true;
	} catch (const std::exception &e) {
		errorstream << "Connection failed: " << e.what() << std::endl;
		return false;
	}
}

void WSSocket::Bind(Address addr)
{
	// Outgoing WebSockets already have a local TCP socket; do not start a listener.
	if (ws_client)
		return;

	websocketpp::lib::error_code ec;
	server.listen(addr.getPort(), ec);
	if (ec) {
		errorstream << "WS listen fail: " << ec.message() << '\n';
		return;
	}
	// Start the server accept loop
	server.start_accept(ec);
	if (ec) {
		errorstream << "WS listen fail: " << ec.message() << '\n';
		return;
	}

	// fm: include the advertised addresses, which may differ behind IPv4 NAT
	std::string advertised_host;
	g_settings->getNoEx("server_address", advertised_host);
	m_game_router = std::make_shared<ws_transport::GameRouter>(server.get_io_context());
	m_game_router->resolve(advertised_host);
	m_datagrams = std::make_unique<ws_transport::DatagramService<ws_server_t>>(
			server, m_game_router, [this](auto hdl, const std::string &data) {
				// Keep real remote addresses in the game queue for bans and logs;
				// the private assignment is only an address on the virtual network.
				ws_transport::Error ec;
				const auto remote =
						server.get_con_from_hdl(hdl)->get_raw_socket().remote_endpoint(
								ec);
				if (ec)
					return;
				Address address =
						remote.address().is_v6()
								? Address(*reinterpret_cast<const sockaddr_in6 *>(
										  remote.data()))
								: Address(*reinterpret_cast<const sockaddr_in *>(
										  remote.data()));
				hdls.emplace(hdl, address);
				{
					std::lock_guard lock(m_incoming_mutex);
					incoming_queue.push_back({address, data});
				}
				m_incoming_ready.notify_one();
			});
	// ===
	ws_serve = true;
	m_server_thread = std::thread([this] {
		try {
			server.run();
		} catch (const std::exception &e) {
			errorstream << "WS server run error: " << e.what() << std::endl;
		}
	});
}

void WSSocket::Send(const Address &destination, const void *data, int size)
{
	bool dumping_packet = false; // for INTERNET_SIMULATOR

	if (INTERNET_SIMULATOR)
		dumping_packet = myrand() % INTERNET_SIMULATOR_PACKET_LOSS == 0;

	if (dumping_packet) {
		// Lol let's forget it
		tracestream << "WSSocket::Send(): INTERNET_SIMULATOR: dumping packet." << '\n';
		return;
	}

	if (size < 0 ||
			static_cast<size_t>(size) > ws_transport::Datagram::MAX_PAYLOAD_SIZE) {
		errorstream << "WS datagram exceeds envelope limit: " << size << '\n';
		return;
	}
	std::string payload(static_cast<const char *>(data), size);
	if (ws_client) {
		if (client_address != destination)
			return;
		boost::asio::post(client.get_io_context(),
				[this, payload = std::move(payload)]() { sendClientPacket(payload); });
	} else if (ws_serve) {
		// Keep the posted queue bounded too: wait for the event loop to admit
		// each datagram before posting another. The event loop remains free to
		// drain the WebSocket and process disconnects while we wait.
		const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(5);
		for (;;) {
			auto result = std::make_shared<std::promise<bool>>();
			auto ready = result->get_future();
			boost::asio::post(
					server.get_io_context(), [this, destination, payload, result]() {
						bool accepted = true;
						if (m_datagrams) {
							for (const auto &[hdl, address] : hdls) {
								if (address == destination) {
									accepted = m_datagrams->send_game(hdl, payload);
									break;
								}
							}
						}
						result->set_value(accepted);
					});
			if (ready.wait_until(deadline) != std::future_status::ready) {
				warningstream
						<< "WS send event loop stalled; reliable packet will be retried\n";
				return;
			}
			if (ready.get())
				return;
			if (std::chrono::steady_clock::now() >= deadline) {
				warningstream
						<< "WS send buffer remained full; reliable packet will be retried\n";
				return;
			}
			std::this_thread::sleep_for(std::chrono::milliseconds(1));
		}
	}
}

int WSSocket::Receive(Address &sender, void *data, int size)
{
	//  Return on timeout
	if (!WaitData(m_timeout_ms))
		return -1;

	queue_item item;
	{
		std::lock_guard lock(m_incoming_mutex);
		if (incoming_queue.empty())
			return -1;
		item = std::move(incoming_queue.front());
		incoming_queue.pop_front();
	}
	int received = item.data.size();
	if (size < received) {
		tracestream << "Packet size " << size << " larger than buffer " << received
					<< '\n';
		return -1;
	}
	sender = item.address;

	memcpy(data, item.data.data(), item.data.size());

	return received;
}

int WSSocket::GetHandle()
{
	return {};
}

void WSSocket::setTimeoutMs(int timeout_ms)
{
	m_timeout_ms = timeout_ms;
}

bool WSSocket::WaitData(int timeout_ms)
{
	std::unique_lock lock(m_incoming_mutex);
	const auto ready = [this] { return !incoming_queue.empty(); };
	if (timeout_ms < 0) {
		m_incoming_ready.wait(lock, ready);
		return true;
	}
	return m_incoming_ready.wait_for(lock, std::chrono::milliseconds(timeout_ms), ready);
}

#endif
