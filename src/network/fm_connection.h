
#include <string_view>
#include "network/networkprotocol.h"

namespace con
{
class IConnection;
// fm:
std::string_view getProtocolName(IConnection &connection, session_t peer_id);
// ==

} // namespace
