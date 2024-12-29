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
#if USE_WEBSOCKET || USE_WEBSOCKET_SCTP

#include "wssocket.h"
#include "constants.h"
#include "filesys.h"
#include "log.h"
#include "../server/serverlist.h"
#include "settings.h"
#include "util/numeric.h"
#include "util/string.h"
#include <string>

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
	ws_server_t::connection_ptr con = server.get_con_from_hdl(hdl);

	std::string res = con->get_request_body();

	std::stringstream ss;
	ss << "got HTTP request with " << res.size() << " bytes of body data.";
	cs << ss.str() << '\n';
	std::string http_root = porting::path_share + DIR_DELIM + "http_root" + DIR_DELIM;
	g_settings->getNoEx("http_root", http_root);

	if (con->get_request().get_method() == "GET") {

		if (con->get_request().get_uri().find("..") != std::string::npos) {
			con->set_status(websocketpp::http::status_code::bad_request);
			return;
		}

		std::string path_serve;

		auto uri = con->get_request().get_uri();
		if (const auto f = uri.find('?'); f != std::string::npos) {
			uri.resize(f);
		}
		if (uri == "/") {
			uri = "index.html";
		}

		if (uri == "/favicon.ico") {
			path_serve = porting::path_share + DIR_DELIM + "misc" + DIR_DELIM +
						 PROJECT_NAME + ".ico";
			con->append_header("Content-Type", "image/x-icon");
		} else if (!uri.empty()) {
			path_serve = http_root + uri;
			if (uri.ends_with(".wasm")) {
				con->append_header("Content-Type", "application/wasm");
			} else if (uri.ends_with(".js")) {
				con->append_header("Content-Type", "application/javascript");
			}
		}

		std::string body;
		if (uri == "/status.json") {
			path_serve = {};
			body = ServerList::last_status;
			con->append_header("Content-Type", "application/json; charset=utf-8");
		}

		bool defered = false;
		if (!path_serve.empty() && body.empty()) {
			con->append_header("Access-Control-Allow-Origin", "*");
			con->append_header("Cross-Origin-Embedder-Policy", "require-corp");
			con->append_header("Cross-Origin-Opener-Policy", "same-origin");
			con->defer_http_response();
			defered = true;
			std::ifstream t(path_serve);
			std::stringstream buffer;
			buffer << t.rdbuf();
			body = buffer.str();
		}

		actionstream << con->get_request().get_version() << " "
					 << con->get_request().get_method() << " "
					 << con->get_request().get_uri()
					 //<< " " << res.size()
					 << " " << body.size() << "\n";

		if (!body.empty()) {
			con->set_body(std::move(body));
			con->set_status(websocketpp::http::status_code::ok);
			websocketpp::lib::error_code ec;
			if (defered) {
				con->send_http_response(ec);
				if (ec.value())
					errorstream << "http send error:" << ec.category().name() << " "
								<< ec.value() << " " << ec.message() << "\n";
			}
			// TODO: serve log here?
			return;
		}
	}
	// DUMP(con->get_request().get_method(), con->get_request().get_version(), con->get_request().get_uri(), con->get_request().get_headers(), res);

	con->set_status(websocketpp::http::status_code::not_found);
	con->set_body(ss.str());
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
	hdls.erase(hdl);
}

void WSSocket::on_open(const websocketpp::connection_hdl &)
{
	cs << "open handler" << '\n';
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
		   << msg->get_payload() << '\n';

		hdls.emplace(hdl, a);

		websocketpp::lib::error_code ec;
		server.send(hdl, "PROXY OK", msg->get_opcode(), ec);
		if (ec) {
			cs << "Echo failed because: " << "(" << ec.value() << ":" << ec.message()
			   << ")" << '\n';
		}
		return;
	}

	std::string s{msg->get_payload().data(), msg->get_payload().size()};
#if !NDEBUG
	cs << "A message: " << msg->get_payload().size() << " " << msg->get_payload() << '\n';
#endif

	incoming_queue.emplace_back(queue_item{a, std::move(s)});
}

WSSocket::WSSocket(bool ipv6)
{
	init(ipv6, false);
}

bool WSSocket::init(bool ipv6, bool noExceptions)
{
	setTimeoutMs(0);

/*	if (socket_enable_debug_output /*con_debug* /) {
		server.set_error_channels(websocketpp::log::elevel::all);
		server.set_access_channels(websocketpp::log::alevel::all ^
								   websocketpp::log::alevel::frame_payload ^
								   websocketpp::log::alevel::frame_header);

	} else 
	*/
	{
		server.set_error_channels(websocketpp::log::elevel::none);
		server.set_access_channels(websocketpp::log::alevel::none);
	}
	const auto timeouts = 30; // Config.GetWsTimeoutsMs();
	server.set_open_handshake_timeout(timeouts);
	server.set_close_handshake_timeout(timeouts);
	server.set_pong_timeout(timeouts);
	server.set_listen_backlog(100);
	server.set_max_http_body_size(500000000);
	server.set_max_message_size(500000000);

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

		std::string chain = "fullchain.pem";
		std::string key = "privkey.pem";
		g_settings->getNoEx("https_chain", chain);
		g_settings->getNoEx("https_key", key);
		ctx->use_certificate_chain_file(chain /*GetSSLCertificateChain()*/);
		ctx->use_private_key_file(key /*GetSSLPrivateKey()*/, asio::ssl::context::pem);
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
}

void WSSocket::Bind(Address addr)
{

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

	ws_serve = true;
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

	websocketpp::connection_hdl hdl;
	bool found = false;
	for (const auto &[h, addr] : hdls) {
		if (addr == destination) {
			hdl = h;
			found = true;
		}
	}
	if (!found) {
		verbosestream << " Send to " << destination << " not found in peers" << '\n';
		return;
	}
	websocketpp::lib::error_code ec;
	server.send(hdl, data, size, websocketpp::frame::opcode::value::binary, ec);
	if (ec.value()) {
		verbosestream << "WS Send failed " << ec.value() << ":" << ec.message() << '\n';
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
	if (!ws_serve) {
		std::this_thread::sleep_for(std::chrono::milliseconds(m_timeout_ms));
		return false;
	}
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
