/*
Copyright (C) 2026 Freeminer contributors
SPDX-License-Identifier: LGPL-2.1-or-later
*/
#pragma once

#include "network/address.h"
#include <cstring>

namespace con_sctp
{

inline Address fmSctpPeerAddress(const sockaddr *address, size_t length)
{
	if (!address || length < sizeof(address->sa_family))
		return {};
	if (address->sa_family == AF_INET6 && length >= sizeof(sockaddr_in6)) {
		sockaddr_in6 ipv6;
		std::memcpy(&ipv6, address, sizeof(ipv6));
		return Address(ipv6); // Preserve the zone and convert the port to host order.
	}
	if (address->sa_family == AF_INET && length >= sizeof(sockaddr_in)) {
		sockaddr_in ipv4;
		std::memcpy(&ipv4, address, sizeof(ipv4));
		return Address(ipv4);
	}
	return {}; // AF_CONN is not an IP endpoint.
}

} // namespace con_sctp
