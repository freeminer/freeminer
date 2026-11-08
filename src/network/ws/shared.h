// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include "transport.h"
#include <cerrno>
#include <map>
#include <mutex>
#include <websocketpp/common/connection_hdl.hpp>
#include <websocketpp/frame.hpp>

namespace ws_transport
{
// The WebSocket server owns connection handles; the SCTP worker owns its
// sockets. Events and sends cross that boundary through Transport's queues.
template <class Server>
class SharedSctpEndpoint final : public SharedServer
{
public:
	using Handle = websocketpp::connection_hdl;
	SharedSctpEndpoint(Server &server, const std::shared_ptr<Transport> &transport) :
			m_server(&server), m_transport(transport)
	{
	}

	bool open(Handle handle, const Address &address)
	{
		std::lock_guard lock(m_mutex);
		auto transport = m_transport.lock();
		if (!m_server || !transport)
			return false;
		const auto id = m_next_id++;
		m_ids.emplace(handle, id);
		m_handles.emplace(id, handle);
		transport->sharedOpen(id, address);
		return true;
	}

	bool message(Handle handle, const std::string &data, bool binary)
	{
		std::lock_guard lock(m_mutex);
		auto it = m_ids.find(handle);
		if (it == m_ids.end())
			return false;
		if (!binary) {
			websocketpp::lib::error_code error;
			m_server->close(handle, websocketpp::close::status::unsupported_data,
					"binary required", error);
		} else if (auto transport = m_transport.lock()) {
			transport->sharedMessage(it->second, data);
		}
		return true;
	}

	bool closed(Handle handle)
	{
		std::lock_guard lock(m_mutex);
		auto it = m_ids.find(handle);
		if (it == m_ids.end())
			return false;
		const auto id = it->second;
		m_ids.erase(it);
		m_handles.erase(id);
		if (auto transport = m_transport.lock())
			transport->sharedClose(id);
		return true;
	}

	int send(uint64_t id, const void *data, size_t size) override
	{
		std::lock_guard lock(m_mutex);
		auto it = m_handles.find(id);
		if (!m_server || it == m_handles.end())
			return ENOTCONN;
		websocketpp::lib::error_code error;
		m_server->send(it->second, data, size, websocketpp::frame::opcode::binary, error);
		return error ? EIO : 0;
	}

	void close(uint64_t id) override
	{
		std::lock_guard lock(m_mutex);
		auto it = m_handles.find(id);
		if (!m_server || it == m_handles.end())
			return;
		websocketpp::lib::error_code error;
		m_server->close(it->second, websocketpp::close::status::going_away,
				"peer removed", error);
		m_ids.erase(it->second);
		m_handles.erase(it);
	}

	void stop() override
	{
		std::lock_guard lock(m_mutex);
		if (m_server) {
			for (const auto &[id, handle] : m_handles) {
				websocketpp::lib::error_code error;
				m_server->close(handle, websocketpp::close::status::going_away,
						"server stopping", error);
			}
		}
		m_handles.clear();
		m_ids.clear();
		m_transport.reset();
		m_server = nullptr;
	}

private:
	std::mutex m_mutex;
	Server *m_server;
	std::weak_ptr<Transport> m_transport;
	uint64_t m_next_id = 1;
	std::map<Handle, uint64_t, std::owner_less<Handle>> m_ids;
	std::map<uint64_t, Handle> m_handles;
};
}
