/*
server.cpp
Copyright (C) 2010-2013 celeron55, Perttu Ahola <celeron55@gmail.com>
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

#include "server.h"
#include <iostream>
#include <queue>
#include <algorithm>
#include "network/networkprotocol.h"
#include "network/serveropcodes.h"
#include "ban.h"
#include "environment.h"
#include "map.h"
#include "jthread/jmutexautolock.h"
#include "constants.h"
#include "voxel.h"
#include "config.h"
#include "version.h"
#include "filesys.h"
#include "mapblock.h"
#include "serverobject.h"
#include "genericobject.h"
#include "settings.h"
#include "profiler.h"
#include "log_types.h"
#include "scripting_game.h"
#include "nodedef.h"
#include "itemdef.h"
#include "craftdef.h"
#include "emerge.h"
#include "mapgen.h"
#include "mg_biome.h"
#include "content_mapnode.h"
#include "content_nodemeta.h"
#include "content_abm.h"
#include "content_sao.h"
#include "mods.h"
#include "util/sha1.h"
#include "util/base64.h"
#include "tool.h"
#include "sound.h" // dummySoundManager
#include "event_manager.h"
#include "util/hex.h"
#include "serverlist.h"
#include "util/string.h"
#include "util/pointedthing.h"
#include "util/mathconstants.h"
#include "rollback.h"
#include "util/serialize.h"
#include "util/thread.h"
#include "defaultsettings.h"
//#include "stat.h"

#include <iomanip>
#include "msgpack_fix.h"
#include <chrono>
#include "util/thread_pool.h"
#include "key_value_storage.h"
#include "database.h"


#include "fm_server.cpp"

#if !MINETEST_PROTO
#include "network/fm_serverpacketsender.cpp"
#endif


class ClientNotFoundException : public BaseException
{
public:
	ClientNotFoundException(const char *s):
		BaseException(s)
	{}
};

class ServerThread : public thread_pool
{
	Server *m_server;

public:

	ServerThread(Server *server):
		m_server(server)
	{
	}

	void * Thread();
};

void *ServerThread::Thread()
{
	log_register_thread("ServerThread");

	DSTACK(__FUNCTION_NAME);
	BEGIN_DEBUG_EXCEPTION_HANDLER

	f32 dedicated_server_step = g_settings->getFloat("dedicated_server_step");
	m_server->AsyncRunStep(0.1, true);

	ThreadStarted();

	porting::setThreadName("ServerThread");
	porting::setThreadPriority(40);

	auto time = porting::getTimeMs();
	while (!StopRequested()) {
		try{
			//TimeTaker timer("AsyncRunStep() + Receive()");
			u32 time_now = porting::getTimeMs();
			m_server->AsyncRunStep((time_now - time)/1000.0f);
			time = time_now;

			// Loop used only when 100% cpu load or on old slow hardware.
			// usually only one packet recieved here
			u32 end_ms = porting::getTimeMs() + u32(1000 * dedicated_server_step/2);
			for (u16 i = 0; i < 1000; ++i) {
				if (!m_server->Receive())
					break;
				if (porting::getTimeMs() > end_ms)
					break;
			}
		} catch (con::NoIncomingDataException &e) {
			//std::this_thread::sleep_for(std::chrono::milliseconds(10));
		} catch (con::PeerNotFoundException &e) {
			infostream<<"Server: PeerNotFoundException"<<std::endl;
		} catch (ClientNotFoundException &e) {
		} catch (con::ConnectionBindFailed &e) {
			m_server->setAsyncFatalError(e.what());
#ifdef NDEBUG
		} catch (LuaError &e) {
			m_server->setAsyncFatalError("Lua: " + std::string(e.what()));
		} catch (std::exception &e) {
			errorstream<<"ServerThread: exception: "<<e.what()<<std::endl;
		} catch (...) {
			errorstream<<"ServerThread: Ooops..."<<std::endl;
#endif
		}
	}

	END_DEBUG_EXCEPTION_HANDLER(errorstream)

	return NULL;
}

v3f ServerSoundParams::getPos(ServerEnvironment *env, bool *pos_exists) const
{
	if(pos_exists) *pos_exists = false;
	switch(type){
	case SSP_LOCAL:
		return v3f(0,0,0);
	case SSP_POSITIONAL:
		if(pos_exists) *pos_exists = true;
		return pos;
	case SSP_OBJECT: {
		if(object == 0)
			return v3f(0,0,0);
		ServerActiveObject *sao = env->getActiveObject(object);
		if(!sao)
			return v3f(0,0,0);
		if(pos_exists) *pos_exists = true;
		return sao->getBasePosition(); }
	}
	return v3f(0,0,0);
}

/*
	Server
*/

Server::Server(
		const std::string &path_world,
		const SubgameSpec &gamespec,
		bool simple_singleplayer_mode,
		bool ipv6
	):
	m_path_world(path_world),
	m_gamespec(gamespec),
	m_simple_singleplayer_mode(simple_singleplayer_mode),
	m_async_fatal_error(""),
	m_env(NULL),
	m_con(PROTOCOL_ID,
			simple_singleplayer_mode ? MAX_PACKET_SIZE_SINGLEPLAYER : MAX_PACKET_SIZE,
			CONNECTION_TIMEOUT,
			ipv6,
			this),
	m_banmanager(NULL),
	m_rollback(NULL),
	m_enable_rollback_recording(false),
	m_emerge(NULL),
	m_script(NULL),
	stat(path_world),
	m_itemdef(createItemDefManager()),
	m_nodedef(createNodeDefManager()),
	m_craftdef(createCraftDefManager()),
	m_event(new EventManager()),
	m_thread(NULL),
	m_map_thread(nullptr),
	m_sendblocks(nullptr),
	m_liquid(nullptr),
	m_envthread(nullptr),
	m_abmthread(nullptr),
	m_time_of_day_send_timer(0),
	m_uptime(0),
	m_clients(&m_con),
	m_shutdown_requested(false),
	m_shutdown_ask_reconnect(false),
	m_ignore_map_edit_events(false),
	m_ignore_map_edit_events_peer_id(0),
	m_next_sound_id(0)

{
	m_liquid_transform_timer = 0.0;
	m_liquid_transform_interval = 1.0;
	m_liquid_send_timer = 0.0;
	m_liquid_send_interval = 1.0;
	m_autoexit = 0;
	maintenance_status = 0;
	m_print_info_timer = 0.0;
	m_masterserver_timer = 0.0;
	m_objectdata_timer = 0.0;
	//m_emergethread_trigger_timer = 5.0; // to start emerge threads instantly
	m_savemap_timer = 0.0;

	m_step_dtime = 0.0;
	m_lag = g_settings->getFloat("dedicated_server_step");
#if ENABLE_THREADS
	m_more_threads = g_settings->getBool("more_threads");
#else
	m_more_threads = 0;
#endif

	if(path_world == "")
		throw ServerError("Supplied empty world path");

	if(!gamespec.isValid())
		throw ServerError("Supplied invalid gamespec");

	infostream<<"Server created for gameid \""<<m_gamespec.id<<"\"";
	if(m_simple_singleplayer_mode)
		infostream<<" in simple singleplayer mode"<<std::endl;
	else
		infostream<<std::endl;
	infostream<<"- world:  "<<m_path_world<<std::endl;
	infostream<<"- game:   "<<m_gamespec.path<<std::endl;

	// Initialize default settings and override defaults with those provided
	// by the game
	set_default_settings(g_settings);
	Settings gamedefaults;
	getGameMinetestConfig(gamespec.path, gamedefaults);
	override_default_settings(g_settings, &gamedefaults);

	// Create server thread
	m_thread = new ServerThread(this);

	// Create emerge manager
	m_emerge = new EmergeManager(this);

	if (m_more_threads) {
		m_map_thread = new MapThread(this);
		m_sendblocks = new SendBlocksThread(this);
		m_liquid = new LiquidThread(this);
		m_envthread = new EnvThread(this);
		m_abmthread = new AbmThread(this);
	}

	// Create world if it doesn't exist
	if(!loadGameConfAndInitWorld(m_path_world, m_gamespec))
		throw ServerError("Failed to initialize world");

	// Create ban manager
	std::string ban_path = m_path_world + DIR_DELIM "ipban.txt";
	m_banmanager = new BanManager(ban_path);

	ModConfiguration modconf(m_path_world);
	m_mods = modconf.getMods();
	std::vector<ModSpec> unsatisfied_mods = modconf.getUnsatisfiedMods();
	// complain about mods with unsatisfied dependencies
	if(!modconf.isConsistent()) {
		for(std::vector<ModSpec>::iterator it = unsatisfied_mods.begin();
			it != unsatisfied_mods.end(); ++it) {
			ModSpec mod = *it;
			errorstream << "mod \"" << mod.name << "\" has unsatisfied dependencies: ";
			for(std::set<std::string>::iterator dep_it = mod.unsatisfied_depends.begin();
				dep_it != mod.unsatisfied_depends.end(); ++dep_it)
				errorstream << " \"" << *dep_it << "\"";
			errorstream << std::endl;
		}
	}

	Settings worldmt_settings;
	std::string worldmt = m_path_world + DIR_DELIM + "world.mt";
	worldmt_settings.readConfigFile(worldmt.c_str());
	std::vector<std::string> names = worldmt_settings.getNames();
	std::set<std::string> load_mod_names;
	for(std::vector<std::string>::iterator it = names.begin();
		it != names.end(); ++it) {
		std::string name = *it;
		if(name.compare(0,9,"load_mod_")==0 && worldmt_settings.getBool(name))
			load_mod_names.insert(name.substr(9));
	}
	// complain about mods declared to be loaded, but not found
	for(std::vector<ModSpec>::iterator it = m_mods.begin();
			it != m_mods.end(); ++it)
		load_mod_names.erase((*it).name);
	for(std::vector<ModSpec>::iterator it = unsatisfied_mods.begin();
			it != unsatisfied_mods.end(); ++it)
		load_mod_names.erase((*it).name);
	if(!load_mod_names.empty()) {
		errorstream << "The following mods could not be found:";
		for(std::set<std::string>::iterator it = load_mod_names.begin();
			it != load_mod_names.end(); ++it)
			errorstream << " \"" << (*it) << "\"";
		errorstream << std::endl;
	}

	// Lock environment
	//JMutexAutoLock envlock(m_env_mutex);

	// Load mapgen params from Settings
	m_emerge->loadMapgenParams();

	// Create the Map (loads map_meta.txt, overriding configured mapgen params)
	ServerMap *servermap = new ServerMap(path_world, this, m_emerge);

	// Initialize scripting
	infostream<<"Server: Initializing Lua"<<std::endl;

	m_script = new GameScripting(this);

	std::string script_path = getBuiltinLuaPath() + DIR_DELIM "init.lua";
	std::string error_msg;

	if (!m_script->loadMod(script_path, BUILTIN_MOD_NAME, &error_msg))
		throw ModError("Failed to load and run " + script_path
				+ "\nError from Lua:\n" + error_msg);

	// Print mods
	infostream << "Server: Loading mods: ";
	for(std::vector<ModSpec>::iterator i = m_mods.begin();
			i != m_mods.end(); i++) {
		const ModSpec &mod = *i;
		infostream << mod.name << " ";
	}
	infostream << std::endl;
	// Load and run "mod" scripts
	for (std::vector<ModSpec>::iterator i = m_mods.begin();
			i != m_mods.end(); i++) {
		const ModSpec &mod = *i;
		if (!string_allowed(mod.name, MODNAME_ALLOWED_CHARS)) {
			std::ostringstream err;
			err << "Error loading mod \"" << mod.name
					<< "\": mod_name does not follow naming conventions: "
					<< "Only chararacters [a-z0-9_] are allowed." << std::endl;
			errorstream << err.str().c_str();
			throw ModError(err.str());
		}
		std::string script_path = mod.path + DIR_DELIM "init.lua";
		infostream << "  [" << padStringRight(mod.name, 12) << "] [\""
				<< script_path << "\"]" << std::endl;
		if (!m_script->loadMod(script_path, mod.name, &error_msg)) {
			errorstream << "Server: Failed to load and run "
					<< script_path << std::endl;
			throw ModError("Failed to load and run " + script_path
					+ "\nError from Lua:\n" + error_msg);
		}
	}

	// Read Textures and calculate sha1 sums
	fillMediaCache();

	// Apply item aliases in the node definition manager
	m_nodedef->updateAliases(m_itemdef);

	// Apply texture overrides from texturepack/override.txt
	std::string texture_path = g_settings->get("texture_path");
	if (texture_path != "" && fs::IsDir(texture_path))
		m_nodedef->applyTextureOverrides(texture_path + DIR_DELIM + "override.txt");

	m_nodedef->setNodeRegistrationStatus(true);

	// Perform pending node name resolutions
	m_nodedef->runNodeResolveCallbacks();

	// init the recipe hashes to speed up crafting
	m_craftdef->initHashes(this);

	// Initialize Environment
	m_env = new ServerEnvironment(servermap, m_script, this, m_path_world);
	m_env->m_more_threads = m_more_threads;
	m_emerge->env = m_env;

	m_clients.setEnv(m_env);

	// Initialize mapgens
	m_emerge->initMapgens();

#if USE_SQLITE
	m_enable_rollback_recording = g_settings->getBool("enable_rollback_recording");
	if (m_enable_rollback_recording) {
		// Create rollback manager
		m_rollback = new RollbackManager(m_path_world, this);
	}
#endif

	// Give environment reference to scripting api
	m_script->initializeEnvironment(m_env);

	// Register us to receive map edit events
	servermap->addEventReceiver(this);

	// If file exists, load environment metadata
	if(fs::PathExists(m_path_world + DIR_DELIM "env_meta.txt"))
	{
		infostream<<"Server: Loading environment metadata"<<std::endl;
		m_env->loadMeta();
	}

	// Add some test ActiveBlockModifiers to environment
	add_legacy_abms(m_env, m_nodedef);

	m_env->m_abmhandler.init(m_env->m_abms); // uses result of add_legacy_abms and m_script->initializeEnvironment

	m_liquid_transform_interval = g_settings->getFloat("liquid_update");
	m_liquid_send_interval = g_settings->getFloat("liquid_send");

	if (!simple_singleplayer_mode)
		m_nodedef->updateTextures(this);

	m_emerge->startThreads();
}

Server::~Server()
{
	infostream<<"Server destructing"<<std::endl;

	if (!m_simple_singleplayer_mode && g_settings->getBool("server_announce"))
		ServerList::sendAnnounce("delete", m_bind_addr.getPort());

	// Send shutdown message
	SendChatMessage(PEER_ID_INEXISTENT, "*** Server shutting down");

	{
		//JMutexAutoLock envlock(m_env_mutex);

		// Execute script shutdown hooks
		m_script->on_shutdown();

		infostream << "Server: Saving players" << std::endl;
		m_env->saveLoadedPlayers();

		infostream << "Server: Kicking players" << std::endl;
		std::string kick_msg;
		bool reconnect = false;
		if (getShutdownRequested()) {
			reconnect = m_shutdown_ask_reconnect;
			kick_msg = m_shutdown_msg;
		}
		if (kick_msg == "") {
			kick_msg = g_settings->get("kick_msg_shutdown");
		}
		m_env->kickAllPlayers(SERVER_ACCESSDENIED_SHUTDOWN,
			kick_msg, reconnect);

		infostream << "Server: Saving environment metadata" << std::endl;
		m_env->saveMeta();
	}

	// Stop threads
	stop();
	delete m_thread;

	delete m_liquid;
	delete m_sendblocks;
	delete m_map_thread;
	delete m_abmthread;
	delete m_envthread;

	// stop all emerge threads before deleting players that may have
	// requested blocks to be emerged
	m_emerge->stopThreads();

	// Delete things in the reverse order of creation
	delete m_env;

	// N.B. the EmergeManager should be deleted after the Environment since Map
	// depends on EmergeManager to write its current params to the map meta
	delete m_emerge;
	delete m_rollback;
	delete m_banmanager;
	delete m_event;
	delete m_itemdef;
	delete m_nodedef;
	delete m_craftdef;

	// Deinitialize scripting
	infostream<<"Server: Deinitializing scripting"<<std::endl;
	delete m_script;

	// Delete detached inventories
	for (std::map<std::string, Inventory*>::iterator
			i = m_detached_inventories.begin();
			i != m_detached_inventories.end(); i++) {
		delete i->second;
	}
	while (!m_unsent_map_edit_queue.empty())
		delete m_unsent_map_edit_queue.pop_front();
}

void Server::start(Address bind_addr)
{
	DSTACK(__FUNCTION_NAME);

	m_bind_addr = bind_addr;

	infostream<<"Starting server on "
			<< bind_addr.serializeString() <<"..."<<std::endl;

	// Initialize connection
	m_con.Serve(bind_addr);

	// Start thread
	m_thread->restart();
	if (m_map_thread)
		m_map_thread->restart();
	if (m_sendblocks)
		m_sendblocks->restart();
	if (m_liquid)
		m_liquid->restart();
	if(m_envthread)
		m_envthread->restart();
	if(m_abmthread)
		m_abmthread->restart();

	actionstream << "\033[1mfree\033[1;33mminer \033[1;36mv" << g_version_hash << "\033[0m \t"
#if ENABLE_THREADS
			<< " THREADS \t"
#endif
#ifndef NDEBUG
			<< " DEBUG \t"
#endif
#if MINETEST_PROTO
			<< " MINETEST_PROTO \t"
#endif
			<< " cpp="<<__cplusplus<<" \t"
			<< " cores="<< porting::getNumberOfProcessors()
#if __ANDROID__
			<< " android=" << porting::android_version_sdk_int
#endif
			<< std::endl;
	actionstream<<"World at ["<<m_path_world<<"]"<<std::endl;
	actionstream<<"Server for gameid=\""<<m_gamespec.id
			<<"\" mapgen=\""<<m_emerge->params.mg_name
			<<"\" listening on "<<bind_addr.serializeString()<<":"
			<<bind_addr.getPort() << "."<<std::endl;
}

void Server::stop()
{
	DSTACK(__FUNCTION_NAME);

	infostream<<"Server: Stopping and waiting threads"<<std::endl;

	// Stop threads (set run=false first so both start stopping)
	m_thread->stop();
	if (m_liquid)
		m_liquid->stop();
	if (m_sendblocks)
		m_sendblocks->stop();
	if (m_map_thread)
		m_map_thread->stop();
	if(m_abmthread)
		m_abmthread->stop();
	if(m_envthread)
		m_envthread->stop();

	//m_emergethread.setRun(false);
	m_thread->join();
	//m_emergethread.stop();
	if (m_liquid)
		m_liquid->join();
	if (m_sendblocks)
		m_sendblocks->join();
	if (m_map_thread)
		m_map_thread->join();
	if(m_abmthread)
		m_abmthread->join();
	if(m_envthread)
		m_envthread->join();

	infostream<<"Server: Threads stopped"<<std::endl;
}

void Server::step(float dtime)
{
	DSTACK(__FUNCTION_NAME);
	// Limit a bit
	if(dtime > 2.0)
		dtime = 2.0;
	{
		JMutexAutoLock lock(m_step_dtime_mutex);
		m_step_dtime += dtime;
	}
	// Assert if fatal error occurred in thread
	std::string async_err = m_async_fatal_error.get();
	if(async_err != "") {
		if (m_simple_singleplayer_mode) {
			//throw ServerError(async_err);
		}
		else {
/*
			m_env->kickAllPlayers(SERVER_ACCESSDENIED_CRASH,
				g_settings->get("kick_msg_crash"),
				g_settings->getBool("ask_reconnect_on_crash"));
*/
			errorstream << "UNRECOVERABLE error occurred. Stopping server. "
					<< "Please fix the following error:" << std::endl
					<< async_err << std::endl;
/*
			FATAL_ERROR(async_err.c_str());
*/
		}
	}
}

void Server::AsyncRunStep(float dtime, bool initial_step)
{
	DSTACK(__FUNCTION_NAME);

	TimeTaker timer_step("Server step");
	g_profiler->add("Server::AsyncRunStep (num)", 1);
/*
	float dtime;
	{
		JMutexAutoLock lock1(m_step_dtime_mutex);
		dtime = m_step_dtime;
	}
*/

	if (!m_more_threads)
	{
		TimeTaker timer_step("Server step: SendBlocks");
		// Send blocks to clients
		SendBlocks(dtime);
	}

	if((dtime < 0.001) && (initial_step == false))
		return;

/*
	g_profiler->add("Server::AsyncRunStep with dtime (num)", 1);
*/
	ScopeProfiler sp(g_profiler, "Server::AsyncRunStep, avg", SPT_AVG);
	//infostream<<"Server steps "<<dtime<<std::endl;
	//infostream<<"Server::AsyncRunStep(): dtime="<<dtime<<std::endl;

/*
	{
		TimeTaker timer_step("Server step: SendBlocks");
		JMutexAutoLock lock1(m_step_dtime_mutex);
		m_step_dtime -= dtime;
	}
*/

	/*
		Update uptime
	*/
	{
		m_uptime.set(m_uptime.get() + dtime);
	}

	f32 dedicated_server_step = g_settings->getFloat("dedicated_server_step");
	//u32 max_cycle_ms = 1000 * (m_lag > dedicated_server_step ? dedicated_server_step/(m_lag/dedicated_server_step) : dedicated_server_step);
	u32 max_cycle_ms = 1000 * (dedicated_server_step/(m_lag/dedicated_server_step));
	if (max_cycle_ms < 40)
		max_cycle_ms = 40;

	{
		TimeTaker timer_step("Server step: handlePeerChanges");
		// This has to be called so that the client list gets synced
		// with the peer list of the connection
		handlePeerChanges();
	}

	/*
		Update time of day and overall game time
	*/
	{
		TimeTaker timer_step("Server step: pdate time of day and overall game time");
		//JMutexAutoLock envlock(m_env_mutex);

		m_env->setTimeOfDaySpeed(g_settings->getFloat("time_speed"));

		/*
			Send to clients at constant intervals
		*/

		m_time_of_day_send_timer -= dtime;
		if(m_time_of_day_send_timer < 0.0)
		{
			m_time_of_day_send_timer = g_settings->getFloat("time_send_interval");
			u16 time = m_env->getTimeOfDay();
			float time_speed = g_settings->getFloat("time_speed");
			SendTimeOfDay(PEER_ID_INEXISTENT, time, time_speed);
		}
	}

	{
		//TimeTaker timer_step("Server step: m_env->step");
		//JMutexAutoLock lock(m_env_mutex);
		// Figure out and report maximum lag to environment
		float max_lag = m_env->getMaxLagEstimate();
		max_lag *= 0.9998; // Decrease slowly (about half per 5 minutes)
		if(dtime > max_lag){
			if(dtime > dedicated_server_step && dtime > max_lag * 2.0)
				infostream<<"Server: Maximum lag peaked to "<<dtime
						<<" s"<<std::endl;
			max_lag = dtime;
		}
		m_env->reportMaxLagEstimate(max_lag);
		// Step environment
		ScopeProfiler sp(g_profiler, "SEnv step");
		if (!m_more_threads)
		m_env->step(dtime, m_uptime.get(), max_cycle_ms);
	}

	/*
		Do background stuff
	*/

	if (!m_more_threads)
		AsyncRunMapStep(dtime, false);

	m_clients.step(dtime);

	m_lag += (m_lag > dtime ? -1 : 1) * dtime/100;
	// send masterserver announce
	{
		float &counter = m_masterserver_timer;
		if(!isSingleplayer() && (!counter || counter >= 300.0) &&
				g_settings->getBool("server_announce"))
		{
			ServerList::sendAnnounce(counter ? "update" : "start",
					m_bind_addr.getPort(),
					m_clients.getPlayerNames(),
					m_uptime.get(),
					m_env->getGameTime(),
					m_lag,
					m_gamespec.id,
					m_emerge->params.mg_name,
					m_mods);
			counter = 0.01;
		}
		counter += dtime;
	}

	/*
		Check added and deleted active objects
	*/
	{
		TimeTaker timer_step("Server step: Check added and deleted active objects");
		//infostream<<"Server: Checking added and deleted active objects"<<std::endl;
		//JMutexAutoLock envlock(m_env_mutex);

		auto clients = m_clients.getClientList();
		ScopeProfiler sp(g_profiler, "Server: checking added and deleted objs");

		// Radius inside which objects are active
		s16 radius = g_settings->getS16("active_object_send_range_blocks");
		s16 player_radius = g_settings->getS16("player_transfer_distance");

		if (player_radius == 0 && g_settings->exists("unlimited_player_transfer_distance") &&
				!g_settings->getBool("unlimited_player_transfer_distance"))
			player_radius = radius;

		radius *= MAP_BLOCKSIZE;
		s16 radius_deactivate = radius*3;
		player_radius *= MAP_BLOCKSIZE;

		for(auto & client : clients)
		{
			// If definitions and textures have not been sent, don't
			// send objects either
			if (client->getState() < CS_DefinitionsSent)
				continue;

			Player *player = m_env->getPlayer(client->peer_id);
			if(player==NULL)
			{
				// This can happen if the client timeouts somehow
				/*infostream<<"WARNING: "<<__FUNCTION_NAME<<": Client "
						<<client->peer_id
						<<" has no associated player"<<std::endl;*/
				continue;
			}
			v3s16 pos = floatToInt(player->getPosition(), BS);

			std::set<u16> removed_objects;
			std::set<u16> added_objects;
			m_env->getRemovedActiveObjects(pos, radius_deactivate, player_radius,
					client->m_known_objects, removed_objects);
			m_env->getAddedActiveObjects(pos, radius, player_radius,
					client->m_known_objects, added_objects);

			// Ignore if nothing happened
			if(removed_objects.empty() && added_objects.empty())
			{
				//infostream<<"active objects: none changed"<<std::endl;
				continue;
			}

#if MINETEST_PROTO

			std::string data_buffer;

			char buf[4];

			// Handle removed objects
			writeU16((u8*)buf, removed_objects.size());
			data_buffer.append(buf, 2);
			for(std::set<u16>::iterator
					i = removed_objects.begin();
					i != removed_objects.end(); ++i)
			{
				// Get object
				u16 id = *i;
				ServerActiveObject* obj = m_env->getActiveObject(id);

				// Add to data buffer for sending
				writeU16((u8*)buf, id);
				data_buffer.append(buf, 2);

				// Remove from known objects
				client->m_known_objects.erase(id);

				if(obj && obj->m_known_by_count > 0)
					obj->m_known_by_count--;
			}

			// Handle added objects
			writeU16((u8*)buf, added_objects.size());
			data_buffer.append(buf, 2);
			for(std::set<u16>::iterator
					i = added_objects.begin();
					i != added_objects.end(); ++i)
			{
				// Get object
				u16 id = *i;
				ServerActiveObject* obj = m_env->getActiveObject(id);

				// Get object type
				u8 type = ACTIVEOBJECT_TYPE_INVALID;
				if(obj == NULL)
					infostream<<"WARNING: "<<__FUNCTION_NAME
							<<": NULL object"<<std::endl;
				else
					type = obj->getSendType();

				// Add to data buffer for sending
				writeU16((u8*)buf, id);
				data_buffer.append(buf, 2);
				writeU8((u8*)buf, type);
				data_buffer.append(buf, 1);

				if(obj)
					data_buffer.append(serializeLongString(
							obj->getClientInitializationData(client->net_proto_version)));
				else
					data_buffer.append(serializeLongString(""));

				// Add to known objects
				client->m_known_objects.set(id, true);

				if(obj)
					obj->m_known_by_count++;
			}

			u32 pktSize = SendActiveObjectRemoveAdd(client->peer_id, data_buffer);
			verbosestream << "Server: Sent object remove/add: "
					<< removed_objects.size() << " removed, "
					<< added_objects.size() << " added, "
					<< "packet size is " << pktSize << std::endl;



#else



			// Handle removed objects
			for(std::set<u16>::iterator
					i = removed_objects.begin();
					i != removed_objects.end(); ++i)
			{
				// Get object
				u16 id = *i;
				ServerActiveObject* obj = m_env->getActiveObject(id);

				// Remove from known objects
				client->m_known_objects.erase(id);

				if(obj && obj->m_known_by_count > 0)
					obj->m_known_by_count--;
			}

			std::vector<ActiveObjectAddData> added_objects_data;

			// Handle added objects
			for(std::set<u16>::iterator
					i = added_objects.begin();
					i != added_objects.end(); ++i)
			{
				// Get object
				u16 id = *i;
				ServerActiveObject* obj = m_env->getActiveObject(id);

				// Get object type
				u8 type = ACTIVEOBJECT_TYPE_INVALID;
				if(obj == NULL)
					infostream<<"WARNING: "<<__FUNCTION_NAME
							<<": NULL object"<<std::endl;
				else
					type = obj->getSendType();

				std::string data = "";
				if(obj)
					data = obj->getClientInitializationData(client->net_proto_version);
				added_objects_data.push_back(ActiveObjectAddData(id, type, data));

				// Add to known objects
				client->m_known_objects.set(id, true);

				if(obj)
					obj->m_known_by_count++;
			}

			MSGPACK_PACKET_INIT(TOCLIENT_ACTIVE_OBJECT_REMOVE_ADD, 2);
			PACK(TOCLIENT_ACTIVE_OBJECT_REMOVE_ADD_REMOVE, removed_objects);
			PACK(TOCLIENT_ACTIVE_OBJECT_REMOVE_ADD_ADD, added_objects_data);

			// Send as reliable
			m_clients.send(client->peer_id, 0, buffer, true);
#endif

		}
	}

	/*
		Send object messages
	*/
	{
		TimeTaker timer_step("Server step: Send object messages");
		//JMutexAutoLock envlock(m_env_mutex);
		ScopeProfiler sp(g_profiler, "Server: sending object messages");

		// Key = object id
		// Value = data sent by object
		std::map<u16, std::vector<ActiveObjectMessage>* > buffered_messages;

		// Get active object messages from environment
		for(;;) {
			ActiveObjectMessage aom = m_env->getActiveObjectMessage();
			if (aom.id == 0)
				break;

			std::vector<ActiveObjectMessage>* message_list = NULL;
			std::map<u16, std::vector<ActiveObjectMessage>* >::iterator n;
			n = buffered_messages.find(aom.id);
			if (n == buffered_messages.end()) {
				message_list = new std::vector<ActiveObjectMessage>;
				buffered_messages[aom.id] = message_list;
			}
			else {
				message_list = n->second;
			}
			message_list->push_back(aom);
		}

		auto clients = m_clients.getClientList();
		// Route data to every client
		for (auto & client : clients) {

#if MINETEST_PROTO
			std::string reliable_data;
			std::string unreliable_data;
			// Go through all objects in message buffer
			for (std::map<u16, std::vector<ActiveObjectMessage>* >::iterator
					j = buffered_messages.begin();
					j != buffered_messages.end(); ++j) {
				// If object is not known by client, skip it
				u16 id = j->first;
				if (client->m_known_objects.find(id) == client->m_known_objects.end())
					continue;

				// Get message list of object
				std::vector<ActiveObjectMessage>* list = j->second;
				// Go through every message
				for (std::vector<ActiveObjectMessage>::iterator
						k = list->begin(); k != list->end(); ++k) {
					// Compose the full new data with header
					ActiveObjectMessage aom = *k;
					std::string new_data;
					// Add object id
					char buf[2];
					writeU16((u8*)&buf[0], aom.id);
					new_data.append(buf, 2);
					// Add data
					new_data += serializeString(aom.datastring);
					// Add data to buffer
					if(aom.reliable)
						reliable_data += new_data;
					else
						unreliable_data += new_data;
				}
			}
			/*
				reliable_data and unreliable_data are now ready.
				Send them.
			*/
			if(reliable_data.size() > 0) {
				SendActiveObjectMessages(client->peer_id, reliable_data);
			}

			if(unreliable_data.size() > 0) {
				SendActiveObjectMessages(client->peer_id, unreliable_data, false);
			}

#else
			ActiveObjectMessages reliable_data;
			ActiveObjectMessages unreliable_data;
			// Go through all objects in message buffer
			for(auto
					j = buffered_messages.begin();
					j != buffered_messages.end(); ++j) {
				// If object is not known by client, skip it
				u16 id = j->first;
				if (client->m_known_objects.find(id) == client->m_known_objects.end())
					continue;
				// Get message list of object
				std::vector<ActiveObjectMessage>* list = j->second;
				// Go through every message
				for(auto
						k = list->begin(); k != list->end(); ++k) {
					// Add data to buffer
					if(k->reliable)
						reliable_data.push_back(make_pair(k->id, k->datastring));
					else
						unreliable_data.push_back(make_pair(k->id, k->datastring));
				}
			}
			/*
				reliable_data and unreliable_data are now ready.
				Send them.
			*/
			if(reliable_data.size() > 0) {
				SendActiveObjectMessages(client->peer_id, reliable_data);
			}
			if(unreliable_data.size() > 0) {
				SendActiveObjectMessages(client->peer_id, unreliable_data, false);
			}
#endif
		}
		// Clear buffered_messages
		for (auto
				i = buffered_messages.begin();
				i != buffered_messages.end(); ++i) {
			delete i->second;
		}
	}

	/*
		Send queued-for-sending map edit events.
	*/
	{
		TimeTaker timer_step("Server step: Send queued-for-sending map edit events.");
		ScopeProfiler sp(g_profiler, "Server: Map events process");
		// We will be accessing the environment
		//JMutexAutoLock lock(m_env_mutex);

		// Don't send too many at a time
		u32 count = 0;

		// Single change sending is disabled if queue size is not small
		bool disable_single_change_sending = false;
		if(m_unsent_map_edit_queue.size() > 1)
			disable_single_change_sending = true;

		//int event_count = m_unsent_map_edit_queue.size();

		// We'll log the amount of each
		Profiler prof;

		u32 end_ms = porting::getTimeMs() + max_cycle_ms;
#if !ENABLE_THREADS
		auto lock = m_env->getMap().m_nothread_locker.lock_shared_rec();
		if (lock->owns_lock())
#endif
		while(m_unsent_map_edit_queue.size() != 0)
		{
			auto event = std::unique_ptr<MapEditEvent>(m_unsent_map_edit_queue.pop_front());

			// Players far away from the change are stored here.
			// Instead of sending the changes, MapBlocks are set not sent
			// for them.
			std::vector<u16> far_players;

			if(event->type == MEET_ADDNODE || event->type == MEET_SWAPNODE) {
				//infostream<<"Server: MEET_ADDNODE"<<std::endl;
				prof.add("MEET_ADDNODE", 1);
				if(disable_single_change_sending)
					sendAddNode(event->p, event->n, event->already_known_by_peer,
							&far_players, 5, event->type == MEET_ADDNODE);
				else
					sendAddNode(event->p, event->n, event->already_known_by_peer,
							&far_players, 30, event->type == MEET_ADDNODE);
			}
			else if(event->type == MEET_REMOVENODE) {
				//infostream<<"Server: MEET_REMOVENODE"<<std::endl;
				prof.add("MEET_REMOVENODE", 1);
				if(disable_single_change_sending)
					sendRemoveNode(event->p, event->already_known_by_peer,
							&far_players, 5);
				else
					sendRemoveNode(event->p, event->already_known_by_peer,
							&far_players, 30);
			}
			else if(event->type == MEET_BLOCK_NODE_METADATA_CHANGED) {
/*
				infostream<<"Server: MEET_BLOCK_NODE_METADATA_CHANGED"<<std::endl;
*/
				prof.add("MEET_BLOCK_NODE_METADATA_CHANGED", 1);
				setBlockNotSent(event->p);
			}
			else if(event->type == MEET_OTHER) {
/*
				infostream<<"Server: MEET_OTHER"<<std::endl;
*/
				prof.add("MEET_OTHER", 1);
				for(std::set<v3s16>::iterator
						i = event->modified_blocks.begin();
						i != event->modified_blocks.end(); ++i)
				{
					setBlockNotSent(*i);
				}
			}
			else {
				prof.add("unknown", 1);
				infostream<<"WARNING: Server: Unknown MapEditEvent "
						<<((u32)event->type)<<std::endl;
			}

			/*
				Set blocks not sent to far players
			*/
			if (!far_players.empty()) {
				// Convert list format to that wanted by SetBlocksNotSent
				std::map<v3s16, MapBlock*> modified_blocks2;
				for(std::set<v3s16>::iterator
						i = event->modified_blocks.begin();
						i != event->modified_blocks.end(); ++i) {
					modified_blocks2[*i] =
							m_env->getMap().getBlockNoCreateNoEx(*i);
				}
				// Set blocks not sent
				for (auto
						i = far_players.begin();
						i != far_players.end(); ++i) {
					u16 peer_id = *i;
					RemoteClient *client = getClient(peer_id);
					if(client==NULL)
						continue;
					client->SetBlocksNotSent(modified_blocks2);
				}
			}

			//delete event;

			++count;
			/*// Don't send too many at a time
			if(count >= 1 && m_unsent_map_edit_queue.size() < 100)
				break;*/
			if (porting::getTimeMs() > end_ms)
				break;
		}

/*
		if(event_count >= 10){
			infostream<<"Server: MapEditEvents count="<<count<<"/"<<event_count<<" :"<<std::endl;
			prof.print(infostream);
		} else if(event_count != 0){
			verbosestream<<"Server: MapEditEvents count="<<count<<"/"<<event_count<<" :"<<std::endl;
			prof.print(verbosestream);
		}
*/

	}

	/*
		Trigger emergethread (it somehow gets to a non-triggered but
		bysy state sometimes)
	*/
/*
	if (!maintenance_status)
	{
		TimeTaker timer_step("Server step: Trigger emergethread");
		float &counter = m_emergethread_trigger_timer;
		counter += dtime;
		if(counter >= 2.0)
		{
			counter = 0.0;

			m_emerge->startThreads();
		}
	}
*/

	{
		if (porting::g_sighup) {
			porting::g_sighup = false;
			if(!maintenance_status) {
				maintenance_status = 1;
				maintenance_start();
				maintenance_status = 2;
			} else if(maintenance_status == 2) {
				maintenance_status = 3;
				maintenance_end();
				maintenance_status = 0;
			}
		}
		if (porting::g_siginfo) {
			// todo: add here more info
			porting::g_siginfo = false;
			infostream<<"uptime="<< (int)m_uptime.get()<<std::endl;
			m_clients.UpdatePlayerList(); //print list
			g_profiler->print(infostream);
			g_profiler->clear();
		}
	}
}

int Server::save(float dtime, bool breakable) {
	// Save map, players and auth stuff
	int ret = 0;
		float &counter = m_savemap_timer;
		counter += dtime;
		if(counter >= g_settings->getFloat("server_map_save_interval"))
		{
			counter = 0.0;
			TimeTaker timer_step("Server step: Save map, players and auth stuff");
			//JMutexAutoLock lock(m_env_mutex);

			ScopeProfiler sp(g_profiler, "Server: saving stuff");

			// Save changed parts of map
			if(m_env->getMap().save(MOD_STATE_WRITE_NEEDED, breakable)) {
				// partial save, will continue on next step
				counter = g_settings->getFloat("server_map_save_interval");
				++ret;
				if (breakable)
					goto save_break;
			}

			// Save ban file
			if (m_banmanager->isModified()) {
				m_banmanager->save();
			}

			// Save players
			m_env->saveLoadedPlayers();

			// Save environment metadata
			m_env->saveMeta();

			stat.save();
		}
		save_break:;

	return ret;
}

u16 Server::Receive()
{
	DSTACK(__FUNCTION_NAME);
	SharedBuffer<u8> data;
	u16 peer_id;
	u16 received = 0;
	try {
		NetworkPacket pkt;
		auto size = m_con.Receive(&pkt, 10);
		peer_id = pkt.getPeerId();
		if (size) {
			ProcessData(&pkt);
			++received;
		}
	}
	catch(con::InvalidIncomingDataException &e) {
		infostream<<"Server::Receive(): "
				"InvalidIncomingDataException: what()="
				<<e.what()<<std::endl;
	}
	catch(SerializationError &e) {
		infostream<<"Server::Receive(): "
				"SerializationError: what()="
				<<e.what()<<std::endl;
	}
	catch(ClientStateError &e) {
		errorstream << "ProcessData: peer=" << peer_id  << e.what() << std::endl;
		DenyAccess_Legacy(peer_id, L"Your client sent something server didn't expect."
				L"Try reconnecting or updating your client");
	}
	catch(con::PeerNotFoundException &e) {
		// Do nothing
	}
	return received;
}

PlayerSAO* Server::StageTwoClientInit(u16 peer_id)
{
	std::string playername = "";
	PlayerSAO *playersao = NULL;
		RemoteClient* client = m_clients.lockedGetClientNoEx(peer_id, CS_InitDone);
		if (client != NULL) {
			playername = client->getName();
			playersao = emergePlayer(playername.c_str(), peer_id, client->net_proto_version);
		}

	RemotePlayer *player =
		static_cast<RemotePlayer*>(m_env->getPlayer(playername));

	// If failed, cancel
	if ((playersao == NULL) || (player == NULL)) {
		if (player && player->peer_id != 0) {
			actionstream << "Server: Failed to emerge player \"" << playername
					<< "\" (player allocated to an another client)" << std::endl;
			DenyAccess_Legacy(peer_id, L"Another client is connected with this "
					L"name. If your client closed unexpectedly, try again in "
					L"a minute.");
		} else {
			errorstream << "Server: " << playername << ": Failed to emerge player"
					<< std::endl;
			DenyAccess_Legacy(peer_id, L"Could not allocate player.");
		}
		return NULL;
	}

	/*
		Send complete position information
	*/
	SendMovePlayer(peer_id);

	// Send privileges
	SendPlayerPrivileges(peer_id);

	// Send inventory formspec
	SendPlayerInventoryFormspec(peer_id);

	// Send inventory
	SendInventory(playersao);

	// Send HP
	SendPlayerHPOrDie(playersao);

	// Send Breath
	SendPlayerBreath(peer_id);

	// Show death screen if necessary
	if(player->isDead())
		SendDeathscreen(peer_id, false, v3f(0,0,0));

	// Note things in chat if not in simple singleplayer mode
	if(!m_simple_singleplayer_mode) {
		// Send information about server to player in chat
		SendChatMessage(peer_id, getStatusString());
	}

/*
	Address addr = getPeerAddress(player->peer_id);
	std::string ip_str = addr.serializeString();
	actionstream<<player->getName() <<" [" << ip_str << "] joins game. " << std::endl;
*/
	/*
		Print out action
	*/
	{
		std::vector<std::string> names = m_clients.getPlayerNames();

		actionstream<<player->getName() << " ["<<getPeerAddress(peer_id).serializeString()<<"]"<<
		" joins game. List of players: ";

		for (std::vector<std::string>::iterator i = names.begin();
				i != names.end(); i++) {
			actionstream << *i << " ";
		}

		actionstream << player->getName() <<std::endl;
	}
	return playersao;
}

//FMTODO
#if MINETEST_PROTO

inline void Server::handleCommand(NetworkPacket* pkt)
{
	const ToServerCommandHandler& opHandle = toServerCommandTable[pkt->getCommand()];
	(this->*opHandle.handler)(pkt);
}

void Server::ProcessData(NetworkPacket *pkt)
{
	DSTACK(__FUNCTION_NAME);
	// Environment is locked first.
	//JMutexAutoLock envlock(m_env_mutex);

	ScopeProfiler sp(g_profiler, "Server::ProcessData");
	u32 peer_id = pkt->getPeerId();

	try {
		Address address = getPeerAddress(peer_id);
		std::string addr_s = address.serializeString();

		if(m_banmanager->isIpBanned(addr_s)) {
			std::string ban_name = m_banmanager->getBanName(addr_s);
			infostream << "Server: A banned client tried to connect from "
					<< addr_s << "; banned name was "
					<< ban_name << std::endl;
			// This actually doesn't seem to transfer to the client
			DenyAccess_Legacy(peer_id, L"Your ip is banned. Banned name was "
					+ utf8_to_wide(ban_name));
			return;
		}
	}
	catch(con::PeerNotFoundException &e) {
		/*
		 * no peer for this packet found
		 * most common reason is peer timeout, e.g. peer didn't
		 * respond for some time, your server was overloaded or
		 * things like that.
		 */
		infostream << "Server::ProcessData(): Canceling: peer "
				<< peer_id << " not found" << std::endl;
		return;
	}

	try {
		ToServerCommand command = (ToServerCommand) pkt->getCommand();

		// Command must be handled into ToServerCommandHandler
		if (command >= TOSERVER_NUM_MSG_TYPES) {
			infostream << "Server: Ignoring unknown command "
					 << command << std::endl;
			return;
		}

		if (toServerCommandTable[command].state == TOSERVER_STATE_NOT_CONNECTED) {
			handleCommand(pkt);
			return;
		}

		u8 peer_ser_ver = getClient(peer_id, CS_InitDone)->serialization_version;

		if(peer_ser_ver == SER_FMT_VER_INVALID) {
			errorstream << "Server::ProcessData(): Cancelling: Peer"
					" serialization format invalid or not initialized."
					" Skipping incoming command=" << command << std::endl;
			return;
		}

		/* Handle commands related to client startup */
		if (toServerCommandTable[command].state == TOSERVER_STATE_STARTUP) {
			handleCommand(pkt);
			return;
		}

		if (m_clients.getClientState(peer_id) < CS_Active) {
			if (command == TOSERVER_PLAYERPOS) return;

			errorstream << "Got packet command: " << command << " for peer id "
					<< peer_id << " but client isn't active yet. Dropping packet "
					<< std::endl;
			return;
		}

		handleCommand(pkt);
	} catch (SendFailedException &e) {
		errorstream << "Server::ProcessData(): SendFailedException: "
				<< "what=" << e.what()
				<< std::endl;
	} catch (PacketError &e) {
		actionstream << "Server::ProcessData(): PacketError: "
				<< "what=" << e.what()
				<< std::endl;
	}
}
#endif


void Server::setTimeOfDay(u32 time)
{
	m_env->setTimeOfDay(time);
	m_time_of_day_send_timer = 0;
}

void Server::onMapEditEvent(MapEditEvent *event)
{
	//infostream<<"Server::onMapEditEvent()"<<std::endl;
	if(m_ignore_map_edit_events)
		return;
/* thread unsafe
	if(m_ignore_map_edit_events_area.contains(event->getArea()))
		return;
*/
	MapEditEvent *e = event->clone();
	m_unsent_map_edit_queue.push(e);
}

Inventory* Server::getInventory(const InventoryLocation &loc)
{
	switch (loc.type) {
	case InventoryLocation::UNDEFINED:
	case InventoryLocation::CURRENT_PLAYER:
		break;
	case InventoryLocation::PLAYER:
	{
		Player *player = m_env->getPlayer(loc.name.c_str());
		if(!player)
			return NULL;
		PlayerSAO *playersao = player->getPlayerSAO();
		if(!playersao)
			return NULL;
		return playersao->getInventory();
	}
		break;
	case InventoryLocation::NODEMETA:
	{
		NodeMetadata *meta = m_env->getMap().getNodeMetadata(loc.p);
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
		break;
	}
	return NULL;
}
void Server::setInventoryModified(const InventoryLocation &loc, bool playerSend)
{
	switch(loc.type){
	case InventoryLocation::UNDEFINED:
		break;
	case InventoryLocation::PLAYER:
	{
		if (!playerSend)
			return;

		Player *player = m_env->getPlayer(loc.name.c_str());
		if(!player)
			return;
		PlayerSAO *playersao = player->getPlayerSAO();
		if(!playersao)
			return;

		SendInventory(playersao);
	}
	break;
	case InventoryLocation::NODEMETA:
	{
		v3s16 blockpos = getNodeBlockPos(loc.p);

		MapBlock *block = m_env->getMap().getBlockNoCreateNoEx(blockpos);
		if(block)
			block->raiseModified(MOD_STATE_WRITE_NEEDED, MOD_REASON_REPORT_META_CHANGE);

		setBlockNotSent(blockpos);
	}
	break;
	case InventoryLocation::DETACHED:
	{
		sendDetachedInventory(loc.name,PEER_ID_INEXISTENT);
	}
	break;
	default:
		break;
	}
}

void Server::SetBlocksNotSent(std::map<v3s16, MapBlock *>& block)
{
	std::vector<u16> clients = m_clients.getClientIDs();
	// Set the modified blocks unsent for all the clients
	for (auto
		 i = clients.begin();
		 i != clients.end(); ++i) {
			RemoteClient *client = m_clients.lockedGetClientNoEx(*i);
			if (client != NULL)
				client->SetBlocksNotSent(block);
		}
}

void Server::peerAdded(u16 peer_id)
{
	DSTACK(__FUNCTION_NAME);
	verbosestream<<"Server::peerAdded(): peer->id="
			<<peer_id<<std::endl;

	con::PeerChange c;
	c.type = con::PEER_ADDED;
	c.peer_id = peer_id;
	c.timeout = false;
	m_peer_change_queue.push(c);
}

void Server::deletingPeer(u16 peer_id, bool timeout)
{
	DSTACK(__FUNCTION_NAME);
	verbosestream<<"Server::deletingPeer(): peer->id="
			<<peer_id<<", timeout="<<timeout<<std::endl;

	m_clients.event(peer_id, CSE_Disconnect);
	con::PeerChange c;
	c.type = con::PEER_REMOVED;
	c.peer_id = peer_id;
	c.timeout = timeout;
	m_peer_change_queue.push(c);
}

bool Server::getClientConInfo(u16 peer_id, con::rtt_stat_type type, float* retval)
{
	*retval = m_con.getPeerStat(peer_id,type);
	if (*retval == -1) return false;
	return true;
}

bool Server::getClientInfo(
		u16          peer_id,
		ClientState* state,
		u32*         uptime,
		u8*          ser_vers,
		u16*         prot_vers,
		u8*          major,
		u8*          minor,
		u8*          patch,
		std::string* vers_string
	)
{
	*state = m_clients.getClientState(peer_id);
	RemoteClient* client = m_clients.lockedGetClientNoEx(peer_id, CS_Invalid);

	if (client == NULL) {
		return false;
	}

	*uptime = client->uptime();
	*ser_vers = client->serialization_version;
	*prot_vers = client->net_proto_version;

	*major = client->getMajor();
	*minor = client->getMinor();
	*patch = client->getPatch();
	*vers_string = client->getPatch();

	return true;
}

void Server::handlePeerChanges()
{
	while(m_peer_change_queue.size() > 0)
	{
		con::PeerChange c = m_peer_change_queue.pop_front();

		verbosestream<<"Server: Handling peer change: "
				<<"id="<<c.peer_id<<", timeout="<<c.timeout
				<<std::endl;

		switch(c.type)
		{
			case con::PEER_ADDED:
				m_clients.CreateClient(c.peer_id);
				break;

			case con::PEER_REMOVED:
				DeleteClient(c.peer_id, c.timeout?CDR_TIMEOUT:CDR_LEAVE);
				break;

			default:
				assert("Invalid peer change event received!" == 0);
				break;
		}
	}
}

#if MINETEST_PROTO
void Server::Send(NetworkPacket* pkt)
{
	g_profiler->add("Server: Packets sended", 1);
	m_clients.send(pkt->getPeerId(),
		clientCommandFactoryTable[pkt->getCommand()].channel,
		pkt,
		clientCommandFactoryTable[pkt->getCommand()].reliable);
}

void Server::SendMovement(u16 peer_id)
{
	DSTACK(__FUNCTION_NAME);
	std::ostringstream os(std::ios_base::binary);

	NetworkPacket pkt(TOCLIENT_MOVEMENT, 12 * sizeof(float), peer_id);

	pkt << g_settings->getFloat("movement_acceleration_default");
	pkt << g_settings->getFloat("movement_acceleration_air");
	pkt << g_settings->getFloat("movement_acceleration_fast");
	pkt << g_settings->getFloat("movement_speed_walk");
	pkt << g_settings->getFloat("movement_speed_crouch");
	pkt << g_settings->getFloat("movement_speed_fast");
	pkt << g_settings->getFloat("movement_speed_climb");
	pkt << g_settings->getFloat("movement_speed_jump");
	pkt << g_settings->getFloat("movement_liquid_fluidity");
	pkt << g_settings->getFloat("movement_liquid_fluidity_smooth");
	pkt << g_settings->getFloat("movement_liquid_sink");
	pkt << g_settings->getFloat("movement_gravity");

	Send(&pkt);
}
#endif

void Server::SendPlayerHPOrDie(PlayerSAO *playersao)
{
	if (!g_settings->getBool("enable_damage"))
		return;

	u16 peer_id   = playersao->getPeerID();
	bool is_alive = playersao->getHP() > 0;

	if (is_alive)
		SendPlayerHP(peer_id);
	else
		DiePlayer(peer_id);
}


#if MINETEST_PROTO
void Server::SendHP(u16 peer_id, u8 hp)
{
	DSTACK(__FUNCTION_NAME);

	NetworkPacket pkt(TOCLIENT_HP, 1, peer_id);
	pkt << hp;
	Send(&pkt);
}

void Server::SendBreath(u16 peer_id, u16 breath)
{
	DSTACK(__FUNCTION_NAME);

	NetworkPacket pkt(TOCLIENT_BREATH, 2, peer_id);
	pkt << (u16) breath;
	Send(&pkt);
}

void Server::SendAccessDenied(u16 peer_id, AccessDeniedCode reason,
		const std::string &custom_reason, bool reconnect)
{
	if (reason >= SERVER_ACCESSDENIED_MAX)
		return;

	NetworkPacket pkt(TOCLIENT_ACCESS_DENIED, 1, peer_id);
	pkt << (u8)reason;
	if (reason == SERVER_ACCESSDENIED_CUSTOM_STRING)
		pkt << narrow_to_wide(custom_reason);
	else if (reason == SERVER_ACCESSDENIED_SHUTDOWN ||
			reason == SERVER_ACCESSDENIED_CRASH)
		pkt << narrow_to_wide(custom_reason) << (u8)reconnect;
	Send(&pkt);
}

void Server::SendAccessDenied_Legacy(u16 peer_id,const std::string &reason)
{
	DSTACK(__FUNCTION_NAME);

	NetworkPacket pkt(TOCLIENT_ACCESS_DENIED_LEGACY, 0, peer_id);
	pkt << narrow_to_wide(reason);
	Send(&pkt);
}

void Server::SendDeathscreen(u16 peer_id,bool set_camera_point_target,
		v3f camera_point_target)
{
	DSTACK(__FUNCTION_NAME);

	NetworkPacket pkt(TOCLIENT_DEATHSCREEN, 1 + sizeof(v3f), peer_id);
	pkt << set_camera_point_target << camera_point_target;
	Send(&pkt);
}

void Server::SendItemDef(u16 peer_id,
		IItemDefManager *itemdef, u16 protocol_version)
{
	DSTACK(__FUNCTION_NAME);

	NetworkPacket pkt(TOCLIENT_ITEMDEF, 0, peer_id);

	/*
		u16 command
		u32 length of the next item
		zlib-compressed serialized ItemDefManager
	*/
	std::ostringstream tmp_os(std::ios::binary);
	itemdef->serialize(tmp_os, protocol_version);
	std::ostringstream tmp_os2(std::ios::binary);
	compressZlib(tmp_os.str(), tmp_os2);
	pkt.putLongString(tmp_os2.str());

	// Make data buffer
	verbosestream << "Server: Sending item definitions to id(" << peer_id
			<< "): size=" << pkt.getSize() << std::endl;

	Send(&pkt);
}

void Server::SendNodeDef(u16 peer_id,
		INodeDefManager *nodedef, u16 protocol_version)
{
	DSTACK(__FUNCTION_NAME);

	NetworkPacket pkt(TOCLIENT_NODEDEF, 0, peer_id);

	/*
		u16 command
		u32 length of the next item
		zlib-compressed serialized NodeDefManager
	*/
	std::ostringstream tmp_os(std::ios::binary);
	nodedef->serialize(tmp_os, protocol_version);
	std::ostringstream tmp_os2(std::ios::binary);
	compressZlib(tmp_os.str(), tmp_os2);

	pkt.putLongString(tmp_os2.str());

	// Make data buffer
	verbosestream << "Server: Sending node definitions to id(" << peer_id
			<< "): size=" << pkt.getSize() << std::endl;

	Send(&pkt);
}

/*
	Non-static send methods
*/

void Server::SendInventory(PlayerSAO* playerSAO)
{
	DSTACK(__FUNCTION_NAME);

	UpdateCrafting(playerSAO->getPlayer());

	/*
		Serialize it
	*/

	NetworkPacket pkt(TOCLIENT_INVENTORY, 0, playerSAO->getPeerID());

	std::ostringstream os;
	playerSAO->getInventory()->serialize(os);

	std::string s = os.str();

	pkt.putRawString(s.c_str(), s.size());
	Send(&pkt);
}

void Server::SendChatMessage(u16 peer_id, const std::string &message)
{
	DSTACK(__FUNCTION_NAME);

	NetworkPacket pkt(TOCLIENT_CHAT_MESSAGE, 0, peer_id);
	pkt << narrow_to_wide(message);

	if (peer_id != PEER_ID_INEXISTENT) {
		Send(&pkt);
	}
	else {
		m_clients.sendToAll(0, &pkt, true);
	}
}

void Server::SendShowFormspecMessage(u16 peer_id, const std::string &formspec,
                                     const std::string &formname)
{
	DSTACK(__FUNCTION_NAME);

	NetworkPacket pkt(TOCLIENT_SHOW_FORMSPEC, 0 , peer_id);

	pkt.putLongString(FORMSPEC_VERSION_STRING + formspec);
	pkt << formname;

	Send(&pkt);
}

// Spawns a particle on peer with peer_id
void Server::SendSpawnParticle(u16 peer_id, v3f pos, v3f velocity, v3f acceleration,
				float expirationtime, float size, bool collisiondetection,
				bool vertical, std::string texture)
{
	DSTACK(__FUNCTION_NAME);

	NetworkPacket pkt(TOCLIENT_SPAWN_PARTICLE, 0, peer_id);

	pkt << pos << velocity << acceleration << expirationtime
			<< size << collisiondetection;
	pkt.putLongString(texture);
	pkt << vertical;

	if (peer_id != PEER_ID_INEXISTENT) {
		Send(&pkt);
	}
	else {
		m_clients.sendToAll(0, &pkt, true);
	}
}

// Adds a ParticleSpawner on peer with peer_id
void Server::SendAddParticleSpawner(u16 peer_id, u16 amount, float spawntime, v3f minpos, v3f maxpos,
	v3f minvel, v3f maxvel, v3f minacc, v3f maxacc, float minexptime, float maxexptime,
	float minsize, float maxsize, bool collisiondetection, bool vertical, std::string texture, u32 id)
{
	DSTACK(__FUNCTION_NAME);

	NetworkPacket pkt(TOCLIENT_ADD_PARTICLESPAWNER, 0, peer_id);

	pkt << amount << spawntime << minpos << maxpos << minvel << maxvel
			<< minacc << maxacc << minexptime << maxexptime << minsize
			<< maxsize << collisiondetection;

	pkt.putLongString(texture);

	pkt << id << vertical;

	if (peer_id != PEER_ID_INEXISTENT) {
		Send(&pkt);
	}
	else {
		m_clients.sendToAll(0, &pkt, true);
	}
}

void Server::SendDeleteParticleSpawner(u16 peer_id, u32 id)
{
	DSTACK(__FUNCTION_NAME);

	NetworkPacket pkt(TOCLIENT_DELETE_PARTICLESPAWNER_LEGACY, 2, peer_id);

	// Ugly error in this packet
	pkt << (u16) id;

	if (peer_id != PEER_ID_INEXISTENT) {
		Send(&pkt);
	}
	else {
		m_clients.sendToAll(0, &pkt, true);
	}

}

void Server::SendHUDAdd(u16 peer_id, u32 id, HudElement *form)
{
	NetworkPacket pkt(TOCLIENT_HUDADD, 0 , peer_id);

	pkt << id << (u8) form->type << form->pos << form->name << form->scale
			<< form->text << form->number << form->item << form->dir
			<< form->align << form->offset << form->world_pos << form->size;

	Send(&pkt);
}

void Server::SendHUDRemove(u16 peer_id, u32 id)
{
	NetworkPacket pkt(TOCLIENT_HUDRM, 4, peer_id);
	pkt << id;
	Send(&pkt);
}

void Server::SendHUDChange(u16 peer_id, u32 id, HudElementStat stat, void *value)
{
	NetworkPacket pkt(TOCLIENT_HUDCHANGE, 0, peer_id);
	pkt << id << (u8) stat;

	switch (stat) {
		case HUD_STAT_POS:
		case HUD_STAT_SCALE:
		case HUD_STAT_ALIGN:
		case HUD_STAT_OFFSET:
			pkt << *(v2f *) value;
			break;
		case HUD_STAT_NAME:
		case HUD_STAT_TEXT:
			pkt << *(std::string *) value;
			break;
		case HUD_STAT_WORLD_POS:
			pkt << *(v3f *) value;
			break;
		case HUD_STAT_SIZE:
			pkt << *(v2s32 *) value;
			break;
		case HUD_STAT_NUMBER:
		case HUD_STAT_ITEM:
		case HUD_STAT_DIR:
		default:
			pkt << *(u32 *) value;
			break;
	}

	Send(&pkt);
}

void Server::SendHUDSetFlags(u16 peer_id, u32 flags, u32 mask)
{
	NetworkPacket pkt(TOCLIENT_HUD_SET_FLAGS, 4 + 4, peer_id);

	flags &= ~(HUD_FLAG_HEALTHBAR_VISIBLE | HUD_FLAG_BREATHBAR_VISIBLE);

	pkt << flags << mask;

	Send(&pkt);
}

void Server::SendHUDSetParam(u16 peer_id, u16 param, const std::string &value)
{
	NetworkPacket pkt(TOCLIENT_HUD_SET_PARAM, 0, peer_id);
	pkt << param << value;
	Send(&pkt);
}

void Server::SendSetSky(u16 peer_id, const video::SColor &bgcolor,
		const std::string &type, const std::vector<std::string> &params)
{
	NetworkPacket pkt(TOCLIENT_SET_SKY, 0, peer_id);
	pkt << bgcolor << type << (u16) params.size();

	for(size_t i=0; i<params.size(); i++)
		pkt << params[i];

	Send(&pkt);
}

void Server::SendOverrideDayNightRatio(u16 peer_id, bool do_override,
		float ratio)
{
	NetworkPacket pkt(TOCLIENT_OVERRIDE_DAY_NIGHT_RATIO,
			1 + 2, peer_id);

	pkt << do_override << (u16) (ratio * 65535);

	Send(&pkt);
}

void Server::SendTimeOfDay(u16 peer_id, u16 time, f32 time_speed)
{
	DSTACK(__FUNCTION_NAME);

	NetworkPacket pkt(TOCLIENT_TIME_OF_DAY, 0, peer_id);
	pkt << time << time_speed;

	if (peer_id == PEER_ID_INEXISTENT) {
		m_clients.sendToAll(0, &pkt, true);
	}
	else {
		Send(&pkt);
	}
}

void Server::SendPlayerHP(u16 peer_id)
{
	DSTACK(__FUNCTION_NAME);
	PlayerSAO *playersao = getPlayerSAO(peer_id);
	// In some rare case, if the player is disconnected
	// while Lua call l_punch, for example, this can be NULL
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
	DSTACK(__FUNCTION_NAME);
	PlayerSAO *playersao = getPlayerSAO(peer_id);
	assert(playersao);

	m_script->player_event(playersao, "breath_changed");
	SendBreath(peer_id, playersao->getBreath());
}

void Server::SendMovePlayer(u16 peer_id)
{
	DSTACK(__FUNCTION_NAME);
	Player *player = m_env->getPlayer(peer_id);
	assert(player);

	NetworkPacket pkt(TOCLIENT_MOVE_PLAYER, sizeof(v3f) + sizeof(f32) * 2, peer_id);
	pkt << player->getPosition() << player->getPitch() << player->getYaw();

	{
		v3f pos = player->getPosition();
		f32 pitch = player->getPitch();
		f32 yaw = player->getYaw();
		verbosestream << "Server: Sending TOCLIENT_MOVE_PLAYER"
				<< " pos=(" << pos.X << "," << pos.Y << "," << pos.Z << ")"
				<< " pitch=" << pitch
				<< " yaw=" << yaw
				<< std::endl;
	}

	Send(&pkt);
}

void Server::SendLocalPlayerAnimations(u16 peer_id, v2s32 animation_frames[4], f32 animation_speed)
{
	NetworkPacket pkt(TOCLIENT_LOCAL_PLAYER_ANIMATIONS, 0,
		peer_id);

	pkt << animation_frames[0] << animation_frames[1] << animation_frames[2]
			<< animation_frames[3] << animation_speed;

	Send(&pkt);
}

void Server::SendEyeOffset(u16 peer_id, v3f first, v3f third)
{
	NetworkPacket pkt(TOCLIENT_EYE_OFFSET, 0, peer_id);
	pkt << first << third;
	Send(&pkt);
}
void Server::SendPlayerPrivileges(u16 peer_id)
{
	Player *player = m_env->getPlayer(peer_id);
	assert(player);
	if(player->peer_id == PEER_ID_INEXISTENT)
		return;

	std::set<std::string> privs;
	m_script->getAuth(player->getName(), NULL, &privs);

	NetworkPacket pkt(TOCLIENT_PRIVILEGES, 0, peer_id);
	pkt << (u16) privs.size();

	for(std::set<std::string>::const_iterator i = privs.begin();
			i != privs.end(); i++) {
		pkt << (*i);
	}

	Send(&pkt);
}

void Server::SendPlayerInventoryFormspec(u16 peer_id)
{
	Player *player = m_env->getPlayer(peer_id);
	assert(player);
	if(player->peer_id == PEER_ID_INEXISTENT)
		return;

	NetworkPacket pkt(TOCLIENT_INVENTORY_FORMSPEC, 0, peer_id);
	pkt.putLongString(FORMSPEC_VERSION_STRING + player->inventory_formspec);
	Send(&pkt);
}

u32 Server::SendActiveObjectRemoveAdd(u16 peer_id, const std::string &datas)
{
	NetworkPacket pkt(TOCLIENT_ACTIVE_OBJECT_REMOVE_ADD, datas.size(), peer_id);
	pkt.putRawString(datas.c_str(), datas.size());
	Send(&pkt);
	return pkt.getSize();
}

void Server::SendActiveObjectMessages(u16 peer_id, const std::string &datas, bool reliable)
{
	NetworkPacket pkt(TOCLIENT_ACTIVE_OBJECT_MESSAGES,
			datas.size(), peer_id);

	pkt.putRawString(datas.c_str(), datas.size());

	m_clients.send(pkt.getPeerId(),
			reliable ? clientCommandFactoryTable[pkt.getCommand()].channel : 1,
			&pkt, reliable);

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
	else {
		std::vector<u16> clients = m_clients.getClientIDs();

		for(std::vector<u16>::iterator
				i = clients.begin(); i != clients.end(); ++i) {
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

	NetworkPacket pkt(TOCLIENT_PLAY_SOUND, 0);
	pkt << id << spec.name << (float) (spec.gain * params.gain)
			<< (u8) params.type << pos << params.object << params.loop;

	for(std::vector<u16>::iterator i = dst_clients.begin();
			i != dst_clients.end(); i++) {
		psound.clients.insert(*i);
		m_clients.send(*i, 0, &pkt, true);
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

	NetworkPacket pkt(TOCLIENT_STOP_SOUND, 4);
	pkt << handle;

	for(std::set<u16>::iterator i = psound.clients.begin();
			i != psound.clients.end(); i++) {
		// Send as reliable
		m_clients.send(*i, 0, &pkt, true);
	}
	// Remove sound reference
	m_playing_sounds.erase(i);
}

void Server::sendRemoveNode(v3s16 p, u16 ignore_id,
	std::vector<u16> *far_players, float far_d_nodes)
{
	float maxd = far_d_nodes*BS;
	v3f p_f = intToFloat(p, BS);

	NetworkPacket pkt(TOCLIENT_REMOVENODE, 6);
	pkt << p;

	std::vector<u16> clients = m_clients.getClientIDs();
	for(std::vector<u16>::iterator i = clients.begin();
		i != clients.end(); ++i) {
		if (far_players) {
			// Get player
			if(Player *player = m_env->getPlayer(*i)) {
				// If player is far away, only set modified blocks not sent
				v3f player_pos = player->getPosition();
				if(player_pos.getDistanceFrom(p_f) > maxd) {
					far_players->push_back(*i);
					continue;
				}
			}
		}

		// Send as reliable
		m_clients.send(*i, 0, &pkt, true);
	}
}

void Server::sendAddNode(v3s16 p, MapNode n, u16 ignore_id,
		std::vector<u16> *far_players, float far_d_nodes,
		bool remove_metadata)
{
	float maxd = far_d_nodes*BS;
	v3f p_f = intToFloat(p, BS);

	std::vector<u16> clients = m_clients.getClientIDs();
	for(std::vector<u16>::iterator i = clients.begin();
			i != clients.end(); ++i) {

		if(far_players) {
			// Get player
			if(Player *player = m_env->getPlayer(*i)) {
				// If player is far away, only set modified blocks not sent
				v3f player_pos = player->getPosition();
				if(player_pos.getDistanceFrom(p_f) > maxd) {
					far_players->push_back(*i);
					continue;
				}
			}
		}

		NetworkPacket pkt(TOCLIENT_ADDNODE, 6 + 2 + 1 + 1 + 1);
		m_clients.Lock();
		RemoteClient* client = m_clients.lockedGetClientNoEx(*i);
		if (client != 0) {
			pkt << p << n.param0 << n.param1 << n.param2
					<< (u8) (remove_metadata ? 0 : 1);

			if (!remove_metadata) {
				if (client->net_proto_version <= 21) {
					// Old clients always clear metadata; fix it
					// by sending the full block again.
					client->SetBlockNotSent(p);
				}
			}
		}
		m_clients.Unlock();

		// Send as reliable
		if (pkt.getSize() > 0)
			m_clients.send(*i, 0, &pkt, true);
	}
}

#endif

void Server::SendChatMessage(u16 peer_id, const std::wstring &message) {
	SendChatMessage(peer_id, wide_to_narrow(message));
}

void Server::setBlockNotSent(v3s16 p)
{
	auto clients = m_clients.getClientIDs();
	for(auto
		i = clients.begin();
		i != clients.end(); ++i)
	{
		RemoteClient *client = m_clients.lockedGetClientNoEx(*i);
		client->SetBlockNotSent(p);
	}
}

#if MINETEST_PROTO

void Server::SendBlockNoLock(u16 peer_id, MapBlock *block, u8 ver, u16 net_proto_version)
{
	DSTACK(__FUNCTION_NAME);

	v3s16 p = block->getPos();

	/*
		Create a packet with the block in the right format
	*/

	std::ostringstream os(std::ios_base::binary);
	block->serialize(os, ver, false);
	block->serializeNetworkSpecific(os, net_proto_version);
	std::string s = os.str();

	NetworkPacket pkt(TOCLIENT_BLOCKDATA, 2 + 2 + 2 + 2 + s.size(), peer_id);

	pkt << p;
	pkt.putRawString(s.c_str(), s.size());
	Send(&pkt);
}

#endif

int Server::SendBlocks(float dtime)
{
	DSTACK(__FUNCTION_NAME);
	//TimeTaker timer("SendBlocks inside");

	//JMutexAutoLock envlock(m_env_mutex);
	//TODO check if one big lock could be faster then multiple small ones

	//ScopeProfiler sp(g_profiler, "Server: sel and send blocks to clients");

	int total = 0;

	std::vector<PrioritySortedBlockTransfer> queue;

	{
		//ScopeProfiler sp(g_profiler, "Server: selecting blocks for sending");

		std::vector<u16> clients = m_clients.getClientIDs();

		for(auto
			i = clients.begin();
			i != clients.end(); ++i)
		{
			auto client = m_clients.getClient(*i, CS_Active);

			if (client == NULL)
				continue;

			total += client->GetNextBlocks(m_env, m_emerge, dtime, m_uptime.get() + m_env->m_game_time_start, queue);
		}
	}

	// Sort.
	// Lowest priority number comes first.
	// Lowest is most important.
	std::sort(queue.begin(), queue.end());

	for(u32 i=0; i<queue.size(); i++)
	{
		//TODO: Calculate limit dynamically

		PrioritySortedBlockTransfer q = queue[i];

		MapBlock *block = NULL;
		try
		{
#if !ENABLE_THREADS
			auto lock = m_env->getServerMap().m_nothread_locker.lock_shared_rec();
#endif
			block = m_env->getMap().getBlockNoCreate(q.pos);
		}
		catch(InvalidPositionException &e)
		{
			continue;
		}

		RemoteClient *client = m_clients.lockedGetClientNoEx(q.peer_id, CS_Active);

		if(!client)
			continue;

		{
		auto lock = block->try_lock_shared_rec();
		if (!lock->owns_lock())
			continue;

		// maybe sometimes blocks will not load (must wait 1+ minute), but reduce network load: q.priority<=4
		SendBlockNoLock(q.peer_id, block, client->serialization_version, client->net_proto_version);
		}

		client->SentBlock(q.pos, m_uptime.get() + m_env->m_game_time_start);
		++total;
	}
	return total;
}

void Server::fillMediaCache()
{
	DSTACK(__FUNCTION_NAME);

	infostream<<"Server: Calculating media file checksums"<<std::endl;

	// Collect all media file paths
	std::list<std::string> paths;
	for(std::vector<ModSpec>::iterator i = m_mods.begin();
			i != m_mods.end(); i++){
		const ModSpec &mod = *i;
		paths.push_back(mod.path + DIR_DELIM + "textures");
		paths.push_back(mod.path + DIR_DELIM + "sounds");
		paths.push_back(mod.path + DIR_DELIM + "media");
		paths.push_back(mod.path + DIR_DELIM + "models");
	}
	paths.push_back(porting::path_user + DIR_DELIM + "textures" + DIR_DELIM + "server");

	unsigned int size_total = 0, files_total = 0;
	// Collect media file information from paths into cache
	for(std::list<std::string>::iterator i = paths.begin();
			i != paths.end(); i++)
	{
		std::string mediapath = *i;
		std::vector<fs::DirListNode> dirlist = fs::GetDirListing(mediapath);
		for(u32 j=0; j<dirlist.size(); j++){
			if(dirlist[j].dir) // Ignode dirs
				continue;
			std::string filename = dirlist[j].name;
			// If name contains illegal characters, ignore the file
			if(!string_allowed(filename, TEXTURENAME_ALLOWED_CHARS)){
				infostream<<"Server: ignoring illegal file name: \""
						<<filename<<"\""<<std::endl;
				continue;
			}
			// If name is not in a supported format, ignore it
			const char *supported_ext[] = {
				".png", ".jpg", ".bmp", ".tga",
				".pcx", ".ppm", ".psd", ".wal", ".rgb",
				".ogg",
				".x", ".b3d", ".md2", ".obj",
				NULL
			};
			if(removeStringEnd(filename, supported_ext) == ""){
				infostream<<"Server: ignoring unsupported file extension: \""
						<<filename<<"\""<<std::endl;
				continue;
			}
			// Ok, attempt to load the file and add to cache
			std::string filepath = mediapath + DIR_DELIM + filename;
			// Read data
			std::ifstream fis(filepath.c_str(), std::ios_base::binary);
			if(fis.good() == false){
				errorstream<<"Server::fillMediaCache(): Could not open \""
						<<filename<<"\" for reading"<<std::endl;
				continue;
			}
			std::ostringstream tmp_os(std::ios_base::binary);
			bool bad = false;
			for(;;){
				char buf[1024];
				fis.read(buf, 1024);
				std::streamsize len = fis.gcount();
				tmp_os.write(buf, len);
				if(fis.eof())
					break;
				if(!fis.good()){
					bad = true;
					break;
				}
			}
			if(bad){
				errorstream<<"Server::fillMediaCache(): Failed to read \""
						<<filename<<"\""<<std::endl;
				continue;
			}
			if(tmp_os.str().length() == 0){
				errorstream<<"Server::fillMediaCache(): Empty file \""
						<<filepath<<"\""<<std::endl;
				continue;
			}
			size_total += tmp_os.str().length();
			++files_total;

			SHA1 sha1;
			sha1.addBytes(tmp_os.str().c_str(), tmp_os.str().length());

			unsigned char *digest = sha1.getDigest();
			std::string sha1_base64 = base64_encode(digest, 20);
			std::string sha1_hex = hex_encode((char*)digest, 20);
			free(digest);

			// Put in list
			this->m_media[filename] = MediaInfo(filepath, sha1_base64);
			verbosestream<<"Server: "<<sha1_hex<<" is "<<filename<<std::endl;
		}
	}
	actionstream << "Serving " << files_total <<" files, " << size_total << " bytes" << std::endl;
}


#if MINETEST_PROTO

void Server::sendMediaAnnouncement(u16 peer_id)
{
	DSTACK(__FUNCTION_NAME);

	verbosestream << "Server: Announcing files to id(" << peer_id << ")"
		<< std::endl;

	// Make packet
	std::ostringstream os(std::ios_base::binary);

	NetworkPacket pkt(TOCLIENT_ANNOUNCE_MEDIA, 0, peer_id);
	pkt << (u16) m_media.size();

	for (std::map<std::string, MediaInfo>::iterator i = m_media.begin();
			i != m_media.end(); ++i) {
		pkt << i->first << i->second.sha1_digest;
	}

	pkt << g_settings->get("remote_media");
	Send(&pkt);
}

#endif

struct SendableMedia
{
	std::string name;
	std::string path;
	std::string data;

	SendableMedia(const std::string &name_="", const std::string &path_="",
	              const std::string &data_=""):
		name(name_),
		path(path_),
		data(data_)
	{}
};

#if MINETEST_PROTO

void Server::sendRequestedMedia(u16 peer_id,
		const std::vector<std::string> &tosend)
{
	DSTACK(__FUNCTION_NAME);

	verbosestream<<"Server::sendRequestedMedia(): "
			<<"Sending files to client"<<std::endl;

	/* Read files */

	// Put 5kB in one bunch (this is not accurate)
	u32 bytes_per_bunch = 5000;

	std::vector< std::vector<SendableMedia> > file_bunches;
	file_bunches.push_back(std::vector<SendableMedia>());

	u32 file_size_bunch_total = 0;

	for(std::vector<std::string>::const_iterator i = tosend.begin();
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
		std::ostringstream tmp_os(std::ios_base::binary);
		bool bad = false;
		for(;;) {
			char buf[1024];
			fis.read(buf, 1024);
			std::streamsize len = fis.gcount();
			tmp_os.write(buf, len);
			file_size_bunch_total += len;
			if(fis.eof())
				break;
			if(!fis.good()) {
				bad = true;
				break;
			}
		}
		if(bad) {
			errorstream<<"Server::sendRequestedMedia(): Failed to read \""
					<<name<<"\""<<std::endl;
			continue;
		}
		/*infostream<<"Server::sendRequestedMedia(): Loaded \""
				<<tname<<"\""<<std::endl;*/
		// Put in list
		file_bunches[file_bunches.size()-1].push_back(
				SendableMedia(name, tpath, tmp_os.str()));

		// Start next bunch if got enough data
		if(file_size_bunch_total >= bytes_per_bunch) {
			file_bunches.push_back(std::vector<SendableMedia>());
			file_size_bunch_total = 0;
		}

	}

	/* Create and send packets */

	u16 num_bunches = file_bunches.size();
	for(u16 i = 0; i < num_bunches; i++) {
		/*
			u16 command
			u16 total number of texture bunches
			u16 index of this bunch
			u32 number of files in this bunch
			for each file {
				u16 length of name
				string name
				u32 length of data
				data
			}
		*/

		NetworkPacket pkt(TOCLIENT_MEDIA, 4 + 0, peer_id);
		pkt << num_bunches << i << (u32) file_bunches[i].size();

		for(std::vector<SendableMedia>::iterator
				j = file_bunches[i].begin();
				j != file_bunches[i].end(); ++j) {
			pkt << j->name;
			pkt.putLongString(j->data);
		}

		verbosestream << "Server::sendRequestedMedia(): bunch "
				<< i << "/" << num_bunches
				<< " files=" << file_bunches[i].size()
				<< " size="  << pkt.getSize() << std::endl;
		Send(&pkt);
	}
}

void Server::sendDetachedInventory(const std::string &name, u16 peer_id)
{
	if(m_detached_inventories.count(name) == 0) {
		errorstream<<__FUNCTION_NAME<<": \""<<name<<"\" not found"<<std::endl;
		return;
	}
	Inventory *inv = m_detached_inventories[name];
	std::ostringstream os(std::ios_base::binary);

	os << serializeString(name);
	inv->serialize(os);

	// Make data buffer
	std::string s = os.str();

	NetworkPacket pkt(TOCLIENT_DETACHED_INVENTORY, 0, peer_id);
	pkt.putRawString(s.c_str(), s.size());

	if (peer_id != PEER_ID_INEXISTENT) {
		Send(&pkt);
	}
	else {
		m_clients.sendToAll(0, &pkt, true);
	}
}

#endif

void Server::sendDetachedInventories(u16 peer_id)
{
	DSTACK(__FUNCTION_NAME);

	for(std::map<std::string, Inventory*>::iterator
			i = m_detached_inventories.begin();
			i != m_detached_inventories.end(); i++){
		const std::string &name = i->first;
		//Inventory *inv = i->second;
		sendDetachedInventory(name, peer_id);
	}
}

/*
	Something random
*/

void Server::DiePlayer(u16 peer_id)
{
	DSTACK(__FUNCTION_NAME);

	PlayerSAO *playersao = getPlayerSAO(peer_id);
	if (!playersao)
		return;

	playersao->m_ms_from_last_respawn = 0;

	infostream << "Server::DiePlayer(): Player "
			<< playersao->getPlayer()->getName()
			<< " dies" << std::endl;

	playersao->setHP(0);

	// Trigger scripted stuff
	m_script->on_dieplayer(playersao);

	SendPlayerHP(peer_id);
	SendDeathscreen(peer_id, false, v3f(0,0,0));

	stat.add("die", playersao->getPlayer()->getName());
}

void Server::RespawnPlayer(u16 peer_id)
{
	DSTACK(__FUNCTION_NAME);

	PlayerSAO *playersao = getPlayerSAO(peer_id);
	if (!playersao)
		return;

	infostream << "Server::RespawnPlayer(): Player "
			<< playersao->getPlayer()->getName()
			<< " respawns" << std::endl;

	playersao->setHP(PLAYER_MAX_HP);
	playersao->setBreath(PLAYER_MAX_BREATH);

	SendPlayerHP(peer_id);
	SendPlayerBreath(peer_id);

	bool repositioned = m_script->on_respawnplayer(playersao);
	if(!repositioned){
		v3f pos = findSpawnPos();
		// setPos will send the new position to client
		playersao->setPos(pos);
	}

	playersao->m_ms_from_last_respawn = 0;

	stat.add("respawn", playersao->getPlayer()->getName());
}


#if MINETEST_PROTO
void Server::DenySudoAccess(u16 peer_id)
{
	DSTACK(__FUNCTION_NAME);

	NetworkPacket pkt(TOCLIENT_DENY_SUDO_MODE, 0, peer_id);
	Send(&pkt);
}
#endif


void Server::DenyAccessVerCompliant(u16 peer_id, u16 proto_ver, AccessDeniedCode reason,
		const std::string &str_reason, bool reconnect)
{
	if (proto_ver >= 25) {
		SendAccessDenied(peer_id, reason, str_reason);
	} else {
		std::string wreason = (
			reason == SERVER_ACCESSDENIED_CUSTOM_STRING ? str_reason :
			accessDeniedStrings[(u8)reason]);
#if MINETEST_PROTO
		SendAccessDenied_Legacy(peer_id, wreason);
#endif
	}

	m_clients.event(peer_id, CSE_SetDenied);
	m_con.DisconnectPeer(peer_id);
}


void Server::DenyAccess(u16 peer_id, AccessDeniedCode reason, const std::string &custom_reason)
{
	DSTACK(__FUNCTION_NAME);

	SendAccessDenied(peer_id, reason, custom_reason);
	m_clients.event(peer_id, CSE_SetDenied);
	m_con.DisconnectPeer(peer_id);
}

//fmtodo: remove:
void Server::DenyAccess(u16 peer_id, const std::string &custom_reason)
{
    DenyAccess(peer_id, SERVER_ACCESSDENIED_CUSTOM_STRING, custom_reason);
}

//fmtodo: remove:
void Server::DenyAccess_Legacy(u16 peer_id, const std::wstring &custom_reason)
{
    DenyAccess(peer_id, SERVER_ACCESSDENIED_CUSTOM_STRING, wide_to_narrow(custom_reason));
}

#if MINETEST_PROTO
void Server::acceptAuth(u16 peer_id, bool forSudoMode)
{
	DSTACK(__FUNCTION_NAME);

	if (!forSudoMode) {
		RemoteClient* client = getClient(peer_id, CS_Invalid);

		NetworkPacket resp_pkt(TOCLIENT_AUTH_ACCEPT, 1 + 6 + 8 + 4, peer_id);

		// Right now, the auth mechs don't change between login and sudo mode.
		u32 sudo_auth_mechs = client->allowed_auth_mechs;
		client->allowed_sudo_mechs = sudo_auth_mechs;

		resp_pkt << v3f(0,0,0) << (u64) m_env->getServerMap().getSeed()
				<< g_settings->getFloat("dedicated_server_step")
				<< sudo_auth_mechs;

		Send(&resp_pkt);
		m_clients.event(peer_id, CSE_AuthAccept);
	} else {
		NetworkPacket resp_pkt(TOCLIENT_ACCEPT_SUDO_MODE, 1 + 6 + 8 + 4, peer_id);

		// We only support SRP right now
		u32 sudo_auth_mechs = AUTH_MECHANISM_FIRST_SRP;

		resp_pkt << sudo_auth_mechs;
		Send(&resp_pkt);
		m_clients.event(peer_id, CSE_SudoSuccess);
	}
}
#endif

void Server::DeleteClient(u16 peer_id, ClientDeletionReason reason)
{
	DSTACK(__FUNCTION_NAME);
	std::string message;
	{
		/*
			Clear references to playing sounds
		*/
		for(std::map<s32, ServerPlayingSound>::iterator
				i = m_playing_sounds.begin();
				i != m_playing_sounds.end();)
		{
			ServerPlayingSound &psound = i->second;
			psound.clients.erase(peer_id);
			if(psound.clients.empty())
				m_playing_sounds.erase(i++);
			else
				i++;
		}

		Player *player = m_env->getPlayer(peer_id);

		// Collect information about leaving in chat
		{
			if(player != NULL && reason != CDR_DENY) {
				std::string name = player->getName();
				message += "*** ";
				message += name;
				message += " left the game.";
				if(reason == CDR_TIMEOUT)
					message += " (timed out)";
			}
		}

		/* Run scripts and remove from environment */
		{
			if(player != NULL)
			{
				PlayerSAO *playersao = player->getPlayerSAO();
				assert(playersao);

				//JMutexAutoLock env_lock(m_env_mutex);
				m_script->on_leaveplayer(playersao);

				playersao->disconnected();
			}
		}

		/*
			Print out action
		*/
		{
			if(player != NULL && reason != CDR_DENY) {
				std::ostringstream os(std::ios_base::binary);
				std::vector<u16> clients = m_clients.getClientIDs();

				for(auto
					i = clients.begin();
					i != clients.end(); ++i)
				{
					// Get player
					Player *player = m_env->getPlayer(*i);
					if(!player)
						continue;
					// Get name of player
					os<<player->getName()<<" ";
				}

				actionstream<<player->getName()<<" "
						<<(reason==CDR_TIMEOUT?"times out.":"leaves game.")
						<<" List of players: "<<os.str()<<std::endl;
			}
		}
		{
			//JMutexAutoLock env_lock(m_env_mutex);
			m_clients.DeleteClient(peer_id);
		}
	}

	// Send leave chat message to all remaining clients
	if(message.length() != 0)
		SendChatMessage(PEER_ID_INEXISTENT,message);
}

void Server::UpdateCrafting(Player* player)
{
	DSTACK(__FUNCTION_NAME);

	// Get a preview for crafting
	ItemStack preview;
	InventoryLocation loc;
	loc.setPlayer(player->getName());
	std::vector<ItemStack> output_replacements;
	getCraftingResult(&player->inventory, preview, output_replacements, false, this);
	m_env->getScriptIface()->item_CraftPredict(preview, player->getPlayerSAO(), (&player->inventory)->getList("craft"), loc);

	// Put the new preview in
	InventoryList *plist = player->inventory.getList("craftpreview");
	assert(plist);
	assert(plist->getSize() >= 1);
	plist->changeItem(0, preview);
}

RemoteClient* Server::getClient(u16 peer_id, ClientState state_min)
{
	RemoteClient *client = getClientNoEx(peer_id,state_min);
	if(!client)
		throw ClientNotFoundException("Client not found");

	return client;
}
RemoteClient* Server::getClientNoEx(u16 peer_id, ClientState state_min)
{
	return m_clients.getClientNoEx(peer_id, state_min);
}

std::string Server::getPlayerName(u16 peer_id)
{
	Player *player = m_env->getPlayer(peer_id);
	if(player == NULL)
		return "[id="+itos(peer_id)+"]";
	return player->getName();
}

PlayerSAO* Server::getPlayerSAO(u16 peer_id)
{
	Player *player = m_env->getPlayer(peer_id);
	if(player == NULL)
		return NULL;
	return player->getPlayerSAO();
}

std::string Server::getStatusString()
{
	std::ostringstream os(std::ios_base::binary);
	os<<"# Server: ";
	// Version
	os<<"version="<<(g_version_string);
	// Uptime
	os<<", uptime="<<m_uptime.get();
	// Max lag estimate
	os<<", max_lag="<<m_env->getMaxLagEstimate();
	// Information about clients
	bool first = true;
	os<<", clients={";
	std::vector<u16> clients = m_clients.getClientIDs();
	for(auto i = clients.begin();
		i != clients.end(); ++i)
	{
		// Get player
		Player *player = m_env->getPlayer(*i);
		// Get name of player
		std::string name = "unknown";
		if(player != NULL)
			name = player->getName();
		// Add name to information string
		if(!first)
			os<<", ";
		else
			first = false;
		os<<name;
	}
	os<<"}";
	if(((ServerMap*)(&m_env->getMap()))->isSavingEnabled() == false)
		os<<std::endl<<"# Server: "<<" WARNING: Map saving is disabled.";
	if(g_settings->get("motd") != "")
		os<<std::endl<<"# Server: "<<g_settings->get("motd");
	return os.str();
}

std::set<std::string> Server::getPlayerEffectivePrivs(const std::string &name)
{
	std::set<std::string> privs;
	m_script->getAuth(name, NULL, &privs);
	return privs;
}

bool Server::checkPriv(const std::string &name, const std::string &priv)
{
	std::set<std::string> privs = getPlayerEffectivePrivs(name);
	return (privs.count(priv) != 0);
}

void Server::reportPrivsModified(const std::string &name)
{
	if(name == ""){
		std::vector<u16> clients = m_clients.getClientIDs();
		for(auto
				i = clients.begin();
				i != clients.end(); ++i){
			Player *player = m_env->getPlayer(*i);
			reportPrivsModified(player->getName());
		}
	} else {
		Player *player = m_env->getPlayer(name.c_str());
		if(!player)
			return;
		SendPlayerPrivileges(player->peer_id);
		PlayerSAO *sao = player->getPlayerSAO();
		if(!sao)
			return;
		sao->updatePrivileges(
				getPlayerEffectivePrivs(name),
				isSingleplayer());
	}
}

void Server::reportInventoryFormspecModified(const std::string &name)
{
	Player *player = m_env->getPlayer(name.c_str());
	if(!player)
		return;
	SendPlayerInventoryFormspec(player->peer_id);
}

void Server::setIpBanned(const std::string &ip, const std::string &name)
{
	m_banmanager->add(ip, name);
}

void Server::unsetIpBanned(const std::string &ip_or_name)
{
	m_banmanager->remove(ip_or_name);
}

std::string Server::getBanDescription(const std::string &ip_or_name)
{
	return m_banmanager->getBanDescription(ip_or_name);
}

void Server::notifyPlayer(const char *name, const std::string &msg)
{
	// m_env will be NULL if the server is initializing
	if (!m_env)
		return;

	Player *player = m_env->getPlayer(name);
	if (!player)
		return;

	if (player->peer_id == PEER_ID_INEXISTENT)
		return;

	SendChatMessage(player->peer_id, std::string("\vffffff") + msg);
}

bool Server::showFormspec(const char *playername, const std::string &formspec,
	const std::string &formname)
{
	// m_env will be NULL if the server is initializing
	if (!m_env)
		return false;

	Player *player = m_env->getPlayer(playername);
	if (!player)
		return false;

	SendShowFormspecMessage(player->peer_id, formspec, formname);
	return true;
}

u32 Server::hudAdd(Player *player, HudElement *form)
{
	if (!player)
		return -1;

	u32 id = player->addHud(form);

	SendHUDAdd(player->peer_id, id, form);

	return id;
}

bool Server::hudRemove(Player *player, u32 id) {
	if (!player)
		return false;

	HudElement* todel = player->removeHud(id);

	if (!todel)
		return false;

	delete todel;

	SendHUDRemove(player->peer_id, id);
	return true;
}

bool Server::hudChange(Player *player, u32 id, HudElementStat stat, void *data)
{
	if (!player)
		return false;

	SendHUDChange(player->peer_id, id, stat, data);
	return true;
}

bool Server::hudSetFlags(Player *player, u32 flags, u32 mask)
{
	if (!player)
		return false;

	SendHUDSetFlags(player->peer_id, flags, mask);
	player->hud_flags = flags;

	PlayerSAO* playersao = player->getPlayerSAO();

	if (playersao == NULL)
		return false;

	m_script->player_event(playersao, "hud_changed");
	return true;
}

bool Server::hudSetHotbarItemcount(Player *player, s32 hotbar_itemcount)
{
	if (!player)
		return false;
	if (hotbar_itemcount <= 0 || hotbar_itemcount > HUD_HOTBAR_ITEMCOUNT_MAX)
		return false;

	player->setHotbarItemcount(hotbar_itemcount);
	std::ostringstream os(std::ios::binary);
	writeS32(os, hotbar_itemcount);
	SendHUDSetParam(player->peer_id, HUD_PARAM_HOTBAR_ITEMCOUNT, os.str());
	return true;
}

s32 Server::hudGetHotbarItemcount(Player *player)
{
	if (!player)
		return 0;
	return player->getHotbarItemcount();
}

void Server::hudSetHotbarImage(Player *player, std::string name)
{
	if (!player)
		return;

	player->setHotbarImage(name);
	SendHUDSetParam(player->peer_id, HUD_PARAM_HOTBAR_IMAGE, name);
}

std::string Server::hudGetHotbarImage(Player *player)
{
	if (!player)
		return "";
	return player->getHotbarImage();
}

void Server::hudSetHotbarSelectedImage(Player *player, std::string name)
{
	if (!player)
		return;

	player->setHotbarSelectedImage(name);
	SendHUDSetParam(player->peer_id, HUD_PARAM_HOTBAR_SELECTED_IMAGE, name);
}

std::string Server::hudGetHotbarSelectedImage(Player *player)
{
	if (!player)
		return "";

	return player->getHotbarSelectedImage();
}

bool Server::setLocalPlayerAnimations(Player *player,
	v2s32 animation_frames[4], f32 frame_speed)
{
	if (!player)
		return false;

	player->setLocalAnimations(animation_frames, frame_speed);
	SendLocalPlayerAnimations(player->peer_id, animation_frames, frame_speed);
	return true;
}

bool Server::setPlayerEyeOffset(Player *player, v3f first, v3f third)
{
	if (!player)
		return false;

	player->eye_offset_first = first;
	player->eye_offset_third = third;
	SendEyeOffset(player->peer_id, first, third);
	return true;
}

bool Server::setSky(Player *player, const video::SColor &bgcolor,
	const std::string &type, const std::vector<std::string> &params)
{
	if (!player)
		return false;

	player->setSky(bgcolor, type, params);
	SendSetSky(player->peer_id, bgcolor, type, params);
	return true;
}

bool Server::overrideDayNightRatio(Player *player, bool do_override,
	float ratio)
{
	if (!player)
		return false;

	player->overrideDayNightRatio(do_override, ratio);
	SendOverrideDayNightRatio(player->peer_id, do_override, ratio);
	return true;
}

void Server::notifyPlayers(const std::string &msg)
{
	SendChatMessage(PEER_ID_INEXISTENT,msg);
}

void Server::spawnParticle(const std::string &playername, v3f pos,
	v3f velocity, v3f acceleration,
	float expirationtime, float size, bool
	collisiondetection, bool vertical, const std::string &texture)
{
	// m_env will be NULL if the server is initializing
	if (!m_env)
		return;

	u16 peer_id = PEER_ID_INEXISTENT;
	if (playername != "") {
		Player* player = m_env->getPlayer(playername.c_str());
		if (!player)
			return;
		peer_id = player->peer_id;
	}

	SendSpawnParticle(peer_id, pos, velocity, acceleration,
			expirationtime, size, collisiondetection, vertical, texture);
}

u32 Server::addParticleSpawner(u16 amount, float spawntime,
	v3f minpos, v3f maxpos, v3f minvel, v3f maxvel, v3f minacc, v3f maxacc,
	float minexptime, float maxexptime, float minsize, float maxsize,
	bool collisiondetection, bool vertical, const std::string &texture,
	const std::string &playername)
{
	// m_env will be NULL if the server is initializing
	if (!m_env)
		return -1;

	u16 peer_id = PEER_ID_INEXISTENT;
	if (playername != "") {
		Player* player = m_env->getPlayer(playername.c_str());
		if (!player)
			return -1;
		peer_id = player->peer_id;
	}

	u32 id = 0;
	for(;;) // look for unused particlespawner id
	{
		id++;
		if (std::find(m_particlespawner_ids.begin(),
				m_particlespawner_ids.end(), id)
				== m_particlespawner_ids.end())
		{
			m_particlespawner_ids.push_back(id);
			break;
		}
	}

	SendAddParticleSpawner(peer_id, amount, spawntime,
		minpos, maxpos, minvel, maxvel, minacc, maxacc,
		minexptime, maxexptime, minsize, maxsize,
		collisiondetection, vertical, texture, id);

	return id;
}

void Server::deleteParticleSpawner(const std::string &playername, u32 id)
{
	// m_env will be NULL if the server is initializing
	if (!m_env)
		throw ServerError("Can't delete particle spawners during initialisation!");

	u16 peer_id = PEER_ID_INEXISTENT;
	if (playername != "") {
		Player* player = m_env->getPlayer(playername.c_str());
		if (!player)
			return;
		peer_id = player->peer_id;
	}

	m_particlespawner_ids.erase(
			std::remove(m_particlespawner_ids.begin(),
			m_particlespawner_ids.end(), id),
			m_particlespawner_ids.end());
	SendDeleteParticleSpawner(peer_id, id);
}

Inventory* Server::createDetachedInventory(const std::string &name)
{
	if(m_detached_inventories.count(name) > 0){
		infostream<<"Server clearing detached inventory \""<<name<<"\""<<std::endl;
		delete m_detached_inventories[name];
	} else {
		infostream<<"Server creating detached inventory \""<<name<<"\""<<std::endl;
	}
	Inventory *inv = new Inventory(m_itemdef);
	assert(inv);
	m_detached_inventories[name] = inv;
	//TODO find a better way to do this
	sendDetachedInventory(name,PEER_ID_INEXISTENT);
	return inv;
}

// actions: time-reversed list
// Return value: success/failure
bool Server::rollbackRevertActions(const std::list<RollbackAction> &actions,
		std::list<std::string> *log)
{
	infostream<<"Server::rollbackRevertActions(len="<<actions.size()<<")"<<std::endl;
	ServerMap *map = (ServerMap*)(&m_env->getMap());

	// Fail if no actions to handle
	if(actions.empty()){
		log->push_back("Nothing to do.");
		return false;
	}

	int num_tried = 0;
	int num_failed = 0;

	for(std::list<RollbackAction>::const_iterator
			i = actions.begin();
			i != actions.end(); i++)
	{
		const RollbackAction &action = *i;
		num_tried++;
		bool success = action.applyRevert(map, this, this);
		if(!success){
			num_failed++;
			std::ostringstream os;
			os<<"Revert of step ("<<num_tried<<") "<<action.toString()<<" failed";
			infostream<<"Map::rollbackRevertActions(): "<<os.str()<<std::endl;
			if(log)
				log->push_back(os.str());
		}else{
			std::ostringstream os;
			os<<"Successfully reverted step ("<<num_tried<<") "<<action.toString();
			infostream<<"Map::rollbackRevertActions(): "<<os.str()<<std::endl;
			if(log)
				log->push_back(os.str());
		}
	}

	infostream<<"Map::rollbackRevertActions(): "<<num_failed<<"/"<<num_tried
			<<" failed"<<std::endl;

	// Call it done if less than half failed
	return num_failed <= num_tried/2;
}

// IGameDef interface
// Under envlock
IItemDefManager *Server::getItemDefManager()
{
	return m_itemdef;
}

INodeDefManager *Server::getNodeDefManager()
{
	return m_nodedef;
}

ICraftDefManager *Server::getCraftDefManager()
{
	return m_craftdef;
}
ITextureSource *Server::getTextureSource()
{
	return NULL;
}
IShaderSource *Server::getShaderSource()
{
	return NULL;
}
scene::ISceneManager *Server::getSceneManager()
{
	return NULL;
}

u16 Server::allocateUnknownNodeId(const std::string &name)
{
	return m_nodedef->allocateDummy(name);
}

ISoundManager *Server::getSoundManager()
{
	return &dummySoundManager;
}

MtEventManager *Server::getEventManager()
{
	return m_event;
}

IWritableItemDefManager *Server::getWritableItemDefManager()
{
	return m_itemdef;
}

IWritableNodeDefManager *Server::getWritableNodeDefManager()
{
	return m_nodedef;
}

IWritableCraftDefManager *Server::getWritableCraftDefManager()
{
	return m_craftdef;
}

const ModSpec *Server::getModSpec(const std::string &modname) const
{
	std::vector<ModSpec>::const_iterator it;
	for (it = m_mods.begin(); it != m_mods.end(); ++it) {
		const ModSpec &mod = *it;
		if (mod.name == modname)
			return &mod;
	}
	return NULL;
}

void Server::getModNames(std::vector<std::string> &modlist)
{
	std::vector<ModSpec>::iterator it;
	for (it = m_mods.begin(); it != m_mods.end(); ++it)
		modlist.push_back(it->name);
}

std::string Server::getBuiltinLuaPath()
{
	return porting::path_share + DIR_DELIM + "builtin";
}

v3f Server::findSpawnPos()
{
	ServerMap &map = m_env->getServerMap();
	v3f nodeposf;
	if (g_settings->getV3FNoEx("static_spawnpoint", nodeposf)) {
		return nodeposf * BS;
	}

	// Default position is static_spawnpoint
	// We will return it if we don't found a good place
	v3s16 nodepos(nodeposf.X, nodeposf.Y, nodeposf.Z);

	s16 water_level = map.getWaterLevel();

	bool is_good = false;

	// Try to find a good place a few times
	for(s32 i = 0; i < 1000 && !is_good; i++) {
		s32 range = 1 + i;
		// We're going to try to throw the player to this position
		v2s16 nodepos2d = v2s16(
				-range + (myrand() % (range * 2)),
				-range + (myrand() % (range * 2)));

		// Get ground height at point
		s16 groundheight = map.findGroundLevel(nodepos2d, g_settings->getBool("cache_block_before_spawn"));
		if (groundheight <= water_level) // Don't go underwater
			continue;
		if (groundheight > water_level + g_settings->getS16("max_spawn_height")) // Don't go to high places
			continue;

		nodepos = v3s16(nodepos2d.X, groundheight, nodepos2d.Y);

		s32 air_count = 0;
		for (s32 i = 0; i < 10; i++) {
			v3s16 blockpos = getNodeBlockPos(nodepos);
			map.emergeBlock(blockpos, false);
			content_t c = map.getNodeNoEx(nodepos).getContent();
			if (c == CONTENT_AIR || c == CONTENT_IGNORE) {
				air_count++;
				if (air_count >= 2){
					is_good = true;
					break;
				}
			}
			nodepos.Y++;
		}
	}

	return intToFloat(nodepos, BS);
}

PlayerSAO* Server::emergePlayer(const char *name, u16 peer_id, u16 proto_version)
{
	RemotePlayer *player = NULL;
	bool newplayer = false;

	/*
		Try to get an existing player
	*/
	player = static_cast<RemotePlayer*>(m_env->getPlayer(name));

	// If player is already connected, cancel
	if(player != NULL && player->peer_id != 0)
	{
		infostream<<"emergePlayer(): Player already connected"<<std::endl;
		return NULL;
	}

	/*
		If player with the wanted peer_id already exists, cancel.
	*/
	if(m_env->getPlayer(peer_id) != NULL)
	{
		infostream<<"emergePlayer(): Player with wrong name but same"
				" peer_id already exists"<<std::endl;
		return NULL;
	}

	if (!player && maintenance_status) {
		infostream<<"emergePlayer(): Maintenance in progress, disallowing loading player"<<std::endl;
		return nullptr;
	}

	// Load player if it isn't already loaded
	if (!player) {
		player = static_cast<RemotePlayer*>(m_env->loadPlayer(name));
	}

	// Create player if it doesn't exist
	if (!player) {
		newplayer = true;
		player = new RemotePlayer(this, name);
		// Set player position
		infostream<<"Server: Finding spawn place for player \""
				<<name<<"\""<<std::endl;
		v3f pos = findSpawnPos();
		player->setPosition(pos);

		// Add player to environment
		m_env->addPlayer(player);
	}

	// Create a new player active object
	PlayerSAO *playersao = new PlayerSAO(m_env, player, peer_id,
			getPlayerEffectivePrivs(player->getName()),
			isSingleplayer());

	player->protocol_version = proto_version;

	/* Clean up old HUD elements from previous sessions */
	player->clearHud();

	/* Add object to environment */
	m_env->addActiveObject(playersao);

	/* Run scripts */
	if (newplayer) {
		m_script->on_newplayer(playersao);
	}

	return playersao;
}

void dedicated_server_loop(Server &server, bool &kill)
{
	DSTACK(__FUNCTION_NAME);

	IntervalLimiter m_profiler_interval;

	int errors = 0;
	double run_time = 0;
	float steplen = g_settings->getFloat("dedicated_server_step");
	for(;;)
	{
		// This is kind of a hack but can be done like this
		// because server.step() is very light
		{
/*
			ScopeProfiler sp(g_profiler, "dedicated server sleep");
*/
			sleep_ms((int)(steplen*1000.0));
		}
		try {
		server.step(steplen);
		}
		//TODO: more errors here
		catch(std::exception &e) {
			if (!errors++ || !(errors % (int)(60/steplen)))
				errorstream<<"Fatal error n="<<errors<< " : "<<e.what()<<std::endl;
		}
		catch (...){
			if (!errors++ || !(errors % (int)(60/steplen)))
				errorstream<<"Fatal error unknown "<<errors<<std::endl;
		}
		if(server.getShutdownRequested() || kill)
		{
			infostream<<"Dedicated server quitting"<<std::endl;
			break;
		}

		run_time += steplen; // wrong not real time
		if (server.m_autoexit && run_time > server.m_autoexit) {
			actionstream << "Profiler:" << std::fixed << std::setprecision(9) << std::endl;
			g_profiler->print(actionstream);
			server.requestShutdown();
		}

		/*
			Profiler
		*/
		float profiler_print_interval =
				g_settings->getFloat("profiler_print_interval");
		if(server.m_clients.getClientList().size() && profiler_print_interval != 0)
		{
			if(m_profiler_interval.step(steplen, profiler_print_interval))
			{
				infostream<<"Profiler:"<<std::endl;
				g_profiler->print(infostream);
				g_profiler->clear();
			}
		}
	}
}
