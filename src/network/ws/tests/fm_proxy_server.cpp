// SPDX-License-Identifier: GPL-3.0-or-later
// Standalone endpoint for protocol integration tests; no game process required.
#include <websocketpp/config/asio_no_tls.hpp>
#include <websocketpp/server.hpp>
#include "../fm_ws_datagram.h"
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
	fm_ws::DatagramService<Server> *datagrams = nullptr;
	fm_ws::DatagramService<Server> service(server, game_router,
			[&](auto hdl, const std::string &payload) {
				datagrams->send_game(hdl, payload); // Echo the game queue.
			});
	datagrams = &service;
	server.set_message_handler([&](auto hdl, auto msg) {
		if (auto it = sessions.find(hdl); it != sessions.end()) {
			it->second->receive(msg->get_payload());
			return;
		}
		const bool text = msg->get_opcode() == websocketpp::frame::opcode::text;
		if (service.receive(hdl, msg->get_payload(), text))
			return;
		if (text && msg->get_payload().starts_with("PROXY ")) {
			auto proxy = std::make_shared<Proxy>(
					server, hdl, argc > 1 && std::string(argv[1]) == "enabled");
			sessions.emplace(hdl, proxy);
			proxy->start(msg->get_payload());
			return;
		}
		websocketpp::lib::error_code ec;
		server.close(hdl, websocketpp::close::status::protocol_error, "", ec);
	});
	server.set_close_handler([&](auto hdl) {
		service.remove(hdl);
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
