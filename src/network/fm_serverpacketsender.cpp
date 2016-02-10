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

void Server::SendMovement(u16 peer_id)
{
	DSTACK(FUNCTION_NAME);

	MSGPACK_PACKET_INIT(TOCLIENT_MOVEMENT, 13);

	PACK(TOCLIENT_MOVEMENT_ACCELERATION_DEFAULT, g_settings->getFloat("movement_acceleration_default") * BS);
	PACK(TOCLIENT_MOVEMENT_ACCELERATION_AIR, g_settings->getFloat("movement_acceleration_air") * BS);
	PACK(TOCLIENT_MOVEMENT_ACCELERATION_FAST, g_settings->getFloat("movement_acceleration_fast") * BS);
	PACK(TOCLIENT_MOVEMENT_SPEED_WALK, g_settings->getFloat("movement_speed_walk") * BS);
	PACK(TOCLIENT_MOVEMENT_SPEED_CROUCH, g_settings->getFloat("movement_speed_crouch") * BS);
	PACK(TOCLIENT_MOVEMENT_SPEED_FAST, g_settings->getFloat("movement_speed_fast") * BS);
	PACK(TOCLIENT_MOVEMENT_SPEED_CLIMB, g_settings->getFloat("movement_speed_climb") * BS);
	PACK(TOCLIENT_MOVEMENT_SPEED_JUMP, g_settings->getFloat("movement_speed_jump") * BS);
	PACK(TOCLIENT_MOVEMENT_LIQUID_FLUIDITY, g_settings->getFloat("movement_liquid_fluidity") * BS);
	PACK(TOCLIENT_MOVEMENT_LIQUID_FLUIDITY_SMOOTH, g_settings->getFloat("movement_liquid_fluidity_smooth") * BS);
	PACK(TOCLIENT_MOVEMENT_LIQUID_SINK, g_settings->getFloat("movement_liquid_sink") * BS);
	PACK(TOCLIENT_MOVEMENT_GRAVITY, g_settings->getFloat("movement_gravity") * BS);
	PACK(TOCLIENT_MOVEMENT_FALL_AERODYNAMICS, g_settings->getFloat("movement_fall_aerodynamics"));

	// Send as reliable
	m_clients.send(peer_id, 0, buffer, true);
}

void Server::SendHP(u16 peer_id, u8 hp)
{
	DSTACK(FUNCTION_NAME);
	std::ostringstream os(std::ios_base::binary);

	MSGPACK_PACKET_INIT(TOCLIENT_HP, 1);
	PACK(TOCLIENT_HP_HP, hp);

	// Send as reliable
	m_clients.send(peer_id, 0, buffer, true);
}

void Server::SendBreath(u16 peer_id, u16 breath)
{
	DSTACK(FUNCTION_NAME);
	MSGPACK_PACKET_INIT(TOCLIENT_BREATH, 1);
	PACK(TOCLIENT_BREATH_BREATH, breath);
	// Send as reliable
	m_clients.send(peer_id, 0, buffer, true);
}

void Server::SendAccessDenied(u16 peer_id, AccessDeniedCode reason, const std::string &custom_reason, bool reconnect)
{
	DSTACK(FUNCTION_NAME);
	MSGPACK_PACKET_INIT(TOCLIENT_ACCESS_DENIED_LEGACY, 3);
	PACK(TOCLIENT_ACCESS_DENIED_CUSTOM_STRING, custom_reason);
	PACK(TOCLIENT_ACCESS_DENIED_REASON, (int)reason);
	PACK(TOCLIENT_ACCESS_DENIED_RECONNECT, reconnect);

	// Send as reliable
	m_clients.send(peer_id, 0, buffer, true);
}

void Server::SendDeathscreen(u16 peer_id,bool set_camera_point_target,
		v3f camera_point_target)
{
	DSTACK(FUNCTION_NAME);

	MSGPACK_PACKET_INIT(TOCLIENT_DEATHSCREEN, 2);
	PACK(TOCLIENT_DEATHSCREEN_SET_CAMERA, set_camera_point_target);
	PACK(TOCLIENT_DEATHSCREEN_CAMERA_POINT, camera_point_target);

	// Send as reliable
	m_clients.send(peer_id, 0, buffer, true);
}

void Server::SendItemDef(u16 peer_id,
		IItemDefManager *itemdef, u16 protocol_version)
{
	DSTACK(FUNCTION_NAME);
	MSGPACK_PACKET_INIT(TOCLIENT_ITEMDEF, 1);

	auto client = m_clients.getClient(peer_id, CS_InitDone);
	if (!client)
		return;

	if (client->net_proto_version_fm >= 2) {
		PACK_ZIP(TOCLIENT_ITEMDEF_DEFINITIONS_ZIP, *itemdef);
	} else {
		PACK(TOCLIENT_ITEMDEF_DEFINITIONS, *itemdef);
	}

	m_clients.send(peer_id, 0, buffer, true);
}

void Server::SendNodeDef(u16 peer_id,
		INodeDefManager *nodedef, u16 protocol_version)
{
	DSTACK(FUNCTION_NAME);

	MSGPACK_PACKET_INIT(TOCLIENT_NODEDEF, 1);

	auto client = m_clients.getClient(peer_id, CS_InitDone);
	if (!client)
		return;
	if (client->net_proto_version_fm >= 2) {
		PACK_ZIP(TOCLIENT_NODEDEF_DEFINITIONS_ZIP, *nodedef);
	} else {
		PACK(TOCLIENT_NODEDEF_DEFINITIONS, *nodedef);
	}

	// Send as reliable
	m_clients.send(peer_id, 0, buffer, true);
}

/* not merged to mt
void Server::SendAnimations(u16 peer_id)
{
	DSTACK(FUNCTION_NAME);

	MSGPACK_PACKET_INIT(TOCLIENT_ANIMATIONS, 8);
	PACK(TOCLIENT_ANIMATIONS_DEFAULT_START, g_settings->getFloat("animation_default_start"));
	PACK(TOCLIENT_ANIMATIONS_DEFAULT_STOP, g_settings->getFloat("animation_default_stop"));
	PACK(TOCLIENT_ANIMATIONS_WALK_START, g_settings->getFloat("animation_walk_start"));
	PACK(TOCLIENT_ANIMATIONS_WALK_STOP, g_settings->getFloat("animation_walk_stop"));
	PACK(TOCLIENT_ANIMATIONS_DIG_START, g_settings->getFloat("animation_dig_start"));
	PACK(TOCLIENT_ANIMATIONS_DIG_STOP, g_settings->getFloat("animation_dig_stop"));
	PACK(TOCLIENT_ANIMATIONS_WD_START, g_settings->getFloat("animation_walk_start"));
	PACK(TOCLIENT_ANIMATIONS_WD_STOP, g_settings->getFloat("animation_walk_stop"));

	// Send as reliable
	m_clients.send(peer_id, 0, buffer, true);
}
*/

/*
	Non-static send methods
*/

void Server::SendInventory(PlayerSAO* playerSAO)
{
	DSTACK(FUNCTION_NAME);

	UpdateCrafting(playerSAO->getPlayer());

	/*
		Serialize it
	*/

	std::ostringstream os;
	playerSAO->getInventory()->serialize(os);

	std::string s = os.str();

	MSGPACK_PACKET_INIT(TOCLIENT_INVENTORY, 1);
	PACK(TOCLIENT_INVENTORY_DATA, s);

	// Send as reliable
	m_clients.send(playerSAO->getPeerID(), 0, buffer, true);
}

void Server::SendChatMessage(u16 peer_id, const std::string &message)
{
	DSTACK(FUNCTION_NAME);

	MSGPACK_PACKET_INIT(TOCLIENT_CHAT_MESSAGE, 1);
	PACK(TOCLIENT_CHAT_MESSAGE_DATA, message);

	if (peer_id != PEER_ID_INEXISTENT)
	{
		// Send as reliable
		m_clients.send(peer_id, 0, buffer, true);
	}
	else
	{
		m_clients.sendToAll(0,buffer,true);
	}
}

void Server::SendShowFormspecMessage(u16 peer_id, const std::string &formspec,
                                     const std::string &formname)
{
	DSTACK(FUNCTION_NAME);

	MSGPACK_PACKET_INIT(TOCLIENT_SHOW_FORMSPEC, 2);
	PACK(TOCLIENT_SHOW_FORMSPEC_DATA, FORMSPEC_VERSION_STRING + formspec);
	PACK(TOCLIENT_SHOW_FORMSPEC_NAME, formname);

	// Send as reliable
	m_clients.send(peer_id, 0, buffer, true);
}

// Spawns a particle on peer with peer_id
void Server::SendSpawnParticle(u16 peer_id, v3f pos, v3f velocity, v3f acceleration,
				float expirationtime, float size, bool collisiondetection,
				bool vertical, std::string texture)
{
	DSTACK(FUNCTION_NAME);

	MSGPACK_PACKET_INIT(TOCLIENT_SPAWN_PARTICLE, 8);
	PACK(TOCLIENT_SPAWN_PARTICLE_POS, pos);
	PACK(TOCLIENT_SPAWN_PARTICLE_VELOCITY, velocity);
	PACK(TOCLIENT_SPAWN_PARTICLE_ACCELERATION, acceleration);
	PACK(TOCLIENT_SPAWN_PARTICLE_EXPIRATIONTIME, expirationtime);
	PACK(TOCLIENT_SPAWN_PARTICLE_SIZE, size);
	PACK(TOCLIENT_SPAWN_PARTICLE_COLLISIONDETECTION, collisiondetection);
	PACK(TOCLIENT_SPAWN_PARTICLE_VERTICAL, vertical);
	PACK(TOCLIENT_SPAWN_PARTICLE_TEXTURE, texture);

	if (peer_id != PEER_ID_INEXISTENT)
	{
	// Send as reliable
		m_clients.send(peer_id, 0, buffer, true);
	}
	else
	{
		m_clients.sendToAll(0,buffer,true);
	}
}

// Adds a ParticleSpawner on peer with peer_id
void Server::SendAddParticleSpawner(u16 peer_id, u16 amount, float spawntime, v3f minpos, v3f maxpos,
	v3f minvel, v3f maxvel, v3f minacc, v3f maxacc, float minexptime, float maxexptime,
	float minsize, float maxsize, bool collisiondetection, bool vertical, std::string texture, u32 id)
{
	DSTACK(FUNCTION_NAME);

	MSGPACK_PACKET_INIT(TOCLIENT_ADD_PARTICLESPAWNER, 16);
	PACK(TOCLIENT_ADD_PARTICLESPAWNER_AMOUNT, amount);
	PACK(TOCLIENT_ADD_PARTICLESPAWNER_SPAWNTIME, spawntime);
	PACK(TOCLIENT_ADD_PARTICLESPAWNER_MINPOS, minpos);
	PACK(TOCLIENT_ADD_PARTICLESPAWNER_MAXPOS, maxpos);
	PACK(TOCLIENT_ADD_PARTICLESPAWNER_MINVEL, minvel);
	PACK(TOCLIENT_ADD_PARTICLESPAWNER_MAXVEL, maxvel);
	PACK(TOCLIENT_ADD_PARTICLESPAWNER_MINACC, minacc);
	PACK(TOCLIENT_ADD_PARTICLESPAWNER_MAXACC, maxacc);
	PACK(TOCLIENT_ADD_PARTICLESPAWNER_MINEXPTIME, minexptime);
	PACK(TOCLIENT_ADD_PARTICLESPAWNER_MAXEXPTIME, maxexptime);
	PACK(TOCLIENT_ADD_PARTICLESPAWNER_MINSIZE, minsize);
	PACK(TOCLIENT_ADD_PARTICLESPAWNER_MAXSIZE, maxsize);
	PACK(TOCLIENT_ADD_PARTICLESPAWNER_COLLISIONDETECTION, collisiondetection);
	PACK(TOCLIENT_ADD_PARTICLESPAWNER_TEXTURE, texture);
	PACK(TOCLIENT_ADD_PARTICLESPAWNER_VERTICAL, vertical);
	PACK(TOCLIENT_ADD_PARTICLESPAWNER_ID, id);

	if (peer_id != PEER_ID_INEXISTENT)
	{
		// Send as reliable
		m_clients.send(peer_id, 0, buffer, true);
	}
	else {
		m_clients.sendToAll(0,buffer,true);
	}
}

void Server::SendDeleteParticleSpawner(u16 peer_id, u32 id)
{
	DSTACK(FUNCTION_NAME);

	MSGPACK_PACKET_INIT(TOCLIENT_DELETE_PARTICLESPAWNER, 1);
	PACK(TOCLIENT_DELETE_PARTICLESPAWNER_ID, id);

	if (peer_id != PEER_ID_INEXISTENT) {
		// Send as reliable
		m_clients.send(peer_id, 0, buffer, true);
	}
	else {
		m_clients.sendToAll(0,buffer,true);
	}

}

void Server::SendHUDAdd(u16 peer_id, u32 id, HudElement *form)
{
	MSGPACK_PACKET_INIT(TOCLIENT_HUDADD, 13);
	PACK(TOCLIENT_HUDADD_ID, id);
	PACK(TOCLIENT_HUDADD_TYPE, (int)form->type);
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

	// Send as reliable
	m_clients.send(peer_id, 1, buffer, true);
}

void Server::SendHUDRemove(u16 peer_id, u32 id)
{
	MSGPACK_PACKET_INIT(TOCLIENT_HUDRM, 1);
	PACK(TOCLIENT_HUDRM_ID, id);

	// Send as reliable

	m_clients.send(peer_id, 1, buffer, true);
}

void Server::SendHUDChange(u16 peer_id, u32 id, HudElementStat stat, void *value)
{
	MSGPACK_PACKET_INIT(TOCLIENT_HUDCHANGE, 3);
	PACK(TOCLIENT_HUDCHANGE_ID, id);
	PACK(TOCLIENT_HUDCHANGE_STAT, (int)stat);

	switch (stat) {
		case HUD_STAT_POS:
		case HUD_STAT_SCALE:
		case HUD_STAT_ALIGN:
		case HUD_STAT_OFFSET:
			PACK(TOCLIENT_HUDCHANGE_V2F, *(v2f*)value);
			break;
		case HUD_STAT_NAME:
		case HUD_STAT_TEXT:
			PACK(TOCLIENT_HUDCHANGE_STRING, *(std::string*)value);
			break;
		case HUD_STAT_WORLD_POS:
			PACK(TOCLIENT_HUDCHANGE_V3F, *(v3f*)value);
			break;
		case HUD_STAT_SIZE:
			PACK(TOCLIENT_HUDCHANGE_V2S32, *(v2s32 *)value);
			break;
		case HUD_STAT_NUMBER:
		case HUD_STAT_ITEM:
		case HUD_STAT_DIR:
		default:
			PACK(TOCLIENT_HUDCHANGE_U32, *(u32*)value);
			break;
	}

	// Send as reliable
	m_clients.send(peer_id, 0, buffer, true);
}

void Server::SendHUDSetFlags(u16 peer_id, u32 flags, u32 mask)
{
	MSGPACK_PACKET_INIT(TOCLIENT_HUD_SET_FLAGS, 2);
	//////////////////////////// compatibility code to be removed //////////////
	// ?? flags &= ~(HUD_FLAG_HEALTHBAR_VISIBLE | HUD_FLAG_BREATHBAR_VISIBLE);
	PACK(TOCLIENT_HUD_SET_FLAGS_FLAGS, flags);
	PACK(TOCLIENT_HUD_SET_FLAGS_MASK, mask);

	// Send as reliable
	m_clients.send(peer_id, 0, buffer, true);
}

void Server::SendHUDSetParam(u16 peer_id, u16 param, const std::string &value)
{
	MSGPACK_PACKET_INIT(TOCLIENT_HUD_SET_PARAM, 2);
	PACK(TOCLIENT_HUD_SET_PARAM_ID, param);
	PACK(TOCLIENT_HUD_SET_PARAM_VALUE, value);

	// Send as reliable
	m_clients.send(peer_id, 0, buffer, true);
}

void Server::SendSetSky(u16 peer_id, const video::SColor &bgcolor,
		const std::string &type, const std::vector<std::string> &params)
{
	MSGPACK_PACKET_INIT(TOCLIENT_SET_SKY, 3);
	PACK(TOCLIENT_SET_SKY_COLOR, bgcolor);
	PACK(TOCLIENT_SET_SKY_TYPE, type);
	PACK(TOCLIENT_SET_SKY_PARAMS, params);

	// Send as reliable
	m_clients.send(peer_id, 0, buffer, true);
}

void Server::SendOverrideDayNightRatio(u16 peer_id, bool do_override,
		float ratio)
{
	MSGPACK_PACKET_INIT(TOCLIENT_OVERRIDE_DAY_NIGHT_RATIO, 2);
	PACK(TOCLIENT_OVERRIDE_DAY_NIGHT_RATIO_DO, do_override);
	PACK(TOCLIENT_OVERRIDE_DAY_NIGHT_RATIO_VALUE, ratio);

	// Send as reliable
	m_clients.send(peer_id, 0, buffer, true);
}

void Server::SendTimeOfDay(u16 peer_id, u16 time, f32 time_speed)
{
	DSTACK(FUNCTION_NAME);

	// Make packet
	MSGPACK_PACKET_INIT(TOCLIENT_TIME_OF_DAY, 2);
	PACK(TOCLIENT_TIME_OF_DAY_TIME, time);
	PACK(TOCLIENT_TIME_OF_DAY_TIME_SPEED, time_speed);

	if (peer_id == PEER_ID_INEXISTENT) {
		m_clients.sendToAll(0,buffer,true);
	}
	else {
		// Send as reliable
		m_clients.send(peer_id, 0, buffer, true);
	}
}

void Server::SendPlayerHP(u16 peer_id)
{
	DSTACK(FUNCTION_NAME);
	PlayerSAO *playersao = getPlayerSAO(peer_id);
	if (!playersao)
		return;
	SendHP(peer_id, playersao->getHP());
	m_script->player_event(playersao,"health_changed");

	// Send to other clients
	std::string str = gob_cmd_punched(playersao->readDamage(), playersao->getHP());
	ActiveObjectMessage aom(playersao->getId(), true, str);
	playersao->m_messages_out.push(aom);
}

void Server::SendPlayerBreath(u16 peer_id)
{
	DSTACK(FUNCTION_NAME);
	PlayerSAO *playersao = getPlayerSAO(peer_id);
	if (!playersao)
		return;
	m_script->player_event(playersao, "breath_changed");
	SendBreath(peer_id, playersao->getBreath());
}

void Server::SendMovePlayer(u16 peer_id)
{
	DSTACK(FUNCTION_NAME);
	Player *player = m_env->getPlayer(peer_id);
	if (!player)
		return;

	MSGPACK_PACKET_INIT(TOCLIENT_MOVE_PLAYER, 3);
	PACK(TOCLIENT_MOVE_PLAYER_POS, player->getPosition());
	PACK(TOCLIENT_MOVE_PLAYER_PITCH, player->getPitch());
	PACK(TOCLIENT_MOVE_PLAYER_YAW, player->getYaw());
	//PACK(TOCLIENT_MOVE_PLAYER_SPEED, player->getSpeed());
	// Send as reliable
	m_clients.send(peer_id, 0, buffer, true);
}

void Server::SendPunchPlayer(u16 peer_id, v3f speed)
{
	DSTACK(FUNCTION_NAME);
	Player *player = m_env->getPlayer(peer_id);
	if (!player)
		return;

	MSGPACK_PACKET_INIT(TOCLIENT_PUNCH_PLAYER, 1);
	PACK(TOCLIENT_PUNCH_PLAYER_SPEED, speed);
	// Send as reliable
	m_clients.send(peer_id, 0, buffer, true);
}

void Server::SendLocalPlayerAnimations(u16 peer_id, v2s32 animation_frames[4], f32 animation_speed)
{
	MSGPACK_PACKET_INIT(TOCLIENT_LOCAL_PLAYER_ANIMATIONS, 5);
	PACK(TOCLIENT_LOCAL_PLAYER_ANIMATIONS_IDLE, animation_frames[0]);
	PACK(TOCLIENT_LOCAL_PLAYER_ANIMATIONS_WALK, animation_frames[1]);
	PACK(TOCLIENT_LOCAL_PLAYER_ANIMATIONS_DIG, animation_frames[2]);
	PACK(TOCLIENT_LOCAL_PLAYER_ANIMATIONS_WALKDIG, animation_frames[3]);
	PACK(TOCLIENT_LOCAL_PLAYER_ANIMATIONS_FRAME_SPEED, animation_speed);

	// Send as reliable
	m_clients.send(peer_id, 0, buffer, true);
}

void Server::SendEyeOffset(u16 peer_id, v3f first, v3f third)
{
	MSGPACK_PACKET_INIT(TOCLIENT_EYE_OFFSET, 2);
	PACK(TOCLIENT_EYE_OFFSET_FIRST, first);
	PACK(TOCLIENT_EYE_OFFSET_THIRD, third);
	// Send as reliable
	m_clients.send(peer_id, 0, buffer, true);
}
void Server::SendPlayerPrivileges(u16 peer_id)
{
	Player *player = m_env->getPlayer(peer_id);
	if (!player)
		return;

	if(player->peer_id == PEER_ID_INEXISTENT)
		return;

	std::set<std::string> privs;
	m_script->getAuth(player->getName(), NULL, &privs);

	MSGPACK_PACKET_INIT(TOCLIENT_PRIVILEGES, 1);
	PACK(TOCLIENT_PRIVILEGES_PRIVILEGES, privs);

	// Send as reliable
	m_clients.send(peer_id, 0, buffer, true);
}

void Server::SendPlayerInventoryFormspec(u16 peer_id)
{
	Player *player = m_env->getPlayer(peer_id);
	if (!player)
		return;

	if(player->peer_id == PEER_ID_INEXISTENT)
		return;

	MSGPACK_PACKET_INIT(TOCLIENT_INVENTORY_FORMSPEC, 1);
	PACK(TOCLIENT_INVENTORY_FORMSPEC_DATA, FORMSPEC_VERSION_STRING + player->inventory_formspec);

	// Send as reliable
	m_clients.send(peer_id, 0, buffer, true);
}

void Server::SendActiveObjectMessages(u16 peer_id, const ActiveObjectMessages &datas, bool reliable)
{
	MSGPACK_PACKET_INIT(TOCLIENT_ACTIVE_OBJECT_MESSAGES, 1);
	PACK(TOCLIENT_ACTIVE_OBJECT_MESSAGES_MESSAGES, datas);

	// Send as reliable
	m_clients.send(peer_id, 0, buffer, reliable);
}


s32 Server::playSound(const SimpleSoundSpec &spec,
		const ServerSoundParams &params)
{
	// Find out initial position of sound
	bool pos_exists = false;
	v3f pos = params.getPos(m_env, &pos_exists);
	// If position is not found while it should be, cancel sound
	if(pos_exists != (params.type != ServerSoundParams::SSP_LOCAL))
		return -1;

	// Filter destination clients
	std::vector<u16> dst_clients;
	if(params.to_player != "")
	{
		Player *player = m_env->getPlayer(params.to_player.c_str());
		if(!player){
			infostream<<"Server::playSound: Player \""<<params.to_player
					<<"\" not found"<<std::endl;
			return -1;
		}
		if(player->peer_id == PEER_ID_INEXISTENT){
			infostream<<"Server::playSound: Player \""<<params.to_player
					<<"\" not connected"<<std::endl;
			return -1;
		}
		dst_clients.push_back(player->peer_id);
	}
	else
	{
		std::vector<u16> clients = m_clients.getClientIDs();

		for(auto
				i = clients.begin(); i != clients.end(); ++i)
		{
			Player *player = m_env->getPlayer(*i);
			if(!player)
				continue;
			if(pos_exists) {
				if(player->getPosition().getDistanceFrom(pos) >
						params.max_hear_distance)
					continue;
			}
			dst_clients.push_back(*i);
		}
	}
	if(dst_clients.empty())
		return -1;

	// Create the sound
	s32 id = m_next_sound_id++;
	// The sound will exist as a reference in m_playing_sounds
	m_playing_sounds[id] = ServerPlayingSound();
	ServerPlayingSound &psound = m_playing_sounds[id];
	psound.params = params;
	for(auto i = dst_clients.begin();
			i != dst_clients.end(); i++)
		psound.clients.insert(*i);
	// Create packet
	MSGPACK_PACKET_INIT(TOCLIENT_PLAY_SOUND, 7);
	PACK(TOCLIENT_PLAY_SOUND_ID, id);
	PACK(TOCLIENT_PLAY_SOUND_NAME, spec.name);
	PACK(TOCLIENT_PLAY_SOUND_GAIN, spec.gain * params.gain);
	PACK(TOCLIENT_PLAY_SOUND_TYPE, (u8)params.type);
	PACK(TOCLIENT_PLAY_SOUND_POS, pos);
	PACK(TOCLIENT_PLAY_SOUND_OBJECT_ID, params.object);
	PACK(TOCLIENT_PLAY_SOUND_LOOP, params.loop);
	// Send
	for(auto i = dst_clients.begin();
			i != dst_clients.end(); i++){
		// Send as reliable
		m_clients.send(*i, 0, buffer, true);
	}
	return id;
}
void Server::stopSound(s32 handle)
{
	// Get sound reference
	std::map<s32, ServerPlayingSound>::iterator i =
			m_playing_sounds.find(handle);
	if(i == m_playing_sounds.end())
		return;
	ServerPlayingSound &psound = i->second;
	// Create packet
	MSGPACK_PACKET_INIT(TOCLIENT_STOP_SOUND, 1);
	PACK(TOCLIENT_STOP_SOUND_ID, handle);
	// Send
	for(std::set<u16>::iterator i = psound.clients.begin();
			i != psound.clients.end(); i++){
		// Send as reliable
		m_clients.send(*i, 0, buffer, true);
	}
	// Remove sound reference
	m_playing_sounds.erase(i);
}


void Server::sendRemoveNode(v3s16 p, u16 ignore_id,
	std::vector<u16> *far_players, float far_d_nodes)
{
	float maxd = far_d_nodes*BS;
	v3f p_f = intToFloat(p, BS);

	// Create packet
	MSGPACK_PACKET_INIT(TOCLIENT_REMOVENODE, 1);
	PACK(TOCLIENT_REMOVENODE_POS, p);

	std::vector<u16> clients = m_clients.getClientIDs();
	for(auto
		i = clients.begin();
		i != clients.end(); ++i)
	{
		if(far_players) {
			// Get player
			Player *player = m_env->getPlayer(*i);
			if(player) {
				// If player is far away, only set modified blocks not sent
				v3f player_pos = player->getPosition();
				if(player_pos.getDistanceFrom(p_f) > maxd) {
					far_players->push_back(*i);
					continue;
				}
			}
		}

		// Send as reliable
		m_clients.send(*i, 0, buffer, true);
	}
}

void Server::sendAddNode(v3s16 p, MapNode n, u16 ignore_id,
		std::vector<u16> *far_players, float far_d_nodes,
		bool remove_metadata)
{
	float maxd = far_d_nodes*BS;
	v3f p_f = intToFloat(p, BS);

	std::vector<u16> clients = m_clients.getClientIDs();
	for(auto
				i = clients.begin();
		i != clients.end(); ++i)
	{

		if(far_players) {
			// Get player
			Player *player = m_env->getPlayer(*i);
			if(player)
			{
				// If player is far away, only set modified blocks not sent
				v3f player_pos = player->getPosition();
				if(player_pos.getDistanceFrom(p_f) > maxd) {
					far_players->push_back(*i);
					continue;
				}
			}
		}
		SharedBuffer<u8> reply(0);
		RemoteClient* client = m_clients.lockedGetClientNoEx(*i);
		if (client != 0)
		{
			// Create packet
			MSGPACK_PACKET_INIT(TOCLIENT_ADDNODE, 3);
			PACK(TOCLIENT_ADDNODE_POS, p);
			PACK(TOCLIENT_ADDNODE_NODE, n);
			PACK(TOCLIENT_ADDNODE_REMOVE_METADATA, remove_metadata);

			m_clients.send(*i, 0, buffer, true);
		}
	}
}

void Server::SendBlockNoLock(u16 peer_id, MapBlock *block, u8 ver, u16 net_proto_version)
{
	DSTACK(FUNCTION_NAME);
	bool reliable = 1;

	g_profiler->add("Connection: blocks sent", 1);

	MSGPACK_PACKET_INIT(TOCLIENT_BLOCKDATA, 8);
	PACK(TOCLIENT_BLOCKDATA_POS, block->getPos());

	std::ostringstream os(std::ios_base::binary);

	auto client = m_clients.getClient(peer_id);
	if (!client)
		return;
	block->serialize(os, ver, false, client->net_proto_version_fm >= 1);
	PACK(TOCLIENT_BLOCKDATA_DATA, os.str());

	PACK(TOCLIENT_BLOCKDATA_HEAT, (s16)(block->heat + block->heat_add));
	PACK(TOCLIENT_BLOCKDATA_HUMIDITY, (s16)(block->humidity + block->humidity_add));
	PACK(TOCLIENT_BLOCKDATA_STEP, (s8)1);
	PACK(TOCLIENT_BLOCKDATA_CONTENT_ONLY, block->content_only);
	PACK(TOCLIENT_BLOCKDATA_CONTENT_ONLY_PARAM1, block->content_only_param1);
	PACK(TOCLIENT_BLOCKDATA_CONTENT_ONLY_PARAM2, block->content_only_param2);

	//MutexAutoLock lock(m_env_mutex);
	/*
		Send packet
	*/
	m_clients.send(peer_id, 2, buffer, reliable);
}

void Server::sendMediaAnnouncement(u16 peer_id)
{
	DSTACK(FUNCTION_NAME);

	MediaAnnounceList announce_list;

	for(std::map<std::string, MediaInfo>::iterator i = m_media.begin();
			i != m_media.end(); i++)
		announce_list.push_back(std::make_pair(i->first, i->second.sha1_digest));

	MSGPACK_PACKET_INIT(TOCLIENT_ANNOUNCE_MEDIA, 2);
	PACK(TOCLIENT_ANNOUNCE_MEDIA_LIST, announce_list);
	PACK(TOCLIENT_ANNOUNCE_MEDIA_REMOTE_SERVER, g_settings->get("remote_media"));

	// Send as reliable
	m_clients.send(peer_id, 0, buffer, true);
}

void Server::sendRequestedMedia(u16 peer_id,
		const std::vector<std::string> &tosend)
{
	DSTACK(FUNCTION_NAME);

	verbosestream<<"Server::sendRequestedMedia(): "
			<<"Sending files to client"<<std::endl;

	/* Read files */
	// TODO: optimize
	MediaData media_data;
	u32 size = 0;

	for(auto i = tosend.begin();
			i != tosend.end(); ++i) {
		const std::string &name = *i;

		if(m_media.find(name) == m_media.end()) {
			errorstream<<"Server::sendRequestedMedia(): Client asked for "
					<<"unknown file \""<<(name)<<"\""<<std::endl;
			continue;
		}

		//TODO get path + name
		std::string tpath = m_media[name].path;

		// Read data
		std::ifstream fis(tpath.c_str(), std::ios_base::binary);
		if(fis.good() == false){
			errorstream<<"Server::sendRequestedMedia(): Could not open \""
					<<tpath<<"\" for reading"<<std::endl;
			continue;
		}
		std::string contents;
		fis.seekg(0, std::ios::end);
		contents.resize(fis.tellg());
		fis.seekg(0, std::ios::beg);
		fis.read(&contents[0], contents.size());
		media_data.push_back(std::make_pair(name, contents));
		size += contents.size();
		if (size > 0xffff) {
			MSGPACK_PACKET_INIT(TOCLIENT_MEDIA, 1);
			PACK(TOCLIENT_MEDIA_MEDIA, media_data);
			m_clients.send(peer_id, 2, buffer, true);
			media_data.clear();
			size = 0;
		}
	}

	if (!media_data.empty()) {
		MSGPACK_PACKET_INIT(TOCLIENT_MEDIA, 1);
		PACK(TOCLIENT_MEDIA_MEDIA, media_data);
		m_clients.send(peer_id, 2, buffer, true);
	}
}

void Server::sendDetachedInventory(const std::string &name, u16 peer_id)
{
	if(m_detached_inventories.count(name) == 0){
		errorstream<<FUNCTION_NAME<<": \""<<name<<"\" not found"<<std::endl;
		return;
	}
	Inventory *inv = m_detached_inventories[name];

	std::ostringstream os(std::ios_base::binary);
	inv->serialize(os);

	MSGPACK_PACKET_INIT(TOCLIENT_DETACHED_INVENTORY, 2);
	PACK(TOCLIENT_DETACHED_INVENTORY_NAME, name);
	PACK(TOCLIENT_DETACHED_INVENTORY_DATA, os.str());

	if (peer_id != PEER_ID_INEXISTENT)
	{
		// Send as reliable
		m_clients.send(peer_id, 0, buffer, true);
	}
	else
	{
		m_clients.sendToAll(0,buffer,true);
	}
}
