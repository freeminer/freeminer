/*
script/lua_api/l_server.h
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

#ifndef L_SERVER_H_
#define L_SERVER_H_

#include "lua_api/l_base.h"

class ModApiServer : public ModApiBase {
private:
	// request_shutdown([message], [reconnect])
	static int l_request_shutdown(lua_State *L);

	// get_server_status()
	static int l_get_server_status(lua_State *L);

	// get_worldpath()
	static int l_get_worldpath(lua_State *L);

	// is_singleplayer()
	static int l_is_singleplayer(lua_State *L);

	// get_current_modname()
	static int l_get_current_modname(lua_State *L);

	// get_modpath(modname)
	static int l_get_modpath(lua_State *L);

	// get_modnames()
	// the returned list is sorted alphabetically for you
	static int l_get_modnames(lua_State *L);

	// print(text)
	static int l_print(lua_State *L);

	// chat_send_all(text)
	static int l_chat_send_all(lua_State *L);

	// chat_send_player(name, text)
	static int l_chat_send_player(lua_State *L);

	// show_formspec(playername,formname,formspec)
	static int l_show_formspec(lua_State *L);

	// sound_play(spec, parameters)
	static int l_sound_play(lua_State *L);

	// sound_stop(handle)
	static int l_sound_stop(lua_State *L);

	// get_player_privs(name, text)
	static int l_get_player_privs(lua_State *L);

	// get_player_ip()
	static int l_get_player_ip(lua_State *L);

	// get_player_information()
	static int l_get_player_information(lua_State *L);

	// get_ban_list()
	static int l_get_ban_list(lua_State *L);

	// get_ban_description()
	static int l_get_ban_description(lua_State *L);

	// ban_player()
	static int l_ban_player(lua_State *L);

	// unban_player_or_ip()
	static int l_unban_player_or_ip(lua_State *L);

	// kick_player(name, [message]) -> success
	static int l_kick_player(lua_State *L);

	// notify_authentication_modified(name)
	static int l_notify_authentication_modified(lua_State *L);

	// get_last_run_mod()
	static int l_get_last_run_mod(lua_State *L);

	// set_last_run_mod(modname)
	static int l_set_last_run_mod(lua_State *L);

#ifndef NDEBUG
	//  cause_error(type_of_error)
	static int l_cause_error(lua_State *L);
#endif

public:
	static void Initialize(lua_State *L, int top);

};

#endif /* L_SERVER_H_ */
