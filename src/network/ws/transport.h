// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include "network/address.h"
#include <cstdint>
#include <cstddef>
#include <memory>
#include <string>
#include <string_view>
#include <vector>

namespace ws_transport
{
class SharedServer
{
public:
	virtual ~SharedServer() = default;
	virtual int send(uint64_t id, const void *data, size_t size) = 0;
	virtual void close(uint64_t id) = 0;
	virtual void stop() = 0;
};
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
inline std::string makeUri(
		const Address &address, bool secure, std::string_view host_name)
{
	if (host_name.empty())
		return makeUri(address, secure);
	std::string host(host_name);
	if (host.size() > 1 && host.front() == '[' && host.back() == ']')
		host = host.substr(1, host.size() - 2);
	const bool ipv6 = host.find(':') != std::string::npos;
	if (ipv6) {
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
	explicit Transport(bool local = false);
	~Transport();
	bool connect(const Address &address, std::string_view host_name = {});
	bool listen(const Address &address);
	void attachSharedServer(std::shared_ptr<SharedServer> server);
	void sharedOpen(Id id, const Address &address);
	void sharedMessage(Id id, std::string data);
	void sharedClose(Id id);
	int send(Id id, const void *data, size_t size);
	void close(Id id);
	void stop();
	std::vector<Event> poll();

private:
	struct Impl;
	std::shared_ptr<Impl> m_impl;
};
}
