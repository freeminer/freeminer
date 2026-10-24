// SPDX-License-Identifier: GPL-3.0-or-later
// Standalone regression: c++ -std=c++17 -Isrc/external/websocketpp
// src/network/ws/tests/handshake.cpp -o /tmp/ws-handshake && /tmp/ws-handshake
#include <websocketpp/processors/processor.hpp>
#include <cassert>
#include <string>

struct Request
{
	std::string host;
	std::string get_header(const std::string &) const { return host; }
	std::string get_uri() const { return "/game?transport=ws"; }
};

int main()
{
	for (const std::string host :
			{"fm.setun.net", "81.207.3.22", "[2a02:a466:8baa:0:216:3eff:fe00:c211]"}) {
		for (const std::string port : {"", ":30013"}) {
			Request request{host + port};
			const auto uri = websocketpp::processor::get_uri_from_host(request, "wss");
			assert(uri->get_valid());
			assert(uri->get_port() == (port.empty() ? 443 : 30013));
			assert(uri->get_resource() == request.get_uri());
			const auto expected =
					host.front() == '[' ? host.substr(1, host.size() - 2) : host;
			assert(uri->get_host() == expected);
			Request roundtrip{uri->get_host_port()};
			const auto parsed =
					websocketpp::processor::get_uri_from_host(roundtrip, "wss");
			assert(parsed->get_valid());
			assert(parsed->get_host() == expected);
			assert(parsed->get_port() == uri->get_port());
		}
	}
}
