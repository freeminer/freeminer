/*
Copyright (C) 2024 proller <proler@gmail.com>
*/

/*
This file is part of Freeminer.

Freeminer is free software: you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation, either version 3 of the License, or
(at your option) any later version.

Freeminer is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with Freeminer. If not, see <http://www.gnu.org/licenses/>.
*/

#include "config.h"
#if !MINETEST_PROTO

#include "server.h"

#include "chatmessage.h"
#include "filesys.h"
#include "inventory.h"
#include "itemdef.h"
#include "lighting.h"
#include "mapblock.h"
#include "nodedef.h"
#include "nodemetadata.h"
#include "particles.h"
#include "profiler.h"
#include "remoteplayer.h"
#include "serialization.h"
#include "server/player_sao.h"
#include "server/serveractiveobject.h"
#include "server/serverinventorymgr.h"
#include "serverenvironment.h"
#include "scripting_server.h"
#include "settings.h"
#include "skyparams.h"
#include "translation.h"
#include "util/base64.h"
#include "util/numeric.h"
#include "util/serialize.h"

#include <cmath>
#include <sstream>
#include <tuple>
#include <unordered_set>

namespace
{
void sendToClients(ClientInterface &clients, session_t peer_id, u8 channel,
		const msgpack::sbuffer &buffer, bool reliable)
{
	if (peer_id == PEER_ID_INEXISTENT) {
		for (const session_t client_id : clients.getClientIDs())
			clients.send(client_id, channel, buffer, reliable);
	} else {
		clients.send(peer_id, channel, buffer, reliable);
	}
}

std::string serializeParticle(const ParticleParameters &p, u16 protocol_version)
{
	std::ostringstream os(std::ios::binary);
	p.serialize(os, protocol_version);
	return os.str();
}

}

void Server::SendMovement(session_t peer_id)
{
	MSGPACK_PACKET_INIT((int)TOCLIENT_MOVEMENT, 13);
	PACK(TOCLIENT_MOVEMENT_ACCELERATION_DEFAULT,
			g_settings->getFloat("movement_acceleration_default") * BS);
	PACK(TOCLIENT_MOVEMENT_ACCELERATION_AIR,
			g_settings->getFloat("movement_acceleration_air") * BS);
	PACK(TOCLIENT_MOVEMENT_ACCELERATION_FAST,
			g_settings->getFloat("movement_acceleration_fast") * BS);
	PACK(TOCLIENT_MOVEMENT_SPEED_WALK, g_settings->getFloat("movement_speed_walk") * BS);
	PACK(TOCLIENT_MOVEMENT_SPEED_CROUCH,
			g_settings->getFloat("movement_speed_crouch") * BS);
	PACK(TOCLIENT_MOVEMENT_SPEED_FAST, g_settings->getFloat("movement_speed_fast") * BS);
	PACK(TOCLIENT_MOVEMENT_SPEED_CLIMB,
			g_settings->getFloat("movement_speed_climb") * BS);
	PACK(TOCLIENT_MOVEMENT_SPEED_JUMP, g_settings->getFloat("movement_speed_jump") * BS);
	PACK(TOCLIENT_MOVEMENT_LIQUID_FLUIDITY,
			g_settings->getFloat("movement_liquid_fluidity") * BS);
	PACK(TOCLIENT_MOVEMENT_LIQUID_FLUIDITY_SMOOTH,
			g_settings->getFloat("movement_liquid_fluidity_smooth") * BS);
	PACK(TOCLIENT_MOVEMENT_LIQUID_SINK,
			g_settings->getFloat("movement_liquid_sink") * BS);
	PACK(TOCLIENT_MOVEMENT_GRAVITY, g_settings->getFloat("movement_gravity") * BS);
	PACK(TOCLIENT_MOVEMENT_FALL_AERODYNAMICS,
			g_settings->getFloat("movement_fall_aerodynamics") * BS);
	m_clients.send(peer_id, 0, buffer, true);
}

void Server::SendPlayerHP(PlayerSAO *playersao, bool effect)
{
	assert(playersao);
	SendHP(playersao->getPeerID(), playersao->getHP(), effect);
}

void Server::SendHP(session_t peer_id, u16 hp, bool effect)
{
	MSGPACK_PACKET_INIT((int)TOCLIENT_HP, 2);
	PACK(TOCLIENT_HP_HP, hp);
	PACK(TOCLIENT_HP_EFFECT, effect);
	m_clients.send(peer_id, 0, buffer, true);
}

void Server::SendBreath(session_t peer_id, u16 breath)
{
	MSGPACK_PACKET_INIT((int)TOCLIENT_BREATH, 1);
	PACK(TOCLIENT_BREATH_BREATH, breath);
	m_clients.send(peer_id, 0, buffer, true);
}

void Server::SendAccessDenied(session_t peer_id, AccessDeniedCode reason,
		std::string_view custom_reason, bool reconnect)
{
	if (reason >= SERVER_ACCESSDENIED_MAX)
		return;

	MSGPACK_PACKET_INIT((int)TOCLIENT_ACCESS_DENIED, 3);
	PACK(TOCLIENT_ACCESS_DENIED_CUSTOM_STRING, std::string(custom_reason));
	PACK(TOCLIENT_ACCESS_DENIED_REASON, (u8)reason);
	PACK(TOCLIENT_ACCESS_DENIED_RECONNECT, reconnect);
	m_clients.send(peer_id, 1, buffer, true);
}

void Server::SendItemDef(
		session_t peer_id, IItemDefManager *itemdef, u16 protocol_version)
{
	auto client = m_clients.getClient(peer_id, CS_InitDone);
	if (!client)
		return;

	MSGPACK_PACKET_INIT((int)TOCLIENT_ITEMDEF, 1);
	std::ostringstream serialized(std::ios::binary);
	itemdef->serialize(serialized, protocol_version);
	if (client->net_proto_version_fm >= 2) {
		std::ostringstream compressed(std::ios::binary);
		compressZlib(serialized.str(), compressed);
		PACK(TOCLIENT_ITEMDEF_DEFINITIONS_ZIP, compressed.str());
	} else {
		PACK(TOCLIENT_ITEMDEF_DEFINITIONS, serialized.str());
	}
	m_clients.send(peer_id, 0, buffer, true);
}

void Server::SendNodeDef(
		session_t peer_id, const NodeDefManager *nodedef, u16 protocol_version)
{
	auto client = m_clients.getClient(peer_id, CS_InitDone);
	if (!client)
		return;

	MSGPACK_PACKET_INIT((int)TOCLIENT_NODEDEF, 1);
	std::ostringstream serialized(std::ios::binary);
	nodedef->serialize(serialized, protocol_version);
	if (client->net_proto_version_fm >= 2) {
		std::ostringstream compressed(std::ios::binary);
		compressZlib(serialized.str(), compressed);
		PACK(TOCLIENT_NODEDEF_DEFINITIONS_ZIP, compressed.str());
	} else {
		PACK(TOCLIENT_NODEDEF_DEFINITIONS, serialized.str());
	}
	m_clients.send(peer_id, 0, buffer, true);
}

void Server::SendInventory(RemotePlayer *player, bool incremental, bool skip_wield_anim)
{
	UpdateCrafting(player);

	// The MessagePack receiver applies complete inventories only.
	std::ostringstream os(std::ios::binary);
	player->inventory.serialize(os, false);
	player->inventory.setModified(false);
	player->setModified(true);

	MSGPACK_PACKET_INIT((int)TOCLIENT_INVENTORY, 2);
	PACK(TOCLIENT_INVENTORY_DATA, os.str());
	PACK(TOCLIENT_INVENTORY_SKIP_WIELD_ANIM, skip_wield_anim);
	m_clients.send(player->getPeerId(), 0, buffer, true);
}

void Server::SendChatMessage(session_t peer_id, const ChatMessage &message)
{
	MSGPACK_PACKET_INIT((int)TOCLIENT_CHAT_MESSAGE, 4);
	PACK(TOCLIENT_CHAT_MESSAGE_DATA, wide_to_utf8(message.message));
	PACK(TOCLIENT_CHAT_MESSAGE_TYPE, (u8)message.type);
	PACK(TOCLIENT_CHAT_MESSAGE_SENDER, wide_to_utf8(message.sender));
	PACK(TOCLIENT_CHAT_MESSAGE_TIMESTAMP, (u64)message.timestamp);

	if (peer_id == PEER_ID_INEXISTENT) {
		for (const session_t client_id : m_clients.getClientIDs(CS_InitDone))
			m_clients.send(client_id, 0, buffer, true);
	} else {
		m_clients.send(peer_id, 0, buffer, true);
	}
}

void Server::SendShowFormspecMessage(
		session_t peer_id, const std::string &formspec, const std::string &formname)
{
	if (formspec.empty()) {
		const auto it = m_formspec_state_data.find(peer_id);
		if (it != m_formspec_state_data.end() &&
				(it->second == formname || formname.empty()))
			m_formspec_state_data.erase(it);
	} else {
		m_formspec_state_data[peer_id] = formname;
	}

	MSGPACK_PACKET_INIT((int)TOCLIENT_SHOW_FORMSPEC, 2);
	PACK(TOCLIENT_SHOW_FORMSPEC_DATA, formspec);
	PACK(TOCLIENT_SHOW_FORMSPEC_NAME, formname);
	m_clients.send(peer_id, 0, buffer, true);
}

void Server::SendSpawnParticles(
		RemotePlayer *player, const std::vector<ParticleParameters> &particles)
{
	PlayerSAO *sao = player->getPlayerSAO();
	if (!sao || sao->isGone())
		return;

	const float radius =
			g_settings->getS16("max_block_send_distance") * MAP_BLOCKSIZE * BS;
	const float radius_sq = radius * radius;
	for (const auto &p : particles) {
		if (sao->getBasePosition().getDistanceFromSQ(p.pos * BS) > radius_sq)
			continue;

		MSGPACK_PACKET_INIT((int)TOCLIENT_SPAWN_PARTICLE, 10);
		PACK(TOCLIENT_SPAWN_PARTICLE_POS, p.pos);
		PACK(TOCLIENT_SPAWN_PARTICLE_VELOCITY, p.vel);
		PACK(TOCLIENT_SPAWN_PARTICLE_ACCELERATION, p.acc);
		PACK(TOCLIENT_SPAWN_PARTICLE_EXPIRATIONTIME, p.expirationtime);
		PACK(TOCLIENT_SPAWN_PARTICLE_SIZE, p.size);
		PACK(TOCLIENT_SPAWN_PARTICLE_COLLISIONDETECTION, p.collisiondetection);
		PACK(TOCLIENT_SPAWN_PARTICLE_VERTICAL, p.vertical);
		PACK(TOCLIENT_SPAWN_PARTICLE_TEXTURE, p.texture.string);
		PACK(TOCLIENT_SPAWN_PARTICLE_COLLISION_REMOVAL, p.collision_removal);
		PACK(TOCLIENT_SPAWN_PARTICLE_DATA,
				serializeParticle(p, player->protocol_version));
		m_clients.send(player->getPeerId(), 0, buffer, true);
	}
}

void Server::SendSpawnParticles()
{
	for (const auto &[pname, particles] : m_particles_to_send) {
		if (pname.empty())
			continue;
		if (auto *player = m_env->getPlayer(pname.c_str()))
			SendSpawnParticles(player, particles);
	}
	for (auto *player : m_env->getPlayers())
		SendSpawnParticles(player, m_particles_to_send[""]);
	m_particles_to_send.clear();
}

void Server::SendAddParticleSpawner(const std::string &to_player,
		const std::string &exclude_player, const ParticleSpawnerParameters &p,
		u16 attached_id, u32 id)
{
	const float radius =
			g_settings->getS16("max_block_send_distance") * MAP_BLOCKSIZE * BS;
	const float radius_sq = radius * radius;
	const v3f pos = (p.pos.start.min.val + p.pos.start.max.val + p.pos.end.min.val +
							p.pos.end.max.val) /
					4.0f * BS;
	const bool distance_check = !attached_id && p.time <= 1.0f && p.time != 0.0f;

	const auto consider_player = [&](RemotePlayer *player) {
		if (distance_check) {
			PlayerSAO *sao = player->getPlayerSAO();
			if (!sao || sao->getBasePosition().getDistanceFromSQ(pos) > radius_sq)
				return;
		}
		SendAddParticleSpawner(
				player->getPeerId(), player->protocol_version, p, attached_id, id);
	};

	if (!to_player.empty()) {
		if (auto *player = m_env->getPlayer(to_player))
			consider_player(player);
		return;
	}
	for (const session_t client_id : m_clients.getClientIDs()) {
		RemotePlayer *player = m_env->getPlayer(client_id);
		if (player && (exclude_player.empty() || exclude_player != player->getName()))
			consider_player(player);
	}
}

void Server::SendAddParticleSpawner(session_t peer_id, u16 protocol_version,
		const ParticleSpawnerParameters &p, u16 attached_id, u32 id)
{
	assert(peer_id != PEER_ID_INEXISTENT);
	assert(protocol_version != 0);

	MSGPACK_PACKET_INIT((int)TOCLIENT_ADD_PARTICLESPAWNER, 24);
	PACK(TOCLIENT_ADD_PARTICLESPAWNER_AMOUNT, p.amount);
	PACK(TOCLIENT_ADD_PARTICLESPAWNER_SPAWNTIME, p.time);
	PACK(TOCLIENT_ADD_PARTICLESPAWNER_MINPOS, p.pos.start.min.val);
	PACK(TOCLIENT_ADD_PARTICLESPAWNER_MAXPOS, p.pos.start.max.val);
	PACK(TOCLIENT_ADD_PARTICLESPAWNER_MINVEL, p.vel.start.min.val);
	PACK(TOCLIENT_ADD_PARTICLESPAWNER_MAXVEL, p.vel.start.max.val);
	PACK(TOCLIENT_ADD_PARTICLESPAWNER_MINACC, p.acc.start.min.val);
	PACK(TOCLIENT_ADD_PARTICLESPAWNER_MAXACC, p.acc.start.max.val);
	PACK(TOCLIENT_ADD_PARTICLESPAWNER_MINEXPTIME, p.exptime.start.min.val);
	PACK(TOCLIENT_ADD_PARTICLESPAWNER_MAXEXPTIME, p.exptime.start.max.val);
	PACK(TOCLIENT_ADD_PARTICLESPAWNER_MINSIZE, p.size.start.min.val);
	PACK(TOCLIENT_ADD_PARTICLESPAWNER_MAXSIZE, p.size.start.max.val);
	PACK(TOCLIENT_ADD_PARTICLESPAWNER_COLLISIONDETECTION, p.collisiondetection);
	PACK(TOCLIENT_ADD_PARTICLESPAWNER_TEXTURE, p.texture.string);
	PACK(TOCLIENT_ADD_PARTICLESPAWNER_VERTICAL, p.vertical);
	PACK(TOCLIENT_ADD_PARTICLESPAWNER_ID, id);
	PACK(TOCLIENT_ADD_PARTICLESPAWNER_COLLISION_REMOVAL, p.collision_removal);
	PACK(TOCLIENT_ADD_PARTICLESPAWNER_ATTACHED_ID, attached_id);

	std::ostringstream os(std::ios::binary);
	p.animation.serialize(os, protocol_version);
	PACK(TOCLIENT_ADD_PARTICLESPAWNER_ANIMATION, os.str());
	PACK(TOCLIENT_ADD_PARTICLESPAWNER_GLOW, p.glow);
	PACK(TOCLIENT_ADD_PARTICLESPAWNER_OBJECT_COLLISION, p.object_collision);
	PACK(TOCLIENT_ADD_PARTICLESPAWNER_NODE, p.node);
	PACK(TOCLIENT_ADD_PARTICLESPAWNER_NODE_TILE, p.node_tile);

	// Preserve every current property for receivers that understand the blob.
	std::ostringstream data(std::ios::binary);
	p.pos.serialize(data);
	p.vel.serialize(data);
	p.acc.serialize(data);
	p.exptime.serialize(data);
	p.size.serialize(data);
	p.texture.serialize(data, protocol_version, true);
	p.drag.serialize(data);
	p.jitter.serialize(data);
	p.bounce.serialize(data);
	ParticleParamTypes::serializeParameterValue(data, p.attractor_kind);
	if (p.attractor_kind != ParticleParamTypes::AttractorKind::none) {
		p.attract.serialize(data);
		p.attractor_origin.serialize(data);
		writeU16(data, p.attractor_attachment);
		writeU8(data, p.attractor_kill);
		if (p.attractor_kind != ParticleParamTypes::AttractorKind::point) {
			p.attractor_direction.serialize(data);
			writeU16(data, p.attractor_direction_attachment);
		}
	}
	p.radius.serialize(data);
	ParticleParamTypes::serializeParameterValue(data, (u16)p.texpool.size());
	for (const auto &texture : p.texpool)
		texture.serialize(data, protocol_version);
	PACK(TOCLIENT_ADD_PARTICLESPAWNER_DATA, data.str());
	m_clients.send(peer_id, 0, buffer, true);
}

void Server::SendDeleteParticleSpawner(session_t peer_id, u32 id)
{
	MSGPACK_PACKET_INIT((int)TOCLIENT_DELETE_PARTICLESPAWNER, 1);
	PACK(TOCLIENT_DELETE_PARTICLESPAWNER_ID, id);
	sendToClients(m_clients, peer_id, 0, buffer, true);
}

void Server::SendHUDAdd(session_t peer_id, u32 id, HudElement *form)
{
	MSGPACK_PACKET_INIT((int)TOCLIENT_HUDADD, 17);
	PACK(TOCLIENT_HUDADD_ID, id);
	PACK(TOCLIENT_HUDADD_TYPE, (u8)form->type);
	PACK(TOCLIENT_HUDADD_POS, form->pos);
	PACK(TOCLIENT_HUDADD_NAME, form->name);
	PACK(TOCLIENT_HUDADD_SCALE, form->scale);
	PACK(TOCLIENT_HUDADD_TEXT, form->text);
	PACK(TOCLIENT_HUDADD_NUMBER, form->number);
	PACK(TOCLIENT_HUDADD_ITEM, form->item);
	PACK(TOCLIENT_HUDADD_DIR, form->dir);
	PACK(TOCLIENT_HUDADD_ALIGN, form->align);
	PACK(TOCLIENT_HUDADD_OFFSET, form->offset);
	PACK(TOCLIENT_HUDADD_WORLD_POS, form->world_pos);
	PACK(TOCLIENT_HUDADD_SIZE, form->size);
	PACK(TOCLIENT_HUDADD_Z_INDEX, form->z_index);
	PACK(TOCLIENT_HUDADD_TEXT2, form->text2);
	PACK(TOCLIENT_HUDADD_STYLE, form->style);
	PACK(TOCLIENT_HUDADD_FLAGS, (u8)(form->hideable ? 1 : 0));
	m_clients.send(peer_id, 1, buffer, true);
}

void Server::SendHUDRemove(session_t peer_id, u32 id)
{
	MSGPACK_PACKET_INIT((int)TOCLIENT_HUDRM, 1);
	PACK(TOCLIENT_HUDRM_ID, id);
	m_clients.send(peer_id, 1, buffer, true);
}

void Server::SendHUDChange(session_t peer_id, u32 id, HudElementStat stat, void *value)
{
	MSGPACK_PACKET_INIT((int)TOCLIENT_HUDCHANGE, 3);
	PACK(TOCLIENT_HUDCHANGE_ID, id);
	PACK(TOCLIENT_HUDCHANGE_STAT, (u8)stat);

	switch (stat) {
	case HUD_STAT_POS:
	case HUD_STAT_SCALE:
	case HUD_STAT_ALIGN:
	case HUD_STAT_OFFSET:
		PACK(TOCLIENT_HUDCHANGE_V2F, *(v2f *)value);
		break;
	case HUD_STAT_NAME:
	case HUD_STAT_TEXT:
	case HUD_STAT_TEXT2:
		PACK(TOCLIENT_HUDCHANGE_STRING, *(std::string *)value);
		break;
	case HUD_STAT_WORLD_POS:
		PACK(TOCLIENT_HUDCHANGE_V3F, *(v3f *)value);
		break;
	case HUD_STAT_SIZE:
		if (m_clients.getProtocolVersion(peer_id) >= 52) {
			PACK(TOCLIENT_HUDCHANGE_V2F, *(v2f *)value);
		} else {
			PACK(TOCLIENT_HUDCHANGE_V2S32, v2s32::from(*(v2f *)value));
		}
		break;
	case HUD_STAT_HIDEABLE:
		PACK(TOCLIENT_HUDCHANGE_U32, u32{*(bool *)value});
		break;
	default:
		PACK(TOCLIENT_HUDCHANGE_U32, *(u32 *)value);
		break;
	}
	m_clients.send(peer_id, 0, buffer, true);
}

void Server::SendHUDSetFlags(session_t peer_id, u32 flags, u32 mask)
{
	MSGPACK_PACKET_INIT((int)TOCLIENT_HUD_SET_FLAGS, 2);
	PACK(TOCLIENT_HUD_SET_FLAGS_FLAGS, flags);
	PACK(TOCLIENT_HUD_SET_FLAGS_MASK, mask);
	m_clients.send(peer_id, 0, buffer, true);
}

void Server::SendHUDSetParam(session_t peer_id, u16 param, std::string_view value)
{
	MSGPACK_PACKET_INIT((int)TOCLIENT_HUD_SET_PARAM, 2);
	PACK(TOCLIENT_HUD_SET_PARAM_ID, param);
	PACK(TOCLIENT_HUD_SET_PARAM_VALUE, std::string(value));
	m_clients.send(peer_id, 0, buffer, true);
}

void Server::SendSetSky(session_t peer_id, const SkyboxParams &params)
{
	MSGPACK_PACKET_INIT((int)TOCLIENT_SET_SKY, 19);
	PACK(TOCLIENT_SET_SKY_COLOR, params.bgcolor);
	PACK(TOCLIENT_SET_SKY_TYPE, params.type);
	PACK(TOCLIENT_SET_SKY_PARAMS, params.textures);
	PACK(TOCLIENT_SET_SKY_CLOUDS, params.clouds);
	PACK(TOCLIENT_SET_SKY_DAY_SKY, params.sky_color.day_sky);
	PACK(TOCLIENT_SET_SKY_DAY_HORIZON, params.sky_color.day_horizon);
	PACK(TOCLIENT_SET_SKY_DAWN_SKY, params.sky_color.dawn_sky);
	PACK(TOCLIENT_SET_SKY_DAWN_HORIZON, params.sky_color.dawn_horizon);
	PACK(TOCLIENT_SET_SKY_NIGHT_SKY, params.sky_color.night_sky);
	PACK(TOCLIENT_SET_SKY_NIGHT_HORIZON, params.sky_color.night_horizon);
	PACK(TOCLIENT_SET_SKY_INDOORS, params.sky_color.indoors);
	PACK(TOCLIENT_SET_SKY_FOG_SUN_TINT, params.fog_sun_tint);
	PACK(TOCLIENT_SET_SKY_FOG_MOON_TINT, params.fog_moon_tint);
	PACK(TOCLIENT_SET_SKY_FOG_TINT_TYPE, params.fog_tint_type);
	PACK(TOCLIENT_SET_SKY_BODY_ORBIT_TILT, params.body_orbit_tilt);
	PACK(TOCLIENT_SET_SKY_FOG_DISTANCE, params.fog_distance);
	PACK(TOCLIENT_SET_SKY_FOG_START, params.fog_start);
	PACK(TOCLIENT_SET_SKY_FOG_COLOR, params.fog_color);
	PACK(TOCLIENT_SET_SKY_AUTO_DIM, params.auto_dim_skybox);
	m_clients.send(peer_id, 0, buffer, true);
}

void Server::SendSetSun(session_t peer_id, const SunParams &params)
{
	MSGPACK_PACKET_INIT((int)TOCLIENT_SET_SUN, 6);
	PACK(TOCLIENT_SET_SUN_VISIBLE, params.visible);
	PACK(TOCLIENT_SET_SUN_TEXTURE, params.texture);
	PACK(TOCLIENT_SET_SUN_TONEMAP, params.tonemap);
	PACK(TOCLIENT_SET_SUN_SUNRISE, params.sunrise);
	PACK(TOCLIENT_SET_SUN_SUNRISE_VISIBLE, params.sunrise_visible);
	PACK(TOCLIENT_SET_SUN_SCALE, params.scale);
	m_clients.send(peer_id, 0, buffer, true);
}

void Server::SendSetMoon(session_t peer_id, const MoonParams &params)
{
	MSGPACK_PACKET_INIT((int)TOCLIENT_SET_MOON, 4);
	PACK(TOCLIENT_SET_MOON_VISIBLE, params.visible);
	PACK(TOCLIENT_SET_MOON_TEXTURE, params.texture);
	PACK(TOCLIENT_SET_MOON_TONEMAP, params.tonemap);
	PACK(TOCLIENT_SET_MOON_SCALE, params.scale);
	m_clients.send(peer_id, 0, buffer, true);
}

void Server::SendSetStars(session_t peer_id, const StarParams &params)
{
	MSGPACK_PACKET_INIT((int)TOCLIENT_SET_STARS, 6);
	PACK(TOCLIENT_SET_STARS_VISIBLE, params.visible);
	PACK(TOCLIENT_SET_STARS_COUNT, params.count);
	PACK(TOCLIENT_SET_STARS_COLOR, params.starcolor);
	PACK(TOCLIENT_SET_STARS_SCALE, params.scale);
	PACK(TOCLIENT_SET_STARS_DAY_OPACITY, params.day_opacity);
	PACK(TOCLIENT_SET_STARS_SEED, params.star_seed);
	m_clients.send(peer_id, 0, buffer, true);
}

void Server::SendCloudParams(session_t peer_id, const CloudParams &params)
{
	MSGPACK_PACKET_INIT((int)TOCLIENT_CLOUD_PARAMS, 7);
	PACK(TOCLIENT_CLOUD_PARAMS_DENSITY, params.density);
	PACK(TOCLIENT_CLOUD_PARAMS_COLOR_BRIGHT, params.color_bright);
	PACK(TOCLIENT_CLOUD_PARAMS_COLOR_AMBIENT, params.color_ambient);
	PACK(TOCLIENT_CLOUD_PARAMS_HEIGHT, params.height);
	PACK(TOCLIENT_CLOUD_PARAMS_THICKNESS, params.thickness);
	PACK(TOCLIENT_CLOUD_PARAMS_SPEED, params.speed);
	PACK(TOCLIENT_CLOUD_PARAMS_COLOR_SHADOW, params.color_shadow);
	m_clients.send(peer_id, 0, buffer, true);
}

void Server::SendOverrideDayNightRatio(session_t peer_id, bool do_override, float ratio)
{
	MSGPACK_PACKET_INIT((int)TOCLIENT_OVERRIDE_DAY_NIGHT_RATIO, 2);
	PACK(TOCLIENT_OVERRIDE_DAY_NIGHT_RATIO_DO, do_override);
	PACK(TOCLIENT_OVERRIDE_DAY_NIGHT_RATIO_VALUE, ratio);
	m_clients.send(peer_id, 0, buffer, true);
}

void Server::SendSetLighting(session_t peer_id, const Lighting &lighting)
{
	MSGPACK_PACKET_INIT((int)TOCLIENT_SET_LIGHTING, 14);
	PACK(TOCLIENT_SET_LIGHTING_SHADOW_INTENSITY, lighting.shadow_intensity);
	PACK(TOCLIENT_SET_LIGHTING_SATURATION, lighting.saturation);
	PACK(TOCLIENT_SET_LIGHTING_LUMINANCE_MIN, lighting.exposure.luminance_min);
	PACK(TOCLIENT_SET_LIGHTING_LUMINANCE_MAX, lighting.exposure.luminance_max);
	PACK(TOCLIENT_SET_LIGHTING_EXPOSURE_CORRECTION,
			lighting.exposure.exposure_correction);
	PACK(TOCLIENT_SET_LIGHTING_SPEED_DARK_BRIGHT, lighting.exposure.speed_dark_bright);
	PACK(TOCLIENT_SET_LIGHTING_SPEED_BRIGHT_DARK, lighting.exposure.speed_bright_dark);
	PACK(TOCLIENT_SET_LIGHTING_CENTER_WEIGHT_POWER,
			lighting.exposure.center_weight_power);
	PACK(TOCLIENT_SET_LIGHTING_VOLUMETRIC_STRENGTH, lighting.volumetric_light_strength);
	PACK(TOCLIENT_SET_LIGHTING_SHADOW_TINT, lighting.shadow_tint);
	PACK(TOCLIENT_SET_LIGHTING_BLOOM_INTENSITY, lighting.bloom_intensity);
	PACK(TOCLIENT_SET_LIGHTING_BLOOM_STRENGTH_FACTOR, lighting.bloom_strength_factor);
	PACK(TOCLIENT_SET_LIGHTING_BLOOM_RADIUS, lighting.bloom_radius);
	PACK(TOCLIENT_SET_LIGHTING_SHADOW_DIRECTION, lighting.shadow_direction);
	m_clients.send(peer_id, 0, buffer, true);
}

void Server::SendCamera(session_t peer_id, Player *player)
{
	MSGPACK_PACKET_INIT((int)TOCLIENT_CAMERA, 1);
	PACK(TOCLIENT_CAMERA_ALLOWED_MODE, (u8)player->allowed_camera_mode);
	m_clients.send(peer_id, 0, buffer, true);
}

void Server::SendTimeOfDay(session_t peer_id, u16 time, f32 time_speed)
{
	MSGPACK_PACKET_INIT((int)TOCLIENT_TIME_OF_DAY, 2);
	PACK(TOCLIENT_TIME_OF_DAY_TIME, time);
	PACK(TOCLIENT_TIME_OF_DAY_TIME_SPEED, time_speed);
	sendToClients(m_clients, peer_id, 0, buffer, true);
}

void Server::SendPlayerBreath(PlayerSAO *sao)
{
	assert(sao);
	m_script->player_event(sao, "breath_changed");
	SendBreath(sao->getPeerID(), sao->getBreath());
}

void Server::SendMovePlayer(PlayerSAO *sao)
{
	if (sao->isAttachmentOutdated() && !sao->isAttached()) {
		std::string data;
		ActiveObjectMessage aom(
				sao->getId(), true, sao->generateUpdateAttachmentCommand());
		aom.appendTo(data);
		SendActiveObjectMessages(sao->getPeerID(), data);
	}

	MSGPACK_PACKET_INIT((int)TOCLIENT_MOVE_PLAYER, 3);
	PACK(TOCLIENT_MOVE_PLAYER_POS, sao->getBasePosition());
	PACK(TOCLIENT_MOVE_PLAYER_PITCH, sao->getLookPitch());
	PACK(TOCLIENT_MOVE_PLAYER_YAW, sao->getRotation().Y);
	m_clients.send(sao->getPeerID(), 0, buffer, true);
}

void Server::SendMovePlayerRel(session_t peer_id, const v3f &added_pos)
{
	MSGPACK_PACKET_INIT((int)TOCLIENT_MOVE_PLAYER_REL, 1);
	PACK(TOCLIENT_MOVE_PLAYER_REL_POS, added_pos);
	m_clients.send(peer_id, 0, buffer, false);
}

void Server::SendPlayerFov(session_t peer_id)
{
	RemotePlayer *player = m_env->getPlayer(peer_id);
	if (!player)
		return;

	const PlayerFovSpec fov = player->getFov();
	MSGPACK_PACKET_INIT((int)TOCLIENT_FOV, 3);
	PACK(TOCLIENT_FOV_FOV, fov.fov);
	PACK(TOCLIENT_FOV_IS_MULTIPLIER, fov.is_multiplier);
	PACK(TOCLIENT_FOV_TRANSITION_TIME, fov.transition_time);
	m_clients.send(peer_id, 0, buffer, true);
}

void Server::SendLocalPlayerAnimations(
		session_t peer_id, v2f animation_frames[4], f32 animation_speed)
{
	MSGPACK_PACKET_INIT((int)TOCLIENT_LOCAL_PLAYER_ANIMATIONS, 5);
	for (int i = 0; i < 4; ++i) {
		if (m_clients.getProtocolVersion(peer_id) >= 46) {
			PACK(i, animation_frames[i]);
		} else {
			PACK(i, v2s32::from(animation_frames[i]));
		}
	}
	PACK(TOCLIENT_LOCAL_PLAYER_ANIMATIONS_FRAME_SPEED, animation_speed);
	m_clients.send(peer_id, 0, buffer, true);
}

void Server::SendEyeOffset(session_t peer_id, v3f first, v3f third, v3f third_front)
{
	MSGPACK_PACKET_INIT((int)TOCLIENT_EYE_OFFSET, 3);
	PACK(TOCLIENT_EYE_OFFSET_FIRST, first);
	PACK(TOCLIENT_EYE_OFFSET_THIRD, third);
	PACK(TOCLIENT_EYE_OFFSET_THIRD_FRONT, third_front);
	m_clients.send(peer_id, 0, buffer, true);
}

void Server::SendPunchPlayer(session_t peer_id, v3f speed)
{
	MSGPACK_PACKET_INIT((int)TOCLIENT_PUNCH_PLAYER, 1);
	PACK(TOCLIENT_PUNCH_PLAYER_SPEED, speed);
	m_clients.send(peer_id, 0, buffer, true);
}

void Server::SendPlayerPrivileges(session_t peer_id)
{
	RemotePlayer *player = m_env->getPlayer(peer_id);
	if (!player)
		return;

	std::set<std::string> privs;
	m_script->getAuth(player->getName(), nullptr, &privs);
	MSGPACK_PACKET_INIT((int)TOCLIENT_PRIVILEGES, 1);
	PACK(TOCLIENT_PRIVILEGES_PRIVILEGES, privs);
	m_clients.send(peer_id, 0, buffer, true);
}

void Server::SendPlayerInventoryFormspec(session_t peer_id)
{
	RemotePlayer *player = m_env->getPlayer(peer_id);
	if (!player)
		return;

	MSGPACK_PACKET_INIT((int)TOCLIENT_INVENTORY_FORMSPEC, 1);
	PACK(TOCLIENT_INVENTORY_FORMSPEC_DATA, player->inventory_formspec);
	m_clients.send(peer_id, 0, buffer, true);
}

void Server::SendPlayerFormspecPrepend(session_t peer_id)
{
	RemotePlayer *player = m_env->getPlayer(peer_id);
	if (!player)
		return;

	MSGPACK_PACKET_INIT((int)TOCLIENT_FORMSPEC_PREPEND, 1);
	PACK(TOCLIENT_FORMSPEC_PREPEND_DATA, player->formspec_prepend);
	m_clients.send(peer_id, 0, buffer, true);
}

void Server::SendActiveObjectRemoveAdd(RemoteClient *client, PlayerSAO *playersao)
{
	static thread_local const pos_t radius =
			g_settings->getS16("active_object_send_range_blocks") * MAP_BLOCKSIZE;
	static thread_local const bool is_transfer_limited =
			g_settings->exists("unlimited_player_transfer_distance") &&
			!g_settings->getBool("unlimited_player_transfer_distance");
	static thread_local const pos_t player_transfer_dist =
			g_settings->getS16("player_transfer_distance") * MAP_BLOCKSIZE;

	pos_t player_radius = player_transfer_dist == 0 && is_transfer_limited
								  ? radius
								  : player_transfer_dist;
	pos_t my_radius = MYMIN(radius, playersao->getWantedRange() * MAP_BLOCKSIZE);
	if (my_radius <= 0)
		my_radius = radius;
	my_radius *= 1.5;

	std::vector<std::pair<bool, u16>> removed_objects;
	std::vector<u16> added_objects;
	m_env->getRemovedActiveObjects(playersao, my_radius, player_radius,
			client->m_known_objects, removed_objects);
	m_env->getAddedActiveObjects(
			playersao, my_radius, player_radius, client->m_known_objects, added_objects);
	if (removed_objects.empty() && added_objects.empty())
		return;

	std::vector<u16> removed_ids;
	removed_ids.reserve(removed_objects.size());
	for (const auto &[gone, id] : removed_objects) {
		removed_ids.push_back(id);
		ServerActiveObject *obj = m_env->getActiveObject(id, true);
		client->m_known_objects.erase(id);
		if (obj && obj->m_known_by_count > 0)
			obj->m_known_by_count--;
	}

	std::vector<ActiveObjectAddData> added_ids;
	added_ids.reserve(added_objects.size());
	for (const u16 id : added_objects) {
		ServerActiveObject *obj = m_env->getActiveObject(id);
		if (!obj) {
			warningstream << FUNCTION_NAME << ": found NULL object id=" << id
						  << std::endl;
			continue;
		}
		added_ids.emplace_back(id, obj->getSendType(),
				obj->getClientInitializationData(client->net_proto_version));
		client->m_known_objects.insert(id);
		obj->m_known_by_count++;
	}

	MSGPACK_PACKET_INIT((int)TOCLIENT_ACTIVE_OBJECT_REMOVE_ADD, 2);
	PACK(TOCLIENT_ACTIVE_OBJECT_REMOVE_ADD_REMOVE, removed_ids);
	PACK(TOCLIENT_ACTIVE_OBJECT_REMOVE_ADD_ADD, added_ids);
	m_clients.send(client->peer_id, 0, buffer, true);
}

void Server::SendActiveObjectMessages(
		session_t peer_id, const ActiveObjectMessages &datas, bool reliable)
{
	MSGPACK_PACKET_INIT((int)TOCLIENT_ACTIVE_OBJECT_MESSAGES, 1);
	PACK(TOCLIENT_ACTIVE_OBJECT_MESSAGES_MESSAGES, datas);
	m_clients.send(peer_id, 0, buffer, reliable);
}

void Server::SendActiveObjectMessages(
		session_t peer_id, const std::string &datas, bool reliable)
{
	ActiveObjectMessages messages;
	size_t offset = 0;
	const auto *bytes = reinterpret_cast<const u8 *>(datas.data());
	while (offset < datas.size()) {
		if (datas.size() - offset < 2 + 2) {
			warningstream << "Truncated active-object message stream" << std::endl;
			return;
		}
		const u16 id = readU16(bytes + offset);
		offset += 2;
		const u16 size = readU16(bytes + offset);
		offset += 2;
		if (size > datas.size() - offset) {
			warningstream << "Invalid active-object message stream" << std::endl;
			return;
		}
		messages.emplace_back(id, datas.substr(offset, size));
		offset += size;
	}
	SendActiveObjectMessages(peer_id, messages, reliable);
}

void Server::SendCSMRestrictionFlags(session_t peer_id)
{
	MSGPACK_PACKET_INIT((int)TOCLIENT_CSM_RESTRICTION_FLAGS, 2);
	PACK(TOCLIENT_CSM_RESTRICTION_FLAGS_FLAGS, m_csm_restriction_flags);
	PACK(TOCLIENT_CSM_RESTRICTION_FLAGS_NODERANGE, m_csm_restriction_noderange);
	m_clients.send(peer_id, 0, buffer, true);
}

void Server::SendPlayerSpeed(session_t peer_id, const v3f &added_vel)
{
	MSGPACK_PACKET_INIT((int)TOCLIENT_PLAYER_SPEED, 1);
	PACK(TOCLIENT_PLAYER_SPEED_ADDED_VELOCITY, added_vel);
	m_clients.send(peer_id, 0, buffer, false);
}

inline s32 Server::nextSoundId()
{
	s32 free_id = m_playing_sounds_id_last_used;
	do {
		free_id = free_id == INT32_MAX ? 0 : free_id + 1;
		if (free_id == m_playing_sounds_id_last_used)
			return 0;
	} while (free_id == 0 || m_playing_sounds.find(free_id) != m_playing_sounds.end());
	m_playing_sounds_id_last_used = free_id;
	return free_id;
}

s32 Server::playSound(ServerPlayingSound &params, bool ephemeral)
{
	bool pos_exists = false;
	const v3f pos = params.getPos(m_env, &pos_exists);
	if (pos_exists != (params.type != SoundLocation::Local))
		return -1;

	std::vector<session_t> dst_clients;
	if (!params.to_player.empty()) {
		RemotePlayer *player = m_env->getPlayer(params.to_player.c_str());
		if (!player) {
			infostream << "Server::playSound: Player \"" << params.to_player
					   << "\" not found" << std::endl;
			return -1;
		}
		dst_clients.push_back(player->getPeerId());
	} else {
		for (const session_t client_id : m_clients.getClientIDs()) {
			RemotePlayer *player = m_env->getPlayer(client_id);
			if (!player || (!params.exclude_player.empty() &&
								   params.exclude_player == player->getName()))
				continue;
			PlayerSAO *sao = player->getPlayerSAO();
			if (!sao)
				continue;
			if (pos_exists && sao->getBasePosition().getDistanceFrom(pos) >
									  params.max_hear_distance)
				continue;
			dst_clients.push_back(client_id);
		}
	}
	if (dst_clients.empty())
		return -1;

	const s32 id = ephemeral ? -1 : nextSoundId();
	if (id == 0)
		return 0;

	MSGPACK_PACKET_INIT((int)TOCLIENT_PLAY_SOUND, 11);
	PACK(TOCLIENT_PLAY_SOUND_ID, id);
	PACK(TOCLIENT_PLAY_SOUND_NAME, params.spec.name);
	PACK(TOCLIENT_PLAY_SOUND_GAIN, params.gain * params.spec.gain);
	PACK(TOCLIENT_PLAY_SOUND_TYPE, (u8)params.type);
	PACK(TOCLIENT_PLAY_SOUND_POS, pos);
	PACK(TOCLIENT_PLAY_SOUND_OBJECT_ID, params.object);
	PACK(TOCLIENT_PLAY_SOUND_LOOP, params.spec.loop);
	PACK(TOCLIENT_PLAY_SOUND_FADE, params.spec.fade);
	PACK(TOCLIENT_PLAY_SOUND_PITCH, params.spec.pitch);
	PACK(TOCLIENT_PLAY_SOUND_EPHEMERAL, ephemeral);
	PACK(TOCLIENT_PLAY_SOUND_START_TIME, params.spec.start_time);
	for (const session_t peer_id : dst_clients) {
		if (!ephemeral)
			params.clients.insert(peer_id);
		m_clients.send(peer_id, 0, buffer, !ephemeral);
	}
	if (!ephemeral)
		m_playing_sounds[id] = std::move(params);
	return id;
}

void Server::stopSound(s32 handle)
{
	auto it = m_playing_sounds.find(handle);
	if (it == m_playing_sounds.end())
		return;

	MSGPACK_PACKET_INIT((int)TOCLIENT_STOP_SOUND, 1);
	PACK(TOCLIENT_STOP_SOUND_ID, handle);
	for (const session_t peer_id : it->second.clients)
		m_clients.send(peer_id, 0, buffer, true);
	m_playing_sounds.erase(it);
}

void Server::fadeSound(s32 handle, float step, float gain)
{
	auto it = m_playing_sounds.find(handle);
	if (it == m_playing_sounds.end())
		return;

	it->second.gain = gain;
	MSGPACK_PACKET_INIT((int)TOCLIENT_FADE_SOUND, 3);
	PACK(TOCLIENT_FADE_SOUND_ID, handle);
	PACK(TOCLIENT_FADE_SOUND_STEP, step);
	PACK(TOCLIENT_FADE_SOUND_GAIN, gain);
	for (const session_t peer_id : it->second.clients)
		m_clients.send(peer_id, 0, buffer, true);
	if (gain <= 0 || it->second.clients.empty())
		m_playing_sounds.erase(it);
}

void Server::sendRemoveNode(
		v3s16 p, std::unordered_set<u16> *far_players, float far_d_nodes)
{
	MSGPACK_PACKET_INIT((int)TOCLIENT_REMOVENODE, 1);
	PACK(TOCLIENT_REMOVENODE_POS, p);
	sendNodeChangePkt(
			buffer, getNodeBlockPos(p), intToFloat(p, BS), far_d_nodes, far_players);
}

void Server::sendAddNode(v3s16 p, MapNode n, std::unordered_set<u16> *far_players,
		float far_d_nodes, bool remove_metadata)
{
	MSGPACK_PACKET_INIT((int)TOCLIENT_ADDNODE, 3);
	PACK(TOCLIENT_ADDNODE_POS, p);
	PACK(TOCLIENT_ADDNODE_NODE, n);
	PACK(TOCLIENT_ADDNODE_REMOVE_METADATA, remove_metadata);
	sendNodeChangePkt(
			buffer, getNodeBlockPos(p), intToFloat(p, BS), far_d_nodes, far_players);
}

void Server::sendNodeChangePkt(const msgpack::sbuffer &buffer, v3s16 block_pos, v3f p,
		float far_d_nodes, std::unordered_set<u16> *far_players)
{
	const float maxd = far_d_nodes * BS;
	const std::vector<session_t> clients = m_clients.getClientIDs();
	ClientInterface::AutoLock clientlock(m_clients);
	for (const session_t client_id : clients) {
		auto client = m_clients.lockedGetClientNoEx(client_id);
		if (!client)
			continue;
		RemotePlayer *player = m_env->getPlayer(client_id);
		PlayerSAO *sao = player ? player->getPlayerSAO() : nullptr;
		if (!client->isBlockSent(block_pos) ||
				(sao && sao->getBasePosition().getDistanceFrom(p) > maxd)) {
			if (far_players)
				far_players->emplace(client_id);
			else
				client->SetBlockNotSent(block_pos);
			continue;
		}
		m_clients.send(client_id, 0, buffer, true);
	}
}

void Server::sendMetadataChanged(
		const std::unordered_set<v3s16> &positions, float far_d_nodes)
{
	NodeMetadataList meta_updates_list(false);
	std::ostringstream os(std::ios::binary);
	const std::vector<session_t> clients = m_clients.getClientIDs();
	ClientInterface::AutoLock clientlock(m_clients);

	for (const session_t peer_id : clients) {
		RemoteClient *client = m_clients.lockedGetClientNoEx(peer_id);
		if (!client)
			continue;
		ServerActiveObject *player = getPlayerSAO(peer_id);
		v3s16 player_pos;
		if (player)
			player_pos = floatToInt(player->getBasePosition(), BS);

		for (const v3s16 pos : positions) {
			NodeMetadata *meta = m_env->getMap().getNodeMetadata(pos);
			if (!meta)
				continue;
			const v3s16 block_pos = getNodeBlockPos(pos);
			if (!client->isBlockSent(block_pos) ||
					player_pos.getDistanceFrom(pos) > far_d_nodes) {
				client->SetBlockNotSent(block_pos);
				continue;
			}
			meta_updates_list.set(pos, meta);
		}
		if (meta_updates_list.size() == 0)
			continue;

		os.str("");
		meta_updates_list.serialize(os, client->serialization_version, false, true, true);
		const std::string raw = os.str();
		os.str("");
		compressZlib(raw, os);
		MSGPACK_PACKET_INIT((int)TOCLIENT_NODEMETA_CHANGED, 1);
		PACK(TOCLIENT_NODEMETA_CHANGED_DATA, os.str());
		m_clients.send(peer_id, 0, buffer, true);
		meta_updates_list.clear();
	}
}

void Server::SendBlockNoLock(session_t peer_id, MapBlock *block, u8 ver,
		u16 net_proto_version, SerializedBlockCache *cache)
{
	const int compression_level =
			rangelim(g_settings->getS16("map_compression_level_net"), -1, 9);
	std::string serialized;
	const std::string *data = nullptr;
	if (cache) {
		auto it = cache->find({block->getPos(), ver});
		if (it != cache->end())
			data = &it->second;
	}
	if (!data) {
		std::ostringstream os(std::ios::binary);
		block->serialize(os, ver, false, compression_level);
		block->serializeNetworkSpecific(os);
		serialized = os.str();
		data = &serialized;
	}

	MSGPACK_PACKET_INIT((int)TOCLIENT_BLOCKDATA, 3);
	PACK(TOCLIENT_BLOCKDATA_POS, block->getPos());
	PACK(TOCLIENT_BLOCKDATA_DATA, *data);
	PACK(TOCLIENT_BLOCKDATA_STEP, block->far_step);
	m_clients.send(peer_id, 2, buffer, true);
	g_profiler->add("Connection: blocks sent", 1);

	if (cache && data == &serialized)
		(*cache)[{block->getPos(), ver}] = std::move(serialized);
}

void Server::sendMediaAnnouncement(session_t peer_id, const std::string &lang_code)
{
	auto include = [&](const std::string &name, const MediaInfo &info) {
		if (info.no_announce)
			return false;
		if (Translations::isTranslationFileType(name)) {
			const auto file_lang = Translations::getFileLanguage(name);
			return !file_lang.empty() && file_lang == lang_code;
		}
		return true;
	};

	MediaAnnounceList announce_list;
	for (const auto &[name, info] : m_media) {
		if (include(name, info))
			announce_list.emplace_back(name, base64_encode(info.sha1_digest));
	}
	MSGPACK_PACKET_INIT((int)TOCLIENT_ANNOUNCE_MEDIA, 2);
	PACK(TOCLIENT_ANNOUNCE_MEDIA_LIST, announce_list);
	PACK(TOCLIENT_ANNOUNCE_MEDIA_REMOTE_SERVER, g_settings->get("remote_media"));
	m_clients.send(peer_id, 0, buffer, true);
	verbosestream << "Server: Announcing files to id(" << peer_id
				  << "): count=" << announce_list.size() << " size=" << buffer.size()
				  << std::endl;
}

void Server::sendRequestedMedia(
		session_t peer_id, const std::unordered_set<std::string> &tosend)
{
	auto *client = getClient(peer_id, CS_DefinitionsSent);
	assert(client);
	infostream << "Server::sendRequestedMedia(): Sending " << tosend.size()
			   << " files to " << client->getName() << std::endl;

	constexpr size_t bytes_per_bunch = 0xffff;
	MediaData media_data;
	size_t size = 0;
	for (const std::string &name : tosend) {
		auto media = m_media.find(name);
		if (media == m_media.end()) {
			warningstream
					<< "Server::sendRequestedMedia(): Client asked for unknown file \""
					<< name << "\"" << std::endl;
			continue;
		}
		if (!media->second.ephemeral && !client->markMediaSent(name)) {
			warningstream << "Server::sendRequestedMedia(): Client requested \"" << name
						  << "\" before, not sending it again" << std::endl;
			continue;
		}

		std::string contents;
		if (!fs::ReadFile(media->second.path, contents, true))
			continue;
		media_data.emplace_back(name, std::move(contents));
		size += media_data.back().second.size();
		if (size >= bytes_per_bunch) {
			MSGPACK_PACKET_INIT((int)TOCLIENT_MEDIA, 1);
			PACK(TOCLIENT_MEDIA_MEDIA, media_data);
			m_clients.send(peer_id, 2, buffer, true);
			media_data.clear();
			size = 0;
		}
	}
	if (!media_data.empty()) {
		MSGPACK_PACKET_INIT((int)TOCLIENT_MEDIA, 1);
		PACK(TOCLIENT_MEDIA_MEDIA, media_data);
		m_clients.send(peer_id, 2, buffer, true);
	}
}

void Server::SendMinimapModes(
		session_t peer_id, std::vector<MinimapMode> &modes, size_t wanted_mode)
{
	if (!m_env->getPlayer(peer_id))
		return;

	std::vector<std::tuple<u16, std::string, u16, std::string, u16>> serialized_modes;
	serialized_modes.reserve(modes.size());
	for (const auto &mode : modes)
		serialized_modes.emplace_back(
				(u16)mode.type, mode.label, mode.size, mode.texture, mode.scale);

	MSGPACK_PACKET_INIT((int)TOCLIENT_MINIMAP_MODES, 2);
	PACK(TOCLIENT_MINIMAP_MODES_LIST, serialized_modes);
	PACK(TOCLIENT_MINIMAP_MODES_SELECTED, (u16)wanted_mode);
	m_clients.send(peer_id, 0, buffer, true);
}

void Server::sendDetachedInventory(
		Inventory *inventory, const std::string &name, session_t peer_id)
{
	std::string data;
	if (inventory) {
		std::ostringstream os(std::ios::binary);
		inventory->serialize(os);
		inventory->setModified(false);
		data = os.str();
	}

	MSGPACK_PACKET_INIT((int)TOCLIENT_DETACHED_INVENTORY, 3);
	PACK(TOCLIENT_DETACHED_INVENTORY_NAME, name);
	PACK(TOCLIENT_DETACHED_INVENTORY_DATA, data);
	PACK(TOCLIENT_DETACHED_INVENTORY_EXISTS, inventory != nullptr);
	sendToClients(m_clients, peer_id, 0, buffer, true);
}

void Server::sendDetachedInventories(session_t peer_id, bool incremental)
{
	std::string peer_name;
	if (peer_id != PEER_ID_INEXISTENT)
		peer_name = getClient(peer_id, CS_Created)->getName();

	auto send_cb = [this, peer_id](const std::string &name, Inventory *inventory) {
		sendDetachedInventory(inventory, name, peer_id);
	};
	m_inventory_mgr->sendDetachedInventories(peer_name, incremental, send_cb);
}

#endif