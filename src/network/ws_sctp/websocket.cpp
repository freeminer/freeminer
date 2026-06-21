/*
Copyright (C) 2023 proller <proler@gmail.com>
*/

#include "network/ws_sctp/internal.h"

#if USE_WEBSOCKET_SCTP

namespace con_ws_sctp
{

#if USE_WS_SCTP_SSL
Connection::context_ptr Connection::on_tls_init(const websocketpp::connection_hdl & /* hdl */)
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

Connection::context_ptr Connection::on_client_tls_init(
		const websocketpp::connection_hdl & /* hdl */)
{
	namespace asio = websocketpp::lib::asio;
	context_ptr ctx = websocketpp::lib::make_shared<asio::ssl::context>(
			asio::ssl::context::tlsv13_client);
	try {
		ctx->set_options(asio::ssl::context::default_workarounds |
						 asio::ssl::context::no_sslv2 | asio::ssl::context::no_sslv3);
	} catch (const std::exception &e) {
		errorstream << "Client TLS init exception: " << e.what();
	}
	return ctx;
}
#endif


void Connection::on_http(websocketpp::connection_hdl hdl)
{
	ws_server_t::connection_ptr con = Server.get_con_from_hdl(hdl);

	std::string res = con->get_request_body();

	std::stringstream ss;
	ss << "got HTTP request with " << res.size() << " bytes of body data.";
	cs << ss.str() << '\n';
	std::string http_root = porting::path_share + DIR_DELIM + "http_root" + DIR_DELIM;
	g_settings->getNoEx("http_root", http_root);

	if (con->get_request().get_method() == "GET") {
		namespace fs = std::filesystem;

		auto uri = con->get_request().get_uri();
		if (const auto f = uri.find('?'); f != std::string::npos) {
			uri.resize(f);
		}

		auto lower_uri = uri;
		std::transform(lower_uri.begin(), lower_uri.end(), lower_uri.begin(),
				[](unsigned char c) { return std::tolower(c); });
		if (uri.find("..") != std::string::npos ||
				lower_uri.find("%2e") != std::string::npos) {
			con->set_status(websocketpp::http::status_code::bad_request);
			return;
		}

		std::string path_serve;
		if (uri == "/") {
			uri = "index.html";
		}

		if (uri == "/favicon.ico") {
			path_serve = porting::path_share + DIR_DELIM + "misc" + DIR_DELIM +
						 PROJECT_NAME + ".ico";
			con->append_header("Content-Type", "image/x-icon");
		} else if (!uri.empty()) {
			while (!uri.empty() && uri.front() == '/')
				uri.erase(uri.begin());

			std::error_code ec;
			const fs::path root = fs::weakly_canonical(fs::path(http_root), ec);
			if (ec) {
				con->set_status(websocketpp::http::status_code::not_found);
				return;
			}
			const fs::path candidate = fs::weakly_canonical(root / fs::path(uri), ec);
			const auto relative = candidate.lexically_relative(root);
			if (ec || relative.empty() || *relative.begin() == ".." ||
					!fs::is_regular_file(candidate, ec)) {
				con->set_status(websocketpp::http::status_code::not_found);
				return;
			}
			path_serve = candidate.string();
			if (uri.ends_with(".wasm")) {
				con->append_header("Content-Type", "application/wasm");
			} else if (uri.ends_with(".js")) {
				con->append_header("Content-Type", "application/javascript");
			}
		}

		if (!path_serve.empty()) {
			con->append_header("Access-Control-Allow-Origin", "*");
			con->append_header("Cross-Origin-Embedder-Policy", "require-corp");
			con->append_header("Cross-Origin-Opener-Policy", "same-origin");
			con->defer_http_response();
			std::ifstream t(path_serve);
			if (!t) {
				con->set_status(websocketpp::http::status_code::not_found);
				con->send_http_response();
				return;
			}
			std::stringstream buffer;
			buffer << t.rdbuf();
			con->set_body(buffer.str());
			con->set_status(websocketpp::http::status_code::ok);
			con->send_http_response();
			// TODO: serve log here?
			return;
		}
	}
	// DUMP(con->get_request().get_method(), con->get_request().get_version(), con->get_request().get_uri(), con->get_request().get_headers(), res);

	con->set_body(ss.str());
	con->set_status(websocketpp::http::status_code::not_found);
}

void Connection::on_fail(websocketpp::connection_hdl hdl)
{
	ws_server_t::connection_ptr con = Server.get_con_from_hdl(hdl);

	cs << "Fail handler: " << con->get_ec() << " " << con->get_ec().message()
	   << std::endl;
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


void Connection::pumpWebSockets()
{
	if (ws_serve) {
		try {
			while (Server.poll_one() > 0) {
			}
		} catch (const std::exception &e) {
			cs << "WS server poll error: " << e.what() << std::endl;
		}
	}
	if (ws_client) {
		try {
			while (Client.poll_one() > 0) {
			}
		} catch (const std::exception &e) {
			cs << "WS client poll error: " << e.what() << std::endl;
		}
	}
}

void Connection::on_close(websocketpp::connection_hdl hdl)
{
	cs << "Close handler" << std::endl;

	std::shared_ptr<ws_peer> peer;
	{
		const auto lock = std::unique_lock(peersMutex);
		auto it = hdls.find(hdl);
		if (it == hdls.end())
			return;
		peer = it->second;
		hdls.erase(it);
	}
	closePeer(peer, false);
}

void Connection::on_open(websocketpp::connection_hdl hdl)
{
	cs << "open handler" << std::endl;

	const auto peer_id = nextPeerId();
	if (peer_id == PEER_ID_INEXISTENT) {
		Server.close(hdl, websocketpp::close::status::try_again_later,
				"no free peer ids");
		return;
	}

	auto peer = makePeer(hdl, peer_id, false);
	if (!setupListenSocket(peer)) {
		closePeer(peer, false);
		Server.close(hdl, websocketpp::close::status::internal_endpoint_error,
				"sctp setup failed");
		return;
	}

	const auto lock = std::unique_lock(peersMutex);
	hdls.emplace(hdl, std::move(peer));
}

// Define a callback to handle incoming messages
void Connection::on_message(websocketpp::connection_hdl hdl, message_ptr msg)
{
	cs << "on_message called with hdl: " << hdl.lock().get()
	   << " and message: " << msg->get_payload().size() << std::endl;

	std::shared_ptr<ws_peer> peer;
	{
		const auto lock = std::shared_lock(peersMutex);
		auto it = hdls.find(hdl);
		if (it != hdls.end())
			peer = it->second;
	}
	if (!peer || !peer->ulp_info) {
		cs << "WS SCTP message without peer state" << std::endl;
		return;
	}

	DUMP("usrsctp_conninput:", (long)peer->ulp_info.get(), msg->get_payload().size());
	usrsctp_conninput(peer->ulp_info.get(), (void *)msg->get_payload().data(),
			msg->get_payload().size(), 0);
}

void Connection::on_client_open(websocketpp::connection_hdl hdl)
{
	cs << "client open handler" << std::endl;
	client_hdl = hdl;
	ws_client_open.store(true, std::memory_order_release);

	auto peer = makePeer(hdl, PEER_ID_SERVER, true);
	if (!setupClientSocket(peer, client_address)) {
		closePeer(peer, false);
		putEvent(ConnectionEvent::connectFailed());
		websocketpp::lib::error_code ec;
		Client.close(hdl, websocketpp::close::status::internal_endpoint_error,
				"sctp setup failed", ec);
		return;
	}
	client_peer = std::move(peer);
}

void Connection::on_client_fail(websocketpp::connection_hdl hdl)
{
	cs << "client fail handler" << std::endl;
	(void)hdl;
	ws_client_open.store(false, std::memory_order_release);
	ws_sctp_connected.store(false, std::memory_order_release);
	putEvent(ConnectionEvent::connectFailed());
	if (client_peer) {
		closePeer(client_peer, false);
		client_peer.reset();
	}
}

void Connection::on_client_close(websocketpp::connection_hdl hdl)
{
	cs << "client close handler" << std::endl;
	(void)hdl;
	ws_client_open.store(false, std::memory_order_release);
	ws_sctp_connected.store(false, std::memory_order_release);
	if (client_peer) {
		closePeer(client_peer, false);
		client_peer.reset();
	}
}

void Connection::on_client_message(websocketpp::connection_hdl hdl, client_message_ptr msg)
{
	(void)hdl;
	if (!client_peer || !client_peer->ulp_info) {
		cs << "WS SCTP client message without peer state" << std::endl;
		return;
	}

	DUMP("client usrsctp_conninput:", (long)client_peer->ulp_info.get(),
			msg->get_payload().size());
	usrsctp_conninput(client_peer->ulp_info.get(), (void *)msg->get_payload().data(),
			msg->get_payload().size(), 0);
}

} // namespace

#endif
