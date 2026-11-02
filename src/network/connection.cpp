// Minetest
// SPDX-License-Identifier: LGPL-2.1-or-later

#include "network/connection.h"

#if USE_MULTI
#include "network/multi/connection.h"
#endif
#if USE_ENET
#include "network/enet/connection.h"
#endif
#if USE_WEBSOCKET_SCTP
#include "network/ws_sctp/connection.h"
#endif
#if USE_SCTP
#include "network/sctp/connection.h"
#endif
#if MINETEST_TRANSPORT
#include "network/mtp/impl.h"
#endif

#include "config.h"

namespace con
{

IConnection *createMTP(float timeout, bool ipv6, PeerHandler *handler, bool simple_singleplayer_mode)
{
	constexpr auto MAX_PACKET_SIZE_SINGLEPLAYER = 65000;

	// safe minimum across internet networks for ipv4 and ipv6
	u32 MAX_PACKET_SIZE = simple_singleplayer_mode ? MAX_PACKET_SIZE_SINGLEPLAYER :
#if __EMSCRIPTEN__
												   MAX_PACKET_SIZE_SINGLEPLAYER;
#elif defined(_WIN32)
												   1350; // TODO: find working maximum?
#else
												   1350;
#endif

#if defined(__EMSCRIPTEN__) && USE_WEBSOCKET_SCTP
	// Connect the embedded server and client through an in-process SCTP carrier.
	if (simple_singleplayer_mode)
		return new con_ws_sctp::Connection(
				0, MAX_PACKET_SIZE, timeout, ipv6, handler, true);
#endif

#if USE_MULTI
	return new con::ConnectionMulti(MAX_PACKET_SIZE, timeout, ipv6, handler);
#elif USE_WEBSOCKET_SCTP
	return new con_ws_sctp::Connection(
			0, MAX_PACKET_SIZE, timeout, ipv6, handler);
#elif USE_SCTP
	return new con_sctp::Connection(MAX_PACKET_SIZE, timeout, ipv6, handler);
#elif USE_ENET
	return new con::ConnectionEnet(MAX_PACKET_SIZE, timeout, ipv6, handler);
#elif MINETEST_TRANSPORT
												   return new con::Connection(
														   MAX_PACKET_SIZE, timeout, ipv6,
														   handler);
#endif
}

}
