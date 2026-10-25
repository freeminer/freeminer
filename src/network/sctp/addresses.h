/*
Copyright (C) 2026 proller <proler@gmail.com>
*/

#pragma once

#include "network/address.h"

#include <algorithm>
#include <cstdint>
#include <cstring>
#include <memory>
#include <string>
#include <vector>

#ifndef _WIN32
#include <netdb.h>
#endif

namespace con_sctp
{

// Keep the selected destination first, then retain the hostname's other paths.
// A public NAT address cannot be discovered from the peer's interface list.
inline std::vector<Address> resolvePeerAddresses(
		const Address &primary, const std::string &hostname, bool ipv6)
{
	std::vector<Address> addresses{primary};
	if (hostname.empty())
		return addresses;

	addrinfo hints{};
	hints.ai_family = ipv6 ? AF_UNSPEC : AF_INET;
	hints.ai_socktype = SOCK_DGRAM;
	addrinfo *result = nullptr;
	if (getaddrinfo(hostname.c_str(), nullptr, &hints, &result) != 0)
		return addresses;
	const std::unique_ptr<addrinfo, decltype(&freeaddrinfo)> resolved(
			result, freeaddrinfo);
	for (const auto *entry = resolved.get(); entry; entry = entry->ai_next) {
		Address address;
		if (entry->ai_family == AF_INET && entry->ai_addrlen >= sizeof(sockaddr_in))
			address = Address(*reinterpret_cast<const sockaddr_in *>(entry->ai_addr));
		else if (entry->ai_family == AF_INET6 &&
				 entry->ai_addrlen >= sizeof(sockaddr_in6))
			address = Address(*reinterpret_cast<const sockaddr_in6 *>(entry->ai_addr));
		else
			continue;
		address.setPort(primary.getPort());
		if (!address.isAny() &&
				std::find(addresses.begin(), addresses.end(), address) == addresses.end())
			addresses.push_back(address);
	}
	return addresses;
}

// connectx consumes consecutive sockaddr structures, without storage padding.
inline std::vector<uint8_t> packPeerAddresses(const std::vector<Address> &addresses)
{
	std::vector<uint8_t> packed;
	packed.reserve(addresses.size() * sizeof(sockaddr_in6));
	for (const auto &address : addresses) {
		if (address.isIPv6()) {
			auto native = address.getAddress6();
			native.sin6_port = htons(address.getPort());
			const auto *bytes = reinterpret_cast<const uint8_t *>(&native);
			packed.insert(packed.end(), bytes, bytes + sizeof(native));
		} else {
			auto native = address.getAddress();
			native.sin_port = htons(address.getPort());
			const auto *bytes = reinterpret_cast<const uint8_t *>(&native);
			packed.insert(packed.end(), bytes, bytes + sizeof(native));
		}
	}
	return packed;
}

} // namespace con_sctp
