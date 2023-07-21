/*
This file is part of Freeminer.

Freeminer is free software: you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation, either version 3 of the License, or
(at your option) any later version.

Freeminer  is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with Freeminer.  If not, see <http://www.gnu.org/licenses/>.
*/

#pragma once

#include "config.h"


#if USE_MULTI
#include "fm_connection_multi.h"
constexpr auto server_proto = "mt";
namespace con_use
{
using namespace con_multi;
}
#elif USE_WEBSOCKET
#include "ws/connection.h"
constexpr auto server_proto = "mt_ws";
namespace con_use
{
using namespace con_ws;
}
#elif USE_WEBSOCKET_SCTP
constexpr auto server_proto = "mt_ws_sctp";
#include "fm_connection_websocket_sctp.h"
namespace con_use
{
using namespace con_ws_sctp;
}
#elif USE_SCTP
constexpr auto server_proto = "mt_sctp";
#include "fm_connection_sctp.h"
namespace con_use
{
using namespace con_sctp;
}
#elif USE_ENET
constexpr auto server_proto = "mt_enet";
#include "fm_connection_enet.h"
namespace con_use
{
using namespace con_enet;
}
#else
constexpr auto server_proto = "mt";
#include "connection.h"
namespace con_use
{
using namespace con;
}
#endif
