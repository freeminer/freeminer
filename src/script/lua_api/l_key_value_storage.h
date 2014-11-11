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

#ifndef L_KEY_VALUE_STORAGE_H
#define L_KEY_VALUE_STORAGE_H

#include "lua_api/l_base.h"

class ModApiKeyValueStorage : public ModApiBase
{
public:
	static void Initialize(lua_State *L, int top);
private:
	static int l_kv_put_string(lua_State *L);
	static int l_kv_get_string(lua_State *L);
	static int l_kv_delete(lua_State *L);
	static int l_stat_get(lua_State *L);
	static int l_stat_add(lua_State *L);
};

#endif
