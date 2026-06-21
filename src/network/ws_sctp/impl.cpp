/*
Copyright (C) 2023 proller <proler@gmail.com>
*/

#include "network/ws_sctp/internal.h"

#if USE_WEBSOCKET_SCTP

namespace con_ws_sctp
{

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

static constexpr long WS_SCTP_TIMEOUT_MS = 5000;

static std::string makeWebSocketUri(const std::string &scheme, const Address &address)
{
	std::string host = address.serializeString();
	if (address.isIPv6()) {
		std::string escaped_host;
		escaped_host.reserve(host.size());
		for (char c : host) {
			if (c == '%')
				escaped_host += "%25";
			else
				escaped_host += c;
		}
		host = "[" + escaped_host + "]";
	}
	return scheme + host + ":" + std::to_string(address.getPort());
}

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
	if (!ulp_info || !ulp_info->active.load(std::memory_order_acquire))
		return EINVAL;
	Connection *self = ulp_info->self.load(std::memory_order_acquire);
	if (!self)
		return EINVAL;

	websocketpp::lib::error_code ec;
	if (ulp_info->client) {
		self->Client.send(ulp_info->hdl, buf, length,
				websocketpp::frame::opcode::value::binary, ec);
	} else {
		self->Server.send(ulp_info->hdl, buf, length,
				websocketpp::frame::opcode::value::binary, ec);
	}
	return ec ? ec.value() : 0;
}

Connection::Connection(u32 protocol_id, u32 max_packet_size, float timeout, bool ipv6,
		PeerHandler *peerhandler) :
		con_sctp::Connection(max_packet_size, timeout, ipv6, peerhandler, false)
{
	(void)protocol_id;
	ws_ipv6 = ipv6;

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
		Server.set_open_handshake_timeout(WS_SCTP_TIMEOUT_MS);
		Server.set_close_handshake_timeout(WS_SCTP_TIMEOUT_MS);
		Server.set_pong_timeout(WS_SCTP_TIMEOUT_MS);
		Server.set_listen_backlog(100);
		Server.set_max_http_body_size(500000000);
		Server.set_max_message_size(500000000);
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
#if USE_WS_SCTP_SSL
		//Server.set_tls_init_handler(bind(&broadcast_server::on_tls_init, this,				websocketpp::lib::placeholders::_1));
		Server.set_tls_init_handler(
			bind(&Connection::on_tls_init, this, websocketpp::lib::placeholders::_1));
#endif
		// Server.set_timer(long duration, timer_handler callback);
		//}

		Client.set_error_channels(websocketpp::log::elevel::none);
		Client.set_access_channels(websocketpp::log::alevel::none);
		Client.init_asio();
		Client.set_open_handler(websocketpp::lib::bind(
				&Connection::on_client_open, this, websocketpp::lib::placeholders::_1));
		Client.set_fail_handler(websocketpp::lib::bind(
				&Connection::on_client_fail, this, websocketpp::lib::placeholders::_1));
		Client.set_close_handler(websocketpp::lib::bind(
				&Connection::on_client_close, this, websocketpp::lib::placeholders::_1));
		Client.set_message_handler(websocketpp::lib::bind(&Connection::on_client_message,
				this, websocketpp::lib::placeholders::_1,
				websocketpp::lib::placeholders::_2));
#if USE_WS_SCTP_SSL
		Client.set_tls_init_handler(bind(
				&Connection::on_client_tls_init, this, websocketpp::lib::placeholders::_1));
#endif

#if 0 && USE_WS_SCTP_SSL
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

	start();
}

// bool Connection::sctp_inited = false;

Connection::~Connection()
{
	join();
	closeWebSocketState();
	disconnect();
	finish_sctp();
	cleanupRetiredUlpInfo(true);
}

void Connection::Connect(Address address)
{
	putCommand(ConnectionCommand::connect(address));
}

bool Connection::Connected()
{
	return con_sctp::Connection::Connected() &&
			ws_client_open.load(std::memory_order_acquire) &&
			ws_sctp_connected.load(std::memory_order_acquire);
}

void Connection::connectWebSocket(const Address &address)
{
	client_address = address;
	ws_client = true;
	ws_client_open.store(false, std::memory_order_release);
	ws_sctp_connected.store(false, std::memory_order_release);
	Client.reset();

#if USE_WS_SCTP_SSL
	const std::string scheme = "wss://";
#else
	const std::string scheme = "ws://";
#endif
	const std::string uri = makeWebSocketUri(scheme, address);

	websocketpp::lib::error_code ec;
	auto con = Client.get_connection(uri, ec);
	if (ec) {
		errorstream << "Could not create WS SCTP connection to " << uri << ": "
					<< ec.message() << std::endl;
		putEvent(ConnectionEvent::connectFailed());
		return;
	}

	client_hdl = con->get_handle();
	Client.connect(con);
}

void Connection::Disconnect()
{
	putCommand(ConnectionCommand::disconnect());
}

void Connection::DisconnectPeer(session_t peer_id)
{
	putCommand(ConnectionCommand::disconnect_peer(peer_id));
}

void Connection::disconnectWebSocket()
{
	closeWebSocketState();
	disconnect();
}

void Connection::processCommand(ConnectionCommandPtr c)
{
	switch (c->type) {
	case CONNCMD_CONNECT:
		dout_con << getDesc() << " processing WS SCTP CONNCMD_CONNECT" << std::endl;
		connectWebSocket(c->address);
		return;
	case CONNCMD_DISCONNECT:
		dout_con << getDesc() << " processing WS SCTP CONNCMD_DISCONNECT" << std::endl;
		disconnectWebSocket();
		return;
	case CONNCMD_DISCONNECT_PEER:
		dout_con << getDesc() << " processing WS SCTP CONNCMD_DISCONNECT_PEER"
				 << std::endl;
		deletePeer(c->peer_id, false);
		return;
	default:
		con_sctp::Connection::processCommand(c);
		return;
	}
}

void Connection::onAssociationChange(
		session_t peer_id, const struct sctp_assoc_change *sac)
{
	if (peer_id != PEER_ID_SERVER || !sac)
		return;

	switch (sac->sac_state) {
	case SCTP_COMM_UP:
		ws_sctp_connected.store(true, std::memory_order_release);
		break;
	case SCTP_COMM_LOST:
	case SCTP_SHUTDOWN_COMP:
	case SCTP_CANT_STR_ASSOC:
		ws_sctp_connected.store(false, std::memory_order_release);
		break;
	default:
		break;
	}
}

/* Internal stuff */

int Connection::receive()
{
	pumpWebSockets();
	cleanupRetiredUlpInfo();
	return acceptPeers() + con_sctp::Connection::receive();
}

// host
void Connection::serve(const Address &bind_address)
{
	infostream << getDesc() << "WS serving at " << bind_address.serializeString() << ":"
			   << std::to_string(bind_address.getPort()) << std::endl;
	ws_sctp_port = bind_address.getPort();
	Server.reset();

	websocketpp::lib::error_code ec;
	if (ws_ipv6 || bind_address.isIPv6()) {
		Server.listen(websocketpp::lib::asio::ip::tcp::v6(),
				bind_address.getPort(), ec);
	} else {
		Server.listen(websocketpp::lib::asio::ip::tcp::v4(),
				bind_address.getPort(), ec);
	}
	if (ec) {
		errorstream << "WS SCTP listen fail: " << ec.message() << std::endl;
		putEvent(ConnectionEvent::bindFailed());
		return;
	}

	Server.start_accept(ec);
	if (ec) {
		errorstream << "WS SCTP accept start fail: " << ec.message() << std::endl;
		putEvent(ConnectionEvent::bindFailed());
		return;
	}

	ws_serve = true;
}

} // namespace

#endif
