// Luanti
// SPDX-License-Identifier: LGPL-2.1-or-later
// Copyright (C) 2013 celeron55, Perttu Ahola <celeron55@gmail.com>

#include "network/fm_connection_use.h"

#include "version.h"
#include "config.h"

#if USE_CMAKE_CONFIG_H
	#include "cmake_config_githash.h"
#endif

#ifndef VERSION_GITHASH
	#define VERSION_GITHASH VERSION_STRING
#endif

#define STRINGIFY(x) #x
#define STR(x) STRINGIFY(x)

const char *g_version_string = VERSION_STRING;
const char *g_version_hash = VERSION_GITHASH;
const char *g_build_info =
	"BUILD_TYPE=" BUILD_TYPE "\n"
	"RUN_IN_PLACE=" STR(RUN_IN_PLACE) "\n"
	"USE_CURL=" STR(USE_CURL) "\n"
#if CHECK_CLIENT_BUILD()
	"USE_GETTEXT=" STR(USE_GETTEXT) "\n"
	"USE_SOUND=" STR(USE_SOUND) "\n"
#endif
	"STATIC_SHAREDIR=" STR(STATIC_SHAREDIR)
#if USE_GETTEXT && defined(STATIC_LOCALEDIR)
	"\n" "STATIC_LOCALEDIR=" STR(STATIC_LOCALEDIR)
#endif

#if	ENABLE_THREADS
	"\n" "ENABLE_THREADS"
#endif
#if	USE_GPERF
	"\n" "USE_GPERF"
#endif
#if	USE_MULTI
	"\n" "USE_MULTI"
#endif
#if	USE_ENET
	"\n" "USE_ENET"
#endif
#if	USE_SCTP
	"\n" "USE_SCTP"
#endif
#if	USE_WEBSOCKET
	"\n" "USE_WEBSOCKET"
#endif
#if	USE_WEBSOCKET_SCTP
	"\n" "USE_WEBSOCKET_SCTP"
#endif
#if	MINETEST_PROTO
	"\n" "MINETEST_PROTO"
#endif
#if	MINETEST_TRANSPORT
	"\n" "MINETEST_TRANSPORT"
#endif
	"\n" "USE_TRANSPORT=" STR(USE_TRANSPORT)
;
