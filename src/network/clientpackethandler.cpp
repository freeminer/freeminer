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
#include "network/fm_clientpackethandler.cpp"
#else //TODO

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

void Client::handleCommand_Deprecated(NetworkPacket* pkt)
{
	infostream << "Got deprecated command "
			<< toClientCommandTable[pkt->getCommand()].name << " from peer "
			<< pkt->getPeerId() << "!" << std::endl;
}

void Client::handleCommand_Hello(NetworkPacket* pkt)
{
	if (pkt->getSize() < 1)
		return;

	u8 deployed;
	*pkt >> deployed;

	infostream << "Client: TOCLIENT_HELLO received with "
			"deployed=" << ((int)deployed & 0xff) << std::endl;

	if (!ser_ver_supported(deployed)) {
		infostream << "Client: TOCLIENT_HELLO: Server sent "
				<< "unsupported ser_fmt_ver"<< std::endl;
		return;
	}

	m_server_ser_ver = deployed;

	// @ TODO auth to server
}

void Client::handleCommand_AuthAccept(NetworkPacket* pkt)
{
	v3f playerpos;
	*pkt >> playerpos >> m_map_seed >> m_recommended_send_interval;

	playerpos -= v3f(0, BS / 2, 0);

	// Set player position
	Player *player = m_env.getLocalPlayer();
	assert(player != NULL);
	player->setPosition(playerpos);

	infostream << "Client: received map seed: " << m_map_seed << std::endl;
	infostream << "Client: received recommended send interval "
					<< m_recommended_send_interval<<std::endl;

	// Reply to server
	NetworkPacket resp_pkt(TOSERVER_INIT2, 0);
	Send(&resp_pkt);

	m_state = LC_Init;
}

void Client::handleCommand_InitLegacy(NetworkPacket* pkt)
{
	if (pkt->getSize() < 1)
		return;

	u8 deployed;
	*pkt >> deployed;

	infostream << "Client: TOCLIENT_INIT_LEGACY received with "
			"deployed=" << ((int)deployed & 0xff) << std::endl;

	if (!ser_ver_supported(deployed)) {
		infostream << "Client: TOCLIENT_INIT_LEGACY: Server sent "
				<< "unsupported ser_fmt_ver"<< std::endl;
		return;
	}

	m_server_ser_ver = deployed;

	// Get player position
	v3s16 playerpos_s16(0, BS * 2 + BS * 20, 0);
	if (pkt->getSize() >= 1 + 6) {
		*pkt >> playerpos_s16;
	}
	v3f playerpos_f = intToFloat(playerpos_s16, BS) - v3f(0, BS / 2, 0);


	// Set player position
	Player *player = m_env.getLocalPlayer();
	assert(player != NULL);
	player->setPosition(playerpos_f);

	if (pkt->getSize() >= 1 + 6 + 8) {
		// Get map seed
		*pkt >> m_map_seed;
		infostream << "Client: received map seed: " << m_map_seed << std::endl;
	}

	if (pkt->getSize() >= 1 + 6 + 8 + 4) {
		*pkt >> m_recommended_send_interval;
		infostream << "Client: received recommended send interval "
				<< m_recommended_send_interval<<std::endl;
	}

	// Reply to server
	NetworkPacket resp_pkt(TOSERVER_INIT2, 0);
	Send(&resp_pkt);

	m_state = LC_Init;
}

void Client::handleCommand_AccessDenied(NetworkPacket* pkt)
{
	// The server didn't like our password. Note, this needs
	// to be processed even if the serialisation format has
	// not been agreed yet, the same as TOCLIENT_INIT.
	m_access_denied = true;
	m_access_denied_reason = "Unknown";

	if (pkt->getCommand() == TOCLIENT_ACCESS_DENIED) {
		if (pkt->getSize() < 1)
			return;

		u8 denyCode = SERVER_ACCESSDENIED_UNEXPECTED_DATA;
		*pkt >> denyCode;
		if (denyCode == SERVER_ACCESSDENIED_CUSTOM_STRING) {
			std::wstring wide_reason;
			*pkt >> wide_reason;
			m_access_denied_reason = wide_to_narrow(wide_reason);
		}
		else if (denyCode < SERVER_ACCESSDENIED_MAX) {
			m_access_denied_reason = accessDeniedStrings[denyCode];
		}
	}
	// 13/03/15 Legacy code from 0.4.12 and lesser. must stay 1 year
	// for compat with old clients
	else {
		if (pkt->getSize() >= 2) {
			std::wstring wide_reason;
			*pkt >> wide_reason;
			m_access_denied_reason = wide_to_narrow(wide_reason);
		}
	}
}

void Client::handleCommand_RemoveNode(NetworkPacket* pkt)
{
	if (pkt->getSize() < 6)
		return;

	v3s16 p;
	*pkt >> p;
	removeNode(p, 2);
}

void Client::handleCommand_AddNode(NetworkPacket* pkt)
{
	if (pkt->getSize() < 6 + MapNode::serializedLength(m_server_ser_ver))
		return;

	v3s16 p;
	*pkt >> p;

	MapNode n;
	n.deSerialize(pkt->getU8Ptr(6), m_server_ser_ver);

	bool remove_metadata = true;
	u32 index = 6 + MapNode::serializedLength(m_server_ser_ver);
	if ((pkt->getSize() >= index + 1) && pkt->getU8(index)) {
		remove_metadata = false;
	}

	addNode(p, n, remove_metadata, 2);
}
void Client::handleCommand_BlockData(NetworkPacket* pkt)
{
	// Ignore too small packet
	if (pkt->getSize() < 6)
		return;

	v3s16 p;
	*pkt >> p;

	std::string datastring(pkt->getString(6), pkt->getSize() - 6);
	std::istringstream istr(datastring, std::ios_base::binary);

	MapBlock *block;

	///v2s16 p2d(p.X, p.Z);
	auto * sector = &m_env.getMap();

	//assert(sector->getPos() == p2d);

	block = sector->getBlockNoCreateNoEx(p);
	if (block) {
		/*
			Update an existing block
		*/
		if (!block->deSerialize(istr, m_server_ser_ver, false)) {
			delete block;
			return;
		}
		block->deSerializeNetworkSpecific(istr);
	}
	else {
		/*
			Create a new block
		*/
		block = new MapBlock(&m_env.getMap(), p, this);
		if(!block->deSerialize(istr, m_server_ser_ver, false)){
			delete block;
			return;
		}
		block->deSerializeNetworkSpecific(istr);
		sector->insertBlock(block);
	}

	if (m_localdb) {
		ServerMap::saveBlock(block, m_localdb);
	}

	/*
		Add it to mesh update queue and set it to be acknowledged after update.
	*/
	//addUpdateMeshTaskWithEdge(p, true);
	updateMeshTimestampWithEdge(p);

	sendGotBlocks(p);
}

void Client::handleCommand_Inventory(NetworkPacket* pkt)
{
	if (pkt->getSize() < 1)
		return;

	std::string datastring(pkt->getString(0), pkt->getSize());
	std::istringstream is(datastring, std::ios_base::binary);

	Player *player = m_env.getLocalPlayer();
	assert(player != NULL);

	player->inventory.deSerialize(is);

	m_inventory_updated = true;

	delete m_inventory_from_server;
	m_inventory_from_server = new Inventory(player->inventory);
	m_inventory_from_server_age = 0.0;
}

void Client::handleCommand_TimeOfDay(NetworkPacket* pkt)
{
	if (pkt->getSize() < 2)
		return;

	u16 time_of_day;

	*pkt >> time_of_day;

	time_of_day      = time_of_day % 24000;
	float time_speed = 0;

	if (pkt->getSize() >= 2 + 4) {
		*pkt >> time_speed;
	}
	else {
		// Old message; try to approximate speed of time by ourselves
		float time_of_day_f = (float)time_of_day / 24000.0;
		float tod_diff_f = 0;

		if (time_of_day_f < 0.2 && m_last_time_of_day_f > 0.8)
			tod_diff_f = time_of_day_f - m_last_time_of_day_f + 1.0;
		else
			tod_diff_f = time_of_day_f - m_last_time_of_day_f;

		m_last_time_of_day_f       = time_of_day_f;
		float time_diff            = m_time_of_day_update_timer;
		m_time_of_day_update_timer = 0;

		if (m_time_of_day_set) {
			time_speed = (3600.0 * 24.0) * tod_diff_f / time_diff;
			infostream << "Client: Measured time_of_day speed (old format): "
					<< time_speed << " tod_diff_f=" << tod_diff_f
					<< " time_diff=" << time_diff << std::endl;
		}
	}

	// Update environment
	m_env.setTimeOfDay(time_of_day);
	m_env.setTimeOfDaySpeed(time_speed);
	m_time_of_day_set = true;

	u32 dr = m_env.getDayNightRatio();
	infostream << "Client: time_of_day=" << time_of_day
			<< " time_speed=" << time_speed
			<< " dr=" << dr << std::endl;
}

void Client::handleCommand_ChatMessage(NetworkPacket* pkt)
{
	/*
		u16 command
		u16 length
		wstring message
	*/
	u16 len, read_wchar;

	*pkt >> len;

	std::wstring message;
	for (u32 i = 0; i < len; i++) {
		*pkt >> read_wchar;
		message += (wchar_t)read_wchar;
	}

	m_chat_queue.push(wide_to_narrow(message));
}

void Client::handleCommand_ActiveObjectRemoveAdd(NetworkPacket* pkt)
{
	/*
		u16 command
		u16 count of removed objects
		for all removed objects {
			u16 id
		}
		u16 count of added objects
		for all added objects {
			u16 id
			u8 type
			u32 initialization data length
			string initialization data
		}
	*/

	// Read removed objects
	u8 type;
	u16 removed_count, added_count, id;

	*pkt >> removed_count;

	for (u16 i = 0; i < removed_count; i++) {
		*pkt >> id;
		m_env.removeActiveObject(id);
	}

	// Read added objects
	*pkt >> added_count;

	for (u16 i = 0; i < added_count; i++) {
		*pkt >> id >> type;
		m_env.addActiveObject(id, type, pkt->readLongString());
	}
}

void Client::handleCommand_ActiveObjectMessages(NetworkPacket* pkt)
{
	/*
		u16 command
		for all objects
		{
			u16 id
			u16 message length
			string message
		}
	*/
	char buf[6];
	// Get all data except the command number
	std::string datastring(pkt->getString(0), pkt->getSize());
	// Throw them in an istringstream
	std::istringstream is(datastring, std::ios_base::binary);

	while(is.eof() == false) {
		is.read(buf, 2);
		u16 id = readU16((u8*)buf);
		if (is.eof())
			break;
		is.read(buf, 2);
		size_t message_size = readU16((u8*)buf);
		std::string message;
		message.reserve(message_size);
		for (u32 i = 0; i < message_size; i++) {
			is.read(buf, 1);
			message.append(buf, 1);
		}
		// Pass on to the environment
		m_env.processActiveObjectMessage(id, message);
	}
}

void Client::handleCommand_Movement(NetworkPacket* pkt)
{
	Player *player = m_env.getLocalPlayer();
	assert(player != NULL);

	float mad, maa, maf, msw, mscr, msf, mscl, msj, lf, lfs, ls, g;

	*pkt >> mad >> maa >> maf >> msw >> mscr >> msf >> mscl >> msj
		>> lf >> lfs >> ls >> g;

	player->movement_acceleration_default   = mad * BS;
	player->movement_acceleration_air       = maa * BS;
	player->movement_acceleration_fast      = maf * BS;
	player->movement_speed_walk             = msw * BS;
	player->movement_speed_crouch           = mscr * BS;
	player->movement_speed_fast             = msf * BS;
	player->movement_speed_climb            = mscl * BS;
	player->movement_speed_jump             = msj * BS;
	player->movement_liquid_fluidity        = lf * BS;
	player->movement_liquid_fluidity_smooth = lfs * BS;
	player->movement_liquid_sink            = ls * BS;
	player->movement_gravity                = g * BS;
}

void Client::handleCommand_HP(NetworkPacket* pkt)
{

	Player *player = m_env.getLocalPlayer();
	assert(player != NULL);

	u8 oldhp   = player->hp;

	u8 hp;
	*pkt >> hp;

	player->hp = hp;

	if (hp < oldhp) {
		// Add to ClientEvent queue
		ClientEvent event;
		event.type = CE_PLAYER_DAMAGE;
		event.player_damage.amount = oldhp - hp;
		m_client_event_queue.push(event);
	}
}

void Client::handleCommand_Breath(NetworkPacket* pkt)
{
	Player *player = m_env.getLocalPlayer();
	assert(player != NULL);

	u16 breath;

	*pkt >> breath;

	player->setBreath(breath);
}

void Client::handleCommand_MovePlayer(NetworkPacket* pkt)
{
	Player *player = m_env.getLocalPlayer();
	assert(player != NULL);

	v3f pos;
	f32 pitch, yaw;

	*pkt >> pos >> pitch >> yaw;

	player->setPosition(pos);

	infostream << "Client got TOCLIENT_MOVE_PLAYER"
			<< " pos=(" << pos.X << "," << pos.Y << "," << pos.Z << ")"
			<< " pitch=" << pitch
			<< " yaw=" << yaw
			<< std::endl;

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

void Client::handleCommand_PlayerItem(NetworkPacket* pkt)
{
	infostream << "Client: WARNING: Ignoring TOCLIENT_PLAYERITEM" << std::endl;
}

void Client::handleCommand_DeathScreen(NetworkPacket* pkt)
{
	bool set_camera_point_target;
	v3f camera_point_target;

	*pkt >> set_camera_point_target;
	*pkt >> camera_point_target;

	ClientEvent event;
	event.type                                = CE_DEATHSCREEN;
	event.deathscreen.set_camera_point_target = set_camera_point_target;
	event.deathscreen.camera_point_target_x   = camera_point_target.X;
	event.deathscreen.camera_point_target_y   = camera_point_target.Y;
	event.deathscreen.camera_point_target_z   = camera_point_target.Z;
	m_client_event_queue.push(event);
}

void Client::handleCommand_AnnounceMedia(NetworkPacket* pkt)
{
	u16 num_files;

	*pkt >> num_files;

	infostream << "Client: Received media announcement: packet size: "
			<< pkt->getSize() << std::endl;

	if (m_media_downloader == NULL ||
			m_media_downloader->isStarted()) {
		const char *problem = m_media_downloader ?
			"we already saw another announcement" :
			"all media has been received already";
		errorstream << "Client: Received media announcement but "
			<< problem << "! "
			<< " files=" << num_files
			<< " size=" << pkt->getSize() << std::endl;
		return;
	}

	// Mesh update thread must be stopped while
	// updating content definitions
	sanity_check(!m_mesh_update_thread.IsRunning());

	for (u16 i = 0; i < num_files; i++) {
		std::string name, sha1_base64;

		*pkt >> name >> sha1_base64;

		std::string sha1_raw = base64_decode(sha1_base64);
		m_media_downloader->addFile(name, sha1_raw);
	}

	std::vector<std::string> remote_media;
	try {
		std::string str;

		*pkt >> str;

		Strfnd sf(str);
		while(!sf.atend()) {
			std::string baseurl = trim(sf.next(","));
			if (baseurl != "")
				m_media_downloader->addRemoteServer(baseurl);
		}
	}
	catch(SerializationError& e) {
		// not supported by server or turned off
	}

	m_media_downloader->step(this);
}

void Client::handleCommand_Media(NetworkPacket* pkt)
{
	/*
		u16 command
		u16 total number of file bunches
		u16 index of this bunch
		u32 number of files in this bunch
		for each file {
			u16 length of name
			string name
			u32 length of data
			data
		}
	*/
	u16 num_bunches;
	u16 bunch_i;
	u32 num_files;

	*pkt >> num_bunches >> bunch_i >> num_files;

	infostream << "Client: Received files: bunch " << bunch_i << "/"
			<< num_bunches << " files=" << num_files
			<< " size=" << pkt->getSize() << std::endl;

	if (num_files == 0)
		return;

	if (m_media_downloader == NULL ||
			!m_media_downloader->isStarted()) {
		const char *problem = m_media_downloader ?
			"media has not been requested" :
			"all media has been received already";
		errorstream << "Client: Received media but "
			<< problem << "! "
			<< " bunch " << bunch_i << "/" << num_bunches
			<< " files=" << num_files
			<< " size=" << pkt->getSize() << std::endl;
		return;
	}

	// Mesh update thread must be stopped while
	// updating content definitions
	sanity_check(!m_mesh_update_thread.IsRunning());

	for (u32 i=0; i < num_files; i++) {
		std::string name;

		*pkt >> name;

		std::string data = pkt->readLongString();

		m_media_downloader->conventionalTransferDone(
				name, data, this);
	}
}

void Client::handleCommand_ToolDef(NetworkPacket* pkt)
{
	infostream << "Client: WARNING: Ignoring TOCLIENT_TOOLDEF" << std::endl;
}

void Client::handleCommand_NodeDef(NetworkPacket* pkt)
{
	infostream << "Client: Received node definitions: packet size: "
			<< pkt->getSize() << std::endl;

	// Mesh update thread must be stopped while
	// updating content definitions
	sanity_check(!m_mesh_update_thread.IsRunning());

	// Decompress node definitions
	std::string datastring(pkt->getString(0), pkt->getSize());
	std::istringstream is(datastring, std::ios_base::binary);
	std::istringstream tmp_is(deSerializeLongString(is), std::ios::binary);
	std::ostringstream tmp_os;
	decompressZlib(tmp_is, tmp_os);

	// Deserialize node definitions
	std::istringstream tmp_is2(tmp_os.str());
	m_nodedef->deSerialize(tmp_is2);
	m_nodedef_received = true;
}

void Client::handleCommand_CraftItemDef(NetworkPacket* pkt)
{
	infostream << "Client: WARNING: Ignoring TOCLIENT_CRAFTITEMDEF" << std::endl;
}

void Client::handleCommand_ItemDef(NetworkPacket* pkt)
{
	infostream << "Client: Received item definitions: packet size: "
			<< pkt->getSize() << std::endl;

	// Mesh update thread must be stopped while
	// updating content definitions
	sanity_check(!m_mesh_update_thread.IsRunning());

	// Decompress item definitions
	std::string datastring(pkt->getString(0), pkt->getSize());
	std::istringstream is(datastring, std::ios_base::binary);
	std::istringstream tmp_is(deSerializeLongString(is), std::ios::binary);
	std::ostringstream tmp_os;
	decompressZlib(tmp_is, tmp_os);

	// Deserialize node definitions
	std::istringstream tmp_is2(tmp_os.str());
	m_itemdef->deSerialize(tmp_is2);
	m_itemdef_received = true;
}

void Client::handleCommand_PlaySound(NetworkPacket* pkt)
{
	s32 server_id;
	std::string name;
	float gain;
	u8 type; // 0=local, 1=positional, 2=object
	v3f pos;
	u16 object_id;
	bool loop;

	*pkt >> server_id >> name >> gain >> type >> pos >> object_id >> loop;

	// Start playing
	int client_id = -1;
	switch(type) {
		case 0: // local
			client_id = m_sound->playSound(name, loop, gain);
			break;
		case 1: // positional
			client_id = m_sound->playSoundAt(name, loop, gain, pos);
			break;
		case 2:
		{ // object
			ClientActiveObject *cao = m_env.getActiveObject(object_id);
			if (cao)
				pos = cao->getPosition();
			client_id = m_sound->playSoundAt(name, loop, gain, pos);
			// TODO: Set up sound to move with object
			break;
		}
		default:
			break;
	}

	if (client_id != -1) {
		m_sounds_server_to_client[server_id] = client_id;
		m_sounds_client_to_server[client_id] = server_id;
		if (object_id != 0)
			m_sounds_to_objects[client_id] = object_id;
	}
}

void Client::handleCommand_StopSound(NetworkPacket* pkt)
{
	s32 server_id;

	*pkt >> server_id;

	std::map<s32, int>::iterator i =
		m_sounds_server_to_client.find(server_id);

	if (i != m_sounds_server_to_client.end()) {
		int client_id = i->second;
		m_sound->stopSound(client_id);
	}
}

void Client::handleCommand_Privileges(NetworkPacket* pkt)
{
	m_privileges.clear();
	infostream << "Client: Privileges updated: ";
	u16 num_privileges;

	*pkt >> num_privileges;

	for (u16 i = 0; i < num_privileges; i++) {
		std::string priv;

		*pkt >> priv;

		m_privileges.insert(priv);
		infostream << priv << " ";
	}
	infostream << std::endl;
}

void Client::handleCommand_InventoryFormSpec(NetworkPacket* pkt)
{
	Player *player = m_env.getLocalPlayer();
	assert(player != NULL);

	// Store formspec in LocalPlayer
	player->inventory_formspec = pkt->readLongString();
}

void Client::handleCommand_DetachedInventory(NetworkPacket* pkt)
{
	std::string datastring(pkt->getString(0), pkt->getSize());
	std::istringstream is(datastring, std::ios_base::binary);

	std::string name = deSerializeString(is);

	infostream << "Client: Detached inventory update: \"" << name
			<< "\"" << std::endl;

	Inventory *inv = NULL;
	if (m_detached_inventories.count(name) > 0)
		inv = m_detached_inventories[name];
	else {
		inv = new Inventory(m_itemdef);
		m_detached_inventories[name] = inv;
	}
	inv->deSerialize(is);
}

void Client::handleCommand_ShowFormSpec(NetworkPacket* pkt)
{
	std::string formspec = pkt->readLongString();
	std::string formname;

	*pkt >> formname;

	ClientEvent event;
	event.type = CE_SHOW_FORMSPEC;
	// pointer is required as event is a struct only!
	// adding a std:string to a struct isn't possible
	event.show_formspec.formspec = new std::string(formspec);
	event.show_formspec.formname = new std::string(formname);
	m_client_event_queue.push(event);
}

void Client::handleCommand_SpawnParticle(NetworkPacket* pkt)
{
	std::string datastring(pkt->getString(0), pkt->getSize());
	std::istringstream is(datastring, std::ios_base::binary);

	v3f pos                 = readV3F1000(is);
	v3f vel                 = readV3F1000(is);
	v3f acc                 = readV3F1000(is);
	float expirationtime    = readF1000(is);
	float size              = readF1000(is);
	bool collisiondetection = readU8(is);
	std::string texture     = deSerializeLongString(is);
	bool vertical           = false;
	try {
		vertical = readU8(is);
	} catch (...) {}

	ClientEvent event;
	event.type                              = CE_SPAWN_PARTICLE;
	event.spawn_particle.pos                = new v3f (pos);
	event.spawn_particle.vel                = new v3f (vel);
	event.spawn_particle.acc                = new v3f (acc);
	event.spawn_particle.expirationtime     = expirationtime;
	event.spawn_particle.size               = size;
	event.spawn_particle.collisiondetection = collisiondetection;
	event.spawn_particle.vertical           = vertical;
	event.spawn_particle.texture            = new std::string(texture);

	m_client_event_queue.push(event);
}

void Client::handleCommand_AddParticleSpawner(NetworkPacket* pkt)
{
	u16 amount;
	float spawntime;
	v3f minpos;
	v3f maxpos;
	v3f minvel;
	v3f maxvel;
	v3f minacc;
	v3f maxacc;
	float minexptime;
	float maxexptime;
	float minsize;
	float maxsize;
	bool collisiondetection;
	u32 id;

	*pkt >> amount >> spawntime >> minpos >> maxpos >> minvel >> maxvel
		>> minacc >> maxacc >> minexptime >> maxexptime >> minsize
		>> maxsize >> collisiondetection;

	std::string texture = pkt->readLongString();

	*pkt >> id;

	bool vertical = false;
	try {
		*pkt >> vertical;
	} catch (...) {}

	ClientEvent event;
	event.type                                   = CE_ADD_PARTICLESPAWNER;
	event.add_particlespawner.amount             = amount;
	event.add_particlespawner.spawntime          = spawntime;
	event.add_particlespawner.minpos             = new v3f (minpos);
	event.add_particlespawner.maxpos             = new v3f (maxpos);
	event.add_particlespawner.minvel             = new v3f (minvel);
	event.add_particlespawner.maxvel             = new v3f (maxvel);
	event.add_particlespawner.minacc             = new v3f (minacc);
	event.add_particlespawner.maxacc             = new v3f (maxacc);
	event.add_particlespawner.minexptime         = minexptime;
	event.add_particlespawner.maxexptime         = maxexptime;
	event.add_particlespawner.minsize            = minsize;
	event.add_particlespawner.maxsize            = maxsize;
	event.add_particlespawner.collisiondetection = collisiondetection;
	event.add_particlespawner.vertical           = vertical;
	event.add_particlespawner.texture            = new std::string(texture);
	event.add_particlespawner.id                 = id;

	m_client_event_queue.push(event);
}


void Client::handleCommand_DeleteParticleSpawner(NetworkPacket* pkt)
{
	u16 legacy_id;
	u32 id;

	// Modification set 13/03/15, 1 year of compat for protocol v24
	if (pkt->getCommand() == TOCLIENT_DELETE_PARTICLESPAWNER_LEGACY) {
		*pkt >> legacy_id;
	}
	else {
		*pkt >> id;
	}


	ClientEvent event;
	event.type                      = CE_DELETE_PARTICLESPAWNER;
	event.delete_particlespawner.id =
			(pkt->getCommand() == TOCLIENT_DELETE_PARTICLESPAWNER_LEGACY ? (u32) legacy_id : id);

	m_client_event_queue.push(event);
}

void Client::handleCommand_HudAdd(NetworkPacket* pkt)
{
	std::string datastring(pkt->getString(0), pkt->getSize());
	std::istringstream is(datastring, std::ios_base::binary);

	u32 id;
	u8 type;
	v2f pos;
	std::string name;
	v2f scale;
	std::string text;
	u32 number;
	u32 item;
	u32 dir;
	v2f align;
	v2f offset;
	v3f world_pos;
	v2s32 size;

	*pkt >> id >> type >> pos >> name >> scale >> text >> number >> item
		>> dir >> align >> offset;
	try {
		*pkt >> world_pos;
	}
	catch(SerializationError &e) {};

	try {
		*pkt >> size;
	} catch(SerializationError &e) {};

	ClientEvent event;
	event.type             = CE_HUDADD;
	event.hudadd.id        = id;
	event.hudadd.type      = type;
	event.hudadd.pos       = new v2f(pos);
	event.hudadd.name      = new std::string(name);
	event.hudadd.scale     = new v2f(scale);
	event.hudadd.text      = new std::string(text);
	event.hudadd.number    = number;
	event.hudadd.item      = item;
	event.hudadd.dir       = dir;
	event.hudadd.align     = new v2f(align);
	event.hudadd.offset    = new v2f(offset);
	event.hudadd.world_pos = new v3f(world_pos);
	event.hudadd.size      = new v2s32(size);
	m_client_event_queue.push(event);
}

void Client::handleCommand_HudRemove(NetworkPacket* pkt)
{
	u32 id;

	*pkt >> id;

	ClientEvent event;
	event.type     = CE_HUDRM;
	event.hudrm.id = id;
	m_client_event_queue.push(event);
}

void Client::handleCommand_HudChange(NetworkPacket* pkt)
{
	std::string sdata;
	v2f v2fdata;
	v3f v3fdata;
	u32 intdata = 0;
	v2s32 v2s32data;
	u32 id;
	u8 stat;

	*pkt >> id >> stat;

	if (stat == HUD_STAT_POS || stat == HUD_STAT_SCALE ||
		stat == HUD_STAT_ALIGN || stat == HUD_STAT_OFFSET)
		*pkt >> v2fdata;
	else if (stat == HUD_STAT_NAME || stat == HUD_STAT_TEXT)
		*pkt >> sdata;
	else if (stat == HUD_STAT_WORLD_POS)
		*pkt >> v3fdata;
	else if (stat == HUD_STAT_SIZE )
		*pkt >> v2s32data;
	else
		*pkt >> intdata;

	ClientEvent event;
	event.type              = CE_HUDCHANGE;
	event.hudchange.id      = id;
	event.hudchange.stat    = (HudElementStat)stat;
	event.hudchange.v2fdata = new v2f(v2fdata);
	event.hudchange.v3fdata = new v3f(v3fdata);
	event.hudchange.sdata   = new std::string(sdata);
	event.hudchange.data    = intdata;
	event.hudchange.v2s32data = new v2s32(v2s32data);
	m_client_event_queue.push(event);
}

void Client::handleCommand_HudSetFlags(NetworkPacket* pkt)
{
	u32 flags, mask;

	*pkt >> flags >> mask;

	Player *player = m_env.getLocalPlayer();
	assert(player != NULL);

	player->hud_flags &= ~mask;
	player->hud_flags |= flags;
}

void Client::handleCommand_HudSetParam(NetworkPacket* pkt)
{
	u16 param; std::string value;

	*pkt >> param >> value;

	Player *player = m_env.getLocalPlayer();
	assert(player != NULL);

	if (param == HUD_PARAM_HOTBAR_ITEMCOUNT && value.size() == 4) {
		s32 hotbar_itemcount = readS32((u8*) value.c_str());
		if (hotbar_itemcount > 0 && hotbar_itemcount <= HUD_HOTBAR_ITEMCOUNT_MAX)
			player->hud_hotbar_itemcount = hotbar_itemcount;
	}
	else if (param == HUD_PARAM_HOTBAR_IMAGE) {
		((LocalPlayer *) player)->hotbar_image = value;
	}
	else if (param == HUD_PARAM_HOTBAR_SELECTED_IMAGE) {
		((LocalPlayer *) player)->hotbar_selected_image = value;
	}
}

void Client::handleCommand_HudSetSky(NetworkPacket* pkt)
{
	std::string datastring(pkt->getString(0), pkt->getSize());
	std::istringstream is(datastring, std::ios_base::binary);

	video::SColor *bgcolor           = new video::SColor(readARGB8(is));
	std::string *type                = new std::string(deSerializeString(is));
	u16 count                        = readU16(is);
	std::vector<std::string> *params = new std::vector<std::string>;

	for (size_t i = 0; i < count; i++)
		params->push_back(deSerializeString(is));

	ClientEvent event;
	event.type            = CE_SET_SKY;
	event.set_sky.bgcolor = bgcolor;
	event.set_sky.type    = type;
	event.set_sky.params  = params;
	m_client_event_queue.push(event);
}

void Client::handleCommand_OverrideDayNightRatio(NetworkPacket* pkt)
{
	bool do_override;
	u16 day_night_ratio_u;

	*pkt >> do_override >> day_night_ratio_u;

	float day_night_ratio_f = (float)day_night_ratio_u / 65536;

	ClientEvent event;
	event.type                                 = CE_OVERRIDE_DAY_NIGHT_RATIO;
	event.override_day_night_ratio.do_override = do_override;
	event.override_day_night_ratio.ratio_f     = day_night_ratio_f;
	m_client_event_queue.push(event);
}

void Client::handleCommand_LocalPlayerAnimations(NetworkPacket* pkt)
{
	LocalPlayer *player = m_env.getLocalPlayer();
	assert(player != NULL);

	*pkt >> player->local_animations[0];
	*pkt >> player->local_animations[1];
	*pkt >> player->local_animations[2];
	*pkt >> player->local_animations[3];
	*pkt >> player->local_animation_speed;
}

void Client::handleCommand_EyeOffset(NetworkPacket* pkt)
{
	LocalPlayer *player = m_env.getLocalPlayer();
	assert(player != NULL);

	*pkt >> player->eye_offset_first >> player->eye_offset_third;
}


#endif
