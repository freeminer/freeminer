// Minetest
// SPDX-License-Identifier: LGPL-2.1-or-later

#include "network/connection.h"

#if USE_MULTI
#include "network/multi/connection.h"
#elif USE_ENET
#include "network/enet/connection.h"
#elif USE_SCTP
#include "network/sctp/connection.h"
#elif MINETEST_TRANSPORT
#include "network/mtp/impl.h"
#endif

#include "config.h"

namespace con
{

IConnection *createMTP(float timeout, bool ipv6, PeerHandler *handler)
{
	// safe minimum across internet networks for ipv4 and ipv6
	constexpr u32 MAX_PACKET_SIZE = 1400; // 512;
#if USE_MULTI
	return new con::ConnectionMulti(MAX_PACKET_SIZE, timeout, ipv6, handler);
#elif USE_ENET
	return new con::ConnectionEnet(MAX_PACKET_SIZE, timeout, ipv6, handler);
#elif USE_SCTP
	return new con::ConnectionSctp(MAX_PACKET_SIZE, timeout, ipv6, handler);
#elif MINETEST_TRANSPORT
	return new con::Connection(MAX_PACKET_SIZE, timeout, ipv6, handler);
#endif

}


}
