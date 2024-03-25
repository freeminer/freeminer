/*
version.cpp
Copyright (C) 2013 celeron55, Perttu Ahola <celeron55@gmail.com>
*/

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

#include "network/fm_connection_use.h"

#include "version.h"
#include "config.h"

#if USE_CMAKE_CONFIG_H
	#include "cmake_config_githash.h"
#endif

#ifndef VERSION_GITHASH
	#define VERSION_GITHASH VERSION_STRING
#endif

const char *g_version_string = VERSION_STRING;
const char *g_version_hash = VERSION_GITHASH;
const char *g_build_info =
	"BUILD_TYPE=" BUILD_TYPE "\n"
	"RUN_IN_PLACE=" STR(RUN_IN_PLACE) "\n"
	"USE_CURL=" STR(USE_CURL) "\n"
#ifndef SERVER
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
