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

#include "config.h"
#if USE_WEBSOCKET || USE_WEBSOCKET_SCTP

#include "wssocket.h"
#include "constants.h"
#include "iostream_debug_helpers.h"
#include "log.h"
#include "util/numeric.h"
#include "util/string.h"

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

#include <websocketpp/config/debug_asio_no_tls.hpp>

// Custom logger
#include <websocketpp/logger/syslog.hpp>
#include <websocketpp/server.hpp>

#define WS_DEBUG 0
// auto & cs = verbosestream; //errorstream; // remove after debug
#if WS_DEBUG
auto &cs = errorstream; // remove after debug
#else
auto &cs = tracestream; // remove after debug
#endif

////////////////////////////////////////////////////////////////////////////////
///////////////// Custom Config for debugging custom policies //////////////////
////////////////////////////////////////////////////////////////////////////////

struct debug_custom : public websocketpp::config::debug_asio
{
	typedef debug_custom type;
	typedef debug_asio base;

	typedef base::concurrency_type concurrency_type;

	typedef base::request_type request_type;
	typedef base::response_type response_type;

	typedef base::message_type message_type;
	typedef base::con_msg_manager_type con_msg_manager_type;
	typedef base::endpoint_msg_manager_type endpoint_msg_manager_type;

	/// Custom Logging policies
	/*typedef websocketpp::log::syslog<concurrency_type,
		websocketpp::log::elevel> elog_type;
	typedef websocketpp::log::syslog<concurrency_type,
		websocketpp::log::alevel> alog_type;
	*/
	typedef base::alog_type alog_type;
	typedef base::elog_type elog_type;

	typedef base::rng_type rng_type;

	struct transport_config : public base::transport_config
	{
		typedef type::concurrency_type concurrency_type;
		typedef type::alog_type alog_type;
		typedef type::elog_type elog_type;
		typedef type::request_type request_type;
		typedef type::response_type response_type;
		typedef websocketpp::transport::asio::basic_socket::endpoint socket_type;
	};

	typedef websocketpp::transport::asio::endpoint<transport_config> transport_type;

	static const long timeout_open_handshake = 0;
};

////////////////////////////////////////////////////////////////////////////////

typedef websocketpp::server<debug_custom> server;

void WSSocket::on_http(const websocketpp::connection_hdl &hdl)
{
	ws_server_t::connection_ptr con = server.get_con_from_hdl(hdl);

	std::string res = con->get_request_body();

	std::stringstream ss;
	ss << "got HTTP request with " << res.size() << " bytes of body data.";
	cs << ss.str() << std::endl;

	con->set_body(ss.str());
	con->set_status(websocketpp::http::status_code::ok);
}

void WSSocket::on_fail(const websocketpp::connection_hdl &hdl)
{
	ws_server_t::connection_ptr con = server.get_con_from_hdl(hdl);

	cs << "Fail handler: " << con->get_ec() << " " << con->get_ec().message()
	   << std::endl;
	   //auto ec = websocketpp::get_transport_ec();
}

void WSSocket::on_close(const websocketpp::connection_hdl &hdl)
{
	cs << "Close handler" << std::endl;

	// auto ws = hdls.at(hdl);

	// deletePeer(ws->peer_id, 0);
	hdls.erase(hdl);
}

void WSSocket::on_open(const websocketpp::connection_hdl &)
{
	cs << "open handler" << std::endl;
}

void WSSocket::on_message(const websocketpp::connection_hdl &hdl, const message_ptr &msg)
{
	// DUMP("om", msg->get_payload().size(), msg->get_payload());

	// cs << "on_message called with hdl: " << hdl.lock().get() << " and message: " <<
	// msg->get_payload().size() << " " << msg->get_payload() << std::endl;

	ws_server_t::connection_ptr con = server.get_con_from_hdl(hdl);
	Address a;
	const auto re = con->get_remote_endpoint();
	const auto pos = re.rfind(':');
	// cut ipv6 braces []
	// TODO cache resolve! :
	a.Resolve(re.substr(re[0] == '[' ? 1 : 0, pos - 1 - (re[pos - 1] == ']' ? 1 : 0))
					  .c_str());
	a.setPort(from_string<uint16_t>(re.substr(pos + 1, re.size())));

	if (!hdls.count(hdl)) {
		// DUMP("first", a, msg->get_payload().size(), con->get_host(),
		// con->get_request().get_headers(), con->get_proxy(),
		// con->get_remote_endpoint());

		cs << "first message from " << a << " : " << msg->get_payload().size() << " "
		   << msg->get_payload() << std::endl;

		hdls.emplace(hdl, a);

		websocketpp::lib::error_code ec;
		server.send(hdl, "PROXY OK", msg->get_opcode(), ec);
		if (ec) {
			cs << "Echo failed because: "
			   << "(" << ec.value() << ":" << ec.message() << ")" << std::endl;
		}
		return;
	}

	std::string s{msg->get_payload().data(), msg->get_payload().size()};
	cs << "A message: " << msg->get_payload().size() << " " << msg->get_payload()
	   << std::endl;

	incoming_queue.emplace_back(queue_item{a, std::move(s)});
}

WSSocket::WSSocket(bool ipv6)
{
	init(ipv6, false);
}

bool WSSocket::init(bool ipv6, bool noExceptions)
{

	if (socket_enable_debug_output /*con_debug*/) {

		server.set_error_channels(websocketpp::log::elevel::all);
		server.set_access_channels(websocketpp::log::alevel::all ^
								   websocketpp::log::alevel::frame_payload ^
								   websocketpp::log::alevel::frame_header);

	} else {
		server.set_error_channels(websocketpp::log::elevel::none);
		server.set_access_channels(websocketpp::log::alevel::none);
	}
	const auto timeouts = 30; // Config.GetWsTimeoutsMs();
	server.set_open_handshake_timeout(timeouts);
	server.set_close_handshake_timeout(timeouts);
	server.set_pong_timeout(timeouts);
	server.set_listen_backlog(100);
	server.init_asio();

	server.set_reuse_addr(true);
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
WSSocket::context_ptr WSSocket::on_tls_init(const websocketpp::connection_hdl & /* hdl */)
{
	namespace asio = websocketpp::lib::asio;
	context_ptr ctx = websocketpp::lib::make_shared<asio::ssl::context>(
			// asio::ssl::context::tlsv13
			asio::ssl::context::tlsv13_server);
	try {
		ctx->set_options(asio::ssl::context::default_workarounds |
						 asio::ssl::context::no_sslv2 | asio::ssl::context::no_sslv3 |
						 asio::ssl::context::single_dh_use);
		ctx->set_password_callback(std::bind([&]() {
			return ""; /*GetSSLPassword();*/
		}));
		ctx->use_certificate_chain_file("fullchain.pem" /*GetSSLCertificateChain()*/);
		ctx->use_private_key_file(
				"privkey.pem" /*GetSSLPrivateKey()*/, asio::ssl::context::pem);
		std::string ciphers; // = GetSSLCiphers();
		if (ciphers.empty())
			ciphers = "ECDHE-RSA-AES128-GCM-SHA256:ECDHE-ECDSA-AES128-GCM-SHA256:"
					  "ECDHE-RSA-AES256-GCM-SHA384:ECDHE-ECDSA-AES256-GCM-SHA384:"
					  "DHE-RSA-AES128-GCM-SHA256:DHE-DSS-AES128-GCM-SHA256:kEDH+"
					  "AESGCM:ECDHE-RSA-AES128-SHA256:ECDHE-ECDSA-AES128-SHA256:"
					  "ECDHE-RSA-AES128-SHA:ECDHE-ECDSA-AES128-SHA:ECDHE-RSA-"
					  "AES256-SHA384:ECDHE-ECDSA-AES256-SHA384:ECDHE-RSA-AES256-"
					  "SHA:ECDHE-ECDSA-AES256-SHA:DHE-RSA-AES128-SHA256:DHE-RSA-"
					  "AES128-SHA:DHE-DSS-AES128-SHA256:DHE-RSA-AES256-SHA256:"
					  "DHE-DSS-AES256-SHA:DHE-RSA-AES256-SHA:!aNULL:!eNULL:!"
					  "EXPORT:!DES:!RC4:!3DES:!MD5:!PSK";

		if (SSL_CTX_set_cipher_list(ctx->native_handle(), ciphers.c_str()) != 1) {
			warningstream << "Error setting cipher list";
		}
	} catch (const std::exception &e) {
		errorstream << "Exception: " << e.what();
	}
	return ctx;
}
#endif

WSSocket::~WSSocket()
{
	if (socket_enable_debug_output) {
		tracestream << "WSSocket( "
					//<< (int)m_handle
					<< ")::~WSSocket()" << std::endl;
	}
}

void WSSocket::Bind(Address addr)
{
	if (socket_enable_debug_output) {
		tracestream << "WSSocket(" //<< (int)m_handle
					<< ")::Bind(): " << addr.serializeString() << ":" << addr.getPort()
					<< std::endl;
	}
	websocketpp::lib::error_code ec;
	server.listen(addr.getPort(), ec);
	if (ec) {
		errorstream << "WS listen fail: " << ec.message() << std::endl;
		return;
	}
	// Start the server accept loop
	server.start_accept(ec);
	if (ec) {
		errorstream << "WS listen fail: " << ec.message() << std::endl;
		return;
	}

	ws_serve = true;
}

void WSSocket::Send(const Address &destination, const void *data, int size)
{
	bool dumping_packet = false; // for INTERNET_SIMULATOR

	if (INTERNET_SIMULATOR)
		dumping_packet = myrand() % INTERNET_SIMULATOR_PACKET_LOSS == 0;

	if (socket_enable_debug_output) {
		// Print packet destination and size
		// tracestream << (int)m_handle << " -> ";
		destination.print(tracestream);
		tracestream << ", size=" << size;

		// Print packet contents
		tracestream << ", data=";
		for (int i = 0; i < size && i < 20; i++) {
			if (i % 2 == 0)
				tracestream << " ";
			unsigned int a = ((const unsigned char *)data)[i];
			tracestream << std::hex << std::setw(2) << std::setfill('0') << a;
		}

		if (size > 20)
			tracestream << "...";

		if (dumping_packet)
			tracestream << " (DUMPED BY INTERNET_SIMULATOR)";

		tracestream << std::endl;
	}

	if (dumping_packet) {
		// Lol let's forget it
		tracestream << "WSSocket::Send(): INTERNET_SIMULATOR: dumping packet."
					<< std::endl;
		return;
	}

	websocketpp::connection_hdl hdl;
	bool found = false;
	for (const auto &[h, addr] : hdls) {
		if (addr == destination) {
			hdl = h;
			found = true;
		}
	}
	if (!found) {
		verbosestream << " Send to " << destination << " not found in peers" << std::endl;
		return;
	}
	websocketpp::lib::error_code ec;
	server.send(hdl, data, size, websocketpp::frame::opcode::value::binary, ec);
	if (ec.value()) {
		verbosestream << "WS Send failed " << ec.value() << ":" << ec.message()
					  << std::endl;
		// Maybe delete peer here?
	}
	// Server.run_one();
}

int WSSocket::Receive(Address &sender, void *data, int size)
{
	//  Return on timeout
	if (!WaitData(m_timeout_ms))
		return -1;

	const auto item = incoming_queue.front();
	incoming_queue.pop_front();
	int received = item.data.size();
	if (size < received) {
		tracestream << "Packet size " << size << " larger than buffer " << received
					<< std::endl;
		return -1;
	}
	sender = item.address;

	memcpy(data, item.data.data(), item.data.size());

	if (socket_enable_debug_output) {
		// Print packet sender and size
		// tracestream << (int)m_handle << " <- ";
		sender.print(tracestream);
		tracestream << ", size=" << received;

		// Print packet contents
		tracestream << ", data=";
		for (int i = 0; i < received && i < 20; i++) {
			if (i % 2 == 0)
				tracestream << " ";
			unsigned int a = ((const unsigned char *)data)[i];
			tracestream << std::hex << std::setw(2) << std::setfill('0') << a;
		}
		if (received > 20)
			tracestream << "...";

		tracestream << std::endl;
	}

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
	if (!ws_serve)
		return false;
	for (int ms = 0; ms < timeout_ms; ++ms) {
		if (server.poll_one())
			server.run_one();
		if (!incoming_queue.empty())
			return true;
		// TODO: condvar here
		std::this_thread::sleep_for(std::chrono::milliseconds(1));
	}
	return !incoming_queue.empty();
}

#endif
