/*
serverlist.h
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

#include <iostream>
#include "config.h"
#include "mods.h"
#include "json/json.h"

#ifndef SERVERLIST_HEADER
#define SERVERLIST_HEADER

typedef Json::Value ServerListSpec;

namespace ServerList
{
	std::vector<ServerListSpec> getLocal();
	std::vector<ServerListSpec> getOnline();
	bool deleteEntry(const ServerListSpec &server);
	bool insert(const ServerListSpec &server);
	std::vector<ServerListSpec> deSerialize(const std::string &liststring);
	const std::string serialize(const std::vector<ServerListSpec> &serverlist);
	void sendAnnounce(const std::string &action, const u16 port,
			const std::vector<std::string> &clients_names = std::vector<std::string>(),
			const double uptime = 0, const u32 game_time = 0,
			const float lag = 0, const std::string &gameid = "",
			const std::string &mg_name = "",
			const std::vector<ModSpec> &mods = std::vector<ModSpec>());
} // ServerList namespace

#endif
