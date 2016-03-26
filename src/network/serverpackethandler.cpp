/*
Minetest
Copyright (C) 2015 nerzhul, Loic Blot <loic.blot@unix-experience.fr>

This program is free software; you can redistribute it and/or modify
it under the terms of the GNU Lesser General Public License as published by
the Free Software Foundation; either version 2.1 of the License, or
(at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU Lesser General Public License for more details.

You should have received a copy of the GNU Lesser General Public License along
with this program; if not, write to the Free Software Foundation, Inc.,
51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
*/

#include "config.h"

#if !MINETEST_PROTO
#include "network/fm_serverpackethandler.cpp"

#else //TODO

#include "server.h"
#include "log.h"

#include "content_abm.h"
#include "content_sao.h"
#include "emerge.h"
#include "nodedef.h"
#include "player.h"
#include "rollback_interface.h"
#include "scripting_game.h"
#include "settings.h"
#include "tool.h"
#include "version.h"
#include "network/networkprotocol.h"
#include "network/serveropcodes.h"
#include "util/auth.h"
#include "util/base64.h"
#include "util/pointedthing.h"
#include "util/serialize.h"
#include "util/srp.h"

void Server::handleCommand_Deprecated(NetworkPacket* pkt)
{
	infostream << "Server: " << toServerCommandTable[pkt->getCommand()].name
		<< " not supported anymore" << std::endl;
}

void Server::handleCommand_Init(NetworkPacket* pkt)
{

	if(pkt->getSize() < 1)
		return;

	RemoteClient* client = getClient(pkt->getPeerId(), CS_Created);

	std::string addr_s;
	try {
		Address address = getPeerAddress(pkt->getPeerId());
		addr_s = address.serializeString();
	}
	catch (con::PeerNotFoundException &e) {
		/*
		 * no peer for this packet found
		 * most common reason is peer timeout, e.g. peer didn't
		 * respond for some time, your server was overloaded or
		 * things like that.
		 */
		infostream << "Server::ProcessData(): Canceling: peer "
				<< pkt->getPeerId() << " not found" << std::endl;
		return;
	}

	// If net_proto_version is set, this client has already been handled
	if (client->getState() > CS_Created) {
		verbosestream << "Server: Ignoring multiple TOSERVER_INITs from "
				<< addr_s << " (peer_id=" << pkt->getPeerId() << ")" << std::endl;
		return;
	}

	verbosestream << "Server: Got TOSERVER_INIT from " << addr_s << " (peer_id="
			<< pkt->getPeerId() << ")" << std::endl;

	// Do not allow multiple players in simple singleplayer mode.
	// This isn't a perfect way to do it, but will suffice for now
	if (m_simple_singleplayer_mode && m_clients.getClientIDs().size() > 1) {
		infostream << "Server: Not allowing another client (" << addr_s
				<< ") to connect in simple singleplayer mode" << std::endl;
		DenyAccess(pkt->getPeerId(), SERVER_ACCESSDENIED_SINGLEPLAYER);
		return;
	}

	// First byte after command is maximum supported
	// serialization version
	u8 client_max;
	u16 supp_compr_modes;
	u16 min_net_proto_version = 0;
	u16 max_net_proto_version;
	std::string playerName;

	*pkt >> client_max >> supp_compr_modes >> min_net_proto_version
			>> max_net_proto_version >> playerName;

	u8 our_max = SER_FMT_VER_HIGHEST_READ;
	// Use the highest version supported by both
	u8 depl_serial_v = std::min(client_max, our_max);
	// If it's lower than the lowest supported, give up.
	if (depl_serial_v < SER_FMT_VER_LOWEST_READ)
		depl_serial_v = SER_FMT_VER_INVALID;

	if (depl_serial_v == SER_FMT_VER_INVALID) {
		actionstream << "Server: A mismatched client tried to connect from "
				<< addr_s << std::endl;
		infostream<<"Server: Cannot negotiate serialization version with "
				<< addr_s << std::endl;
		DenyAccess(pkt->getPeerId(), SERVER_ACCESSDENIED_WRONG_VERSION);
		return;
	}

	client->setPendingSerializationVersion(depl_serial_v);

	/*
		Read and check network protocol version
	*/

	u16 net_proto_version = 0;

	// Figure out a working version if it is possible at all
	if (max_net_proto_version >= SERVER_PROTOCOL_VERSION_MIN ||
			min_net_proto_version <= SERVER_PROTOCOL_VERSION_MAX) {
		// If maximum is larger than our maximum, go with our maximum
		if (max_net_proto_version > SERVER_PROTOCOL_VERSION_MAX)
			net_proto_version = SERVER_PROTOCOL_VERSION_MAX;
		// Else go with client's maximum
		else
			net_proto_version = max_net_proto_version;
	}

	verbosestream << "Server: " << addr_s << ": Protocol version: min: "
			<< min_net_proto_version << ", max: " << max_net_proto_version
			<< ", chosen: " << net_proto_version << std::endl;

	client->net_proto_version = net_proto_version;

	// On this handler at least protocol version 25 is required
	if (net_proto_version < 25 ||
			net_proto_version < SERVER_PROTOCOL_VERSION_MIN ||
			net_proto_version > SERVER_PROTOCOL_VERSION_MAX) {
		actionstream << "Server: A mismatched client tried to connect from "
				<< addr_s << std::endl;
		DenyAccess(pkt->getPeerId(), SERVER_ACCESSDENIED_WRONG_VERSION);
		return;
	}

	if (g_settings->getBool("strict_protocol_version_checking")) {
		if (net_proto_version != LATEST_PROTOCOL_VERSION) {
			actionstream << "Server: A mismatched (strict) client tried to "
					<< "connect from " << addr_s << std::endl;
			DenyAccess(pkt->getPeerId(), SERVER_ACCESSDENIED_WRONG_VERSION);
			return;
		}
	}

	/*
		Validate player name
	*/
	const char* playername = playerName.c_str();

	size_t pns = playerName.size();
	if (pns == 0 || pns > PLAYERNAME_SIZE) {
		actionstream << "Server: Player with "
			<< ((pns > PLAYERNAME_SIZE) ? "a too long" : "an empty")
			<< " name tried to connect from " << addr_s << std::endl;
		DenyAccess(pkt->getPeerId(), SERVER_ACCESSDENIED_WRONG_NAME);
		return;
	}

	if (!g_settings->getBool("enable_any_name") && string_allowed(playerName, PLAYERNAME_ALLOWED_CHARS) == false) {
		actionstream << "Server: Player with an invalid name "
				<< "tried to connect from " << addr_s << std::endl;
		DenyAccess(pkt->getPeerId(), SERVER_ACCESSDENIED_WRONG_CHARS_IN_NAME);
		return;
	}

	m_clients.setPlayerName(pkt->getPeerId(), playername);
	//TODO (later) case insensitivity

	std::string legacyPlayerNameCasing = playerName;

	if (!isSingleplayer() && strcasecmp(playername, "singleplayer") == 0) {
		actionstream << "Server: Player with the name \"singleplayer\" "
				<< "tried to connect from " << addr_s << std::endl;
		DenyAccess(pkt->getPeerId(), SERVER_ACCESSDENIED_WRONG_NAME);
		return;
	}

	{
		std::string reason;
		if (m_script->on_prejoinplayer(playername, addr_s, &reason)) {
			actionstream << "Server: Player with the name \"" << playerName << "\" "
					<< "tried to connect from " << addr_s << " "
					<< "but it was disallowed for the following reason: "
					<< reason << std::endl;
			DenyAccess(pkt->getPeerId(), SERVER_ACCESSDENIED_CUSTOM_STRING,
					reason.c_str());
			return;
		}
	}

	infostream << "Server: New connection: \"" << playerName << "\" from "
			<< addr_s << " (peer_id=" << pkt->getPeerId() << ")" << std::endl;

	// Enforce user limit.
	// Don't enforce for users that have some admin right
	if (m_clients.getClientIDs(CS_Created).size() >= g_settings->getU16("max_users") &&
			!checkPriv(playername, "server") &&
			!checkPriv(playername, "ban") &&
			!checkPriv(playername, "privs") &&
			!checkPriv(playername, "password") &&
			playername != g_settings->get("name")) {
		actionstream << "Server: " << playername << " tried to join from "
				<< addr_s << ", but there" << " are already max_users="
				<< g_settings->getU16("max_users") << " players." << std::endl;
		DenyAccess(pkt->getPeerId(), SERVER_ACCESSDENIED_TOO_MANY_USERS);
		return;
	}

	/*
		Compose auth methods for answer
	*/
	std::string encpwd; // encrypted Password field for the user
	bool has_auth = m_script->getAuth(playername, &encpwd, NULL);
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
					<< " tried to log in, but password field"
					<< " was invalid (unknown mechcode)." << std::endl;
				DenyAccess(pkt->getPeerId(), SERVER_ACCESSDENIED_SERVER_FAIL);
				return;
			}
		} else if (base64_is_valid(encpwd)) {
			auth_mechs |= AUTH_MECHANISM_LEGACY_PASSWORD;
			client->enc_pwd = encpwd;
		} else {
			actionstream << "User " << playername
				<< " tried to log in, but password field"
				<< " was invalid (invalid base64)." << std::endl;
			DenyAccess(pkt->getPeerId(), SERVER_ACCESSDENIED_SERVER_FAIL);
			return;
		}
	} else {
		std::string default_password = g_settings->get("default_password");
		if (default_password.length() == 0) {
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

	verbosestream << "Sending TOCLIENT_HELLO with auth method field: "
		<< auth_mechs << std::endl;

	NetworkPacket resp_pkt(TOCLIENT_HELLO, 1 + 4
		+ legacyPlayerNameCasing.size(), pkt->getPeerId());

	u16 depl_compress_mode = NETPROTO_COMPRESSION_NONE;
	resp_pkt << depl_serial_v << depl_compress_mode << net_proto_version
		<< auth_mechs << legacyPlayerNameCasing;

	Send(&resp_pkt);

	client->allowed_auth_mechs = auth_mechs;
	client->setDeployedCompressionMode(depl_compress_mode);

	m_clients.event(pkt->getPeerId(), CSE_Hello);
}

void Server::handleCommand_Init_Legacy(NetworkPacket* pkt)
{
	// [0] u8 SER_FMT_VER_HIGHEST_READ
	// [1] u8[20] player_name
	// [21] u8[28] password <--- can be sent without this, from old versions

	if (pkt->getSize() < 1+PLAYERNAME_SIZE)
		return;

	RemoteClient* client = getClient(pkt->getPeerId(), CS_Created);

	std::string addr_s;
	try {
		Address address = getPeerAddress(pkt->getPeerId());
		addr_s = address.serializeString();
	}
	catch (con::PeerNotFoundException &e) {
		/*
		 * no peer for this packet found
		 * most common reason is peer timeout, e.g. peer didn't
		 * respond for some time, your server was overloaded or
		 * things like that.
		 */
		infostream << "Server::ProcessData(): Canceling: peer "
				<< pkt->getPeerId() << " not found" << std::endl;
		return;
	}

	// If net_proto_version is set, this client has already been handled
	if (client->getState() > CS_Created) {
		verbosestream << "Server: Ignoring multiple TOSERVER_INITs from "
				<< addr_s << " (peer_id=" << pkt->getPeerId() << ")" << std::endl;
		return;
	}

	verbosestream << "Server: Got TOSERVER_INIT_LEGACY from " << addr_s << " (peer_id="
			<< pkt->getPeerId() << ")" << std::endl;

	// Do not allow multiple players in simple singleplayer mode.
	// This isn't a perfect way to do it, but will suffice for now
	if (m_simple_singleplayer_mode && m_clients.getClientIDs().size() > 1) {
		infostream << "Server: Not allowing another client (" << addr_s
				<< ") to connect in simple singleplayer mode" << std::endl;
		DenyAccess_Legacy(pkt->getPeerId(), L"Running in simple singleplayer mode.");
		return;
	}

	// First byte after command is maximum supported
	// serialization version
	u8 client_max;

	*pkt >> client_max;

	u8 our_max = SER_FMT_VER_HIGHEST_READ;
	// Use the highest version supported by both
	int deployed = std::min(client_max, our_max);
	// If it's lower than the lowest supported, give up.
	if (deployed < SER_FMT_VER_LOWEST_READ)
		deployed = SER_FMT_VER_INVALID;

	if (deployed == SER_FMT_VER_INVALID) {
		actionstream << "Server: A mismatched client tried to connect from "
				<< addr_s << std::endl;
		infostream<<"Server: Cannot negotiate serialization version with "
				<< addr_s << std::endl;
		DenyAccess_Legacy(pkt->getPeerId(), std::wstring(
				L"Your client's version is not supported.\n"
				L"Server version is ")
				+ utf8_to_wide(g_version_string) + L"."
		);
		return;
	}

	client->setPendingSerializationVersion(deployed);

	/*
		Read and check network protocol version
	*/

	u16 min_net_proto_version = 0;
	if (pkt->getSize() >= 1 + PLAYERNAME_SIZE + PASSWORD_SIZE + 2)
		min_net_proto_version = pkt->getU16(1 + PLAYERNAME_SIZE + PASSWORD_SIZE);

	// Use same version as minimum and maximum if maximum version field
	// doesn't exist (backwards compatibility)
	u16 max_net_proto_version = min_net_proto_version;
	if (pkt->getSize() >= 1 + PLAYERNAME_SIZE + PASSWORD_SIZE + 2 + 2)
		max_net_proto_version = pkt->getU16(1 + PLAYERNAME_SIZE + PASSWORD_SIZE + 2);

	// Start with client's maximum version
	u16 net_proto_version = max_net_proto_version;

	// Figure out a working version if it is possible at all
	if (max_net_proto_version >= SERVER_PROTOCOL_VERSION_MIN ||
			min_net_proto_version <= SERVER_PROTOCOL_VERSION_MAX) {
		// If maximum is larger than our maximum, go with our maximum
		if (max_net_proto_version > SERVER_PROTOCOL_VERSION_MAX)
			net_proto_version = SERVER_PROTOCOL_VERSION_MAX;
		// Else go with client's maximum
		else
			net_proto_version = max_net_proto_version;
	}

	// The client will send up to date init packet, ignore this one
	if (net_proto_version >= 25)
		return;

	verbosestream << "Server: " << addr_s << ": Protocol version: min: "
			<< min_net_proto_version << ", max: " << max_net_proto_version
			<< ", chosen: " << net_proto_version << std::endl;

	client->net_proto_version = net_proto_version;

	if (net_proto_version < SERVER_PROTOCOL_VERSION_MIN ||
			net_proto_version > SERVER_PROTOCOL_VERSION_MAX) {
		actionstream << "Server: A mismatched client tried to connect from "
				<< addr_s << std::endl;
		DenyAccess_Legacy(pkt->getPeerId(), std::wstring(
				L"Your client's version is not supported.\n"
				L"Server version is ")
				+ utf8_to_wide(g_version_string) + L",\n"
				+ L"server's PROTOCOL_VERSION is "
				+ utf8_to_wide(itos(SERVER_PROTOCOL_VERSION_MIN))
				+ L"..."
				+ utf8_to_wide(itos(SERVER_PROTOCOL_VERSION_MAX))
				+ L", client's PROTOCOL_VERSION is "
				+ utf8_to_wide(itos(min_net_proto_version))
				+ L"..."
				+ utf8_to_wide(itos(max_net_proto_version))
		);
		return;
	}

	if (g_settings->getBool("strict_protocol_version_checking")) {
		if (net_proto_version != LATEST_PROTOCOL_VERSION) {
			actionstream << "Server: A mismatched (strict) client tried to "
					<< "connect from " << addr_s << std::endl;
			DenyAccess_Legacy(pkt->getPeerId(), std::wstring(
					L"Your client's version is not supported.\n"
					L"Server version is ")
					+ utf8_to_wide(g_version_string) + L",\n"
					+ L"server's PROTOCOL_VERSION (strict) is "
					+ utf8_to_wide(itos(LATEST_PROTOCOL_VERSION))
					+ L", client's PROTOCOL_VERSION is "
					+ utf8_to_wide(itos(min_net_proto_version))
					+ L"..."
					+ utf8_to_wide(itos(max_net_proto_version))
			);
			return;
		}
	}

	/*
		Set up player
	*/
	char playername[PLAYERNAME_SIZE];
	unsigned int playername_length = 0;
	for (; playername_length < PLAYERNAME_SIZE; playername_length++ ) {
		playername[playername_length] = pkt->getChar(1+playername_length);
		if (pkt->getChar(1+playername_length) == 0)
			break;
	}

	if (playername_length == PLAYERNAME_SIZE) {
		actionstream << "Server: Player with name exceeding max length "
				<< "tried to connect from " << addr_s << std::endl;
		DenyAccess_Legacy(pkt->getPeerId(), L"Name too long");
		return;
	}


	if (playername[0]=='\0') {
		actionstream << "Server: Player with an empty name "
				<< "tried to connect from " << addr_s << std::endl;
		DenyAccess_Legacy(pkt->getPeerId(), L"Empty name");
		return;
	}

	if (!g_settings->getBool("enable_any_name") && string_allowed(playername, PLAYERNAME_ALLOWED_CHARS) == false) {
		actionstream << "Server: Player with an invalid name "
				<< "tried to connect from " << addr_s << std::endl;
		DenyAccess_Legacy(pkt->getPeerId(), L"Name contains unallowed characters");
		return;
	}

	if (!isSingleplayer() && strcasecmp(playername, "singleplayer") == 0) {
		actionstream << "Server: Player with the name \"singleplayer\" "
				<< "tried to connect from " << addr_s << std::endl;
		DenyAccess_Legacy(pkt->getPeerId(), L"Name is not allowed");
		return;
	}

	{
		std::string reason;
		if (m_script->on_prejoinplayer(playername, addr_s, &reason)) {
			actionstream << "Server: Player with the name \"" << playername << "\" "
					<< "tried to connect from " << addr_s << " "
					<< "but it was disallowed for the following reason: "
					<< reason << std::endl;
			DenyAccess_Legacy(pkt->getPeerId(), utf8_to_wide(reason.c_str()));
			return;
		}
	}

	infostream<<"Server: New connection: \""<<playername<<"\" from "
			<<addr_s<<" (peer_id="<<pkt->getPeerId()<<")"<<std::endl;

	// Get password
	char given_password[PASSWORD_SIZE];
	if (pkt->getSize() < 1 + PLAYERNAME_SIZE + PASSWORD_SIZE) {
		// old version - assume blank password
		given_password[0] = 0;
	}
	else {
		for (u16 i = 0; i < PASSWORD_SIZE - 1; i++) {
			given_password[i] = pkt->getChar(21 + i);
		}
		given_password[PASSWORD_SIZE - 1] = 0;
	}

	if (!base64_is_valid(given_password)) {
		actionstream << "Server: " << playername
				<< " supplied invalid password hash" << std::endl;
		DenyAccess_Legacy(pkt->getPeerId(), L"Invalid password hash");
		return;
	}

	// Enforce user limit.
	// Don't enforce for users that have some admin right
	if (m_clients.getClientIDs(CS_Created).size() >= g_settings->getU16("max_users") &&
			!checkPriv(playername, "server") &&
			!checkPriv(playername, "ban") &&
			!checkPriv(playername, "privs") &&
			!checkPriv(playername, "password") &&
			playername != g_settings->get("name")) {
		actionstream << "Server: " << playername << " tried to join, but there"
				<< " are already max_users="
				<< g_settings->getU16("max_users") << " players." << std::endl;
		DenyAccess_Legacy(pkt->getPeerId(), L"Too many users.");
		return;
	}

	std::string checkpwd; // Password hash to check against
	bool has_auth = m_script->getAuth(playername, &checkpwd, NULL);

	// If no authentication info exists for user, create it
	if (!has_auth) {
		if (!isSingleplayer() &&
				g_settings->getBool("disallow_empty_password") &&
				std::string(given_password) == "") {
			actionstream << "Server: " << playername
					<< " supplied empty password" << std::endl;
			DenyAccess_Legacy(pkt->getPeerId(), L"Empty passwords are "
					L"disallowed. Set a password and try again.");
			return;
		}
		std::string raw_default_password =
			g_settings->get("default_password");
		std::string initial_password =
			translate_password(playername, raw_default_password);

		// If default_password is empty, allow any initial password
		if (raw_default_password.length() == 0)
			initial_password = given_password;

		m_script->createAuth(playername, initial_password);
	}

	has_auth = m_script->getAuth(playername, &checkpwd, NULL);

	if (!has_auth) {
		actionstream << "Server: " << playername << " cannot be authenticated"
				<< " (auth handler does not work?)" << std::endl;
		DenyAccess_Legacy(pkt->getPeerId(), L"Not allowed to login");
		return;
	}

	if (given_password != checkpwd) {
		actionstream << "Server: User " << playername
			<< " at " << addr_s
			<< " supplied wrong password (auth mechanism: legacy)."
			<< std::endl;
		DenyAccess_Legacy(pkt->getPeerId(), L"Wrong password");
		return;
	}

	RemotePlayer *player =
			static_cast<RemotePlayer*>(m_env->getPlayer(playername));

	if (player && player->peer_id != 0) {
		actionstream << "Server: " << playername << ": Failed to emerge player"
				<< " (player allocated to an another client)" << std::endl;
		DenyAccess_Legacy(pkt->getPeerId(), L"Another client is connected with this "
				L"name. If your client closed unexpectedly, try again in "
				L"a minute.");
	}

	m_clients.setPlayerName(pkt->getPeerId(), playername);

	/*
		Answer with a TOCLIENT_INIT
	*/

	NetworkPacket resp_pkt(TOCLIENT_INIT_LEGACY, 1 + 6 + 8 + 4,
			pkt->getPeerId());

	resp_pkt << (u8) deployed << (v3s16) floatToInt(v3f(0,0,0), BS)
			<< (u64) m_env->getServerMap().getSeed()
			<< g_settings->getFloat("dedicated_server_step");

	Send(&resp_pkt);
	m_clients.event(pkt->getPeerId(), CSE_InitLegacy);
}

void Server::handleCommand_Init2(NetworkPacket* pkt)
{
	verbosestream << "Server: Got TOSERVER_INIT2 from "
			<< pkt->getPeerId() << std::endl;

	m_clients.event(pkt->getPeerId(), CSE_GotInit2);
	u16 protocol_version = m_clients.getProtocolVersion(pkt->getPeerId());


	///// begin compatibility code
	PlayerSAO* playersao = NULL;
	if (protocol_version <= 22) {
		playersao = StageTwoClientInit(pkt->getPeerId());

		if (playersao == NULL) {
			actionstream
				<< "TOSERVER_INIT2 stage 2 client init failed for peer "
				<< pkt->getPeerId() << std::endl;
			return;
		}
	}
	///// end compatibility code

	/*
		Send some initialization data
	*/

	infostream << "Server: Sending content to "
			<< getPlayerName(pkt->getPeerId()) << std::endl;

	// Send player movement settings
	SendMovement(pkt->getPeerId());

	// Send item definitions
	SendItemDef(pkt->getPeerId(), m_itemdef, protocol_version);

	// Send node definitions
	SendNodeDef(pkt->getPeerId(), m_nodedef, protocol_version);

	m_clients.event(pkt->getPeerId(), CSE_SetDefinitionsSent);

	// Send media announcement
	sendMediaAnnouncement(pkt->getPeerId());

	// Send detached inventories
	sendDetachedInventories(pkt->getPeerId());

	// Send time of day
	u16 time = m_env->getTimeOfDay();
	float time_speed = g_settings->getFloat("time_speed");
	SendTimeOfDay(pkt->getPeerId(), time, time_speed);

	///// begin compatibility code
	if (protocol_version <= 22) {
		m_clients.event(pkt->getPeerId(), CSE_SetClientReady);
		m_script->on_joinplayer(playersao);
	}
	///// end compatibility code

	// Warnings about protocol version can be issued here
	if (getClient(pkt->getPeerId())->net_proto_version < LATEST_PROTOCOL_VERSION) {
		SendChatMessage(pkt->getPeerId(), L"# Server: WARNING: YOUR CLIENT'S "
				L"VERSION MAY NOT BE FULLY COMPATIBLE WITH THIS SERVER!");
	}
}

void Server::handleCommand_RequestMedia(NetworkPacket* pkt)
{
	std::vector<std::string> tosend;
	u16 numfiles;

	*pkt >> numfiles;

	infostream << "Sending " << numfiles << " files to "
			<< getPlayerName(pkt->getPeerId()) << std::endl;
	verbosestream << "TOSERVER_REQUEST_MEDIA: " << std::endl;

	for (u16 i = 0; i < numfiles; i++) {
		std::string name;

		*pkt >> name;

		tosend.push_back(name);
		verbosestream << "TOSERVER_REQUEST_MEDIA: requested file "
				<< name << std::endl;
	}

	sendRequestedMedia(pkt->getPeerId(), tosend);
}

void Server::handleCommand_ReceivedMedia(NetworkPacket* pkt)
{
}

void Server::handleCommand_ClientReady(NetworkPacket* pkt)
{
	u16 peer_id = pkt->getPeerId();
	u16 peer_proto_ver = getClient(peer_id, CS_InitDone)->net_proto_version;

	// clients <= protocol version 22 did not send ready message,
	// they're already initialized
	if (peer_proto_ver <= 22) {
		infostream << "Client sent message not expected by a "
			<< "client using protocol version <= 22,"
			<< "disconnecting peer_id: " << peer_id << std::endl;
		m_con.DisconnectPeer(peer_id);
		return;
	}

	PlayerSAO* playersao = StageTwoClientInit(peer_id);

	if (playersao == NULL) {
		actionstream
			<< "TOSERVER_CLIENT_READY stage 2 client init failed for peer_id: "
			<< peer_id << std::endl;
		m_con.DisconnectPeer(peer_id);
		return;
	}


	if (pkt->getSize() < 8) {
		errorstream
			<< "TOSERVER_CLIENT_READY client sent inconsistent data, disconnecting peer_id: "
			<< peer_id << std::endl;
		m_con.DisconnectPeer(peer_id);
		return;
	}

	u8 major_ver, minor_ver, patch_ver, reserved;
	std::string full_ver;
	*pkt >> major_ver >> minor_ver >> patch_ver >> reserved >> full_ver;

	m_clients.setClientVersion(
			peer_id, major_ver, minor_ver, patch_ver,
			full_ver);

	m_clients.event(peer_id, CSE_SetClientReady);
	m_script->on_joinplayer(playersao);

	stat.add("join", playersao->getPlayer()->getName());
}

void Server::handleCommand_GotBlocks(NetworkPacket* pkt)
{
#if NOTUSED
	if (pkt->getSize() < 1)
		return;

	/*
		[0] u16 command
		[2] u8 count
		[3] v3s16 pos_0
		[3+6] v3s16 pos_1
		...
	*/

	u8 count;
	*pkt >> count;

	RemoteClient *client = getClient(pkt->getPeerId());

	if ((s16)pkt->getSize() < 1 + (int)count * 6) {
		throw con::InvalidIncomingDataException
				("GOTBLOCKS length is too short");
	}

	for (u16 i = 0; i < count; i++) {
		v3s16 p;
		*pkt >> p;
		client->GotBlock(p);
	}
#endif
}

void Server::handleCommand_PlayerPos(NetworkPacket* pkt)
{
	if (pkt->getSize() < 12 + 12 + 4 + 4)
		return;

	v3s32 ps, ss;
	s32 f32pitch, f32yaw;

	*pkt >> ps;
	*pkt >> ss;
	*pkt >> f32pitch;
	*pkt >> f32yaw;

	f32 pitch = (f32)f32pitch / 100.0;
	f32 yaw = (f32)f32yaw / 100.0;
	u32 keyPressed = 0;

	if (pkt->getSize() >= 12 + 12 + 4 + 4 + 4)
		*pkt >> keyPressed;

	v3f position((f32)ps.X / 100.0, (f32)ps.Y / 100.0, (f32)ps.Z / 100.0);
	v3f speed((f32)ss.X / 100.0, (f32)ss.Y / 100.0, (f32)ss.Z / 100.0);

	pitch = modulo360f(pitch);
	yaw = modulo360f(yaw);

	Player *player = m_env->getPlayer(pkt->getPeerId());
	if (player == NULL) {
		errorstream << "Server::ProcessData(): Canceling: "
				"No player for peer_id=" << pkt->getPeerId()
				<< " disconnecting peer!" << std::endl;
		m_con.DisconnectPeer(pkt->getPeerId());
		return;
	}

	// If player is dead we don't care of this packet
	if (player->isDead()) {
		verbosestream << "TOSERVER_PLAYERPOS: " << player->getName()
			<< " is dead. Ignoring packet";
		return;
	}

	PlayerSAO *playersao = player->getPlayerSAO();
	if (playersao == NULL) {
		errorstream << "Server::ProcessData(): Canceling: "
				"No player object for peer_id=" << pkt->getPeerId()
				<< " disconnecting peer!" << std::endl;
		m_con.DisconnectPeer(pkt->getPeerId());
		return;
	}

	player->setPosition(position);
	player->setSpeed(speed);
	player->setPitch(pitch);
	player->setYaw(yaw);
	player->keyPressed = keyPressed;

	{
	std::lock_guard<Mutex> lock(player->control_mutex);

	player->control.up = (keyPressed & 1);
	player->control.down = (keyPressed & 2);
	player->control.left = (keyPressed & 4);
	player->control.right = (keyPressed & 8);
	player->control.jump = (keyPressed & 16);
	player->control.aux1 = (keyPressed & 32);
	player->control.sneak = (keyPressed & 64);
	player->control.LMB = (keyPressed & 128);
	player->control.RMB = (keyPressed & 256);
	}

	auto old_pos = playersao->m_last_good_position;
	if (playersao->checkMovementCheat()) {
		// Call callbacks
		m_script->on_cheat(playersao, "moved_too_fast");
		SendMovePlayer(pkt->getPeerId());
	}
// copypaste from fm_serverpackethandler.cpp
		else if (playersao->m_ms_from_last_respawn > 3000) {
			auto dist = (old_pos/BS).getDistanceFrom(playersao->m_last_good_position/BS);
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
//copypaste end
}

void Server::handleCommand_DeletedBlocks(NetworkPacket* pkt)
{
	if (pkt->getSize() < 1)
		return;

	/*
		[0] u16 command
		[2] u8 count
		[3] v3s16 pos_0
		[3+6] v3s16 pos_1
		...
	*/

	u8 count;
	*pkt >> count;

	RemoteClient *client = getClient(pkt->getPeerId());

	if ((s16)pkt->getSize() < 1 + (int)count * 6) {
		throw con::InvalidIncomingDataException
				("DELETEDBLOCKS length is too short");
	}

	for (u16 i = 0; i < count; i++) {
		v3s16 p;
		*pkt >> p;
		client->SetBlockDeleted(p);
	}
}

void Server::handleCommand_InventoryAction(NetworkPacket* pkt)
{
	Player *player = m_env->getPlayer(pkt->getPeerId());
	if (player == NULL) {
		errorstream << "Server::ProcessData(): Canceling: "
				"No player for peer_id=" << pkt->getPeerId()
				<< " disconnecting peer!" << std::endl;
		m_con.DisconnectPeer(pkt->getPeerId());
		return;
	}

	PlayerSAO *playersao = player->getPlayerSAO();
	if (playersao == NULL) {
		errorstream << "Server::ProcessData(): Canceling: "
				"No player object for peer_id=" << pkt->getPeerId()
				<< " disconnecting peer!" << std::endl;
		m_con.DisconnectPeer(pkt->getPeerId());
		return;
	}

	// Strip command and create a stream
	std::string datastring(pkt->getString(0), pkt->getSize());
	verbosestream << "TOSERVER_INVENTORY_ACTION: data=" << datastring
		<< std::endl;
	std::istringstream is(datastring, std::ios_base::binary);
	// Create an action
	InventoryAction *a = InventoryAction::deSerialize(is);
	if (a == NULL) {
		infostream << "TOSERVER_INVENTORY_ACTION: "
				<< "InventoryAction::deSerialize() returned NULL"
				<< std::endl;
		return;
	}

	// If something goes wrong, this player is to blame
	RollbackScopeActor rollback_scope(m_rollback,
			std::string("player:")+player->getName());

	/*
		Note: Always set inventory not sent, to repair cases
		where the client made a bad prediction.
	*/

	/*
		Handle restrictions and special cases of the move action
	*/
	if (a->getType() == IACTION_MOVE) {
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
		if (ma->from_list == "craftpreview") {
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
		if (ma->to_list == "craftpreview" || ma->to_list == "craftresult") {
			infostream << "Ignoring IMoveAction from "
					<< (ma->from_inv.dump()) << ":" << ma->from_list
					<< " to " << (ma->to_inv.dump()) << ":" << ma->to_list
					<< " because dst is " << ma->to_list << std::endl;
			delete a;
			return;
		}

		// Disallow moving items in elsewhere than player's inventory
		// if not allowed to interact
		if (!checkPriv(player->getName(), "interact") &&
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
	else if (a->getType() == IACTION_DROP) {
		IDropAction *da = (IDropAction*)a;

		da->from_inv.applyCurrentPlayer(player->getName());

		setInventoryModified(da->from_inv, false);

		/*
			Disable dropping items out of craftpreview
		*/
		if (da->from_list == "craftpreview") {
			infostream << "Ignoring IDropAction from "
					<< (da->from_inv.dump()) << ":" << da->from_list
					<< " because src is " << da->from_list << std::endl;
			delete a;
			return;
		}

		// Disallow dropping items if not allowed to interact
		if (!checkPriv(player->getName(), "interact")) {
			delete a;
			return;
		}
		stat.add("drop", player->getName());
	}
	/*
		Handle restrictions and special cases of the craft action
	*/
	else if (a->getType() == IACTION_CRAFT) {
		ICraftAction *ca = (ICraftAction*)a;

		ca->craft_inv.applyCurrentPlayer(player->getName());

		setInventoryModified(ca->craft_inv, false);

		//bool craft_inv_is_current_player =
		//	(ca->craft_inv.type == InventoryLocation::PLAYER) &&
		//	(ca->craft_inv.name == player->getName());

		// Disallow crafting if not allowed to interact
		if (!checkPriv(player->getName(), "interact")) {
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

void Server::handleCommand_ChatMessage(NetworkPacket* pkt)
{
	/*
		u16 command
		u16 length
		wstring message
	*/
	u16 len;
	*pkt >> len;

	std::wstring message;
	for (u16 i = 0; i < len; i++) {
		u16 tmp_wchar;
		*pkt >> tmp_wchar;

		message += (wchar_t)tmp_wchar;
	}

	Player *player = m_env->getPlayer(pkt->getPeerId());
	if (player == NULL) {
		errorstream << "Server::ProcessData(): Canceling: "
				"No player for peer_id=" << pkt->getPeerId()
				<< " disconnecting peer!" << std::endl;
		m_con.DisconnectPeer(pkt->getPeerId());
		return;
	}

	// Get player name of this client
	std::string name = player->getName();
	std::wstring wname = narrow_to_wide(name);

	std::wstring answer_to_sender = handleChat(name, wname, message,
		true, pkt->getPeerId());
	if (!answer_to_sender.empty()) {
		// Send the answer to sender
		SendChatMessage(pkt->getPeerId(), answer_to_sender);
	}
}

void Server::handleCommand_Damage(NetworkPacket* pkt)
{
	u8 damage;

	*pkt >> damage;

	Player *player = m_env->getPlayer(pkt->getPeerId());
	if (player == NULL) {
		errorstream << "Server::ProcessData(): Canceling: "
				"No player for peer_id=" << pkt->getPeerId()
				<< " disconnecting peer!" << std::endl;
		m_con.DisconnectPeer(pkt->getPeerId());
		return;
	}

	PlayerSAO *playersao = player->getPlayerSAO();
	if (playersao == NULL) {
		errorstream << "Server::ProcessData(): Canceling: "
				"No player object for peer_id=" << pkt->getPeerId()
				<< " disconnecting peer!" << std::endl;
		m_con.DisconnectPeer(pkt->getPeerId());
		return;
	}

	if (g_settings->getBool("enable_damage")) {
		actionstream << player->getName() << " damaged by "
				<< (int)damage << " hp at " << PP(player->getPosition() / BS)
				<< std::endl;

		playersao->setHP(playersao->getHP() - damage);
		SendPlayerHPOrDie(playersao);

		stat.add("damage", player->getName(), damage);
	}
}

void Server::handleCommand_Breath(NetworkPacket* pkt)
{
	u16 breath;

	*pkt >> breath;

	Player *player = m_env->getPlayer(pkt->getPeerId());
	if (player == NULL) {
		errorstream << "Server::ProcessData(): Canceling: "
				"No player for peer_id=" << pkt->getPeerId()
				<< " disconnecting peer!" << std::endl;
		m_con.DisconnectPeer(pkt->getPeerId());
		return;
	}

	/*
	 * If player is dead, we don't need to update the breath
	 * He is dead !
	 */
	if (player->isDead()) {
		verbosestream << "TOSERVER_BREATH: " << player->getName()
			<< " is dead. Ignoring packet";
		return;
	}


	PlayerSAO *playersao = player->getPlayerSAO();
	if (playersao == NULL) {
		errorstream << "Server::ProcessData(): Canceling: "
				"No player object for peer_id=" << pkt->getPeerId()
				<< " disconnecting peer!" << std::endl;
		m_con.DisconnectPeer(pkt->getPeerId());
		return;
	}

	playersao->setBreath(breath);
	SendPlayerBreath(pkt->getPeerId());
}

void Server::handleCommand_Password(NetworkPacket* pkt)
{
	if (pkt->getSize() != PASSWORD_SIZE * 2)
		return;

	std::string oldpwd;
	std::string newpwd;

	// Deny for clients using the new protocol
	RemoteClient* client = getClient(pkt->getPeerId(), CS_Created);
	if (client->net_proto_version >= 25) {
		infostream << "Server::handleCommand_Password(): Denying change: "
			<< " Client protocol version for peer_id=" << pkt->getPeerId()
			<< " too new!" << std::endl;
		return;
	}

	for (u16 i = 0; i < PASSWORD_SIZE - 1; i++) {
		char c = pkt->getChar(i);
		if (c == 0)
			break;
		oldpwd += c;
	}

	for (u16 i = 0; i < PASSWORD_SIZE - 1; i++) {
		char c = pkt->getChar(PASSWORD_SIZE + i);
		if (c == 0)
			break;
		newpwd += c;
	}

	Player *player = m_env->getPlayer(pkt->getPeerId());
	if (player == NULL) {
		errorstream << "Server::ProcessData(): Canceling: "
				"No player for peer_id=" << pkt->getPeerId()
				<< " disconnecting peer!" << std::endl;
		m_con.DisconnectPeer(pkt->getPeerId());
		return;
	}

	if (!base64_is_valid(newpwd)) {
		infostream<<"Server: " << player->getName() <<
				" supplied invalid password hash" << std::endl;
		// Wrong old password supplied!!
		SendChatMessage(pkt->getPeerId(), L"Invalid new password hash supplied. Password NOT changed.");
		return;
	}

	infostream << "Server: Client requests a password change from "
			<< "'" << oldpwd << "' to '" << newpwd << "'" << std::endl;

	std::string playername = player->getName();

	std::string checkpwd;
	m_script->getAuth(playername, &checkpwd, NULL);

	if (oldpwd != checkpwd) {
		infostream << "Server: invalid old password" << std::endl;
		// Wrong old password supplied!!
		SendChatMessage(pkt->getPeerId(), L"Invalid old password supplied. Password NOT changed.");
		return;
	}

	bool success = m_script->setPassword(playername, newpwd);
	if (success) {
		actionstream << player->getName() << " changes password" << std::endl;
		SendChatMessage(pkt->getPeerId(), L"Password change successful.");
	} else {
		actionstream << player->getName() << " tries to change password but "
				<< "it fails" << std::endl;
		SendChatMessage(pkt->getPeerId(), L"Password change failed or unavailable.");
	}
}

void Server::handleCommand_PlayerItem(NetworkPacket* pkt)
{
	if (pkt->getSize() < 2)
		return;

	Player *player = m_env->getPlayer(pkt->getPeerId());
	if (player == NULL) {
		errorstream << "Server::ProcessData(): Canceling: "
				"No player for peer_id=" << pkt->getPeerId()
				<< " disconnecting peer!" << std::endl;
		m_con.DisconnectPeer(pkt->getPeerId());
		return;
	}

	PlayerSAO *playersao = player->getPlayerSAO();
	if (playersao == NULL) {
		errorstream << "Server::ProcessData(): Canceling: "
				"No player object for peer_id=" << pkt->getPeerId()
				<< " disconnecting peer!" << std::endl;
		m_con.DisconnectPeer(pkt->getPeerId());
		return;
	}

	u16 item;

	*pkt >> item;

	playersao->setWieldIndex(item);
}

void Server::handleCommand_Respawn(NetworkPacket* pkt)
{
	Player *player = m_env->getPlayer(pkt->getPeerId());
	if (player == NULL) {
		errorstream << "Server::ProcessData(): Canceling: "
				"No player for peer_id=" << pkt->getPeerId()
				<< " disconnecting peer!" << std::endl;
		m_con.DisconnectPeer(pkt->getPeerId());
		return;
	}

	if (!player->isDead())
		return;

	RespawnPlayer(pkt->getPeerId());

	actionstream << player->getName() << " respawns at "
			<< PP(player->getPosition()/BS) << std::endl;

	// ActiveObject is added to environment in AsyncRunStep after
	// the previous addition has been successfully removed
}

void Server::handleCommand_Interact(NetworkPacket* pkt)
{
	std::string datastring(pkt->getString(0), pkt->getSize());
	std::istringstream is(datastring, std::ios_base::binary);

	/*
		[0] u16 command
		[2] u8 action
		[3] u16 item
		[5] u32 length of the next item
		[9] serialized PointedThing
		actions:
		0: start digging (from undersurface) or use
		1: stop digging (all parameters ignored)
		2: digging completed
		3: place block or item (to abovesurface)
		4: use item
	*/
	u8 action = readU8(is);
	u16 item_i = readU16(is);
	std::istringstream tmp_is(deSerializeLongString(is), std::ios::binary);
	PointedThing pointed;
	pointed.deSerialize(tmp_is);

	verbosestream << "TOSERVER_INTERACT: action=" << (int)action << ", item="
			<< item_i << ", pointed=" << pointed.dump() << std::endl;

	Player *player = m_env->getPlayer(pkt->getPeerId());
	if (player == NULL) {
		errorstream << "Server::ProcessData(): Canceling: "
				"No player for peer_id=" << pkt->getPeerId()
				<< " disconnecting peer!" << std::endl;
		m_con.DisconnectPeer(pkt->getPeerId());
		return;
	}

	PlayerSAO *playersao = player->getPlayerSAO();
	if (playersao == NULL) {
		errorstream << "Server::ProcessData(): Canceling: "
				"No player object for peer_id=" << pkt->getPeerId()
				<< " disconnecting peer!" << std::endl;
		m_con.DisconnectPeer(pkt->getPeerId());
		return;
	}

	if (player->isDead()) {
		verbosestream << "TOSERVER_INTERACT: " << player->getName()
			<< " is dead. Ignoring packet";
		return;
	}

	v3f player_pos = playersao->getLastGoodPosition();

	// Update wielded item
	playersao->setWieldIndex(item_i);

	// Get pointed to node (undefined if not POINTEDTYPE_NODE)
	v3s16 p_under = pointed.node_undersurface;
	v3s16 p_above = pointed.node_abovesurface;

	// Get pointed to object (NULL if not POINTEDTYPE_OBJECT)
	ServerActiveObject *pointed_object = NULL;
	if (pointed.type == POINTEDTHING_OBJECT) {
		pointed_object = m_env->getActiveObject(pointed.object_id);
		if (pointed_object == NULL) {
			verbosestream << "TOSERVER_INTERACT: "
				"pointed object is NULL" << std::endl;
			return;
		}

	}

	v3f pointed_pos_under = player_pos;
	v3f pointed_pos_above = player_pos;
	if (pointed.type == POINTEDTHING_NODE) {
		pointed_pos_under = intToFloat(p_under, BS);
		pointed_pos_above = intToFloat(p_above, BS);
	}
	else if (pointed.type == POINTEDTHING_OBJECT) {
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
		if (d > max_d) {
			actionstream << "Player " << player->getName()
					<< " tried to access " << pointed.dump()
					<< " from too far: "
					<< "d=" << d <<", max_d=" << max_d
					<< ". ignoring." << std::endl;
			// Re-send block to revert change on client-side
			RemoteClient *client = getClient(pkt->getPeerId());
			v3s16 blockpos = getNodeBlockPos(floatToInt(pointed_pos_under, BS));
			client->SetBlockNotSent(blockpos);
			// Call callbacks
			m_script->on_cheat(playersao, "interacted_too_far");
			// Do nothing else
			stat.add("interact_denied", player->getName());
			return;
		}
	}

	/*
		Make sure the player is allowed to do it
	*/
	if (!checkPriv(player->getName(), "interact")) {
		actionstream<<player->getName()<<" attempted to interact with "
				<<pointed.dump()<<" without 'interact' privilege"
				<<std::endl;
		// Re-send block to revert change on client-side
		RemoteClient *client = getClient(pkt->getPeerId());
		// Digging completed -> under
		if (action == 2) {
			v3s16 blockpos = getNodeBlockPos(floatToInt(pointed_pos_under, BS));
			client->SetBlockNotSent(blockpos);
		}
		// Placement -> above
		if (action == 3) {
			v3s16 blockpos = getNodeBlockPos(floatToInt(pointed_pos_above, BS));
			client->SetBlockNotSent(blockpos);
		}
		return;
	}

	/*
		If something goes wrong, this player is to blame
	*/
	RollbackScopeActor rollback_scope(m_rollback,
			std::string("player:")+player->getName());

	/*
		0: start digging or punch object
	*/
	if (action == 0) {
		if (pointed.type == POINTEDTHING_NODE) {
			/*
				NOTE: This can be used in the future to check if
				somebody is cheating, by checking the timing.
			*/

			MapNode n(CONTENT_IGNORE);
			bool pos_ok;

			n = m_env->getMap().getNodeNoEx(p_under, &pos_ok);
			if (!pos_ok) {
				infostream << "Server: Not punching: Node not found."
						<< " Adding block to emerge queue."
						<< std::endl;
				m_emerge->enqueueBlockEmerge(pkt->getPeerId(),
					getNodeBlockPos(p_above), false);
			}

			if (n.getContent() != CONTENT_IGNORE)
				m_script->node_on_punch(p_under, n, playersao, pointed);

			// Cheat prevention
			playersao->noCheatDigStart(p_under);
		}
		else if (pointed.type == POINTEDTHING_OBJECT) {
			// Skip if object has been removed
			if (pointed_object->m_removed)
				return;

			actionstream<<player->getName()<<" punches object "
					<<pointed.object_id<<": "
					<<pointed_object->getDescription()<<std::endl;

			ItemStack punchitem = playersao->getWieldedItem();
			ToolCapabilities toolcap =
					punchitem.getToolCapabilities(m_itemdef);
			v3f dir = (pointed_object->getBasePosition() -
					(player->getPosition() + player->getEyeOffset())
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
				SendPlayerHPOrDie((PlayerSAO *)pointed_object);
			}

			// If the puncher is a player and its HP changed
			if (dst_origin_hp != playersao->getHP())
				SendPlayerHPOrDie(playersao);

			stat.add("punch", player->getName());
		}

	} // action == 0

	/*
		1: stop digging
	*/
	else if (action == 1) {
	} // action == 1

	/*
		2: Digging completed
	*/
	else if (action == 2) {
		// Only digging of nodes
		if (pointed.type == POINTEDTHING_NODE) {
			bool pos_ok;
			MapNode n = m_env->getMap().getNodeNoEx(p_under, &pos_ok);
			if (!pos_ok) {
				infostream << "Server: Not finishing digging: Node not found."
						<< " Adding block to emerge queue."
						<< std::endl;
				m_emerge->enqueueBlockEmerge(pkt->getPeerId(),
					getNodeBlockPos(p_above), false);
			}

			/* Cheat prevention */
			bool is_valid_dig = true;
			if (enable_anticheat && !isSingleplayer()) {
				v3s16 nocheat_p = playersao->getNoCheatDigPos();
				float nocheat_t = playersao->getNoCheatDigTime();
				playersao->noCheatDigEnd();
				// If player didn't start digging this, ignore dig
				if (nocheat_p != p_under) {
					infostream << "Server: NoCheat: " << player->getName()
							<< " started digging "
							<< PP(nocheat_p) << " and completed digging "
							<< PP(p_under) << "; not digging." << std::endl;
					is_valid_dig = false;
					// Call callbacks
					m_script->on_cheat(playersao, "finished_unknown_dig");
				}
				// Get player's wielded item
				ItemStack playeritem;
				InventoryList *mlist = playersao->getInventory()->getList("main");
				if (mlist != NULL)
					playeritem = mlist->getItem(playersao->getWieldIndex());
				ToolCapabilities playeritem_toolcap =
						playeritem.getToolCapabilities(m_itemdef);
				// Get diggability and expected digging time
				DigParams params = getDigParams(m_nodedef->get(n).groups,
						&playeritem_toolcap);
				// If can't dig, try hand
				if (!params.diggable) {
					const ItemDefinition &hand = m_itemdef->get("");
					const ToolCapabilities *tp = hand.tool_capabilities;
					if (tp)
						params = getDigParams(m_nodedef->get(n).groups, tp);
				}
				// If can't dig, ignore dig
				if (!params.diggable) {
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

			if (is_valid_dig && n.getContent() != CONTENT_IGNORE) {
				m_script->node_on_dig(p_under, n, playersao);
				stat.add("dig", player->getName());
				stat.add("dig_"+ m_nodedef->get(n).name , player->getName());
					m_env->nodeUpdate(p_under, 5, 0);
			}

			v3s16 blockpos = getNodeBlockPos(floatToInt(pointed_pos_under, BS));
			RemoteClient *client = getClient(pkt->getPeerId());
			// Send unusual result (that is, node not being removed)
			if (m_env->getMap().getNodeNoEx(p_under).getContent() != CONTENT_AIR) {
				// Re-send block to revert change on client-side
				client->SetBlockNotSent(blockpos);
			}
			else {
				client->ResendBlockIfOnWire(blockpos);
			}
		}
	} // action == 2

	/*
		3: place block or right-click object
	*/
	else if (action == 3) {
		ItemStack item = playersao->getWieldedItem();

		// Reset build time counter
		if (pointed.type == POINTEDTHING_NODE &&
				item.getDefinition(m_itemdef).type == ITEM_NODE)
			getClient(pkt->getPeerId())->m_time_from_building = 0.0;

		if (pointed.type == POINTEDTHING_OBJECT) {
			// Right click object

			// Skip if object has been removed
			if (pointed_object->m_removed)
				return;

			actionstream << player->getName() << " right-clicks object "
					<< pointed.object_id << ": "
					<< pointed_object->getDescription() << std::endl;

			// Do stuff
			pointed_object->rightClick(playersao);

			m_env->nodeUpdate(p_under, 5, 0);
		}


		else if (m_script->item_OnPlace(
				item, playersao, pointed)) {
			// Placement was handled in lua

			// Apply returned ItemStack
			if (playersao->setWieldedItem(item)) {
				SendInventory(playersao);
			}

			stat.add("place", player->getName());
		}

		// If item has node placement prediction, always send the
		// blocks to make sure the client knows what exactly happened
		RemoteClient *client = getClient(pkt->getPeerId());
		v3s16 blockpos = getNodeBlockPos(floatToInt(pointed_pos_above, BS));
		v3s16 blockpos2 = getNodeBlockPos(floatToInt(pointed_pos_under, BS));
		if (item.getDefinition(m_itemdef).node_placement_prediction != "") {
			client->SetBlockNotSent(blockpos);
			if (blockpos2 != blockpos) {
				client->SetBlockNotSent(blockpos2);
			}
		}
		else {
			client->ResendBlockIfOnWire(blockpos);
			if (blockpos2 != blockpos) {
				client->ResendBlockIfOnWire(blockpos2);
			}
		}
	} // action == 3

	/*
		4: use
	*/
	else if (action == 4) {
		ItemStack item = playersao->getWieldedItem();

		actionstream << player->getName() << " uses " << item.name
				<< ", pointing at " << pointed.dump() << std::endl;

		if (m_script->item_OnUse(
				item, playersao, pointed)) {
			// Apply returned ItemStack
			if (playersao->setWieldedItem(item)) {
				SendInventory(playersao);
			}

			stat.add("use", player->getName());
			stat.add("use_" + item.name, player->getName());
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
		warningstream << "Server: Invalid action "
				<< action << std::endl;
	}
}

void Server::handleCommand_RemovedSounds(NetworkPacket* pkt)
{
	u16 num;
	*pkt >> num;
	for (u16 k = 0; k < num; k++) {
		s32 id;

		*pkt >> id;

		std::map<s32, ServerPlayingSound>::iterator i =
			m_playing_sounds.find(id);

		if (i == m_playing_sounds.end())
			continue;

		ServerPlayingSound &psound = i->second;
		psound.clients.erase(pkt->getPeerId());
		if (psound.clients.empty())
			m_playing_sounds.erase(i++);
	}
}

void Server::handleCommand_NodeMetaFields(NetworkPacket* pkt)
{
	v3s16 p;
	std::string formname;
	u16 num;

	*pkt >> p >> formname >> num;

	StringMap fields;
	for (u16 k = 0; k < num; k++) {
		std::string fieldname;
		*pkt >> fieldname;
		fields[fieldname] = pkt->readLongString();
	}

	Player *player = m_env->getPlayer(pkt->getPeerId());
	if (player == NULL) {
		errorstream << "Server::ProcessData(): Canceling: "
				"No player for peer_id=" << pkt->getPeerId()
				<< " disconnecting peer!" << std::endl;
		m_con.DisconnectPeer(pkt->getPeerId());
		return;
	}

	PlayerSAO *playersao = player->getPlayerSAO();
	if (playersao == NULL) {
		errorstream << "Server::ProcessData(): Canceling: "
				"No player object for peer_id=" << pkt->getPeerId()
				<< " disconnecting peer!"  << std::endl;
		m_con.DisconnectPeer(pkt->getPeerId());
		return;
	}

	// If something goes wrong, this player is to blame
	RollbackScopeActor rollback_scope(m_rollback,
			std::string("player:")+player->getName());

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

void Server::handleCommand_InventoryFields(NetworkPacket* pkt)
{
	std::string formname;
	u16 num;

	*pkt >> formname >> num;

	StringMap fields;
	for (u16 k = 0; k < num; k++) {
		std::string fieldname;
		*pkt >> fieldname;
		fields[fieldname] = pkt->readLongString();
	}

	Player *player = m_env->getPlayer(pkt->getPeerId());
	if (player == NULL) {
		errorstream << "Server::ProcessData(): Canceling: "
				"No player for peer_id=" << pkt->getPeerId()
				<< " disconnecting peer!" << std::endl;
		m_con.DisconnectPeer(pkt->getPeerId());
		return;
	}

	PlayerSAO *playersao = player->getPlayerSAO();
	if (playersao == NULL) {
		errorstream << "Server::ProcessData(): Canceling: "
				"No player object for peer_id=" << pkt->getPeerId()
				<< " disconnecting peer!" << std::endl;
		m_con.DisconnectPeer(pkt->getPeerId());
		return;
	}

	m_script->on_playerReceiveFields(playersao, formname, fields);
}

void Server::handleCommand_FirstSrp(NetworkPacket* pkt)
{
	RemoteClient* client = getClient(pkt->getPeerId(), CS_Invalid);
	ClientState cstate = client->getState();

	std::string playername = client->getName();

	std::string salt;
	std::string verification_key;

	std::string addr_s = getPeerAddress(pkt->getPeerId()).serializeString();
	u8 is_empty;

	*pkt >> salt >> verification_key >> is_empty;

	verbosestream << "Server: Got TOSERVER_FIRST_SRP from " << addr_s
		<< ", with is_empty= " << is_empty << std::endl;

	// Either this packet is sent because the user is new or to change the password
	if (cstate == CS_HelloSent) {
		if (!client->isMechAllowed(AUTH_MECHANISM_FIRST_SRP)) {
			actionstream << "Server: Client from " << addr_s
					<< " tried to set password without being "
					<< "authenticated, or the username being new." << std::endl;
			DenyAccess(pkt->getPeerId(), SERVER_ACCESSDENIED_UNEXPECTED_DATA);
			return;
		}

		if (!isSingleplayer() &&
				g_settings->getBool("disallow_empty_password") &&
				is_empty == 1) {
			actionstream << "Server: " << playername
					<< " supplied empty password from " << addr_s << std::endl;
			DenyAccess(pkt->getPeerId(), SERVER_ACCESSDENIED_EMPTY_PASSWORD);
			return;
		}

		std::string initial_ver_key;

		initial_ver_key = encode_srp_verifier(verification_key, salt);
		m_script->createAuth(playername, initial_ver_key);

		acceptAuth(pkt->getPeerId(), false);
	} else {
		if (cstate < CS_SudoMode) {
			infostream << "Server::ProcessData(): Ignoring TOSERVER_FIRST_SRP from "
					<< addr_s << ": " << "Client has wrong state " << cstate << "."
					<< std::endl;
			return;
		}
		m_clients.event(pkt->getPeerId(), CSE_SudoLeave);
		std::string pw_db_field = encode_srp_verifier(verification_key, salt);
		bool success = m_script->setPassword(playername, pw_db_field);
		if (success) {
			actionstream << playername << " changes password" << std::endl;
			SendChatMessage(pkt->getPeerId(), L"Password change successful.");
		} else {
			actionstream << playername << " tries to change password but "
				<< "it fails" << std::endl;
			SendChatMessage(pkt->getPeerId(), L"Password change failed or unavailable.");
		}
	}
}

void Server::handleCommand_SrpBytesA(NetworkPacket* pkt)
{
	RemoteClient* client = getClient(pkt->getPeerId(), CS_Invalid);
	ClientState cstate = client->getState();

	bool wantSudo = (cstate == CS_Active);

	if (!((cstate == CS_HelloSent) || (cstate == CS_Active))) {
		actionstream << "Server: got SRP _A packet in wrong state "
			<< cstate << " from "
			<< getPeerAddress(pkt->getPeerId()).serializeString()
			<< ". Ignoring." << std::endl;
		return;
	}

	if (client->chosen_mech != AUTH_MECHANISM_NONE) {
		actionstream << "Server: got SRP _A packet, while auth"
			<< "is already going on with mech " << client->chosen_mech
			<< " from " << getPeerAddress(pkt->getPeerId()).serializeString()
			<< " (wantSudo=" << wantSudo << "). Ignoring." << std::endl;
		if (wantSudo) {
			DenySudoAccess(pkt->getPeerId());
			return;
		} else {
			DenyAccess(pkt->getPeerId(), SERVER_ACCESSDENIED_UNEXPECTED_DATA);
			return;
		}
	}

	std::string bytes_A;
	u8 based_on;
	*pkt >> bytes_A >> based_on;

	infostream << "Server: TOSERVER_SRP_BYTES_A received with "
		<< "based_on=" << int(based_on) << " and len_A="
		<< bytes_A.length() << "." << std::endl;

	AuthMechanism chosen = (based_on == 0) ?
		AUTH_MECHANISM_LEGACY_PASSWORD : AUTH_MECHANISM_SRP;

	if (wantSudo) {
		if (!client->isSudoMechAllowed(chosen)) {
			actionstream << "Server: Player \"" << client->getName()
				<< "\" at " << getPeerAddress(pkt->getPeerId()).serializeString()
				<< " tried to change password using unallowed mech "
				<< chosen << "." << std::endl;
			DenySudoAccess(pkt->getPeerId());
			return;
		}
	} else {
		if (!client->isMechAllowed(chosen)) {
			actionstream << "Server: Client tried to authenticate from "
				<< getPeerAddress(pkt->getPeerId()).serializeString()
				<< " using unallowed mech " << chosen << "." << std::endl;
			DenyAccess(pkt->getPeerId(), SERVER_ACCESSDENIED_UNEXPECTED_DATA);
			return;
		}
	}

	client->chosen_mech = chosen;

	std::string salt;
	std::string verifier;

	if (based_on == 0) {

		generate_srp_verifier_and_salt(client->getName(), client->enc_pwd,
			&verifier, &salt);
	} else if (!decode_srp_verifier_and_salt(client->enc_pwd, &verifier, &salt)) {
		// Non-base64 errors should have been catched in the init handler
		actionstream << "Server: User " << client->getName()
			<< " tried to log in, but srp verifier field"
			<< " was invalid (most likely invalid base64)." << std::endl;
		DenyAccess(pkt->getPeerId(), SERVER_ACCESSDENIED_SERVER_FAIL);
		return;
	}

	char *bytes_B = 0;
	size_t len_B = 0;

	client->auth_data = srp_verifier_new(SRP_SHA256, SRP_NG_2048,
		client->getName().c_str(),
		(const unsigned char *) salt.c_str(), salt.size(),
		(const unsigned char *) verifier.c_str(), verifier.size(),
		(const unsigned char *) bytes_A.c_str(), bytes_A.size(),
		NULL, 0,
		(unsigned char **) &bytes_B, &len_B, NULL, NULL);

	if (!bytes_B) {
		actionstream << "Server: User " << client->getName()
			<< " tried to log in, SRP-6a safety check violated in _A handler."
			<< std::endl;
		if (wantSudo) {
			DenySudoAccess(pkt->getPeerId());
			return;
		} else {
			DenyAccess(pkt->getPeerId(), SERVER_ACCESSDENIED_UNEXPECTED_DATA);
			return;
		}
	}

	NetworkPacket resp_pkt(TOCLIENT_SRP_BYTES_S_B, 0, pkt->getPeerId());
	resp_pkt << salt << std::string(bytes_B, len_B);
	Send(&resp_pkt);
}

void Server::handleCommand_SrpBytesM(NetworkPacket* pkt)
{
	RemoteClient* client = getClient(pkt->getPeerId(), CS_Invalid);
	ClientState cstate = client->getState();

	bool wantSudo = (cstate == CS_Active);

	verbosestream << "Server: Recieved TOCLIENT_SRP_BYTES_M." << std::endl;

	if (!((cstate == CS_HelloSent) || (cstate == CS_Active))) {
		actionstream << "Server: got SRP _M packet in wrong state "
			<< cstate << " from "
			<< getPeerAddress(pkt->getPeerId()).serializeString()
			<< ". Ignoring." << std::endl;
		return;
	}

	if ((client->chosen_mech != AUTH_MECHANISM_SRP)
		&& (client->chosen_mech != AUTH_MECHANISM_LEGACY_PASSWORD)) {
		actionstream << "Server: got SRP _M packet, while auth"
			<< "is going on with mech " << client->chosen_mech
			<< " from " << getPeerAddress(pkt->getPeerId()).serializeString()
			<< " (wantSudo=" << wantSudo << "). Denying." << std::endl;
		if (wantSudo) {
			DenySudoAccess(pkt->getPeerId());
			return;
		} else {
			DenyAccess(pkt->getPeerId(), SERVER_ACCESSDENIED_UNEXPECTED_DATA);
			return;
		}
	}

	std::string bytes_M;
	*pkt >> bytes_M;

	if (srp_verifier_get_session_key_length((SRPVerifier *) client->auth_data)
			!= bytes_M.size()) {
		actionstream << "Server: User " << client->getName()
			<< " at " << getPeerAddress(pkt->getPeerId()).serializeString()
			<< " sent bytes_M with invalid length " << bytes_M.size() << std::endl;
		DenyAccess(pkt->getPeerId(), SERVER_ACCESSDENIED_UNEXPECTED_DATA);
		return;
	}

	unsigned char *bytes_HAMK = 0;

	srp_verifier_verify_session((SRPVerifier *) client->auth_data,
		(unsigned char *)bytes_M.c_str(), &bytes_HAMK);

	if (!bytes_HAMK) {
		if (wantSudo) {
			actionstream << "Server: User " << client->getName()
				<< " at " << getPeerAddress(pkt->getPeerId()).serializeString()
				<< " tried to change their password, but supplied wrong"
				<< " (SRP) password for authentication." << std::endl;
			DenySudoAccess(pkt->getPeerId());
			return;
		} else {
			actionstream << "Server: User " << client->getName()
				<< " at " << getPeerAddress(pkt->getPeerId()).serializeString()
				<< " supplied wrong password (auth mechanism: SRP)."
				<< std::endl;
			DenyAccess(pkt->getPeerId(), SERVER_ACCESSDENIED_WRONG_PASSWORD);
			return;
		}
	}

	if (client->create_player_on_auth_success) {
		std::string playername = client->getName();
		m_script->createAuth(playername, client->enc_pwd);

		std::string checkpwd; // not used, but needed for passing something
		if (!m_script->getAuth(playername, &checkpwd, NULL)) {
			actionstream << "Server: " << playername << " cannot be authenticated"
				<< " (auth handler does not work?)" << std::endl;
			DenyAccess(pkt->getPeerId(), SERVER_ACCESSDENIED_SERVER_FAIL);
			return;
		}
		client->create_player_on_auth_success = false;
	}

	acceptAuth(pkt->getPeerId(), wantSudo);
}

void Server::handleCommand_Drawcontrol(NetworkPacket* pkt) { }

#endif
