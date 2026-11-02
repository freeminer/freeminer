
#include <string_view>
#include "network/networkprotocol.h"

namespace con
{
class IConnection;
std::string_view getProtocolName(IConnection &connection, session_t peer_id);

} // namespace
