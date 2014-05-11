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

#define GET_ENV_PTR ServerEnvironment* env =                                   \
				dynamic_cast<ServerEnvironment*>(getEnv(L));                   \
				if( env == NULL) return 0

void ModApiKeyValueStorage::Initialize(lua_State *L, int top)
{
	API_FCT(kv_put_string);
	API_FCT(kv_get_string);
	API_FCT(kv_delete);
}

int ModApiKeyValueStorage::l_kv_put_string(lua_State *L)
{
	GET_ENV_PTR;
	
	const char *key = luaL_checkstring(L, 1);
	const char *data = luaL_checkstring(L, 2);
	env->getKeyValueStorage()->put(key, data);

	return 0;
}

int ModApiKeyValueStorage::l_kv_get_string(lua_State *L)
{
	GET_ENV_PTR;

	const char *key = luaL_checkstring(L, 1);
	std::string data;
	if(env->getKeyValueStorage()->get(key, data)) {
		lua_pushstring(L, data.c_str());
		return 1;
	} else {
		return 0;
	}
}

int ModApiKeyValueStorage::l_kv_delete(lua_State *L)
{
	GET_ENV_PTR;

	const char *key = luaL_checkstring(L, 1);
	env->getKeyValueStorage()->del(key);

	return 0;
}
