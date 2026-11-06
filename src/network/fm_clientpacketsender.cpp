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
#include <array>
#include <cmath>
#include <ctime>
#include <sstream>
#include <string>
#include <vector>

#include "../util/auth.h"
#include "client/camera.h"
#include "client/client.h"
#include "client/clientmap.h"
#include "client/localplayer.h"
#include "clientdynamicinfo.h"
#include "fm_networkprotocol.h"
#include "network/connection.h"
#include "network/networkpacket.h"
#include "network/serveropcodes.h"
#include "networkprotocol.h"
#include "profiler.h"
#include "util/srp.h"
#include "version.h"

namespace
{
void writePlayerPos(LocalPlayer *player, ClientMap *map,
		msgpack::packer<msgpack::sbuffer> &pk, bool camera_inverted,
		bool interaction = false)
{
	v3s32 position = v3s32::from(player->getPosition() * 100);
	v3s32 speed = v3s32::from(player->getSpeed() * 100);
	v3f legacy_position = player->getPosition();
	v3f legacy_speed = player->getSpeed();
	f32 legacy_pitch = player->getPitch();
	f32 legacy_yaw = player->getYaw();
	s32 pitch = player->getPitch() * 100;
	s32 yaw = player->getYaw() * 100;
	u32 key_pressed = player->control.getKeysPressed();
	// scaled by 80, so that pi can fit into a u8
	u8 fov = std::fmin(255.0f, map->getCameraFov() * 80.0f);
	u8 wanted_range = std::fmin(
			255.0f, std::ceil(map->getServerWantedRange() * (1.0f / MAP_BLOCKSIZE)));

	const auto key = [interaction](
							 int player_field, int interaction_field) -> packet_field_t {
		return interaction ? interaction_field : player_field;
	};
	const packet_field_t position_field =
			key(TOSERVER_PLAYERPOS_POSITION, TOSERVER_INTERACT_POSITION);
	const packet_field_t speed_field =
			key(TOSERVER_PLAYERPOS_SPEED, TOSERVER_INTERACT_SPEED);
	const packet_field_t pitch_field =
			key(TOSERVER_PLAYERPOS_PITCH, TOSERVER_INTERACT_PITCH);
	const packet_field_t yaw_field = key(TOSERVER_PLAYERPOS_YAW, TOSERVER_INTERACT_YAW);
	PACK_PK(pk, position_field, legacy_position);
	PACK_PK(pk, speed_field, legacy_speed);
	PACK_PK(pk, pitch_field, legacy_pitch);
	PACK_PK(pk, yaw_field, legacy_yaw);
	PACK_PK(pk, key(TOSERVER_PLAYERPOS_KEY_PRESSED, TOSERVER_INTERACT_KEY_PRESSED),
			key_pressed);
	PACK_PK(pk, key(TOSERVER_PLAYERPOS_FOV, TOSERVER_INTERACT_FOV), fov);
	PACK_PK(pk, key(TOSERVER_PLAYERPOS_WANTED_RANGE, TOSERVER_INTERACT_WANTED_RANGE),
			wanted_range);
	PACK_PK(pk,
			key(TOSERVER_PLAYERPOS_CAMERA_INVERTED, TOSERVER_INTERACT_CAMERA_INVERTED),
			camera_inverted);
	PACK_PK(pk, key(TOSERVER_PLAYERPOS_MOVEMENT_SPEED, TOSERVER_INTERACT_MOVEMENT_SPEED),
			player->control.movement_speed);
	PACK_PK(pk,
			key(TOSERVER_PLAYERPOS_MOVEMENT_DIRECTION,
					TOSERVER_INTERACT_MOVEMENT_DIRECTION),
			player->control.movement_direction);
	const packet_field_t scaled_position_field =
			key(TOSERVER_PLAYERPOS_POSITION_SCALED, TOSERVER_INTERACT_POSITION_SCALED);
	const packet_field_t scaled_speed_field =
			key(TOSERVER_PLAYERPOS_SPEED_SCALED, TOSERVER_INTERACT_SPEED_SCALED);
	const packet_field_t scaled_pitch_field =
			key(TOSERVER_PLAYERPOS_PITCH_SCALED, TOSERVER_INTERACT_PITCH_SCALED);
	const packet_field_t scaled_yaw_field =
			key(TOSERVER_PLAYERPOS_YAW_SCALED, TOSERVER_INTERACT_YAW_SCALED);
	PACK_PK(pk, scaled_position_field, position);
	PACK_PK(pk, scaled_speed_field, speed);
	PACK_PK(pk, scaled_pitch_field, pitch);
	PACK_PK(pk, scaled_yaw_field, yaw);
}
} // namespace

void Client::request_media(const std::vector<std::string> &file_requests)
{
	MSGPACK_PACKET_INIT((int)TOSERVER_REQUEST_MEDIA, 1);
	PACK(TOSERVER_REQUEST_MEDIA_FILES, file_requests);

	Send(1, buffer, true);
	infostream << "Client: Sending media request list to server (" << file_requests.size()
			   << " files)" << std::endl;
}

void Client::interact(InteractAction action, const PointedThing &pointed)
{
	if (m_state != LC_Ready) {
		errorstream << "Client::interact() canceled (not connected)" << std::endl;
		return;
	}

	LocalPlayer *player = m_env.getLocalPlayer();
	if (!player || !m_camera)
		return;

	MSGPACK_PACKET_INIT((int)TOSERVER_INTERACT, 17);
	PACK(TOSERVER_INTERACT_ACTION, (u8)action);
	PACK(TOSERVER_INTERACT_ITEM, player->getWieldIndex());
	PACK(TOSERVER_INTERACT_POINTED_THING, pointed);
	writePlayerPos(player, &m_env.getClientMap(), pk,
			m_camera->getCameraMode() == CAMERA_MODE_THIRD_FRONT, true);

	Send(0, buffer, true);
}

void Client::deleteAuthData()
{
	if (!m_auth_data)
		return;

	switch (m_chosen_auth_mech) {
	case AUTH_MECHANISM_FIRST_SRP:
		break;
	case AUTH_MECHANISM_SRP:
	case AUTH_MECHANISM_LEGACY_PASSWORD:
		srp_user_delete((SRPUser *)m_auth_data);
		m_auth_data = nullptr;
		break;
	case AUTH_MECHANISM_NONE:
		break;
	}
	m_chosen_auth_mech = AUTH_MECHANISM_NONE;
}

AuthMechanism Client::choseAuthMech(const u32 mechs)
{
	if (mechs & AUTH_MECHANISM_SRP)
		return AUTH_MECHANISM_SRP;
	if (mechs & AUTH_MECHANISM_FIRST_SRP)
		return AUTH_MECHANISM_FIRST_SRP;
	if (mechs & AUTH_MECHANISM_LEGACY_PASSWORD)
		return AUTH_MECHANISM_LEGACY_PASSWORD;
	return AUTH_MECHANISM_NONE;
}

void Client::sendInit(const std::string &playerName)
{
	MSGPACK_PACKET_INIT((int)TOSERVER_INIT, 5);
	PACK(TOSERVER_INIT_FMT, SER_FMT_VER_HIGHEST_READ);
	PACK(TOSERVER_INIT_COMPRESSION, (u16)0);
	PACK(TOSERVER_INIT_PROTOCOL_VERSION_MIN, CLIENT_PROTOCOL_VERSION_MIN);
	PACK(TOSERVER_INIT_PROTOCOL_VERSION_MAX, LATEST_PROTOCOL_VERSION);
	PACK(TOSERVER_INIT_NAME, playerName);

	Send(1, buffer, false);
}

void Client::startAuth(AuthMechanism chosen_auth_mechanism)
{
	m_chosen_auth_mech = chosen_auth_mechanism;
	const std::string playername = m_env.getLocalPlayer()->getName();

	switch (chosen_auth_mechanism) {
	case AUTH_MECHANISM_FIRST_SRP: {
		std::string verifier;
		std::string salt;
		generate_srp_verifier_and_salt(playername, m_password, &verifier, &salt);
		MSGPACK_PACKET_INIT((int)TOSERVER_FIRST_SRP, 3);
		PACK(TOSERVER_FIRST_SRP_SALT, salt);
		PACK(TOSERVER_FIRST_SRP_VERIFIER, verifier);
		PACK(TOSERVER_FIRST_SRP_PASSWORD_EMPTY, (u8)m_password.empty());
		Send(1, buffer, true);
		break;
	}
	case AUTH_MECHANISM_SRP:
	case AUTH_MECHANISM_LEGACY_PASSWORD: {
		u8 based_on = 1;
		if (chosen_auth_mechanism == AUTH_MECHANISM_LEGACY_PASSWORD) {
			m_password = translate_password(playername, m_password);
			based_on = 0;
		}

		const std::string playername_u = lowercase(playername);
		m_auth_data = srp_user_new(SRP_SHA256, SRP_NG_2048, playername.c_str(),
				playername_u.c_str(), (const unsigned char *)m_password.c_str(),
				m_password.length(), nullptr, nullptr);
		char *bytes_A = nullptr;
		size_t len_A = 0;
		SRP_Result res = srp_user_start_authentication((SRPUser *)m_auth_data, nullptr,
				nullptr, 0, (unsigned char **)&bytes_A, &len_A);
		FATAL_ERROR_IF(res != SRP_OK, "Creating local SRP user failed.");
		MSGPACK_PACKET_INIT((int)TOSERVER_SRP_BYTES_A, 2);
		PACK(TOSERVER_SRP_BYTES_A_BYTES, std::string(bytes_A, len_A));
		PACK(TOSERVER_SRP_BYTES_A_BASED_ON, based_on);
		Send(1, buffer, true);
		break;
	}
	case AUTH_MECHANISM_NONE:
		break;
	}
}

void Client::sendDeletedBlocks(std::vector<v3s16> &blocks)
{
	MSGPACK_PACKET_INIT((int)TOSERVER_DELETEDBLOCKS, 1);
	PACK(TOSERVER_DELETEDBLOCKS_DATA, blocks);
	Send(2, buffer, true);
}

void Client::sendGotBlocks(const std::vector<v3s16> &blocks)
{
	MSGPACK_PACKET_INIT((int)TOSERVER_GOTBLOCKS, 1);
	PACK(TOSERVER_DELETEDBLOCKS_DATA, blocks);
	Send(2, buffer, true);
}

void Client::sendRemovedSounds(const std::vector<s32> &soundList)
{
	MSGPACK_PACKET_INIT((int)TOSERVER_REMOVED_SOUNDS, 1);
	PACK(TOSERVER_REMOVED_SOUNDS_IDS, soundList);
	Send(2, buffer, true);
}
void Client::sendNodemetaFields(
		v3s16 p, const std::string &formname, const StringMap &fields)
{
	FATAL_ERROR_IF(fields.size() > 0xffff, "Unsupported number of nodemeta fields");
	MSGPACK_PACKET_INIT((int)TOSERVER_NODEMETA_FIELDS, 3);
	PACK(TOSERVER_NODEMETA_FIELDS_POS, p);
	PACK(TOSERVER_NODEMETA_FIELDS_FORMNAME, formname);
	PACK(TOSERVER_NODEMETA_FIELDS_DATA, fields);
	Send(0, buffer, true);
}

void Client::sendInventoryFields(const std::string &formname, const StringMap &fields)
{
	FATAL_ERROR_IF(fields.size() > 0xffff, "Unsupported number of inventory fields");
	MSGPACK_PACKET_INIT((int)TOSERVER_INVENTORY_FIELDS, 2);
	PACK(TOSERVER_INVENTORY_FIELDS_FORMNAME, formname);
	PACK(TOSERVER_INVENTORY_FIELDS_DATA, fields);
	Send(0, buffer, true);
}

void Client::sendInventoryAction(InventoryAction *a)
{
	std::ostringstream os(std::ios_base::binary);
	a->serialize(os);
	MSGPACK_PACKET_INIT((int)TOSERVER_INVENTORY_ACTION, 1);
	PACK(TOSERVER_INVENTORY_ACTION_DATA, os.str());
	Send(0, buffer, true);
}

bool Client::canSendChatMessage() const
{
	const u32 now = time(nullptr);
	const float time_passed = now - m_last_chat_message_sent;
	const float allowance = m_chat_message_allowance +
							time_passed * (CLIENT_CHAT_MESSAGE_LIMIT_PER_10S / 8.0f);
	return allowance >= 1.0f;
}

void Client::sendChatMessage(const std::wstring &message)
{
	const s16 max_queue_size = g_settings->getS16("max_out_chat_queue_size");
	if (canSendChatMessage()) {
		const u32 now = time(nullptr);
		const float time_passed = now - m_last_chat_message_sent;
		m_last_chat_message_sent = now;
		m_chat_message_allowance +=
				time_passed * (CLIENT_CHAT_MESSAGE_LIMIT_PER_10S / 8.0f);
		if (m_chat_message_allowance > CLIENT_CHAT_MESSAGE_LIMIT_PER_10S)
			m_chat_message_allowance = CLIENT_CHAT_MESSAGE_LIMIT_PER_10S;
		m_chat_message_allowance -= 1.0f;

		MSGPACK_PACKET_INIT((int)TOSERVER_CHAT_MESSAGE, 1);
		PACK(TOSERVER_CHAT_MESSAGE_DATA, wide_to_utf8(message));
		Send(0, buffer, true);
	} else if (m_out_chat_queue.size() < (u16)max_queue_size || max_queue_size < 0) {
		m_out_chat_queue.push(message);
	} else {
		infostream << "Could not queue chat message because maximum out chat queue size ("
				   << max_queue_size << ") is reached." << std::endl;
	}
}

void Client::sendChatMessage(const std::string &message)
{
	sendChatMessage(utf8_to_wide(message));
}

void Client::clearOutChatQueue()
{
	m_out_chat_queue = decltype(m_out_chat_queue)();
}

void Client::sendChangePassword(
		const std::string &oldpassword, const std::string &newpassword)
{
	LocalPlayer *player = m_env.getLocalPlayer();
	if (!player)
		return;

	// Get into sudo mode and then send the new password to the server.
	m_password = oldpassword;
	m_new_password = newpassword;
	startAuth(choseAuthMech(m_sudo_auth_methods));
}

void Client::sendDamage(u16 damage)
{
	MSGPACK_PACKET_INIT((int)TOSERVER_DAMAGE, 1);
	PACK(TOSERVER_DAMAGE_VALUE, damage);
	Send(0, buffer, true);
}

void Client::Send(u16 channelnum, const msgpack::sbuffer &data, bool reliable)
{
	g_profiler->add("Client::Send", 1);
	NetworkPacket packet;
	packet.putRawPacket(
			reinterpret_cast<const u8 *>(data.data()), data.size(), PEER_ID_SERVER);
	m_con->Send(PEER_ID_SERVER, channelnum, &packet, reliable);
}

void Client::sendRespawnLegacy()
{
	MSGPACK_PACKET_INIT((int)TOSERVER_RESPAWN_LEGACY, 0);
	Send(0, buffer, true);
}

void Client::sendReady()
{
	MSGPACK_PACKET_INIT((int)TOSERVER_CLIENT_READY, 7);
	PACK(TOSERVER_CLIENT_READY_VERSION_MAJOR, VERSION_MAJOR);
	PACK(TOSERVER_CLIENT_READY_VERSION_MINOR, VERSION_MINOR);
	PACK(TOSERVER_CLIENT_READY_VERSION_PATCH, VERSION_PATCH);
	PACK(TOSERVER_CLIENT_READY_LEGACY_STRING, "");
	PACK(TOSERVER_CLIENT_READY_VERSION_TWEAK, VERSION_TWEAK);
	PACK(TOSERVER_CLIENT_READY_VERSION_STRING, std::string(g_version_hash));
	PACK(TOSERVER_CLIENT_READY_FORMSPEC_VERSION, FORMSPEC_API_VERSION);
	Send(1, buffer, true);
}

void Client::sendPlayerPos()
{
	LocalPlayer *player = m_env.getLocalPlayer();
	if (!player || !m_camera)
		return;

	ClientMap &map = m_env.getClientMap();
	PlayerControl &control = player->control;
	const float camera_fov = map.getCameraFov();
	const float wanted_range = map.getServerWantedRange();
	const u32 key_pressed = control.getKeysPressed();
	const bool camera_inverted = m_camera->getCameraMode() == CAMERA_MODE_THIRD_FRONT;
	const f32 movement_speed = control.movement_speed;
	const f32 movement_dir = control.movement_direction;

	const bool identical = player->last_position == player->getPosition() &&
						   player->last_speed == player->getSpeed() &&
						   player->last_pitch == player->getPitch() &&
						   player->last_yaw == player->getYaw() &&
						   player->last_keyPressed == key_pressed &&
						   player->last_camera_fov == camera_fov &&
						   player->last_camera_inverted == camera_inverted &&
						   player->last_wanted_range == wanted_range &&
						   player->last_movement_speed == movement_speed &&
						   player->last_movement_dir == movement_dir;
	if (identical) {
		// Repeat unreliable packets to reduce the impact of packet loss.
		m_playerpos_repeat_count++;
		if (m_playerpos_repeat_count >= 5)
			return;
	} else {
		m_playerpos_repeat_count = 0;
	}

	player->last_position = player->getPosition();
	player->last_speed = player->getSpeed();
	player->last_pitch = player->getPitch();
	player->last_yaw = player->getYaw();
	player->last_keyPressed = key_pressed;
	player->last_camera_fov = camera_fov;
	player->last_camera_inverted = camera_inverted;
	player->last_wanted_range = wanted_range;
	player->last_movement_speed = movement_speed;
	player->last_movement_dir = movement_dir;

	MSGPACK_PACKET_INIT((int)TOSERVER_PLAYERPOS, 14);
	writePlayerPos(player, &map, pk, camera_inverted);
	Send(0, buffer, false);
}

void Client::sendHaveMedia(const std::vector<u32> &tokens)
{
	sanity_check(tokens.size() < 256);
	MSGPACK_PACKET_INIT((int)TOSERVER_HAVE_MEDIA, 1);
	PACK(TOSERVER_HAVE_MEDIA_TOKENS, tokens);
	Send(2, buffer, true);
}

void Client::sendUpdateClientInfo(const ClientDynamicInfo &info)
{
	MSGPACK_PACKET_INIT((int)TOSERVER_UPDATE_CLIENT_INFO, 6);
	const std::array<u32, 2> render_target_size{
			info.render_target_size.X, info.render_target_size.Y};
	PACK(TOSERVER_UPDATE_CLIENT_INFO_RENDER_TARGET_SIZE, render_target_size);
	PACK(TOSERVER_UPDATE_CLIENT_INFO_REAL_GUI_SCALING, info.real_gui_scaling);
	PACK(TOSERVER_UPDATE_CLIENT_INFO_REAL_HUD_SCALING, info.real_hud_scaling);
	PACK(TOSERVER_UPDATE_CLIENT_INFO_MAX_FS_SIZE, info.max_fs_size);
	PACK(TOSERVER_UPDATE_CLIENT_INFO_TOUCH_CONTROLS, info.touch_controls);
	Send(2, buffer, true);
}

#endif
