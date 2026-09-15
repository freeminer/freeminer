// SPDX-License-Identifier: GPL-3.0-or-later
// Freeminer services for the emsocket WebSocket transport.
#pragma once

#include <boost/asio.hpp>
#include <websocketpp/common/connection_hdl.hpp>
#include <websocketpp/close.hpp>
#include <websocketpp/frame.hpp>
#include <array>
#include <chrono>
#include <cstdint>
#include <deque>
#include <memory>
#include <set>
#include <sstream>
#include <string>

namespace fm_ws
{
namespace asio = boost::asio;
using tcp = asio::ip::tcp;
using Error = boost::system::error_code;

inline bool parse_port(const std::string &text, uint16_t &port)
{
	if (text.empty() || text.size() > 5)
		return false;
	unsigned value = 0;
	for (char c : text) {
		if (c < '0' || c > '9')
			return false;
		value = value * 10 + c - '0';
	}
	if (!value || value > 65535)
		return false;
	port = value;
	return true;
}

// All callbacks run on the WebSocket server's io_context. No blocking network
// operations or detached threads may hold up the game or outlive the server.
template <typename Server>
class Proxy : public std::enable_shared_from_this<Proxy<Server>>
{
public:
	Proxy(Server &server, websocketpp::connection_hdl hdl, bool enabled) :
			m_server(server), m_hdl(hdl), m_enabled(enabled),
			m_resolver(server.get_io_context()), m_socket(server.get_io_context()),
			m_timer(server.get_io_context())
	{
	}

	void start(const std::string &request)
	{
		std::istringstream input(request);
		std::string command, family, transport, host, port_text, extra;
		uint16_t port;
		input >> command >> family >> transport >> host >> port_text;
		Error ec;
		auto address = asio::ip::make_address(host, ec);
		if (request.size() > 512 || command != "PROXY" || transport != "TCP" ||
				!parse_port(port_text, port) || (input >> extra) || ec ||
				(family != "IPV4" && family != "IPV6") ||
				(address.is_v4() != (family == "IPV4"))) {
			fail();
			return;
		}
		arm_timeout();
		if (address == asio::ip::make_address("fd00::1")) {
			if (port == 53) {
				m_state = State::dns;
				send("PROXY OK", true);
			} else if ((port == 8080 || port == 3128) && m_enabled) {
				m_state = State::http;
				send("PROXY OK", true);
			} else {
				fail();
			}
		} else if (m_enabled) {
			connect(host, port_text, false);
		} else {
			fail();
		}
	}

	void receive(const std::string &data)
	{
		if (m_state == State::closed)
			return;
		if (m_state == State::dns) {
			resolve(data);
		} else if (m_state == State::http) {
			if (data.size() > MAX_PENDING - m_header.size()) {
				http_error("431 Request Header Fields Too Large");
				return;
			}
			m_header += data;
			const auto end = m_header.find("\r\n\r\n");
			if (end == std::string::npos || end > 16380) {
				if (m_header.size() > 16384)
					http_error("431 Request Header Fields Too Large");
				return;
			}
			std::istringstream line(m_header.substr(0, m_header.find("\r\n")));
			std::string method, target, version, extra;
			line >> method >> target >> version;
			if (method != "CONNECT" || (version != "HTTP/1.1" && version != "HTTP/1.0") ||
					(line >> extra)) {
				http_error("400 Bad Request");
				return;
			}
			const auto colon = target.rfind(':');
			uint16_t port;
			if (colon == std::string::npos ||
					!parse_port(target.substr(colon + 1), port)) {
				http_error("400 Bad Request");
				return;
			}
			std::string host = target.substr(0, colon);
			if (host.size() > 2 && host.front() == '[' && host.back() == ']')
				host = host.substr(1, host.size() - 2);
			else if (host.find(':') != std::string::npos)
				host.clear();
			if (host.empty() || host.find('\0') != std::string::npos ||
					host.find_first_of("/@?#\r\n\t ") != std::string::npos) {
				http_error("400 Bad Request");
				return;
			}
			queue(m_header.substr(end + 4));
			m_header.clear();
			if (m_state == State::closed)
				return;
			connect(host, std::to_string(port), true);
		} else if (m_state == State::connecting || m_state == State::stream) {
			queue(data);
		} else {
			// One outstanding resolver query per connection.
			fail();
		}
	}

	void stop()
	{
		m_state = State::closed;
		m_resolver.cancel();
		m_timer.cancel();
		Error ec;
		m_socket.close(ec);
	}

private:
	enum class State
	{
		connecting,
		dns,
		resolving,
		http,
		stream,
		closed
	};
	static constexpr size_t MAX_PENDING = 1024 * 1024;
	Server &m_server;
	websocketpp::connection_hdl m_hdl;
	bool m_enabled;
	tcp::resolver m_resolver;
	tcp::socket m_socket;
	asio::steady_timer m_timer;
	State m_state = State::connecting;
	std::array<char, 16384> m_read;
	std::string m_header;
	std::deque<std::shared_ptr<std::string>> m_writes;
	size_t m_pending = 0;
	bool m_writing = false;

	void arm_timeout()
	{
		m_timer.expires_after(std::chrono::seconds(30));
		m_timer.async_wait([self = this->shared_from_this()](Error ec) {
			if (!ec)
				self->close();
		});
	}

	void close()
	{
		if (m_state == State::closed)
			return;
		stop();
		websocketpp::lib::error_code ec;
		m_server.close(m_hdl, websocketpp::close::status::normal, "", ec);
	}

	void send(const std::string &data, bool text = false)
	{
		if (m_state == State::closed)
			return;
		websocketpp::lib::error_code ec;
		auto con = m_server.get_con_from_hdl(m_hdl, ec);
		if (ec || con->get_buffered_amount() + data.size() > MAX_PENDING) {
			close();
			return;
		}
		m_server.send(m_hdl, data,
				text ? websocketpp::frame::opcode::text
					 : websocketpp::frame::opcode::binary,
				ec);
		if (ec)
			close();
	}

	void fail()
	{
		send("PROXY FAILED", true);
		close();
	}

	void http_error(const std::string &status)
	{
		send("HTTP/1.1 " + status + "\r\nContent-Length: 0\r\nConnection: close\r\n\r\n");
		close();
	}

	void resolve(const std::string &request)
	{
		std::istringstream input(request);
		std::string host, type, extra;
		input >> host >> type;
		if (request.size() > 260 || host.empty() || host.size() > 253 ||
				host.find('\0') != std::string::npos || (input >> extra) ||
				(type != "A" && type != "AAAA" && type != "ANY")) {
			send(std::string(1, '\0'));
			close();
			return;
		}
		m_state = State::resolving;
		arm_timeout();
		auto callback = [self = this->shared_from_this()](
								Error ec, tcp::resolver::results_type results) {
			if (self->m_state == State::closed)
				return;
			std::string reply(1, '\0');
			std::set<std::string> seen;
			if (!ec) {
				for (const auto &entry : results) {
					auto address = entry.endpoint().address();
					if (address.is_unspecified())
						continue;
					std::string record;
					if (address.is_v4()) {
						auto bytes = address.to_v4().to_bytes();
						record = std::string(1, 4) +
								 std::string(reinterpret_cast<const char *>(bytes.data()),
										 bytes.size());
					} else {
						auto bytes = address.to_v6().to_bytes();
						record = std::string(1, 6) +
								 std::string(reinterpret_cast<const char *>(bytes.data()),
										 bytes.size());
					}
					if (seen.insert(record).second)
						reply += record;
					if (seen.size() == 255)
						break;
				}
			}
			reply[0] = static_cast<char>(seen.size());
			self->send(reply);
			if (self->m_state == State::closed)
				return;
			self->m_state = State::dns;
			self->arm_timeout();
		};
		// Explicit flags avoid suppressing AAAA records on IPv4-only hosts.
		if (type == "ANY")
			m_resolver.async_resolve(
					host, "", tcp::resolver::flags(), std::move(callback));
		else
			m_resolver.async_resolve(type == "A" ? tcp::v4() : tcp::v6(), host, "",
					tcp::resolver::flags(), std::move(callback));
	}

	void connect(const std::string &host, const std::string &port, bool http)
	{
		m_state = State::connecting;
		arm_timeout();
		m_resolver.async_resolve(host, port,
				[self = this->shared_from_this(), http](
						Error ec, tcp::resolver::results_type results) {
					if (self->m_state == State::closed)
						return;
					if (ec) {
						self->connect_failed(http);
						return;
					}
					asio::async_connect(self->m_socket, results,
							[self, http](Error ec, const tcp::endpoint &) {
								if (self->m_state == State::closed)
									return;
								if (ec) {
									self->connect_failed(http);
									return;
								}
								self->m_state = State::stream;
								self->m_timer.cancel();
								self->send(
										http ? "HTTP/1.1 200 Connection Established\r\n\r\n"
											 : "PROXY OK",
										!http);
								self->read();
								self->write();
							});
				});
	}

	void connect_failed(bool http)
	{
		if (http)
			http_error("502 Bad Gateway");
		else
			fail();
	}

	void read()
	{
		if (m_state != State::stream)
			return;
		m_socket.async_read_some(asio::buffer(m_read),
				[self = this->shared_from_this()](Error ec, size_t size) {
					if (size)
						self->send(std::string(self->m_read.data(), size));
					if (ec)
						self->close();
					else
						self->read();
				});
	}

	void queue(const std::string &data)
	{
		if (data.empty() || m_state == State::closed)
			return;
		if (data.size() > MAX_PENDING - m_pending) {
			close();
			return;
		}
		m_pending += data.size();
		m_writes.push_back(std::make_shared<std::string>(data));
		write();
	}

	void write()
	{
		if (m_state != State::stream || m_writing || m_writes.empty())
			return;
		m_writing = true;
		auto data = m_writes.front();
		asio::async_write(m_socket, asio::buffer(*data),
				[self = this->shared_from_this(), data](Error ec, size_t) {
					self->m_writing = false;
					self->m_pending -= data->size();
					self->m_writes.pop_front();
					if (ec)
						self->close();
					else
						self->write();
				});
	}
};
} // namespace fm_ws
