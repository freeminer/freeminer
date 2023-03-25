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

#pragma once

#include "irr_v3d.h"
#include "lua_api/l_base.h"

class ServerMap;

class NodeTimerRef : public ModApiBase
{
private:
	v3s16 m_p;
	ServerMap *m_map;

	static const luaL_Reg methods[];

	static int gc_object(lua_State *L);

	static int l_set(lua_State *L);

	static int l_start(lua_State *L);

	static int l_stop(lua_State *L);

	static int l_is_started(lua_State *L);

	static int l_get_timeout(lua_State *L);

	static int l_get_elapsed(lua_State *L);

public:
	NodeTimerRef(v3s16 p, ServerMap *map) : m_p(p), m_map(map) {}
	~NodeTimerRef() = default;

	// Creates an NodeTimerRef and leaves it on top of stack
	// Not callable from Lua; all references are created on the C side.
	static void create(lua_State *L, v3s16 p, ServerMap *map);

	static void Register(lua_State *L);

	static const char className[];
};
