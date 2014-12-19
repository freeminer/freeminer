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
#include "jthread/jmutexautolock.h"
#include "util/directiontables.h"
#include "util/pointedthing.h"
#include "util/serialize.h"
#include "util/string.h"
#include "strfnd.h"
#include "client.h"
#include "clientserver.h"
#include "main.h"
#include "filesys.h"
#include "porting.h"
#include "mapblock_mesh.h"
#include "mapblock.h"
#include "settings.h"
#include "profiler.h"
#include "gettext.h"
#include "log_types.h"
#include "nodemetadata.h"
#include "nodedef.h"
#include "itemdef.h"
#include "shader.h"
#include "base64.h"
#include "clientmap.h"
#include "clientmedia.h"
#include "sound.h"
#include "IMeshCache.h"
#include "serialization.h"
#include "config.h"
#include "version.h"
#include "drawscene.h"
#include "subgame.h"
#include "server.h"
#include "database.h" //remove with g sunsed shit localdb

#include "emerge.h"


extern gui::IGUIEnvironment* guienv;

#include "msgpack.h"

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
	DSTACK(__FUNCTION_NAME);

	auto lock = m_queue.lock_unique_rec();
	unsigned int range = urgent ? 0 : 1 + data->range + data->step * 10;
	if (m_process.count(p))
		range += 3;
	else if (m_ranges.count(p)) {
		auto range_old = m_ranges[p];
		if (range_old > 0 && range != range_old)  {
			auto & rmap = m_queue.get(range_old);
			m_ranges.erase(p);
			rmap.erase(p);
			if (rmap.empty())
				m_queue.erase(range_old);
		} else {
			return m_ranges.size(); //already queued
		}
	}
	auto & rmap = m_queue.get(range);
	if (rmap.count(p))
		return m_ranges.size();
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

void * MeshUpdateThread::Thread()
{
	ThreadStarted();

	log_register_thread("MeshUpdateThread" + itos(id));

	DSTACK(__FUNCTION_NAME);
	
	BEGIN_DEBUG_EXCEPTION_HANDLER

	porting::setThreadName(("MeshUpdateThread" + itos(id)).c_str());
	porting::setThreadPriority(30);

	while(!StopRequested())
	{

		try {
		auto q = m_queue_in.pop();
		if(!q)
		{
			sleep_ms(3);
			continue;
		}
		m_queue_in.m_process.set(q->m_blockpos, 1);

		ScopeProfiler sp(g_profiler, "Client: Mesh making " + itos(q->step));

		m_queue_out.push_back(MeshUpdateResult(q->m_blockpos, MapBlock::mesh_type(new MapBlockMesh(q.get(), m_camera_offset))));

		m_queue_in.m_process.erase(q->m_blockpos);

#ifdef NDEBUG
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

	END_DEBUG_EXCEPTION_HANDLER(errorstream)

	return NULL;
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
	m_mesh_update_thread(this),
	m_env(
		new ClientMap(this, this, control,
			device->getSceneManager()->getRootSceneNode(),
			device->getSceneManager(), 666),
		device->getSceneManager(),
		tsrc, this, device
	),
	m_con(PROTOCOL_ID, is_simple_singleplayer_game ? MAX_PACKET_SIZE_SINGLEPLAYER : MAX_PACKET_SIZE, CONNECTION_TIMEOUT, ipv6, this),
	m_device(device),
	m_server_ser_ver(SER_FMT_VER_INVALID),
	m_playeritem(0),
	m_inventory_updated(false),
	m_inventory_from_server(NULL),
	m_inventory_from_server_age(0.0),
	m_show_highlighted(false),
	m_animation_time(0),
	m_crack_level(-1),
	m_crack_pos(0,0,0),
	m_highlighted_pos(0,0,0),
	m_map_seed(0),
	m_password(password),
	m_access_denied(false),
	m_itemdef_received(false),
	m_nodedef_received(false),
	m_media_downloader(new ClientMediaDownloader()),
	m_time_of_day_set(false),
	m_last_time_of_day_f(-1),
	m_time_of_day_update_timer(0),
	m_recommended_send_interval(0.1),
	m_removed_sounds_check_timer(0),
	m_simple_singleplayer_mode(is_simple_singleplayer_game),
	m_state(LC_Created)
{
	/*
		Add local player
	*/
	{
		Player *player = new LocalPlayer(this, playername);

		m_env.addPlayer(player);
	}

	if (g_settings->getBool("enable_local_map_saving")
			&& !is_simple_singleplayer_game) {
		const std::string world_path = porting::path_user + DIR_DELIM + "worlds"
				+ DIR_DELIM + "server_" + g_settings->get("address")
				+ "_" + g_settings->get("remote_port");

		SubgameSpec gamespec;
		if (!getWorldExists(world_path)) {
			gamespec = findSubgame(g_settings->get("default_game"));
			if (!gamespec.isValid())
				gamespec = findSubgame("minimal");
		} else {
			std::string world_gameid = getWorldGameId(world_path, false);
			gamespec = findWorldSubgame(world_path);
		}
		if (!gamespec.isValid()) {
			errorstream << "Couldn't find subgame for local map saving." << std::endl;
			return;
		}

		localserver = new Server(world_path, gamespec, false, false);
		localdb = nullptr;
		actionstream << "Local map saving started, map will be saved at '" << world_path << "'" << std::endl;
	} else {
		localdb = NULL;
		localserver = nullptr;
	}

	m_cache_smooth_lighting = g_settings->getBool("smooth_lighting");
}

void Client::Stop()
{
	//request all client managed threads to stop
	m_mesh_update_thread.Stop();
	m_mesh_update_thread.Wait();
	if (localdb != NULL) {
		actionstream << "Local map saving ended" << std::endl;
		localdb->endSave();
	}
}

Client::~Client()
{
	m_con.Disconnect();

	m_mesh_update_thread.Stop();
	m_mesh_update_thread.Wait();
/*
	while(!m_mesh_update_thread.m_queue_out.empty()) {
		MeshUpdateResult r = m_mesh_update_thread.m_queue_out.pop_frontNoEx();
		delete r.mesh;
	}
*/

	delete m_inventory_from_server;

	// Delete detached inventories
	for(std::map<std::string, Inventory*>::iterator
			i = m_detached_inventories.begin();
			i != m_detached_inventories.end(); i++){
		delete i->second;
	}

	// cleanup 3d model meshes on client shutdown
	while (m_device->getSceneManager()->getMeshCache()->getMeshCount() != 0) {
		scene::IAnimatedMesh * mesh =
			m_device->getSceneManager()->getMeshCache()->getMeshByIndex(0);

		if (mesh != NULL)
			m_device->getSceneManager()->getMeshCache()->removeMesh(mesh);
	}

	if (localserver)
		delete localserver;
	if (localdb)
		delete localdb;
}

void Client::connect(Address address)
{
	DSTACK(__FUNCTION_NAME);
	m_con.Connect(address);
}

void Client::step(float dtime)
{
	DSTACK(__FUNCTION_NAME);

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
			
			infostream << "Client packetcounter (" << m_packetcounter_timer
					<< "):"<<std::endl;
			m_packetcounter.print(infostream);
			m_packetcounter.clear();
		}
	}

	// UGLY hack to fix 2 second startup delay caused by non existent
	// server client startup synchronization in local server or singleplayer mode
	static bool initial_step = true;
	if (initial_step) {
		initial_step = false;
	}
	else if(m_state == LC_Created)
	{
		float &counter = m_connection_reinit_timer;
		counter -= dtime;
		if(counter <= 0.0)
		{
			counter = 2.0;

			//JMutexAutoLock envlock(m_env_mutex); //bulk comment-out
			
			Player *myplayer = m_env.getLocalPlayer();
			assert(myplayer != NULL);
			// Send TOSERVER_INIT
			// [0] u16 TOSERVER_INIT
			// [2] u8 SER_FMT_VER_HIGHEST_READ
			// [3] u8[20] player_name
			// [23] u8[28] password (new in some version)
			// [51] u16 minimum supported network protocol version (added sometime)
			// [53] u16 maximum supported network protocol version (added later than the previous one)
			MSGPACK_PACKET_INIT(TOSERVER_INIT, 5);
			PACK(TOSERVER_INIT_FMT, SER_FMT_VER_HIGHEST_READ);
			PACK(TOSERVER_INIT_NAME, myplayer->getName());
			PACK(TOSERVER_INIT_PASSWORD, m_password);
			PACK(TOSERVER_INIT_PROTOCOL_VERSION_MIN, CLIENT_PROTOCOL_VERSION_MIN);
			PACK(TOSERVER_INIT_PROTOCOL_VERSION_MAX, CLIENT_PROTOCOL_VERSION_MAX);

			// Send as unreliable
			Send(1, buffer, false);
		}

		// Not connected, return
		return;
	}

	/*
		Do stuff if connected
	*/
	
	int max_cycle_ms = 500/g_settings->getFloat("wanted_fps");
	/*
		Run Map's timers and unload unused data
	*/
	const float map_timer_and_unload_dtime = 10.25;
	if(m_map_timer_and_unload_interval.step(dtime, map_timer_and_unload_dtime))
	{
		ScopeProfiler sp(g_profiler, "Client: map timer and unload");
		std::list<v3s16> deleted_blocks;
		
		if(m_env.getMap().timerUpdate(m_uptime,
				g_settings->getFloat("client_unload_unused_data_timeout"),
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

		std::list<v3s16>::iterator i = deleted_blocks.begin();
		std::list<v3s16> sendlist;
		for(;;)
		{
			if(sendlist.size() == 255 || i == deleted_blocks.end())
			{
				if(sendlist.empty())
					break;

				MSGPACK_PACKET_INIT(TOSERVER_DELETEDBLOCKS, 1);
				PACK(TOSERVER_DELETEDBLOCKS_DATA, sendlist);

				m_con.Send(PEER_ID_SERVER, 2, buffer, true);

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
	{
		// Control local player (0ms)
		LocalPlayer *player = m_env.getLocalPlayer();
		assert(player != NULL);
		player->applyControl(dtime, &m_env);

		// Step environment
		m_env.step(dtime, m_uptime, max_cycle_ms);
		
		/*
			Get events
		*/
		for(;;)
		{
			ClientEnvEvent event = m_env.getClientEvent();
			if(event.type == CEE_NONE)
			{
				break;
			}
			else if(event.type == CEE_PLAYER_DAMAGE)
			{
				if(m_ignore_damage_timer <= 0)
				{
					u8 damage = event.player_damage.amount;
					
					if(event.player_damage.send_to_server)
						sendDamage(damage);

					// Add to ClientEvent queue
					ClientEvent event;
					event.type = CE_PLAYER_DAMAGE;
					event.player_damage.amount = damage;
					m_client_event_queue.push_back(event);
				}
			}
			else if(event.type == CEE_PLAYER_BREATH)
			{
					u16 breath = event.player_breath.amount;
					sendBreath(breath);
			}
		}
	}

	/*
		Print some info
	*/
	{
		float &counter = m_avg_rtt_timer;
		counter += dtime;
		if(counter >= 10)
		{
			counter = 0.0;
			// connectedAndInitialized() is true, peer exists.
			float avg_rtt = getRTT();
			infostream<<"Client: avg_rtt="<<avg_rtt<<std::endl;

			sendDrawControl(); //not very good place. maybe 5s timer better
		}
	}

	/*
		Send player position to server
	*/
	{
		float &counter = m_playerpos_send_timer;
		counter += dtime;
		if((m_state == LC_Ready) && (counter >= m_recommended_send_interval))
		{
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

		while(!m_mesh_update_thread.m_queue_out.empty_try())
		{
			num_processed_meshes++;
			MeshUpdateResult r = m_mesh_update_thread.m_queue_out.pop_frontNoEx();
			if (!r.mesh)
				continue;
			auto block = m_env.getMap().getBlock(r.p);
			if(block)
			{
				block->setMesh(r.mesh);
			}
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
	if(m_inventory_from_server)
	{
		float interval = 10.0;
		float count_before = floor(m_inventory_from_server_age / interval);

		m_inventory_from_server_age += dtime;

		float count_after = floor(m_inventory_from_server_age / interval);

		if(count_after != count_before)
		{
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
				i != m_sounds_to_objects.end(); i++)
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
		std::set<s32> removed_server_ids;
		for(std::map<s32, int>::iterator
				i = m_sounds_server_to_client.begin();
				i != m_sounds_server_to_client.end();)
		{
			s32 server_id = i->first;
			int client_id = i->second;
			i++;
			if(!m_sound->soundExists(client_id)){
				m_sounds_server_to_client.erase(server_id);
				m_sounds_client_to_server.erase(client_id);
				m_sounds_to_objects.erase(client_id);
				removed_server_ids.insert(server_id);
			}
		}
		// Sync to server
		if(!removed_server_ids.empty())
		{
			MSGPACK_PACKET_INIT(TOSERVER_REMOVED_SOUNDS, 1);
			PACK(TOSERVER_REMOVED_SOUNDS_IDS, removed_server_ids);
			// Send as reliable
			Send(1, buffer, true);
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
		assert(rfile);
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
	infostream<<"Client::peerAdded(): peer->id="
			<<peer_id<<std::endl;
}
void Client::deletingPeer(u16 peer_id, bool timeout)
{
	infostream<<"Client::deletingPeer(): "
			"Server Peer is getting deleted "
			<<"(timeout="<<timeout<<")"<<std::endl;
}

/*
	u16 command
	u16 number of files requested
	for each file {
		u16 length of name
		string name
	}
*/
void Client::request_media(const std::list<std::string> &file_requests)
{
	MSGPACK_PACKET_INIT(TOSERVER_REQUEST_MEDIA, 1);
	PACK(TOSERVER_REQUEST_MEDIA_FILES, file_requests);

	// Send as reliable
	Send(1, buffer, true);
	infostream<<"Client: Sending media request list to server ("
			<<file_requests.size()<<" files)"<<std::endl;
}

void Client::received_media()
{
	// notify server we received everything
	MSGPACK_PACKET_INIT(TOSERVER_RECEIVED_MEDIA, 0);
	// Send as reliable
	Send(1, buffer, true);
	infostream<<"Client: Notifying server that we received all media"
			<<std::endl;
}

void Client::ReceiveAll()
{
	DSTACK(__FUNCTION_NAME);
	auto end_ms = porting::getTimeMs() + 10;
	for(;;)
	{
		try{
			Receive();
			g_profiler->graphAdd("client_received_packets", 1);
		}
		catch(con::NoIncomingDataException &e)
		{
			break;
		}
		catch(con::InvalidIncomingDataException &e)
		{
			infostream<<"Client::ReceiveAll(): "
					"InvalidIncomingDataException: what()="
					<<e.what()<<std::endl;
		}
		// Limit time even if there would be huge amounts of data to
		// process
		if(porting::getTimeMs() > end_ms)
			break;
	}
}

void Client::Receive()
{
	DSTACK(__FUNCTION_NAME);
	SharedBuffer<u8> data;
	u16 sender_peer_id;
	u32 datasize = m_con.Receive(sender_peer_id, data);
	ProcessData(*data, datasize, sender_peer_id);
}

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

		Settings settings;
		packet[TOCLIENT_INIT_MAP_PARAMS].convert(&settings);
		if (localserver)
			localserver->getEmergeManager()->loadParamsFromSettings(&settings);

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
		packet[TOCLIENT_ACCESS_DENIED_REASON].convert(&m_access_denied_reason);
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
		removeNode(p);
	}
	else if(command == TOCLIENT_ADDNODE)
	{
		v3s16 p = packet[TOCLIENT_ADDNODE_POS].as<v3s16>();
		MapNode n = packet[TOCLIENT_ADDNODE_NODE].as<MapNode>();
		bool remove_metadata = packet[TOCLIENT_ADDNODE_REMOVE_METADATA].as<bool>();

		addNode(p, n, remove_metadata);
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

		if (new_block)
			m_env.getMap().insertBlock(block);

		if (localserver != NULL) {
			localserver->getMap().saveBlock(block);
		}

		/*
			//Add it to mesh update queue and set it to be acknowledged after update.
		*/
		//infostream<<"Adding mesh update task for received block "<<p<<std::endl;
		updateMeshTimestampWithEdge(p);

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
		m_chat_queue.push_back(message);
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
			m_client_event_queue.push_back(event);
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
		m_client_event_queue.push_back(event);

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
		m_client_event_queue.push_back(event);
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
		m_client_event_queue.push_back(event);
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

		m_client_event_queue.push_back(event);
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

		m_client_event_queue.push_back(event);
	}
	else if(command == TOCLIENT_DELETE_PARTICLESPAWNER)
	{
		u32 id = packet[TOCLIENT_DELETE_PARTICLESPAWNER_ID].as<u32>();

		ClientEvent event;
		event.type = CE_DELETE_PARTICLESPAWNER;
		event.delete_particlespawner.id = id;

		m_client_event_queue.push_back(event);
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
		m_client_event_queue.push_back(event);
	}
	else if(command == TOCLIENT_HUDRM)
	{
		u32 id = packet[TOCLIENT_HUDRM_ID].as<u32>();

		ClientEvent event;
		event.type = CE_HUDRM;
		event.hudrm.id = id;
		m_client_event_queue.push_back(event);
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
		m_client_event_queue.push_back(event);
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
		m_client_event_queue.push_back(event);
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
		m_client_event_queue.push_back(event);
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

void Client::Send(u16 channelnum, SharedBuffer<u8> data, bool reliable)
{
	//JMutexAutoLock lock(m_con_mutex); //bulk comment-out
	m_con.Send(PEER_ID_SERVER, channelnum, data, reliable);
}

void Client::Send(u16 channelnum, const msgpack::sbuffer &data, bool reliable) {
	m_con.Send(PEER_ID_SERVER, channelnum, data, reliable);
}

void Client::interact(u8 action, const PointedThing& pointed)
{
	if(m_state != LC_Ready){
		infostream<<"Client::interact() "
				"cancelled (not connected)"
				<<std::endl;
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
	*/
	MSGPACK_PACKET_INIT(TOSERVER_INTERACT, 3);
	PACK(TOSERVER_INTERACT_ACTION, action);
	PACK(TOSERVER_INTERACT_ITEM, getPlayerItem());
	PACK(TOSERVER_INTERACT_POINTED_THING, pointed);

	// Send as reliable
	Send(0, buffer, true);
}

void Client::sendNodemetaFields(v3s16 p, const std::string &formname,
		const std::map<std::string, std::string> &fields)
{
	MSGPACK_PACKET_INIT(TOSERVER_NODEMETA_FIELDS, 3);
	PACK(TOSERVER_NODEMETA_FIELDS_POS, p);
	PACK(TOSERVER_NODEMETA_FIELDS_FORMNAME, formname);
	PACK(TOSERVER_NODEMETA_FIELDS_DATA, fields);
	// Send as reliable
	Send(0, buffer, true);
}
	
void Client::sendInventoryFields(const std::string &formname,
		const std::map<std::string, std::string> &fields)
{
	MSGPACK_PACKET_INIT(TOSERVER_INVENTORY_FIELDS, 2);
	PACK(TOSERVER_INVENTORY_FIELDS_FORMNAME, formname);
	PACK(TOSERVER_INVENTORY_FIELDS_DATA, fields);
	Send(0, buffer, true);
}

void Client::sendInventoryAction(InventoryAction *a)
{
	MSGPACK_PACKET_INIT(TOSERVER_INVENTORY_ACTION, 1);

	std::ostringstream os(std::ios_base::binary);
	a->serialize(os);
	std::string s = os.str();

	PACK(TOSERVER_INVENTORY_ACTION_DATA, s);

	// Send as reliable
	Send(0, buffer, true);
}

void Client::sendChatMessage(const std::string &message)
{
	MSGPACK_PACKET_INIT(TOSERVER_CHAT_MESSAGE, 1);
	PACK(TOSERVER_CHAT_MESSAGE_DATA, message);
	// Send as reliable
	Send(0, buffer, true);
}

void Client::sendChangePassword(const std::string &oldpassword,
								const std::string &newpassword)
{
	Player *player = m_env.getLocalPlayer();
	if(player == NULL)
		return;

	std::string playername = player->getName();
	std::string oldpwd = translatePassword(playername, oldpassword);
	std::string newpwd = translatePassword(playername, newpassword);

	MSGPACK_PACKET_INIT(TOSERVER_CHANGE_PASSWORD, 2);
	PACK(TOSERVER_CHANGE_PASSWORD_OLD, oldpwd);
	PACK(TOSERVER_CHANGE_PASSWORD_NEW, newpwd);

	// Send as reliable
	Send(0, buffer, true);
}


void Client::sendDamage(u8 damage)
{
	DSTACK(__FUNCTION_NAME);
	MSGPACK_PACKET_INIT(TOSERVER_DAMAGE, 1);
	PACK(TOSERVER_DAMAGE_VALUE, damage);

	// Send as reliable
	Send(0, buffer, true);
}

void Client::sendBreath(u16 breath)
{
	DSTACK(__FUNCTION_NAME);

	MSGPACK_PACKET_INIT(TOSERVER_BREATH, 1);
	PACK(TOSERVER_BREATH_VALUE, breath);
	// Send as reliable
	Send(0, buffer, true);
}

void Client::sendRespawn()
{
	DSTACK(__FUNCTION_NAME);

	MSGPACK_PACKET_INIT(TOSERVER_RESPAWN, 0);
	// Send as reliable
	Send(0, buffer, true);
}

void Client::sendReady()
{
	DSTACK(__FUNCTION_NAME);

	MSGPACK_PACKET_INIT(TOSERVER_CLIENT_READY, 3);
	PACK(TOSERVER_CLIENT_READY_VERSION_MAJOR, VERSION_MAJOR);
	PACK(TOSERVER_CLIENT_READY_VERSION_MINOR, VERSION_MINOR);
	// PACK(TOSERVER_CLIENT_READY_VERSION_PATCH, VERSION_PATCH_ORIG); TODO
	PACK(TOSERVER_CLIENT_READY_VERSION_STRING, std::string(minetest_version_hash));

	// Send as reliable
	Send(0, buffer, true);
}

void Client::sendPlayerPos()
{
	LocalPlayer *myplayer = m_env.getLocalPlayer();
	if(myplayer == NULL)
		return;

	// Save bandwidth by only updating position when something changed
	if(myplayer->last_position == myplayer->getPosition() &&
			myplayer->last_speed == myplayer->getSpeed() &&
			myplayer->last_pitch == myplayer->getPitch() &&
			myplayer->last_yaw == myplayer->getYaw() &&
			myplayer->last_keyPressed == myplayer->keyPressed)
		return;

	myplayer->last_position = myplayer->getPosition();
	myplayer->last_speed = myplayer->getSpeed();
	myplayer->last_pitch = myplayer->getPitch();
	myplayer->last_yaw = myplayer->getYaw();
	myplayer->last_keyPressed = myplayer->keyPressed;

	u16 our_peer_id;
	{
		//JMutexAutoLock lock(m_con_mutex); //bulk comment-out
		our_peer_id = m_con.GetPeerID();
	}

	// Set peer id if not set already
	if(myplayer->peer_id == PEER_ID_INEXISTENT)
		myplayer->peer_id = our_peer_id;
	// Check that an existing peer_id is the same as the connection's
	assert(myplayer->peer_id == our_peer_id);

	MSGPACK_PACKET_INIT(TOSERVER_PLAYERPOS, 5);
	PACK(TOSERVER_PLAYERPOS_POSITION, myplayer->getPosition());
	PACK(TOSERVER_PLAYERPOS_SPEED, myplayer->getSpeed());
	PACK(TOSERVER_PLAYERPOS_PITCH, myplayer->getPitch());
	PACK(TOSERVER_PLAYERPOS_YAW, myplayer->getYaw());
	PACK(TOSERVER_PLAYERPOS_KEY_PRESSED, myplayer->keyPressed);
	// Send as unreliable
	Send(0, buffer, false);
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
	// Check that an existing peer_id is the same as the connection's
	assert(myplayer->peer_id == our_peer_id);

	MSGPACK_PACKET_INIT(TOSERVER_PLAYERITEM, 1);
	PACK(TOSERVER_PLAYERITEM_VALUE, item);

	// Send as reliable
	Send(0, buffer, true);
}

void Client::removeNode(v3s16 p)
{
	std::map<v3s16, MapBlock*> modified_blocks;

	try
	{
		m_env.getMap().removeNodeAndUpdate(p, modified_blocks);
	}
	catch(InvalidPositionException &e)
	{
	}
	
	for(std::map<v3s16, MapBlock * >::iterator
			i = modified_blocks.begin();
			i != modified_blocks.end(); ++i)
	{
		addUpdateMeshTaskWithEdge(i->first, true);
	}
}

void Client::addNode(v3s16 p, MapNode n, bool remove_metadata)
{
	//TimeTaker timer1("Client::addNode()");

	std::map<v3s16, MapBlock*> modified_blocks;

	try
	{
		//TimeTaker timer3("Client::addNode(): addNodeAndUpdate");
		m_env.getMap().addNodeAndUpdate(p, n, modified_blocks, remove_metadata);
	}
	catch(InvalidPositionException &e)
	{}
	
	addUpdateMeshTaskForNode(p, true);

	for(std::map<v3s16, MapBlock * >::iterator
			i = modified_blocks.begin();
			i != modified_blocks.end(); ++i)
	{
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
		assert(0);
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
		
		core::aabbox3d<f32> *selection_box = obj->getSelectionBox();
		if(selection_box == NULL)
			continue;

		v3f pos = obj->getPosition();

		core::aabbox3d<f32> offsetted_box(
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

void Client::setHighlighted(v3s16 pos, bool show_highlighted)
{
	m_show_highlighted = show_highlighted;
	v3s16 old_highlighted_pos = m_highlighted_pos;
	m_highlighted_pos = pos;
	addUpdateMeshTaskForNode(old_highlighted_pos, true);
	addUpdateMeshTaskForNode(m_highlighted_pos, true);
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

void Client::typeChatMessage(const std::wstring &message)
{
	// Discard empty line
	if(message.empty())
		return;

	// Send to others
	sendChatMessage(wide_to_utf8(message));

	// Show locally
	if (message[0] == '/')
	{
		m_chat_queue.push_back("issued command: " + wide_to_utf8(message));
	}
	else
	{
		LocalPlayer *player = m_env.getLocalPlayer();
		if(!player)
			return;
		std::string name = player->getName();
		m_chat_queue.push_back("<" + name + "> " + wide_to_utf8(message));
	}
}

void Client::addUpdateMeshTask(v3s16 p, bool urgent)
{
	//ScopeProfiler sp(g_profiler, "Client: Mesh prepare");
	MapBlock *b = m_env.getMap().getBlockNoCreateNoEx(p);
	if(b == NULL)
		return;

	/*
		Create a task to update the mesh of the block
	*/
	auto & draw_control = m_env.getClientMap().getControl();
	std::shared_ptr<MeshMakeData> data(new MeshMakeData(this, m_env.getMap(), draw_control));

	{
		//TimeTaker timer("data fill");
		// Release: ~0ms
		// Debug: 1-6ms, avg=2ms
		data->fill(b);

#if ! CMAKE_THREADS
		data->fill_data();
#endif

		data->setCrack(m_crack_level, m_crack_pos);
		data->setHighlighted(m_highlighted_pos, m_show_highlighted);
		data->setSmoothLighting(m_cache_smooth_lighting);
		data->step = getFarmeshStep(data->draw_control, getNodeBlockPos(floatToInt(m_env.getLocalPlayer()->getPosition(), BS)), p);
		data->range = getNodeBlockPos(floatToInt(m_env.getLocalPlayer()->getPosition(), BS)).getDistanceFrom(p);
	}

	// Add task to queue
	unsigned int qsize = m_mesh_update_thread.m_queue_in.addBlock(p, data, urgent);
	draw_control.block_overflow = qsize > 1000; // todo: depend on mesh make speed

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

void Client::afterContentReceived(IrrlichtDevice *device, gui::IGUIFont* font)
{
	infostream<<"Client::afterContentReceived() started"<<std::endl;
	//assert(m_itemdef_received);
	//assert(m_nodedef_received);
	//assert(mediaReceived());
	

	bool no_output = device->getVideoDriver()->getDriverType() == video::EDT_NULL;

	// Rebuild inherited images and recreate textures
	infostream<<"- Rebuilding images and textures"<<std::endl;
	if (!no_output)
		m_tsrc->rebuildImagesAndTextures();

	// Rebuild shaders
	infostream<<"- Rebuilding shaders"<<std::endl;
	if (!no_output)
		m_shsrc->rebuildShaders();

	// Update node aliases
	infostream<<"- Updating node aliases"<<std::endl;
	m_nodedef->updateAliases(m_itemdef);

	// Update node textures and assign shaders to each tile
	infostream<<"- Updating node textures"<<std::endl;
	if (!no_output)
	m_nodedef->updateTextures(this);

	// Preload item textures and meshes if configured to
	if(!no_output && g_settings->getBool("preload_item_visuals"))
	{
		verbosestream<<"Updating item textures and meshes"<<std::endl;
		wchar_t* text = wgettext("Item textures...");
		draw_load_screen(text, device, guienv, 0, 0);
		std::set<std::string> names = m_itemdef->getAll();
		size_t size = names.size();
		size_t count = 0;
		int percent = 0;
		for(std::set<std::string>::const_iterator
				i = names.begin(); i != names.end(); ++i){
			// Asking for these caches the result
			m_itemdef->getInventoryTexture(*i, this);
			m_itemdef->getWieldMesh(*i, this);
			count++;
			percent = count*100/size;
			if (count%50 == 0) // only update every 50 item
				draw_load_screen(text, device, guienv, 0, percent);
		}
		delete[] text;
	}

	// Start mesh update thread after setting up content definitions
	infostream<<"- Starting mesh update thread"<<std::endl;
	if (!no_output) {
		auto threads = !g_settings->getBool("more_threads") ? 1 : (porting::getNumberOfProcessors() - (m_simple_singleplayer_mode ? 3 : 1));
		m_mesh_update_thread.Start(threads < 1 ? 1 : threads);
	}

	m_state = LC_Ready;
	sendReady();
	infostream<<"Client::afterContentReceived() done"<<std::endl;
}

float Client::getRTT(void)
{
	return 0;
	//return m_con.getPeerStat(PEER_ID_SERVER,con::AVG_RTT);
}

float Client::getCurRate(void)
{
	return 0;
//	return ( m_con.getLocalStat(con::CUR_INC_RATE) +
//			m_con.getLocalStat(con::CUR_DL_RATE));
}

float Client::getAvgRate(void)
{
	return 0;
//	return ( m_con.getLocalStat(con::AVG_INC_RATE) +
//			m_con.getLocalStat(con::AVG_DL_RATE));
}

void Client::makeScreenshot(IrrlichtDevice *device)
{
	irr::video::IVideoDriver *driver = device->getVideoDriver();
	irr::video::IImage* const raw_image = driver->createScreenShot();
	if (raw_image) {
		irr::video::IImage* const image = driver->createImage(video::ECF_R8G8B8,
			raw_image->getDimension());

		if (image) {
			raw_image->copyTo(image);
			irr::c8 filename[256];
			snprintf(filename, sizeof(filename), "%s" DIR_DELIM "screenshot_%u.png",
				 g_settings->get("screenshot_path").c_str(),
				 device->getTimer()->getRealTime());
			std::ostringstream sstr;
			if (driver->writeImageToFile(image, filename)) {
				sstr << "Saved screenshot to '" << filename << "'";
			} else {
				sstr << "Failed to save screenshot '" << filename << "'";
			}
			m_chat_queue.push_back(sstr.str());
			infostream << sstr.str() << std::endl;
			image->drop();
		}
		raw_image->drop();
	}
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
	errorstream<<"Client::allocateUnknownNodeId(): "
			<<"Client cannot allocate node IDs"<<std::endl;
	assert(0);
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

scene::IAnimatedMesh* Client::getMesh(const std::string &filename)
{
	std::map<std::string, std::string>::const_iterator i =
			m_mesh_data.find(filename);
	if(i == m_mesh_data.end()){
		errorstream<<"Client::getMesh(): Mesh not found: \""<<filename<<"\""
				<<std::endl;
		return NULL;
	}
	const std::string &data = i->second;
	scene::ISceneManager *smgr = m_device->getSceneManager();

	// Create the mesh, remove it from cache and return it
	// This allows unique vertex colors and other properties for each instance
	Buffer<char> data_rw(data.c_str(), data.size()); // Const-incorrect Irrlicht
	io::IFileSystem *irrfs = m_device->getFileSystem();
	io::IReadFile *rfile = irrfs->createMemoryReadFile(
			*data_rw, data_rw.getSize(), filename.c_str());
	assert(rfile);

	scene::IAnimatedMesh *mesh = smgr->getMesh(rfile);
	rfile->drop();
	// NOTE: By playing with Irrlicht refcounts, maybe we could cache a bunch
	// of uniquely named instances and re-use them
	mesh->grab();
	smgr->getMeshCache()->removeMesh(mesh);
	return mesh;
}



//freeminer:
void Client::sendDrawControl() {
	MSGPACK_PACKET_INIT(TOSERVER_DRAWCONTROL, 5);
	const auto & draw_control = m_env.getClientMap().getControl();
	PACK(TOSERVER_DRAWCONTROL_WANTED_RANGE, (u32)draw_control.wanted_range);
	PACK(TOSERVER_DRAWCONTROL_RANGE_ALL, (u32)draw_control.range_all);
	PACK(TOSERVER_DRAWCONTROL_FARMESH, (u8)draw_control.farmesh);
	PACK(TOSERVER_DRAWCONTROL_FOV, draw_control.fov);
	PACK(TOSERVER_DRAWCONTROL_BLOCK_OVERFLOW, draw_control.block_overflow);

	Send(0, buffer, false);
}

