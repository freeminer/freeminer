/*
script/lua_api/l_nodetimer.h
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

#ifndef L_NODETIMER_H_
#define L_NODETIMER_H_

#include "lua_api/l_base.h"
#include "irr_v3d.h"

class ServerEnvironment;

class NodeTimerRef : public ModApiBase {
private:
	v3s16 m_p;
	ServerEnvironment *m_env;

	static const char className[];
	static const luaL_reg methods[];

	static int gc_object(lua_State *L);

	static NodeTimerRef *checkobject(lua_State *L, int narg);

	static int l_set(lua_State *L);

	static int l_start(lua_State *L);

	static int l_stop(lua_State *L);

	static int l_is_started(lua_State *L);

	static int l_get_timeout(lua_State *L);

	static int l_get_elapsed(lua_State *L);

public:
	NodeTimerRef(v3s16 p, ServerEnvironment *env);
	~NodeTimerRef();

	// Creates an NodeTimerRef and leaves it on top of stack
	// Not callable from Lua; all references are created on the C side.
	static void create(lua_State *L, v3s16 p, ServerEnvironment *env);

	static void set_null(lua_State *L);

	static void Register(lua_State *L);
};



#endif /* L_NODETIMER_H_ */
