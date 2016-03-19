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

#include <fstream>
#include <iostream>
#include <sstream>
#include <algorithm>

#include "version.h"
//#include <fstream>
#include "settings.h"
#include "serverlist.h"
#include "filesys.h"
#include "porting.h"
#include "log.h"
#include "network/networkprotocol.h"
#include "json/json.h"
#include "convert_json.h"
#include "httpfetch.h"
#include "util/string.h"
#include "config.h"

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


std::vector<ServerListSpec> cached_online;

std::vector<ServerListSpec> getOnline()
{
	std::ostringstream geturl;

	u16 proto_version_min = g_settings->getFlag("send_pre_v25_init") ?
		CLIENT_PROTOCOL_VERSION_MIN_LEGACY : CLIENT_PROTOCOL_VERSION_MIN;

	geturl << g_settings->get("serverlist_url") <<
		"/list?proto_version_min=" << proto_version_min <<
		"&proto_version_max=" << CLIENT_PROTOCOL_VERSION_MAX;
	Json::Value root = fetchJsonValue(geturl.str(), NULL);

	std::vector<ServerListSpec> server_list;

	if (!root.isObject()) {
		return server_list;
	}

	root = root["list"];
	if (!root.isArray()) {
		return server_list;
	}

	for (unsigned int i = 0; i < root.size(); i++) {
		if (root[i].isObject()) {
			server_list.push_back(root[i]);
		}
	}

	cached_online = server_list; 
	return server_list;
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
			++it) {
		list.append(*it);
	}
	root["list"] = list;
	Json::StyledWriter writer;
	return writer.write(root);
}


void sendAnnounce(const std::string &action,
		const u16 port,
		const std::vector<std::string> &clients_names,
		const double uptime,
		const u32 game_time,
		const float lag,
		const std::string &gameid,
		const std::string &mg_name,
		const std::vector<ModSpec> &mods)
{
#if USE_CURL
	Json::Value server;
	server["action"] = action;
	server["port"] = port;
	if (g_settings->exists("server_address")) {
		server["address"] = g_settings->get("server_address");
	}
	if (action != "delete") {
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
		for (std::vector<std::string>::const_iterator it = clients_names.begin();
				it != clients_names.end();
				++it) {
			server["clients_list"].append(*it);
		}
		if (gameid != "") server["gameid"] = gameid;
		server["proto"]        = g_settings->get("server_proto");
	}

	if (action == "start") {
		server["dedicated"]         = g_settings->getBool("server_dedicated");
		server["rollback"]          = g_settings->getBool("enable_rollback_recording");
		server["mapgen"]            = mg_name;
		server["privs"]             = g_settings->getBool("creative_mode") ? g_settings->get("default_privs_creative") : g_settings->get("default_privs");
		server["can_see_far_names"] = g_settings->getS16("player_transfer_distance") <= 0;
		server["liquid_real"]       = g_settings->getBool("liquid_real");
		server["version_hash"]      = g_version_hash;
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
	HTTPFetchRequest fetch_request;
	fetch_request.timeout = fetch_request.connect_timeout = 59000;
	fetch_request.url = g_settings->get("serverlist_url") + std::string("/announce");

#if !MINETEST_PROTO
	// todo: need to patch masterserver script to parse multipart posts
	std::string query = std::string("json=") + urlencode(writer.write(server));
	if (query.size() < 1000)
		fetch_request.url += "?" + query;
	else
		fetch_request.post_data = query;
#else
	fetch_request.post_fields["json"] = writer.write(server);
	fetch_request.multipart = true;
#endif

	httpfetch_async(fetch_request);
#endif
}


lan_adv lan_adv_client;

void lan_get() {
	if (!g_settings->getBool("serverlist_lan"))
		return;
	ServerList::lan_adv_client.ask();
}

void lan_apply(std::vector<ServerListSpec> & servers) {
	auto lock = lan_adv_client.collected.lock_unique_rec();
	if (lan_adv_client.collected.size()) {
		if (servers.size()) {
			Json::Value separator;
			separator["name"] = "-----lan-servers-end-----";
			servers.insert(servers.begin(), separator);
		}
		for (auto & i : lan_adv_client.collected) {
			servers.insert(servers.begin(), i.second);
		}
	}
}

bool lan_fresh() {
	auto result = lan_adv_client.fresh.load();
	lan_adv_client.fresh = false;
	return result;
}


} //namespace ServerList
