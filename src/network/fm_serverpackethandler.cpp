/*
Copyright (C) 2024 proller <proler@gmail.com>
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

#include "config.h"
#if !MINETEST_PROTO

#include <algorithm>
#include <cmath>
#include <array>
#include <memory>
#include <optional>
#include <sstream>
#include <unordered_map>
#include <unordered_set>
#include <vector>

#include "chatmessage.h"
#include "clientdynamicinfo.h"
#include "emerge.h"
#include "itemdef.h"
#include "modchannels.h"
#include "nodedef.h"
#include "player.h"
#include "remoteplayer.h"
#include "rollback_interface.h"
#include "servermap.h"
#include "scripting_server.h"
#include "server.h"
#include "serverenvironment.h"
#include "server/player_sao.h"
#include "server/serverinventorymgr.h"
#include "settings.h"
#include "tool.h"
#include "util/auth.h"
#include "util/base64.h"
#include "util/serialize.h"
#include "util/srp.h"
#include "network/connection.h"
#include "network/networkexceptions.h"
#include "network/networkpacket.h"
#include "network/networkprotocol.h"
#include "network/serveropcodes.h"
#include "fm_networkprotocol.h"

void Server::handleCommand_Deprecated(NetworkPacket *pkt)
{
	auto &h = toServerCommandTable[pkt->getCommand()];
	infostream << "Server: ignoring unsupported " << h.name << " from peer "
			   << pkt->getPeerId() << std::endl;
}

void Server::handleCommand_Init(NetworkPacket *pkt)
{
	auto &packet = *(pkt->packet);
	const auto peer_id = pkt->getPeerId();
	RemoteClient *client = getClient(peer_id, CS_Created);

	Address addr;
	std::string addr_s;
	try {
		addr = m_con->GetPeerAddress(peer_id);
		addr_s = addr.serializeString();
	} catch (con::PeerNotFoundException &e) {
		/*
		 * no peer for this packet found
		 * most common reason is peer timeout, e.g. peer didn't
		 * respond for some time, your server was overloaded or
		 * things like that.
		 */
		infostream << "Server: peer " << peer_id << " not found during INIT?!"
				   << std::endl;
		return;
	}

	if (client->getState() > CS_Created) {
		verbosestream << "Server: Ignoring multiple TOSERVER_INITs from " << addr_s
					  << " (peer_id=" << peer_id << ")" << std::endl;
		return;
	}

	client->setCachedAddress(addr);

	verbosestream << "Server: Got TOSERVER_INIT from " << addr_s
				  << " (peer_id=" << peer_id << ")" << std::endl;

	if (denyIfBanned(peer_id))
		return;

	u8 max_ser_ver; // SER_FMT_VER_HIGHEST_READ (of client)
	u16 unused;
	u16 min_net_proto_version;
	u16 max_net_proto_version;
	std::string playerName;

	packet[TOSERVER_INIT_FMT].convert(max_ser_ver);
	packet[TOSERVER_INIT_COMPRESSION].convert(unused);
	packet[TOSERVER_INIT_PROTOCOL_VERSION_MIN].convert(min_net_proto_version);
	packet[TOSERVER_INIT_PROTOCOL_VERSION_MAX].convert(max_net_proto_version);
	packet[TOSERVER_INIT_NAME].convert(playerName);

	// Use the highest version supported by both
	const u8 serialization_ver = std::min(max_ser_ver, SER_FMT_VER_HIGHEST_WRITE);

	if (!ser_ver_supported_write(serialization_ver)) {
		actionstream << "Server: A mismatched client " << playerName
					 << " tried to connect from " << addr_s
					 << " ser_fmt_max=" << (int)serialization_ver << std::endl;
		DenyAccess(peer_id, SERVER_ACCESSDENIED_WRONG_VERSION);
		return;
	}

	client->setPendingSerializationVersion(serialization_ver);

	/*
		Read and check network protocol version
	*/

	u16 net_proto_version = 0;

	// Figure out a working version if it is possible at all
	if (max_net_proto_version >= SERVER_PROTOCOL_VERSION_MIN ||
			min_net_proto_version <= LATEST_PROTOCOL_VERSION) {
		// If maximum is larger than our maximum, go with our maximum
		if (max_net_proto_version > LATEST_PROTOCOL_VERSION)
			net_proto_version = LATEST_PROTOCOL_VERSION;
		// Else go with client's maximum
		else
			net_proto_version = max_net_proto_version;
	}

	verbosestream << "Server: " << addr_s
				  << ": Protocol version: min: " << min_net_proto_version
				  << ", max: " << max_net_proto_version
				  << ", chosen: " << net_proto_version << std::endl;

	client->net_proto_version = net_proto_version;

	if (net_proto_version < Server::getProtocolVersionMin() ||
			net_proto_version > Server::getProtocolVersionMax()) {
		actionstream << "Server: A mismatched client " << playerName
					 << " tried to connect from " << addr_s
					 << " proto_max=" << (int)max_net_proto_version << std::endl;
		DenyAccess(peer_id, SERVER_ACCESSDENIED_WRONG_VERSION);
		return;
	}

	/*
		Validate player name
	*/
	const char *playername = playerName.c_str();

	size_t pns = playerName.size();
	if (pns == 0 || pns > PLAYERNAME_SIZE) {
		actionstream << "Server: Player with "
					 << ((pns > PLAYERNAME_SIZE) ? "a too long" : "an empty")
					 << " name tried to connect from " << addr_s << std::endl;
		DenyAccess(peer_id, SERVER_ACCESSDENIED_WRONG_NAME);
		return;
	}

	if (!g_settings->getBool("enable_any_name") &&
			!string_allowed(playerName, PLAYERNAME_ALLOWED_CHARS)) {
		actionstream << "Server: Player with an invalid name tried to connect "
						"from "
					 << addr_s << std::endl;
		DenyAccess(peer_id, SERVER_ACCESSDENIED_WRONG_CHARS_IN_NAME);
		return;
	}

	// Do not allow multiple players in simple singleplayer mode
	if (isSingleplayer() && !m_clients.getClientIDs(CS_HelloSent).empty()) {
		infostream << "Server: Not allowing another client (" << addr_s
				   << ") to connect in simple singleplayer mode" << std::endl;
		DenyAccess(peer_id, SERVER_ACCESSDENIED_SINGLEPLAYER);
		return;
	}
	// Or the "singleplayer" name to be used on regular servers
	if (!isSingleplayer() && strcasecmp(playername, "singleplayer") == 0) {
		actionstream << "Server: Player with the name \"singleplayer\" tried "
						"to connect from "
					 << addr_s << std::endl;
		DenyAccess(peer_id, SERVER_ACCESSDENIED_WRONG_NAME);
		return;
	}

	{
		RemotePlayer *player = m_env->getPlayer(playername, true);
		// If player is already connected, cancel
		if (player && player->getPeerId() != PEER_ID_INEXISTENT) {
			actionstream
					<< "Server: Player with name \"" << playername
					<< "\" tried to connect, but player with same name is already connected"
					<< std::endl;
			DenyAccess(peer_id, SERVER_ACCESSDENIED_ALREADY_CONNECTED);
			return;
		}
	}

	client->setName(playerName);

	{
		std::string reason;
		if (m_script->on_prejoinplayer(playername, addr_s, &reason)) {
			actionstream << "Server: Player with the name \"" << playerName
						 << "\" tried to connect from " << addr_s
						 << " but was disallowed for the following reason: " << reason
						 << std::endl;
			DenyAccess(peer_id, SERVER_ACCESSDENIED_CUSTOM_STRING, reason);
			return;
		}
	}

	infostream << "Server: New connection: \"" << playerName << "\" from " << addr_s
			   << " (peer_id=" << peer_id << ")" << std::endl;

	// Early check for user limit, so the client doesn't need to run
	// through the join process only to be denied.
	if (checkUserLimit(playerName, addr_s)) {
		actionstream << "Server: " << playername << " tried to join from " << addr_s
					 << ", but the user limit was reached." << std::endl;
		DenyAccess(peer_id, SERVER_ACCESSDENIED_TOO_MANY_USERS);
		return;
	}

	/*
		Compose auth methods for answer
	*/
	std::string encpwd; // encrypted Password field for the user
	bool has_auth = m_script->getAuth(playername, &encpwd, nullptr);
	u32 auth_mechs = 0;

	client->chosen_mech = AUTH_MECHANISM_NONE;

	if (has_auth) {
		std::vector<std::string> pwd_components = str_split(encpwd, '#');
		if (pwd_components.size() == 4) {
			if (pwd_components[1] == "1") { // 1 means srp
				auth_mechs |= AUTH_MECHANISM_SRP;
				client->enc_pwd = encpwd;
			} else {
				actionstream << "User " << playername
							 << " tried to log in, "
								"but password field was invalid (unknown mechcode)."
							 << std::endl;
				DenyAccess(peer_id, SERVER_ACCESSDENIED_SERVER_FAIL);
				return;
			}
		} else if (base64_is_valid(encpwd)) {
			auth_mechs |= AUTH_MECHANISM_LEGACY_PASSWORD;
			client->enc_pwd = encpwd;
		} else {
			actionstream << "User " << playername
						 << " tried to log in, but "
							"password field was invalid (invalid base64)."
						 << std::endl;
			DenyAccess(peer_id, SERVER_ACCESSDENIED_SERVER_FAIL);
			return;
		}
	} else {
		std::string default_password = g_settings->get("default_password");
		if (isSingleplayer() || default_password.length() == 0) {
			auth_mechs |= AUTH_MECHANISM_FIRST_SRP;
		} else {
			// Take care of default passwords.
			client->enc_pwd = get_encoded_srp_verifier(playerName, default_password);
			auth_mechs |= AUTH_MECHANISM_SRP;
			// Allocate player in db, but only on successful login.
			client->create_player_on_auth_success = true;
		}
	}

	/*
		Answer with a TOCLIENT_HELLO
	*/

	verbosestream << "Sending TOCLIENT_HELLO with auth method field: " << auth_mechs
				  << std::endl;

	MSGPACK_PACKET_INIT((int)TOCLIENT_HELLO, 5);
	PACK(TOCLIENT_HELLO_SERIALIZATION_VERSION, serialization_ver);
	PACK(TOCLIENT_HELLO_COMPRESSION_MODE, (u16)0);
	PACK(TOCLIENT_HELLO_PROTOCOL_VERSION, net_proto_version);
	PACK(TOCLIENT_HELLO_AUTH_METHODS, auth_mechs);
	PACK(TOCLIENT_HELLO_LEGACY_PASSWORD, std::string());
	m_clients.send(peer_id, 1, buffer, true);

	client->allowed_auth_mechs = auth_mechs;

	m_clients.event(peer_id, CSE_Hello);
}

void Server::handleCommand_Init2(NetworkPacket *pkt)
{
	session_t peer_id = pkt->getPeerId();
	verbosestream << "Server: Got TOSERVER_INIT2 from " << peer_id << std::endl;

	RemoteClient *client = getClientNoEx(peer_id, CS_Invalid);
	if (!client || client->getState() != CS_AwaitingInit2) {
		actionstream << "Server: Ignoring TOSERVER_INIT2 in wrong state from " << peer_id
					 << std::endl;
		return;
	}

	m_clients.event(peer_id, CSE_GotInit2);
	u16 protocol_version = m_clients.getProtocolVersion(peer_id);

	std::string lang;
	pkt->packet->convert_safe(TOSERVER_INIT2_LANGUAGE, lang);

	/*
		Send some initialization data
	*/

	infostream << "Server: Sending content to " << getPlayerName(peer_id) << std::endl;

	// Send item definitions
	SendItemDef(peer_id, m_itemdef, protocol_version);

	// Send node definitions
	SendNodeDef(peer_id, m_nodedef, protocol_version);

	SendFreeminerInit(peer_id, protocol_version);

	m_clients.event(peer_id, CSE_SetDefinitionsSent);

	// Send media announcement
	sendMediaAnnouncement(peer_id, lang);

	// Keep client language for server translations
	client->setLangCode(lang);

	// Send active objects
	{
		PlayerSAO *sao = getPlayerSAO(peer_id);
		if (sao)
			SendActiveObjectRemoveAdd(client, sao);
	}

	// Send detached inventories
	sendDetachedInventories(peer_id, false);

	// Send player movement settings
	SendMovement(peer_id);

	// Send time of day
	u16 time = m_env->getTimeOfDay();
	float time_speed = g_settings->getFloat("time_speed");
	SendTimeOfDay(peer_id, time, time_speed);

	SendCSMRestrictionFlags(peer_id);
}

void Server::handleCommand_RequestMedia(NetworkPacket *pkt)
{
	std::unordered_set<std::string> tosend;
	const auto peer_id = pkt->getPeerId();
	pkt->packet->operator[](TOSERVER_REQUEST_MEDIA_FILES).convert(tosend);
	verbosestream << "Client " << getPlayerName(peer_id) << " requested " << tosend.size()
				  << " media file(s)" << std::endl;
	verbosestream << std::flush;

	sendRequestedMedia(peer_id, tosend);
}

void Server::handleCommand_ClientReady(NetworkPacket *pkt)
{
	session_t peer_id = pkt->getPeerId();
	RemoteClient *client = getClient(peer_id, CS_Created);
	assert(client);

	auto &packet = *(pkt->packet);
	// decode all information first
	u8 major_ver = 0, minor_ver = 0, patch_ver = 0;
	std::string reserved;
	u16 formspec_ver = 1; // v1 for clients older than 5.1.0-dev
	std::string full_ver;

	packet_convert_safe(packet, TOSERVER_CLIENT_READY_VERSION_MAJOR, major_ver);
	packet_convert_safe(packet, TOSERVER_CLIENT_READY_VERSION_MINOR, minor_ver);
	packet_convert_safe(packet, TOSERVER_CLIENT_READY_VERSION_PATCH, patch_ver);
	packet_convert_safe(packet, TOSERVER_CLIENT_READY_LEGACY_STRING, reserved);
	packet_convert_safe(packet, TOSERVER_CLIENT_READY_VERSION_STRING, full_ver);
	packet_convert_safe(packet, TOSERVER_CLIENT_READY_FORMSPEC_VERSION, formspec_ver);

	if (full_ver.empty())
		full_ver = reserved;

	client->setVersionInfo(major_ver, minor_ver, patch_ver, full_ver);

	// Since only active clients count for the user limit, two could race the
	// join process so we have to do a final check for the user limit here.
	std::string addr_s = client->getAddress().serializeString();
	if (checkUserLimit(client->getName(), addr_s)) {
		actionstream << "Server: " << client->getName() << " tried to join from "
					 << addr_s << ", but the user limit was reached (late)." << std::endl;
		DenyAccess(peer_id, SERVER_ACCESSDENIED_TOO_MANY_USERS);
		return;
	}

	// Emerge player
	PlayerSAO *playersao = StageTwoClientInit(peer_id);
	if (!playersao) {
		errorstream << "Server: stage 2 client init failed "
					   "peer_id="
					<< peer_id << std::endl;
		DisconnectPeer(peer_id);
		return;
	}

	playersao->getPlayer()->formspec_version = formspec_ver;
	m_clients.event(peer_id, CSE_SetClientReady);

	// Send player list to this client
	{
		const std::vector<std::string> &players = m_clients.getPlayerNames();
		MSGPACK_PACKET_INIT((int)TOCLIENT_UPDATE_PLAYER_LIST, 2);
		PACK(TOCLIENT_UPDATE_PLAYER_LIST_TYPE, (u8)PLAYER_LIST_INIT);
		PACK(TOCLIENT_UPDATE_PLAYER_LIST_NAMES, players);
		m_clients.send(peer_id, 0, buffer, true);
	}

	s64 last_login;
	m_script->getAuth(playersao->getPlayer()->getName(), nullptr, nullptr, &last_login);
	m_script->on_joinplayer(playersao, last_login);

	stat.add("join", playersao->getPlayer()->getName());

	playersao->last_time_online = m_uptime_counter->get();

	// Send shutdown timer if shutdown has been scheduled
	if (m_shutdown_state.isTimerRunning())
		SendChatMessage(peer_id, m_shutdown_state.getShutdownTimerMessage());
}

void Server::handleCommand_GotBlocks(NetworkPacket *pkt)
{
	// Kept for protocol compatibility. Freeminer uses its own block-emergence queue.
}

void Server::handleCommand_DeletedBlocks(NetworkPacket *pkt)
{
	std::vector<v3pos_t> blocks;
	pkt->packet->operator[](TOSERVER_DELETEDBLOCKS_DATA).convert(blocks);
	RemoteClient *client = getClient(pkt->getPeerId());
	for (const auto &pos : blocks)
		client->SetBlockDeleted(pos);
}

void Server::handleCommand_RemovedSounds(NetworkPacket *pkt)
{
	std::vector<s32> ids;
	pkt->packet->operator[](TOSERVER_REMOVED_SOUNDS_IDS).convert(ids);
	for (const auto id : ids) {
		auto it = m_playing_sounds.find(id);
		if (it == m_playing_sounds.end())
			continue;
		it->second.clients.erase(pkt->getPeerId());
		if (it->second.clients.empty())
			m_playing_sounds.erase(it);
	}
}

static bool sanitizeFormspecFields(StringMap &fields)
{
	size_t length = 0;
	StringMap sanitized;
	sanitized.reserve(fields.size());
	for (const auto &entry : fields) {
		auto fieldname = sanitize_untrusted(entry.first, false);
		auto fieldvalue = sanitize_untrusted(entry.second);
		length += fieldname.size() + fieldvalue.size();
		if (length >= 640 * 1024)
			return false;
		sanitized[std::move(fieldname)] = std::move(fieldvalue);
	}
	fields = std::move(sanitized);
	return true;
}

void Server::process_PlayerPos(
		RemotePlayer *player, PlayerSAO *playersao, NetworkPacket *pkt)
{
	const bool interaction = pkt->getCommand() == TOSERVER_INTERACT;
	const auto key = [interaction](packet_field_t player_field) {
		return static_cast<packet_field_t>(
				player_field + (interaction ? TOSERVER_INTERACT_POSITION : 0));
	};
	auto &packet = *(pkt->packet);
	v3opos_t position(0.0f, 0.0f, 0.0f);
	v3opos_t speed(0.0f, 0.0f, 0.0f);
	f32 pitch = 0.0f, yaw = 0.0f;
	v3s32 ps, ss;
	s32 scaled_pitch, scaled_yaw;
	if (packet_convert_safe(packet, key(TOSERVER_PLAYERPOS_POSITION_SCALED), ps) &&
			packet_convert_safe(packet, key(TOSERVER_PLAYERPOS_SPEED_SCALED), ss) &&
			packet_convert_safe(
					packet, key(TOSERVER_PLAYERPOS_PITCH_SCALED), scaled_pitch) &&
			packet_convert_safe(packet, key(TOSERVER_PLAYERPOS_YAW_SCALED), scaled_yaw)) {
		position = v3f(ps.X / 100.0f, ps.Y / 100.0f, ps.Z / 100.0f);
		speed = v3f(ss.X / 100.0f, ss.Y / 100.0f, ss.Z / 100.0f);
		pitch = scaled_pitch / 100.0f;
		yaw = scaled_yaw / 100.0f;
	} else {
		packet.convert_safe(key(TOSERVER_PLAYERPOS_POSITION), position);
		packet.convert_safe(key(TOSERVER_PLAYERPOS_SPEED), speed);
		packet.convert_safe(key(TOSERVER_PLAYERPOS_PITCH), pitch);
		packet.convert_safe(key(TOSERVER_PLAYERPOS_YAW), yaw);
	}

	u32 key_pressed = 0;
	packet.convert_safe(key(TOSERVER_PLAYERPOS_KEY_PRESSED), key_pressed);
	player->control.unpackKeysPressed(key_pressed);
	u8 packed_fov = 0;
	packet.convert_safe(key(TOSERVER_PLAYERPOS_FOV), packed_fov);
	const f32 fov = packed_fov / 80.0f;
	u8 wanted_range = 0;
	packet.convert_safe(key(TOSERVER_PLAYERPOS_WANTED_RANGE), wanted_range);
	bool camera_inverted = false;
	packet.convert_safe(key(TOSERVER_PLAYERPOS_CAMERA_INVERTED), camera_inverted);

	f32 movement_speed = 0.0f;
	f32 movement_direction = 0.0f;
	const bool have_movement_data =
			packet_convert_safe(
					packet, key(TOSERVER_PLAYERPOS_MOVEMENT_SPEED), movement_speed) &&
			packet_convert_safe(packet, key(TOSERVER_PLAYERPOS_MOVEMENT_DIRECTION),
					movement_direction);
	if (have_movement_data) {
		player->control.movement_speed = std::isfinite(movement_speed)
												 ? std::clamp(movement_speed, 0.0f, 1.0f)
												 : 0.0f;
		player->control.movement_direction =
				std::isfinite(movement_direction) ? movement_direction : 0.0f;
	} else {
		player->control.movement_speed = 0.0f;
		player->control.movement_direction = 0.0f;
		player->control.setMovementFromKeys();
	}
	if (!std::isfinite(position.X) || !std::isfinite(position.Y) ||
			!std::isfinite(position.Z) || !std::isfinite(speed.X) ||
			!std::isfinite(speed.Y) || !std::isfinite(speed.Z) || !std::isfinite(pitch) ||
			!std::isfinite(yaw)) {
		m_script->on_cheat(playersao, "invalid_position");
		SendMovePlayer(playersao);
		return;
	}

	pitch = modulo360f(pitch);
	yaw = wrapDegrees_0_360(yaw);

	if (!playersao->isAttached()) {
		// Only update player positions when moving freely
		// to not interfere with attachment handling
		playersao->setBasePosition(position);
		player->setSpeed(speed);
	}
	playersao->setLookPitch(pitch);
	playersao->setPlayerYaw(yaw);
	playersao->setFov(fov);
	playersao->setWantedRange(wanted_range);
	playersao->setCameraInverted(camera_inverted);

	getClient(pkt->getPeerId())->wanted_range = wanted_range;

	if (playersao->checkMovementCheat()) {
		// Call callbacks
		m_script->on_cheat(playersao, "moved_too_fast");
		SendMovePlayer(playersao);
	}
	// copypaste from fm_serverpackethandler.cpp
	else if (playersao->m_ms_from_last_respawn > 3000) {
		const auto dist =
				(position / BS).getDistanceFrom(playersao->m_last_stat_position / BS);
		// distance per step (1/10s)
		if (dist && dist < 8) {
			stat.add("move", playersao->getPlayer()->getName(), dist);
		}
		playersao->m_last_stat_position = position;
	}

	/*
		if (playersao->m_ms_from_last_respawn > 2000) {
			auto obj = playersao; // copypasted from server step:
			auto uptime = m_uptime_counter->get();
			if (!obj->m_uptime_last)  // not very good place, but minimum modifications
				obj->m_uptime_last = uptime - 0.1;
			if (uptime - obj->m_uptime_last > 0.5) {
				obj->step(uptime - obj->m_uptime_last, true); //todo: maybe limit count per time
				obj->m_uptime_last = uptime;
			}
		}
*/
	//copypaste end
}

void Server::handleCommand_PlayerPos(NetworkPacket *pkt)
{
	session_t peer_id = pkt->getPeerId();
	RemotePlayer *player = m_env->getPlayer(peer_id);
	if (!player) {
		warningstream << FUNCTION_NAME << ": player is null" << std::endl;
		return;
	}

	PlayerSAO *playersao = player->getPlayerSAO();
	if (!playersao) {
		warningstream << FUNCTION_NAME << ": player SAO is null" << std::endl;
		return;
	}

	// If player is dead we don't care of this packet
	if (playersao->isDead()) {
		verbosestream << "TOSERVER_PLAYERPOS: " << player->getName()
					  << " is dead. Ignoring packet";
		return;
	}

	process_PlayerPos(player, playersao, pkt);
}

void Server::handleCommand_InventoryAction(NetworkPacket *pkt)
{
	session_t peer_id = pkt->getPeerId();
	RemotePlayer *player = m_env->getPlayer(peer_id);
	if (!player) {
		warningstream << FUNCTION_NAME << ": player is null" << std::endl;
		return;
	}

	PlayerSAO *playersao = player->getPlayerSAO();
	if (!playersao) {
		warningstream << FUNCTION_NAME << ": player SAO is null" << std::endl;
		return;
	}

	// Strip command and create a stream
	std::string data;
	pkt->packet->operator[](TOSERVER_INVENTORY_ACTION_DATA).convert(data);
	std::istringstream is(data, std::ios_base::binary);
	// Create an action
	std::unique_ptr<InventoryAction> a(InventoryAction::deSerialize(is));
	if (!a) {
		infostream << "TOSERVER_INVENTORY_ACTION: "
				   << "InventoryAction::deSerialize() returned NULL" << std::endl;
		return;
	}

	// If something goes wrong, this player is to blame
	RollbackScopeActor rollback_scope(m_rollback, "player:" + player->getName());

	/*
		Note: Always set inventory not sent, to repair cases
		where the client made a bad prediction.
	*/

	auto mark_player_inv_list_dirty = [this](const InventoryLocation &loc,
											  const std::string &list_name) {
		// Undo the client prediction of the affected list. See `clientApply`.
		if (loc.type != InventoryLocation::PLAYER)
			return;

		Inventory *inv = m_inventory_mgr->getInventory(loc);
		if (!inv)
			return;

		InventoryList *list = inv->getList(list_name);
		if (!list)
			return;

		list->setModified(true);
	};

	const bool player_has_interact = checkPriv(player->getName(), "interact");

	auto check_inv_access = [player, player_has_interact, this](
									const InventoryLocation &loc) -> bool {
		// Players without interact may modify their own inventory
		if (!player_has_interact && loc.type != InventoryLocation::PLAYER) {
			infostream << "Cannot modify foreign inventory: " << "No interact privilege"
					   << std::endl;
			return false;
		}

		switch (loc.type) {
		case InventoryLocation::CURRENT_PLAYER:
			// Only used internally on the client, never sent
			return false;
		case InventoryLocation::PLAYER:
			// Allow access to own inventory in all cases
			return loc.name == player->getName();
		case InventoryLocation::NODEMETA: {
			// Check for out-of-range interaction
			v3f node_pos = intToFloat(loc.p, BS);
			v3f player_pos = player->getPlayerSAO()->getEyePosition();
			f32 d = player_pos.getDistanceFrom(node_pos);
			return checkInteractDistance(player, d, "inventory");
		}
		case InventoryLocation::DETACHED:
			return getInventoryMgr()->checkDetachedInventoryAccess(
					loc, player->getName());
		default:
			return false;
		}
	};

	/*
		Handle restrictions and special cases of the move action
	*/
	if (a->getType() == IAction::Move) {
		IMoveAction *ma = (IMoveAction *)a.get();

		ma->from_inv.applyCurrentPlayer(player->getName());
		ma->to_inv.applyCurrentPlayer(player->getName());

		m_inventory_mgr->setInventoryModified(ma->from_inv);
		mark_player_inv_list_dirty(ma->from_inv, ma->from_list);
		bool inv_different = ma->from_inv != ma->to_inv;
		if (inv_different)
			m_inventory_mgr->setInventoryModified(ma->to_inv);
		if (inv_different || ma->from_list != ma->to_list)
			mark_player_inv_list_dirty(ma->to_inv, ma->to_list);

		if (!check_inv_access(ma->from_inv) || !check_inv_access(ma->to_inv))
			return;

		/*
			Disable moving items out of craftpreview
		*/
		if (ma->from_list == "craftpreview") {
			infostream << "Ignoring IMoveAction from " << (ma->from_inv.dump()) << ":"
					   << ma->from_list << " to " << (ma->to_inv.dump()) << ":"
					   << ma->to_list << " because src is " << ma->from_list << std::endl;
			return;
		}

		/*
			Disable moving items into craftresult and craftpreview
		*/
		if (ma->to_list == "craftpreview" || ma->to_list == "craftresult") {
			infostream << "Ignoring IMoveAction from " << (ma->from_inv.dump()) << ":"
					   << ma->from_list << " to " << (ma->to_inv.dump()) << ":"
					   << ma->to_list << " because dst is " << ma->to_list << std::endl;
			return;
		}
	}
	/*
		Handle restrictions and special cases of the drop action
	*/
	else if (a->getType() == IAction::Drop) {
		IDropAction *da = (IDropAction *)a.get();

		da->from_inv.applyCurrentPlayer(player->getName());

		m_inventory_mgr->setInventoryModified(da->from_inv);
		mark_player_inv_list_dirty(da->from_inv, da->from_list);

		/*
			Disable dropping items out of craftpreview
		*/
		if (da->from_list == "craftpreview") {
			infostream << "Ignoring IDropAction from " << (da->from_inv.dump()) << ":"
					   << da->from_list << " because src is " << da->from_list
					   << std::endl;
			return;
		}

		// Disallow dropping items if not allowed to interact
		if (!player_has_interact || !check_inv_access(da->from_inv))
			return;

		// Disallow dropping items if dead
		if (playersao->isDead()) {
			infostream << "Ignoring IDropAction from " << (da->from_inv.dump()) << ":"
					   << da->from_list << " because player is dead." << std::endl;
			return;
		}

		stat.add("drop", player->getName());

	}
	/*
		Handle restrictions and special cases of the craft action
	*/
	else if (a->getType() == IAction::Craft) {
		ICraftAction *ca = (ICraftAction *)a.get();

		ca->craft_inv.applyCurrentPlayer(player->getName());

		m_inventory_mgr->setInventoryModified(ca->craft_inv);
		// Note: `ICraftAction::clientApply` is empty, thus nothing to revert.

		// Disallow crafting if not allowed to interact
		if (!player_has_interact) {
			infostream << "Cannot craft: " << "No interact privilege" << std::endl;
			return;
		}

		if (!check_inv_access(ca->craft_inv))
			return;
		stat.add("craft", player->getName());
	} else {
		// Unknown action. Ignored.
		return;
	}

	// Do the action
	a->apply(m_inventory_mgr.get(), playersao, this);
}

void Server::handleCommand_ChatMessage(NetworkPacket *pkt)
{
	std::wstring message;
	std::string utf8_message;
	pkt->packet->operator[](TOSERVER_CHAT_MESSAGE_DATA).convert(utf8_message);
	message = utf8_to_wide(utf8_message);

	session_t peer_id = pkt->getPeerId();
	RemotePlayer *player = m_env->getPlayer(peer_id);
	if (!player) {
		warningstream << FUNCTION_NAME << ": player is null" << std::endl;
		return;
	}

	const auto &name = player->getName();

	std::wstring answer_to_sender = handleChat(name, message, true, player);
	if (!answer_to_sender.empty()) {
		// Send the answer to sender
		SendChatMessage(peer_id, ChatMessage(CHATMESSAGE_TYPE_SYSTEM, answer_to_sender));
	}
}

void Server::handleCommand_Damage(NetworkPacket *pkt)
{
	u16 damage;

	pkt->packet->operator[](TOSERVER_DAMAGE_VALUE).convert(damage);

	session_t peer_id = pkt->getPeerId();
	RemotePlayer *player = m_env->getPlayer(peer_id);
	if (!player) {
		warningstream << FUNCTION_NAME << ": player is null" << std::endl;
		return;
	}

	PlayerSAO *playersao = player->getPlayerSAO();
	if (!playersao) {
		warningstream << FUNCTION_NAME << ": player SAO is null" << std::endl;
		return;
	}

	if (playersao->getHP() && !playersao->isImmortal()) {
		if (playersao->isDead()) {
			verbosestream << "Server: "
							 "Ignoring damage as player "
						  << player->getName() << " is already dead" << std::endl;
			return;
		}

		actionstream << player->getName() << " damaged by " << (int)damage << " hp at "
					 << (playersao->getBasePosition() / BS) << std::endl;

		PlayerHPChangeReason reason(PlayerHPChangeReason::FALL);
		playersao->setHP((s32)playersao->getHP() - (s32)damage, reason, true);

		stat.add("damage", player->getName(), damage);
	}
}

void Server::handleCommand_PlayerItem(NetworkPacket *pkt)
{
	session_t peer_id = pkt->getPeerId();
	RemotePlayer *player = m_env->getPlayer(peer_id);
	if (!player) {
		warningstream << FUNCTION_NAME << ": player is null" << std::endl;
		return;
	}

	PlayerSAO *playersao = player->getPlayerSAO();
	if (!playersao) {
		warningstream << FUNCTION_NAME << ": player SAO is null" << std::endl;
		return;
	}

	u16 item;

	pkt->packet->operator[](TOSERVER_PLAYERITEM_VALUE).convert(item);

	if (player->getMaxHotbarItemcount() == 0) {
		return; // ignore silently
	} else if (item >= player->getMaxHotbarItemcount()) {
		actionstream << "Player " << player->getName() << " tried to access item=" << item
					 << " out of hotbar_itemcount=" << player->getMaxHotbarItemcount()
					 << "; ignoring." << std::endl;
		return;
	}

	playersao->getPlayer()->setWieldIndex(item);
}

bool Server::checkInteractDistance(
		RemotePlayer *player, const f32 d, const std::string &what)
{
	ItemStack selected_item, hand_item;
	const ItemStack &tool_item = player->getWieldedItem(&selected_item, &hand_item);
	f32 max_d = BS * getToolRange(tool_item, hand_item, m_itemdef);

	// Cube diagonal * 1.5 for maximal supported node extents:
	// sqrt(3) * 1.5 ≅ 2.6
	if (d > max_d + 2.6f * BS) {
		actionstream << "Player " << player->getName() << " tried to access " << what
					 << " from too far: " << "d=" << d << ", max_d=" << max_d
					 << "; ignoring." << std::endl;
		// Call callbacks
		m_script->on_cheat(player->getPlayerSAO(), "interacted_too_far");
		return false;
	}
	return true;
}

static inline void getWieldedItem(
		const PlayerSAO *playersao, std::optional<ItemStack> &ret)
{
	ret = ItemStack();
	playersao->getWieldedItem(&ret.value());
}

void Server::handleCommand_Interact(NetworkPacket *pkt)
{
	/*
		[0] u16 command
		[2] u8 action
		[3] u16 item
		[5] u32 length of the next item (plen)
		[9] serialized PointedThing
		[9 + plen] player position information
	*/

	auto &packet = *(pkt->packet);
	u8 action_raw;
	packet[TOSERVER_INTERACT_ACTION].convert(action_raw);
	const auto action = static_cast<InteractAction>(action_raw);
	u16 item_i;
	packet[TOSERVER_INTERACT_ITEM].convert(item_i);
	PointedThing pointed;
	packet[TOSERVER_INTERACT_POINTED_THING].convert(pointed);

	if (overload) {
		if (pointed.type == POINTEDTHING_NOTHING || action == 1)
			return;
		//errorstream<<"overload pointed peer_id=" << peer_id << " action=" << (int)action  << " pointed.type="<<pointed.type<< "\n";
	}

	verbosestream << "TOSERVER_INTERACT: action=" << (int)action << ", item=" << item_i
				  << ", pointed=" << pointed.dump() << std::endl;

	session_t peer_id = pkt->getPeerId();
	RemotePlayer *player = m_env->getPlayer(peer_id);
	if (!player) {
		warningstream << FUNCTION_NAME << ": player is null" << std::endl;
		return;
	}

	PlayerSAO *playersao = player->getPlayerSAO();
	if (!playersao) {
		warningstream << FUNCTION_NAME << ": player SAO is null" << std::endl;
		return;
	}

	if (playersao->isDead()) {
		actionstream << "Server: " << player->getName()
					 << " tried to interact while dead; ignoring." << std::endl;
		if (pointed.type == POINTEDTHING_NODE) {
			// Re-send block to revert change on client-side
			RemoteClient *client = getClient(peer_id);
			v3bpos_t blockpos = getNodeBlockPos(pointed.node_undersurface);
			client->SetBlockNotSent(blockpos);
		}
		// Call callbacks
		m_script->on_cheat(playersao, "interacted_while_dead");
		return;
	}

	process_PlayerPos(player, playersao, pkt);

	v3f player_pos = playersao->getLastGoodPosition();

	// Update wielded item
	if (player->getMaxHotbarItemcount() == 0) {
		return; // ignore silently
	} else if (item_i >= player->getMaxHotbarItemcount()) {
		actionstream << "Player " << player->getName()
					 << " tried to access item=" << item_i
					 << " out of hotbar_itemcount=" << player->getMaxHotbarItemcount()
					 << "; ignoring." << std::endl;
		return;
	}

	player->setWieldIndex(item_i);

	// Get pointed to object (NULL if not POINTEDTYPE_OBJECT)
	ServerActiveObject *pointed_object = NULL;
	if (pointed.type == POINTEDTHING_OBJECT) {
		pointed_object = m_env->getActiveObject(pointed.object_id);
		if (pointed_object == NULL) {
			verbosestream << "TOSERVER_INTERACT: "
							 "pointed object is NULL"
						  << std::endl;
			return;
		}
	}

	/*
		Make sure the player is allowed to do it
	*/
	if (!checkPriv(player->getName(), "interact")) {
		actionstream << player->getName() << " attempted to interact with "
					 << pointed.dump() << " without 'interact' privilege" << std::endl;

		if (pointed.type != POINTEDTHING_NODE)
			return;

		// Re-send block to revert change on client-side
		RemoteClient *client = getClient(peer_id);
		// Digging completed -> under
		if (action == INTERACT_DIGGING_COMPLETED) {
			v3bpos_t blockpos = getNodeBlockPos(pointed.node_undersurface);
			client->SetBlockNotSent(blockpos);
		}
		// Placement -> above
		else if (action == INTERACT_PLACE) {
			v3bpos_t blockpos = getNodeBlockPos(pointed.node_abovesurface);
			client->SetBlockNotSent(blockpos);
		}
		return;
	}

	/*
		Check that target is reasonably close
	*/
	static thread_local const u32 anticheat_flags =
			g_settings->getFlagStr("anticheat_flags", flagdesc_anticheat, nullptr);

	if ((action == INTERACT_START_DIGGING || action == INTERACT_DIGGING_COMPLETED ||
				action == INTERACT_PLACE || action == INTERACT_USE) &&
			(anticheat_flags & AC_INTERACTION) && !isSingleplayer()) {
		v3f target_pos = player_pos;
		if (pointed.type == POINTEDTHING_NODE) {
			target_pos = intToFloat(pointed.node_undersurface, BS);
		} else if (pointed.type == POINTEDTHING_OBJECT) {
			if (playersao->getId() == pointed_object->getId()) {
				actionstream << "Server: " << player->getName()
							 << " attempted to interact with themselves" << std::endl;
				m_script->on_cheat(playersao, "interacted_with_self");
				return;
			}
			target_pos = pointed_object->getBasePosition();
		}
		float d = playersao->getEyePosition().getDistanceFrom(target_pos);

		if (!checkInteractDistance(player, d, pointed.dump())) {
			if (pointed.type == POINTEDTHING_NODE) {
				// Re-send block to revert change on client-side
				RemoteClient *client = getClient(peer_id);
				v3bpos_t blockpos = getNodeBlockPos(pointed.node_undersurface);
				client->SetBlockNotSent(blockpos);
			}
			return;
		}
	}

	/*
		If something goes wrong, this player is to blame
	*/
	RollbackScopeActor rollback_scope(m_rollback, "player:" + player->getName());

	switch (action) {
	// Start digging or punch object
	case INTERACT_START_DIGGING: {
		if (pointed.type == POINTEDTHING_NODE) {
			MapNode n(CONTENT_IGNORE);
			bool pos_ok;

			v3pos_t p_under = pointed.node_undersurface;
			n = m_env->getMap().getNode(p_under, &pos_ok);
			if (!pos_ok) {
				infostream << "Server: Not punching: Node not found. "
							  "Adding block to emerge queue."
						   << std::endl;
				m_emerge->enqueueBlockEmerge(
						peer_id, getNodeBlockPos(pointed.node_abovesurface), false);
			}

			if (n.getContent() != CONTENT_IGNORE)
				m_script->node_on_punch(p_under, n, playersao, pointed);

			// Cheat prevention
			playersao->noCheatDigStart(p_under);

			return;
		}

		// Skip if the object can't be interacted with anymore
		if (pointed.type != POINTEDTHING_OBJECT || pointed_object->isGone())
			return;

		ItemStack selected_item, hand_item;
		ItemStack tool_item = playersao->getWieldedItem(&selected_item, &hand_item);
		const ToolCapabilities &toolcap =
				tool_item.getToolCapabilities(m_itemdef, &hand_item);
		v3f dir = (pointed_object->getBasePosition() -
				   (playersao->getBasePosition() + playersao->getEyeOffset()))
						  .normalize();
		float time_from_last_punch = playersao->resetTimeFromLastPunch();

		u32 wear = pointed_object->punch(
				dir, toolcap, playersao, time_from_last_punch, tool_item.wear);

		stat.add("punch", player->getName());

		// Callback may have changed item, so get it again
		playersao->getWieldedItem(&selected_item);
		bool changed = selected_item.addWear(wear, m_itemdef);
		if (changed)
			playersao->setWieldedItem(selected_item);

		return;
	} // action == INTERACT_START_DIGGING

	case INTERACT_STOP_DIGGING:
		// Nothing to do
		return;

	case INTERACT_DIGGING_COMPLETED: {
		// Only digging of nodes
		if (pointed.type != POINTEDTHING_NODE)
			return;
		bool pos_ok;
		v3pos_t p_under = pointed.node_undersurface;
		MapNode n = m_env->getMap().getNode(p_under, &pos_ok);
		if (!pos_ok) {
			infostream << "Server: Not finishing digging: Node not found. "
						  "Adding block to emerge queue."
					   << std::endl;
			m_emerge->enqueueBlockEmerge(
					peer_id, getNodeBlockPos(pointed.node_abovesurface), false);
		}

		/* Cheat prevention */
		bool is_valid_dig = true;
		if ((anticheat_flags & AC_DIGGING) && !isSingleplayer()) {
			v3pos_t nocheat_p = playersao->getNoCheatDigPos();
			float nocheat_t = playersao->getNoCheatDigTime();
			playersao->noCheatDigEnd();
			// If player didn't start digging this, ignore dig
			if (nocheat_p != p_under) {
				infostream << "Server: " << player->getName() << " started digging "
						   << nocheat_p << " and completed digging " << p_under
						   << "; not digging." << std::endl;
				is_valid_dig = false;
				// Call callbacks
				m_script->on_cheat(playersao, "finished_unknown_dig");
			}

			// Get player's wielded item
			// See also: Game::handleDigging
			ItemStack selected_item, hand_item;
			ItemStack &tool_item = player->getWieldedItem(&selected_item, &hand_item);

			// Get diggability and expected digging time
			DigParams params = getDigParams(m_nodedef->get(n).groups,
					&tool_item.getToolCapabilities(m_itemdef, &hand_item),
					tool_item.wear);
			// If can't dig, try hand
			if (!params.diggable) {
				params = getDigParams(m_nodedef->get(n).groups,
						&hand_item.getToolCapabilities(m_itemdef));
			}
			// If can't dig, ignore dig
			if (!params.diggable) {
				infostream << "Server: " << player->getName() << " completed digging "
						   << p_under << ", which is not diggable with tool; not digging."
						   << std::endl;
				is_valid_dig = false;
				// Call callbacks
				m_script->on_cheat(playersao, "dug_unbreakable");
			}
			// Check digging time
			// If already invalidated, we don't have to
			if (!is_valid_dig) {
				// Well not our problem then
			}
			// Clean and long dig
			else if (params.time > 2.0 && nocheat_t * 1.2 > params.time) {
				// All is good, but grab time from pool; don't care if
				// it's actually available
				playersao->getDigPool().grab(params.time);
			}
			// Short or laggy dig
			// Try getting the time from pool
			else if (playersao->getDigPool().grab(params.time)) {
				// All is good
				m_env->nodeUpdate(p_under, 5, 0);
			}
			// Dig not possible
			else {
				infostream << "Server: " << player->getName() << " completed digging "
						   << p_under << "too fast; not digging." << std::endl;
				is_valid_dig = false;
				// Call callbacks
				m_script->on_cheat(playersao, "dug_too_fast");
			}
		}

		if (is_valid_dig) {
			stat.add("dig", player->getName());
			stat.add("dig_" + m_nodedef->get(n).name, player->getName());
		}

		/* Actually dig node */

		if (is_valid_dig && n.getContent() != CONTENT_IGNORE)
			m_script->node_on_dig(p_under, n, playersao);

		v3bpos_t blockpos = getNodeBlockPos(p_under);
		RemoteClient *client = getClient(peer_id);
		// Send unusual result (that is, node not being removed)
		if (m_env->getMap().getNode(p_under).getContent() != CONTENT_AIR)
			// Re-send block to revert change on client-side
			client->SetBlockNotSent(blockpos);
		else
			client->ResendBlockIfOnWire(blockpos);

		return;
	} // action == INTERACT_DIGGING_COMPLETED

	// Place block or right-click object
	case INTERACT_PLACE: {
		std::optional<ItemStack> selected_item;
		getWieldedItem(playersao, selected_item);

		const bool had_prediction = !selected_item->getDefinition(m_itemdef)
											 .node_placement_prediction.empty();

		if (pointed.type == POINTEDTHING_OBJECT) {
			// Right click object

			// Skip if object can't be interacted with anymore
			if (pointed_object->isGone())
				return;

			actionstream << player->getName() << " right-clicks object "
						 << pointed.object_id << ": " << pointed_object->getDescription()
						 << std::endl;

			// Do stuff
			if (m_script->item_OnSecondaryUse(selected_item, playersao, pointed)) {
				if (selected_item.has_value() &&
						playersao->setWieldedItem(*selected_item))
					SendInventory(player, true);
			}

			// on_secondary_use might have removed the object
			if (pointed_object->isGone())
				return;

			pointed_object->rightClick(playersao);
		} else if (m_script->item_OnPlace(selected_item, playersao, pointed)) {
			// Placement was handled in lua

			stat.add("place", player->getName());

			// Apply returned ItemStack
			if (selected_item.has_value() && playersao->setWieldedItem(*selected_item))
				SendInventory(player, true);
		}

		if (pointed.type != POINTEDTHING_NODE)
			return;

		getClient(peer_id)->m_time_from_building = 0;

		// If item has node placement prediction, always send the
		// blocks to make sure the client knows what exactly happened
		RemoteClient *client = getClient(peer_id);
		v3bpos_t blockpos = getNodeBlockPos(pointed.node_abovesurface);
		v3bpos_t blockpos2 = getNodeBlockPos(pointed.node_undersurface);
		if (had_prediction) {
			client->SetBlockNotSent(blockpos);
			if (blockpos2 != blockpos)
				client->SetBlockNotSent(blockpos2);
		} else {
			client->ResendBlockIfOnWire(blockpos);
			if (blockpos2 != blockpos)
				client->ResendBlockIfOnWire(blockpos2);
		}

		return;
	} // action == INTERACT_PLACE

	case INTERACT_USE: {
		std::optional<ItemStack> selected_item;
		getWieldedItem(playersao, selected_item);

		actionstream << player->getName() << " uses " << selected_item->name
					 << ", pointing at " << pointed.dump() << std::endl;

		if (m_script->item_OnUse(selected_item, playersao, pointed)) {
			// Apply returned ItemStack
			if (selected_item.has_value() && playersao->setWieldedItem(*selected_item))
				SendInventory(player, true);

			stat.add("use", player->getName());
			if (selected_item.has_value())
				stat.add("use_" + selected_item->name, player->getName());
		}

		return;
	}

	// Rightclick air
	case INTERACT_ACTIVATE: {
		std::optional<ItemStack> selected_item;
		getWieldedItem(playersao, selected_item);

		actionstream << player->getName() << " activates " << selected_item->name
					 << std::endl;

		pointed.type = POINTEDTHING_NOTHING; // can only ever be NOTHING

		if (m_script->item_OnSecondaryUse(selected_item, playersao, pointed)) {
			// Apply returned ItemStack
			if (selected_item.has_value() && playersao->setWieldedItem(*selected_item))
				SendInventory(player, true);
		}

		return;
	}

	default:
		warningstream << "Server: Invalid action " << action << std::endl;
	}
}

void Server::handleCommand_NodeMetaFields(NetworkPacket *pkt)
{
	session_t peer_id = pkt->getPeerId();
	RemotePlayer *player = m_env->getPlayer(peer_id);
	if (!player) {
		warningstream << FUNCTION_NAME << ": player is null" << std::endl;
		return;
	}

	PlayerSAO *playersao = player->getPlayerSAO();
	if (!playersao) {
		warningstream << FUNCTION_NAME << ": player SAO is null" << std::endl;
		return;
	}

	v3pos_t p;
	std::string formname;
	StringMap fields;

	auto &packet = *(pkt->packet);
	packet[TOSERVER_NODEMETA_FIELDS_POS].convert(p);
	packet[TOSERVER_NODEMETA_FIELDS_FORMNAME].convert(formname);
	packet[TOSERVER_NODEMETA_FIELDS_DATA].convert(fields);

	if (!sanitizeFormspecFields(fields)) {
		warningstream << "Too large formspec fields! Ignoring for pos=" << p
					  << ", player=" << player->getName() << std::endl;
		return;
	}

	static const thread_local auto enable_rollback_recording =
			g_settings->getBool("enable_rollback_recording");
	if (!enable_rollback_recording) {
		m_script->node_on_receive_fields(p, formname, fields, playersao);
	} else {

		// If something goes wrong, this player is to blame
		RollbackScopeActor rollback_scope(m_rollback, "player:" + player->getName());

		// Check the target node for rollback data; leave others unnoticed
		RollbackNode rn_old(&m_env->getMap(), p, this);

		m_script->node_on_receive_fields(p, formname, fields, playersao);

		// Report rollback data
		RollbackNode rn_new(&m_env->getMap(), p, this);
		if (rollback() && rn_new != rn_old) {
			RollbackAction action;
			action.setSetNode(p, rn_old, rn_new);
			rollback()->reportAction(action);
		}
	}
}

void Server::handleCommand_InventoryFields(NetworkPacket *pkt)
{
	session_t peer_id = pkt->getPeerId();
	RemotePlayer *player = m_env->getPlayer(peer_id);

	if (!player)
		return;
	PlayerSAO *playersao = player->getPlayerSAO();
	if (!playersao)
		return;

	std::string client_formspec_name;
	StringMap fields;

	auto &packet = *(pkt->packet);
	packet[TOSERVER_INVENTORY_FIELDS_FORMNAME].convert(client_formspec_name);
	packet[TOSERVER_INVENTORY_FIELDS_DATA].convert(fields);

	if (!sanitizeFormspecFields(fields)) {
		warningstream << "Too large formspec fields! Ignoring for formname=\""
					  << client_formspec_name << "\", player=" << player->getName()
					  << std::endl;
		return;
	}

	if (client_formspec_name.empty()) { // pass through inventory submits
		m_script->on_playerReceiveFields(playersao, client_formspec_name, fields);
		return;
	}

	// verify that we displayed the formspec to the user
	const auto it = m_formspec_state_data.find(peer_id);
	if (it != m_formspec_state_data.end()) {
		const auto &server_formspec_name = it->second;
		if (client_formspec_name == server_formspec_name) {
			// delete state if formspec was closed
			auto it2 = fields.find("quit");
			if (it2 != fields.end() && it2->second == "true")
				m_formspec_state_data.erase(it);

			m_script->on_playerReceiveFields(playersao, client_formspec_name, fields);
			return;
		}
		actionstream << player->getName() << " submitted formspec ('"
					 << client_formspec_name
					 << "') but the name of the formspec doesn't match the"
						" expected name ('"
					 << server_formspec_name << "')";

	} else {
		actionstream << player->getName() << " submitted formspec ('"
					 << client_formspec_name
					 << "') but server hasn't sent formspec to client";
	}
	actionstream << ", possible exploitation attempt" << std::endl;
}

void Server::handleCommand_FirstSrp(NetworkPacket *pkt)
{
	session_t peer_id = pkt->getPeerId();
	RemoteClient *client = getClient(peer_id, CS_Invalid);
	ClientState cstate = client->getState();
	const std::string playername = client->getName();

	std::string salt, verification_key;

	std::string addr_s = client->getAddress().serializeString();
	u8 is_empty;

	auto &packet = *(pkt->packet);
	packet[TOSERVER_FIRST_SRP_SALT].convert(salt);
	packet[TOSERVER_FIRST_SRP_VERIFIER].convert(verification_key);
	packet[TOSERVER_FIRST_SRP_PASSWORD_EMPTY].convert(is_empty);

	verbosestream << "Server: Got TOSERVER_FIRST_SRP from " << addr_s
				  << ", with is_empty=" << (is_empty == 1) << std::endl;

	const bool empty_disallowed = !isSingleplayer() && is_empty == 1 &&
								  g_settings->getBool("disallow_empty_password");

	// Either this packet is sent because the user is new or to change the password
	if (cstate == CS_HelloSent) {
		if (!client->isMechAllowed(AUTH_MECHANISM_FIRST_SRP)) {
			actionstream << "Server: Client from " << addr_s
						 << " tried to set password without being "
						 << "authenticated, or the username being new." << std::endl;
			DenyAccess(peer_id, SERVER_ACCESSDENIED_UNEXPECTED_DATA);
			return;
		}

		if (empty_disallowed) {
			actionstream << "Server: " << playername << " supplied empty password from "
						 << addr_s << std::endl;
			DenyAccess(peer_id, SERVER_ACCESSDENIED_EMPTY_PASSWORD);
			return;
		}

		std::string encpwd = encode_srp_verifier(verification_key, salt);

		// It is possible for multiple connections to get this far with the same
		// player name. In the end only one player with a given name will be emerged
		// (see Server::StateTwoClientInit) but we still have to be careful here.
		if (m_script->getAuth(playername, nullptr, nullptr)) {
			// Another client beat us to it
			actionstream << "Server: Client from " << addr_s << " tried to register "
						 << playername << " a second time." << std::endl;
			DenyAccess(peer_id, SERVER_ACCESSDENIED_ALREADY_CONNECTED);
			return;
		}

		m_script->createAuth(playername, encpwd);
		client->setEncryptedPassword(encpwd);

		m_script->on_authplayer(playername, addr_s, true);
		acceptAuth(peer_id, false);
	} else {
		if (cstate < CS_SudoMode) {
			infostream << "Server: Ignoring TOSERVER_FIRST_SRP from " << addr_s << ": "
					   << "Client has wrong state " << cstate << "." << std::endl;
			return;
		}
		m_clients.event(peer_id, CSE_SudoLeave);

		if (empty_disallowed) {
			actionstream << "Server: " << playername << " supplied empty password"
						 << std::endl;
			SendChatMessage(
					peer_id, ChatMessage(CHATMESSAGE_TYPE_SYSTEM,
									 L"Changing to an empty password is not allowed."));
			return;
		}

		std::string encpwd = encode_srp_verifier(verification_key, salt);
		bool success = m_script->setPassword(playername, encpwd);
		if (success) {
			actionstream << playername << " changes password" << std::endl;
			SendChatMessage(peer_id,
					ChatMessage(CHATMESSAGE_TYPE_SYSTEM, L"Password change successful."));
			client->setEncryptedPassword(encpwd);
		} else {
			actionstream << playername << " tries to change password but it fails"
						 << std::endl;
			SendChatMessage(peer_id, ChatMessage(CHATMESSAGE_TYPE_SYSTEM,
											 L"Password change failed or unavailable."));
		}
	}
}

void Server::handleCommand_SrpBytesA(NetworkPacket *pkt)
{
	session_t peer_id = pkt->getPeerId();
	RemoteClient *client = getClient(peer_id, CS_Invalid);
	ClientState cstate = client->getState();

	std::string addr_s = client->getAddress().serializeString();

	if (!((cstate == CS_HelloSent) || (cstate == CS_Active))) {
		actionstream << "Server: got SRP _A packet in wrong state " << cstate << " from "
					 << addr_s << ". Ignoring." << std::endl;
		return;
	}

	const bool wantSudo = (cstate == CS_Active);

	if (client->chosen_mech != AUTH_MECHANISM_NONE) {
		actionstream << "Server: got SRP _A packet, while auth is already "
						"going on with mech "
					 << client->chosen_mech << " from " << addr_s
					 << " (wantSudo=" << wantSudo << "). Ignoring." << std::endl;
		if (wantSudo) {
			DenySudoAccess(peer_id);
			return;
		}

		DenyAccess(peer_id, SERVER_ACCESSDENIED_UNEXPECTED_DATA);
		return;
	}

	std::string bytes_A;
	u8 based_on;
	auto &packet = *(pkt->packet);
	packet[TOSERVER_SRP_BYTES_A_BYTES].convert(bytes_A);
	packet[TOSERVER_SRP_BYTES_A_BASED_ON].convert(based_on);

	infostream << "Server: TOSERVER_SRP_BYTES_A received with "
			   << "based_on=" << int(based_on) << " and len_A=" << bytes_A.length()
			   << std::endl;

	AuthMechanism chosen =
			(based_on == 0) ? AUTH_MECHANISM_LEGACY_PASSWORD : AUTH_MECHANISM_SRP;

	if (wantSudo) {
		// Right now, the auth mechs don't change between login and sudo mode.
		if (!client->isMechAllowed(chosen)) {
			actionstream << "Server: Player \"" << client->getName() << "\" from "
						 << addr_s << " tried to change password using unallowed mech "
						 << chosen << std::endl;
			DenySudoAccess(peer_id);
			return;
		}
	} else {
		if (!client->isMechAllowed(chosen)) {
			actionstream << "Server: Client tried to authenticate from " << addr_s
						 << " using unallowed mech " << chosen << std::endl;
			DenyAccess(peer_id, SERVER_ACCESSDENIED_UNEXPECTED_DATA);
			return;
		}
	}

	client->chosen_mech = chosen;

	std::string salt, verifier;

	if (based_on == 0) {

		generate_srp_verifier_and_salt(
				client->getName(), client->enc_pwd, &verifier, &salt);
	} else if (!decode_srp_verifier_and_salt(client->enc_pwd, &verifier, &salt)) {
		// Non-base64 errors should have been catched in the init handler
		actionstream
				<< "Server: User " << client->getName()
				<< " tried to log in, but srp verifier field was invalid (most likely "
				   "invalid base64)."
				<< std::endl;
		DenyAccess(peer_id, SERVER_ACCESSDENIED_SERVER_FAIL);
		return;
	}

	char *bytes_B = 0;
	size_t len_B = 0;

	client->auth_data = srp_verifier_new(SRP_SHA256, SRP_NG_2048,
			client->getName().c_str(), (const unsigned char *)salt.c_str(), salt.size(),
			(const unsigned char *)verifier.c_str(), verifier.size(),
			(const unsigned char *)bytes_A.c_str(), bytes_A.size(), NULL, 0,
			(unsigned char **)&bytes_B, &len_B, NULL, NULL);

	if (!bytes_B) {
		actionstream << "Server: User " << client->getName()
					 << " tried to log in, SRP-6a safety check violated in _A handler."
					 << std::endl;
		if (wantSudo) {
			DenySudoAccess(peer_id);
			client->resetChosenMech();
			return;
		}

		DenyAccess(peer_id, SERVER_ACCESSDENIED_UNEXPECTED_DATA);
		return;
	}

	MSGPACK_PACKET_INIT((int)TOCLIENT_SRP_BYTES_S_B, 2);
	PACK(TOCLIENT_SRP_BYTES_S_B_SALT, salt);
	PACK(TOCLIENT_SRP_BYTES_S_B_BYTES, std::string(bytes_B, len_B));
	m_clients.send(peer_id, 1, buffer, true);
}

void Server::handleCommand_SrpBytesM(NetworkPacket *pkt)
{
	session_t peer_id = pkt->getPeerId();
	RemoteClient *client = getClient(peer_id, CS_Invalid);
	ClientState cstate = client->getState();
	const std::string addr_s = client->getAddress().serializeString();
	const std::string playername = client->getName();

	const bool wantSudo = (cstate == CS_Active);

	verbosestream << "Server: Received TOSERVER_SRP_BYTES_M." << std::endl;

	if (!((cstate == CS_HelloSent) || (cstate == CS_Active))) {
		warningstream << "Server: got SRP_M packet in wrong state " << cstate << " from "
					  << addr_s << ". Ignoring." << std::endl;
		return;
	}

	if (client->chosen_mech != AUTH_MECHANISM_SRP &&
			client->chosen_mech != AUTH_MECHANISM_LEGACY_PASSWORD) {
		warningstream << "Server: got SRP_M packet, while auth "
						 "is going on with mech "
					  << client->chosen_mech << " from " << addr_s
					  << " (wantSudo=" << wantSudo << "). Denying." << std::endl;
		if (wantSudo) {
			DenySudoAccess(peer_id);
			return;
		}

		DenyAccess(peer_id, SERVER_ACCESSDENIED_UNEXPECTED_DATA);
		return;
	}

	std::string bytes_M;
	pkt->packet->operator[](TOSERVER_SRP_BYTES_M_BYTES).convert(bytes_M);

	if (!client->auth_data) {
		DenyAccess(peer_id, SERVER_ACCESSDENIED_UNEXPECTED_DATA);
		return;
	}

	if (srp_verifier_get_session_key_length((SRPVerifier *)client->auth_data) !=
			bytes_M.size()) {
		actionstream << "Server: User " << playername << " at " << addr_s
					 << " sent bytes_M with invalid length " << bytes_M.size()
					 << std::endl;
		DenyAccess(peer_id, SERVER_ACCESSDENIED_UNEXPECTED_DATA);
		return;
	}

	unsigned char *bytes_HAMK = 0;

	srp_verifier_verify_session((SRPVerifier *)client->auth_data,
			(unsigned char *)bytes_M.c_str(), &bytes_HAMK);

	// skip authentication check for singleplayer world.
	const bool is_true_singleplayer =
			isSingleplayer() && (strcasecmp(playername.c_str(), "singleplayer") == 0);
	if (!bytes_HAMK && !is_true_singleplayer) {
		if (wantSudo) {
			actionstream << "Server: User " << playername << " at " << addr_s
						 << " tried to change their password, but supplied wrong"
						 << " (SRP) password for authentication." << std::endl;
			DenySudoAccess(peer_id);
			client->resetChosenMech();
			return;
		}

		actionstream << "Server: User " << playername << " at " << addr_s
					 << " supplied wrong password (auth mechanism: SRP)." << std::endl;
		m_script->on_authplayer(playername, addr_s, false);
		DenyAccess(peer_id, SERVER_ACCESSDENIED_WRONG_PASSWORD);
		return;
	}

	if (client->create_player_on_auth_success) {
		m_script->createAuth(playername, client->enc_pwd);

		if (!m_script->getAuth(playername, nullptr, nullptr)) {
			errorstream << "Server: " << playername
						<< " cannot be authenticated (auth handler does not work?)"
						<< std::endl;
			DenyAccess(peer_id, SERVER_ACCESSDENIED_SERVER_FAIL);
			return;
		}
		client->create_player_on_auth_success = false;
	}

	m_script->on_authplayer(playername, addr_s, true);
	acceptAuth(peer_id, wantSudo);
}

void Server::DenySudoAccess(session_t peer_id)

{
	MSGPACK_PACKET_INIT((int)TOCLIENT_DENY_SUDO_MODE, 0);
	m_clients.send(peer_id, 1, buffer, true);
}

void Server::handleCommand_ModChannelJoin(NetworkPacket *pkt)
{
	std::string channel;
	pkt->packet->operator[](TOSERVER_MODCHANNEL_JOIN_NAME).convert(channel);
	const bool success = g_settings->getBool("enable_mod_channels") &&
						 m_modchannel_mgr->joinChannel(channel, pkt->getPeerId());
	MSGPACK_PACKET_INIT((int)TOCLIENT_MODCHANNEL_SIGNAL, 2);
	PACK(TOCLIENT_MODCHANNEL_SIGNAL_SIGNAL,
			(u8)(success ? MODCHANNEL_SIGNAL_JOIN_OK : MODCHANNEL_SIGNAL_JOIN_FAILURE));
	PACK(TOCLIENT_MODCHANNEL_SIGNAL_CHANNEL, channel);
	m_clients.send(pkt->getPeerId(), 0, buffer, true);
}

void Server::handleCommand_ModChannelLeave(NetworkPacket *pkt)
{
	std::string channel;
	pkt->packet->operator[](TOSERVER_MODCHANNEL_LEAVE_NAME).convert(channel);
	const bool success = g_settings->getBool("enable_mod_channels") &&
						 m_modchannel_mgr->leaveChannel(channel, pkt->getPeerId());
	MSGPACK_PACKET_INIT((int)TOCLIENT_MODCHANNEL_SIGNAL, 2);
	PACK(TOCLIENT_MODCHANNEL_SIGNAL_SIGNAL,
			(u8)(success ? MODCHANNEL_SIGNAL_LEAVE_OK : MODCHANNEL_SIGNAL_LEAVE_FAILURE));
	PACK(TOCLIENT_MODCHANNEL_SIGNAL_CHANNEL, channel);
	m_clients.send(pkt->getPeerId(), 0, buffer, true);
}

void Server::handleCommand_ModChannelMsg(NetworkPacket *pkt)
{
	std::string channel, message;
	auto &packet = *(pkt->packet);
	packet[TOSERVER_MODCHANNEL_MSG_CHANNEL].convert(channel);
	packet[TOSERVER_MODCHANNEL_MSG_MESSAGE].convert(message);
	if (!g_settings->getBool("enable_mod_channels"))
		return;
	if (!m_modchannel_mgr->channelRegistered(channel)) {
		MSGPACK_PACKET_INIT((int)TOCLIENT_MODCHANNEL_SIGNAL, 2);
		PACK(TOCLIENT_MODCHANNEL_SIGNAL_SIGNAL,
				(u8)MODCHANNEL_SIGNAL_CHANNEL_NOT_REGISTERED);
		PACK(TOCLIENT_MODCHANNEL_SIGNAL_CHANNEL, channel);
		m_clients.send(pkt->getPeerId(), 0, buffer, true);
		return;
	}
	broadcastModChannelMessage(channel, message, pkt->getPeerId());
}

void Server::broadcastModChannelMessage(
		const std::string &channel, const std::string &message, session_t from_peer)
{
	const auto &peers = m_modchannel_mgr->getChannelPeers(channel);
	if (peers.empty() || message.size() > STRING_MAX_LEN)
		return;
	std::string sender;
	if (from_peer != PEER_ID_SERVER)
		sender = getPlayerName(from_peer);
	MSGPACK_PACKET_INIT((int)TOCLIENT_MODCHANNEL_MSG, 3);
	PACK(TOCLIENT_MODCHANNEL_MSG_CHANNEL, channel);
	PACK(TOCLIENT_MODCHANNEL_MSG_SENDER, sender);
	PACK(TOCLIENT_MODCHANNEL_MSG_MESSAGE, message);
	for (const auto peer_id : peers) {
		if (peer_id != from_peer)
			m_clients.send(peer_id, 0, buffer, true);
	}
	if (from_peer != PEER_ID_SERVER)
		m_script->on_modchannel_message(channel, sender, message);
}

void Server::handleCommand_HaveMedia(NetworkPacket *pkt)

{
	std::vector<u32> tokens;
	pkt->packet->operator[](TOSERVER_HAVE_MEDIA_TOKENS).convert(tokens);
	const auto peer_id = pkt->getPeerId();
	auto player = m_env->getPlayer(peer_id);
	for (const auto token : tokens) {
		auto it = m_pending_dyn_media.find(token);
		if (it == m_pending_dyn_media.end() || !it->second.waiting_players.erase(peer_id))
			continue;
		if (player)
			m_script->on_dynamic_media_added(token, player->getName().c_str());
	}
}

void Server::handleCommand_UpdateClientInfo(NetworkPacket *pkt)
{
	ClientDynamicInfo info{};
	auto &packet = *(pkt->packet);
	std::array<u32, 2> render_target_size{};
	std::array<f32, 2> max_fs_size{};
	packet[TOSERVER_UPDATE_CLIENT_INFO_RENDER_TARGET_SIZE].convert(render_target_size);
	packet[TOSERVER_UPDATE_CLIENT_INFO_REAL_GUI_SCALING].convert(info.real_gui_scaling);
	packet[TOSERVER_UPDATE_CLIENT_INFO_REAL_HUD_SCALING].convert(info.real_hud_scaling);
	packet[TOSERVER_UPDATE_CLIENT_INFO_MAX_FS_SIZE].convert(max_fs_size);
	info.render_target_size.X = render_target_size[0];
	info.render_target_size.Y = render_target_size[1];
	info.max_fs_size.X = max_fs_size[0];
	info.max_fs_size.Y = max_fs_size[1];
	info.touch_controls = false;
	packet_convert_safe(
			packet, TOSERVER_UPDATE_CLIENT_INFO_TOUCH_CONTROLS, info.touch_controls);
	getClient(pkt->getPeerId(), CS_Invalid)->setDynamicInfo(info);
}

void Server::acceptAuth(session_t peer_id, bool forSudoMode)
{
	auto client = getClient(peer_id, CS_Invalid);
	if (forSudoMode) {
		MSGPACK_PACKET_INIT((int)TOCLIENT_ACCEPT_SUDO_MODE, 1);
		PACK(TOCLIENT_ACCEPT_SUDO_MODE_AUTH_METHODS, (u32)AUTH_MECHANISM_FIRST_SRP);
		m_clients.send(peer_id, 1, buffer, true);
		m_clients.event(peer_id, CSE_SudoSuccess);
		return;
	}
	MSGPACK_PACKET_INIT((int)TOCLIENT_AUTH_ACCEPT, 4);
	PACK(TOCLIENT_AUTH_ACCEPT_POSITION, v3f());
	PACK(TOCLIENT_AUTH_ACCEPT_SEED, (u64)m_env->getServerMap().getSeed());
	PACK(TOCLIENT_AUTH_ACCEPT_RECOMMENDED_SEND_INTERVAL,
			g_settings->getFloat("dedicated_server_step"));
	PACK(TOCLIENT_AUTH_ACCEPT_SUDO_AUTH_METHODS, client->allowed_auth_mechs);
	m_clients.send(peer_id, 1, buffer, true);
	m_clients.event(peer_id, CSE_AuthAccept);
}

#endif
