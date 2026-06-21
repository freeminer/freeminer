/*
Copyright (C) 2023 proller <proler@gmail.com>
*/

#pragma once

#include "config.h"

#if USE_WEBSOCKET_SCTP

#include "debug/dump.h"
#include "external/usrsctp/usrsctplib/usrsctp.h"
#include "filesys.h"
#include "log.h"
#include "network/networkpacket.h"
#include "network/networkprotocol.h"
#include "network/sctp/connection.h"
#include "network/ws_sctp/connection.h"
#include "porting.h"
#include "profiler.h"
#include "serialization.h"
#include "settings.h"
#include "util/numeric.h"
#include "util/serialize.h"
#include "util/string.h"

#include <algorithm>
#include <cerrno>
#include <cctype>
#include <cstdarg>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <memory>
#include <string>
#include <sys/socket.h>
#include <system_error>

#include <websocketpp/client.hpp>
#include <websocketpp/config/debug_asio_no_tls.hpp>
#include <websocketpp/logger/syslog.hpp>
#include <websocketpp/server.hpp>

namespace con_ws_sctp
{

#define WS_DEBUG 1
#if WS_DEBUG
static auto &cs = errorstream; // remove after debug
#else
static auto &cs = verbosestream; // remove after debug
#endif

} // namespace con_ws_sctp

#endif
