/*
script/lua_api/l_base.cpp
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

#include "lua_api/l_base.h"
#include "lua_api/l_internal.h"
#include "cpp_api/s_base.h"

ScriptApiBase* ModApiBase::getScriptApiBase(lua_State *L) {
	// Get server from registry
	lua_getfield(L, LUA_REGISTRYINDEX, "scriptapi");
	ScriptApiBase *sapi_ptr = (ScriptApiBase*) lua_touserdata(L, -1);
	lua_pop(L, 1);
	return sapi_ptr;
}

Server* ModApiBase::getServer(lua_State *L) {
	return getScriptApiBase(L)->getServer();
}

Environment* ModApiBase::getEnv(lua_State *L) {
	return getScriptApiBase(L)->getEnv();
}

GUIEngine* ModApiBase::getGuiEngine(lua_State *L) {
	return getScriptApiBase(L)->getGuiEngine();
}

bool ModApiBase::registerFunction(lua_State *L,
		const char *name,
		lua_CFunction fct,
		int top
		) {
	//TODO check presence first!

	lua_pushstring(L,name);
	lua_pushcfunction(L,fct);
	lua_settable(L, top);

	return true;
}
