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
#include <memory>
#include <string>
#include <sys/socket.h>
#include "debug/iostream_debug_helpers.h"

#include "config.h"

#if USE_WEBSOCKET_SCTP

#include "fm_connection_websocket_sctp.h"

#include "network/fm_connection_sctp.h"
#include "external/usrsctp/usrsctplib/usrsctp.h"

#include "log.h"
#include "network/networkpacket.h"
#include "network/networkprotocol.h"
#include "porting.h"
#include "profiler.h"
#include "serialization.h"
#include "settings.h"
#include "util/numeric.h"
#include "util/serialize.h"
#include "util/string.h"
#include <cstdarg>

#include <websocketpp/server.hpp>

#include <websocketpp/config/debug_asio_no_tls.hpp>
// #include <websocketpp/config/asio_no_tls.hpp>
// #include "websocketpp/common/connection_hdl.hpp"

// Custom logger
#include <websocketpp/logger/syslog.hpp>

#include <websocketpp/server.hpp>

// #include <iostream>

namespace con_ws_sctp
{

#define WS_DEBUG 1
// auto & cs = verbosestream; //errorstream; // remove after debug
#if WS_DEBUG
auto &cs = errorstream; // remove after debug
#else
auto &cs = verbosestream; // remove after debug
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

void Connection::on_http(websocketpp::connection_hdl hdl)
{
	server::connection_ptr con = Server.get_con_from_hdl(hdl);

	std::string res = con->get_request_body();

	std::stringstream ss;
	ss << "got HTTP request with " << res.size() << " bytes of body data.";
	cs << ss.str() << std::endl;

	con->set_body(ss.str());
	con->set_status(websocketpp::http::status_code::ok);
}

void Connection::on_fail(websocketpp::connection_hdl hdl)
{
	server::connection_ptr con = Server.get_con_from_hdl(hdl);

	cs << "Fail handler: " << con->get_ec() << " " << con->get_ec().message()
	   << std::endl;
}

void Connection::on_close(websocketpp::connection_hdl hdl)
{
	cs << "Close handler" << std::endl;

	auto ws = hdls.at(hdl);

	deletePeer(ws->peer_id, 0);
	hdls.erase(hdl);
}

void Connection::on_open(websocketpp::connection_hdl)
{
	cs << "open handler" << std::endl;
}

#define EMC_VERIFY(x) // TODO: Replace this.
#define SAFE_FREE(p)                                                                     \
	{                                                                                    \
		if (p) {                                                                         \
			free(p);                                                                     \
			(p) = nullptr;                                                               \
		}                                                                                \
	}                                                                                    \
	((void)0) // There must be ";".

std::string string_sprintf(const char *format, ...)
{
	va_list args;
	va_start(args, format);
	// Referenced from
	// https://stackoverflow.com/questions/436367/best-way-to-safely-printf-to-a-string
	size_t sz = vsnprintf(nullptr, 0, format, args);
	size_t bufsize = sz + 1;
	char *buf = (char *)malloc(bufsize);
	EMC_VERIFY(buf != nullptr);
	vsnprintf(buf, bufsize, format, args);
	// buf[bufsize - 1] = '\0'; // This line is not necessary, check the official
	// documentation of vsnprintf for proof.
	va_end(args);
	std::string str = buf;
	SAFE_FREE(buf);
	return str;
}
static int receive_cb(struct socket *sock, union sctp_sockstore addr, void *data,
		size_t datalen, struct sctp_rcvinfo rcv, int flags, void *ulp_info)
{
	DUMP("rcb", (long)data, datalen);
	if (data) {
		if (flags & MSG_NOTIFICATION) {
			DUMP(string_sprintf("Notification of length %d received.\n", (int)datalen));
		} else {
			DUMP(string_sprintf("Msg of length %d received via %p:%u on stream %u with "
								"SSN %u and TSN "
								"%u, PPID %u, context %u.\n",
					(int)datalen, addr.sconn.sconn_addr, ntohs(addr.sconn.sconn_port),
					rcv.rcv_sid, rcv.rcv_ssn, rcv.rcv_tsn, (uint32_t)ntohl(rcv.rcv_ppid),
					rcv.rcv_context));
		}
		free(data);
	} else {
		usrsctp_deregister_address(ulp_info);
		usrsctp_close(sock);
	}
	return (1);
}

static int send_cb(struct socket *sock, uint32_t sb_free, void *ulp_info)
{
	DUMP((long)sock, sb_free);

	return 1;
}

// Define a callback to handle incoming messages
void Connection::on_message(websocketpp::connection_hdl hdl, message_ptr msg)
{

	cs << "on_message called with hdl: " << hdl.lock().get()
	   << " and message: " << msg->get_payload().size() << " " << msg->get_payload()
	   << std::endl;

	if (!hdls.count(hdl)) {

		{

			u16 peer_id = 0;
			static u16 last_try = PEER_ID_SERVER + 1;
			if (m_peers.size() > 0) {
				for (int i = 0; i < 1000; ++i) {
					if (last_try > 30000)
						last_try = PEER_ID_SERVER + 20000;
					++last_try;
					if (!m_peers.count(last_try)) {
						peer_id = last_try;
						break;
					}
				}
			} else {
				peer_id = last_try;
			}
			if (!peer_id)
				last_try = peer_id = m_peers.rbegin()->first + 1;

			int fd[2] = {};
			if (socketpair(AF_LOCAL,
						// AF_UNIX,
						//  SOCK_STREAM,
						SOCK_DGRAM, 0, fd) == -1) {
				DUMP("socketpair fail", fd);
				return;
			}
			auto ulp_info = std::make_unique<ulp_info_holder>(ulp_info_holder{this, hdl});
			// DUMP((long)ulp_info.get());

			usrsctp_register_address(ulp_info.get());

			struct socket *sock = nullptr;
			if ((sock = usrsctp_socket(AF_CONN, SOCK_STREAM, IPPROTO_SCTP, receive_cb,
						 send_cb, 0, ulp_info.get())) == NULL) {
				DUMP(peer_id, (long)sock, fd);
				perror("usrsctp_socket");
				return;
			}

			struct sockaddr_conn sconn
			{
			};

			sconn.sconn_family = AF_CONN;
#ifdef HAVE_SCONN_LEN
			sconn.sconn_len = sizeof(struct sockaddr_conn);
#endif

			// sconn.sconn_port = htons(PORT);
			sconn.sconn_addr = (void *)&fd;
			if (usrsctp_bind(sock, (struct sockaddr *)&sconn, sizeof(sconn)) < 0) {
				perror("usrsctp_bind");
			}
			if (usrsctp_listen(sock, 1) < 0) {
				perror("usrsctp_listen");
			}

			DUMP(peer_id, (long)sock, fd);
			m_peers.insert_or_assign(peer_id, sock);

			hdls.emplace(hdl, std::make_shared<ws_peer>(ws_peer{.peer_id = peer_id,
									  .fd{fd[0], fd[1]},
									  .sock = sock,
									  .ulp_info{std::move(ulp_info)}}));
			putEvent(ConnectionEvent::peerAdded(peer_id, {}));
		}

		try {
			Server.send(hdl, "PROXY OK", msg->get_opcode());

		} catch (const websocketpp::exception &e) {
			cs << "Echo failed because: "
			   << "(" << e.what() << ")" << std::endl;
		}
	} else

	//	if (hdls.count(hdl))
	{
		const auto &wsp = hdls.at(hdl);

		// auto sr = ::send(				wsp->fd[0], msg->get_payload().data(),
		// msg->get_payload().size(), 0);
		// auto sr = write(wsp->fd[0], msg->get_payload().data(),
		// msg->get_payload().size());
		DUMP("usrsctp_conninput:", wsp->fd[1]);
		//		usrsctp_conninput((void*)wsp->fd[1], (void*)msg->get_payload().data(),
		// msg->get_payload().size(), 0); 		usrsctp_conninput((void*)ulp_info.get(),
		//(void*)msg->get_payload().data(), msg->get_payload().size(), 0);
		usrsctp_conninput((void *)wsp->ulp_info.get(), (void *)msg->get_payload().data(),
				msg->get_payload().size(), 0);

		// DUMP(sr);
		//  const auto reret = con_sctp::Connection::recv(wsp->peer_id, wsp->sock);
		//  DUMP(reret);
		return;
		// for (int header_size = 0; header_size <
		// msg->get_payload().size();++header_size)
		if (0) {
			constexpr size_t header_size = 8;
			DUMP("try hdsz=", header_size);
			SharedBuffer<u8> resultdata(
					(const unsigned char *)msg->get_payload().data() + header_size,
					msg->get_payload().size() - header_size);
			// putEvent(ConnectionEvent::dataReceived(hdls[hdl].peer_id, resultdata));
			putEvent(ConnectionEvent::dataReceived(hdls.at(hdl)->peer_id, resultdata));
		}
		return;
	}
}

/*
	Connection
*/
void debug_printf(const char *format, ...)
{
	printf("WS_DEBUG: ");
	va_list ap;
	va_start(ap, format);
	vprintf(format, ap);
	va_end(ap);
}

// int Connection::
static int sctp_send_cb(struct socket *sock, uint32_t sb_free, void *ulp_info)
{
	DUMP((long)sock, sb_free, (long)ulp_info);
	return 0;
}

static int conn_output(void *addr, void *buf, size_t length, uint8_t tos, uint8_t set_df)
{
	DUMP((long)addr, (long)buf, length, tos, set_df);

	Connection::ulp_info_holder *ulp_info = (Connection::ulp_info_holder *)addr;

	//
	ulp_info->self->Server.send(
			ulp_info->hdl, buf, length, websocketpp::frame::opcode::value::binary);
	return 0;
}

Connection::Connection(u32 protocol_id, u32 max_packet_size, float timeout, bool ipv6,
		PeerHandler *peerhandler) :
		con_sctp::Connection(protocol_id, max_packet_size, timeout, peerhandler)
{

	domain = AF_CONN;
	// send_cb = Connection::sctp_send_cb;
	server_send_cb = client_send_cb = sctp_send_cb;
	DUMP((long)server_send_cb);
	sctp_conn_output = conn_output;
	DUMP((long)sctp_conn_output);
	sctp_setup(0);
	usrsctp_sysctl_set_sctp_asconf_enable(0);

	{

		if (0 /*con_debug*/) {

			Server.set_error_channels(websocketpp::log::elevel::all);
			Server.set_access_channels(websocketpp::log::alevel::all ^
									   websocketpp::log::alevel::frame_payload ^
									   websocketpp::log::alevel::frame_header);

		} else {
			Server.set_error_channels(websocketpp::log::elevel::none);
			Server.set_access_channels(websocketpp::log::alevel::none);
		}
		const auto timeouts = 30; // Config.GetWsTimeoutsMs();
		Server.set_open_handshake_timeout(timeouts);
		Server.set_close_handshake_timeout(timeouts);
		Server.set_pong_timeout(timeouts);
		Server.set_listen_backlog(100);
		Server.init_asio();

		Server.set_reuse_addr(true);
		Server.set_open_handler(websocketpp::lib::bind(
				&Connection::on_open, this, websocketpp::lib::placeholders::_1));
		Server.set_close_handler(websocketpp::lib::bind(
				&Connection::on_close, this, websocketpp::lib::placeholders::_1));
		Server.set_message_handler(websocketpp::lib::bind(&Connection::on_message, this,
				websocketpp::lib::placeholders::_1, websocketpp::lib::placeholders::_2));
		// Server.set_message_handler(websocketpp::lib::bind(&Connection::on_message,
		// this, websocketpp::lib::placeholders::_1, websocketpp::lib::placeholders::_2));
		Server.set_http_handler(websocketpp::lib::bind(
				&Connection::on_http, this, websocketpp::lib::placeholders::_1));
#if USE_SSL
		Server.set_tls_init_handler(bind(&broadcast_server::on_tls_init, this,
				websocketpp::lib::placeholders::_1));
#endif
		// Server.set_timer(long duration, timer_handler callback);
		//}

#if USE_SSL
		context_ptr on_tls_init(websocketpp::connection_hdl /* hdl */)
		{
			namespace asio = websocketpp::lib::asio;
			context_ptr ctx = websocketpp::lib::make_shared<asio::ssl::context>(
					asio::ssl::context::tlsv12);
			try {
				ctx->set_options(asio::ssl::context::default_workarounds |
								 asio::ssl::context::no_sslv2 |
								 asio::ssl::context::no_sslv3 |
								 asio::ssl::context::single_dh_use);
				ctx->set_password_callback(std::bind([&]() { return GetSSLPassword(); }));
				ctx->use_certificate_chain_file(GetSSLCertificateChain());
				ctx->use_private_key_file(GetSSLPrivateKey(), asio::ssl::context::pem);
				std::string ciphers = GetSSLCiphers();
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
	}

	// start();
}

// bool Connection::sctp_inited = false;

Connection::~Connection()
{
}

/* Internal stuff */

int Connection::receive()
{
	if (ws_serve)
		Server.run_one();
	return con_sctp::Connection::receive();
}

// host
void Connection::serve(Address bind_address)
{
	infostream << getDesc() << "WS serving at " << bind_address.serializeString() << ":"
			   << std::to_string(bind_address.getPort()) << std::endl;
	// Server.listen(bind_address.getAddress6());
	//  Listen on port 9012
	Server.listen(bind_address.getPort());

	// Start the server accept loop
	Server.start_accept();

	// Start the ASIO io_service run loop
	// Server.run();
	ws_serve = true;
	Server.run_one();
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