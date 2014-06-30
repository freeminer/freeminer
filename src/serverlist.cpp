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
#include <sstream>
#include <algorithm>

#include "version.h"
#include "main.h" // for g_settings
#include "settings.h"
#include "serverlist.h"
#include "filesys.h"
#include "porting.h"
#include "log.h"
#include "json/json.h"
#include "convert_json.h"
#include "httpfetch.h"
#include "util/string.h"

namespace ServerList
{

std::string getFilePath()
{
	std::string serverlist_file = g_settings->get("serverlist_file");

	std::string dir_path = std::string("client") + DIR_DELIM;
	fs::CreateDir(porting::path_user + DIR_DELIM + dir_path);
	return porting::path_user + DIR_DELIM + dir_path + serverlist_file;
}


std::vector<ServerListSpec> getLocal()
{
	std::string path = ServerList::getFilePath();
	std::string liststring;
	if (fs::PathExists(path)) {
		std::ifstream istream(path.c_str());
		if (istream.is_open()) {
			std::ostringstream ostream;
			ostream << istream.rdbuf();
			liststring = ostream.str();
			istream.close();
		}
	}

	return deSerialize(liststring);
}


std::vector<ServerListSpec> getOnline()
{
	Json::Value root = fetchJsonValue(
			(g_settings->get("serverlist_url") + "/list").c_str(), NULL);

	std::vector<ServerListSpec> serverlist;

	if (root.isArray()) {
		for (unsigned int i = 0; i < root.size(); i++) {
			if (root[i].isObject()) {
				serverlist.push_back(root[i]);
			}
		}
	}

	return serverlist;
}


// Delete a server from the local favorites list
bool deleteEntry(const ServerListSpec &server)
{
	std::vector<ServerListSpec> serverlist = ServerList::getLocal();
	for (std::vector<ServerListSpec>::iterator it = serverlist.begin();
			it != serverlist.end();) {
		if ((*it)["address"] == server["address"] &&
				(*it)["port"] == server["port"]) {
			it = serverlist.erase(it);
		} else {
			++it;
		}
	}

	std::string path = ServerList::getFilePath();
	std::ostringstream ss(std::ios_base::binary);
	ss << ServerList::serialize(serverlist);
	if (!fs::safeWriteToFile(path, ss.str()))
		return false;
	return true;
}

// Insert a server to the local favorites list
bool insert(const ServerListSpec &server)
{
	// Remove duplicates
	ServerList::deleteEntry(server);

	std::vector<ServerListSpec> serverlist = ServerList::getLocal();

	// Insert new server at the top of the list
	serverlist.insert(serverlist.begin(), server);

	std::string path = ServerList::getFilePath();
	std::ostringstream ss(std::ios_base::binary);
	ss << ServerList::serialize(serverlist);
	if (!fs::safeWriteToFile(path, ss.str()))
		return false;

	return true;
}

std::vector<ServerListSpec> deSerialize(const std::string &liststring)
{
	std::vector<ServerListSpec> serverlist;
	Json::Value root;
	Json::Reader reader;
	std::istringstream stream(liststring);
	if (!liststring.size()) {
		return serverlist;
	}
	if (!reader.parse( stream, root ) )
	{
		errorstream  << "Failed to parse server list " << reader.getFormattedErrorMessages();
		return serverlist;
	}
	if (root["list"].isArray())
		for (unsigned int i = 0; i < root["list"].size(); i++)
			if (root["list"][i].isObject())
				serverlist.push_back(root["list"][i]);
	return serverlist;
}

const std::string serialize(const std::vector<ServerListSpec> &serverlist)
{
	Json::Value root;
	Json::Value list(Json::arrayValue);
	for (std::vector<ServerListSpec>::const_iterator it = serverlist.begin();
			it != serverlist.end();
			it++) {
		list.append(*it);
	}
	root["list"] = list;
	Json::FastWriter writer;
	return writer.write(root);
}


#if USE_CURL
void sendAnnounce(const std::string &action,
		const std::vector<std::string> &clients_names,
		const double uptime,
		const u32 game_time,
		const float lag,
		const std::string &gameid,
		const std::vector<ModSpec> &mods)
{
	Json::Value server;
	server["action"] = action;
	server["port"]    = g_settings->getU16("port");
	if (g_settings->exists("server_address")) {
		server["address"] = g_settings->get("server_address");
	}
	if (action != "delete") {
		server["name"]         = g_settings->get("server_name");
		server["description"]  = g_settings->get("server_description");
		server["version"]      = minetest_version_simple;
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
		for (std::vector<std::string>::const_iterator it = clients_names.begin();
				it != clients_names.end();
				++it) {
			server["clients_list"].append(*it);
		}
		if (gameid != "") server["gameid"] = gameid;
	}

	if (action == "start") {
		server["dedicated"]         = g_settings->getBool("server_dedicated");
		server["rollback"]          = g_settings->getBool("enable_rollback_recording");
		server["mapgen"]            = g_settings->get("mg_name");
		server["privs"]             = g_settings->get("default_privs");
		server["can_see_far_names"] = g_settings->getBool("unlimited_player_transfer_distance");
		server["liquid_finite"]	= g_settings->getBool("liquid_real");
		server["mods"]              = Json::Value(Json::arrayValue);
		for (std::vector<ModSpec>::const_iterator it = mods.begin();
				it != mods.end();
				++it) {
			server["mods"].append(it->name);
		}
		actionstream << "Announcing to " << g_settings->get("serverlist_url") << std::endl;
	} else {
		if (lag)
			server["lag"] = lag;
	}

	Json::FastWriter writer;
	HTTPFetchRequest fetchrequest;
	fetchrequest.url = g_settings->get("serverlist_url") + std::string("/announce");
	fetchrequest.post_fields["json"] = writer.write(server);
	fetchrequest.multipart = true;
	httpfetch_async(fetchrequest);
}
#endif

} //namespace ServerList
