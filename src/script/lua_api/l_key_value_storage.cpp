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

#include "lua_api/l_internal.h"
#include "lua_api/l_key_value_storage.h"
#include "environment.h"
#include "key_value_storage.h"

void ModApiKeyValueStorage::Initialize(lua_State *L, int top)
{
	API_FCT(kv_put_string);
	API_FCT(kv_get_string);
	API_FCT(kv_delete);
	API_FCT(stat_get);
	API_FCT(stat_add);
}

int ModApiKeyValueStorage::l_kv_put_string(lua_State *L)
{
	GET_ENV_PTR_NO_MAP_LOCK;

	std::string key = luaL_checkstring(L, 1);
	std::string data = luaL_checkstring(L, 2);

	std::string db;
	if (lua_isstring(L, 3))
		db = luaL_checkstring(L, 3);
	env->getKeyValueStorage(db).put(key, data);

	return 0;
}

int ModApiKeyValueStorage::l_kv_get_string(lua_State *L)
{
	GET_ENV_PTR_NO_MAP_LOCK;

	std::string key = luaL_checkstring(L, 1);
	std::string db;
	if (lua_isstring(L, 2))
		db = luaL_checkstring(L, 2);
	std::string data;
	if(env->getKeyValueStorage(db).get(key, data)) {
		lua_pushstring(L, data.c_str());
		return 1;
	} else {
		return 0;
	}
}

int ModApiKeyValueStorage::l_kv_delete(lua_State *L)
{
	GET_ENV_PTR_NO_MAP_LOCK;

	std::string key = luaL_checkstring(L, 1);
	std::string db;
	if (lua_isstring(L, 2))
		db = luaL_checkstring(L, 2);
	env->getKeyValueStorage(db).del(key);

	return 0;
}


// bad place, todo: move to l_stat.h
#include "server.h"
#include "stat.h"

int ModApiKeyValueStorage::l_stat_get(lua_State *L)
{
	GET_ENV_PTR_NO_MAP_LOCK;

	std::string key = luaL_checkstring(L, 1);
	lua_pushnumber(L, getServer(L)->stat.get(key));
	return 1;
}

int ModApiKeyValueStorage::l_stat_add(lua_State *L)
{
	GET_ENV_PTR_NO_MAP_LOCK;

	std::string key = luaL_checkstring(L, 1);

	std::string name;
	if(lua_isstring(L, 2))
		name = lua_tostring(L, 1);

	float value = 1;
	if(lua_isnumber(L, 3))
		value = lua_tonumber(L, 3);

	getServer(L)->stat.add(key, name, value);
	return 0;
}

