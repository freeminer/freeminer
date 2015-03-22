/*
client.cpp
Copyright (C) 2013 celeron55, Perttu Ahola <celeron55@gmail.com>
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

#include "client.h"

#include "util/base64.h"
#include "clientmedia.h"
#include "log.h"
#include "map.h"
#include "mapsector.h"
#include "nodedef.h"
#include "serialization.h"
#include "server.h"
#include "strfnd.h"
#include "network/clientopcodes.h"
#include "util/serialize.h"

#include "settings.h"
#include "emerge.h"


// TODO! split to packethandlers


/*
	sender_peer_id given to this shall be quaranteed to be a valid peer
*/
void Client::ProcessData(u8 *data, u32 datasize, u16 sender_peer_id) {
	DSTACK(__FUNCTION_NAME);

	// Ignore packets that don't even fit a command
	if (datasize < 2) {
		m_packetcounter.add(60000);
		return;
	}

	int command;
	MsgpackPacket packet;
	msgpack::unpacked msg;

	if (!con::parse_msgpack_packet(data, datasize, &packet, &command, &msg)) {
		// invalid packet
		return;
	}

	//infostream<<"Client: received command="<<command<<std::endl;
	m_packetcounter.add((u16)command);

	/*
		If this check is removed, be sure to change the queue
		system to know the ids
	*/
	if(sender_peer_id != PEER_ID_SERVER)
	{
		infostream<<"Client::ProcessData(): Discarding data not "
				"coming from server: peer_id="<<sender_peer_id
				<<std::endl;
		return;
	}

	u8 ser_version = m_server_ser_ver;

	//infostream<<"Client received command="<<(int)command<<std::endl;

	if(command == TOCLIENT_INIT)
	{
		u8 deployed;
		packet[TOCLIENT_INIT_DEPLOYED].convert(&deployed);

		infostream<<"Client: TOCLIENT_INIT received with "
				"deployed="<<((int)deployed&0xff)<<std::endl;

		if(!ser_ver_supported(deployed))
		{
			infostream<<"Client: TOCLIENT_INIT: Server sent "
					<<"unsupported ser_fmt_ver"<<std::endl;
			return;
		}

		m_server_ser_ver = deployed;

		// Set player position
		Player *player = m_env.getLocalPlayer();
		if(!player)
			return;

		packet[TOCLIENT_INIT_SEED].convert(&m_map_seed);
		infostream<<"Client: received map seed: "<<m_map_seed<<std::endl;

		packet[TOCLIENT_INIT_STEP].convert(&m_recommended_send_interval);
		infostream<<"Client: received recommended send interval "
				<<m_recommended_send_interval<<std::endl;

		// TOCLIENT_INIT_POS

		if (m_localserver) {
			Settings settings;
			packet[TOCLIENT_INIT_MAP_PARAMS].convert(&settings);
			m_localserver->getEmergeManager()->params.load(settings);
		}

		// Reply to server
		MSGPACK_PACKET_INIT(TOSERVER_INIT2, 0);
		m_con.Send(PEER_ID_SERVER, 1, buffer, true);

		m_state = LC_Init;

		return;
	}

	if(command == TOCLIENT_ACCESS_DENIED)
	{
		// The server didn't like our password. Note, this needs
		// to be processed even if the serialisation format has
		// not been agreed yet, the same as TOCLIENT_INIT.
		m_access_denied = true;
		packet[TOCLIENT_ACCESS_DENIED_CUSTOM_STRING].convert(&m_access_denied_reason);
		return;
	}

	if(ser_version == SER_FMT_VER_INVALID)
	{
		infostream<<"Client: Server serialization"
				" format invalid or not initialized."
				" Skipping incoming command="<<command<<std::endl;
		return;
	}

	/*
	  Handle runtime commands
	*/
	// there's no sane reason why we shouldn't have a player and
	// almost everyone needs a player reference
	Player *player = m_env.getLocalPlayer();
	assert(player != NULL);

	if(command == TOCLIENT_REMOVENODE)
	{
		v3s16 p = packet[TOCLIENT_REMOVENODE_POS].as<v3s16>();
		removeNode(p, 2); //use light from top node
	}
	else if(command == TOCLIENT_ADDNODE)
	{
		v3s16 p = packet[TOCLIENT_ADDNODE_POS].as<v3s16>();
		MapNode n = packet[TOCLIENT_ADDNODE_NODE].as<MapNode>();
		bool remove_metadata = packet[TOCLIENT_ADDNODE_REMOVE_METADATA].as<bool>();

		addNode(p, n, remove_metadata, 2); //fast add
	}
	else if(command == TOCLIENT_BLOCKDATA)
	{
		v3s16 p = packet[TOCLIENT_BLOCKDATA_POS].as<v3s16>();
		s8 step = 1;
		packet[TOCLIENT_BLOCKDATA_STEP].convert(&step);
		if (step == 1) {

		std::istringstream istr(packet[TOCLIENT_BLOCKDATA_DATA].as<std::string>(), std::ios_base::binary);

		MapBlock *block;

		block = m_env.getMap().getBlockNoCreateNoEx(p);
		bool new_block = !block;
		if (new_block)
			block = new MapBlock(&m_env.getMap(), p, this);

		block->deSerialize(istr, ser_version, false);
		s32 h; // for convert to atomic
		packet[TOCLIENT_BLOCKDATA_HEAT].convert(&h);
		block->heat = h;
		packet[TOCLIENT_BLOCKDATA_HUMIDITY].convert(&h);
		block->humidity = h;


		if (packet.count(TOCLIENT_BLOCKDATA_CONTENT_ONLY))
			block->content_only = packet[TOCLIENT_BLOCKDATA_CONTENT_ONLY].as<content_t>();


		if (m_localserver != NULL) {
			m_localserver->getMap().saveBlock(block);
		}

		if (new_block)
			if (!m_env.getMap().insertBlock(block))
				delete block;

		/*
			//Add it to mesh update queue and set it to be acknowledged after update.
		*/
		//infostream<<"Adding mesh update task for received block "<<p<<std::endl;
		if (!block->content_only || block->content_only != CONTENT_AIR) {
			updateMeshTimestampWithEdge(p);
		}

/*
#if !defined(NDEBUG)
		if (m_env.getClientMap().m_block_boundary.size() > 150)
			m_env.getClientMap().m_block_boundary.clear();
		m_env.getClientMap().m_block_boundary[p] = block;
#endif
*/

		}//step

	}
	else if(command == TOCLIENT_INVENTORY)
	{
		std::string datastring = packet[TOCLIENT_INVENTORY_DATA].as<std::string>();
		std::istringstream is(datastring, std::ios_base::binary);
		Player *player = m_env.getLocalPlayer();
		assert(player != NULL);

		player->inventory.deSerialize(is);

		m_inventory_updated = true;

		delete m_inventory_from_server;
		m_inventory_from_server = new Inventory(player->inventory);
		m_inventory_from_server_age = 0.0;
	}
	else if(command == TOCLIENT_TIME_OF_DAY)
	{
		u16 time_of_day = packet[TOCLIENT_TIME_OF_DAY_TIME].as<u16>();
		time_of_day = time_of_day % 24000;
		f32 time_speed = packet[TOCLIENT_TIME_OF_DAY_TIME_SPEED].as<f32>();

		// Update environment
		m_env.setTimeOfDay(time_of_day);
		m_env.setTimeOfDaySpeed(time_speed);
		m_time_of_day_set = true;

		u32 dr = m_env.getDayNightRatio();
		verbosestream<<"Client: time_of_day="<<time_of_day
				<<" time_speed="<<time_speed
				<<" dr="<<dr<<std::endl;
	}
	else if(command == TOCLIENT_CHAT_MESSAGE)
	{
		std::string message = packet[TOCLIENT_CHAT_MESSAGE_DATA].as<std::string>();
		m_chat_queue.push(message);
	}
	else if(command == TOCLIENT_ACTIVE_OBJECT_REMOVE_ADD)
	{
		std::vector<u16> removed_objects;
		packet[TOCLIENT_ACTIVE_OBJECT_REMOVE_ADD_REMOVE].convert(&removed_objects);
		for (size_t i = 0; i < removed_objects.size(); ++i)
			m_env.removeActiveObject(removed_objects[i]);

		std::vector<ActiveObjectAddData> added_objects;
		packet[TOCLIENT_ACTIVE_OBJECT_REMOVE_ADD_ADD].convert(&added_objects);
		for (size_t i = 0; i < added_objects.size(); ++i)
			m_env.addActiveObject(added_objects[i].id, added_objects[i].type, added_objects[i].data);
	}
	else if(command == TOCLIENT_ACTIVE_OBJECT_MESSAGES)
	{
		ActiveObjectMessages messages;
		packet[TOCLIENT_ACTIVE_OBJECT_MESSAGES_MESSAGES].convert(&messages);
		for (size_t i = 0; i < messages.size(); ++i)
			m_env.processActiveObjectMessage(messages[i].first, messages[i].second);
	}
	else if(command == TOCLIENT_MOVEMENT)
	{
		Player *player = m_env.getLocalPlayer();
		packet[TOCLIENT_MOVEMENT_ACCELERATION_DEFAULT].convert(&player->movement_acceleration_default);
		packet[TOCLIENT_MOVEMENT_ACCELERATION_AIR].convert(&player->movement_acceleration_air);
		packet[TOCLIENT_MOVEMENT_ACCELERATION_FAST].convert(&player->movement_acceleration_fast);
		packet[TOCLIENT_MOVEMENT_SPEED_WALK].convert(&player->movement_speed_walk);
		packet[TOCLIENT_MOVEMENT_SPEED_CROUCH].convert(&player->movement_speed_crouch);
		packet[TOCLIENT_MOVEMENT_SPEED_FAST].convert(&player->movement_speed_fast);
		packet[TOCLIENT_MOVEMENT_SPEED_CLIMB].convert(&player->movement_speed_climb);
		packet[TOCLIENT_MOVEMENT_SPEED_JUMP].convert(&player->movement_speed_jump);
		packet[TOCLIENT_MOVEMENT_LIQUID_FLUIDITY].convert(&player->movement_liquid_fluidity);
		packet[TOCLIENT_MOVEMENT_LIQUID_FLUIDITY_SMOOTH].convert(&player->movement_liquid_fluidity_smooth);
		packet[TOCLIENT_MOVEMENT_LIQUID_SINK].convert(&player->movement_liquid_sink);
		packet[TOCLIENT_MOVEMENT_GRAVITY].convert(&player->movement_gravity);
	}
	else if(command == TOCLIENT_HP)
	{
		Player *player = m_env.getLocalPlayer();
		assert(player != NULL);
		u8 oldhp = player->hp;
		u8 hp = packet[TOCLIENT_HP_HP].as<u8>();
		player->hp = hp;

		if(hp < oldhp)
		{
			// Add to ClientEvent queue
			ClientEvent event;
			event.type = CE_PLAYER_DAMAGE;
			event.player_damage.amount = oldhp - hp;
			m_client_event_queue.push(event);
		}
	}
	else if(command == TOCLIENT_BREATH)
	{
		Player *player = m_env.getLocalPlayer();
		player->setBreath(packet[TOCLIENT_BREATH_BREATH].as<u16>()) ;
	}
	else if(command == TOCLIENT_MOVE_PLAYER)
	{
		Player *player = m_env.getLocalPlayer();
		assert(player != NULL);
		v3f pos = packet[TOCLIENT_MOVE_PLAYER_POS].as<v3f>();
		f32 pitch = packet[TOCLIENT_MOVE_PLAYER_PITCH].as<f32>();
		f32 yaw = packet[TOCLIENT_MOVE_PLAYER_YAW].as<f32>();
		player->setPosition(pos);

		infostream<<"Client got TOCLIENT_MOVE_PLAYER"
				<<" pos=("<<pos.X<<","<<pos.Y<<","<<pos.Z<<")"
				<<" pitch="<<pitch
				<<" yaw="<<yaw
				<<std::endl;

		/*
			Add to ClientEvent queue.
			This has to be sent to the main program because otherwise
			it would just force the pitch and yaw values to whatever
			the camera points to.
		*/
		ClientEvent event;
		event.type = CE_PLAYER_FORCE_MOVE;
		event.player_force_move.pitch = pitch;
		event.player_force_move.yaw = yaw;
		m_client_event_queue.push(event);

		// Ignore damage for a few seconds, so that the player doesn't
		// get damage from falling on ground
		m_ignore_damage_timer = 3.0;
	}
	else if(command == TOCLIENT_DEATHSCREEN)
	{
		bool set_camera_point_target = packet[TOCLIENT_DEATHSCREEN_SET_CAMERA].as<bool>();
		v3f camera_point_target = packet[TOCLIENT_DEATHSCREEN_CAMERA_POINT].as<v3f>();

		ClientEvent event;
		event.type = CE_DEATHSCREEN;
		event.deathscreen.set_camera_point_target = set_camera_point_target;
		event.deathscreen.camera_point_target_x = camera_point_target.X;
		event.deathscreen.camera_point_target_y = camera_point_target.Y;
		event.deathscreen.camera_point_target_z = camera_point_target.Z;
		m_client_event_queue.push(event);
	}
	else if(command == TOCLIENT_ANNOUNCE_MEDIA)
	{
		if (m_media_downloader == NULL ||
				m_media_downloader->isStarted()) {
			const char *problem = m_media_downloader ?
				"we already saw another announcement" :
				"all media has been received already";
			errorstream<<"Client: Received media announcement but "
				<<problem<<"!"
				<<std::endl;
			return;
		}

		// Mesh update thread must be stopped while
		// updating content definitions
		//assert(!m_mesh_update_thread.IsRunning());

		MediaAnnounceList announce_list;
		packet[TOCLIENT_ANNOUNCE_MEDIA_LIST].convert(&announce_list);
		for (size_t i = 0; i < announce_list.size(); ++i)
			m_media_downloader->addFile(announce_list[i].first, base64_decode(announce_list[i].second));

		std::vector<std::string> remote_media;
		std::string remote_media_string = packet[TOCLIENT_ANNOUNCE_MEDIA_REMOTE_SERVER].as<std::string>();
		Strfnd sf(remote_media_string);
		while(!sf.atend()) {
			std::string baseurl = trim(sf.next(","));
			if(baseurl != "")
				m_media_downloader->addRemoteServer(baseurl);
		}

		m_media_downloader->step(this);
	}
	else if(command == TOCLIENT_MEDIA)
	{
		MediaData media_data;
		packet[TOCLIENT_MEDIA_MEDIA].convert(&media_data);

		// Mesh update thread must be stopped while
		// updating content definitions
		//assert(!m_mesh_update_thread.IsRunning());

		for(size_t i = 0; i < media_data.size(); ++i)
			m_media_downloader->conventionalTransferDone(
					media_data[i].first, media_data[i].second, this);
	}
	else if(command == TOCLIENT_NODEDEF)
	{
		infostream<<"Client: Received node definitions: packet size: "
				<<datasize<<std::endl;

		// Mesh update thread must be stopped while
		// updating content definitions
		//assert(!m_mesh_update_thread.IsRunning());

		packet[TOCLIENT_NODEDEF_DEFINITIONS].convert(m_nodedef);
		m_nodedef_received = true;
	}
	else if(command == TOCLIENT_ITEMDEF)
	{
		infostream<<"Client: Received item definitions: packet size: "
				<<datasize<<std::endl;

		// Mesh update thread must be stopped while
		// updating content definitions
		//assert(!m_mesh_update_thread.IsRunning());

		packet[TOCLIENT_ITEMDEF_DEFINITIONS].convert(m_itemdef);
		m_itemdef_received = true;
	}
	else if(command == TOCLIENT_PLAY_SOUND)
	{
		s32 server_id = packet[TOCLIENT_PLAY_SOUND_ID].as<s32>();
		std::string name = packet[TOCLIENT_PLAY_SOUND_NAME].as<std::string>();
		float gain = packet[TOCLIENT_PLAY_SOUND_GAIN].as<f32>();
		int type = packet[TOCLIENT_PLAY_SOUND_TYPE].as<u8>(); // 0=local, 1=positional, 2=object
		v3f pos = packet[TOCLIENT_PLAY_SOUND_POS].as<v3f>();
		u16 object_id = packet[TOCLIENT_PLAY_SOUND_OBJECT_ID].as<u16>();
		bool loop = packet[TOCLIENT_PLAY_SOUND_LOOP].as<bool>();
		// Start playing
		int client_id = -1;
		switch(type){
		case 0: // local
			client_id = m_sound->playSound(name, loop, gain);
			break;
		case 1: // positional
			client_id = m_sound->playSoundAt(name, loop, gain, pos);
			break;
		case 2: { // object
			ClientActiveObject *cao = m_env.getActiveObject(object_id);
			if(cao)
				pos = cao->getPosition();
			client_id = m_sound->playSoundAt(name, loop, gain, pos);
			// TODO: Set up sound to move with object
			break; }
		default:
			break;
		}
		if(client_id != -1){
			m_sounds_server_to_client[server_id] = client_id;
			m_sounds_client_to_server[client_id] = server_id;
			if(object_id != 0)
				m_sounds_to_objects[client_id] = object_id;
		}
	}
	else if(command == TOCLIENT_STOP_SOUND)
	{
		s32 server_id = packet[TOCLIENT_STOP_SOUND_ID].as<s32>();
		std::map<s32, int>::iterator i =
				m_sounds_server_to_client.find(server_id);
		if(i != m_sounds_server_to_client.end()){
			int client_id = i->second;
			m_sound->stopSound(client_id);
		}
	}
	else if(command == TOCLIENT_PRIVILEGES)
	{
		packet[TOCLIENT_PRIVILEGES_PRIVILEGES].convert(&m_privileges);
	}
	else if(command == TOCLIENT_INVENTORY_FORMSPEC)
	{
		// Store formspec in LocalPlayer
		player->inventory_formspec = packet[TOCLIENT_INVENTORY_FORMSPEC_DATA].as<std::string>();
	}
	else if(command == TOCLIENT_DETACHED_INVENTORY)
	{
		std::string name = packet[TOCLIENT_DETACHED_INVENTORY_NAME].as<std::string>();
		std::string datastring = packet[TOCLIENT_DETACHED_INVENTORY_DATA].as<std::string>();
		std::istringstream is(datastring, std::ios_base::binary);

		infostream<<"Client: Detached inventory update: \""<<name<<"\""<<std::endl;

		Inventory *inv = NULL;
		if(m_detached_inventories.count(name) > 0)
			inv = m_detached_inventories[name];
		else{
			inv = new Inventory(m_itemdef);
			m_detached_inventories[name] = inv;
		}
		inv->deSerialize(is);
	}
	else if(command == TOCLIENT_SHOW_FORMSPEC)
	{
		std::string formspec = packet[TOCLIENT_SHOW_FORMSPEC_DATA].as<std::string>();
		std::string formname = packet[TOCLIENT_SHOW_FORMSPEC_NAME].as<std::string>();

		ClientEvent event;
		event.type = CE_SHOW_FORMSPEC;
		// pointer is required as event is a struct only!
		// adding a std:string to a struct isn't possible
		event.show_formspec.formspec = new std::string(formspec);
		event.show_formspec.formname = new std::string(formname);
		m_client_event_queue.push(event);
	}
	else if(command == TOCLIENT_SPAWN_PARTICLE)
	{
		v3f pos = packet[TOCLIENT_SPAWN_PARTICLE_POS].as<v3f>();
		v3f vel = packet[TOCLIENT_SPAWN_PARTICLE_VELOCITY].as<v3f>();
		v3f acc = packet[TOCLIENT_SPAWN_PARTICLE_ACCELERATION].as<v3f>();
		float expirationtime = packet[TOCLIENT_SPAWN_PARTICLE_EXPIRATIONTIME].as<float>();
		float size = packet[TOCLIENT_SPAWN_PARTICLE_SIZE].as<float>();
		bool collisiondetection = packet[TOCLIENT_SPAWN_PARTICLE_COLLISIONDETECTION].as<bool>();
		std::string texture = packet[TOCLIENT_SPAWN_PARTICLE_TEXTURE].as<std::string>();
		bool vertical = packet[TOCLIENT_SPAWN_PARTICLE_VERTICAL].as<bool>();

		ClientEvent event;
		event.type = CE_SPAWN_PARTICLE;
		event.spawn_particle.pos = new v3f (pos);
		event.spawn_particle.vel = new v3f (vel);
		event.spawn_particle.acc = new v3f (acc);

		event.spawn_particle.expirationtime = expirationtime;
		event.spawn_particle.size = size;
		event.spawn_particle.collisiondetection =
				collisiondetection;
		event.spawn_particle.vertical = vertical;
		event.spawn_particle.texture = new std::string(texture);

		m_client_event_queue.push(event);
	}
	else if(command == TOCLIENT_ADD_PARTICLESPAWNER)
	{
		u16 amount;
		float spawntime, minexptime, maxexptime, minsize, maxsize;
		v3f minpos, maxpos, minvel, maxvel, minacc, maxacc;
		bool collisiondetection, vertical;
		u32 id;
		std::string texture;

		packet[TOCLIENT_ADD_PARTICLESPAWNER_AMOUNT].convert(&amount);
		packet[TOCLIENT_ADD_PARTICLESPAWNER_SPAWNTIME].convert(&spawntime);
		packet[TOCLIENT_ADD_PARTICLESPAWNER_MINPOS].convert(&minpos);
		packet[TOCLIENT_ADD_PARTICLESPAWNER_MAXPOS].convert(&maxpos);
		packet[TOCLIENT_ADD_PARTICLESPAWNER_MINVEL].convert(&minvel);
		packet[TOCLIENT_ADD_PARTICLESPAWNER_MAXVEL].convert(&maxvel);
		packet[TOCLIENT_ADD_PARTICLESPAWNER_MINACC].convert(&minacc);
		packet[TOCLIENT_ADD_PARTICLESPAWNER_MAXACC].convert(&maxacc);
		packet[TOCLIENT_ADD_PARTICLESPAWNER_MINEXPTIME].convert(&minexptime);
		packet[TOCLIENT_ADD_PARTICLESPAWNER_MAXEXPTIME].convert(&maxexptime);
		packet[TOCLIENT_ADD_PARTICLESPAWNER_MINSIZE].convert(&minsize);
		packet[TOCLIENT_ADD_PARTICLESPAWNER_MAXSIZE].convert(&maxsize);
		packet[TOCLIENT_ADD_PARTICLESPAWNER_COLLISIONDETECTION].convert(&collisiondetection);
		packet[TOCLIENT_ADD_PARTICLESPAWNER_TEXTURE].convert(&texture);
		packet[TOCLIENT_ADD_PARTICLESPAWNER_VERTICAL].convert(&vertical);
		packet[TOCLIENT_ADD_PARTICLESPAWNER_ID].convert(&id);

		ClientEvent event;
		event.type = CE_ADD_PARTICLESPAWNER;
		event.add_particlespawner.amount = amount;
		event.add_particlespawner.spawntime = spawntime;

		event.add_particlespawner.minpos = new v3f (minpos);
		event.add_particlespawner.maxpos = new v3f (maxpos);
		event.add_particlespawner.minvel = new v3f (minvel);
		event.add_particlespawner.maxvel = new v3f (maxvel);
		event.add_particlespawner.minacc = new v3f (minacc);
		event.add_particlespawner.maxacc = new v3f (maxacc);

		event.add_particlespawner.minexptime = minexptime;
		event.add_particlespawner.maxexptime = maxexptime;
		event.add_particlespawner.minsize = minsize;
		event.add_particlespawner.maxsize = maxsize;
		event.add_particlespawner.collisiondetection = collisiondetection;
		event.add_particlespawner.vertical = vertical;
		event.add_particlespawner.texture = new std::string(texture);
		event.add_particlespawner.id = id;

		m_client_event_queue.push(event);
	}
	else if(command == TOCLIENT_DELETE_PARTICLESPAWNER)
	{
		u32 id = packet[TOCLIENT_DELETE_PARTICLESPAWNER_ID].as<u32>();

		ClientEvent event;
		event.type = CE_DELETE_PARTICLESPAWNER;
		event.delete_particlespawner.id = id;

		m_client_event_queue.push(event);
	}
	else if(command == TOCLIENT_HUDADD)
	{
		std::string datastring((char *)&data[2], datasize - 2);
		std::istringstream is(datastring, std::ios_base::binary);

		u32 id, number, item, dir;
		u8 type;
		v2f pos, scale, align, offset;
		std::string name, text;
		v3f world_pos;
		v2s32 size;

		packet[TOCLIENT_HUDADD_ID].convert(&id);
		packet[TOCLIENT_HUDADD_TYPE].convert(&type);
		packet[TOCLIENT_HUDADD_POS].convert(&pos);
		packet[TOCLIENT_HUDADD_NAME].convert(&name);
		packet[TOCLIENT_HUDADD_SCALE].convert(&scale);
		packet[TOCLIENT_HUDADD_TEXT].convert(&text);
		packet[TOCLIENT_HUDADD_NUMBER].convert(&number);
		packet[TOCLIENT_HUDADD_ITEM].convert(&item);
		packet[TOCLIENT_HUDADD_DIR].convert(&dir);
		packet[TOCLIENT_HUDADD_ALIGN].convert(&align);
		packet[TOCLIENT_HUDADD_OFFSET].convert(&offset);
		packet[TOCLIENT_HUDADD_WORLD_POS].convert(&world_pos);
		packet[TOCLIENT_HUDADD_SIZE].convert(&size);

		ClientEvent event;
		event.type = CE_HUDADD;
		event.hudadd.id     = id;
		event.hudadd.type   = type;
		event.hudadd.pos    = new v2f(pos);
		event.hudadd.name   = new std::string(name);
		event.hudadd.scale  = new v2f(scale);
		event.hudadd.text   = new std::string(text);
		event.hudadd.number = number;
		event.hudadd.item   = item;
		event.hudadd.dir    = dir;
		event.hudadd.align  = new v2f(align);
		event.hudadd.offset = new v2f(offset);
		event.hudadd.world_pos = new v3f(world_pos);
		event.hudadd.size      = new v2s32(size);
		m_client_event_queue.push(event);
	}
	else if(command == TOCLIENT_HUDRM)
	{
		u32 id = packet[TOCLIENT_HUDRM_ID].as<u32>();

		ClientEvent event;
		event.type = CE_HUDRM;
		event.hudrm.id = id;
		m_client_event_queue.push(event);
	}
	else if(command == TOCLIENT_HUDCHANGE)
	{
		std::string sdata;
		v2f v2fdata;
		v3f v3fdata;
		v2s32 v2s32data;
		u32 intdata = 0;

		u32 id = packet[TOCLIENT_HUDCHANGE_ID].as<u32>();
		u8 stat = packet[TOCLIENT_HUDCHANGE_STAT].as<int>();

		if (stat == HUD_STAT_POS || stat == HUD_STAT_SCALE ||
				stat == HUD_STAT_ALIGN || stat == HUD_STAT_OFFSET)
			packet[TOCLIENT_HUDCHANGE_V2F].convert(&v2fdata);
		else if (stat == HUD_STAT_NAME || stat == HUD_STAT_TEXT)
			packet[TOCLIENT_HUDCHANGE_STRING].convert(&sdata);
		else if (stat == HUD_STAT_WORLD_POS)
			packet[TOCLIENT_HUDCHANGE_V3F].convert(&v3fdata);
		else if (stat == HUD_STAT_SIZE)
			packet[TOCLIENT_HUDCHANGE_V2S32].convert(&v2s32data);
		else
			packet[TOCLIENT_HUDCHANGE_U32].convert(&intdata);

		ClientEvent event;
		event.type = CE_HUDCHANGE;
		event.hudchange.id      = id;
		event.hudchange.stat    = (HudElementStat)stat;
		event.hudchange.v2fdata = new v2f(v2fdata);
		event.hudchange.v3fdata = new v3f(v3fdata);
		event.hudchange.sdata   = new std::string(sdata);
		event.hudchange.data    = intdata;
		event.hudchange.v2s32data = new v2s32(v2s32data);
		m_client_event_queue.push(event);
	}
	else if(command == TOCLIENT_HUD_SET_FLAGS)
	{
		Player *player = m_env.getLocalPlayer();
		assert(player != NULL);

		u32 flags = packet[TOCLIENT_HUD_SET_FLAGS_FLAGS].as<u32>();
		u32 mask = packet[TOCLIENT_HUD_SET_FLAGS_MASK].as<u32>();

		player->hud_flags &= ~mask;
		player->hud_flags |= flags;
	}
	else if(command == TOCLIENT_HUD_SET_PARAM)
	{
		u16 param = packet[TOCLIENT_HUD_SET_PARAM_ID].as<u16>();
		std::string value = packet[TOCLIENT_HUD_SET_PARAM_VALUE].as<std::string>();

		if(param == HUD_PARAM_HOTBAR_ITEMCOUNT && value.size() == 4){
			s32 hotbar_itemcount = readS32((u8*) value.c_str());
			if(hotbar_itemcount > 0 && hotbar_itemcount <= HUD_HOTBAR_ITEMCOUNT_MAX)
				player->hud_hotbar_itemcount = hotbar_itemcount;
		} else if (param == HUD_PARAM_HOTBAR_IMAGE) {
			((LocalPlayer *) player)->hotbar_image = value;
		} else if (param == HUD_PARAM_HOTBAR_SELECTED_IMAGE) {
			((LocalPlayer *) player)->hotbar_selected_image = value;
		}
	}
/*
	else if(command == TOCLIENT_ANIMATIONS)
	{
		LocalPlayer *player = m_env.getLocalPlayer();
		packet[TOCLIENT_ANIMATIONS_DEFAULT_START].convert(&player->animation_default_start);
		packet[TOCLIENT_ANIMATIONS_DEFAULT_STOP].convert(&player->animation_default_stop);
		packet[TOCLIENT_ANIMATIONS_WALK_START].convert(&player->animation_walk_start);
		packet[TOCLIENT_ANIMATIONS_WALK_STOP].convert(&player->animation_walk_stop);
		packet[TOCLIENT_ANIMATIONS_DIG_START].convert(&player->animation_dig_start);
		packet[TOCLIENT_ANIMATIONS_DIG_STOP].convert(&player->animation_dig_stop);
		packet[TOCLIENT_ANIMATIONS_WD_START].convert(&player->animation_wd_start);
		packet[TOCLIENT_ANIMATIONS_WD_STOP].convert(&player->animation_wd_stop);
	}
*/
	else if(command == TOCLIENT_SET_SKY)
	{
		video::SColor *bgcolor = new video::SColor(packet[TOCLIENT_SET_SKY_COLOR].as<video::SColor>());
		std::string *type = new std::string(packet[TOCLIENT_SET_SKY_TYPE].as<std::string>());
		std::vector<std::string> *params = new std::vector<std::string>;
		packet[TOCLIENT_SET_SKY_PARAMS].convert(params);

		ClientEvent event;
		event.type            = CE_SET_SKY;
		event.set_sky.bgcolor = bgcolor;
		event.set_sky.type    = type;
		event.set_sky.params  = params;
		m_client_event_queue.push(event);
	}
	else if(command == TOCLIENT_OVERRIDE_DAY_NIGHT_RATIO)
	{
		bool do_override;
		float day_night_ratio_f;
		packet[TOCLIENT_OVERRIDE_DAY_NIGHT_RATIO_DO].convert(&do_override);
		packet[TOCLIENT_OVERRIDE_DAY_NIGHT_RATIO_VALUE].convert(&day_night_ratio_f);

		ClientEvent event;
		event.type                                 = CE_OVERRIDE_DAY_NIGHT_RATIO;
		event.override_day_night_ratio.do_override = do_override;
		event.override_day_night_ratio.ratio_f     = day_night_ratio_f;
		m_client_event_queue.push(event);
	}
	else if(command == TOCLIENT_LOCAL_PLAYER_ANIMATIONS)
	{
		LocalPlayer *player = m_env.getLocalPlayer();
		assert(player != NULL);

		packet[TOCLIENT_LOCAL_PLAYER_ANIMATIONS_IDLE].convert(&player->local_animations[0]);
		packet[TOCLIENT_LOCAL_PLAYER_ANIMATIONS_WALK].convert(&player->local_animations[1]);
		packet[TOCLIENT_LOCAL_PLAYER_ANIMATIONS_DIG].convert(&player->local_animations[2]);
		packet[TOCLIENT_LOCAL_PLAYER_ANIMATIONS_WALKDIG].convert(&player->local_animations[3]);
		packet[TOCLIENT_LOCAL_PLAYER_ANIMATIONS_FRAME_SPEED].convert(&player->local_animation_speed);
	}
	else if(command == TOCLIENT_EYE_OFFSET)
	{
		LocalPlayer *player = m_env.getLocalPlayer();
		assert(player != NULL);

		packet[TOCLIENT_EYE_OFFSET_FIRST].convert(&player->eye_offset_first);
		packet[TOCLIENT_EYE_OFFSET_THIRD].convert(&player->eye_offset_third);
	}
	else
	{
		infostream<<"Client: Ignoring unknown command "
				<<command<<std::endl;
	}
}
