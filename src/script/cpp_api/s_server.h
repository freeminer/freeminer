/*
script/cpp_api/s_server.h
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
#include <set>

class ScriptApiServer
		: virtual public ScriptApiBase
{
public:
	// Calls on_chat_message handlers
	// Returns true if script handled message
	bool on_chat_message(const std::string &name, const std::string &message);

	// Calls when mods are loaded
	void on_mods_loaded();

	// Calls on_shutdown handlers
	void on_shutdown();

	// Calls core.format_chat_message
	std::string formatChatMessage(const std::string &name,
		const std::string &message);

	/* auth */
	bool getAuth(const std::string &playername,
		std::string *dst_password,
		std::set<std::string> *dst_privs,
		s64 *dst_last_login = nullptr);
	void createAuth(const std::string &playername,
		const std::string &password);
	bool setPassword(const std::string &playername,
		const std::string &password);

	/* dynamic media handling */
	static u32 allocateDynamicMediaCallback(lua_State *L, int f_idx);
	void freeDynamicMediaCallback(u32 token);
	void on_dynamic_media_added(u32 token, const char *playername);

private:
	void getAuthHandler();
	void readPrivileges(int index, std::set<std::string> &result);
};
