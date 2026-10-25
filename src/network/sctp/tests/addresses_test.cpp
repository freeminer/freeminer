// SPDX-License-Identifier: LGPL-2.1-or-later
#include "network/sctp/addresses.h"

#include <cassert>
#include <iostream>

int main()
{
	sockaddr_in6 native6{};
	native6.sin6_family = AF_INET6;
	native6.sin6_addr = in6addr_loopback;
	Address ipv6(native6);
	ipv6.setPort(30113);
	Address ipv4(127, 0, 0, 1, ipv6.getPort());

	const auto from6 = con_sctp::resolvePeerAddresses(ipv6, "127.0.0.1", true);
	assert(from6.size() == 2 && from6[0] == ipv6 && from6[1] == ipv4);
	const auto from4 = con_sctp::resolvePeerAddresses(ipv4, "::1", true);
	assert(from4.size() == 2 && from4[0] == ipv4 && from4[1] == ipv6);
	assert(con_sctp::resolvePeerAddresses(ipv4, "127.0.0.1", true).size() == 1);
	assert(con_sctp::resolvePeerAddresses(ipv4, "::1", false).size() == 1);
	assert(con_sctp::resolvePeerAddresses(ipv6, "", true) == std::vector<Address>{ipv6});

	for (const auto &addresses : {from6, from4}) {
		const auto packed = con_sctp::packPeerAddresses(addresses);
		assert(packed.size() == sizeof(sockaddr_in) + sizeof(sockaddr_in6));
		size_t offset = 0;
		for (const auto &address : addresses) {
			if (address.isIPv6()) {
				sockaddr_in6 decoded;
				std::memcpy(&decoded, packed.data() + offset, sizeof(decoded));
				assert(Address(decoded) == address);
				offset += sizeof(decoded);
			} else {
				sockaddr_in decoded;
				std::memcpy(&decoded, packed.data() + offset, sizeof(decoded));
				assert(Address(decoded) == address);
				offset += sizeof(decoded);
			}
		}
		assert(offset == packed.size());
	}
	std::cout << "PASS: DNS alternatives, selected address order, deduplication, "
				 "IPv6 setting, and packed connectx addresses\n";
}
