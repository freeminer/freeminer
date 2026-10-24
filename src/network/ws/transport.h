// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include "network/address.h"
#include <cstdint>
#include <cstddef>
#include <memory>
#include <string>
#include <vector>

namespace ws_transport
{
// URI syntax is shared by the native datagram client and binary carriers.
inline std::string makeUri(const Address &address, bool secure)
{
	std::string host = address.serializeString();
	if (address.isIPv6()) {
		size_t offset = 0;
		while ((offset = host.find('%', offset)) != std::string::npos) {
			host.replace(offset, 1, "%25");
			offset += 3;
		}
		host = "[" + host + "]";
	}
	return std::string(secure ? "wss://" : "ws://") + host + ":" +
		   std::to_string(address.getPort());
}
// Protocol-neutral binary WebSocket carrier. Poll and lifecycle methods belong
// to the connection worker; send may also be called by usrsctp timer threads.
class Transport
{
public:
	using Id = uint64_t;
	enum class Type
	{
		Open,
		Message,
		Closed,
		Failed
	};
	struct Event
	{
		Type type;
		Id id;
		bool client = false;
		Address address;
		std::string data;
	};
	Transport();
	~Transport();
	bool connect(const Address &address);
	bool listen(const Address &address);
	int send(Id id, const void *data, size_t size);
	void close(Id id);
	void stop();
	std::vector<Event> poll();

private:
	struct Impl;
	std::shared_ptr<Impl> m_impl;
};
}
