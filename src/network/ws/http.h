// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include "filesys.h"
#include "log.h"
#include "porting.h"
#include "settings.h"
#include "server/serverlist.h"
#include <filesystem>
#include <fstream>
#include <sstream>
#include <websocketpp/server.hpp>
namespace ws_transport
{
template <class Server>
void serveHttp(Server &server, const websocketpp::connection_hdl &hdl)
{
	auto con = server.get_con_from_hdl(hdl);

	std::string res = con->get_request_body();

	std::stringstream ss;
	ss << "got HTTP request with " << res.size() << " bytes of body data.";
	tracestream << ss.str() << '\n';
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
		} else if (!uri.empty() && uri != "/status.json") {
			namespace fs = std::filesystem;
			std::error_code ec;
			while (!uri.empty() && uri.front() == '/')
				uri.erase(uri.begin());
			const auto root = fs::weakly_canonical(http_root, ec);
			if (ec) {
				con->set_status(websocketpp::http::status_code::not_found);
				return;
			}
			const auto file = fs::weakly_canonical(root / uri, ec);
			const auto relative = file.lexically_relative(root);
			if (ec || relative.empty() || *relative.begin() == ".." ||
					!fs::is_regular_file(file, ec)) {
				con->set_status(websocketpp::http::status_code::not_found);
				return;
			}
			path_serve = file.string();
			if (uri.ends_with(".wasm")) {
				con->append_header("Content-Type", "application/wasm");
			} else if (uri.ends_with(".js")) {
				con->append_header("Content-Type", "application/javascript");
			}
		}

		std::string body;
		if (uri == "status.json" || uri == "/status.json") {
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

}
