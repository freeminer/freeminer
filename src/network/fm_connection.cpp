#include "fm_connection.h"
#include "network/multi/connection.h"

namespace con
{

std::string_view getProtocolName(IConnection &connection, session_t peer_id)
{
#if USE_MULTI
	if (auto multi = dynamic_cast<ConnectionMulti *>(&connection))
		return multi->getProtocolName(peer_id);
	return "unknown";
#else
	(void)connection;
	(void)peer_id;
#if USE_ENET
	return "enet";
#elif USE_SCTP
	return "sctp";
#elif USE_WEBSOCKET_SCTP
	return "ws_sctp";
#elif USE_WEBSOCKET
	return "ws";
#elif MINETEST_TRANSPORT
	return "mt";
#else
	return "unknown";
#endif
#endif
}

}