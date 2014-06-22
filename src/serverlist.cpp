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

	std::string dir_path = std::string("client") + DIR_DELIM
		;
	fs::CreateDir(porting::path_user + DIR_DELIM + dir_path);
	std::string rel_path = dir_path + serverlist_file;
	std::string path = porting::path_user + DIR_DELIM + rel_path;
	return path;
}

std::vector<ServerListSpec> getLocal()
{
	std::string path = ServerList::getFilePath();
	std::string liststring;
	if(fs::PathExists(path))
	{
		std::ifstream istream(path.c_str());
		if(istream.is_open())
		{
			std::ostringstream ostream;
			ostream << istream.rdbuf();
			liststring = ostream.str();
			istream.close();
		}
	}

	return ServerList::deSerialize(liststring);
}


std::vector<ServerListSpec> getOnline()
{
	Json::Value root = fetchJsonValue((g_settings->get("serverlist_url")+"/list").c_str(), NULL);

	std::vector<ServerListSpec> serverlist;

	if (root.isArray()) {
		for (unsigned int i = 0; i < root.size(); i++)
		{
			if (root[i].isObject()) {
				serverlist.push_back(root[i]);
			}
		}
	}

	return serverlist;
}

/*
	Delete a server fromt he local favorites list
*/
bool deleteEntry (ServerListSpec server)
{
	std::vector<ServerListSpec> serverlist = ServerList::getLocal();
	for(unsigned i = 0; i < serverlist.size(); i++)
	{
		if  (serverlist[i]["address"] == server["address"]
		&&   serverlist[i]["port"]    == server["port"])
		{
			serverlist.erase(serverlist.begin() + i);
		}
	}

	std::string path = ServerList::getFilePath();
	std::ostringstream ss(std::ios_base::binary);
	ss << ServerList::serialize(serverlist);
	if (!fs::safeWriteToFile(path, ss.str()))
		return false;
	return true;
}

/*
	Insert a server to the local favorites list
*/
bool insert (ServerListSpec server)
{
	// Remove duplicates
	ServerList::deleteEntry(server);

	std::vector<ServerListSpec> serverlist = ServerList::getLocal();

	// Insert new server at the top of the list
	serverlist.insert(serverlist.begin(), server);

	std::string path = ServerList::getFilePath();
	std::ostringstream ss(std::ios_base::binary);
	ss << ServerList::serialize(serverlist);
	fs::safeWriteToFile(path, ss.str());

	return false;
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
	{
		if (root["list"][i].isObject()) {
			serverlist.push_back(root["list"][i]);
		}
	}
	return serverlist;
}

std::string serialize(std::vector<ServerListSpec> &serverlist)
{
	Json::Value root;
	Json::Value list(Json::arrayValue);
	for(std::vector<ServerListSpec>::iterator i = serverlist.begin(); i != serverlist.end(); i++)
	{
		list.append(*i);
	}
	root["list"] = list;
	Json::StyledWriter writer;
	return writer.write( root );
}


#if USE_CURL
void sendAnnounce(std::string action, const std::vector<std::string> & clients_names, double uptime, u32 game_time, float lag, std::string gameid, std::vector<ModSpec> mods) {
	Json::Value server;
	if (action.size())
		server["action"]	= action;
	server["port"]		= g_settings->get("port");
	server["address"]	= g_settings->get("server_address");
	if (action != "delete") {
		server["name"]		= g_settings->get("server_name");
		server["description"]	= g_settings->get("server_description");
		server["version"]	= minetest_version_simple;
		server["url"]		= g_settings->get("server_url");
		server["creative"]	= g_settings->get("creative_mode");
		server["damage"]	= g_settings->get("enable_damage");
		server["password"]	= g_settings->getBool("disallow_empty_password");
		server["pvp"]		= g_settings->getBool("enable_pvp");
		server["clients"]	= (int)clients_names.size();
		server["clients_max"]	= g_settings->get("max_users");
		server["clients_list"]	= Json::Value(Json::arrayValue);
		for(u32 i = 0; i < clients_names.size(); ++i) {
			server["clients_list"].append(clients_names[i]);
		}
		if (uptime >= 1)	server["uptime"]	= (int)uptime;
		if (gameid != "")	server["gameid"]	= gameid;
		if (game_time >= 1)	server["game_time"]	= game_time;
	}

	if(server["action"] == "start") {
		server["dedicated"]	= g_settings->get("server_dedicated");
		server["privs"]		= g_settings->get("default_privs");
		server["rollback"]	= g_settings->getBool("enable_rollback_recording");
		server["liquid_finite"]	= g_settings->getBool("liquid_real");
		server["mapgen"]	= g_settings->get("mg_name");
		server["can_see_far_names"]	= g_settings->getBool("unlimited_player_transfer_distance");
		server["mods"]		= Json::Value(Json::arrayValue);
		for(std::vector<ModSpec>::iterator m = mods.begin(); m != mods.end(); m++) {
			server["mods"].append(m->name);
		}
		actionstream << "announcing to " << g_settings->get("serverlist_url") << std::endl;
	} else {
		if (lag)
			server["lag"]	= lag;
	}

	Json::FastWriter writer;
	HTTPFetchRequest fetchrequest;
	fetchrequest.url = g_settings->get("serverlist_url") + std::string("/announce");
	std::string query = std::string("json=") + urlencode(writer.write(server));
	if (query.size() < 1000)
		fetchrequest.url += "?" + query;
	else
		fetchrequest.post_fields = query;
	httpfetch_async(fetchrequest);
}
#endif

} //namespace ServerList
