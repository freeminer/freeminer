/*
Copyright (C) 2023 proller <proler@gmail.com>
*/

#pragma once

#include "config.h"

#if USE_SCTP

#include "debug/dump.h"
#include "external/usrsctp/usrsctplib/usrsctp.h"
#include "log.h"
#include "network/networkexceptions.h"
#include "network/networkpacket.h"
#include "network/networkprotocol.h"
#include "network/sctp/connection.h"
#include "porting.h"
#include "profiler.h"
#include "serialization.h"
#include "settings.h"
#include "util/numeric.h"
#include "util/serialize.h"
#include "util/string.h"

#include <cerrno>
#include <cstdarg>
#include <cstdio>
#include <string_view>
#include <sys/socket.h>
#include <unistd.h>

#ifdef __EMSCRIPTEN__
#include <emsocket.h>
#endif

namespace con_sctp
{

#if SCTP_DEBUG
static auto &cs = errorstream; // remove after debug
#else
static auto &cs = verbosestream; // remove after debug
#endif

} // namespace con_sctp

#endif
