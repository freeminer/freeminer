// SPDX-License-Identifier: GPL-3.0-or-later
// emsocket address assignments and bound, encapsulated UDP sockets.
#pragma once

#include "fm_ws_proxy.h"
#include <openssl/rand.h>
#include <algorithm>
#include <functional>
#include <map>
#include <optional>

namespace fm_ws
{
struct Datagram
{
	static constexpr size_t HEADER_SIZE = 24;
	tcp::endpoint endpoint;
	std::string payload;

	bool parse(const std::string &data)
	{
		if (data.size() < HEADER_SIZE || data.compare(0, 4, "\x77\x8b\x4c\xf6", 4))
			return false;
		auto word = [&](size_t offset) {
			return (static_cast<unsigned char>(data[offset]) << 8) |
					static_cast<unsigned char>(data[offset + 1]);
		};
		if (data.size() - HEADER_SIZE != static_cast<size_t>(word(22)) || !word(20))
			return false;
		asio::ip::address_v6::bytes_type bytes;
		std::copy_n(data.begin() + 4, bytes.size(), bytes.begin());
		endpoint = tcp::endpoint(normalize_address(asio::ip::address_v6(bytes)), word(20));
		payload = data.substr(HEADER_SIZE);
		return true;
	}

	static std::string encode(const tcp::endpoint &source, const std::string &payload)
	{
		if (payload.size() > 65535)
			return {};
		std::string result("\x77\x8b\x4c\xf6", 4);
		asio::ip::address_v6::bytes_type bytes{};
		if (source.address().is_v4()) {
			bytes[10] = bytes[11] = 0xff;
			auto v4 = source.address().to_v4().to_bytes();
			std::copy(v4.begin(), v4.end(), bytes.begin() + 12);
		} else {
			bytes = source.address().to_v6().to_bytes();
		}
		result.append(reinterpret_cast<const char *>(bytes.data()), bytes.size());
		for (unsigned value : {unsigned(source.port()), unsigned(payload.size())}) {
			result += static_cast<char>(value >> 8);
			result += static_cast<char>(value);
		}
		return result + payload;
	}
};

// All access is on the server io_context, like Proxy and GameRouter. Assignments
// survive the short NEWADDR connection and remain alive while any port is bound.
template <typename Server>
class DatagramService
{
public:
	using Handle = websocketpp::connection_hdl;
	using GameHandler = std::function<void(Handle, const std::string &)>;
	using Clock = std::chrono::steady_clock;

	DatagramService(Server &server, std::shared_ptr<GameRouter> router, GameHandler game) :
			m_server(server), m_router(std::move(router)), m_game(std::move(game))
	{
	}

	bool receive(Handle hdl, const std::string &data, bool text)
	{
		auto it = m_sessions.find(hdl);
		if (it != m_sessions.end() && it->second.bound) {
			Datagram packet;
			if (text || !packet.parse(data)) {
				close(hdl, websocketpp::close::status::protocol_error);
				return true;
			}
			auto &session = it->second;
			Error ec;
			auto local = m_server.get_con_from_hdl(hdl)->get_raw_socket().local_endpoint(ec);
			if (!ec && m_router->matches(packet.endpoint, local)) {
				session.game_source = packet.endpoint;
				m_game(hdl, packet.payload);
			} else if (auto target = m_bindings.find(packet.endpoint);
					target != m_bindings.end()) {
				send(target->second, Datagram::encode(session.endpoint, packet.payload), false);
			}
			// This endpoint serves the game and its virtual network, not arbitrary
			// internet UDP destinations. Unbound/unknown destinations drop like UDP.
			return true;
		}
		if (!text)
			return false;
		if (data == "NEWADDR") {
			auto &session = m_sessions[hdl];
			if (session.assigned) {
				close(hdl, websocketpp::close::status::policy_violation);
				return true;
			}
			session.assigned = true;
			assign(hdl);
			return true;
		}
		if (data == "BIND" || data.starts_with("BIND ")) {
			bind(hdl, data);
			return true;
		}
		return false;
	}

	void send_game(Handle hdl, const std::string &payload)
	{
		auto it = m_sessions.find(hdl);
		if (it != m_sessions.end() && it->second.game_source) {
			auto data = Datagram::encode(*it->second.game_source, payload);
			if (!data.empty())
				send(hdl, data, false);
		}
	}

	void remove(Handle hdl)
	{
		auto it = m_sessions.find(hdl);
		if (it == m_sessions.end())
			return;
		if (it->second.bound) {
			m_bindings.erase(it->second.endpoint);
			auto lease = m_leases.find(it->second.passcode);
			if (lease != m_leases.end()) {
				--lease->second.bindings;
				lease->second.expires = Clock::now() + std::chrono::minutes(30);
			}
		}
		m_sessions.erase(it);
	}

private:
	struct Lease
	{
		asio::ip::address address;
		Clock::time_point expires;
		size_t bindings = 0;
	};
	struct Session
	{
		bool assigned = false;
		bool bound = false;
		std::string passcode;
		tcp::endpoint endpoint;
		std::optional<tcp::endpoint> game_source;
	};
	Server &m_server;
	std::shared_ptr<GameRouter> m_router;
	GameHandler m_game;
	std::map<std::string, Lease> m_leases;
	std::map<Handle, Session, std::owner_less<Handle>> m_sessions;
	std::map<tcp::endpoint, Handle> m_bindings;

	void prune()
	{
		const auto now = Clock::now();
		std::erase_if(m_leases, [&](const auto &entry) {
			return !entry.second.bindings && entry.second.expires <= now;
		});
	}

	void assign(Handle hdl)
	{
		prune();
		if (m_leases.size() >= 4096) {
			close(hdl, websocketpp::close::status::try_again_later);
			return;
		}
		std::array<unsigned char, 32> random;
		if (RAND_bytes(random.data(), random.size()) != 1) {
			close(hdl, websocketpp::close::status::internal_endpoint_error);
			return;
		}
		auto hex = [&](size_t first, size_t last) {
			std::string result;
			for (size_t i = first; i < last; ++i) {
				result += "0123456789abcdef"[random[i] >> 4];
				result += "0123456789abcdef"[random[i] & 15];
			}
			return result;
		};
		asio::ip::address_v6::bytes_type bytes{};
		bytes[0] = 0xfd;
		std::copy_n(random.begin(), 8, bytes.begin() + 8);
		asio::ip::address address = asio::ip::address_v6(bytes);
		const auto passcode = hex(8, 24);
		if (address == asio::ip::make_address("fd00::1") ||
				m_leases.count(passcode) ||
				std::any_of(m_leases.begin(), m_leases.end(), [&](const auto &entry) {
					return entry.second.address == address;
				})) {
			close(hdl, websocketpp::close::status::try_again_later);
			return;
		}
		m_leases.emplace(passcode, Lease{address, Clock::now() + std::chrono::minutes(30)});
		send(hdl, "ADDR " + address.to_string() + " " + passcode + " " + hex(24, 32), true);
	}

	void bind(Handle hdl, const std::string &data)
	{
		prune();
		std::istringstream input(data.size() <= 128 ? data : "");
		std::string command, passcode, transport, port_text, extra;
		input >> command >> passcode >> transport >> port_text;
		uint16_t port;
		auto lease = m_leases.find(passcode);
		if (command != "BIND" || transport != "UDP" || !parse_port(port_text, port) ||
				(input >> extra) || lease == m_leases.end()) {
			send(hdl, "BIND FAILED", true);
			return; // The shim obtains a fresh assignment and retries on this socket.
		}
		tcp::endpoint endpoint(lease->second.address, port);
		if (m_bindings.count(endpoint)) {
			send(hdl, "BIND FAILED", true);
			return;
		}
		auto &session = m_sessions[hdl];
		session.bound = true;
		session.passcode = passcode;
		session.endpoint = endpoint;
		++lease->second.bindings;
		m_bindings.emplace(endpoint, hdl);
		send(hdl, "BIND OK", true);
	}

	void close(Handle hdl, websocketpp::close::status::value status)
	{
		websocketpp::lib::error_code ec;
		m_server.close(hdl, status, "", ec);
		remove(hdl);
	}

	void send(Handle hdl, const std::string &data, bool text)
	{
		websocketpp::lib::error_code ec;
		auto con = m_server.get_con_from_hdl(hdl, ec);
		if (ec || con->get_buffered_amount() + data.size() > 1024 * 1024) {
			close(hdl, websocketpp::close::status::policy_violation);
			return;
		}
		m_server.send(hdl, data, text ? websocketpp::frame::opcode::text :
				websocketpp::frame::opcode::binary, ec);
		if (ec)
			close(hdl, websocketpp::close::status::internal_endpoint_error);
	}
};
} // namespace fm_ws
