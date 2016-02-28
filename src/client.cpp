/*
client.cpp
Copyright (C) 2013 celeron55, Perttu Ahola <celeron55@gmail.com>
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

#include <iostream>
#include <algorithm>
#include <sstream>
#include <IFileSystem.h>
#include "threading/mutex_auto_lock.h"
#include "util/auth.h"
#include "util/directiontables.h"
#include "util/pointedthing.h"
#include "util/serialize.h"
#include "util/string.h"
#include "strfnd.h"
#include "util/srp.h"
#include "client.h"
#include "network/clientopcodes.h"
#include "network/networkprotocol.h"
#include "filesys.h"
#include "porting.h"
#include "mapblock_mesh.h"
#include "mapblock.h"
#include "minimap.h"
#include "settings.h"
#include "profiler.h"
#include "gettext.h"
#include "log_types.h"
#include "nodemetadata.h"
#include "nodedef.h"
#include "itemdef.h"
#include "shader.h"
#include "util/base64.h"
#include "clientmap.h"
#include "clientmedia.h"
#include "sound.h"
#include "IMeshCache.h"
#include "serialization.h"
#include "config.h"
#include "version.h"
#include "drawscene.h"
#include "database-sqlite3.h"
//#include "serialization.h"
#include "guiscalingfilter.h"

#include "database.h"
#include "server.h"
#include "emerge.h"
#if !MINETEST_PROTO
#include "network/fm_clientpacketsender.cpp"
#endif
#include "chat.h"


extern gui::IGUIEnvironment* guienv;

#include "msgpack_fix.h"

/*
	MeshUpdateQueue
*/

MeshUpdateQueue::MeshUpdateQueue()
{
}

MeshUpdateQueue::~MeshUpdateQueue()
{
}

unsigned int MeshUpdateQueue::addBlock(v3POS p, std::shared_ptr<MeshMakeData> data, bool urgent)
{
	DSTACK(FUNCTION_NAME);

	auto lock = m_queue.lock_unique_rec();
	unsigned int range = urgent ? 0 : 1 + data->range + data->step * 10;
	if (m_process.count(p)) {
		if (!urgent)
			range += 3;
	} else if (m_ranges.count(p)) {
		auto range_old = m_ranges[p];
		auto & rmap = m_queue.get(range_old);
		if (range_old > 0 && range != range_old)  {
			m_ranges.erase(p);
			rmap.erase(p);
			if (rmap.empty())
				m_queue.erase(range_old);
		} else {
			rmap[p] = data;
			return m_ranges.size();
		}
	}
	auto & rmap = m_queue.get(range);
	rmap[p] = data;
	m_ranges[p] = range;
	g_profiler->avg("Client: mesh make queue", m_ranges.size());
	return m_ranges.size();
}

std::shared_ptr<MeshMakeData> MeshUpdateQueue::pop()
{
	auto lock = m_queue.lock_unique_rec();
	for (auto & it : m_queue) {
		auto & rmap = it.second;
		auto begin = rmap.begin();
		auto data = begin->second;
		m_ranges.erase(begin->first);
		rmap.erase(begin->first);
		if (rmap.empty())
			m_queue.erase(it.first);
		return data;
	}
	return nullptr;
}

/*
	MeshUpdateThread
*/

void MeshUpdateThread::enqueueUpdate(v3s16 p, std::shared_ptr<MeshMakeData> data,
		bool urgent)
{
	m_queue_in.addBlock(p, data, urgent);
	deferUpdate();
}

void MeshUpdateThread::doUpdate()
{
	std::shared_ptr<MeshMakeData> q;
	while ((q = m_queue_in.pop())) {
		try {
		m_queue_in.m_process.set(q->m_blockpos, 1);

		ScopeProfiler sp(g_profiler, "Client: Mesh making " + itos(q->step));

		m_queue_out.push_back(MeshUpdateResult(q->m_blockpos, MapBlock::mesh_type(new MapBlockMesh(q.get(), m_camera_offset))));

		m_queue_in.m_process.erase(q->m_blockpos);

#if _MSC_VER
		sleep_ms(1); // dont overflow gpu, fix lag and spikes on drawtime
#endif

#if !EXEPTION_DEBUG
		} catch (BaseException &e) {
			errorstream<<"MeshUpdateThread: exception: "<<e.what()<<std::endl;
		} catch(std::exception &e) {
			errorstream<<"MeshUpdateThread: exception: "<<e.what()<<std::endl;
		} catch (...) {
			errorstream<<"MeshUpdateThread: Ooops..."<<std::endl;
#else
		} catch (int) { //nothing
#endif
		}

	}
}

/*
	Client
*/

Client::Client(
		IrrlichtDevice *device,
		const char *playername,
		std::string password,
		bool is_simple_singleplayer_game,
		MapDrawControl &control,
		IWritableTextureSource *tsrc,
		IWritableShaderSource *shsrc,
		IWritableItemDefManager *itemdef,
		IWritableNodeDefManager *nodedef,
		ISoundManager *sound,
		MtEventManager *event,
		bool ipv6
):
	m_packetcounter_timer(0.0),
	m_connection_reinit_timer(0.1),
	m_avg_rtt_timer(0.0),
	m_playerpos_send_timer(0.0),
	m_ignore_damage_timer(0.0),
	m_tsrc(tsrc),
	m_shsrc(shsrc),
	m_itemdef(itemdef),
	m_nodedef(nodedef),
	m_sound(sound),
	m_event(event),
	m_mesh_update_thread(),
	m_env(
		new ClientMap(this, this, control,
			device->getSceneManager()->getRootSceneNode(),
			device->getSceneManager(), 666),
		device->getSceneManager(),
		tsrc, this, device
	),
	m_particle_manager(&m_env),
	m_con(PROTOCOL_ID, is_simple_singleplayer_game ? MAX_PACKET_SIZE_SINGLEPLAYER : MAX_PACKET_SIZE, CONNECTION_TIMEOUT, ipv6, this),
	m_device(device),
	m_camera(NULL),
	m_minimap_disabled_by_server(false),
	m_server_ser_ver(SER_FMT_VER_INVALID),
	m_proto_ver(0),
	m_playeritem(0),
	m_previous_playeritem(0),
	m_inventory_updated(false),
	m_inventory_from_server(NULL),
	m_inventory_from_server_age(0.0),
	m_animation_time(0),
	m_crack_level(-1),
	m_crack_pos(0,0,0),
	m_map_seed(0),
	m_password(password),
	m_chosen_auth_mech(AUTH_MECHANISM_NONE),
	m_auth_data(NULL),
	m_access_denied(false),
	m_access_denied_reconnect(false),
	m_itemdef_received(false),
	m_nodedef_received(false),
	m_media_downloader(new ClientMediaDownloader()),
	m_time_of_day_set(false),
	m_last_time_of_day_f(-1),
	m_time_of_day_update_timer(0),
	m_recommended_send_interval(0.1),
	m_removed_sounds_check_timer(0),
	m_uptime(0),
	m_simple_singleplayer_mode(is_simple_singleplayer_game),
	m_timelapse_timer(-1),
	m_state(LC_Created),
	m_localdb(NULL)
{
	// Add local player
	m_env.addPlayer(new LocalPlayer(this, playername));

	m_mapper = new Mapper(device, this);
	//m_cache_save_interval = g_settings->getU16("server_map_save_interval");

	m_cache_smooth_lighting = g_settings->getBool("smooth_lighting");
	m_cache_enable_shaders  = g_settings->getBool("enable_shaders");
	m_cache_use_tangent_vertices = m_cache_enable_shaders && (
		g_settings->getBool("enable_bumpmapping") || 
		g_settings->getBool("enable_parallax_occlusion"));
}

void Client::Stop()
{
	//request all client managed threads to stop
	m_mesh_update_thread.stop();
	// Save local server map
	if (m_localdb) {
		actionstream << "Local map saving ended" << std::endl;
		m_localdb->endSave();
	}

	delete m_localserver;
	delete m_localdb;
}

Client::~Client()
{
	m_con.Disconnect();

	m_mesh_update_thread.stop();
	m_mesh_update_thread.wait();
/*
	while (!m_mesh_update_thread.m_queue_out.empty()) {
		MeshUpdateResult r = m_mesh_update_thread.m_queue_out.pop_frontNoEx();
		delete r.mesh;
	}
*/

	delete m_inventory_from_server;

	// Delete detached inventories
	for (std::map<std::string, Inventory*>::iterator
			i = m_detached_inventories.begin();
			i != m_detached_inventories.end(); ++i) {
		delete i->second;
	}

	// cleanup 3d model meshes on client shutdown
	while (m_device->getSceneManager()->getMeshCache()->getMeshCount() != 0) {
		scene::IAnimatedMesh *mesh =
			m_device->getSceneManager()->getMeshCache()->getMeshByIndex(0);

		if (mesh != NULL)
			m_device->getSceneManager()->getMeshCache()->removeMesh(mesh);
	}

	delete m_mapper;
	delete m_media_downloader;
}

void Client::connect(Address address,
		const std::string &address_name,
		bool is_local_server)
{
	DSTACK(FUNCTION_NAME);

	initLocalMapSaving(address, address_name, is_local_server);

	m_con.Connect(address);
}

void Client::step(float dtime)
{
	DSTACK(FUNCTION_NAME);

	m_uptime += dtime;

	// Limit a bit
	if(dtime > 2.0)
		dtime = 2.0;

	if(m_ignore_damage_timer > dtime)
		m_ignore_damage_timer -= dtime;
	else
		m_ignore_damage_timer = 0.0;

	m_animation_time += dtime;
	if(m_animation_time > 60.0)
		m_animation_time -= 60.0;

	m_time_of_day_update_timer += dtime;

	ReceiveAll();

	/*
		Packet counter
	*/
	{
		float &counter = m_packetcounter_timer;
		counter -= dtime;
		if(counter <= 0.0)
		{
			counter = 20.0;

			verbosestream << "Client packetcounter (" << m_packetcounter_timer
					<< "):"<<std::endl;
			m_packetcounter.print(verbosestream);
			m_packetcounter.clear();
		}
	}

	// UGLY hack to fix 2 second startup delay caused by non existent
	// server client startup synchronization in local server or singleplayer mode
	static bool initial_step = true;
	if (initial_step) {
		initial_step = false;
	}
	else if(m_state == LC_Created) {
		float &counter = m_connection_reinit_timer;
		counter -= dtime;
		if(counter <= 0.0) {
			counter = 2.0;

			Player *myplayer = m_env.getLocalPlayer();
			FATAL_ERROR_IF(myplayer == NULL, "Local player not found in environment.");

			// Send TOSERVER_INIT_LEGACY
			// [0] u16 TOSERVER_INIT_LEGACY
			// [2] u8 SER_FMT_VER_HIGHEST_READ
			// [3] u8[20] player_name
			// [23] u8[28] password (new in some version)
			// [51] u16 minimum supported network protocol version (added sometime)
			// [53] u16 maximum supported network protocol version (added later than the previous one)

/*
			char pName[PLAYERNAME_SIZE];
			char pPassword[PASSWORD_SIZE];
			memset(pName, 0, PLAYERNAME_SIZE * sizeof(char));
			memset(pPassword, 0, PASSWORD_SIZE * sizeof(char));

*/
			std::string hashed_password = translatePassword(myplayer->getName(), m_password);
/*
			snprintf(pName, PLAYERNAME_SIZE, "%s", myplayer->getName());
			snprintf(pPassword, PASSWORD_SIZE, "%s", hashed_password.c_str());

			sendLegacyInit(pName, pPassword);
*/
			sendLegacyInit(myplayer->getName(), hashed_password);

			if (LATEST_PROTOCOL_VERSION >= 25)
				sendInit(myplayer->getName());
		}

		// Not connected, return
		return;
	}

	/*
		Do stuff if connected
	*/
	unsigned int max_cycle_ms = 200/g_settings->getFloat("wanted_fps");

	/*
		Run Map's timers and unload unused data
	*/
	const float map_timer_and_unload_dtime = 10.25;
	if(m_map_timer_and_unload_interval.step(dtime, map_timer_and_unload_dtime)) {
		ScopeProfiler sp(g_profiler, "Client: map timer and unload");
		std::vector<v3s16> deleted_blocks;
		
		if(m_env.getMap().timerUpdate(m_uptime,
			g_settings->getFloat("client_unload_unused_data_timeout"),
			g_settings->getS32("client_mapblock_limit"),
			max_cycle_ms,
			&deleted_blocks))
			m_map_timer_and_unload_interval.run_next(map_timer_and_unload_dtime);

		/*if(deleted_blocks.size() > 0)
			infostream<<"Client: Unloaded "<<deleted_blocks.size()
					<<" unused blocks"<<std::endl;*/

		/*
			Send info to server
			NOTE: This loop is intentionally iterated the way it is.
		*/

		auto i = deleted_blocks.begin();
		std::vector<v3s16> sendlist;
		for(;;) {
			if(sendlist.size() == 255 || i == deleted_blocks.end()) {
				if(sendlist.empty())
					break;

				sendDeletedBlocks(sendlist);

				if(i == deleted_blocks.end())
					break;

				sendlist.clear();
			}

			sendlist.push_back(*i);
			++i;
		}
	}

	/*
		Handle environment
	*/
	// Control local player (0ms)
	LocalPlayer *player = m_env.getLocalPlayer();
	assert(player != NULL);
	player->applyControl(dtime, &m_env);

	// Step environment
	m_env.step(dtime, m_uptime, max_cycle_ms);

	/*
		Get events
	*/
	for(;;) {
		ClientEnvEvent event = m_env.getClientEvent();
		if(event.type == CEE_NONE) {
			break;
		}
		else if(event.type == CEE_PLAYER_DAMAGE) {
			if(m_ignore_damage_timer <= 0) {
				u8 damage = event.player_damage.amount;

				if(event.player_damage.send_to_server)
					sendDamage(damage);

				// Add to ClientEvent queue
				ClientEvent event;
				event.type = CE_PLAYER_DAMAGE;
				event.player_damage.amount = damage;
				m_client_event_queue.push(event);
			}
		}
		else if(event.type == CEE_PLAYER_BREATH) {
				u16 breath = event.player_breath.amount;
				sendBreath(breath);
		}
	}

	/*
		Print some info
	*/
	float &counter = m_avg_rtt_timer;
	counter += dtime;
	if(counter >= 10) {
		counter = 0.0;
		// connectedAndInitialized() is true, peer exists.
#if MINETEST_PROTO
		float avg_rtt = getRTT();
		infostream<<"Client: avg_rtt="<<avg_rtt<<std::endl;
#endif

		sendDrawControl(); //not very good place. maybe 5s timer better
	}

	/*
		Send player position to server
	*/
	{
		float &counter = m_playerpos_send_timer;
		counter += dtime;
		if((m_state == LC_Ready) && (counter >= m_recommended_send_interval)) {
			counter = 0.0;
			sendPlayerPos();
		}
	}

	/*
		Replace updated meshes
	*/
	{
		TimeTaker timer_step("Client: Replace updated meshes");

		int num_processed_meshes = 0;
		u32 end_ms = porting::getTimeMs() + 10;

		/*
		auto lock = m_env.getMap().m_blocks.try_lock_shared_rec();
		if (!lock->owns_lock()) {
			infostream<<"skip updating meshes"<<std::endl;
		} else 
		*/
		{

		while (!m_mesh_update_thread.m_queue_out.empty_try()) {
			num_processed_meshes++;

			MinimapMapblock *minimap_mapblock = NULL;
			bool do_mapper_update = true;

			MeshUpdateResult r = m_mesh_update_thread.m_queue_out.pop_frontNoEx();
			if (!r.mesh)
				continue;
			auto block = m_env.getMap().getBlock(r.p);
			if(block) {
				block->setMesh(r.mesh);
				if (r.mesh) {
					minimap_mapblock = r.mesh->moveMinimapMapblock();
					if (minimap_mapblock == NULL)
						do_mapper_update = false;
				}

			} else {
				//delete r.mesh;
			}
			if (do_mapper_update)
				m_mapper->addBlock(r.p, minimap_mapblock);
			if (porting::getTimeMs() > end_ms) {
				break;
			}
		}
		if(num_processed_meshes > 0)
			g_profiler->graphAdd("num_processed_meshes", num_processed_meshes);
		}
	}

	/*
		Load fetched media
	*/
	if (m_media_downloader && m_media_downloader->isStarted()) {
		m_media_downloader->step(this);
		if (m_media_downloader->isDone()) {
			received_media();
			delete m_media_downloader;
			m_media_downloader = NULL;
		}
	}

	/*
		If the server didn't update the inventory in a while, revert
		the local inventory (so the player notices the lag problem
		and knows something is wrong).
	*/
	if(m_inventory_from_server) {
		float interval = 10.0;
		float count_before = floor(m_inventory_from_server_age / interval);

		m_inventory_from_server_age += dtime;

		float count_after = floor(m_inventory_from_server_age / interval);

		if(count_after != count_before) {
			// Do this every <interval> seconds after TOCLIENT_INVENTORY
			// Reset the locally changed inventory to the authoritative inventory
			Player *player = m_env.getLocalPlayer();
			player->inventory = *m_inventory_from_server;
			m_inventory_updated = true;
		}
	}

	/*
		Update positions of sounds attached to objects
	*/
	{
		for(std::map<int, u16>::iterator
				i = m_sounds_to_objects.begin();
				i != m_sounds_to_objects.end(); ++i)
		{
			int client_id = i->first;
			u16 object_id = i->second;
			ClientActiveObject *cao = m_env.getActiveObject(object_id);
			if(!cao)
				continue;
			v3f pos = cao->getPosition();
			m_sound->updateSoundPosition(client_id, pos);
		}
	}

	/*
		Handle removed remotely initiated sounds
	*/
	m_removed_sounds_check_timer += dtime;
	if(m_removed_sounds_check_timer >= 2.32)
	{
		m_removed_sounds_check_timer = 0;
		// Find removed sounds and clear references to them
		std::vector<s32> removed_server_ids;
		for(std::map<s32, int>::iterator
				i = m_sounds_server_to_client.begin();
				i != m_sounds_server_to_client.end();)
		{
			s32 server_id = i->first;
			int client_id = i->second;
			++i;
			if(!m_sound->soundExists(client_id)) {
				m_sounds_server_to_client.erase(server_id);
				m_sounds_client_to_server.erase(client_id);
				m_sounds_to_objects.erase(client_id);
				removed_server_ids.push_back(server_id);
			}
		}
		// Sync to server
		if(!removed_server_ids.empty()) {
			sendRemovedSounds(removed_server_ids);
		}
	}

	float timelapse = g_settings->getFloat("timelapse");
	if (timelapse) {
		m_timelapse_timer += dtime;
		if (m_timelapse_timer > timelapse) {
			m_timelapse_timer = 0;
			makeScreenshot("timelapse_");
		}
	}

}

bool Client::loadMedia(const std::string &data, const std::string &filename)
{
	// Silly irrlicht's const-incorrectness
	Buffer<char> data_rw(data.c_str(), data.size());

	std::string name;

	const char *image_ext[] = {
		".png", ".jpg", ".bmp", ".tga",
		".pcx", ".ppm", ".psd", ".wal", ".rgb",
		NULL
	};
	name = removeStringEnd(filename, image_ext);
	if(name != "")
	{
		verbosestream<<"Client: Attempting to load image "
		<<"file \""<<filename<<"\""<<std::endl;

		io::IFileSystem *irrfs = m_device->getFileSystem();
		video::IVideoDriver *vdrv = m_device->getVideoDriver();

		// Create an irrlicht memory file
		io::IReadFile *rfile = irrfs->createMemoryReadFile(
				*data_rw, data_rw.getSize(), "_tempreadfile");
		FATAL_ERROR_IF(!rfile, "Could not create irrlicht memory file.");
		// Read image
		video::IImage *img = vdrv->createImageFromFile(rfile);
		if(!img){
			errorstream<<"Client: Cannot create image from data of "
					<<"file \""<<filename<<"\""<<std::endl;
			rfile->drop();
			return false;
		}
		else {
			m_tsrc->insertSourceImage(filename, img);
			img->drop();
			rfile->drop();
			return true;
		}
	}

	const char *sound_ext[] = {
		".0.ogg", ".1.ogg", ".2.ogg", ".3.ogg", ".4.ogg",
		".5.ogg", ".6.ogg", ".7.ogg", ".8.ogg", ".9.ogg",
		".ogg", NULL
	};
	name = removeStringEnd(filename, sound_ext);
	if(name != "")
	{
		verbosestream<<"Client: Attempting to load sound "
		<<"file \""<<filename<<"\""<<std::endl;
		m_sound->loadSoundData(name, data);
		return true;
	}

	const char *model_ext[] = {
		".x", ".b3d", ".md2", ".obj",
		NULL
	};
	name = removeStringEnd(filename, model_ext);
	if(name != "")
	{
		verbosestream<<"Client: Storing model into memory: "
				<<"\""<<filename<<"\""<<std::endl;
		if(m_mesh_data.count(filename))
			errorstream<<"Multiple models with name \""<<filename.c_str()
					<<"\" found; replacing previous model"<<std::endl;
		m_mesh_data[filename] = data;
		return true;
	}

	errorstream<<"Client: Don't know how to load file \""
			<<filename<<"\""<<std::endl;
	return false;
}

// Virtual methods from con::PeerHandler
void Client::peerAdded(u16 peer_id)
{
	infostream << "Client::peerAdded(): peer->id="
			<< peer_id << std::endl;
}
void Client::deletingPeer(u16 peer_id, bool timeout)
{
	infostream << "Client::deletingPeer(): "
			"Server Peer is getting deleted "
			<< "(timeout=" << timeout << ")" << std::endl;

	if (timeout) {
		m_access_denied = true;
		m_access_denied_reason = _("Connection timed out.");
	}
}

/*
	u16 command
	u16 number of files requested
	for each file {
		u16 length of name
		string name
	}
*/

#if MINETEST_PROTO
void Client::request_media(const std::vector<std::string> &file_requests)
{
	std::ostringstream os(std::ios_base::binary);
	writeU16(os, TOSERVER_REQUEST_MEDIA);
	size_t file_requests_size = file_requests.size();

	FATAL_ERROR_IF(file_requests_size > 0xFFFF, "Unsupported number of file requests");

	// Packet dynamicly resized
	NetworkPacket pkt(TOSERVER_REQUEST_MEDIA, 2 + 0);

	pkt << (u16) (file_requests_size & 0xFFFF);

	for(std::vector<std::string>::const_iterator i = file_requests.begin();
			i != file_requests.end(); ++i) {
		pkt << (*i);
	}

	Send(&pkt);

	infostream << "Client: Sending media request list to server ("
			<< file_requests.size() << " files. packet size)" << std::endl;
}

void Client::received_media()
{
	NetworkPacket pkt(TOSERVER_RECEIVED_MEDIA, 0);
	Send(&pkt);
	infostream << "Client: Notifying server that we received all media"
			<< std::endl;
}
#endif


void Client::initLocalMapSaving(const Address &address,
		const std::string &hostname,
		bool is_local_server)
{

	m_localserver = nullptr;

	m_localdb = NULL;

	if (!g_settings->getBool("enable_local_map_saving") || is_local_server) {
		return;
	}

	std::string address_replaced = hostname + "_" + to_string(address.getPort());
	replace( address_replaced.begin(), address_replaced.end(), ':', '_' );

	const std::string world_path = porting::path_user
		+ DIR_DELIM + "worlds"
		+ DIR_DELIM + "server_"
		+ address_replaced;

	SubgameSpec gamespec;

	if (!getWorldExists(world_path)) {
		gamespec = findSubgame(g_settings->get("default_game"));
		if (!gamespec.isValid())
			gamespec = findSubgame("minimal");
	} else {
		gamespec = findWorldSubgame(world_path);
	}

	fs::CreateAllDirs(world_path);

#if !MINETEST_PROTO
	m_localserver = new Server(world_path, gamespec, false, false);
#endif
	/*
	m_localdb = new Database_SQLite3(world_path);
	m_localdb->beginSave();
	*/
	actionstream << "Local map saving started, map will be saved at '" << world_path << "'" << std::endl;
}

void Client::ReceiveAll()
{
	DSTACK(FUNCTION_NAME);
	auto end_ms = porting::getTimeMs() + 10;
	for(;;)
	{
#if MINETEST_PROTO
		try {
#endif
			if (!Receive())
				break;
			g_profiler->graphAdd("client_received_packets", 1);

#if MINETEST_PROTO
		}
		catch(con::NoIncomingDataException &e) {
			break;
		}
		catch(con::InvalidIncomingDataException &e) {
			infostream<<"Client::ReceiveAll(): "
					"InvalidIncomingDataException: what()="
					<<e.what()<<std::endl;
		}
#endif
		// Limit time even if there would be huge amounts of data to
		// process
		if(porting::getTimeMs() > end_ms)
			break;
	}
}

bool Client::Receive()
{
	DSTACK(FUNCTION_NAME);
	NetworkPacket pkt;
	if (!m_con.Receive(&pkt))
		return false;

	ProcessData(&pkt);
	return true;
}

//FMTODO
#if MINETEST_PROTO

inline void Client::handleCommand(NetworkPacket* pkt)
{
	const ToClientCommandHandler& opHandle = toClientCommandTable[pkt->getCommand()];
	(this->*opHandle.handler)(pkt);
}

/*
	sender_peer_id given to this shall be quaranteed to be a valid peer
*/
void Client::ProcessData(NetworkPacket *pkt)
{
	DSTACK(FUNCTION_NAME);

	ToClientCommand command = (ToClientCommand) pkt->getCommand();
	u32 sender_peer_id = pkt->getPeerId();

	//infostream<<"Client: received command="<<command<<std::endl;
	m_packetcounter.add((u16)command);

	/*
		If this check is removed, be sure to change the queue
		system to know the ids
	*/
	if(sender_peer_id != PEER_ID_SERVER) {
		infostream << "Client::ProcessData(): Discarding data not "
			"coming from server: peer_id=" << sender_peer_id
			<< std::endl;
		return;
	}

	// Command must be handled into ToClientCommandHandler
	if (command >= TOCLIENT_NUM_MSG_TYPES) {
		infostream << "Client: Ignoring unknown command "
			<< command << std::endl;
		return;
	}

	/*
	 * Those packets are handled before m_server_ser_ver is set, it's normal
	 * But we must use the new ToClientConnectionState in the future,
	 * as a byte mask
	 */
	if(toClientCommandTable[command].state == TOCLIENT_STATE_NOT_CONNECTED) {
		handleCommand(pkt);
		return;
	}

	if(m_server_ser_ver == SER_FMT_VER_INVALID) {
		infostream << "Client: Server serialization"
				" format invalid or not initialized."
				" Skipping incoming command=" << command << std::endl;
		return;
	}

	/*
	  Handle runtime commands
	*/

	handleCommand(pkt);
}
#endif


/*
void Client::Send(u16 channelnum, SharedBuffer<u8> data, bool reliable)
{
	//MutexAutoLock lock(m_con_mutex); //bulk comment-out
	m_con.Send(PEER_ID_SERVER, channelnum, data, reliable);
}
*/

#if !MINETEST_PROTO
void Client::Send(u16 channelnum, const msgpack::sbuffer &data, bool reliable) {
	g_profiler->add("Client::Send", 1);
	m_con.Send(PEER_ID_SERVER, channelnum, data, reliable);
}
#else

void Client::Send(NetworkPacket* pkt)
{
	g_profiler->add("Client::Send", 1);
	m_con.Send(PEER_ID_SERVER,
		serverCommandFactoryTable[pkt->getCommand()].channel,
		pkt,
		serverCommandFactoryTable[pkt->getCommand()].reliable);
}
#endif

#if MINETEST_PROTO
void Client::interact(u8 action, const PointedThing& pointed)
{
	if(m_state != LC_Ready) {
		errorstream << "Client::interact() "
				"Canceled (not connected)"
				<< std::endl;
		return;
	}

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
		5: perform secondary action of item
	*/

	NetworkPacket pkt(TOSERVER_INTERACT, 1 + 2 + 0);

	pkt << action;
	pkt << (u16)getPlayerItem();

	std::ostringstream tmp_os(std::ios::binary);
	pointed.serialize(tmp_os);

	pkt.putLongString(tmp_os.str());

	Send(&pkt);
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
			srp_user_delete((SRPUser *) m_auth_data);
			m_auth_data = NULL;
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

void Client::sendLegacyInit(const std::string &playerName, const std::string &playerPassword)
{
	NetworkPacket pkt(TOSERVER_INIT_LEGACY,
			1 + PLAYERNAME_SIZE + PASSWORD_SIZE + 2 + 2);

	pkt << (u8) SER_FMT_VER_HIGHEST_READ;
	
	std::string tmp = playerName;
	tmp.resize(tmp.size()+PLAYERNAME_SIZE);
	pkt.putRawString(tmp.c_str(),PLAYERNAME_SIZE);
	tmp = playerPassword;
	tmp.resize(tmp.size()+PASSWORD_SIZE);
	pkt.putRawString(tmp.c_str(), PASSWORD_SIZE);
	pkt << (u16) CLIENT_PROTOCOL_VERSION_MIN << (u16) CLIENT_PROTOCOL_VERSION_MAX;

	Send(&pkt);
}

void Client::sendInit(const std::string &playerName)
{
	NetworkPacket pkt(TOSERVER_INIT, 1 + 2 + 2 + (1 + playerName.size()));

	// we don't support network compression yet
	u16 supp_comp_modes = NETPROTO_COMPRESSION_NONE;
	pkt << (u8) SER_FMT_VER_HIGHEST_READ << (u16) supp_comp_modes;
	pkt << (u16) CLIENT_PROTOCOL_VERSION_MIN << (u16) CLIENT_PROTOCOL_VERSION_MAX;
	pkt << playerName;

	Send(&pkt);
}

void Client::startAuth(AuthMechanism chosen_auth_mechanism)
{
	m_chosen_auth_mech = chosen_auth_mechanism;

	switch (chosen_auth_mechanism) {
		case AUTH_MECHANISM_FIRST_SRP: {
			// send srp verifier to server
			NetworkPacket resp_pkt(TOSERVER_FIRST_SRP, 0);
			char *salt, *bytes_v;
			std::size_t len_salt, len_v;
			salt = NULL;
			getSRPVerifier(getPlayerName(), m_password,
				&salt, &len_salt, &bytes_v, &len_v);
			resp_pkt
				<< std::string((char*)salt, len_salt)
				<< std::string((char*)bytes_v, len_v)
				<< (u8)((m_password == "") ? 1 : 0);
			free(salt);
			free(bytes_v);
			Send(&resp_pkt);
			break;
		}
		case AUTH_MECHANISM_SRP:
		case AUTH_MECHANISM_LEGACY_PASSWORD: {
			u8 based_on = 1;

			if (chosen_auth_mechanism == AUTH_MECHANISM_LEGACY_PASSWORD) {
				m_password = translatePassword(getPlayerName(), m_password);
				based_on = 0;
			}

			std::string playername_u = lowercase(getPlayerName());
			m_auth_data = srp_user_new(SRP_SHA256, SRP_NG_2048,
				getPlayerName().c_str(), playername_u.c_str(),
				(const unsigned char *) m_password.c_str(),
				m_password.length(), NULL, NULL);
			char *bytes_A = 0;
			size_t len_A = 0;
			SRP_Result res = srp_user_start_authentication(
				(struct SRPUser *) m_auth_data, NULL, NULL, 0,
				(unsigned char **) &bytes_A, &len_A);
			FATAL_ERROR_IF(res != SRP_OK, "Creating local SRP user failed.");

			NetworkPacket resp_pkt(TOSERVER_SRP_BYTES_A, 0);
			resp_pkt << std::string(bytes_A, len_A) << based_on;
			Send(&resp_pkt);
			break;
		}
		case AUTH_MECHANISM_NONE:
			break; // not handled in this method
	}
}

void Client::sendDeletedBlocks(std::vector<v3s16> &blocks)
{
	NetworkPacket pkt(TOSERVER_DELETEDBLOCKS, 1 + sizeof(v3s16) * blocks.size());

	pkt << (u8) blocks.size();

	u32 k = 0;
	for(std::vector<v3s16>::iterator
			j = blocks.begin();
			j != blocks.end(); ++j) {
		pkt << *j;
		k++;
	}

	Send(&pkt);
}

void Client::sendGotBlocks(v3s16 block)
{
	NetworkPacket pkt(TOSERVER_GOTBLOCKS, 1 + 6);
	pkt << (u8) 1 << block;
	Send(&pkt);
}

void Client::sendRemovedSounds(std::vector<s32> &soundList)
{
	size_t server_ids = soundList.size();
	assert(server_ids <= 0xFFFF);

	NetworkPacket pkt(TOSERVER_REMOVED_SOUNDS, 2 + server_ids * 4);

	pkt << (u16) (server_ids & 0xFFFF);

	for(std::vector<s32>::iterator i = soundList.begin();
			i != soundList.end(); ++i)
		pkt << *i;

	Send(&pkt);
}

void Client::sendNodemetaFields(v3s16 p, const std::string &formname,
		const StringMap &fields)
{
	size_t fields_size = fields.size();

	FATAL_ERROR_IF(fields_size > 0xFFFF, "Unsupported number of nodemeta fields");

	NetworkPacket pkt(TOSERVER_NODEMETA_FIELDS, 0);

	pkt << p << formname << (u16) (fields_size & 0xFFFF);

	StringMap::const_iterator it;
	for (it = fields.begin(); it != fields.end(); ++it) {
		const std::string &name = it->first;
		const std::string &value = it->second;
		pkt << name;
		pkt.putLongString(value);
	}

	Send(&pkt);
}

void Client::sendInventoryFields(const std::string &formname,
		const StringMap &fields)
{
	size_t fields_size = fields.size();
	FATAL_ERROR_IF(fields_size > 0xFFFF, "Unsupported number of inventory fields");

	NetworkPacket pkt(TOSERVER_INVENTORY_FIELDS, 0);
	pkt << formname << (u16) (fields_size & 0xFFFF);

	StringMap::const_iterator it;
	for (it = fields.begin(); it != fields.end(); ++it) {
		const std::string &name  = it->first;
		const std::string &value = it->second;
		pkt << name;
		pkt.putLongString(value);
	}

	Send(&pkt);
}

void Client::sendInventoryAction(InventoryAction *a)
{
	std::ostringstream os(std::ios_base::binary);

	a->serialize(os);

	// Make data buffer
	std::string s = os.str();

	NetworkPacket pkt(TOSERVER_INVENTORY_ACTION, s.size());
	pkt.putRawString(s.c_str(),s.size());

	Send(&pkt);
}

void Client::sendChatMessage(const std::string &message)
{
	NetworkPacket pkt(TOSERVER_CHAT_MESSAGE, 2 + message.size() * sizeof(u16));

	pkt << narrow_to_wide(message);

	Send(&pkt);
}

void Client::sendChangePassword(const std::string &oldpassword,
        const std::string &newpassword)
{
	Player *player = m_env.getLocalPlayer();
	if (player == NULL)
		return;

	std::string playername = player->getName();
	if (m_proto_ver >= 25) {
		// get into sudo mode and then send new password to server
		m_password = oldpassword;
		m_new_password = newpassword;
		startAuth(choseAuthMech(m_sudo_auth_methods));
	} else {
		std::string oldpwd = translatePassword(playername, oldpassword);
		std::string newpwd = translatePassword(playername, newpassword);

		NetworkPacket pkt(TOSERVER_PASSWORD_LEGACY, 2 * PASSWORD_SIZE);

		for (u8 i = 0; i < PASSWORD_SIZE; i++) {
			pkt << (u8) (i < oldpwd.length() ? oldpwd[i] : 0);
		}

		for (u8 i = 0; i < PASSWORD_SIZE; i++) {
			pkt << (u8) (i < newpwd.length() ? newpwd[i] : 0);
		}
		Send(&pkt);
	}
}


void Client::sendDamage(u8 damage)
{
	DSTACK(FUNCTION_NAME);

	NetworkPacket pkt(TOSERVER_DAMAGE, sizeof(u8));
	pkt << damage;
	Send(&pkt);
}

void Client::sendBreath(u16 breath)
{
	DSTACK(FUNCTION_NAME);

	NetworkPacket pkt(TOSERVER_BREATH, sizeof(u16));
	pkt << breath;
	Send(&pkt);
}

void Client::sendRespawn()
{
	DSTACK(FUNCTION_NAME);

	NetworkPacket pkt(TOSERVER_RESPAWN, 0);
	Send(&pkt);
}

void Client::sendReady()
{
	DSTACK(FUNCTION_NAME);

	NetworkPacket pkt(TOSERVER_CLIENT_READY,
			1 + 1 + 1 + 1 + 2 + sizeof(char) * strlen(g_version_hash));

	pkt << (u8) VERSION_MAJOR << (u8) VERSION_MINOR << (u8) VERSION_PATCH
		<< (u8) 0 << (u16) strlen(g_version_hash);

	pkt.putRawString(g_version_hash, (u16) strlen(g_version_hash));
	Send(&pkt);
}

void Client::sendPlayerPos()
{
	LocalPlayer *myplayer = m_env.getLocalPlayer();
	if(myplayer == NULL)
		return;

	// Save bandwidth by only updating position when something changed
	if(myplayer->last_position        == myplayer->getPosition() &&
			myplayer->last_speed      == myplayer->getSpeed()    &&
			myplayer->last_pitch      == myplayer->getPitch()    &&
			myplayer->last_yaw        == myplayer->getYaw()      &&
			myplayer->last_keyPressed == myplayer->keyPressed)
		return;

	myplayer->last_position   = myplayer->getPosition();
	myplayer->last_speed      = myplayer->getSpeed();
	myplayer->last_pitch      = myplayer->getPitch();
	myplayer->last_yaw        = myplayer->getYaw();
	myplayer->last_keyPressed = myplayer->keyPressed;

	u16 our_peer_id;
	{
		//MutexAutoLock lock(m_con_mutex); //bulk comment-out
		our_peer_id = m_con.GetPeerID();
	}

	// Set peer id if not set already
	if(myplayer->peer_id == PEER_ID_INEXISTENT)
		myplayer->peer_id = our_peer_id;

	assert(myplayer->peer_id == our_peer_id);

	v3f pf         = myplayer->getPosition();
	v3f sf         = myplayer->getSpeed();
	s32 pitch      = myplayer->getPitch() * 100;
	s32 yaw        = myplayer->getYaw() * 100;
	u32 keyPressed = myplayer->keyPressed;

	v3s32 position(pf.X*100, pf.Y*100, pf.Z*100);
	v3s32 speed(sf.X*100, sf.Y*100, sf.Z*100);
	/*
		Format:
		[0] v3s32 position*100
		[12] v3s32 speed*100
		[12+12] s32 pitch*100
		[12+12+4] s32 yaw*100
		[12+12+4+4] u32 keyPressed
	*/

	NetworkPacket pkt(TOSERVER_PLAYERPOS, 12 + 12 + 4 + 4 + 4);

	pkt << position << speed << pitch << yaw << keyPressed;

	Send(&pkt);
}

void Client::sendPlayerItem(u16 item)
{
	Player *myplayer = m_env.getLocalPlayer();
	if(myplayer == NULL)
		return;

	u16 our_peer_id = m_con.GetPeerID();

	// Set peer id if not set already
	if(myplayer->peer_id == PEER_ID_INEXISTENT)
		myplayer->peer_id = our_peer_id;
	assert(myplayer->peer_id == our_peer_id);

	NetworkPacket pkt(TOSERVER_PLAYERITEM, 2);

	pkt << item;

	Send(&pkt);
}


void Client::sendDrawControl() { }
#endif


void Client::removeNode(v3s16 p, int fast)
{
	std::map<v3s16, MapBlock*> modified_blocks;

	try {
		m_env.getMap().removeNodeAndUpdate(p, modified_blocks, fast ? fast : 2);
	}
	catch(InvalidPositionException &e) {
	}

	for(std::map<v3s16, MapBlock * >::iterator
			i = modified_blocks.begin();
			i != modified_blocks.end(); ++i) {
		addUpdateMeshTaskWithEdge(i->first, true);
	}
}

void Client::addNode(v3s16 p, MapNode n, bool remove_metadata, int fast)
{
	//TimeTaker timer1("Client::addNode()");

	std::map<v3s16, MapBlock*> modified_blocks;

	try {
		//TimeTaker timer3("Client::addNode(): addNodeAndUpdate");
		m_env.getMap().addNodeAndUpdate(p, n, modified_blocks, remove_metadata, fast ? fast : 2);
	}
	catch(InvalidPositionException &e) {
	}
	addUpdateMeshTaskForNode(p, true);

	for(std::map<v3s16, MapBlock * >::iterator
			i = modified_blocks.begin();
			i != modified_blocks.end(); ++i) {
		addUpdateMeshTaskWithEdge(i->first, true);
	}
}

void Client::setPlayerControl(PlayerControl &control)
{
	LocalPlayer *player = m_env.getLocalPlayer();
	assert(player != NULL);
	player->control = control;
}

void Client::selectPlayerItem(u16 item)
{
	m_previous_playeritem = m_playeritem;
	m_playeritem = item;
	m_inventory_updated = true;
	sendPlayerItem(item);
}

// Returns true if the inventory of the local player has been
// updated from the server. If it is true, it is set to false.
bool Client::getLocalInventoryUpdated()
{
	bool updated = m_inventory_updated;
	m_inventory_updated = false;
	return updated;
}

// Copies the inventory of the local player to parameter
void Client::getLocalInventory(Inventory &dst)
{
	Player *player = m_env.getLocalPlayer();
	assert(player != NULL);
	dst = player->inventory;
}

Inventory* Client::getInventory(const InventoryLocation &loc)
{
	switch(loc.type){
	case InventoryLocation::UNDEFINED:
	{}
	break;
	case InventoryLocation::CURRENT_PLAYER:
	{
		Player *player = m_env.getLocalPlayer();
		assert(player != NULL);
		return &player->inventory;
	}
	break;
	case InventoryLocation::PLAYER:
	{
		Player *player = m_env.getPlayer(loc.name.c_str());
		if(!player)
			return NULL;
		return &player->inventory;
	}
	break;
	case InventoryLocation::NODEMETA:
	{
		NodeMetadata *meta = m_env.getMap().getNodeMetadata(loc.p);
		if(!meta)
			return NULL;
		return meta->getInventory();
	}
	break;
	case InventoryLocation::DETACHED:
	{
		if(m_detached_inventories.count(loc.name) == 0)
			return NULL;
		return m_detached_inventories[loc.name];
	}
	break;
	default:
		FATAL_ERROR("Invalid inventory location type.");
		break;
	}
	return NULL;
}

void Client::inventoryAction(InventoryAction *a)
{
	/*
		Send it to the server
	*/
	sendInventoryAction(a);

	/*
		Predict some local inventory changes
	*/
	a->clientApply(this, this);

	// Remove it
	delete a;
}

ClientActiveObject * Client::getSelectedActiveObject(
		f32 max_d,
		v3f from_pos_f_on_map,
		core::line3d<f32> shootline_on_map
	)
{
	std::vector<DistanceSortedActiveObject> objects;

	m_env.getActiveObjects(from_pos_f_on_map, max_d, objects);

	// Sort them.
	// After this, the closest object is the first in the array.
	std::sort(objects.begin(), objects.end());

	for(u32 i=0; i<objects.size(); i++)
	{
		ClientActiveObject *obj = objects[i].obj;

		aabb3f *selection_box = obj->getSelectionBox();
		if(selection_box == NULL)
			continue;

		v3f pos = obj->getPosition();

		aabb3f offsetted_box(
				selection_box->MinEdge + pos,
				selection_box->MaxEdge + pos
		);

		if(offsetted_box.intersectsWithLine(shootline_on_map))
		{
			return obj;
		}
	}

	return NULL;
}

std::list<std::string> Client::getConnectedPlayerNames()
{
	return m_env.getPlayerNames();
}

float Client::getAnimationTime()
{
	return m_animation_time;
}

int Client::getCrackLevel()
{
	return m_crack_level;
}

void Client::setCrack(int level, v3s16 pos)
{
	int old_crack_level = m_crack_level;
	v3s16 old_crack_pos = m_crack_pos;

	m_crack_level = level;
	m_crack_pos = pos;

	if(old_crack_level >= 0 && (level < 0 || pos != old_crack_pos))
	{
		// remove old crack
		addUpdateMeshTaskForNode(old_crack_pos, true);
	}
	if(level >= 0 && (old_crack_level < 0 || pos != old_crack_pos))
	{
		// add new crack
		addUpdateMeshTaskForNode(pos, true);
	}
}

u16 Client::getHP()
{
	Player *player = m_env.getLocalPlayer();
	assert(player != NULL);
	return player->hp;
}

u16 Client::getBreath()
{
	Player *player = m_env.getLocalPlayer();
	assert(player != NULL);
	return player->getBreath();
}

bool Client::getChatMessage(std::string &message)
{
	if(m_chat_queue.size() == 0)
		return false;
	message = m_chat_queue.pop_front();
	return true;
}

void Client::typeChatMessage(const std::string &message)
{
	// Discard empty line
	if(message.empty())
		return;

	if (message[0] == '/') {
		// TODO register client commands in help
		std::string command = message.substr(1,-1);
		// Clears on-screen chat messages
		if (command.compare("clear") == 0) {
			chat_backend->clearRecentChat();
			return;
		// it's kinda self-evident when you run a local command
		} else {
			m_chat_queue.push("issued command: " + message);
		}
	}
	//freeminer display self message after recieving from server
#if MINETEST_PROTO
	else
	{
		LocalPlayer *player = m_env.getLocalPlayer();
		assert(player != NULL);
		std::string name = (player->getName());
		m_chat_queue.push(std::string() + "<" + name + "> " + message);
	}
#endif

	// Send to others
	sendChatMessage(message);
}

void Client::addUpdateMeshTask(v3s16 p, bool urgent, int step)
{
	//ScopeProfiler sp(g_profiler, "Client: Mesh prepare");
	MapBlock *b = m_env.getMap().getBlockNoCreateNoEx(p);
	if(b == NULL)
		return;

	/*
		Create a task to update the mesh of the block
	*/
	auto & draw_control = m_env.getClientMap().getControl();
	std::shared_ptr<MeshMakeData> data(new MeshMakeData(this, m_cache_enable_shaders,
		m_cache_use_tangent_vertices,
		m_env.getMap(), draw_control));

	{
		//TimeTaker timer("data fill");
		// Release: ~0ms
		// Debug: 1-6ms, avg=2ms
		data->fill(b);

#if ! ENABLE_THREADS
		if (!data->fill_data())
			return;
#endif

		data->setCrack(m_crack_level, m_crack_pos);
		data->setSmoothLighting(m_cache_smooth_lighting);
		data->step = step ? step : getFarmeshStep(data->draw_control, getNodeBlockPos(floatToInt(m_env.getLocalPlayer()->getPosition(), BS)), p);
		data->range = getNodeBlockPos(floatToInt(m_env.getLocalPlayer()->getPosition(), BS)).getDistanceFrom(p);
		if (step)
			data->no_draw = true;
	}

	// Add task to queue
	//unsigned int qsize = 
	//m_mesh_update_thread.m_queue_in.addBlock(p, data, urgent);
	m_mesh_update_thread.enqueueUpdate(p, data, urgent);
	//draw_control.block_overflow = qsize > 1000; // todo: depend on mesh make speed
}

void Client::addUpdateMeshTaskWithEdge(v3POS blockpos, bool urgent)
{
	for (int i=0;i<7;i++)
	{
		try{
			v3s16 p = blockpos + g_6dirs[i];
			addUpdateMeshTask(p, urgent);
		}
		catch(InvalidPositionException &e){}
	}
}

void Client::addUpdateMeshTaskForNode(v3s16 nodepos, bool urgent)
{
/*
	{
		v3s16 p = nodepos;
		infostream<<"Client::addUpdateMeshTaskForNode(): "
				<<"("<<p.X<<","<<p.Y<<","<<p.Z<<")"
				<<std::endl;
	}
*/

	v3s16 blockpos = getNodeBlockPos(nodepos);
	v3s16 blockpos_relative = blockpos * MAP_BLOCKSIZE;

	try{
		addUpdateMeshTask(blockpos, urgent);
	}
	catch(InvalidPositionException &e){}

	// Leading edge
	if(nodepos.X == blockpos_relative.X){
		try{
			v3s16 p = blockpos + v3s16(-1,0,0);
			addUpdateMeshTask(p, urgent);
		}
		catch(InvalidPositionException &e){}
	}

	if(nodepos.Y == blockpos_relative.Y){
		try{
			v3s16 p = blockpos + v3s16(0,-1,0);
			addUpdateMeshTask(p, urgent);
		}
		catch(InvalidPositionException &e){}
	}

	if(nodepos.Z == blockpos_relative.Z){
		try{
			v3s16 p = blockpos + v3s16(0,0,-1);
			addUpdateMeshTask(p, urgent);
		}
		catch(InvalidPositionException &e){}
	}
}

void Client::updateMeshTimestampWithEdge(v3s16 blockpos) {
	for (int i = 0; i < 7; ++i) {
		auto *block = m_env.getMap().getBlockNoCreateNoEx(blockpos + g_6dirs[i]);
		if(!block)
			continue;
		block->setTimestampNoChangedFlag(m_uptime);
	}
}

ClientEvent Client::getClientEvent()
{
	if(m_client_event_queue.size() == 0)
	{
		ClientEvent event;
		event.type = CE_NONE;
		return event;
	}
	return m_client_event_queue.pop_front();
}

float Client::mediaReceiveProgress()
{
	if (m_media_downloader)
		return m_media_downloader->getProgress();
	else
		return 1.0; // downloader only exists when not yet done
}

typedef struct TextureUpdateArgs {
	IrrlichtDevice *device;
	gui::IGUIEnvironment *guienv;
	u32 last_time_ms;
	u16 last_percent;
	const wchar_t* text_base;
} TextureUpdateArgs;

void texture_update_progress(void *args, u32 progress, u32 max_progress)
{
		TextureUpdateArgs* targs = (TextureUpdateArgs*) args;
		u16 cur_percent = ceil(progress / (double) max_progress * 100.);

		// update the loading menu -- if neccessary
		bool do_draw = false;
		u32 time_ms = targs->last_time_ms;
		if (cur_percent != targs->last_percent) {
			targs->last_percent = cur_percent;
			time_ms = getTimeMs();
			// only draw when the user will notice something:
			do_draw = (time_ms - targs->last_time_ms > 100);
		}

		if (do_draw) {
			targs->last_time_ms = time_ms;
			std::basic_stringstream<wchar_t> strm;
			strm << targs->text_base << " " << targs->last_percent << "%...";
			draw_load_screen(strm.str(), targs->device, targs->guienv, 0,
				72 + (u16) ((18. / 100.) * (double) targs->last_percent));
		}
}

void Client::afterContentReceived(IrrlichtDevice *device)
{
	//infostream<<"Client::afterContentReceived() started"<<std::endl;

	bool headless_optimize = g_settings->getBool("headless_optimize"); //device->getVideoDriver()->getDriverType() == video::EDT_NULL;
	//bool no_output = device->getVideoDriver()->getDriverType() == video::EDT_NULL;

	const wchar_t* text = wgettext("Loading textures...");

	// Clear cached pre-scaled 2D GUI images, as this cache
	// might have images with the same name but different
	// content from previous sessions.
	guiScalingCacheClear(device->getVideoDriver());

	// Rebuild inherited images and recreate textures
	infostream<<"- Rebuilding images and textures"<<std::endl;
	draw_load_screen(text,device, guienv, 0, 70);
	if (!headless_optimize)
	m_tsrc->rebuildImagesAndTextures();
	delete[] text;

	// Rebuild shaders
	infostream<<"- Rebuilding shaders"<<std::endl;
	text = wgettext("Rebuilding shaders...");
	draw_load_screen(text, device, guienv, 0, 71);
	if (!headless_optimize)
	m_shsrc->rebuildShaders();
	delete[] text;

	// Update node aliases
	infostream<<"- Updating node aliases"<<std::endl;
	text = wgettext("Initializing nodes...");
	draw_load_screen(text, device, guienv, 0, 72);
	m_nodedef->updateAliases(m_itemdef);
	std::string texture_path = g_settings->get("texture_path");
	if (texture_path != "" && fs::IsDir(texture_path))
		m_nodedef->applyTextureOverrides(texture_path + DIR_DELIM + "override.txt");
	m_nodedef->setNodeRegistrationStatus(true);
	m_nodedef->runNodeResolveCallbacks();
	delete[] text;

	if (!headless_optimize) {
	// Update node textures and assign shaders to each tile
	infostream<<"- Updating node textures"<<std::endl;
	TextureUpdateArgs tu_args;
	tu_args.device = device;
	tu_args.guienv = guienv;
	tu_args.last_time_ms = getTimeMs();
	tu_args.last_percent = 0;
	tu_args.text_base =  wgettext("Initializing nodes");
	m_nodedef->updateTextures(this, texture_update_progress, &tu_args);
	delete[] tu_args.text_base;
	}

	if (!headless_optimize) {
	// Start mesh update thread after setting up content definitions
		int threads = !g_settings->getBool("more_threads") ? 1 : (Thread::getNumberOfProcessors() - (m_simple_singleplayer_mode ? 3 : 1));
		infostream<<"- Starting mesh update threads = "<<threads<<std::endl;
		m_mesh_update_thread.start(threads < 1 ? 1 : threads);
	}

	m_state = LC_Ready;
	sendReady();
	text = wgettext("Done!");
	draw_load_screen(text, device, guienv, 0, 100);
	//infostream<<"Client::afterContentReceived() done"<<std::endl;
	delete[] text;
}

float Client::getRTT(void)
{
#if MINETEST_PROTO
	return m_con.getPeerStat(PEER_ID_SERVER,con::AVG_RTT);
#else
	return 0;
#endif
}

float Client::getCurRate(void)
{
#if MINETEST_PROTO
	return ( m_con.getLocalStat(con::CUR_INC_RATE) +
			m_con.getLocalStat(con::CUR_DL_RATE));
#else
	return 0;
#endif
}

float Client::getAvgRate(void)
{
#if MINETEST_PROTO
	return ( m_con.getLocalStat(con::AVG_INC_RATE) +
			m_con.getLocalStat(con::AVG_DL_RATE));
#else
	return 0;
#endif
}

void Client::makeScreenshot(const std::string & name, IrrlichtDevice *device)
{
	if (!device)
		device = m_device;
	irr::video::IVideoDriver *driver = device->getVideoDriver();
	irr::video::IImage* const raw_image = driver->createScreenShot();

	if (!raw_image)
		return;

	time_t t = time(NULL);
	struct tm *tm = localtime(&t);

	char timetstamp_c[64];
	strftime(timetstamp_c, sizeof(timetstamp_c), "%Y%m%d_%H%M%S", tm);

	std::string screenshot_path = porting::path_user + DIR_DELIM + g_settings->get("screenshot_path");
	if (!fs::CreateDir(screenshot_path)) {
		errorstream << "Failed to save screenshot: can't create directory for screenshots (\"" << screenshot_path << "\")." << std::endl;
		return;
	}

	std::string screenshot_name = name + std::string(timetstamp_c);
	std::string filename_base = screenshot_path
			+ DIR_DELIM
			+ screenshot_name;
	std::string filename_ext = ".png";
	std::string filename;

	// Try to find a unique filename
	unsigned serial = 0;

	while (serial < SCREENSHOT_MAX_SERIAL_TRIES) {
		filename = filename_base + (serial > 0 ? ("_" + itos(serial)) : "") + filename_ext;
		std::ifstream tmp(filename.c_str());
		if (!tmp.good())
			break;	// File did not apparently exist, we'll go with it
		serial++;
	}

	if (serial == SCREENSHOT_MAX_SERIAL_TRIES) {
		errorstream << "Could not find suitable filename for screenshot" << std::endl;
	} else {
		irr::video::IImage* const image =
				driver->createImage(video::ECF_R8G8B8, raw_image->getDimension());

		if (image) {
			raw_image->copyTo(image);

			std::ostringstream sstr;
			if (driver->writeImageToFile(image, filename.c_str())) {
				if (name == "screenshot_")
					sstr << "Saved screenshot to '" << screenshot_name << filename_ext << "'";
			} else {
				sstr << "Failed to save screenshot '" << screenshot_name << filename_ext << "'";
			}
			m_chat_queue.push(sstr.str());
			infostream << sstr.str() << std::endl;
			image->drop();
		}
	}

	raw_image->drop();
}

// IGameDef interface
// Under envlock
IItemDefManager* Client::getItemDefManager()
{
	return m_itemdef;
}
INodeDefManager* Client::getNodeDefManager()
{
	return m_nodedef;
}
ICraftDefManager* Client::getCraftDefManager()
{
	return NULL;
	//return m_craftdef;
}
ITextureSource* Client::getTextureSource()
{
	return m_tsrc;
}
IShaderSource* Client::getShaderSource()
{
	return m_shsrc;
}
scene::ISceneManager* Client::getSceneManager()
{
	return m_device->getSceneManager();
}
u16 Client::allocateUnknownNodeId(const std::string &name)
{
	errorstream << "Client::allocateUnknownNodeId(): "
			<< "Client cannot allocate node IDs" << std::endl;
	FATAL_ERROR("Client allocated unknown node");
	return CONTENT_IGNORE;
}
ISoundManager* Client::getSoundManager()
{
	return m_sound;
}
MtEventManager* Client::getEventManager()
{
	return m_event;
}

ParticleManager* Client::getParticleManager()
{
	return &m_particle_manager;
}

scene::IAnimatedMesh* Client::getMesh(const std::string &filename)
{
	StringMap::const_iterator it = m_mesh_data.find(filename);
	if (it == m_mesh_data.end()) {
		errorstream << "Client::getMesh(): Mesh not found: \"" << filename
			<< "\"" << std::endl;
		return NULL;
	}
	const std::string &data    = it->second;
	scene::ISceneManager *smgr = m_device->getSceneManager();

	// Create the mesh, remove it from cache and return it
	// This allows unique vertex colors and other properties for each instance
	Buffer<char> data_rw(data.c_str(), data.size()); // Const-incorrect Irrlicht
	io::IFileSystem *irrfs = m_device->getFileSystem();
	io::IReadFile *rfile = irrfs->createMemoryReadFile(
			*data_rw, data_rw.getSize(), filename.c_str());
	FATAL_ERROR_IF(!rfile, "Could not create/open RAM file");

	scene::IAnimatedMesh *mesh = smgr->getMesh(rfile);
	rfile->drop();
	// NOTE: By playing with Irrlicht refcounts, maybe we could cache a bunch
	// of uniquely named instances and re-use them
	mesh->grab();
	smgr->getMeshCache()->removeMesh(mesh);
	return mesh;
}

