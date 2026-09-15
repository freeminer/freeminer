// SPDX-License-Identifier: GPL-3.0-or-later
// Standalone endpoint for protocol integration tests; no game process required.
#include <websocketpp/config/asio_no_tls.hpp>
#include <websocketpp/server.hpp>
#include "../fm_ws_proxy.h"
#include <iostream>

int main(int argc, char **argv)
{
	using Server = websocketpp::server<websocketpp::config::asio>;
	using Proxy = fm_ws::Proxy<Server>;
	Server server;
	std::map<websocketpp::connection_hdl, std::shared_ptr<Proxy>,
			std::owner_less<websocketpp::connection_hdl>>
			sessions;
	server.clear_access_channels(websocketpp::log::alevel::all);
	server.clear_error_channels(websocketpp::log::elevel::all);
	server.init_asio();
	auto game_router = std::make_shared<fm_ws::GameRouter>(server.get_io_context());
	game_router->resolve("192.0.2.10"); // Simulated advertised address behind NAT.
	std::set<websocketpp::connection_hdl, std::owner_less<websocketpp::connection_hdl>>
			games;
	server.set_message_handler([&](auto hdl, auto msg) {
		auto it = sessions.find(hdl);
		if (it == sessions.end()) {
			websocketpp::lib::error_code ec;
			if (games.count(hdl)) {
				server.send(hdl, msg->get_payload(), msg->get_opcode(), ec);
				return;
			}
			fm_ws::Error endpoint_error;
			auto local = server.get_con_from_hdl(hdl)->get_raw_socket().local_endpoint(
					endpoint_error);
			if (!endpoint_error && game_router->matches(msg->get_payload(), local)) {
				games.insert(hdl);
				server.send(hdl, "GAME OK", websocketpp::frame::opcode::text, ec);
				return;
			}
			auto proxy = std::make_shared<Proxy>(
					server, hdl, argc > 1 && std::string(argv[1]) == "enabled");
			sessions.emplace(hdl, proxy);
			proxy->start(msg->get_payload());
		} else {
			it->second->receive(msg->get_payload());
		}
	});
	server.set_close_handler([&](auto hdl) {
		games.erase(hdl);
		if (auto it = sessions.find(hdl); it != sessions.end()) {
			it->second->stop();
			sessions.erase(it);
		}
	});
	server.listen(fm_ws::tcp::endpoint(fm_ws::asio::ip::make_address("127.0.0.1"), 0));
	server.start_accept();
	fm_ws::Error ec;
	std::cout << server.get_local_endpoint(ec).port() << std::endl;
	server.run();
}
