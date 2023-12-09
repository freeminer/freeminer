/*
script/scripting_game.h
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
#include "cpp_api/s_base.h"
#include "cpp_api/s_entity.h"
#include "cpp_api/s_env.h"
#include "cpp_api/s_inventory.h"
#include "cpp_api/s_modchannels.h"
#include "cpp_api/s_node.h"
#include "cpp_api/s_player.h"
#include "cpp_api/s_server.h"
#include "cpp_api/s_security.h"
#include "cpp_api/s_async.h"

struct PackedValue;

/*****************************************************************************/
/* Scripting <-> Server Game Interface                                       */
/*****************************************************************************/

class ServerScripting:
		virtual public ScriptApiBase,
		public ScriptApiDetached,
		public ScriptApiEntity,
		public ScriptApiEnv,
		public ScriptApiModChannels,
		public ScriptApiNode,
		public ScriptApiPlayer,
		public ScriptApiServer,
		public ScriptApiSecurity
{
public:
	ServerScripting(Server* server);

	// use ScriptApiBase::loadMod() to load mods

	// Save globals that are copied into other Lua envs
	void saveGlobals();

	// Initialize async engine, call this AFTER loading all mods
	void initAsync();

	// Global step handler to collect async results
	void stepAsync();

	// Pass job to async threads
	u32 queueAsync(std::string &&serialized_func,
		PackedValue *param, const std::string &mod_origin);

private:
	void InitializeModApi(lua_State *L, int top);

	static void InitializeAsync(lua_State *L, int top);

	AsyncEngine asyncEngine;
};
