/*
script/lua_api/l_internal.h
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

/******************************************************************************/
/******************************************************************************/
/* WARNING!!!! do NOT add this header in any include file or any code file    */
/*             not being a modapi file!!!!!!!!                                */
/******************************************************************************/
/******************************************************************************/

#pragma once

#include "common/c_internal.h"

#define luamethod(class, name) {#name, class::l_##name}
<<<<<<< HEAD
#define luamethod_aliased(class, name, alias) {#name, class::l_##name}, {#alias, class::l_##name}
#define API_FCT(name) registerFunction(L, #name, l_##name,top)
#define ASYNC_API_FCT(name) engine.registerFunction(#name, l_##name)
=======
>>>>>>> 5.5.0

#define luamethod_dep(class, good, bad)                                     \
		{#bad, [](lua_State *L) -> int {                                    \
			return l_deprecated_function(L, #good, #bad, &class::l_##good); \
		}}

<<<<<<< HEAD
/*
#if (defined(WIN32) || defined(_WIN32) || defined(_WIN32_WCE))
	#define NO_MAP_LOCK_REQUIRED
=======
#define luamethod_aliased(class, good, bad) \
		luamethod(class, good),               \
		luamethod_dep(class, good, bad)

#define API_FCT(name) registerFunction(L, #name, l_##name, top)

// For future use
#define MAP_LOCK_REQUIRED ((void)0)
#define NO_MAP_LOCK_REQUIRED ((void)0)

/* In debug mode ensure no code tries to retrieve the server env when it isn't
 * actually available (in CSM) */
#if !defined(SERVER) && !defined(NDEBUG)
#define DEBUG_ASSERT_NO_CLIENTAPI                    \
	FATAL_ERROR_IF(getClient(L) != nullptr, "Tried " \
		"to retrieve ServerEnvironment on client")
>>>>>>> 5.5.0
#else
#define DEBUG_ASSERT_NO_CLIENTAPI ((void)0)
#endif

// Retrieve ServerEnvironment pointer as `env` (no map lock)
#define GET_ENV_PTR_NO_MAP_LOCK                              \
	DEBUG_ASSERT_NO_CLIENTAPI;                               \
	ServerEnvironment *env = (ServerEnvironment *)getEnv(L); \
	if (env == NULL)                                         \
		return 0

// Retrieve ServerEnvironment pointer as `env`
#define GET_ENV_PTR         \
	MAP_LOCK_REQUIRED;      \
	GET_ENV_PTR_NO_MAP_LOCK

// Retrieve Environment pointer as `env` (no map lock)
#define GET_PLAIN_ENV_PTR_NO_MAP_LOCK            \
	Environment *env = (Environment *)getEnv(L); \
	if (env == NULL)                             \
		return 0

// Retrieve Environment pointer as `env`
#define GET_PLAIN_ENV_PTR         \
	MAP_LOCK_REQUIRED;            \
	GET_PLAIN_ENV_PTR_NO_MAP_LOCK
