/*
script/lua_api/l_base.h
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

#pragma once

#include "common/c_types.h"
#include "common/c_internal.h"
#include "common/helper.h"
#include "gamedef.h"
#include <unordered_map>

extern "C" {
#include <lua.h>
#include <lauxlib.h>
}

#ifndef SERVER
class Client;
class GUIEngine;
#endif

class ScriptApiBase;
class Server;
class Environment;
class ServerInventoryManager;

class ModApiBase : protected LuaHelper {
public:
	static ScriptApiBase*   getScriptApiBase(lua_State *L);
	static Server*          getServer(lua_State *L);
	static ServerInventoryManager *getServerInventoryMgr(lua_State *L);
	#ifndef SERVER
	static Client*          getClient(lua_State *L);
	static GUIEngine*       getGuiEngine(lua_State *L);
	#endif // !SERVER

	static IGameDef*        getGameDef(lua_State *L);

	static Environment*     getEnv(lua_State *L);

	// When we are not loading the mod, this function returns "."
	static std::string      getCurrentModPath(lua_State *L);

	// Get an arbitrary subclass of ScriptApiBase
	// by using dynamic_cast<> on getScriptApiBase()
	template<typename T>
	static T* getScriptApi(lua_State *L) {
		ScriptApiBase *scriptIface = getScriptApiBase(L);
		T *scriptIfaceDowncast = dynamic_cast<T*>(scriptIface);
		if (!scriptIfaceDowncast) {
			throw LuaError("Requested unavailable ScriptApi - core engine bug!");
		}
		return scriptIfaceDowncast;
	}

	static bool registerFunction(lua_State *L,
			const char* name,
			lua_CFunction func,
			int top);

	static void registerClass(lua_State *L, const char *name,
			const luaL_Reg *methods,
			const luaL_Reg *metamethods);

	template<typename T>
	static inline T *checkObject(lua_State *L, int narg)
	{
		return *reinterpret_cast<T**>(luaL_checkudata(L, narg, T::className));
	}

	/**
	 * A wrapper for deprecated functions.
	 *
	 * When called, handles the deprecation according to user settings and then calls `func`.
	 *
	 * @throws Lua Error if required by the user settings.
	 *
	 * @param L Lua state
	 * @param good Name of good function/method
	 * @param bad Name of deprecated function/method
	 * @param func Actual implementation of function
	 * @return value from `func`
	 */
	static int l_deprecated_function(lua_State *L, const char *good, const char *bad, lua_CFunction func);
};
