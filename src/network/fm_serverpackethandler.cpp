/*
Copyright (C) 2015 nerzhul, Loic Blot <loic.blot@unix-experience.fr>
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


#include "remoteplayer.h"
#include "scripting_server.h"
#include "server.h"
#include "log_types.h"

//#include "../content_abm.h"
//#include "content_sao.h"
#include "emerge.h"
#include "nodedef.h"
#include "player.h"
#include "rollback_interface.h"
//#include "scripting_game.h"
#include "server/player_sao.h"
#include "settings.h"
#include "tool.h"
#include "version.h"
#include "network/networkprotocol.h"
#include "fm_networkprotocol.h"
#include "network/serveropcodes.h"
#include "util/base64.h"
#include "util/pointedthing.h"
#include "util/serialize.h"

#include "profiler.h"
#include "ban.h"

#include "../util/auth.h"

void Server::handleCommand_Deprecated(NetworkPacket* pkt) {
	infostream << "Server: " << toServerCommandTable[pkt->getCommand()].name
	           << " not supported anymore" << std::endl;
}

void Server::handleCommand_Init(NetworkPacket* pkt) {
	//const auto peer_id = pkt->getPeerId();
	//auto & packet = *(pkt->packet);

// TODO

}

void Server::handleCommand_Init_Legacy(NetworkPacket* pkt) {
	const auto peer_id = pkt->getPeerId();
	auto & packet = *(pkt->packet);

	RemoteClient* client = getClient(pkt->getPeerId(), CS_Created);

	std::string addr_s;
	try {
		addr_s = getPeerAddress(pkt->getPeerId()).serializeString();
	} catch (const std::exception &e) {
		/*
		 * no peer for this packet found
		 * most common reason is peer timeout, e.g. peer didn't
		 * respond for some time, your server was overloaded or
		 * things like that.
		 */
		infostream << "Server::ProcessData(): Canceling: peer "
		           << pkt->getPeerId() << " not found : " << e.what() << std::endl;
		return;
	}

	// If net_proto_version is set, this client has already been handled
	if(client->getState() > CS_Created) {
		verbosestream << "Server: Ignoring multiple TOSERVER_INITs from "
		              << addr_s << " (peer_id=" << peer_id << ")" << std::endl;
		return;
	}

	verbosestream << "Server: Got TOSERVER_INIT from " << addr_s << " (peer_id="
	              << peer_id << ")" << std::endl;

	// Do not allow multiple players in simple singleplayer mode.
	// This isn't a perfect way to do it, but will suffice for now
	if(m_simple_singleplayer_mode && m_clients.getClientIDs().size() > 1) {
		infostream << "Server: Not allowing another client (" << addr_s
		           << ") to connect in simple singleplayer mode" << std::endl;
		DenyAccess(peer_id, "Running in simple singleplayer mode.");
		return;
	}

	// First byte after command is maximum supported
	// serialization version
	u8 client_max;
	packet[TOSERVER_INIT_LEGACY_FMT].convert(client_max);
	u8 our_max = SER_FMT_VER_HIGHEST_READ;
	// Use the highest version supported by both
	int deployed = std::min(client_max, our_max);
	// If it's lower than the lowest supported, give up.
	if (deployed < SER_FMT_VER_LOWEST_READ)
		deployed = SER_FMT_VER_INVALID;

	if (deployed == SER_FMT_VER_INVALID) {
		actionstream << "Server: A mismatched client tried to connect from "
		             << addr_s << std::endl;
		infostream << "Server: Cannot negotiate serialization version with "
		           << addr_s << std::endl;
		DenyAccess(peer_id, std::string(
		               "Your client's version is not supported.\n"
		               "Server version is ")
		           + (g_version_string) + "."
		          );
		return;
	}

	client->setPendingSerializationVersion(deployed);

	/*
		Read and check network protocol version
	*/

	u16 min_net_proto_version = 0;
	packet[TOSERVER_INIT_LEGACY_PROTOCOL_VERSION_MIN].convert(min_net_proto_version);
	u16 max_net_proto_version = min_net_proto_version;
	packet[TOSERVER_INIT_LEGACY_PROTOCOL_VERSION_MAX].convert(max_net_proto_version);

	packet.convert_safe(TOSERVER_INIT_LEGACY_PROTOCOL_VERSION_FM, client->net_proto_version_fm);

	// Start with client's maximum version
	u16 net_proto_version = max_net_proto_version;

	// Figure out a working version if it is possible at all
	if(max_net_proto_version >= SERVER_PROTOCOL_VERSION_MIN ||
	        min_net_proto_version <= SERVER_PROTOCOL_VERSION_MAX) {
		// If maximum is larger than our maximum, go with our maximum
		if(max_net_proto_version > SERVER_PROTOCOL_VERSION_MAX)
			net_proto_version = SERVER_PROTOCOL_VERSION_MAX;
		// Else go with client's maximum
		else
			net_proto_version = max_net_proto_version;
	}

	verbosestream << "Server: " << addr_s << ": Protocol version: min: "
	              << min_net_proto_version << ", max: " << max_net_proto_version
	              << ", chosen: " << net_proto_version << std::endl;

	client->net_proto_version = net_proto_version;

	if(net_proto_version < SERVER_PROTOCOL_VERSION_MIN ||
	        net_proto_version > SERVER_PROTOCOL_VERSION_MAX) {
		actionstream << "Server: A mismatched client tried to connect from "
		             << addr_s << std::endl;
		DenyAccess(peer_id, std::string(
		               "Your client's version is not supported.\n"
		               "Server version is ")
		           + (g_version_string) + ",\n"
		           + "server's PROTOCOL_VERSION is "
		           + itos(SERVER_PROTOCOL_VERSION_MIN)
		           + "..."
		           + itos(SERVER_PROTOCOL_VERSION_MAX)
		           + ", client's PROTOCOL_VERSION is "
		           + itos(min_net_proto_version)
		           + "..."
		           + itos(max_net_proto_version)
		          );
		return;
	}

	if(g_settings->getBool("strict_protocol_version_checking")) {
		if(net_proto_version != LATEST_PROTOCOL_VERSION) {
			actionstream << "Server: A mismatched (strict) client tried to "
			             << "connect from " << addr_s << std::endl;
			DenyAccess(peer_id, std::string(
			               "Your client's version is not supported.\n"
			               "Server version is ")
			           + (g_version_string) + ",\n"
			           + "server's PROTOCOL_VERSION (strict) is "
			           + itos(LATEST_PROTOCOL_VERSION)
			           + ", client's PROTOCOL_VERSION is "
			           + itos(min_net_proto_version)
			           + "..."
			           + itos(max_net_proto_version)
			          );
			return;
		}
	}

	/*
		Set up player
	*/

	// Get player name
	std::string playername;
	packet[TOSERVER_INIT_LEGACY_NAME].convert(playername);

	if(playername.empty()) {
		actionstream << "Server: Player with an empty name "
		             << "tried to connect from " << addr_s << std::endl;
		DenyAccess(peer_id, "Empty name");
		return;
	}

	if(!g_settings->getBool("enable_any_name") && string_allowed(playername, PLAYERNAME_ALLOWED_CHARS) == false) {
		actionstream << "Server: Player with an invalid name [" << playername
		             << "] tried to connect from " << addr_s << std::endl;
		DenyAccess(peer_id, "Name contains unallowed characters");
		return;
	}

	if(!isSingleplayer() && playername == "singleplayer") {
		actionstream << "Server: Player with the name \"singleplayer\" "
		             << "tried to connect from " << addr_s << std::endl;
		DenyAccess(peer_id, "Name is not allowed");
		return;
	}

	{
		std::string reason;
		if(m_script->on_prejoinplayer(playername, addr_s, &reason)) {
			actionstream << "Server: Player with the name \"" << playername << "\" "
			             << "tried to connect from " << addr_s << " "
			             << "but it was disallowed for the following reason: "
			             << reason << std::endl;
			DenyAccess(peer_id, reason);
			return;
		}
	}

	infostream << "Server: New connection: \"" << playername << "\" from "
	           << addr_s << " (peer_id=" << peer_id << ")" << std::endl;

	// Get password
	std::string given_password;
	packet[TOSERVER_INIT_LEGACY_PASSWORD].convert(given_password);

	if(!base64_is_valid(given_password.c_str())) {
		actionstream << "Server: " << playername
		             << " supplied invalid password hash" << std::endl;
		DenyAccess(peer_id, "Invalid password hash");
		return;
	}

	// Enforce user limit.
	// Don't enforce for users that have some admin right
	if(m_clients.getClientIDs(CS_Created).size() >= g_settings->getU16("max_users") &&
	        !checkPriv(playername, "server") &&
	        !checkPriv(playername, "ban") &&
	        !checkPriv(playername, "privs") &&
	        !checkPriv(playername, "password") &&
	        playername != g_settings->get("name")) {
		actionstream << "Server: " << playername << " tried to join, but there"
		             << " are already max_users="
		             << g_settings->getU16("max_users") << " players." << std::endl;
		DenyAccess(peer_id, "Too many users.");
		return;
	}

	std::string checkpwd; // Password hash to check against
	bool has_auth = m_script->getAuth(playername, &checkpwd, NULL);

	// If no authentication info exists for user, create it
	if(!has_auth) {
		if(!isSingleplayer() &&
		        g_settings->getBool("disallow_empty_password") &&
		        given_password == "") {
			actionstream << "Server: " << playername
			             << " supplied empty password" << std::endl;
			DenyAccess(peer_id, "Empty passwords are "
			           "disallowed. Set a password and try again.");
			return;
		}
		std::string raw_default_password = g_settings->get("default_password");
		std::string initial_password =
		    translate_password(playername, raw_default_password);

		// If default_password is empty, allow any initial password
		if (raw_default_password.length() == 0)
			initial_password = given_password;

		m_script->createAuth(playername, initial_password);
	}

	has_auth = m_script->getAuth(playername, &checkpwd, NULL);

	if(!has_auth) {
		actionstream << "Server: " << playername << " cannot be authenticated"
		             << " (auth handler does not work?)" << std::endl;
		DenyAccess(peer_id, "Not allowed to login");
		return;
	}

	if(given_password != checkpwd) {
		actionstream << "Server: " << playername << " supplied wrong password"
		             << " at " << addr_s
		             << std::endl;
		DenyAccess(peer_id, "Wrong password");
		return;
	}

	RemotePlayer *player =
	    static_cast<RemotePlayer*>(m_env->getPlayer(playername.c_str()));

	if (player && player->getPeerId() != 0) {

		if (given_password.size()) {
			actionstream << "Server: " << playername << " rejoining" << std::endl;
			DenyAccessVerCompliant(player->getPeerId(), player->protocol_version, SERVER_ACCESSDENIED_ALREADY_CONNECTED);
			player->getPlayerSAO()->removingFromEnvironment();
			m_env->removePlayer(player);
			player = nullptr;
		} else {
			errorstream << "Server: " << playername << ": Failed to emerge player" << " (player allocated to an another client)" << std::endl;
			DenyAccess(peer_id, "Another client is connected with this "
			           "name. If your client closed unexpectedly, try again in "
			           "a minute.");
		}
	}

	m_clients.setPlayerName(peer_id, playername);

	/*
		Answer with a TOCLIENT_INIT
	*/
	{
		MSGPACK_PACKET_INIT((int)TOCLIENT_INIT_LEGACY, 7);
		PACK(TOCLIENT_INIT_DEPLOYED, deployed);
		PACK(TOCLIENT_INIT_SEED, m_env->getServerMap().getSeed());
		PACK(TOCLIENT_INIT_STEP, g_settings->getFloat("dedicated_server_step"));

		//if (player) //todo : remake me
		//	PACK(TOCLIENT_INIT_POS, player->getPosition());

		Settings params;
		m_emerge->mgparams->MapgenParams::writeParams(&params);
		m_emerge->mgparams->writeParams(&params);
		PACK(TOCLIENT_INIT_MAP_PARAMS, params);
		PACK(TOCLIENT_INIT_GAMEID, m_gamespec.id);

		PACK(TOCLIENT_INIT_PROTOCOL_VERSION_FM, SERVER_PROTOCOL_VERSION_FM);

		PACK(TOCLIENT_INIT_WEATHER, g_settings->getBool("weather"));

		// Send as reliable
		m_clients.send(peer_id, 0, buffer, true);
		m_clients.event(peer_id, CSE_InitLegacy);
	}

	return;
}

void Server::handleCommand_Init2(NetworkPacket* pkt) {
	const auto peer_id = pkt->getPeerId();
	//auto & packet = *(pkt->packet);

	verbosestream << "Server: Got TOSERVER_INIT2 from "
	              << peer_id << std::endl;

	m_clients.event(peer_id, CSE_GotInit2);
	u16 protocol_version = m_clients.getProtocolVersion(peer_id);


	///// begin compatibility code
	PlayerSAO* playersao = NULL;
	if (protocol_version <= 22) {
		playersao = StageTwoClientInit(peer_id);

		if (playersao == NULL) {
			errorstream
			        << "TOSERVER_INIT2 stage 2 client init failed for peer "
			        << peer_id << std::endl;
			return;
		}
	}
	///// end compatibility code

	/*
		Send some initialization data
	*/

	infostream << "Server: Sending content to "
	           << getPlayerName(peer_id) << std::endl;

	// Send player movement settings
	SendMovement(peer_id);

	// Send item definitions
	SendItemDef(peer_id, m_itemdef, protocol_version);

	// Send node definitions
	SendNodeDef(peer_id, m_nodedef, protocol_version);

	m_clients.event(peer_id, CSE_SetDefinitionsSent);

	// Send media announcement
	sendMediaAnnouncement(peer_id);

	// Send detached inventories
	sendDetachedInventories(peer_id);

	// Send time of day
	u16 time = m_env->getTimeOfDay();
	float time_speed = g_settings->getFloat("time_speed");
	SendTimeOfDay(peer_id, time, time_speed);

	///// begin compatibility code
	if (protocol_version <= 22) {
		m_clients.event(peer_id, CSE_SetClientReady);
		m_script->on_joinplayer(playersao);
	}
	///// end compatibility code

	// Warnings about protocol version can be issued here
	if(getClient(peer_id)->net_proto_version < LATEST_PROTOCOL_VERSION) {
		SendChatMessage(peer_id, "# Server: WARNING: YOUR CLIENT'S "
		                "VERSION MAY NOT BE FULLY COMPATIBLE WITH THIS SERVER!");
	}
}


void Server::handleCommand_RequestMedia(NetworkPacket* pkt) {
	const auto peer_id = pkt->getPeerId();
	auto & packet = *(pkt->packet);

	std::vector<std::string> tosend;
	packet[TOSERVER_REQUEST_MEDIA_FILES].convert(tosend);

	sendRequestedMedia(peer_id, tosend);
}

void Server::handleCommand_ReceivedMedia(NetworkPacket* pkt) {
}

void Server::handleCommand_ClientReady(NetworkPacket* pkt) {
	const auto peer_id = pkt->getPeerId();
	auto & packet = *(pkt->packet);

	u16 peer_proto_ver = getClient(peer_id, CS_InitDone)->net_proto_version;
	// clients <= protocol version 22 did not send ready message,
	// they're already initialized

	if (peer_proto_ver <= 22) {
		infostream << "Client sent message not expected by a "
		           << "client using protocol version <= 22,"
		           << "disconnecting peer_id: " << peer_id << std::endl;
		m_con->DisconnectPeer(peer_id);
		return;
	}

	PlayerSAO* playersao = StageTwoClientInit(peer_id);

	// If failed, cancel
	if (playersao == NULL) {
		errorstream
		        << "TOSERVER_CLIENT_READY stage 2 client init failed for peer_id: "
		        << peer_id << std::endl;
		m_con.DisconnectPeer(peer_id);
		return;
	}
	int version_patch = 0, version_tweak = 0;
	packet.convert_safe(TOSERVER_CLIENT_READY_VERSION_PATCH, version_patch);
	packet.convert_safe(TOSERVER_CLIENT_READY_VERSION_TWEAK, version_tweak);
	if (version_tweak) {} //no warn todo remove
	m_clients.setClientVersion(
	    peer_id,
	    packet[TOSERVER_CLIENT_READY_VERSION_MAJOR].as<int>(),
	    packet[TOSERVER_CLIENT_READY_VERSION_MINOR].as<int>(),
	    version_patch,
	    //version_tweak,
	    packet[TOSERVER_CLIENT_READY_VERSION_STRING].as<std::string>()
	);
	m_clients.event(peer_id, CSE_SetClientReady);
	m_script->on_joinplayer(playersao);

	stat.add("join", playersao->getPlayer()->getName());

}

void Server::handleCommand_GotBlocks(NetworkPacket* pkt) {
}

void Server::handleCommand_PlayerPos(NetworkPacket* pkt) {
	const auto peer_id = pkt->getPeerId();
	auto & packet = *(pkt->packet);
	auto player = m_env->getPlayer(pkt->getPeerId());
	if (!player) {
		m_con.DisconnectPeer(pkt->getPeerId());
		return;
	}
	auto playersao = player->getPlayerSAO();
	if (!playersao) {
		m_con.DisconnectPeer(pkt->getPeerId());
		return;
	}


	// If player is dead we don't care of this packet
	if (playersao->isDead()) {
		verbosestream << "TOSERVER_PLAYERPOS: " << player->getName()
				<< " is dead. Ignoring packet";
		return;
	}

	if (playersao->m_ms_from_last_respawn > 1000)
		playersao->setBasePosition(packet[TOSERVER_PLAYERPOS_POSITION].as<v3f>());
	player->setSpeed(packet[TOSERVER_PLAYERPOS_SPEED].as<v3f>());
	playersao->setPitch(modulo360f(packet[TOSERVER_PLAYERPOS_PITCH].as<f32>()));
	playersao->setYaw(modulo360f(packet[TOSERVER_PLAYERPOS_YAW].as<f32>()));
	u32 keyPressed = packet[TOSERVER_PLAYERPOS_KEY_PRESSED].as<u32>();
	player->keyPressed = keyPressed;
	{
		std::lock_guard<std::mutex> lock(player->control_mutex);
		player->control.up = (bool)(keyPressed & 1);
		player->control.down = (bool)(keyPressed & 2);
		player->control.left = (bool)(keyPressed & 4);
		player->control.right = (bool)(keyPressed & 8);
		player->control.jump = (bool)(keyPressed & 16);
		player->control.aux1 = (bool)(keyPressed & 32);
		player->control.sneak = (bool)(keyPressed & 64);
		player->control.LMB = (bool)(keyPressed & 128);
		player->control.RMB = (bool)(keyPressed & 256);
	}
	auto old_pos = playersao->m_last_good_position;
	if(playersao->checkMovementCheat()) {
		// Call callbacks
		m_script->on_cheat(playersao, "moved_too_fast");
		SendMovePlayer(peer_id);
	} else if (playersao->m_ms_from_last_respawn > 3000) {
		auto dist = (old_pos / BS).getDistanceFrom(playersao->m_last_good_position / BS);
		if (dist)
			stat.add("move", playersao->getPlayer()->getName(), dist);
	}

	if (playersao->m_ms_from_last_respawn > 2000) {
		auto obj = playersao; // copypasted from server step:
		auto uptime = m_uptime.get();
		if (!obj->m_uptime_last)  // not very good place, but minimum modifications
			obj->m_uptime_last = uptime - 0.1;
		if (uptime - obj->m_uptime_last > 0.5) {
			obj->step(uptime - obj->m_uptime_last, true); //todo: maybe limit count per time
			obj->m_uptime_last = uptime;
		}
	}

	/*infostream<<"Server::ProcessData(): Moved player "<<peer_id<<" to "
														<<"("<<position.X<<","<<position.Y<<","<<position.Z<<")"
														<<" pitch="<<pitch<<" yaw="<<yaw<<std::endl;*/
}

void Server::handleCommand_DeletedBlocks(NetworkPacket* pkt) {
	const auto peer_id = pkt->getPeerId();
	auto & packet = *(pkt->packet);

	std::vector<v3s16> deleted_blocks;
	packet[TOSERVER_DELETEDBLOCKS_DATA].convert(deleted_blocks);
	RemoteClient *client = getClient(peer_id);
	for (auto &block : deleted_blocks)
		client->SetBlockDeleted(block);
}

void Server::handleCommand_InventoryAction(NetworkPacket* pkt) {
	//const auto peer_id = pkt->getPeerId();
	auto & packet = *(pkt->packet);
	auto player = m_env->getPlayer(pkt->getPeerId());
	if (!player) {
		m_con.DisconnectPeer(pkt->getPeerId());
		return;
	}
	auto playersao = player->getPlayerSAO();
	if (!playersao) {
		m_con.DisconnectPeer(pkt->getPeerId());
		return;
	}

	std::string datastring;
	packet[TOSERVER_INVENTORY_ACTION_DATA].convert(datastring);
	std::istringstream is(datastring, std::ios_base::binary);
	// Create an action
	InventoryAction *a = InventoryAction::deSerialize(is);
	if(a == NULL) {
		infostream << "TOSERVER_INVENTORY_ACTION: "
		           << "InventoryAction::deSerialize() returned NULL"
		           << std::endl;
		return;
	}

	// If something goes wrong, this player is to blame
	RollbackScopeActor rollback_scope(m_rollback,
	                                  std::string("player:") + player->getName());

	/*
		Note: Always set inventory not sent, to repair cases
		where the client made a bad prediction.
	*/

	/*
		Handle restrictions and special cases of the move action
	*/
	if(a->getType() == IACTION_MOVE) {
		IMoveAction *ma = (IMoveAction*)a;

		ma->from_inv.applyCurrentPlayer(player->getName());
		ma->to_inv.applyCurrentPlayer(player->getName());

		setInventoryModified(ma->from_inv, false);
		setInventoryModified(ma->to_inv, false);

		bool from_inv_is_current_player =
		    (ma->from_inv.type == InventoryLocation::PLAYER) &&
		    (ma->from_inv.name == player->getName());

		bool to_inv_is_current_player =
		    (ma->to_inv.type == InventoryLocation::PLAYER) &&
		    (ma->to_inv.name == player->getName());

		/*
			Disable moving items out of craftpreview
		*/
		if(ma->from_list == "craftpreview") {
			infostream << "Ignoring IMoveAction from "
			           << (ma->from_inv.dump()) << ":" << ma->from_list
			           << " to " << (ma->to_inv.dump()) << ":" << ma->to_list
			           << " because src is " << ma->from_list << std::endl;
			delete a;
			return;
		}

		/*
			Disable moving items into craftresult and craftpreview
		*/
		if(ma->to_list == "craftpreview" || ma->to_list == "craftresult") {
			infostream << "Ignoring IMoveAction from "
			           << (ma->from_inv.dump()) << ":" << ma->from_list
			           << " to " << (ma->to_inv.dump()) << ":" << ma->to_list
			           << " because dst is " << ma->to_list << std::endl;
			delete a;
			return;
		}

		// Disallow moving items in elsewhere than player's inventory
		// if not allowed to interact
		if(!checkPriv(player->getName(), "interact") &&
		        (!from_inv_is_current_player ||
		         !to_inv_is_current_player)) {
			infostream << "Cannot move outside of player's inventory: "
			           << "No interact privilege" << std::endl;
			delete a;
			return;
		}
	}
	/*
		Handle restrictions and special cases of the drop action
	*/
	else if(a->getType() == IACTION_DROP) {
		IDropAction *da = (IDropAction*)a;

		da->from_inv.applyCurrentPlayer(player->getName());

		setInventoryModified(da->from_inv, false);

		/*
			Disable dropping items out of craftpreview
		*/
		if(da->from_list == "craftpreview") {
			infostream << "Ignoring IDropAction from "
			           << (da->from_inv.dump()) << ":" << da->from_list
			           << " because src is " << da->from_list << std::endl;
			delete a;
			return;
		}

		// Disallow dropping items if not allowed to interact
		if(!checkPriv(player->getName(), "interact")) {
			delete a;
			return;
		}

		// Disallow dropping items if dead
		if (playersao->isDead()) {
			infostream << "Ignoring IDropAction from "
					<< (da->from_inv.dump()) << ":" << da->from_list
					<< " because player is dead." << std::endl;
			delete a;
			return;
		}

		stat.add("drop", player->getName());
	}
	/*
		Handle restrictions and special cases of the craft action
	*/
	else if(a->getType() == IACTION_CRAFT) {
		ICraftAction *ca = (ICraftAction*)a;

		ca->craft_inv.applyCurrentPlayer(player->getName());

		setInventoryModified(ca->craft_inv, false);

		//bool craft_inv_is_current_player =
		//	(ca->craft_inv.type == InventoryLocation::PLAYER) &&
		//	(ca->craft_inv.name == player->getName());

		// Disallow crafting if not allowed to interact
		if(!checkPriv(player->getName(), "interact")) {
			infostream << "Cannot craft: "
			           << "No interact privilege" << std::endl;
			delete a;
			return;
		}
		stat.add("craft", player->getName());
	}

	// Do the action
	a->apply(this, playersao, this);
	// Eat the action
	delete a;

	SendInventory(playersao);

}

void Server::handleCommand_ChatMessage(NetworkPacket* pkt) {
	const auto peer_id = pkt->getPeerId();
	auto & packet = *(pkt->packet);
	auto player = m_env->getPlayer(pkt->getPeerId());
	if (!player) {
		m_con.DisconnectPeer(pkt->getPeerId());
		return;
	}
	auto playersao = player->getPlayerSAO();
	if (!playersao) {
		m_con.DisconnectPeer(pkt->getPeerId());
		return;
	}

	std::string message = packet[TOSERVER_CHAT_MESSAGE_DATA].as<std::string>();

	// If something goes wrong, this player is to blame
	RollbackScopeActor rollback_scope(m_rollback,
	                                  std::string("player:") + player->getName());

	// Get player name of this client
	std::string name = player->getName();

	// Run script hook
	bool ate = m_script->on_chat_message(name, message);
	// If script ate the message, don't proceed
	if(ate)
		return;

	// Line to send to players
	std::string line;
	// Whether to send to other players
	bool send_to_others = false;

	// Commands are implemented in Lua, so only catch invalid
	// commands that were not "eaten" and send an error back
	if(message[0] == '/') {
		message = message.substr(1);
		if(message.length() == 0)
			line += "-!- Empty command";
		else
			// TODO: str_split(message, ' ')[0]
			line += "-!- Invalid command: " + message;
	} else {
		if(checkPriv(player->getName(), "shout")) {
			line += "<";
			if (name.size() > 25) {
				auto cutted = name;
				cutted.resize(25);
				line += cutted + ".";
			} else {
				line += name;
			}
			line += "> ";
			line += message;
			send_to_others = true;
		} else
			line += "-!- You don't have permission to shout.";
	}

	if(!line.empty()) {
		if(send_to_others) {
			stat.add("chat", name);
			actionstream << "CHAT: " << line << std::endl;
			SendChatMessage(PEER_ID_INEXISTENT, line);
		} else
			SendChatMessage(peer_id, line);
	}
}

void Server::handleCommand_Damage(NetworkPacket* pkt) {
	//const auto peer_id = pkt->getPeerId();
	auto & packet = *(pkt->packet);
	auto player = m_env->getPlayer(pkt->getPeerId());
	if (!player) {
		m_con.DisconnectPeer(pkt->getPeerId());
		return;
	}
	auto playersao = player->getPlayerSAO();
	if (!playersao) {
		m_con.DisconnectPeer(pkt->getPeerId());
		return;
	}
	u8 damage = packet[TOSERVER_DAMAGE_VALUE].as<u8>();

	if(playersao->getHP() && g_settings->getBool("enable_damage")) {
		actionstream << player->getName() << " damaged by "
		             << (int)damage << " hp at " << (playersao->getBasePosition() / BS)
		             << std::endl;

		playersao->setHP(playersao->getHP() - damage);

		SendPlayerHPOrDie(playersao);

		stat.add("damage", player->getName(), damage);
	}
}

void Server::handleCommand_Breath(NetworkPacket* pkt) {
	const auto peer_id = pkt->getPeerId();
	auto & packet = *(pkt->packet);
	auto player = m_env->getPlayer(pkt->getPeerId());
	if (!player) {
		m_con.DisconnectPeer(pkt->getPeerId());
		return;
	}
	auto playersao = player->getPlayerSAO();
	if (!playersao) {
		m_con.DisconnectPeer(pkt->getPeerId());
		return;
	}

	/*
	 * If player is dead, we don't need to update the breath
	 * He is dead !
	 */
	if (!playersao->isDead()) {
		playersao->setBreath(packet[TOSERVER_BREATH_VALUE].as<u16>());
		SendPlayerBreath(peer_id);
	}

}

void Server::handleCommand_Password(NetworkPacket* pkt) {
	const auto peer_id = pkt->getPeerId();
	auto & packet = *(pkt->packet);
	auto player = m_env->getPlayer(pkt->getPeerId());
	if (!player) {
		m_con.DisconnectPeer(pkt->getPeerId());
		return;
	}
	std::string oldpwd, newpwd;
	packet[TOSERVER_CHANGE_PASSWORD_OLD].convert(oldpwd);
	packet[TOSERVER_CHANGE_PASSWORD_NEW].convert(newpwd);

	if(!base64_is_valid(newpwd)) {
		infostream << "Server: " << player->getName() << " supplied invalid password hash" << std::endl;
		// Wrong old password supplied!!
		SendChatMessage(peer_id, "Invalid new password hash supplied. Password NOT changed.");
		return;
	}

	infostream << "Server: Client requests a password change from "
	           << "'" << oldpwd << "' to '" << newpwd << "'" << std::endl;

	std::string playername = player->getName();

	std::string checkpwd;
	m_script->getAuth(playername, &checkpwd, NULL);

	if(oldpwd != checkpwd) {
		infostream << "Server: invalid old password" << std::endl;
		// Wrong old password supplied!!
		SendChatMessage(peer_id, "Invalid old password supplied. Password NOT changed.");
		return;
	}

	bool success = m_script->setPassword(playername, newpwd);
	if(success) {
		actionstream << player->getName() << " changes password" << std::endl;
		SendChatMessage(peer_id, "Password change successful.");
	} else {
		actionstream << player->getName() << " tries to change password but "
		             << "it fails" << std::endl;
		SendChatMessage(peer_id, "Password change failed or inavailable.");
	}

}

void Server::handleCommand_PlayerItem(NetworkPacket* pkt) {
	//const auto peer_id = pkt->getPeerId();
	auto & packet = *(pkt->packet);
	auto player = m_env->getPlayer(pkt->getPeerId());
	if (!player) {
		m_con.DisconnectPeer(pkt->getPeerId());
		return;
	}
	auto playersao = player->getPlayerSAO();
	if (!playersao) {
		m_con.DisconnectPeer(pkt->getPeerId());
		return;
	}
	u16 item = packet[TOSERVER_PLAYERITEM_VALUE].as<u16>();
	playersao->setWieldIndex(item);
}

void Server::handleCommand_Respawn(NetworkPacket* pkt) {
	const auto peer_id = pkt->getPeerId();
	//auto & packet = *(pkt->packet);
	auto player = m_env->getPlayer(pkt->getPeerId());
	if (!player) {
		m_con.DisconnectPeer(pkt->getPeerId());
		return;
	}
	auto playersao = player->getPlayerSAO();
	if (!playersao) {
		m_con.DisconnectPeer(pkt->getPeerId());
		return;
	}

	if(!playersao->isDead())
		return;

	RespawnPlayer(peer_id);

	actionstream << player->getName() << " respawns at "
	             << (playersao->getBasePosition() / BS) << std::endl;

	// ActiveObject is added to environment in AsyncRunStep after
	// the previous addition has been successfully removed
}

void Server::handleCommand_Interact(NetworkPacket* pkt) {
	const auto peer_id = pkt->getPeerId();
	auto & packet = *(pkt->packet);

	u8 action;
	u16 item_i;
	PointedThing pointed;

	packet[TOSERVER_INTERACT_ACTION].convert(action);
	packet[TOSERVER_INTERACT_ITEM].convert(item_i);
	packet[TOSERVER_INTERACT_POINTED_THING].convert(pointed);

	if (overload) {
		if (pointed.type == POINTEDTHING_NOTHING || action == 1) return;
		//errorstream<<"overload pointed peer_id=" << peer_id << " action=" << (int)action  << " pointed.type="<<pointed.type<< "\n";
	}

	auto player = m_env->getPlayer(pkt->getPeerId());
	if (!player) {
		m_con.DisconnectPeer(pkt->getPeerId());
		return;
	}
	auto playersao = player->getPlayerSAO();
	if (!playersao) {
		m_con.DisconnectPeer(pkt->getPeerId());
		return;
	}

	if (playersao->isDead()) {
		verbosestream << "TOSERVER_INTERACT: " << player->getName()
		              << " tried to interact, but is dead!" << std::endl;
		return;
	}

	MAP_NOTHREAD_LOCK((&m_env->getMap()));

	v3f player_pos = playersao->getLastGoodPosition();

	// Update wielded item
	playersao->setWieldIndex(item_i);

	// Get pointed to node (undefined if not POINTEDTYPE_NODE)
	v3s16 p_under = pointed.node_undersurface;
	v3s16 p_above = pointed.node_abovesurface;

	// Get pointed to object (NULL if not POINTEDTYPE_OBJECT)
	ServerActiveObject *pointed_object = NULL;
	if(pointed.type == POINTEDTHING_OBJECT) {
		pointed_object = m_env->getActiveObject(pointed.object_id);
		if(pointed_object == NULL) {
			verbosestream << "TOSERVER_INTERACT: "
			              "pointed object is NULL" << std::endl;
			return;
		}

	}

	v3f pointed_pos_under = player_pos;
	v3f pointed_pos_above = player_pos;
	if(pointed.type == POINTEDTHING_NODE) {
		pointed_pos_under = intToFloat(p_under, BS);
		pointed_pos_above = intToFloat(p_above, BS);
	} else if(pointed.type == POINTEDTHING_OBJECT) {
		pointed_pos_under = pointed_object->getBasePosition();
		pointed_pos_above = pointed_pos_under;
	}

	/*
		Check that target is reasonably close
		(only when digging or placing things)
	*/
	static const bool enable_anticheat = !g_settings->getBool("disable_anticheat");
	if ((action == 0 || action == 2 || action == 3) &&
	        (enable_anticheat && !isSingleplayer())) {
		float d = player_pos.getDistanceFrom(pointed_pos_under);
		float max_d = BS * 14; // Just some large enough value
		if(d > max_d) {
			actionstream << "Player " << player->getName()
			             << " tried to access " << pointed.dump()
			             << " from too far: "
			             << "d=" << d << ", max_d=" << max_d
			             << ". ignoring." << std::endl;
			// Re-send block to revert change on client-side
			RemoteClient *client = getClient(peer_id);
			v3s16 blockpos = getNodeBlockPos(floatToInt(pointed_pos_under, BS));
			client->SetBlockNotSent(blockpos);
			// Call callbacks
			m_script->on_cheat(playersao, "interacted_too_far");
			// Do nothing else
			return;
		}
	}

	/*
		Make sure the player is allowed to do it
	*/
	if(!checkPriv(player->getName(), "interact")) {
		actionstream << player->getName() << " attempted to interact with "
		             << pointed.dump() << " without 'interact' privilege"
		             << std::endl;
		// Re-send block to revert change on client-side
		RemoteClient *client = getClient(peer_id);
		// Digging completed -> under
		if(action == 2) {
			v3s16 blockpos = getNodeBlockPos(floatToInt(pointed_pos_under, BS));
			client->SetBlockNotSent(blockpos);
		}
		// Placement -> above
		if(action == 3) {
			v3s16 blockpos = getNodeBlockPos(floatToInt(pointed_pos_above, BS));
			client->SetBlockNotSent(blockpos);
		}
		stat.add("interact_denied", player->getName());
		return;
	}

	/*
		If something goes wrong, this player is to blame
	*/
	RollbackScopeActor rollback_scope(m_rollback,
	                                  std::string("player:") + player->getName());

	/*
		0: start digging or punch object
	*/
	if(action == 0) {
		if(pointed.type == POINTEDTHING_NODE) {
			/*
				NOTE: This can be used in the future to check if
				somebody is cheating, by checking the timing.
			*/
			MapNode n = m_env->getMap().getNode(p_under);

			if (!n) {
				infostream << "Server: Not punching: Node not found."
				           << " Adding block to emerge queue."
				           << std::endl;
				m_emerge->enqueueBlockEmerge(peer_id, getNodeBlockPos(p_above), false);
			}

			if(n.getContent() != CONTENT_IGNORE)
				m_script->node_on_punch(p_under, n, playersao, pointed);
			// Cheat prevention
			playersao->noCheatDigStart(p_under);
		} else if(pointed.type == POINTEDTHING_OBJECT) {
			// Skip if object has been removed
			if(pointed_object->m_removed)
				return;

			actionstream << player->getName() << " punches object "
			             << pointed.object_id << ": "
			             << pointed_object->getDescription() << std::endl;

			ItemStack punchitem = playersao->getWieldedItem();
			ToolCapabilities toolcap =
			    punchitem.getToolCapabilities(m_itemdef);
			v3f dir = (pointed_object->getBasePosition() -
			           (playersao->getBasePosition() + playersao->getEyeOffset())
			          ).normalize();
			float time_from_last_punch =
			    playersao->resetTimeFromLastPunch();

			s16 src_original_hp = pointed_object->getHP();
			s16 dst_origin_hp = playersao->getHP();

			pointed_object->punch(dir, &toolcap, playersao,
			                      time_from_last_punch);

			// If the object is a player and its HP changed
			if (src_original_hp != pointed_object->getHP() &&
			        pointed_object->getType() == ACTIVEOBJECT_TYPE_PLAYER) {
				SendPlayerHPOrDie(((PlayerSAO*)pointed_object));
			}

			// If the puncher is a player and its HP changed
			if (dst_origin_hp != playersao->getHP()) {
				SendPlayerHPOrDie(playersao);
			}

			stat.add("punch", player->getName());
		}

	} // action == 0

	/*
		1: stop digging
	*/
	else if(action == 1) {
	} // action == 1

	/*
		2: Digging completed
	*/
	else if(action == 2) {
		// Only digging of nodes
		if(pointed.type == POINTEDTHING_NODE) {
			MapNode n = m_env->getMap().getNode(p_under);
			if (!n) {
				infostream << "Server: Not finishing digging: Node not found."
				           << " Adding block to emerge queue."
				           << std::endl;
				m_emerge->enqueueBlockEmerge(peer_id, getNodeBlockPos(p_above), false);
			}

			/* Cheat prevention */
			bool is_valid_dig = true;
			if (enable_anticheat && !isSingleplayer()) {
				v3s16 nocheat_p = playersao->getNoCheatDigPos();
				float nocheat_t = playersao->getNoCheatDigTime();
				playersao->noCheatDigEnd();
				// If player didn't start digging this, ignore dig
				if(nocheat_p != p_under) {
					infostream << "Server: NoCheat: " << player->getName()
					           << " started digging "
					           << PP(nocheat_p) << " and completed digging "
					           << PP(p_under) << "; not digging." << std::endl;
					is_valid_dig = false;
					// Call callbacks
					m_script->on_cheat(playersao, "finished_unknown_dig");
				}
				// Get player's wielded item
				ItemStack playeritem = playersao->getWieldedItem();
				ToolCapabilities playeritem_toolcap =
				    playeritem.getToolCapabilities(m_itemdef);
				// Get diggability and expected digging time
				DigParams params = getDigParams(m_nodedef->get(n).groups,
				                                &playeritem_toolcap);
				// If can't dig, try hand
				if(!params.diggable) {
					const ItemDefinition &hand = m_itemdef->get("");
					const ToolCapabilities *tp = hand.tool_capabilities;
					if(tp)
						params = getDigParams(m_nodedef->get(n).groups, tp);
				}
				// If can't dig, ignore dig
				if(!params.diggable) {
					infostream << "Server: NoCheat: " << player->getName()
					           << " completed digging " << PP(p_under)
					           << ", which is not diggable with tool. not digging."
					           << std::endl;
					is_valid_dig = false;
					// Call callbacks
					m_script->on_cheat(playersao, "dug_unbreakable");
				}
				// Check digging time
				// If already invalidated, we don't have to
				if(!is_valid_dig) {
					// Well not our problem then
				}
				// Clean and long dig
				else if(params.time > 2.0 && nocheat_t * 1.2 > params.time) {
					// All is good, but grab time from pool; don't care if
					// it's actually available
					playersao->getDigPool().grab(params.time);
				}
				// Short or laggy dig
				// Try getting the time from pool
				else if(playersao->getDigPool().grab(params.time)) {
					// All is good
				}
				// Dig not possible
				else {
					infostream << "Server: NoCheat: " << player->getName()
					           << " completed digging " << PP(p_under)
					           << "too fast; not digging." << std::endl;
					is_valid_dig = false;
					// Call callbacks
					m_script->on_cheat(playersao, "dug_too_fast");
				}
			}

			/* Actually dig node */

			if(is_valid_dig && n.getContent() != CONTENT_IGNORE) {
				m_script->node_on_dig(p_under, n, playersao);
				stat.add("dig", player->getName());
				stat.add("dig_" + m_nodedef->get(n).name , player->getName());
			}

			v3s16 blockpos = getNodeBlockPos(floatToInt(pointed_pos_under, BS));
			RemoteClient *client = getClient(peer_id);
			// Send unusual result (that is, node not being removed)
			if(m_env->getMap().getNode(p_under).getContent() != CONTENT_AIR) {
				// Re-send block to revert change on client-side
				client->SetBlockNotSent(blockpos);
			} else {
				client->ResendBlockIfOnWire(blockpos);
			}
			m_env->nodeUpdate(p_under, 5, 0);
		}
	} // action == 2

	/*
		3: place block or right-click object
	*/
	else if(action == 3) {
		ItemStack item = playersao->getWieldedItem();

		// Reset build time counter
		if(pointed.type == POINTEDTHING_NODE &&
		        item.getDefinition(m_itemdef).type == ITEM_NODE)
			getClient(peer_id)->m_time_from_building = 0.0;

		if(pointed.type == POINTEDTHING_OBJECT) {
			// Right click object

			// Skip if object has been removed
			if(pointed_object->m_removed)
				return;

			/* android bug - too many
							actionstream<<player->getName()<<" right-clicks object "
									<<pointed.object_id<<": "
									<<pointed_object->getDescription()<<std::endl;
			*/

			// Do stuff
			pointed_object->rightClick(playersao);
		} else if(m_script->item_OnPlace(
		              item, playersao, pointed)) {
			// Placement was handled in lua

			// Apply returned ItemStack
			if (playersao->setWieldedItem(item)) {
				SendInventory(playersao);
			}

			stat.add("place", player->getName());
			//stat.add("place_" + item.name, player->getName());
		}

		// If item has node placement prediction, always send the
		// blocks to make sure the client knows what exactly happened
		RemoteClient *client = getClient(peer_id);
		v3s16 blockpos = getNodeBlockPos(floatToInt(pointed_pos_above, BS));
		v3s16 blockpos2 = getNodeBlockPos(floatToInt(pointed_pos_under, BS));
		if(item.getDefinition(m_itemdef).node_placement_prediction != "") {
			client->SetBlockNotSent(blockpos);
			if(blockpos2 != blockpos) {
				client->SetBlockNotSent(blockpos2);
			}
		} else {
			client->ResendBlockIfOnWire(blockpos);
			if(blockpos2 != blockpos) {
				client->ResendBlockIfOnWire(blockpos2);
			}
		}
		m_env->nodeUpdate(p_under, 5, 0);
	} // action == 3

	/*
		4: use
	*/
	else if(action == 4) {
		ItemStack item = playersao->getWieldedItem();

		actionstream << player->getName() << " uses " << item.name
		             << ", pointing at " << pointed.dump() << std::endl;

		if(m_script->item_OnUse(
		            item, playersao, pointed)) {
			// Apply returned ItemStack
			if (playersao->setWieldedItem(item)) {
				SendInventory(playersao);
			}
			stat.add("use", player->getName());
			stat.add("use_" + item.name, player->getName());
			m_env->nodeUpdate(p_under, 5, 0);
		}

	} // action == 4

	/*
		5: rightclick air
	*/
	else if (action == 5) {
		ItemStack item = playersao->getWieldedItem();

		actionstream << player->getName() << " activates "
		             << item.name << std::endl;

		if (m_script->item_OnSecondaryUse(
		            item, playersao)) {
			if( playersao->setWieldedItem(item)) {
				SendInventory(playersao);
			}
		}
	}


	/*
		Catch invalid actions
	*/
	else {
		infostream << "WARNING: Server: Invalid action "
		           << action << std::endl;
	}

}

void Server::handleCommand_RemovedSounds(NetworkPacket* pkt) {
	const auto peer_id = pkt->getPeerId();
	auto & packet = *(pkt->packet);
	auto player = m_env->getPlayer(pkt->getPeerId());
	if (!player) {
		m_con.DisconnectPeer(pkt->getPeerId());
		return;
	}
/*
	auto playersao = player->getPlayerSAO();
	if (!playersao) {
		m_con.DisconnectPeer(pkt->getPeerId());
		return;
	}
*/

	std::vector<s32> removed_ids;
	packet[TOSERVER_REMOVED_SOUNDS_IDS].convert(removed_ids);
	for (auto & id : removed_ids) {
		auto i =
		    m_playing_sounds.find(id);
		if(i == m_playing_sounds.end())
			continue;
		ServerPlayingSound &psound = i->second;
		psound.clients.erase(peer_id);
		if(psound.clients.empty())
			m_playing_sounds.erase(i);
	}
}

void Server::handleCommand_NodeMetaFields(NetworkPacket* pkt) {
	//const auto peer_id = pkt->getPeerId();
	auto & packet = *(pkt->packet);
	auto player = m_env->getPlayer(pkt->getPeerId());
	if (!player) {
		m_con.DisconnectPeer(pkt->getPeerId());
		return;
	}
	auto playersao = player->getPlayerSAO();
	if (!playersao) {
		m_con.DisconnectPeer(pkt->getPeerId());
		return;
	}

	v3s16 p = packet[TOSERVER_NODEMETA_FIELDS_POS].as<v3s16>();
	std::string formname = packet[TOSERVER_NODEMETA_FIELDS_FORMNAME].as<std::string>();
	std::unordered_map<std::string, std::string> fields;
	packet[TOSERVER_NODEMETA_FIELDS_DATA].convert(fields);


	if (!m_enable_rollback_recording) {
		m_script->node_on_receive_fields(p, formname, fields, playersao);
	} else {
	// If something goes wrong, this player is to blame
	RollbackScopeActor rollback_scope(m_rollback,
	                                  std::string("player:") + player->getName());

	// Check the target node for rollback data; leave others unnoticed
	RollbackNode rn_old(&m_env->getMap(), p, this);

	m_script->node_on_receive_fields(p, formname, fields, playersao);

	// Report rollback data
	RollbackNode rn_new(&m_env->getMap(), p, this);
	if(rollback() && rn_new != rn_old) {
		RollbackAction action;
		action.setSetNode(p, rn_old, rn_new);
		rollback()->reportAction(action);
	}
	}
}

void Server::handleCommand_InventoryFields(NetworkPacket* pkt) {
	//const auto peer_id = pkt->getPeerId();
	auto & packet = *(pkt->packet);
	auto player = m_env->getPlayer(pkt->getPeerId());
	if (!player) {
		m_con.DisconnectPeer(pkt->getPeerId());
		return;
	}
	auto playersao = player->getPlayerSAO();
	if (!playersao) {
		m_con.DisconnectPeer(pkt->getPeerId());
		return;
	}

	std::string formname;
	std::unordered_map<std::string, std::string> fields;

	packet[TOSERVER_INVENTORY_FIELDS_FORMNAME].convert(formname);
	packet[TOSERVER_INVENTORY_FIELDS_DATA].convert(fields);

	m_script->on_playerReceiveFields(playersao, formname, fields);
}

void Server::handleCommand_FirstSrp(NetworkPacket* pkt) {
	/*
		const auto peer_id = pkt->getPeerId();
		auto & packet = *(pkt->packet);
		auto player = m_env->getPlayer(pkt->getPeerId());
		if (!player) {
			m_con.DisconnectPeer(pkt->getPeerId());
			return;
		}
		auto playersao = player->getPlayerSAO();
		if (!playersao) {
			m_con.DisconnectPeer(pkt->getPeerId());
			return;
		}
	*/
}

void Server::handleCommand_SrpBytesA(NetworkPacket* pkt) {
	/*
		const auto peer_id = pkt->getPeerId();
		auto & packet = *(pkt->packet);
		auto player = m_env->getPlayer(pkt->getPeerId());
		if (!player) {
			m_con.DisconnectPeer(pkt->getPeerId());
			return;
		}
		auto playersao = player->getPlayerSAO();
		if (!playersao) {
			m_con.DisconnectPeer(pkt->getPeerId());
			return;
		}
	*/
}

void Server::handleCommand_SrpBytesM(NetworkPacket* pkt) {
	/*
		const auto peer_id = pkt->getPeerId();
		auto & packet = *(pkt->packet);
		auto player = m_env->getPlayer(pkt->getPeerId());
		if (!player) {
			m_con.DisconnectPeer(pkt->getPeerId());
			return;
		}
		auto playersao = player->getPlayerSAO();
		if (!playersao) {
			m_con.DisconnectPeer(pkt->getPeerId());
			return;
		}
	*/
}
