/*
serverlist.cpp
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
#include "json/value.h"
#include "version.h"
//#include <fstream>
#include "settings.h"
#include "serverlist.h"
#include "filesys.h"
#include "log.h"
#include "network/networkprotocol.h"
#include <json/json.h>
#include "convert_json.h"
#include "httpfetch.h"
#include "config.h"

namespace ServerList
{
void sendAnnounce(AnnounceAction action,
		const u16 port,
		const std::vector<std::string> &clients_names,
		const double uptime,
		const u32 game_time,
		const float lag,
		const std::string &gameid,
		const std::string &mg_name,
		const std::vector<ModSpec> &mods,
		bool dedicated)
{
#if USE_CURL
	static const char *aa_names[] = {"start", "update", "delete"};
	Json::Value server;
	server["action"] = aa_names[action];
	server["port"] = port;
	if (g_settings->exists("server_address")) {
		server["address"] = g_settings->get("server_address");
	}
	if (action != AA_DELETE) {
		bool strict_checking = g_settings->getBool("strict_protocol_version_checking");
		server["name"]         = g_settings->get("server_name");
		server["description"]  = g_settings->get("server_description");
		server["version"]      = g_version_string;
		server["proto_min"]    = strict_checking ? LATEST_PROTOCOL_VERSION : SERVER_PROTOCOL_VERSION_MIN;
		server["proto_max"]    = strict_checking ? LATEST_PROTOCOL_VERSION : SERVER_PROTOCOL_VERSION_MAX;
		server["url"]          = g_settings->get("server_url");
		server["creative"]     = g_settings->getBool("creative_mode");
		server["damage"]       = g_settings->getBool("enable_damage");
		server["password"]     = g_settings->getBool("disallow_empty_password");
		server["pvp"]          = g_settings->getBool("enable_pvp");
		if (uptime >= 1)
			server["uptime"]   = (int) uptime;
		if (game_time >= 1)
			server["game_time"]= game_time;
		server["clients"]      = (int) clients_names.size();
		server["clients_max"]  = g_settings->getU16("max_users");
		server["clients_list"] = Json::Value(Json::arrayValue);
		for (const std::string &clients_name : clients_names) {
			server["clients_list"].append(clients_name);
		}
		if (!gameid.empty())
			server["gameid"] = gameid;
		server["proto"]        = g_settings->get("server_proto");
	}

	if (action == AA_START) {
		server["dedicated"]         = dedicated;
		server["rollback"]          = g_settings->getBool("enable_rollback_recording");
		server["mapgen"]            = mg_name;
		server["privs"]             = g_settings->getBool("creative_mode") ? g_settings->get("default_privs_creative") : g_settings->get("default_privs");
		server["can_see_far_names"] = g_settings->getS16("player_transfer_distance") <= 0;
		server["liquid_real"]       = g_settings->getBool("liquid_real");
		server["version_hash"]      = g_version_hash;
		server["mods"]              = Json::Value(Json::arrayValue);
		for (const ModSpec &mod : mods) {
			server["mods"].append(mod.name);
		}
	} else if (action == AA_UPDATE) {
		if (lag)
			server["lag"] = lag;
	}

	if (action == AA_START) {
		actionstream << "Announcing " << aa_names[action] << " to " <<
			g_settings->get("serverlist_url") << std::endl;
	} else {
		infostream << "Announcing " << aa_names[action] << " to " <<
			g_settings->get("serverlist_url") << std::endl;
	}

	HTTPFetchRequest fetch_request;
	fetch_request.caller = HTTPFETCH_PRINT_ERR;
	fetch_request.url = g_settings->get("serverlist_url") + std::string("/announce");
	fetch_request.method = HTTP_POST;

	fetch_request.timeout = fetch_request.connect_timeout = 59000;
#if !MINETEST_PROTO || !MINETEST_TRANSPORT
	// todo: need to patch masterserver script to parse multipart posts
	std::string query = std::string("json=") + urlencode(fastWriteJson(server));
	if (query.size() < 1000)
		fetch_request.url += "?" + query;
	else
		fetch_request.raw_data = query;
#else
	fetch_request.fields["json"] = fastWriteJson(server);
	fetch_request.multipart = true;
#endif

	httpfetch_async(fetch_request);
#endif
}


lan_adv lan_adv_client;

void lan_get() {
	if (!g_settings->getBool("serverlist_lan"))
		return;
	lan_adv_client.ask();
}

bool lan_fresh() {
	auto result = lan_adv_client.fresh.load();
	lan_adv_client.fresh = false;
	return result;
}


} // namespace ServerList
