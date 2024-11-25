// Luanti
// SPDX-License-Identifier: LGPL-2.1-or-later
// Copyright (C) 2013 celeron55, Perttu Ahola <celeron55@gmail.com>

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
#include "server.h"

namespace ServerList
{

void addMultiProto(Json::Value &server, const u16 port)
{
#if USE_MULTI
	server["proto_multi"]["mt"] = port;
#if USE_SCTP
	{
		u16 port_multi = 0;
		if (!g_settings->getU16NoEx("port_sctp", port_multi)) {
			port_multi = port + 100;
		}
		server["proto_multi"]["sctp"] = port_multi;
	}
#endif
#if USE_WEBSOCKET
	{
		u16 port_multi = 0;
		if (!g_settings->getU16NoEx("port_wss", port_multi)) {
			port_multi = port;
		}
		server["proto_multi"]["wss"] = port_multi;
	}
#endif
#if USE_WEBSOCKET_SCTP
	{
		u16 port_multi = 0;
		if (!g_settings->getU16NoEx("port_sctp_wss", port_multi)) {
			port_multi = port + 100;
		}
		server["proto_multi"]["sctp_wss"] = port_multi;
	}
#endif
#if USE_ENET
	{
		u16 port_multi = 0;
		if (!g_settings->getU16NoEx("port_enet", port_multi)) {
			port_multi = port + 200;
		}
		server["proto_multi"]["enet"] = port_multi;
	}
#endif
#endif
}

static const char *aa_names[] = {"start", "update", "delete"};

Json::Value MakeReport(AnnounceAction action,
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
	Json::Value server;
	server["action"] = aa_names[action];
	server["port"] = port;
	if (g_settings->exists("server_address")) {
		server["address"] = g_settings->get("server_address");
	}
	if (action != AA_DELETE) {
		server["name"]         = g_settings->get("server_name");
		server["description"]  = g_settings->get("server_description");
		server["version"]      = g_version_string;
		server["proto_min"]    = Server::getProtocolVersionMin();
		server["proto_max"]    = Server::getProtocolVersionMax();
		server["url"]          = g_settings->get("server_url");
		server["creative"]     = g_settings->getBool("creative_mode");
		server["damage"]       = g_settings->getBool("enable_damage");
		server["password"]     = g_settings->getBool("disallow_empty_password");
		server["pvp"]          = g_settings->getBool("enable_pvp");
		server["uptime"]   = (int) uptime;
		server["game_time"]= game_time;
		server["clients"]      = (int) clients_names.size();
		server["clients_max"]  = g_settings->getU16("max_users");
		if (g_settings->getBool("server_announce_send_players")) {
			server["clients_list"] = Json::Value(Json::arrayValue);
			for (const std::string &clients_name : clients_names)
				server["clients_list"].append(clients_name);
		}
		if (!gameid.empty())
			server["gameid"] = gameid;
		server["proto"]        = g_settings->get("server_proto");

		addMultiProto(server, port);
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

	return server;
}

std::string MakeReportString(AnnounceAction action,
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
	return fastWriteJson(MakeReport(action, port, clients_names, uptime, game_time, lag,
			gameid, mg_name, mods, dedicated));
}
	std::string last_status;

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

	last_status = MakeReportString(action, port, clients_names, uptime, game_time, lag,
			gameid, mg_name, mods, dedicated);

#if USE_CURL

	auto server = MakeReport(action, port, clients_names, uptime, game_time, lag, gameid,
			mg_name, mods, dedicated);

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

	std::string serverlist;
	if (g_settings->getNoEx("serverlist_url_freeminer", serverlist) && !serverlist.empty()) {
		infostream << "Announcing " << aa_names[action] << " to " << serverlist << '\n';
		fetch_request.url = serverlist + std::string("/announce");
		httpfetch_async(fetch_request);
	}

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
