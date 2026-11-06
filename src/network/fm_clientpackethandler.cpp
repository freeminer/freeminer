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

#include "client/client.h"

#include "chatmessage.h"
#include "client/camera.h"
#include "client/clientevent.h"
#include "client/clientmedia.h"
#include "client/localplayer.h"
#include "client/mesh_generator_thread.h"
#include "client/minimap.h"
#include "client/sound.h"
#include "filesys.h"
#include "fm_world_merge.h"
#include "gettext.h"
#include "itemdef.h"
#include "lighting.h"
#include "map.h"
#include "mapgen/mapgen.h"
#include "modchannels.h"
#include "network/connection.h"
#include "network/networkpacket.h"
#include "nodedef.h"
#include "nodemetadata.h"
#include "particles.h"
#include "script/scripting_client.h"
#include "serialization.h"
#include "servermap.h"
#include "settings.h"
#include "skyparams.h"
#include "sound_spec.h"
#include "util/base64.h"
#include "util/hashing.h"
#include "util/serialize.h"
#include "util/srp.h"
#include "util/strfnd.h"
#include "util/string.h"
#include "network/clientopcodes.h"
#include "fm_networkprotocol.h"

#include <memory>
#include <sstream>
#include <tuple>

namespace
{
const char *const fmAccessDeniedStrings[SERVER_ACCESSDENIED_MAX] = {
		N_("Invalid password"),
		N_("Your client sent something the server didn't expect. Try reconnecting or updating your client."),
		N_("The server is running in singleplayer mode. You cannot connect."),
		N_("Your client's version is not supported.\nPlease contact the server administrator."),
		N_("Player name contains disallowed characters"),
		N_("Player name not allowed"),
		N_("Too many users"),
		N_("Empty passwords are disallowed. Set a password and try again."),
		N_("Another client is connected with this name. If your client closed unexpectedly, try again in a minute."),
		N_("Internal server error"),
		"",
		N_("Server shutting down"),
		N_("The server has experienced an internal error. You will now be disconnected."),
};
}

void Client::handleCommand_Deprecated(NetworkPacket *pkt)
{
	infostream << "Got deprecated command "
			   << toClientCommandTable[pkt->getCommand()].name << " from peer "
			   << pkt->getPeerId() << "!" << std::endl;
}

void Client::handleCommand_Hello(NetworkPacket *pkt)
{
	auto &packet = *(pkt->packet);
	u8 serialization_ver; // negotiated value
	u16 proto_ver;
	u16 unused_compression_mode;
	u32 auth_mechs;
	std::string unused;
	packet[TOCLIENT_HELLO_SERIALIZATION_VERSION].convert(serialization_ver);
	packet[TOCLIENT_HELLO_COMPRESSION_MODE].convert(unused_compression_mode);
	packet[TOCLIENT_HELLO_PROTOCOL_VERSION].convert(proto_ver);
	packet[TOCLIENT_HELLO_AUTH_METHODS].convert(auth_mechs);
	packet[TOCLIENT_HELLO_LEGACY_PASSWORD].convert(unused);

	// Chose an auth method we support
	AuthMechanism chosen_auth_mechanism = choseAuthMech(auth_mechs);

	infostream << "Client: TOCLIENT_HELLO received with "
			   << "serialization_ver=" << (u32)serialization_ver
			   << ", auth_mechs=" << auth_mechs << ", proto_ver=" << proto_ver
			   << ". Doing auth with mech " << chosen_auth_mechanism << std::endl;

	if (!ser_ver_supported_read(serialization_ver)) {
		infostream << "Client: TOCLIENT_HELLO: Server sent "
				   << "unsupported ser_fmt_ver=" << (int)serialization_ver << std::endl;
		return;
	}

	m_server_ser_ver = serialization_ver;
	m_proto_ver = proto_ver;

	if (m_chosen_auth_mech != AUTH_MECHANISM_NONE) {
		// we received a TOCLIENT_HELLO while auth was already going on
		errorstream << "Client: TOCLIENT_HELLO while auth was already going on"
					<< "(chosen_mech=" << m_chosen_auth_mech << ")." << std::endl;
		if (m_chosen_auth_mech == AUTH_MECHANISM_SRP ||
				m_chosen_auth_mech == AUTH_MECHANISM_LEGACY_PASSWORD) {
			srp_user_delete((SRPUser *)m_auth_data);
			m_auth_data = 0;
		}
	}

	// Authenticate using that method, or abort if there wasn't any method found
	if (chosen_auth_mechanism != AUTH_MECHANISM_NONE) {
		bool is_register = chosen_auth_mechanism == AUTH_MECHANISM_FIRST_SRP;
		ELoginRegister mode =
				is_register ? ELoginRegister::Register : ELoginRegister::Login;
		if (m_allow_login_or_register != ELoginRegister::Any &&
				m_allow_login_or_register != mode) {
			m_chosen_auth_mech = AUTH_MECHANISM_NONE;
			m_access_denied = true;
			if (m_allow_login_or_register == ELoginRegister::Login) {
				m_access_denied_reason = gettext(
						"Name is not registered. To create an account on this server, click 'Register'");
			} else {
				m_access_denied_reason =
						gettext("Name is taken. Please choose another name");
			}
			m_con->Disconnect();
		} else {
			startAuth(chosen_auth_mechanism);
		}
	} else {
		m_chosen_auth_mech = AUTH_MECHANISM_NONE;
		m_access_denied = true;
		m_access_denied_reason = "Unknown";
		m_con->Disconnect();
	}
}

void Client::handleCommand_AuthAccept(NetworkPacket *pkt)
{
	deleteAuthData();

	auto &packet = *(pkt->packet);
	packet[TOCLIENT_AUTH_ACCEPT_SEED].convert(m_map_seed);
	packet[TOCLIENT_AUTH_ACCEPT_RECOMMENDED_SEND_INTERVAL].convert(
			m_recommended_send_interval);
	packet[TOCLIENT_AUTH_ACCEPT_SUDO_AUTH_METHODS].convert(m_sudo_auth_methods);

	infostream << "Client: received map seed: " << m_map_seed << std::endl;
	infostream << "Client: received recommended send interval "
			   << m_recommended_send_interval << std::endl;

	// Reply to server
	/* TRANSLATORS: DO NOT TRANSLATE THIS LITERALLY!
	This is a special string which needs to contain the translation's
	language code (e.g. "de" for German). */
	std::string lang = gettext("LANG_CODE");
	if (lang == "LANG_CODE")
		lang.clear();

	MSGPACK_PACKET_INIT((int)TOSERVER_INIT2, 1);
	PACK(TOSERVER_INIT2_LANGUAGE, lang);
	Send(1, buffer, true);

	m_state = LC_Init;

	// Log meaningful info
	if (!m_internal_server) {
		Address remote = m_con->GetPeerAddress(PEER_ID_SERVER);
		actionstream << "Connected to " << m_address_name << " (";
		remote.print(actionstream);
		actionstream << ")" << std::endl;
	}
}

void Client::handleCommand_AcceptSudoMode(NetworkPacket *pkt)
{
	deleteAuthData();

	m_password = m_new_password;

	verbosestream << "Client: Received TOCLIENT_ACCEPT_SUDO_MODE." << std::endl;

	// send packet to actually set the password
	startAuth(AUTH_MECHANISM_FIRST_SRP);

	// reset again
	m_chosen_auth_mech = AUTH_MECHANISM_NONE;
}

void Client::handleCommand_DenySudoMode(NetworkPacket *pkt)
{
	ChatMessage *chatMessage = new ChatMessage(
			CHATMESSAGE_TYPE_SYSTEM, L"Password change denied. Password NOT changed.");
	pushToChatQueue(chatMessage);
	// reset everything and be sad
	deleteAuthData();
}

void Client::handleCommand_AccessDenied(NetworkPacket *pkt)
{
	auto &packet = *(pkt->packet);
	m_access_denied = true;
	m_access_denied_reason.clear();
	packet_convert_safe(
			packet, TOCLIENT_ACCESS_DENIED_CUSTOM_STRING, m_access_denied_reason);
	m_access_denied_reconnect = false;
	packet_convert_safe(
			packet, TOCLIENT_ACCESS_DENIED_RECONNECT, m_access_denied_reconnect);

	u8 denyCode = SERVER_ACCESSDENIED_UNEXPECTED_DATA;
	packet_convert_safe(packet, TOCLIENT_ACCESS_DENIED_REASON, denyCode);
	if (denyCode == SERVER_ACCESSDENIED_TOO_MANY_USERS)
		m_access_denied_reconnect = true;

	if (m_access_denied_reason.empty()) {
		if (denyCode < SERVER_ACCESSDENIED_MAX &&
				denyCode != SERVER_ACCESSDENIED_CUSTOM_STRING)
			m_access_denied_reason = gettext(fmAccessDeniedStrings[denyCode]);
		else
			m_access_denied_reason = gettext("Unknown disconnect reason.");
	}
}

void Client::handleCommand_RemoveNode(NetworkPacket *pkt)
{
	auto &packet = *(pkt->packet);
	v3pos_t p = packet[TOCLIENT_REMOVENODE_POS].as<v3pos_t>();
	removeNode(p, 2); // Use light from the top node.
}

void Client::handleCommand_AddNode(NetworkPacket *pkt)
{
	auto &packet = *(pkt->packet);
	v3pos_t p = packet[TOCLIENT_ADDNODE_POS].as<v3pos_t>();
	MapNode n = packet[TOCLIENT_ADDNODE_NODE].as<MapNode>();
	bool remove_metadata = packet[TOCLIENT_ADDNODE_REMOVE_METADATA].as<bool>();

	addNode(p, n, remove_metadata, 2);
}

void Client::handleCommand_Inventory(NetworkPacket *pkt)
{
	auto &packet = *(pkt->packet);
	std::string datastring = packet[TOCLIENT_INVENTORY_DATA].as<std::string>();
	std::istringstream is(datastring, std::ios_base::binary);
	LocalPlayer *player = m_env.getLocalPlayer();
	if (!player)
		return;

	player->inventory.deSerialize(is);
	m_skip_next_wield_animation = false;
	packet_convert_safe(
			packet, TOCLIENT_INVENTORY_SKIP_WIELD_ANIM, m_skip_next_wield_animation);
	m_update_wielded_item = true;
	m_inventory_from_server = std::make_unique<Inventory>(player->inventory);
	m_inventory_from_server_age = 0.0f;
}

void Client::handleCommand_TimeOfDay(NetworkPacket *pkt)
{
	auto &packet = *(pkt->packet);
	u16 time_of_day = packet[TOCLIENT_TIME_OF_DAY_TIME].as<u16>() % 24000;
	f32 time_speed = packet[TOCLIENT_TIME_OF_DAY_TIME_SPEED].as<f32>();

	m_env.setTimeOfDay(time_of_day);
	m_env.setTimeOfDaySpeed(time_speed);
}

void Client::handleCommand_ChatMessage(NetworkPacket *pkt)
{
	auto &packet = *(pkt->packet);
	const std::string message = packet[TOCLIENT_CHAT_MESSAGE_DATA].as<std::string>();

	u8 type = CHATMESSAGE_TYPE_NORMAL;
	std::string sender;
	u64 timestamp = std::time(nullptr);
	packet_convert_safe(packet, TOCLIENT_CHAT_MESSAGE_TYPE, type);
	packet_convert_safe(packet, TOCLIENT_CHAT_MESSAGE_SENDER, sender);
	packet_convert_safe(packet, TOCLIENT_CHAT_MESSAGE_TIMESTAMP, timestamp);
	if (type >= CHATMESSAGE_TYPE_MAX)
		return;
	auto *chatMessage = new ChatMessage(static_cast<ChatMessageType>(type), message,
			sender, static_cast<std::time_t>(timestamp));
	if (modsLoaded() && m_script->on_receiving_message(message)) {
		delete chatMessage;
	} else {
		pushToChatQueue(chatMessage);
	}
}

void Client::handleCommand_ActiveObjectRemoveAdd(NetworkPacket *pkt)
{
	auto &packet = *(pkt->packet);
	std::vector<u16> removed_objects;
	packet[TOCLIENT_ACTIVE_OBJECT_REMOVE_ADD_REMOVE].convert(removed_objects);
	for (size_t i = 0; i < removed_objects.size(); ++i)
		m_env.removeActiveObject(removed_objects[i]);

	std::vector<ActiveObjectAddData> added_objects;
	packet[TOCLIENT_ACTIVE_OBJECT_REMOVE_ADD_ADD].convert(added_objects);
	for (size_t i = 0; i < added_objects.size(); ++i)
		m_env.addActiveObject(
				added_objects[i].id, added_objects[i].type, added_objects[i].data);
}

void Client::handleCommand_ActiveObjectMessages(NetworkPacket *pkt)
{
	auto &packet = *(pkt->packet);
	ActiveObjectMessages messages;
	packet[TOCLIENT_ACTIVE_OBJECT_MESSAGES_MESSAGES].convert(messages);
	for (size_t i = 0; i < messages.size(); ++i)
		m_env.processActiveObjectMessage(messages[i].first, messages[i].second);
}

void Client::handleCommand_Movement(NetworkPacket *pkt)
{
	auto &packet = *(pkt->packet);
	LocalPlayer *player = m_env.getLocalPlayer();
	if (!player)
		return;
	packet[TOCLIENT_MOVEMENT_ACCELERATION_DEFAULT].convert(
			player->movement_acceleration_default);
	packet[TOCLIENT_MOVEMENT_ACCELERATION_AIR].convert(player->movement_acceleration_air);
	packet[TOCLIENT_MOVEMENT_ACCELERATION_FAST].convert(
			player->movement_acceleration_fast);
	packet[TOCLIENT_MOVEMENT_SPEED_WALK].convert(player->movement_speed_walk);
	packet[TOCLIENT_MOVEMENT_SPEED_CROUCH].convert(player->movement_speed_crouch);
	packet[TOCLIENT_MOVEMENT_SPEED_FAST].convert(player->movement_speed_fast);
	packet[TOCLIENT_MOVEMENT_SPEED_CLIMB].convert(player->movement_speed_climb);
	packet[TOCLIENT_MOVEMENT_SPEED_JUMP].convert(player->movement_speed_jump);
	packet[TOCLIENT_MOVEMENT_LIQUID_FLUIDITY].convert(player->movement_liquid_fluidity);
	packet[TOCLIENT_MOVEMENT_LIQUID_FLUIDITY_SMOOTH].convert(
			player->movement_liquid_fluidity_smooth);
	packet[TOCLIENT_MOVEMENT_LIQUID_SINK].convert(player->movement_liquid_sink);
	packet[TOCLIENT_MOVEMENT_GRAVITY].convert(player->movement_gravity);
	packet_convert_safe(packet, TOCLIENT_MOVEMENT_FALL_AERODYNAMICS,
			player->movement_fall_aerodynamics);
}

void Client::handleCommand_HP(NetworkPacket *pkt)
{
	auto &packet = *(pkt->packet);
	LocalPlayer *player = m_env.getLocalPlayer();
	if (!player)
		return;

	const u16 oldhp = player->hp;
	const u16 hp = packet[TOCLIENT_HP_HP].as<u16>();
	bool effect = false;
	packet_convert_safe(packet, TOCLIENT_HP_EFFECT, effect);
	player->hp = hp;

	if (modsLoaded())
		m_script->on_hp_modification(hp);

	if (hp < oldhp) {
		auto *event = new ClientEvent(CE_PLAYER_DAMAGE);
		event->player_damage.amount = oldhp - hp;
		event->player_damage.effect = effect;
		m_client_event_queue.push(event);
	}
}

void Client::handleCommand_Breath(NetworkPacket *pkt)
{
	auto &packet = *(pkt->packet);
	LocalPlayer *player = m_env.getLocalPlayer();
	if (player)
		player->setBreath(packet[TOCLIENT_BREATH_BREATH].as<u16>());
}

void Client::handleCommand_MovePlayer(NetworkPacket *pkt)
{
	auto &packet = *(pkt->packet);
	auto player = m_env.getLocalPlayer();
	if (!player)
		return;

	v3f pos = packet[TOCLIENT_MOVE_PLAYER_POS].as<v3f>();
	f32 pitch = packet[TOCLIENT_MOVE_PLAYER_PITCH].as<f32>();
	f32 yaw = packet[TOCLIENT_MOVE_PLAYER_YAW].as<f32>();
	player->setPosition(pos);

	/*
			v3f speed;
			if (packet.count(TOCLIENT_MOVE_PLAYER_SPEED)) {
				speed = packet[TOCLIENT_MOVE_PLAYER_SPEED].as<v3f>();
				player->setSpeed(speed);
			}
	*/

	infostream << "Client got TOCLIENT_MOVE_PLAYER" << " pos=(" << pos.X << "," << pos.Y
			   << "," << pos.Z << ")" << " pitch=" << pitch << " yaw="
			   << yaw
			   //<<" speed="<<speed
			   << std::endl;

	/*
		Add to ClientEvent queue.
		This has to be sent to the main program because otherwise
		it would just force the pitch and yaw values to whatever
		the camera points to.
	*/
	auto *event = new ClientEvent(CE_PLAYER_FORCE_MOVE);
	event->player_force_move.pitch = pitch;
	event->player_force_move.yaw = yaw;
	m_client_event_queue.push(event);
}

void Client::handleCommand_PunchPlayer(NetworkPacket *pkt)
{
	auto &packet = *(pkt->packet);
	auto player = m_env.getLocalPlayer();
	if (!player)
		return;

	v3f speed = packet[TOCLIENT_PUNCH_PLAYER_SPEED].as<v3f>();
	player->addSpeed(speed);
}

void Client::handleCommand_DeathScreenLegacy(NetworkPacket *pkt)
{
	m_client_event_queue.push(new ClientEvent(CE_DEATHSCREEN_LEGACY));
}

void Client::handleCommand_AnnounceMedia(NetworkPacket *pkt)
{
	auto &packet = *(pkt->packet);
	if (m_media_downloader == NULL || m_media_downloader->isStarted()) {
		const char *problem = m_media_downloader ? "we already saw another announcement"
												 : "all media has been received already";
		errorstream << "Client: Received media announcement but " << problem << "!"
					<< std::endl;
		return;
	}

	// Mesh updates must be stopped while media is received for content loading.
	sanity_check(!m_mesh_update_manager->isRunning());

	MediaAnnounceList announce_list;
	packet[TOCLIENT_ANNOUNCE_MEDIA_LIST].convert(announce_list);
	for (size_t i = 0; i < announce_list.size(); ++i)
		m_media_downloader->addFile(
				announce_list[i].first, base64_decode(announce_list[i].second));

	std::string remote_media_string =
			packet[TOCLIENT_ANNOUNCE_MEDIA_REMOTE_SERVER].as<std::string>();
	Strfnd sf(remote_media_string);
	while (!sf.at_end()) {
		std::string baseurl = trim(sf.next(","));
		if (!baseurl.empty()) {
			m_remote_media_servers.emplace_back(baseurl);
			m_media_downloader->addRemoteServer(baseurl);
		}
	}

	m_media_downloader->step(this);
}

void Client::handleCommand_Media(NetworkPacket *pkt)
{
	auto &packet = *(pkt->packet);
	MediaData media_data;
	packet[TOCLIENT_MEDIA_MEDIA].convert(media_data);
	if (media_data.empty())
		return;

	if (!m_media_downloader || !m_media_downloader->isStarted()) {
		errorstream << "Client: Received media but no downloads are pending" << std::endl;
		return;
	}
	sanity_check(!m_mesh_update_manager->isRunning());

	for (auto &i : media_data)
		m_media_downloader->conventionalTransferDone(i.first, i.second, this);
}

void Client::handleCommand_NodeDef(NetworkPacket *pkt)
{
	auto &packet = *(pkt->packet);
	infostream << "Client: Received node definitions: packet size: " << pkt->getSize()
			   << std::endl;

	// Do this check before decompression; content definitions are not thread-safe.
	sanity_check(!m_mesh_update_manager->isRunning());

	std::string data;
	if (packet_convert_safe(packet, TOCLIENT_NODEDEF_DEFINITIONS_ZIP, data)) {
		std::istringstream compressed(data, std::ios_base::binary);
		std::ostringstream decompressed(std::ios_base::binary);
		decompressZlib(compressed, decompressed);
		data = decompressed.str();
	} else if (!packet_convert_safe(packet, TOCLIENT_NODEDEF_DEFINITIONS, data)) {
		errorstream << "Client: Node definitions packet has no data" << std::endl;
		return;
	}

	std::istringstream stream(data, std::ios_base::binary);
	m_nodedef->deSerialize(stream, m_proto_ver);
	m_nodedef_received = true;
}

void Client::handleCommand_ItemDef(NetworkPacket *pkt)
{
	auto &packet = *(pkt->packet);
	infostream << "Client: Received item definitions: packet size: " << pkt->getSize()
			   << std::endl;
	sanity_check(!m_mesh_update_manager->isRunning());

	std::string data;
	if (packet_convert_safe(packet, TOCLIENT_ITEMDEF_DEFINITIONS_ZIP, data)) {
		std::istringstream compressed(data, std::ios_base::binary);
		std::ostringstream decompressed(std::ios_base::binary);
		decompressZlib(compressed, decompressed);
		data = decompressed.str();
	} else if (!packet_convert_safe(packet, TOCLIENT_ITEMDEF_DEFINITIONS, data)) {
		errorstream << "Client: Item definitions packet has no data" << std::endl;
		return;
	}

	std::istringstream stream(data, std::ios_base::binary);
	m_itemdef->deSerialize(stream, m_proto_ver);
	m_itemdef_received = true;
}

void Client::handleCommand_PlaySound(NetworkPacket *pkt)
{
	auto &packet = *(pkt->packet);
	const s32 server_id = packet[TOCLIENT_PLAY_SOUND_ID].as<s32>();
	SoundSpec spec;
	packet[TOCLIENT_PLAY_SOUND_NAME].convert(spec.name);
	packet[TOCLIENT_PLAY_SOUND_GAIN].convert(spec.gain);
	packet[TOCLIENT_PLAY_SOUND_LOOP].convert(spec.loop);
	bool ephemeral = false;
	packet_convert_safe(packet, TOCLIENT_PLAY_SOUND_FADE, spec.fade);
	packet_convert_safe(packet, TOCLIENT_PLAY_SOUND_PITCH, spec.pitch);
	packet_convert_safe(packet, TOCLIENT_PLAY_SOUND_EPHEMERAL, ephemeral);
	packet_convert_safe(packet, TOCLIENT_PLAY_SOUND_START_TIME, spec.start_time);
	const auto type =
			static_cast<SoundLocation>(packet[TOCLIENT_PLAY_SOUND_TYPE].as<u8>());
	v3f pos = packet[TOCLIENT_PLAY_SOUND_POS].as<v3f>() * (1.0f / BS);
	const u16 object_id = packet[TOCLIENT_PLAY_SOUND_OBJECT_ID].as<u16>();

	const sound_handle_t client_id = m_sound->allocateId(2);
	switch (type) {
	case SoundLocation::Local:
		m_sound->playSound(client_id, spec);
		break;
	case SoundLocation::Position:
		m_sound->playSoundAt(client_id, spec, pos, v3f());
		break;
	case SoundLocation::Object: {
		v3f velocity;
		if (const auto *cao = m_env.getActiveObject(object_id)) {
			pos = cao->getPosition() * (1.0f / BS);
			velocity = cao->getVelocity() * (1.0f / BS);
		}
		m_sound->playSoundAt(client_id, spec, pos, velocity);
		break;
	}
	default:
		m_sound->freeId(client_id, 2);
		sendRemovedSounds({server_id});
		return;
	}

	if (ephemeral)
		return;
	m_sounds_server_to_client[server_id] = client_id;
	m_sounds_client_to_server[client_id] = server_id;
	if (object_id != 0)
		m_sounds_to_objects[client_id] = object_id;
}

void Client::handleCommand_StopSound(NetworkPacket *pkt)
{
	auto &packet = *(pkt->packet);
	s32 server_id = packet[TOCLIENT_STOP_SOUND_ID].as<s32>();
	auto i = m_sounds_server_to_client.find(server_id);
	if (i != m_sounds_server_to_client.end()) {
		int client_id = i->second;
		m_sound->stopSound(client_id);
	}
}

void Client::handleCommand_Privileges(NetworkPacket *pkt)
{
	auto &packet = *(pkt->packet);
	packet[TOCLIENT_PRIVILEGES_PRIVILEGES].convert(m_privileges);
}

void Client::handleCommand_InventoryFormSpec(NetworkPacket *pkt)
{
	auto &packet = *(pkt->packet);
	auto player = m_env.getLocalPlayer();
	if (!player)
		return;
	// Store formspec in LocalPlayer
	player->inventory_formspec =
			packet[TOCLIENT_INVENTORY_FORMSPEC_DATA].as<std::string>();
}

void Client::handleCommand_DetachedInventory(NetworkPacket *pkt)
{
	auto &packet = *(pkt->packet);
	std::string name = packet[TOCLIENT_DETACHED_INVENTORY_NAME].as<std::string>();
	bool exists = true;
	packet_convert_safe(packet, TOCLIENT_DETACHED_INVENTORY_EXISTS, exists);
	if (!exists) {
		auto it = m_detached_inventories.find(name);
		if (it != m_detached_inventories.end()) {
			delete it->second;
			m_detached_inventories.erase(it);
		}
		return;
	}
	std::string datastring = packet[TOCLIENT_DETACHED_INVENTORY_DATA].as<std::string>();
	std::istringstream is(datastring, std::ios_base::binary);

	infostream << "Client: Detached inventory update: \"" << name << "\"" << std::endl;

	Inventory *inv = NULL;
	if (m_detached_inventories.count(name) > 0)
		inv = m_detached_inventories[name];
	else {
		inv = new Inventory(static_cast<IItemDefManager *>(m_itemdef));
		m_detached_inventories[name] = inv;
	}
	inv->deSerialize(is);
}

void Client::handleCommand_ShowFormSpec(NetworkPacket *pkt)
{
	auto &packet = *(pkt->packet);
	auto *event = new ClientEvent(CE_SHOW_FORMSPEC);
	event->show_formspec.formspec =
			new std::string(packet[TOCLIENT_SHOW_FORMSPEC_DATA].as<std::string>());
	event->show_formspec.formname =
			new std::string(packet[TOCLIENT_SHOW_FORMSPEC_NAME].as<std::string>());
	m_client_event_queue.push(event);
}

void Client::handleCommand_SpawnParticle(NetworkPacket *pkt)
{
	auto &packet = *(pkt->packet);
	auto p = std::make_unique<ParticleParameters>();
	std::string data;
	if (packet_convert_safe(packet, TOCLIENT_SPAWN_PARTICLE_DATA, data)) {
		std::istringstream stream(data, std::ios_base::binary);
		p->deSerialize(stream, m_proto_ver);
		auto *event = new ClientEvent(CE_SPAWN_PARTICLE);
		event->spawn_particle = p.release();
		m_client_event_queue.push(event);
		return;
	}
	packet[TOCLIENT_SPAWN_PARTICLE_POS].convert(p->pos);
	packet[TOCLIENT_SPAWN_PARTICLE_VELOCITY].convert(p->vel);
	packet[TOCLIENT_SPAWN_PARTICLE_ACCELERATION].convert(p->acc);
	packet[TOCLIENT_SPAWN_PARTICLE_EXPIRATIONTIME].convert(p->expirationtime);
	packet[TOCLIENT_SPAWN_PARTICLE_SIZE].convert(p->size);
	packet[TOCLIENT_SPAWN_PARTICLE_COLLISIONDETECTION].convert(p->collisiondetection);
	packet[TOCLIENT_SPAWN_PARTICLE_VERTICAL].convert(p->vertical);
	packet[TOCLIENT_SPAWN_PARTICLE_TEXTURE].convert(p->texture.string);
	packet_convert_safe(
			packet, TOCLIENT_SPAWN_PARTICLE_COLLISION_REMOVAL, p->collision_removal);

	auto *event = new ClientEvent(CE_SPAWN_PARTICLE);
	event->spawn_particle = p.release();
	m_client_event_queue.push(event);
}

void Client::handleCommand_AddParticleSpawner(NetworkPacket *pkt)
{
	auto &packet = *(pkt->packet);
	auto p = std::make_unique<ParticleSpawnerParameters>();
	p->attractor_kind = ParticleParamTypes::AttractorKind::none;
	packet[TOCLIENT_ADD_PARTICLESPAWNER_AMOUNT].convert(p->amount);
	packet[TOCLIENT_ADD_PARTICLESPAWNER_SPAWNTIME].convert(p->time);
	packet[TOCLIENT_ADD_PARTICLESPAWNER_MINPOS].convert(p->pos.start.min.val);
	packet[TOCLIENT_ADD_PARTICLESPAWNER_MAXPOS].convert(p->pos.start.max.val);
	packet[TOCLIENT_ADD_PARTICLESPAWNER_MINVEL].convert(p->vel.start.min.val);
	packet[TOCLIENT_ADD_PARTICLESPAWNER_MAXVEL].convert(p->vel.start.max.val);
	packet[TOCLIENT_ADD_PARTICLESPAWNER_MINACC].convert(p->acc.start.min.val);
	packet[TOCLIENT_ADD_PARTICLESPAWNER_MAXACC].convert(p->acc.start.max.val);
	packet[TOCLIENT_ADD_PARTICLESPAWNER_MINEXPTIME].convert(p->exptime.start.min.val);
	packet[TOCLIENT_ADD_PARTICLESPAWNER_MAXEXPTIME].convert(p->exptime.start.max.val);
	packet[TOCLIENT_ADD_PARTICLESPAWNER_MINSIZE].convert(p->size.start.min.val);
	packet[TOCLIENT_ADD_PARTICLESPAWNER_MAXSIZE].convert(p->size.start.max.val);
	packet[TOCLIENT_ADD_PARTICLESPAWNER_COLLISIONDETECTION].convert(
			p->collisiondetection);
	packet[TOCLIENT_ADD_PARTICLESPAWNER_TEXTURE].convert(p->texture.string);
	packet[TOCLIENT_ADD_PARTICLESPAWNER_VERTICAL].convert(p->vertical);
	packet_convert_safe(
			packet, TOCLIENT_ADD_PARTICLESPAWNER_COLLISION_REMOVAL, p->collision_removal);

	p->pos.end = p->pos.start;
	p->vel.end = p->vel.start;
	p->acc.end = p->acc.start;
	p->exptime.end = p->exptime.start;
	p->size.end = p->size.start;

	packet_convert_safe(packet, TOCLIENT_ADD_PARTICLESPAWNER_GLOW, p->glow);
	packet_convert_safe(
			packet, TOCLIENT_ADD_PARTICLESPAWNER_OBJECT_COLLISION, p->object_collision);
	packet_convert_safe(packet, TOCLIENT_ADD_PARTICLESPAWNER_NODE, p->node);
	packet_convert_safe(packet, TOCLIENT_ADD_PARTICLESPAWNER_NODE_TILE, p->node_tile);
	std::string animation;
	if (packet_convert_safe(packet, TOCLIENT_ADD_PARTICLESPAWNER_ANIMATION, animation)) {
		std::istringstream stream(animation, std::ios_base::binary);
		p->animation.deSerialize(stream, m_proto_ver);
	}
	std::string data;
	if (packet_convert_safe(packet, TOCLIENT_ADD_PARTICLESPAWNER_DATA, data)) {
		std::istringstream stream(data, std::ios_base::binary);
		p->pos.deSerialize(stream);
		p->vel.deSerialize(stream);
		p->acc.deSerialize(stream);
		p->exptime.deSerialize(stream);
		p->size.deSerialize(stream);
		p->texture.deSerialize(stream, m_proto_ver, true);
		p->drag.deSerialize(stream);
		p->jitter.deSerialize(stream);
		p->bounce.deSerialize(stream);
		ParticleParamTypes::deSerializeParameterValue(stream, p->attractor_kind);
		if (p->attractor_kind != ParticleParamTypes::AttractorKind::none) {
			p->attract.deSerialize(stream);
			p->attractor_origin.deSerialize(stream);
			p->attractor_attachment = readU16(stream);
			p->attractor_kill = readU8(stream);
			if (p->attractor_kind != ParticleParamTypes::AttractorKind::point) {
				p->attractor_direction.deSerialize(stream);
				p->attractor_direction_attachment = readU16(stream);
			}
		}
		p->radius.deSerialize(stream);
		u16 texpool_size;
		ParticleParamTypes::deSerializeParameterValue(stream, texpool_size);
		p->texpool.resize(texpool_size);
		for (auto &texture : p->texpool)
			texture.deSerialize(stream, m_proto_ver);
	}

	u16 attached_id = 0;
	packet_convert_safe(packet, TOCLIENT_ADD_PARTICLESPAWNER_ATTACHED_ID, attached_id);
	const u32 id = packet[TOCLIENT_ADD_PARTICLESPAWNER_ID].as<u32>();
	auto *event = new ClientEvent(CE_ADD_PARTICLESPAWNER);
	event->add_particlespawner.p = p.release();
	event->add_particlespawner.attached_id = attached_id;
	event->add_particlespawner.id = id;
	m_client_event_queue.push(event);
}

void Client::handleCommand_DeleteParticleSpawner(NetworkPacket *pkt)
{
	auto &packet = *(pkt->packet);
	auto *event = new ClientEvent(CE_DELETE_PARTICLESPAWNER);
	event->delete_particlespawner.id =
			packet[TOCLIENT_DELETE_PARTICLESPAWNER_ID].as<u32>();

	m_client_event_queue.push(event);
}

void Client::handleCommand_HudAdd(NetworkPacket *pkt)
{
	auto &packet = *(pkt->packet);
	auto hud = std::make_unique<ClientEventHudAdd>();
	packet[TOCLIENT_HUDADD_ID].convert(hud->server_id);
	packet[TOCLIENT_HUDADD_TYPE].convert(hud->type);
	packet[TOCLIENT_HUDADD_POS].convert(hud->pos);
	packet[TOCLIENT_HUDADD_NAME].convert(hud->name);
	packet[TOCLIENT_HUDADD_SCALE].convert(hud->scale);
	packet[TOCLIENT_HUDADD_TEXT].convert(hud->text);
	packet[TOCLIENT_HUDADD_NUMBER].convert(hud->number);
	packet[TOCLIENT_HUDADD_ITEM].convert(hud->item);
	packet[TOCLIENT_HUDADD_DIR].convert(hud->dir);
	packet[TOCLIENT_HUDADD_ALIGN].convert(hud->align);
	packet[TOCLIENT_HUDADD_OFFSET].convert(hud->offset);
	packet[TOCLIENT_HUDADD_WORLD_POS].convert(hud->world_pos);
	if (m_proto_ver >= 52) {
		packet[TOCLIENT_HUDADD_SIZE].convert(hud->size);
	} else {
		v2s32 old_size;
		packet[TOCLIENT_HUDADD_SIZE].convert(old_size);
		hud->size = v2f::from(old_size);
	}
	packet_convert_safe(packet, TOCLIENT_HUDADD_Z_INDEX, hud->z_index);
	packet_convert_safe(packet, TOCLIENT_HUDADD_TEXT2, hud->text2);
	packet_convert_safe(packet, TOCLIENT_HUDADD_STYLE, hud->style);
	u8 flags = 1;
	packet_convert_safe(packet, TOCLIENT_HUDADD_FLAGS, flags);
	hud->hideable = flags & 1;

	auto *event = new ClientEvent(CE_HUDADD);
	event->hudadd = hud.release();
	m_client_event_queue.push(event);
}

void Client::handleCommand_HudRemove(NetworkPacket *pkt)
{
	auto &packet = *(pkt->packet);
	auto *event = new ClientEvent(CE_HUDRM);
	event->hudrm.id = packet[TOCLIENT_HUDRM_ID].as<u32>();
	m_client_event_queue.push(event);
}

void Client::handleCommand_HudChange(NetworkPacket *pkt)
{
	auto &packet = *(pkt->packet);
	const u32 id = packet[TOCLIENT_HUDCHANGE_ID].as<u32>();
	const u8 stat = packet[TOCLIENT_HUDCHANGE_STAT].as<u8>();
	if (stat >= HudElementStat_END)
		return;

	auto hud = std::make_unique<ClientEventHudChange>();
	hud->id = id;
	hud->stat = static_cast<HudElementStat>(stat);

	switch (hud->stat) {
	case HUD_STAT_POS:
	case HUD_STAT_SCALE:
	case HUD_STAT_ALIGN:
	case HUD_STAT_OFFSET:
		packet[TOCLIENT_HUDCHANGE_V2F].convert(hud->v2fdata);
		break;
	case HUD_STAT_NAME:
	case HUD_STAT_TEXT:
	case HUD_STAT_TEXT2:
		packet[TOCLIENT_HUDCHANGE_STRING].convert(hud->sdata);
		break;
	case HUD_STAT_WORLD_POS:
		packet[TOCLIENT_HUDCHANGE_V3F].convert(hud->v3fdata);
		break;
	case HUD_STAT_SIZE:
		if (m_proto_ver >= 52) {
			packet[TOCLIENT_HUDCHANGE_V2F].convert(hud->v2fdata);
		} else {
			packet[TOCLIENT_HUDCHANGE_V2S32].convert(hud->v2s32data);
			hud->v2fdata = v2f::from(hud->v2s32data);
		}
		break;
	default:
		packet[TOCLIENT_HUDCHANGE_U32].convert(hud->data);
		break;
	}

	auto *event = new ClientEvent(CE_HUDCHANGE);
	event->hudchange = hud.release();
	m_client_event_queue.push(event);
}

void Client::handleCommand_HudSetFlags(NetworkPacket *pkt)
{
	auto &packet = *(pkt->packet);
	LocalPlayer *player = m_env.getLocalPlayer();
	if (!player)
		return;

	u32 flags = packet[TOCLIENT_HUD_SET_FLAGS_FLAGS].as<u32>();
	u32 mask = packet[TOCLIENT_HUD_SET_FLAGS_MASK].as<u32>();
	const bool was_minimap_radar_visible =
			player->hud_flags & HUD_FLAG_MINIMAP_RADAR_VISIBLE;

	player->hud_flags &= ~mask;
	player->hud_flags |= flags;

	const bool radar_disabled = !(player->hud_flags & HUD_FLAG_MINIMAP_RADAR_VISIBLE);
	if (m_minimap && radar_disabled && was_minimap_radar_visible) {
		while (m_minimap->getModeIndex() > 0 &&
				m_minimap->getModeDef().type == MINIMAP_TYPE_RADAR)
			m_minimap->nextMode();
	}
}

void Client::handleCommand_HudSetParam(NetworkPacket *pkt)
{
	auto &packet = *(pkt->packet);
	auto player = m_env.getLocalPlayer();
	if (!player)
		return;
	u16 param = packet[TOCLIENT_HUD_SET_PARAM_ID].as<u16>();
	std::string value = packet[TOCLIENT_HUD_SET_PARAM_VALUE].as<std::string>();
	if (param == HUD_PARAM_HOTBAR_ITEMCOUNT && value.size() == 4) {
		s32 hotbar_itemcount = readS32(reinterpret_cast<const u8 *>(value.data()));
		if (hotbar_itemcount > 0 && hotbar_itemcount <= HUD_HOTBAR_ITEMCOUNT_MAX)
			player->hud_hotbar_itemcount = hotbar_itemcount;
	} else if (param == HUD_PARAM_HOTBAR_IMAGE) {
		player->hotbar_image = value;
	} else if (param == HUD_PARAM_HOTBAR_IMAGE_ITEMS) {
		try {
			player->hotbar_image_items = stoi(value);
		} catch (const std::exception &) {
			warningstream << "Invalid hotbar image item count: " << value << std::endl;
		}
	} else if (param == HUD_PARAM_HOTBAR_SELECTED_IMAGE) {
		player->hotbar_selected_image = value;
	}
}

void Client::handleCommand_HudSetSky(NetworkPacket *pkt)
{
	auto &packet = *(pkt->packet);
	SkyboxParams skybox = SkyboxDefaults::getSkyDefaults();
	packet[TOCLIENT_SET_SKY_COLOR].convert(skybox.bgcolor);
	packet[TOCLIENT_SET_SKY_TYPE].convert(skybox.type);
	packet[TOCLIENT_SET_SKY_PARAMS].convert(skybox.textures);
	packet_convert_safe(packet, TOCLIENT_SET_SKY_CLOUDS, skybox.clouds);
	packet_convert_safe(packet, TOCLIENT_SET_SKY_DAY_SKY, skybox.sky_color.day_sky);
	packet_convert_safe(
			packet, TOCLIENT_SET_SKY_DAY_HORIZON, skybox.sky_color.day_horizon);
	packet_convert_safe(packet, TOCLIENT_SET_SKY_DAWN_SKY, skybox.sky_color.dawn_sky);
	packet_convert_safe(
			packet, TOCLIENT_SET_SKY_DAWN_HORIZON, skybox.sky_color.dawn_horizon);
	packet_convert_safe(packet, TOCLIENT_SET_SKY_NIGHT_SKY, skybox.sky_color.night_sky);
	packet_convert_safe(
			packet, TOCLIENT_SET_SKY_NIGHT_HORIZON, skybox.sky_color.night_horizon);
	packet_convert_safe(packet, TOCLIENT_SET_SKY_INDOORS, skybox.sky_color.indoors);
	packet_convert_safe(packet, TOCLIENT_SET_SKY_FOG_SUN_TINT, skybox.fog_sun_tint);
	packet_convert_safe(packet, TOCLIENT_SET_SKY_FOG_MOON_TINT, skybox.fog_moon_tint);
	packet_convert_safe(packet, TOCLIENT_SET_SKY_FOG_TINT_TYPE, skybox.fog_tint_type);
	packet_convert_safe(packet, TOCLIENT_SET_SKY_BODY_ORBIT_TILT, skybox.body_orbit_tilt);
	packet_convert_safe(packet, TOCLIENT_SET_SKY_FOG_DISTANCE, skybox.fog_distance);
	packet_convert_safe(packet, TOCLIENT_SET_SKY_FOG_START, skybox.fog_start);
	packet_convert_safe(packet, TOCLIENT_SET_SKY_FOG_COLOR, skybox.fog_color);
	packet_convert_safe(packet, TOCLIENT_SET_SKY_AUTO_DIM, skybox.auto_dim_skybox);

	if (skybox.type == "skybox" && skybox.textures.size() != 6) {
		warningstream << "Ignoring skybox packet with " << skybox.textures.size()
					  << " textures (expected 6)" << std::endl;
		return;
	}
	if (skybox.type == "plain")
		skybox.clouds = false;

	auto *event = new ClientEvent(CE_SET_SKY);
	event->set_sky = new SkyboxParams(skybox);
	m_client_event_queue.push(event);

	if (skybox.type != "regular") {
		SunParams sun = SkyboxDefaults::getSunDefaults();
		MoonParams moon = SkyboxDefaults::getMoonDefaults();
		StarParams stars = SkyboxDefaults::getStarDefaults();
		sun.visible = sun.sunrise_visible = moon.visible = stars.visible = false;

		auto *sun_event = new ClientEvent(CE_SET_SUN);
		sun_event->sun_params = new SunParams(sun);
		m_client_event_queue.push(sun_event);
		auto *moon_event = new ClientEvent(CE_SET_MOON);
		moon_event->moon_params = new MoonParams(moon);
		m_client_event_queue.push(moon_event);
		auto *star_event = new ClientEvent(CE_SET_STARS);
		star_event->star_params = new StarParams(stars);
		m_client_event_queue.push(star_event);
	}
}

void Client::handleCommand_OverrideDayNightRatio(NetworkPacket *pkt)
{
	auto &packet = *(pkt->packet);
	bool do_override;
	float day_night_ratio_f;
	packet[TOCLIENT_OVERRIDE_DAY_NIGHT_RATIO_DO].convert(do_override);
	packet[TOCLIENT_OVERRIDE_DAY_NIGHT_RATIO_VALUE].convert(day_night_ratio_f);

	auto *event = new ClientEvent(CE_OVERRIDE_DAY_NIGHT_RATIO);
	event->override_day_night_ratio.do_override = do_override;
	event->override_day_night_ratio.ratio_f = day_night_ratio_f;
	m_client_event_queue.push(event);
}

void Client::handleCommand_LocalPlayerAnimations(NetworkPacket *pkt)
{
	auto &packet = *(pkt->packet);
	LocalPlayer *player = m_env.getLocalPlayer();
	if (!player)
		return;

	constexpr int fields[4] = {
			TOCLIENT_LOCAL_PLAYER_ANIMATIONS_IDLE,
			TOCLIENT_LOCAL_PLAYER_ANIMATIONS_WALK,
			TOCLIENT_LOCAL_PLAYER_ANIMATIONS_DIG,
			TOCLIENT_LOCAL_PLAYER_ANIMATIONS_WALKDIG,
	};
	for (int i = 0; i < 4; ++i) {
		if (m_proto_ver >= 46) {
			packet[fields[i]].convert(player->local_animations[i]);
		} else {
			v2s32 frames;
			packet[fields[i]].convert(frames);
			player->local_animations[i] = v2f::from(frames);
		}
	}
	packet[TOCLIENT_LOCAL_PLAYER_ANIMATIONS_FRAME_SPEED].convert(
			player->local_animation_speed);
	player->last_animation = LocalPlayerAnimation::NO_ANIM;
}

void Client::handleCommand_EyeOffset(NetworkPacket *pkt)
{
	auto &packet = *(pkt->packet);
	LocalPlayer *player = m_env.getLocalPlayer();
	if (!player)
		return;

	packet[TOCLIENT_EYE_OFFSET_FIRST].convert(player->eye_offset_first);
	packet[TOCLIENT_EYE_OFFSET_THIRD].convert(player->eye_offset_third);
	if (!packet_convert_safe(
				packet, TOCLIENT_EYE_OFFSET_THIRD_FRONT, player->eye_offset_third_front))
		player->eye_offset_third_front = player->eye_offset_third;
}

void Client::handleCommand_ModChannelMsg(NetworkPacket *pkt)
{
	std::string channel_name, sender, channel_msg;
	auto &packet = *(pkt->packet);
	packet[TOCLIENT_MODCHANNEL_MSG_CHANNEL].convert(channel_name);
	packet[TOCLIENT_MODCHANNEL_MSG_SENDER].convert(sender);
	packet[TOCLIENT_MODCHANNEL_MSG_MESSAGE].convert(channel_msg);

	verbosestream << "Mod channel message received from server " << pkt->getPeerId()
				  << " on channel " << channel_name << ". sender: `" << sender
				  << "`, message: " << channel_msg << std::endl;

	if (!m_modchannel_mgr->channelRegistered(channel_name)) {
		verbosestream << "Server sent us messages on unregistered channel "
					  << channel_name << ", ignoring." << std::endl;
		return;
	}

	m_script->on_modchannel_message(channel_name, sender, channel_msg);
}

void Client::handleCommand_ModChannelSignal(NetworkPacket *pkt)
{
	u8 signal_tmp = 0;
	ModChannelSignal signal;
	std::string channel;

	auto &packet = *(pkt->packet);
	packet[TOCLIENT_MODCHANNEL_SIGNAL_SIGNAL].convert(signal_tmp);
	packet[TOCLIENT_MODCHANNEL_SIGNAL_CHANNEL].convert(channel);

	signal = (ModChannelSignal)signal_tmp;

	bool valid_signal = true;
	// @TODO: send Signal to Lua API
	switch (signal) {
	case MODCHANNEL_SIGNAL_JOIN_OK:
		m_modchannel_mgr->setChannelState(channel, MODCHANNEL_STATE_READ_WRITE);
		infostream << "Server ack our mod channel join on channel `" << channel
				   << "`, joining." << std::endl;
		break;
	case MODCHANNEL_SIGNAL_JOIN_FAILURE:
		// Unable to join, remove channel
		m_modchannel_mgr->leaveChannel(channel, 0);
		infostream << "Server refused our mod channel join on channel `" << channel << "`"
				   << std::endl;
		break;
	case MODCHANNEL_SIGNAL_LEAVE_OK:
#ifndef NDEBUG
		infostream << "Server ack our mod channel leave on channel " << channel
				   << "`, leaving." << std::endl;
#endif
		break;
	case MODCHANNEL_SIGNAL_LEAVE_FAILURE:
		infostream << "Server refused our mod channel leave on channel `" << channel
				   << "`" << std::endl;
		break;
	case MODCHANNEL_SIGNAL_CHANNEL_NOT_REGISTERED:
#ifndef NDEBUG
		// Generally unused, but ensure we don't do an implementation error
		infostream << "Server tells us we sent a message on channel `" << channel
				   << "` but we are not registered. Message was dropped." << std::endl;
#endif
		break;
	case MODCHANNEL_SIGNAL_SET_STATE: {
		u8 state;
		state = static_cast<u8>(packet[TOCLIENT_MODCHANNEL_SIGNAL_STATE].as<u32>());

		if (state == MODCHANNEL_STATE_INIT || state >= MODCHANNEL_STATE_MAX) {
			infostream << "Received wrong channel state " << state << ", ignoring."
					   << std::endl;
			return;
		}

		m_modchannel_mgr->setChannelState(channel, (ModChannelState)state);
		infostream << "Server sets mod channel `" << channel << "` in read-only mode."
				   << std::endl;
		break;
	}
	default:
#ifndef NDEBUG
		warningstream << "Received unhandled mod channel signal ID " << signal
					  << ", ignoring." << std::endl;
#endif
		valid_signal = false;
		break;
	}

	// If signal is valid, forward it to client side mods
	if (valid_signal)
		m_script->on_modchannel_signal(channel, signal);
}

void Client::handleCommand_UpdatePlayerList(NetworkPacket *pkt)
{
	auto &packet = *(pkt->packet);
	const auto type = static_cast<PlayerListModifer>(
			packet[TOCLIENT_UPDATE_PLAYER_LIST_TYPE].as<u8>());
	std::vector<std::string> names;
	packet[TOCLIENT_UPDATE_PLAYER_LIST_NAMES].convert(names);
	for (const auto &name : names) {
		if (type == PLAYER_LIST_INIT || type == PLAYER_LIST_ADD)
			m_env.addPlayerName(name);
		else if (type == PLAYER_LIST_REMOVE)
			m_env.removePlayerName(name);
	}
}

void Client::handleCommand_SrpBytesSandB(NetworkPacket *pkt)
{
	if (m_chosen_auth_mech != AUTH_MECHANISM_SRP &&
			m_chosen_auth_mech != AUTH_MECHANISM_LEGACY_PASSWORD) {
		errorstream << "Client: Received SRP S_B login message,"
					<< " but wasn't supposed to (chosen_mech=" << m_chosen_auth_mech
					<< ")." << std::endl;
		return;
	}

	char *bytes_M = 0;
	size_t len_M = 0;
	SRPUser *usr = (SRPUser *)m_auth_data;
	std::string s;
	std::string B;
	auto &packet = *(pkt->packet);
	packet[TOCLIENT_SRP_BYTES_S_B_SALT].convert(s);
	packet[TOCLIENT_SRP_BYTES_S_B_BYTES].convert(B);

	infostream << "Client: Received TOCLIENT_SRP_BYTES_S_B." << std::endl;

	srp_user_process_challenge(usr, (const unsigned char *)s.c_str(), s.size(),
			(const unsigned char *)B.c_str(), B.size(), (unsigned char **)&bytes_M,
			&len_M);

	if (!bytes_M) {
		errorstream << "Client: SRP-6a S_B safety check violation!" << std::endl;
		return;
	}

	MSGPACK_PACKET_INIT((int)TOSERVER_SRP_BYTES_M, 1);
	PACK(TOSERVER_SRP_BYTES_M_BYTES, std::string(bytes_M, len_M));
	Send(1, buffer, true);
}

void Client::handleCommand_Fov(NetworkPacket *pkt)
{
	PlayerFovSpec fov;
	auto &packet = *(pkt->packet);
	packet[TOCLIENT_FOV_FOV].convert(fov.fov);
	packet_convert_safe(packet, TOCLIENT_FOV_IS_MULTIPLIER, fov.is_multiplier);
	packet_convert_safe(packet, TOCLIENT_FOV_TRANSITION_TIME, fov.transition_time);
	LocalPlayer *player = m_env.getLocalPlayer();
	if (!player)
		return;
	player->setFov(fov);
	if (m_camera)
		m_camera->notifyFovChange();
}

void Client::handleCommand_MovePlayerRel(NetworkPacket *pkt)
{
	LocalPlayer *player = m_env.getLocalPlayer();
	if (!player)
		return;
	player->addPosition(pkt->packet->at(TOCLIENT_MOVE_PLAYER_REL_POS).as<v3f>());
}

void Client::handleCommand_PlayerSpeed(NetworkPacket *pkt)
{
	LocalPlayer *player = m_env.getLocalPlayer();
	if (!player)
		return;
	player->addVelocity(pkt->packet->at(TOCLIENT_PLAYER_SPEED_ADDED_VELOCITY).as<v3f>());
}

void Client::handleCommand_Camera(NetworkPacket *pkt)
{
	LocalPlayer *player = m_env.getLocalPlayer();
	if (!player)
		return;
	player->allowed_camera_mode = static_cast<CameraMode>(
			pkt->packet->at(TOCLIENT_CAMERA_ALLOWED_MODE).as<u8>());
	if (player->allowed_camera_mode >= CameraMode_END)
		player->allowed_camera_mode = CAMERA_MODE_ANY;
	m_client_event_queue.push(new ClientEvent(CE_UPDATE_CAMERA));
}

void Client::handleCommand_FormspecPrepend(NetworkPacket *pkt)
{
	LocalPlayer *player = m_env.getLocalPlayer();
	if (!player)
		return;
	player->formspec_prepend =
			pkt->packet->at(TOCLIENT_FORMSPEC_PREPEND_DATA).as<std::string>();
}

void Client::handleCommand_CSMRestrictionFlags(NetworkPacket *pkt)
{
	auto &packet = *(pkt->packet);
	packet[TOCLIENT_CSM_RESTRICTION_FLAGS_FLAGS].convert(m_csm_restriction_flags);
	packet[TOCLIENT_CSM_RESTRICTION_FLAGS_NODERANGE].convert(m_csm_restriction_noderange);
	loadMods();
}

void Client::handleCommand_FadeSound(NetworkPacket *pkt)
{
	auto &packet = *(pkt->packet);
	const s32 server_id = packet[TOCLIENT_FADE_SOUND_ID].as<s32>();
	f32 step = 0.0f;
	f32 gain = 0.0f;
	packet[TOCLIENT_FADE_SOUND_STEP].convert(step);
	packet[TOCLIENT_FADE_SOUND_GAIN].convert(gain);
	auto it = m_sounds_server_to_client.find(server_id);
	if (it != m_sounds_server_to_client.end())
		m_sound->fadeSound(it->second, step, gain);
}

void Client::handleCommand_HudSetSun(NetworkPacket *pkt)
{
	auto &packet = *(pkt->packet);
	SunParams sun = SkyboxDefaults::getSunDefaults();
	packet[TOCLIENT_SET_SUN_VISIBLE].convert(sun.visible);
	packet[TOCLIENT_SET_SUN_TEXTURE].convert(sun.texture);
	packet[TOCLIENT_SET_SUN_TONEMAP].convert(sun.tonemap);
	packet[TOCLIENT_SET_SUN_SUNRISE].convert(sun.sunrise);
	packet[TOCLIENT_SET_SUN_SUNRISE_VISIBLE].convert(sun.sunrise_visible);
	packet[TOCLIENT_SET_SUN_SCALE].convert(sun.scale);
	auto *event = new ClientEvent(CE_SET_SUN);
	event->sun_params = new SunParams(sun);
	m_client_event_queue.push(event);
}

void Client::handleCommand_HudSetMoon(NetworkPacket *pkt)
{
	auto &packet = *(pkt->packet);
	MoonParams moon = SkyboxDefaults::getMoonDefaults();
	packet[TOCLIENT_SET_MOON_VISIBLE].convert(moon.visible);
	packet[TOCLIENT_SET_MOON_TEXTURE].convert(moon.texture);
	packet[TOCLIENT_SET_MOON_TONEMAP].convert(moon.tonemap);
	packet[TOCLIENT_SET_MOON_SCALE].convert(moon.scale);
	auto *event = new ClientEvent(CE_SET_MOON);
	event->moon_params = new MoonParams(moon);
	m_client_event_queue.push(event);
}

void Client::handleCommand_HudSetStars(NetworkPacket *pkt)
{
	auto &packet = *(pkt->packet);
	StarParams stars = SkyboxDefaults::getStarDefaults();
	packet[TOCLIENT_SET_STARS_VISIBLE].convert(stars.visible);
	packet[TOCLIENT_SET_STARS_COUNT].convert(stars.count);
	packet[TOCLIENT_SET_STARS_COLOR].convert(stars.starcolor);
	packet[TOCLIENT_SET_STARS_SCALE].convert(stars.scale);
	packet_convert_safe(packet, TOCLIENT_SET_STARS_DAY_OPACITY, stars.day_opacity);
	packet_convert_safe(packet, TOCLIENT_SET_STARS_SEED, stars.star_seed);
	auto *event = new ClientEvent(CE_SET_STARS);
	event->star_params = new StarParams(stars);
	m_client_event_queue.push(event);
}

void Client::handleCommand_CloudParams(NetworkPacket *pkt)
{
	auto &packet = *(pkt->packet);
	CloudParams clouds = SkyboxDefaults::getCloudDefaults();
	packet[TOCLIENT_CLOUD_PARAMS_DENSITY].convert(clouds.density);
	packet[TOCLIENT_CLOUD_PARAMS_COLOR_BRIGHT].convert(clouds.color_bright);
	packet[TOCLIENT_CLOUD_PARAMS_COLOR_AMBIENT].convert(clouds.color_ambient);
	packet[TOCLIENT_CLOUD_PARAMS_HEIGHT].convert(clouds.height);
	packet[TOCLIENT_CLOUD_PARAMS_THICKNESS].convert(clouds.thickness);
	packet[TOCLIENT_CLOUD_PARAMS_SPEED].convert(clouds.speed);
	packet_convert_safe(packet, TOCLIENT_CLOUD_PARAMS_COLOR_SHADOW, clouds.color_shadow);
	auto *event = new ClientEvent(CE_CLOUD_PARAMS);
	event->cloud_params.density = clouds.density;
	event->cloud_params.color_bright = clouds.color_bright.color;
	event->cloud_params.color_ambient = clouds.color_ambient.color;
	event->cloud_params.color_shadow = clouds.color_shadow.color;
	event->cloud_params.height = clouds.height;
	event->cloud_params.thickness = clouds.thickness;
	event->cloud_params.speed_x = clouds.speed.X;
	event->cloud_params.speed_y = clouds.speed.Y;
	m_client_event_queue.push(event);
}

void Client::handleCommand_SetLighting(NetworkPacket *pkt)
{
	LocalPlayer *player = m_env.getLocalPlayer();
	if (!player)
		return;
	auto &packet = *(pkt->packet);
	Lighting &lighting = player->getLighting();
	packet[TOCLIENT_SET_LIGHTING_SHADOW_INTENSITY].convert(lighting.shadow_intensity);
	packet_convert_safe(packet, TOCLIENT_SET_LIGHTING_SATURATION, lighting.saturation);
	packet_convert_safe(
			packet, TOCLIENT_SET_LIGHTING_LUMINANCE_MIN, lighting.exposure.luminance_min);
	packet_convert_safe(
			packet, TOCLIENT_SET_LIGHTING_LUMINANCE_MAX, lighting.exposure.luminance_max);
	packet_convert_safe(packet, TOCLIENT_SET_LIGHTING_EXPOSURE_CORRECTION,
			lighting.exposure.exposure_correction);
	packet_convert_safe(packet, TOCLIENT_SET_LIGHTING_SPEED_DARK_BRIGHT,
			lighting.exposure.speed_dark_bright);
	packet_convert_safe(packet, TOCLIENT_SET_LIGHTING_SPEED_BRIGHT_DARK,
			lighting.exposure.speed_bright_dark);
	packet_convert_safe(packet, TOCLIENT_SET_LIGHTING_CENTER_WEIGHT_POWER,
			lighting.exposure.center_weight_power);
	packet_convert_safe(packet, TOCLIENT_SET_LIGHTING_VOLUMETRIC_STRENGTH,
			lighting.volumetric_light_strength);
	packet_convert_safe(packet, TOCLIENT_SET_LIGHTING_SHADOW_TINT, lighting.shadow_tint);
	packet_convert_safe(
			packet, TOCLIENT_SET_LIGHTING_BLOOM_INTENSITY, lighting.bloom_intensity);
	packet_convert_safe(packet, TOCLIENT_SET_LIGHTING_BLOOM_STRENGTH_FACTOR,
			lighting.bloom_strength_factor);
	packet_convert_safe(
			packet, TOCLIENT_SET_LIGHTING_BLOOM_RADIUS, lighting.bloom_radius);
	packet_convert_safe(
			packet, TOCLIENT_SET_LIGHTING_SHADOW_DIRECTION, lighting.shadow_direction);
}

void Client::handleCommand_MinimapModes(NetworkPacket *pkt)
{
	auto &packet = *(pkt->packet);
	std::vector<std::tuple<u16, std::string, u16, std::string, u16>> modes;
	packet[TOCLIENT_MINIMAP_MODES_LIST].convert(modes);
	const u16 wanted_mode = packet[TOCLIENT_MINIMAP_MODES_SELECTED].as<u16>();
	if (m_minimap) {
		m_minimap->clearModes();
		for (const auto &[type, label, size, texture, scale] : modes)
			m_minimap->addMode(MinimapType(type), size, label, texture, scale);
		m_minimap->setModeIndex(wanted_mode);
	}
}

void Client::handleCommand_NodemetaChanged(NetworkPacket *pkt)
{
	const std::string data =
			pkt->packet->at(TOCLIENT_NODEMETA_CHANGED_DATA).as<std::string>();
	std::istringstream compressed(data, std::ios_base::binary);
	std::ostringstream decompressed(std::ios_base::binary);
	decompressZlib(compressed, decompressed);
	std::istringstream stream(decompressed.str(), std::ios_base::binary);
	NodeMetadataList meta_updates_list(false);
	meta_updates_list.deSerialize(stream, m_itemdef, true);
	Map &map = m_env.getMap();
	for (auto it = meta_updates_list.begin(); it != meta_updates_list.end(); ++it) {
		const v3s16 pos = it->first;
		if (map.isValidPosition(pos) && map.setNodeMetadata(pos, it->second))
			continue;
		delete it->second;
	}
}

void Client::handleCommand_BlockData(NetworkPacket *pkt)
{
	auto &packet = *(pkt->packet);
	if (!packet.contains(TOCLIENT_BLOCKDATA_POS) ||
			!packet.contains(TOCLIENT_BLOCKDATA_DATA)) {
		warningstream << "Client: incomplete block data packet" << std::endl;
		return;
	}
	const v3bpos_t pos = packet[TOCLIENT_BLOCKDATA_POS].as<v3bpos_t>();
	const std::string data = packet[TOCLIENT_BLOCKDATA_DATA].as<std::string>();
	std::istringstream stream(data, std::ios_base::binary);
	Map *map = &m_env.getMap();
	MapBlockPtr block = map->getBlock(pos);
	if (!block) {
		block = map->createBlankBlockNoInsert(pos);
		if (!block->deSerialize(stream, m_server_ser_ver, false))
			return;
		block->deSerializeNetworkSpecific(stream);
		map->insertBlock(block);
		++m_new_meshes;
	} else {
		if (!block->deSerialize(stream, m_server_ser_ver, false))
			return;
		block->deSerializeNetworkSpecific(stream);
	}

	if (m_localdb && !m_simple_singleplayer_mode) {
		ServerMap::saveBlock(block.get(), m_localdb.get());
		if (!far_container.have_params)
			merger->add_changed(pos);
	}

	updateMeshTimestampWithEdge(pos);
	const auto *player = m_env.getLocalPlayer();
	if (player && !overload &&
			getNodeBlockPos(floatToInt(player->getPosition(), BS)).getDistanceFrom(pos) <=
					1)
		addUpdateMeshTask(pos);
	sendGotBlocks({pos});
}

void Client::handleCommand_SpawnParticleBatch(NetworkPacket *pkt)
{
	const std::string data =
			pkt->packet->at(TOCLIENT_SPAWN_PARTICLE_BATCH_DATA).as<std::string>();
	std::istringstream compressed(data, std::ios_base::binary);
	std::stringstream batch(std::ios::binary | std::ios::in | std::ios::out);
	decompressZstd(compressed, batch);
	while (canRead(batch)) {
		auto particle = std::make_unique<ParticleParameters>();
		std::istringstream stream(deSerializeString32(batch), std::ios_base::binary);
		particle->deSerialize(stream, m_proto_ver);
		auto *event = new ClientEvent(CE_SPAWN_PARTICLE);
		event->spawn_particle = particle.release();
		m_client_event_queue.push(event);
	}
}

void Client::handleCommand_MediaPush(NetworkPacket *pkt)
{
	auto &packet = *(pkt->packet);
	const std::string raw_hash = packet[TOCLIENT_MEDIA_PUSH_HASH].as<std::string>();
	const std::string filename = packet[TOCLIENT_MEDIA_PUSH_FILENAME].as<std::string>();
	const bool cached = packet[TOCLIENT_MEDIA_PUSH_CACHED].as<bool>();
	u32 token = 0;
	packet_convert_safe(packet, TOCLIENT_MEDIA_PUSH_TOKEN, token);
	std::string filedata;
	packet_convert_safe(packet, TOCLIENT_MEDIA_PUSH_DATA, filedata);

	if (raw_hash.size() != 20 || filename.empty() ||
			(m_proto_ver < 40 && filedata.empty()) ||
			!string_allowed(filename, TEXTURENAME_ALLOWED_CHARS))
		throw PacketError("Illegal filename, data or hash");

	verbosestream << "Server pushes media file \"" << filename << "\" ";
	if (filedata.empty())
		verbosestream << "to be fetched ";
	else
		verbosestream << "with " << filedata.size() << " bytes ";
	verbosestream << "(cached=" << cached << ")" << std::endl;

	if (!filedata.empty()) {
		if (raw_hash != hashing::sha1(filedata)) {
			warningstream << "Hash of pushed media data mismatches, ignoring"
						  << std::endl;
			return;
		}
		loadMedia(filedata, filename, true);
		if (cached)
			clientMediaUpdateCache(raw_hash, filedata);
		return;
	}

	auto pending = std::find_if(m_pending_media_downloads.begin(),
			m_pending_media_downloads.end(), [&](const PendingMediaDownload &download) {
				return download.name == filename;
			});
	if (pending != m_pending_media_downloads.end()) {
		pending->tokens.push_back(token);
		return;
	}

	auto downloader = std::make_shared<SingleMediaDownloader>(cached);
	m_pending_media_downloads.emplace_back(token, filename, downloader);
	downloader->addFile(filename, raw_hash);
	for (const auto &baseurl : m_remote_media_servers)
		downloader->addRemoteServer(baseurl);
	downloader->step(this);
}

#endif
