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
#include "clientserver.h"
#include "ban.h"
#include "environment.h"
#include "map.h"
#include "jthread/jmutexautolock.h"
#include "main.h"
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
#include "log.h"
#include "scripting_game.h"
#include "nodedef.h"
#include "itemdef.h"
#include "craftdef.h"
#include "emerge.h"
#include "mapgen.h"
#include "biome.h"
#include "content_mapnode.h"
#include "content_nodemeta.h"
#include "content_abm.h"
#include "content_sao.h"
#include "mods.h"
#include "sha1.h"
#include "base64.h"
#include "tool.h"
#include "sound.h" // dummySoundManager
#include "event_manager.h"
#include "hex.h"
#include "serverlist.h"
#include "util/string.h"
#include "util/pointedthing.h"
#include "util/mathconstants.h"
#include "rollback.h"
#include "util/serialize.h"
#include "util/thread.h"
#include "defaultsettings.h"
#include "circuit.h"

#include <msgpack.hpp>
#include <chrono>
#include "util/thread_pool.h"

class ClientNotFoundException : public BaseException
{
public:
	ClientNotFoundException(const char *s):
		BaseException(s)
	{}
};

class MapThread : public thread_pool
{
	Server *m_server;
public:

	MapThread(Server *server):
		m_server(server)
	{}

	void * Thread() {
		log_register_thread("MapThread");

		DSTACK(__FUNCTION_NAME);
		BEGIN_DEBUG_EXCEPTION_HANDLER
		ThreadStarted();

		porting::setThreadName("Map");
		porting::setThreadPriority(20);
		while(!StopRequested()) {
			try {
				if (!m_server->AsyncRunMapStep())
					std::this_thread::sleep_for(std::chrono::milliseconds(100));
				else
					std::this_thread::sleep_for(std::chrono::milliseconds(10));
#ifdef NDEBUG
			} catch (BaseException &e) {
				errorstream<<"MapThread: exception: "<<e.what()<<std::endl;
			} catch(std::exception &e) {
				errorstream<<"MapThread: exception: "<<e.what()<<std::endl;
			} catch (...) {
				errorstream<<"MapThread: Ooops..."<<std::endl;
#else
			} catch (int) { //nothing
#endif
			}
		}
		END_DEBUG_EXCEPTION_HANDLER(errorstream)
		return nullptr;
	}
};

class SendBlocksThread : public thread_pool
{
	Server *m_server;
public:

	SendBlocksThread(Server *server):
		m_server(server)
	{}

	void * Thread() {
		log_register_thread("SendBlocksThread");

		DSTACK(__FUNCTION_NAME);
		BEGIN_DEBUG_EXCEPTION_HANDLER

		ThreadStarted();

		porting::setThreadName("SendBlocksThread");
		porting::setThreadPriority(50);
		auto time = porting::getTimeMs();
		while(!StopRequested()) {
			//infostream<<"S run d="<<m_server->m_step_dtime<< " myt="<<(porting::getTimeMs() - time)/1000.0f<<std::endl;
			try {
				int sent = m_server->SendBlocks((porting::getTimeMs() - time)/1000.0f);
				time = porting::getTimeMs();
				std::this_thread::sleep_for(std::chrono::milliseconds(sent ? 5 : 100));
#ifdef NDEBUG
			} catch (BaseException &e) {
				errorstream<<"SendBlocksThread: exception: "<<e.what()<<std::endl;
			} catch(std::exception &e) {
				errorstream<<"SendBlocksThread: exception: "<<e.what()<<std::endl;
			} catch (...) {
				errorstream<<"SendBlocksThread: Ooops..."<<std::endl;
#else
			} catch (int) { //nothing
#endif
			}
		}
		END_DEBUG_EXCEPTION_HANDLER(errorstream)
	return nullptr;
	}
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

void * ServerThread::Thread()
{
	log_register_thread("ServerThread");

	DSTACK(__FUNCTION_NAME);
	BEGIN_DEBUG_EXCEPTION_HANDLER

	f32 dedicated_server_step = g_settings->getFloat("dedicated_server_step");
	m_server->AsyncRunStep(true);

	ThreadStarted();

	porting::setThreadName("ServerThread");
	porting::setThreadPriority(10);

	while(!StopRequested())
	{
		try{
			//TimeTaker timer("AsyncRunStep() + Receive()");

			m_server->AsyncRunStep();

			// Loop used only when 100% cpu load or on old slow hardware.
			// usually only one packet recieved here
			u32 end_ms = porting::getTimeMs() + u32(1000 * dedicated_server_step);
			for (u16 i = 0; i < 1000; ++i)
				if (!m_server->Receive() || porting::getTimeMs() > end_ms)
					break;
		}
		catch(con::NoIncomingDataException &e)
		{
		}
		catch(con::PeerNotFoundException &e)
		{
			infostream<<"Server: PeerNotFoundException"<<std::endl;
		}
		catch(ClientNotFoundException &e)
		{
		}
		catch(con::ConnectionBindFailed &e)
		{
			m_server->setAsyncFatalError(e.what());
		}
		catch(LuaError &e)
		{
			m_server->setAsyncFatalError(e.what());
#ifdef NDEBUG
		} catch(std::exception &e) {
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
	m_rollback_sink_enabled(true),
	m_enable_rollback_recording(false),
	m_emerge(NULL),
	m_script(NULL),
	m_circuit(NULL),
	m_itemdef(createItemDefManager()),
	m_nodedef(createNodeDefManager()),
	m_craftdef(createCraftDefManager()),
	m_event(new EventManager()),
	m_thread(NULL),
	m_map_thread(nullptr),
	m_sendblocks(nullptr),
	m_time_of_day_send_timer(0),
	m_uptime(0),
	m_clients(&m_con),
	m_shutdown_requested(false),
	m_ignore_map_edit_events(false),
	m_ignore_map_edit_events_peer_id(0)

{
	m_liquid_transform_timer = 0.0;
	m_liquid_transform_interval = 1.0;
	m_liquid_send_timer = 0.0;
	m_liquid_send_interval = 1.0;
	m_print_info_timer = 0.0;
	m_masterserver_timer = 0.0;
	m_objectdata_timer = 0.0;
	m_emergethread_trigger_timer = 5.0; // to start emerge threads instantly
	m_savemap_timer = 0.0;

	m_step_dtime = 0.0;
	m_lag = g_settings->getFloat("dedicated_server_step");
	more_threads = g_settings->getBool("more_threads");

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

	if (more_threads) {
		m_map_thread = new MapThread(this);
		m_sendblocks = new SendBlocksThread(this);
	}

	// Create world if it doesn't exist
	if(!initializeWorld(m_path_world, m_gamespec.id))
		throw ServerError("Failed to initialize world");

	// Create ban manager
	std::string ban_path = m_path_world+DIR_DELIM+"ipban.txt";
	m_banmanager = new BanManager(ban_path);

	// Create rollback manager
	std::string rollback_path = m_path_world+DIR_DELIM+"rollback.txt";
	m_rollback = createRollbackManager(rollback_path, this);

	ModConfiguration modconf(m_path_world);
	m_mods = modconf.getMods();
	std::vector<ModSpec> unsatisfied_mods = modconf.getUnsatisfiedMods();
	// complain about mods with unsatisfied dependencies
	if(!modconf.isConsistent())
	{
		for(std::vector<ModSpec>::iterator it = unsatisfied_mods.begin();
			it != unsatisfied_mods.end(); ++it)
		{
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
		it != names.end(); ++it)
	{
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
	if(!load_mod_names.empty())
	{
		errorstream << "The following mods could not be found:";
		for(std::set<std::string>::iterator it = load_mod_names.begin();
			it != load_mod_names.end(); ++it)
			errorstream << " \"" << (*it) << "\"";
		errorstream << std::endl;
	}

	// Lock environment
	JMutexAutoLock envlock(m_env_mutex);

	// Initialize scripting
	infostream<<"Server: Initializing Lua"<<std::endl;

	m_script = new GameScripting(this);
	
	m_circuit = new Circuit(m_script, path_world);

	std::string scriptpath = getBuiltinLuaPath() + DIR_DELIM "init.lua";

	if (!m_script->loadScript(scriptpath)) {
		throw ModError("Failed to load and run " + scriptpath);
	}


	// Print 'em
	infostream<<"Server: Loading mods: ";
	for(std::vector<ModSpec>::iterator i = m_mods.begin();
			i != m_mods.end(); i++){
		const ModSpec &mod = *i;
		infostream<<mod.name<<" ";
	}
	infostream<<std::endl;
	// Load and run "mod" scripts
	for(std::vector<ModSpec>::iterator i = m_mods.begin();
			i != m_mods.end(); i++){
		const ModSpec &mod = *i;
		std::string scriptpath = mod.path + DIR_DELIM + "init.lua";
		infostream<<"  ["<<padStringRight(mod.name, 12)<<"] [\""
				<<scriptpath<<"\"]"<<std::endl;
		bool success = m_script->loadMod(scriptpath, mod.name);
		if(!success){
			errorstream<<"Server: Failed to load and run "
					<<scriptpath<<std::endl;
			throw ModError("Failed to load and run "+scriptpath);
		}
	}

	// Read Textures and calculate sha1 sums
	fillMediaCache();

	// Apply item aliases in the node definition manager
	m_nodedef->updateAliases(m_itemdef);

	// Load the mapgen params from global settings now after any
	// initial overrides have been set by the mods
	m_emerge->loadMapgenParams();

	// Initialize Environment
	ServerMap *servermap = new ServerMap(path_world, this, m_emerge, m_circuit);
	m_env = new ServerEnvironment(servermap, m_script, m_circuit, this, m_path_world);
	m_emerge->env = m_env;

	m_clients.setEnv(m_env);

	// Run some callbacks after the MG params have been set up but before activation
	m_script->environment_OnMapgenInit(&m_emerge->params);

	// Initialize mapgens
	m_emerge->initMapgens();

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

	m_liquid_transform_interval = g_settings->getFloat("liquid_update");
	m_liquid_send_interval = g_settings->getFloat("liquid_send");
}

Server::~Server()
{
	infostream<<"Server destructing"<<std::endl;

	// Send shutdown message
	SendChatMessage(PEER_ID_INEXISTENT, "*** Server shutting down");

	{
		JMutexAutoLock envlock(m_env_mutex);

		// Execute script shutdown hooks
		m_script->on_shutdown();

		infostream<<"Server: Saving players"<<std::endl;
		m_env->saveLoadedPlayers();

		infostream<<"Server: Saving environment metadata"<<std::endl;
		m_env->saveMeta();
	}

	// Stop threads
	stop();
	delete m_thread;

	if (m_sendblocks)
		delete m_sendblocks;
	if (m_map_thread)
		delete m_map_thread;

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
	delete m_circuit;

	// Deinitialize scripting
	infostream<<"Server: Deinitializing scripting"<<std::endl;
	delete m_script;

	// Delete detached inventories
	for (std::map<std::string, Inventory*>::iterator
			i = m_detached_inventories.begin();
			i != m_detached_inventories.end(); i++) {
		delete i->second;
	}
}

void Server::start(Address bind_addr)
{
	DSTACK(__FUNCTION_NAME);
	infostream<<"Starting server on "
			<< bind_addr.serializeString() <<"..."<<std::endl;

	// Stop thread if already running
	m_thread->Stop();
	if (m_sendblocks)
		m_sendblocks->Stop();
	if (m_map_thread)
		m_map_thread->Stop();
	
	// Initialize connection
	m_con.Serve(bind_addr.getPort());

	// Start thread
	m_thread->Start();
	if (m_map_thread)
		m_map_thread->Start();
	if (m_sendblocks)
		m_sendblocks->Start();

	actionstream << "\033[1mfree\033[1;33mminer \033[1;36mv" << minetest_version_hash << "\033[0m \t"
#ifndef NDEBUG
			<< " DEBUG \t"
#endif
			<< " cpp="<<__cplusplus<<" \t"
			<< " cores="<< porting::getNumberOfProcessors()
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
	m_thread->Stop();
	//m_emergethread.setRun(false);
	m_thread->Wait();
	//m_emergethread.stop();
	if (m_sendblocks) {
		m_sendblocks->Stop();
		m_sendblocks->Wait();
	}
	if (m_map_thread) {
		m_map_thread->Stop();
		m_map_thread->Wait();
	}

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
	// Throw if fatal error occurred in thread
	std::string async_err = m_async_fatal_error.get();
	if(async_err != ""){
		throw ServerError(async_err);
	}
}

void Server::AsyncRunStep(bool initial_step)
{
	DSTACK(__FUNCTION_NAME);

	TimeTaker timer_step("Server step");
	g_profiler->add("Server::AsyncRunStep (num)", 1);

	float dtime;
	{
		JMutexAutoLock lock1(m_step_dtime_mutex);
		dtime = m_step_dtime;
	}

	if (!more_threads)
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

	{
		TimeTaker timer_step("Server step: SendBlocks");
		JMutexAutoLock lock1(m_step_dtime_mutex);
		m_step_dtime -= dtime;
	}

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
		JMutexAutoLock envlock(m_env_mutex);

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
		JMutexAutoLock lock(m_env_mutex);
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
		m_env->step(dtime, m_uptime.get(), max_cycle_ms);
	}

	/*
		Do background stuff
	*/

	/*
		Handle players
	*/
	{
		//TimeTaker timer_step("Server step: Handle players");
		JMutexAutoLock lock(m_env_mutex);

		std::list<u16> clientids = m_clients.getClientIDs();

		//ScopeProfiler sp(g_profiler, "Server: handle players");

		for(std::list<u16>::iterator
			i = clientids.begin();
			i != clientids.end(); ++i)
		{
			PlayerSAO *playersao = getPlayerSAO(*i);
			if(playersao == NULL)
				continue;

			/*
				Handle player HPs (die if hp=0)
			*/
			if(playersao->m_hp_not_sent && g_settings->getBool("enable_damage"))
			{
				if(playersao->getHP() == 0)
					DiePlayer(*i);
				else
					SendPlayerHP(*i);
			}

			/*
				Send player breath if changed
			*/
			if(playersao->m_breath_not_sent) {
				SendPlayerBreath(*i);
			}

			/*
				Send player inventories if necessary
			*/
			if(playersao->m_moved){
				SendMovePlayer(*i);
				playersao->m_moved = false;
			}
			if(playersao->m_inventory_not_sent){
				UpdateCrafting(*i);
				SendInventory(*i);
			}
		}
	}

	if (!more_threads)
		AsyncRunMapStep();

	m_clients.step(dtime);

	m_lag += (m_lag > dtime ? -1 : 1) * dtime/100;
#if USE_CURL
	// send masterserver announce
	{
		float &counter = m_masterserver_timer;
		if(!isSingleplayer() && (!counter || counter >= 300.0) &&
				g_settings->getBool("server_announce"))
		{
			ServerList::sendAnnounce(counter ? "update" : "start",
					m_clients.getPlayerNames(),
					m_uptime.get(),
					m_env->getGameTime(),
					m_lag,
					m_gamespec.id,
					m_mods);
			counter = 0.01;
		}
		counter += dtime;
	}
#endif

	/*
		Check added and deleted active objects
	*/
	{
		TimeTaker timer_step("Server step: Check added and deleted active objects");
		//infostream<<"Server: Checking added and deleted active objects"<<std::endl;
		JMutexAutoLock envlock(m_env_mutex);

		m_clients.Lock();
		auto & clients = m_clients.getClientList();
		ScopeProfiler sp(g_profiler, "Server: checking added and deleted objs");

		// Radius inside which objects are active
		s16 radius = g_settings->getS16("active_object_send_range_blocks");
		radius *= MAP_BLOCKSIZE;
		s16 radius_deactivate = radius*3;

		for(auto
			i = clients.begin();
			i != clients.end(); ++i)
		{
			RemoteClient *client = i->second;

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
			m_env->getRemovedActiveObjects(pos, radius_deactivate,
					client->m_known_objects, removed_objects);
			m_env->getAddedActiveObjects(pos, radius,
					client->m_known_objects, added_objects);

			// Ignore if nothing happened
			if(removed_objects.size() == 0 && added_objects.size() == 0)
			{
				//infostream<<"active objects: none changed"<<std::endl;
				continue;
			}

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
				client->m_known_objects.insert(id);

				if(obj)
					obj->m_known_by_count++;
			}

			MSGPACK_PACKET_INIT(TOCLIENT_ACTIVE_OBJECT_REMOVE_ADD, 2);
			PACK(TOCLIENT_ACTIVE_OBJECT_REMOVE_ADD_REMOVE, removed_objects);
			PACK(TOCLIENT_ACTIVE_OBJECT_REMOVE_ADD_ADD, added_objects_data);

			// Send as reliable
			m_clients.send(client->peer_id, 0, buffer, true);

/*
			verbosestream<<"Server: Sent object remove/add: "
					<<removed_objects.size()<<" removed, "
					<<added_objects.size()<<" added, "
					<<"packet size is "<<reply.getSize()<<std::endl;
*/
		}
		m_clients.Unlock();
	}

	/*
		Send object messages
	*/
	{
		TimeTaker timer_step("Server step: Send object messages");
		JMutexAutoLock envlock(m_env_mutex);
		ScopeProfiler sp(g_profiler, "Server: sending object messages");

		// Key = object id
		// Value = data sent by object
		std::map<u16, std::list<ActiveObjectMessage>* > buffered_messages;

		// Get active object messages from environment
		for(;;)
		{
			ActiveObjectMessage aom = m_env->getActiveObjectMessage();
			if(aom.id == 0)
				break;

			std::list<ActiveObjectMessage>* message_list = NULL;
			std::map<u16, std::list<ActiveObjectMessage>* >::iterator n;
			n = buffered_messages.find(aom.id);
			if(n == buffered_messages.end())
			{
				message_list = new std::list<ActiveObjectMessage>;
				buffered_messages[aom.id] = message_list;
			}
			else
			{
				message_list = n->second;
			}
			message_list->push_back(aom);
		}

		m_clients.Lock();
		auto & clients = m_clients.getClientList();
		// Route data to every client
		for(auto
			i = clients.begin();
			i != clients.end(); ++i)
		{
			RemoteClient *client = i->second;
			ActiveObjectMessages reliable_data;
			ActiveObjectMessages unreliable_data;
			// Go through all objects in message buffer
			for(std::map<u16, std::list<ActiveObjectMessage>* >::iterator
					j = buffered_messages.begin();
					j != buffered_messages.end(); ++j)
			{
				// If object is not known by client, skip it
				u16 id = j->first;
				if(client->m_known_objects.find(id) == client->m_known_objects.end())
					continue;
				// Get message list of object
				std::list<ActiveObjectMessage>* list = j->second;
				// Go through every message
				for(std::list<ActiveObjectMessage>::iterator
						k = list->begin(); k != list->end(); ++k)
				{
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
			if(reliable_data.size() > 0)
			{
				MSGPACK_PACKET_INIT(TOCLIENT_ACTIVE_OBJECT_MESSAGES, 1);
				PACK(TOCLIENT_ACTIVE_OBJECT_MESSAGES_MESSAGES, reliable_data);
				// Send as reliable
				m_clients.send(client->peer_id, 0, buffer, true);
			}
			if(unreliable_data.size() > 0)
			{
				MSGPACK_PACKET_INIT(TOCLIENT_ACTIVE_OBJECT_MESSAGES, 1);
				PACK(TOCLIENT_ACTIVE_OBJECT_MESSAGES_MESSAGES, unreliable_data);
				m_clients.send(client->peer_id, 1, buffer, false);
			}
		}
		m_clients.Unlock();

		// Clear buffered_messages
		for(std::map<u16, std::list<ActiveObjectMessage>* >::iterator
				i = buffered_messages.begin();
				i != buffered_messages.end(); ++i)
		{
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
		JMutexAutoLock lock(m_env_mutex);

		// Don't send too many at a time
		u32 count = 0;

		// Single change sending is disabled if queue size is not small
		bool disable_single_change_sending = false;
		if(m_unsent_map_edit_queue.size() >= 4)
			disable_single_change_sending = true;

		int event_count = m_unsent_map_edit_queue.size();

		// We'll log the amount of each
		Profiler prof;

		u32 end_ms = porting::getTimeMs() + max_cycle_ms;
		while(m_unsent_map_edit_queue.size() != 0)
		{
			MapEditEvent* event = m_unsent_map_edit_queue.pop_front();

			// Players far away from the change are stored here.
			// Instead of sending the changes, MapBlocks are set not sent
			// for them.
			std::list<u16> far_players;

			if(event->type == MEET_ADDNODE || event->type == MEET_SWAPNODE)
			{
				//infostream<<"Server: MEET_ADDNODE"<<std::endl;
				prof.add("MEET_ADDNODE", 1);
				if(disable_single_change_sending)
					sendAddNode(event->p, event->n, event->already_known_by_peer,
							&far_players, 5, event->type == MEET_ADDNODE);
				else
					sendAddNode(event->p, event->n, event->already_known_by_peer,
							&far_players, 30, event->type == MEET_ADDNODE);
			}
			else if(event->type == MEET_REMOVENODE)
			{
				//infostream<<"Server: MEET_REMOVENODE"<<std::endl;
				prof.add("MEET_REMOVENODE", 1);
				if(disable_single_change_sending)
					sendRemoveNode(event->p, event->already_known_by_peer,
							&far_players, 5);
				else
					sendRemoveNode(event->p, event->already_known_by_peer,
							&far_players, 30);
			}
			else if(event->type == MEET_BLOCK_NODE_METADATA_CHANGED)
			{
/*
				infostream<<"Server: MEET_BLOCK_NODE_METADATA_CHANGED"<<std::endl;
*/
				prof.add("MEET_BLOCK_NODE_METADATA_CHANGED", 1);
				setBlockNotSent(event->p);
			}
			else if(event->type == MEET_OTHER)
			{
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
			else
			{
				prof.add("unknown", 1);
				infostream<<"WARNING: Server: Unknown MapEditEvent "
						<<((u32)event->type)<<std::endl;
			}

			/*
				Set blocks not sent to far players
			*/
			if(far_players.size() > 0)
			{
				// Convert list format to that wanted by SetBlocksNotSent
				std::map<v3s16, MapBlock*> modified_blocks2;
				for(std::set<v3s16>::iterator
						i = event->modified_blocks.begin();
						i != event->modified_blocks.end(); ++i)
				{
					modified_blocks2[*i] =
							m_env->getMap().getBlockNoCreateNoEx(*i);
				}
				// Set blocks not sent
				for(std::list<u16>::iterator
						i = far_players.begin();
						i != far_players.end(); ++i)
				{
					u16 peer_id = *i;
					RemoteClient *client = getClient(peer_id);
					if(client==NULL)
						continue;
					client->SetBlocksNotSent(modified_blocks2);
				}
			}

			delete event;

			++count;
			/*// Don't send too many at a time
			if(count >= 1 && m_unsent_map_edit_queue.size() < 100)
				break;*/
			if (porting::getTimeMs() > end_ms)
				break;
		}

		if(event_count >= 10){
			infostream<<"Server: MapEditEvents count="<<count<<"/"<<event_count<<" :"<<std::endl;
			prof.print(infostream);
		} else if(event_count != 0){
			verbosestream<<"Server: MapEditEvents count="<<count<<"/"<<event_count<<" :"<<std::endl;
			prof.print(verbosestream);
		}

	}

	/*
		Trigger emergethread (it somehow gets to a non-triggered but
		bysy state sometimes)
	*/
	{
		TimeTaker timer_step("Server step: Trigger emergethread");
		float &counter = m_emergethread_trigger_timer;
		counter += dtime;
		if(counter >= 2.0)
		{
			counter = 0.0;

			m_emerge->startThreads();

			// Update m_enable_rollback_recording here too
			m_enable_rollback_recording =
					g_settings->getBool("enable_rollback_recording");
		}
	}
}

int Server::AsyncRunMapStep(bool initial_step) {
	DSTACK(__FUNCTION_NAME);

	TimeTaker timer_step("Server map step");
	g_profiler->add("Server::AsyncRunMapStep (num)", 1);

	int ret = 0;

	float dtime;
	{
		JMutexAutoLock lock1(m_step_dtime_mutex);
		dtime = m_step_dtime;
	}

	u32 max_cycle_ms = 500;

	const float map_timer_and_unload_dtime = 10.92;
	if(m_map_timer_and_unload_interval.step(dtime, map_timer_and_unload_dtime))
	{
		TimeTaker timer_step("Server step: Run Map's timers and unload unused data");
		//JMutexAutoLock lock(m_env_mutex);
		// Run Map's timers and unload unused data
		ScopeProfiler sp(g_profiler, "Server: map timer and unload");
		if(m_env->getMap().timerUpdate(m_uptime.get(), g_settings->getFloat("server_unload_unused_data_timeout"), max_cycle_ms)) {
			m_map_timer_and_unload_interval.run_next(map_timer_and_unload_dtime);
			++ret;
		}
	}

	/* Transform liquids */
	m_liquid_transform_timer += dtime;
	if(m_liquid_transform_timer >= m_liquid_transform_interval)
	{
		TimeTaker timer_step("Server step: liquid transform");
		m_liquid_transform_timer -= m_liquid_transform_interval;
		if (m_liquid_transform_timer > m_liquid_transform_interval * 2)
			m_liquid_transform_timer = 0;

		//JMutexAutoLock lock(m_env_mutex);

		ScopeProfiler sp(g_profiler, "Server: liquid transform");

		// not all liquid was processed per step, forcing on next step
		shared_map<v3s16, MapBlock*> modified_blocks; //not used
		if (m_env->getMap().transformLiquids(this, modified_blocks, m_lighting_modified_blocks, max_cycle_ms) > 0) {
			m_liquid_transform_timer = m_liquid_transform_interval /*  *0.8  */;
			++ret;
		}
	}

		/*
			Set the modified blocks unsent for all the clients
		*/

	m_liquid_send_timer += dtime;
	if(m_liquid_send_timer >= m_liquid_send_interval)
	{
		TimeTaker timer_step("Server step: set the modified blocks unsent for all the clients");
		m_liquid_send_timer -= m_liquid_send_interval;
		if (m_liquid_send_timer > m_liquid_send_interval * 2)
			m_liquid_send_timer = 0;

		shared_map<v3s16, MapBlock*> modified_blocks; //not used

		if (m_env->getMap().updateLighting(m_lighting_modified_blocks, modified_blocks, max_cycle_ms)) {
			m_liquid_send_timer = m_liquid_send_interval;
			++ret;
			goto no_send;
		}

		for (auto i = m_clients.getClientList().begin(); i != m_clients.getClientList().end(); ++i)
			if (i->second->m_nearest_unsent_nearest) {
				i->second->m_nearest_unsent_d = 0;
				i->second->m_nearest_unsent_nearest = 0;
			}

		//JMutexAutoLock lock(m_env_mutex);
		//JMutexAutoLock lock2(m_con_mutex);

/*
		if(m_modified_blocks.size() > 0)
		{
			SetBlocksNotSent(m_modified_blocks);
		}
		m_modified_blocks.clear();
*/
	}
	no_send:

	// Save map, players and auth stuff
	{
		float &counter = m_savemap_timer;
		counter += dtime;
		if(counter >= g_settings->getFloat("server_map_save_interval"))
		{
			counter = 0.0;
			TimeTaker timer_step("Server step: Save map, players and auth stuff");
			//JMutexAutoLock lock(m_env_mutex);

			ScopeProfiler sp(g_profiler, "Server: saving stuff");

			// Save ban file
			if (m_banmanager->isModified()) {
				m_banmanager->save();
			}


			// Save changed parts of map
			if(m_env->getMap().save(MOD_STATE_WRITE_NEEDED, 1)) {
				// partial save, will continue on next step
				counter = g_settings->getFloat("server_map_save_interval");
				++ret;
				goto save_break;
			}

			// Save players
			m_env->saveLoadedPlayers();

			// Save environment metadata
			m_env->saveMeta();
		}
		save_break:;
	}

	return ret;
}

u16 Server::Receive()
{
	DSTACK(__FUNCTION_NAME);
	SharedBuffer<u8> data;
	u16 peer_id;
	u32 datasize;
	u16 received = 0;
	try{
		datasize = m_con.Receive(peer_id,data);
		ProcessData(*data, datasize, peer_id);
		++received;
	}
	catch(con::InvalidIncomingDataException &e)
	{
		infostream<<"Server::Receive(): "
				"InvalidIncomingDataException: what()="
				<<e.what()<<std::endl;
	}
	catch(SerializationError &e) {
		infostream<<"Server::Receive(): "
				"SerializationError: what()="
				<<e.what()<<std::endl;
	}
	catch(ClientStateError &e)
	{
		errorstream << "ProcessData: peer=" << peer_id  << e.what() << std::endl;
		DenyAccess(peer_id, L"Your client sent something server didn't expect."
				L"Try reconnecting or updating your client");
	}
	catch(con::PeerNotFoundException &e)
	{
		// Do nothing
	}
	return received;
}

PlayerSAO* Server::StageTwoClientInit(u16 peer_id)
{
	std::string playername = "";
	PlayerSAO *playersao = NULL;
	m_clients.Lock();
	RemoteClient* client = m_clients.lockedGetClientNoEx(peer_id, CS_InitDone);
	if (client != NULL) {
		playername = client->getName();
		playersao = emergePlayer(playername.c_str(), peer_id);
	}
	m_clients.Unlock();

	RemotePlayer *player =
		static_cast<RemotePlayer*>(m_env->getPlayer(playername.c_str()));

	// If failed, cancel
	if((playersao == NULL) || (player == NULL))
	{
		if(player && player->peer_id != 0){
			errorstream<<"Server: "<<playername<<": Failed to emerge player"
					<<" (player allocated to an another client)"<<std::endl;
			DenyAccess(peer_id, L"Another client is connected with this "
					L"name. If your client closed unexpectedly, try again in "
					L"a minute.");
		} else {
			errorstream<<"Server: "<<playername<<": Failed to emerge player"
					<<std::endl;
			DenyAccess(peer_id, L"Could not allocate player.");
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
	UpdateCrafting(peer_id);
	SendInventory(peer_id);

	// Send HP
	if(g_settings->getBool("enable_damage"))
		SendPlayerHP(peer_id);

	// Send Breath
	SendPlayerBreath(peer_id);

	// Show death screen if necessary
	if(player->hp == 0)
		SendDeathscreen(peer_id, false, v3f(0,0,0));

	// Note things in chat if not in simple singleplayer mode
	if(!m_simple_singleplayer_mode)
	{
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
				i != names.end(); i++)
		{
			actionstream << *i << " ";
		}

		actionstream << player->getName() <<std::endl;
	}
	return playersao;
}

void Server::ProcessData(u8 *data, u32 datasize, u16 peer_id)
{
	DSTACK(__FUNCTION_NAME);
	// Environment is locked first.
	JMutexAutoLock envlock(m_env_mutex);

	ScopeProfiler sp(g_profiler, "Server::ProcessData");

	std::string addr_s;
	try{
		Address address = getPeerAddress(peer_id);
		addr_s = address.serializeString();

		// drop player if is ip is banned
		if(m_banmanager->isIpBanned(addr_s)){
			std::string ban_name = m_banmanager->getBanName(addr_s);
			infostream<<"Server: A banned client tried to connect from "
					<<addr_s<<"; banned name was "
					<<ban_name<<std::endl;
			// This actually doesn't seem to transfer to the client
			DenyAccess(peer_id, std::string("Your ip is banned. Banned name was ") + ban_name);
			return;
		}
	}
	catch(con::PeerNotFoundException &e)
	{
		/*
		 * no peer for this packet found
		 * most common reason is peer timeout, e.g. peer didn't
		 * respond for some time, your server was overloaded or
		 * things like that.
		 */
		verbosestream<<"Server::ProcessData(): Cancelling: peer "
				<<peer_id<<" not found"<<std::endl;
		return;
	}

	try
	{

	if(datasize < 2)
		return;

	int command;
	std::map<int, msgpack::object> packet;
	msgpack::unpacked msg;
	if (!con::parse_msgpack_packet(data, datasize, &packet, &command, &msg)) {
		return;
	}

	if(command == TOSERVER_INIT)
	{
		RemoteClient* client = getClient(peer_id, CS_Created);

		// If net_proto_version is set, this client has already been handled
		if(client->getState() > CS_Created)
		{
			verbosestream<<"Server: Ignoring multiple TOSERVER_INITs from "
					<<addr_s<<" (peer_id="<<peer_id<<")"<<std::endl;
			return;
		}

		verbosestream<<"Server: Got TOSERVER_INIT from "<<addr_s<<" (peer_id="
				<<peer_id<<")"<<std::endl;

		// Do not allow multiple players in simple singleplayer mode.
		// This isn't a perfect way to do it, but will suffice for now
		if(m_simple_singleplayer_mode && m_clients.getClientIDs().size() > 1){
			infostream<<"Server: Not allowing another client ("<<addr_s
					<<") to connect in simple singleplayer mode"<<std::endl;
			DenyAccess(peer_id, "Running in simple singleplayer mode.");
			return;
		}

		// First byte after command is maximum supported
		// serialization version
		u8 client_max;
		packet[TOSERVER_INIT_FMT].convert(&client_max);
		u8 our_max = SER_FMT_VER_HIGHEST_READ;
		// Use the highest version supported by both
		u8 deployed = std::min(client_max, our_max);
		// If it's lower than the lowest supported, give up.
		if(deployed < SER_FMT_VER_LOWEST)
			deployed = SER_FMT_VER_INVALID;

		if(deployed == SER_FMT_VER_INVALID)
		{
			actionstream<<"Server: A mismatched client tried to connect from "
					<<addr_s<<std::endl;
			infostream<<"Server: Cannot negotiate serialization version with "
					<<addr_s<<std::endl;
			DenyAccess(peer_id, std::string(
					"Your client's version is not supported.\n"
					"Server version is ")
					+ minetest_version_simple + "."
			);
			return;
		}

		client->setPendingSerializationVersion(deployed);

		/*
			Read and check network protocol version
		*/

		u16 min_net_proto_version = 0;
		packet[TOSERVER_INIT_PROTOCOL_VERSION_MIN].convert(&min_net_proto_version);
		u16 max_net_proto_version = min_net_proto_version;
		packet[TOSERVER_INIT_PROTOCOL_VERSION_MAX].convert(&max_net_proto_version);

		// Start with client's maximum version
		u16 net_proto_version = max_net_proto_version;

		// Figure out a working version if it is possible at all
		if(max_net_proto_version >= SERVER_PROTOCOL_VERSION_MIN ||
				min_net_proto_version <= SERVER_PROTOCOL_VERSION_MAX)
		{
			// If maximum is larger than our maximum, go with our maximum
			if(max_net_proto_version > SERVER_PROTOCOL_VERSION_MAX)
				net_proto_version = SERVER_PROTOCOL_VERSION_MAX;
			// Else go with client's maximum
			else
				net_proto_version = max_net_proto_version;
		}

		verbosestream<<"Server: "<<addr_s<<": Protocol version: min: "
				<<min_net_proto_version<<", max: "<<max_net_proto_version
				<<", chosen: "<<net_proto_version<<std::endl;

		client->net_proto_version = net_proto_version;

		if(net_proto_version < SERVER_PROTOCOL_VERSION_MIN ||
				net_proto_version > SERVER_PROTOCOL_VERSION_MAX)
		{
			actionstream<<"Server: A mismatched client tried to connect from "
					<<addr_s<<std::endl;
			DenyAccess(peer_id, std::string(
					"Your client's version is not supported.\n"
					"Server version is ")
					+ minetest_version_simple + ",\n"
					+ "server's PROTOCOL_VERSION is "
					+ itos(SERVER_PROTOCOL_VERSION_MIN)
					+ "..."
					+ itos(SERVER_PROTOCOL_VERSION_MAX)
					+ ", client's PROTOCOL_VERSION is "
					+ itos(min_net_proto_version)
					+ "..."
					+ itos(max_net_proto_version)
			);
			return;
		}

		if(g_settings->getBool("strict_protocol_version_checking"))
		{
			if(net_proto_version != LATEST_PROTOCOL_VERSION)
			{
				actionstream<<"Server: A mismatched (strict) client tried to "
						<<"connect from "<<addr_s<<std::endl;
				DenyAccess(peer_id, std::string(
						"Your client's version is not supported.\n"
						"Server version is ")
						+ minetest_version_simple + ",\n"
						+ "server's PROTOCOL_VERSION (strict) is "
						+ itos(LATEST_PROTOCOL_VERSION)
						+ ", client's PROTOCOL_VERSION is "
						+ itos(min_net_proto_version)
						+ "..."
						+ itos(max_net_proto_version)
				);
				return;
			}
		}

		/*
			Set up player
		*/

		// Get player name
		std::string playername;
		packet[TOSERVER_INIT_NAME].convert(&playername);

		if(playername.empty())
		{
			actionstream<<"Server: Player with an empty name "
					<<"tried to connect from "<<addr_s<<std::endl;
			DenyAccess(peer_id, "Empty name");
			return;
		}

		if(!g_settings->getBool("enable_any_name") && string_allowed(playername, PLAYERNAME_ALLOWED_CHARS)==false)
		{
			actionstream<<"Server: Player with an invalid name ["<<playername
					<<"] tried to connect from "<<addr_s<<std::endl;
			DenyAccess(peer_id, "Name contains unallowed characters");
			return;
		}

		if(!isSingleplayer() && playername == "singleplayer")
		{
			actionstream<<"Server: Player with the name \"singleplayer\" "
					<<"tried to connect from "<<addr_s<<std::endl;
			DenyAccess(peer_id, "Name is not allowed");
			return;
		}

		{
			std::string reason;
			if(m_script->on_prejoinplayer(playername, addr_s, reason))
			{
				actionstream<<"Server: Player with the name \""<<playername<<"\" "
						<<"tried to connect from "<<addr_s<<" "
						<<"but it was disallowed for the following reason: "
						<<reason<<std::endl;
				DenyAccess(peer_id, reason);
				return;
			}
		}

		infostream<<"Server: New connection: \""<<playername<<"\" from "
				<<addr_s<<" (peer_id="<<peer_id<<")"<<std::endl;

		// Get password
		std::string given_password;
		packet[TOSERVER_INIT_PASSWORD].convert(&given_password);

		if(!base64_is_valid(given_password.c_str())){
			actionstream<<"Server: "<<playername
					<<" supplied invalid password hash"<<std::endl;
			DenyAccess(peer_id, "Invalid password hash");
			return;
		}

		// Enforce user limit.
		// Don't enforce for users that have some admin right
		if(m_clients.getClientIDs(CS_Created).size() >= g_settings->getU16("max_users") &&
				!checkPriv(playername, "server") &&
				!checkPriv(playername, "ban") &&
				!checkPriv(playername, "privs") &&
				!checkPriv(playername, "password") &&
				playername != g_settings->get("name"))
		{
			actionstream<<"Server: "<<playername<<" tried to join, but there"
					<<" are already max_users="
					<<g_settings->getU16("max_users")<<" players."<<std::endl;
			DenyAccess(peer_id, "Too many users.");
			return;
		}

		std::string checkpwd; // Password hash to check against
		bool has_auth = m_script->getAuth(playername, &checkpwd, NULL);

		// If no authentication info exists for user, create it
		if(!has_auth){
			if(!isSingleplayer() &&
					g_settings->getBool("disallow_empty_password") &&
					given_password == ""){
				actionstream<<"Server: "<<playername
						<<" supplied empty password"<<std::endl;
				DenyAccess(peer_id, "Empty passwords are "
						"disallowed. Set a password and try again.");
				return;
			}
			std::string raw_default_password = g_settings->get("default_password");
			std::string initial_password =
				translatePassword(playername, raw_default_password);

			// If default_password is empty, allow any initial password
			if (raw_default_password.length() == 0)
				initial_password = given_password;

			m_script->createAuth(playername, initial_password);
		}

		has_auth = m_script->getAuth(playername, &checkpwd, NULL);

		if(!has_auth){
			actionstream<<"Server: "<<playername<<" cannot be authenticated"
					<<" (auth handler does not work?)"<<std::endl;
			DenyAccess(peer_id, "Not allowed to login");
			return;
		}

		if(given_password != checkpwd){
			actionstream<<"Server: "<<playername<<" supplied wrong password"
					<<std::endl;
			DenyAccess(peer_id, "Wrong password");
			return;
		}

		RemotePlayer *player =
				static_cast<RemotePlayer*>(m_env->getPlayer(playername.c_str()));

		if(player && player->peer_id != 0){
			errorstream<<"Server: "<<playername<<": Failed to emerge player"
					<<" (player allocated to an another client)"<<std::endl;
			DenyAccess(peer_id, "Another client is connected with this "
					"name. If your client closed unexpectedly, try again in "
					"a minute.");
		}

		m_clients.setPlayerName(peer_id,playername);

		/*
			Answer with a TOCLIENT_INIT
		*/
		{
			MSGPACK_PACKET_INIT(TOCLIENT_INIT, 3);
			PACK(TOCLIENT_INIT_DEPLOYED, deployed);
			PACK(TOCLIENT_INIT_SEED, m_env->getServerMap().getSeed());
			PACK(TOCLIENT_INIT_STEP, g_settings->getFloat("dedicated_server_step"));

			// Send as reliable
			m_clients.send(peer_id, 0, buffer, true);
			m_clients.event(peer_id, CSE_Init);
		}

		return;
	}

	if(command == TOSERVER_INIT2)
	{
		verbosestream<<"Server: Got TOSERVER_INIT2 from "
				<<peer_id<<std::endl;

		m_clients.event(peer_id, CSE_GotInit2);
		u16 protocol_version = m_clients.getProtocolVersion(peer_id);


		///// begin compatibility code
		PlayerSAO* playersao = NULL;
		if (protocol_version <= 22) {
			playersao = StageTwoClientInit(peer_id);

			if (playersao == NULL) {
				errorstream
					<< "TOSERVER_INIT2 stage 2 client init failed for peer "
					<< peer_id << std::endl;
				return;
			}
		}
		///// end compatibility code

		/*
			Send some initialization data
		*/

		infostream<<"Server: Sending content to "
				<<getPlayerName(peer_id)<<std::endl;

		// Send player movement settings
		SendMovement(peer_id);

		// Send item definitions
		SendItemDef(peer_id, m_itemdef, protocol_version);

		// Send node definitions
		SendNodeDef(peer_id, m_nodedef, protocol_version);

		m_clients.event(peer_id, CSE_SetDefinitionsSent);

		// Send media announcement
		sendMediaAnnouncement(peer_id);

		// Send detached inventories
		sendDetachedInventories(peer_id);

		// Send time of day
		u16 time = m_env->getTimeOfDay();
		float time_speed = g_settings->getFloat("time_speed");
		SendTimeOfDay(peer_id, time, time_speed);

		///// begin compatibility code
		if (protocol_version <= 22) {
			m_clients.event(peer_id, CSE_SetClientReady);
			m_script->on_joinplayer(playersao);
		}
		///// end compatibility code

		// Warnings about protocol version can be issued here
		if(getClient(peer_id)->net_proto_version < LATEST_PROTOCOL_VERSION)
		{
			SendChatMessage(peer_id, "# Server: WARNING: YOUR CLIENT'S "
					"VERSION MAY NOT BE FULLY COMPATIBLE WITH THIS SERVER!");
		}

		return;
	}

	u8 peer_ser_ver = getClient(peer_id, CS_InitDone)->serialization_version;
	u16 peer_proto_ver = getClient(peer_id, CS_InitDone)->net_proto_version;

	if(peer_ser_ver == SER_FMT_VER_INVALID)
	{
		errorstream<<"Server::ProcessData(): Cancelling: Peer"
				" serialization format invalid or not initialized."
				" Skipping incoming command="<<command<<std::endl;
		return;
	}

	/* Handle commands relate to client startup */
	if(command == TOSERVER_REQUEST_MEDIA) {
		std::list<std::string> tosend;
		packet[TOSERVER_REQUEST_MEDIA_FILES].convert(&tosend);

		sendRequestedMedia(peer_id, tosend);
		return;
	}
	else if(command == TOSERVER_RECEIVED_MEDIA) {
		return;
	}
	else if(command == TOSERVER_CLIENT_READY) {
		// clients <= protocol version 22 did not send ready message,
		// they're already initialized
		if (peer_proto_ver <= 22) {
			infostream << "Client sent message not expected by a "
				<< "client using protocol version <= 22,"
				<< "disconnecing peer_id: " << peer_id << std::endl;
			m_con.DisconnectPeer(peer_id);
			return;
		}

		PlayerSAO* playersao = StageTwoClientInit(peer_id);

		// If failed, cancel
		if (playersao == NULL) {
			errorstream
				<< "TOSERVER_CLIENT_READY stage 2 client init failed for peer_id: "
				<< peer_id << std::endl;
			m_con.DisconnectPeer(peer_id);
			return;
		}
		m_clients.setClientVersion(
			peer_id,
			packet[TOSERVER_CLIENT_READY_VERSION_MAJOR].as<int>(),
			packet[TOSERVER_CLIENT_READY_VERSION_MINOR].as<int>(),
			0, // packet[TOSERVER_CLIENT_READY_VERSION_PATCH].as<int>(), TODO
			packet[TOSERVER_CLIENT_READY_VERSION_STRING].as<std::string>()
		);
		m_clients.event(peer_id, CSE_SetClientReady);
		m_script->on_joinplayer(playersao);

	}
	else if(command == TOSERVER_GOTBLOCKS) // TODO: REMOVE IN NEXT, move wanted_range to new packet
	{
		RemoteClient *client = getClient(peer_id);
		packet[TOSERVER_GOTBLOCKS_RANGE].convert(&client->wanted_range);
		return;
	}

	if (m_clients.getClientState(peer_id) < CS_Active)
	{
		if (command == TOSERVER_PLAYERPOS) return;

		errorstream<<"Got packet command: " << command << " for peer id "
				<< peer_id << " but client isn't active yet. Dropping packet "
				<<std::endl;
		return;
	}

	Player *player = m_env->getPlayer(peer_id);
	if(player == NULL) {
/*
		verbosestream<<"Server::ProcessData(): Cancelling: "
				"No player for peer_id="<<peer_id
				<< " disconnecting peer!" <<std::endl;
*/
		m_con.DisconnectPeer(peer_id);
		return;
	}

	PlayerSAO *playersao = player->getPlayerSAO();
	if(playersao == NULL) {
		errorstream<<"Server::ProcessData(): Cancelling: "
				"No player object for peer_id="<<peer_id
				<< " disconnecting peer!" <<std::endl;
		m_con.DisconnectPeer(peer_id);
		return;
	}

	if(command == TOSERVER_PLAYERPOS)
	{
		player->setPosition(packet[TOSERVER_PLAYERPOS_POSITION].as<v3f>());
		player->setSpeed(packet[TOSERVER_PLAYERPOS_SPEED].as<v3f>());
		player->setPitch(wrapDegrees(packet[TOSERVER_PLAYERPOS_PITCH].as<f32>()));
		player->setYaw(wrapDegrees(packet[TOSERVER_PLAYERPOS_YAW].as<f32>()));
		u32 keyPressed = packet[TOSERVER_PLAYERPOS_KEY_PRESSED].as<u32>();
		player->keyPressed = keyPressed;
		player->control.up = (bool)(keyPressed&1);
		player->control.down = (bool)(keyPressed&2);
		player->control.left = (bool)(keyPressed&4);
		player->control.right = (bool)(keyPressed&8);
		player->control.jump = (bool)(keyPressed&16);
		player->control.aux1 = (bool)(keyPressed&32);
		player->control.sneak = (bool)(keyPressed&64);
		player->control.LMB = (bool)(keyPressed&128);
		player->control.RMB = (bool)(keyPressed&256);

		bool cheated = playersao->checkMovementCheat();
		if(cheated){
			// Call callbacks
			m_script->on_cheat(playersao, "moved_too_fast");
		}

		/*infostream<<"Server::ProcessData(): Moved player "<<peer_id<<" to "
															<<"("<<position.X<<","<<position.Y<<","<<position.Z<<")"
															<<" pitch="<<pitch<<" yaw="<<yaw<<std::endl;*/
	}
	else if(command == TOSERVER_DELETEDBLOCKS)
	{
		std::vector<v3s16> deleted_blocks;
		packet[TOSERVER_DELETEDBLOCKS_DATA].convert(&deleted_blocks);
		RemoteClient *client = getClient(peer_id);
		for (auto &block : deleted_blocks)
			client->SetBlockDeleted(block);
	}
	else if(command == TOSERVER_INVENTORY_ACTION)
	{
		std::string datastring;
		packet[TOSERVER_INVENTORY_ACTION_DATA].convert(&datastring);
		std::istringstream is(datastring, std::ios_base::binary);
		// Create an action
		InventoryAction *a = InventoryAction::deSerialize(is);
		if(a == NULL)
		{
			infostream<<"TOSERVER_INVENTORY_ACTION: "
					<<"InventoryAction::deSerialize() returned NULL"
					<<std::endl;
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
		if(a->getType() == IACTION_MOVE)
		{
			IMoveAction *ma = (IMoveAction*)a;

			ma->from_inv.applyCurrentPlayer(player->getName());
			ma->to_inv.applyCurrentPlayer(player->getName());

			setInventoryModified(ma->from_inv);
			setInventoryModified(ma->to_inv);

			bool from_inv_is_current_player =
				(ma->from_inv.type == InventoryLocation::PLAYER) &&
				(ma->from_inv.name == player->getName());

			bool to_inv_is_current_player =
				(ma->to_inv.type == InventoryLocation::PLAYER) &&
				(ma->to_inv.name == player->getName());

			/*
				Disable moving items out of craftpreview
			*/
			if(ma->from_list == "craftpreview")
			{
				infostream<<"Ignoring IMoveAction from "
						<<(ma->from_inv.dump())<<":"<<ma->from_list
						<<" to "<<(ma->to_inv.dump())<<":"<<ma->to_list
						<<" because src is "<<ma->from_list<<std::endl;
				delete a;
				return;
			}

			/*
				Disable moving items into craftresult and craftpreview
			*/
			if(ma->to_list == "craftpreview" || ma->to_list == "craftresult")
			{
				infostream<<"Ignoring IMoveAction from "
						<<(ma->from_inv.dump())<<":"<<ma->from_list
						<<" to "<<(ma->to_inv.dump())<<":"<<ma->to_list
						<<" because dst is "<<ma->to_list<<std::endl;
				delete a;
				return;
			}

			// Disallow moving items in elsewhere than player's inventory
			// if not allowed to interact
			if(!checkPriv(player->getName(), "interact") &&
					(!from_inv_is_current_player ||
					!to_inv_is_current_player))
			{
				infostream<<"Cannot move outside of player's inventory: "
						<<"No interact privilege"<<std::endl;
				delete a;
				return;
			}
		}
		/*
			Handle restrictions and special cases of the drop action
		*/
		else if(a->getType() == IACTION_DROP)
		{
			IDropAction *da = (IDropAction*)a;

			da->from_inv.applyCurrentPlayer(player->getName());

			setInventoryModified(da->from_inv);

			/*
				Disable dropping items out of craftpreview
			*/
			if(da->from_list == "craftpreview")
			{
				infostream<<"Ignoring IDropAction from "
						<<(da->from_inv.dump())<<":"<<da->from_list
						<<" because src is "<<da->from_list<<std::endl;
				delete a;
				return;
			}

			// Disallow dropping items if not allowed to interact
			if(!checkPriv(player->getName(), "interact"))
			{
				delete a;
				return;
			}
		}
		/*
			Handle restrictions and special cases of the craft action
		*/
		else if(a->getType() == IACTION_CRAFT)
		{
			ICraftAction *ca = (ICraftAction*)a;

			ca->craft_inv.applyCurrentPlayer(player->getName());

			setInventoryModified(ca->craft_inv);

			//bool craft_inv_is_current_player =
			//	(ca->craft_inv.type == InventoryLocation::PLAYER) &&
			//	(ca->craft_inv.name == player->getName());

			// Disallow crafting if not allowed to interact
			if(!checkPriv(player->getName(), "interact"))
			{
				infostream<<"Cannot craft: "
						<<"No interact privilege"<<std::endl;
				delete a;
				return;
			}
		}

		// Do the action
		a->apply(this, playersao, this);
		// Eat the action
		delete a;
	}
	else if(command == TOSERVER_CHAT_MESSAGE)
	{
		std::string message = packet[TOSERVER_CHAT_MESSAGE_DATA].as<std::string>();

		// If something goes wrong, this player is to blame
		RollbackScopeActor rollback_scope(m_rollback,
				std::string("player:")+player->getName());

		// Get player name of this client
		std::string name = player->getName();

		// Run script hook
		bool ate = m_script->on_chat_message(player->getName(), message);
		// If script ate the message, don't proceed
		if(ate)
			return;

		// Line to send to players
		std::string line;
		// Whether to send to other players
		bool send_to_others = false;

		// Commands are implemented in Lua, so only catch invalid
		// commands that were not "eaten" and send an error back
		if(message[0] == '/')
		{
			message = message.substr(1);
			if(message.length() == 0)
				line += "-!- Empty command";
			else
				// TODO: str_split(message, ' ')[0]
				line += "-!- Invalid command: " + message;
		}
		else
		{
			if(checkPriv(player->getName(), "shout")){
				line += "<";
				line += name;
				line += "> ";
				line += message;
				send_to_others = true;
			} else
				line += "-!- You don't have permission to shout.";
		}

		if(!line.empty())
		{
			if(send_to_others) {
				actionstream<<"CHAT: "<<line<<std::endl;
				SendChatMessage(PEER_ID_INEXISTENT, line);
			} else
				SendChatMessage(peer_id, line);
		}
	}
	else if(command == TOSERVER_DAMAGE)
	{
		u8 damage = packet[TOSERVER_DAMAGE_VALUE].as<u8>();

		if(g_settings->getBool("enable_damage"))
		{
			actionstream<<player->getName()<<" damaged by "
					<<(int)damage<<" hp at "<<PP(player->getPosition()/BS)
					<<std::endl;

			playersao->setHP(playersao->getHP() - damage);

			if(playersao->getHP() == 0 && playersao->m_hp_not_sent)
				DiePlayer(peer_id);

			if(playersao->m_hp_not_sent)
				SendPlayerHP(peer_id);
		}
	}
	else if(command == TOSERVER_BREATH)
	{
		playersao->setBreath(packet[TOSERVER_BREATH_VALUE].as<u16>());
		m_script->player_event(playersao,"breath_changed");
	}
	else if(command == TOSERVER_CHANGE_PASSWORD)
	{
		std::string oldpwd, newpwd;
		packet[TOSERVER_CHANGE_PASSWORD_OLD].convert(&oldpwd);
		packet[TOSERVER_CHANGE_PASSWORD_NEW].convert(&newpwd);

		if(!base64_is_valid(newpwd)){
			infostream<<"Server: "<<player->getName()<<" supplied invalid password hash"<<std::endl;
			// Wrong old password supplied!!
			SendChatMessage(peer_id, "Invalid new password hash supplied. Password NOT changed.");
			return;
		}

		infostream<<"Server: Client requests a password change from "
				<<"'"<<oldpwd<<"' to '"<<newpwd<<"'"<<std::endl;

		std::string playername = player->getName();

		std::string checkpwd;
		m_script->getAuth(playername, &checkpwd, NULL);

		if(oldpwd != checkpwd)
		{
			infostream<<"Server: invalid old password"<<std::endl;
			// Wrong old password supplied!!
			SendChatMessage(peer_id, "Invalid old password supplied. Password NOT changed.");
			return;
		}

		bool success = m_script->setPassword(playername, newpwd);
		if(success){
			actionstream<<player->getName()<<" changes password"<<std::endl;
			SendChatMessage(peer_id, "Password change successful.");
		} else {
			actionstream<<player->getName()<<" tries to change password but "
					<<"it fails"<<std::endl;
			SendChatMessage(peer_id, "Password change failed or inavailable.");
		}
	}
	else if(command == TOSERVER_PLAYERITEM)
	{
		u16 item = packet[TOSERVER_PLAYERITEM_VALUE].as<u16>();
		playersao->setWieldIndex(item);
	}
	else if(command == TOSERVER_RESPAWN)
	{
		if(player->hp != 0 || !g_settings->getBool("enable_damage"))
			return;

		RespawnPlayer(peer_id);

		actionstream<<player->getName()<<" respawns at "
				<<PP(player->getPosition()/BS)<<std::endl;

		// ActiveObject is added to environment in AsyncRunStep after
		// the previous addition has been succesfully removed
	}
	else if(command == TOSERVER_INTERACT)
	{
		u8 action;
		u16 item_i;
		PointedThing pointed;

		packet[TOSERVER_INTERACT_ACTION].convert(&action);
		packet[TOSERVER_INTERACT_ITEM].convert(&item_i);
		packet[TOSERVER_INTERACT_POINTED_THING].convert(&pointed);

		if(player->hp == 0)
		{
			verbosestream<<"TOSERVER_INTERACT: "<<player->getName()
				<<" tried to interact, but is dead!"<<std::endl;
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
		if(pointed.type == POINTEDTHING_OBJECT)
		{
			pointed_object = m_env->getActiveObject(pointed.object_id);
			if(pointed_object == NULL)
			{
				verbosestream<<"TOSERVER_INTERACT: "
					"pointed object is NULL"<<std::endl;
				return;
			}

		}

		v3f pointed_pos_under = player_pos;
		v3f pointed_pos_above = player_pos;
		if(pointed.type == POINTEDTHING_NODE)
		{
			pointed_pos_under = intToFloat(p_under, BS);
			pointed_pos_above = intToFloat(p_above, BS);
		}
		else if(pointed.type == POINTEDTHING_OBJECT)
		{
			pointed_pos_under = pointed_object->getBasePosition();
			pointed_pos_above = pointed_pos_under;
		}

		/*
			Check that target is reasonably close
			(only when digging or placing things)
		*/
		if(action == 0 || action == 2 || action == 3)
		{
			float d = player_pos.getDistanceFrom(pointed_pos_under);
			float max_d = BS * 14; // Just some large enough value
			if(d > max_d){
				actionstream<<"Player "<<player->getName()
						<<" tried to access "<<pointed.dump()
						<<" from too far: "
						<<"d="<<d<<", max_d="<<max_d
						<<". ignoring."<<std::endl;
				// Re-send block to revert change on client-side
				RemoteClient *client = getClient(peer_id);
				v3s16 blockpos = getNodeBlockPos(floatToInt(pointed_pos_under, BS));
				client->SetBlockNotSent(blockpos);
				// Call callbacks
				m_script->on_cheat(playersao, "interacted_too_far");
				// Do nothing else
				return;
			}
		}

		/*
			Make sure the player is allowed to do it
		*/
		if(!checkPriv(player->getName(), "interact"))
		{
			actionstream<<player->getName()<<" attempted to interact with "
					<<pointed.dump()<<" without 'interact' privilege"
					<<std::endl;
			// Re-send block to revert change on client-side
			RemoteClient *client = getClient(peer_id);
			// Digging completed -> under
			if(action == 2){
				v3s16 blockpos = getNodeBlockPos(floatToInt(pointed_pos_under, BS));
				client->SetBlockNotSent(blockpos);
			}
			// Placement -> above
			if(action == 3){
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
		if(action == 0)
		{
			if(pointed.type == POINTEDTHING_NODE)
			{
				/*
					NOTE: This can be used in the future to check if
					somebody is cheating, by checking the timing.
				*/
				MapNode n(CONTENT_IGNORE);
				try
				{
					n = m_env->getMap().getNode(p_under);
				}
				catch(InvalidPositionException &e)
				{
					infostream<<"Server: Not punching: Node not found."
							<<" Adding block to emerge queue."
							<<std::endl;
					m_emerge->enqueueBlockEmerge(peer_id, getNodeBlockPos(p_above), false);
				}
				if(n.getContent() != CONTENT_IGNORE)
					m_script->node_on_punch(p_under, n, playersao, pointed);
				// Cheat prevention
				playersao->noCheatDigStart(p_under);
			}
			else if(pointed.type == POINTEDTHING_OBJECT)
			{
				// Skip if object has been removed
				if(pointed_object->m_removed)
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
				pointed_object->punch(dir, &toolcap, playersao,
						time_from_last_punch);
			}

		} // action == 0

		/*
			1: stop digging
		*/
		else if(action == 1)
		{
		} // action == 1

		/*
			2: Digging completed
		*/
		else if(action == 2)
		{
			// Only digging of nodes
			if(pointed.type == POINTEDTHING_NODE)
			{
				MapNode n(CONTENT_IGNORE);
				try
				{
					n = m_env->getMap().getNode(p_under);
				}
				catch(InvalidPositionException &e)
				{
					infostream<<"Server: Not finishing digging: Node not found."
							<<" Adding block to emerge queue."
							<<std::endl;
					m_emerge->enqueueBlockEmerge(peer_id, getNodeBlockPos(p_above), false);
				}

				/* Cheat prevention */
				bool is_valid_dig = true;
				if(!isSingleplayer() && !g_settings->getBool("disable_anticheat"))
				{
					v3s16 nocheat_p = playersao->getNoCheatDigPos();
					float nocheat_t = playersao->getNoCheatDigTime();
					playersao->noCheatDigEnd();
					// If player didn't start digging this, ignore dig
					if(nocheat_p != p_under){
						infostream<<"Server: NoCheat: "<<player->getName()
								<<" started digging "
								<<PP(nocheat_p)<<" and completed digging "
								<<PP(p_under)<<"; not digging."<<std::endl;
						is_valid_dig = false;
						// Call callbacks
						m_script->on_cheat(playersao, "finished_unknown_dig");
					}
					// Get player's wielded item
					ItemStack playeritem;
					InventoryList *mlist = playersao->getInventory()->getList("main");
					if(mlist != NULL)
						playeritem = mlist->getItem(playersao->getWieldIndex());
					ToolCapabilities playeritem_toolcap =
							playeritem.getToolCapabilities(m_itemdef);
					// Get diggability and expected digging time
					DigParams params = getDigParams(m_nodedef->get(n).groups,
							&playeritem_toolcap);
					// If can't dig, try hand
					if(!params.diggable){
						const ItemDefinition &hand = m_itemdef->get("");
						const ToolCapabilities *tp = hand.tool_capabilities;
						if(tp)
							params = getDigParams(m_nodedef->get(n).groups, tp);
					}
					// If can't dig, ignore dig
					if(!params.diggable){
						infostream<<"Server: NoCheat: "<<player->getName()
								<<" completed digging "<<PP(p_under)
								<<", which is not diggable with tool. not digging."
								<<std::endl;
						is_valid_dig = false;
						// Call callbacks
						m_script->on_cheat(playersao, "dug_unbreakable");
					}
					// Check digging time
					// If already invalidated, we don't have to
					if(!is_valid_dig){
						// Well not our problem then
					}
					// Clean and long dig
					else if(params.time > 2.0 && nocheat_t * 1.2 > params.time){
						// All is good, but grab time from pool; don't care if
						// it's actually available
						playersao->getDigPool().grab(params.time);
					}
					// Short or laggy dig
					// Try getting the time from pool
					else if(playersao->getDigPool().grab(params.time)){
						// All is good
					}
					// Dig not possible
					else{
						infostream<<"Server: NoCheat: "<<player->getName()
								<<" completed digging "<<PP(p_under)
								<<"too fast; not digging."<<std::endl;
						is_valid_dig = false;
						// Call callbacks
						m_script->on_cheat(playersao, "dug_too_fast");
					}
				}

				/* Actually dig node */

				if(is_valid_dig && n.getContent() != CONTENT_IGNORE)
					m_script->node_on_dig(p_under, n, playersao);

				// Send unusual result (that is, node not being removed)
				if(m_env->getMap().getNodeNoEx(p_under).getContent() != CONTENT_AIR)
				{
					// Re-send block to revert change on client-side
					RemoteClient *client = getClient(peer_id);
					v3s16 blockpos = getNodeBlockPos(floatToInt(pointed_pos_under, BS));
					client->SetBlockNotSent(blockpos);
				}
			}
		} // action == 2

		/*
			3: place block or right-click object
		*/
		else if(action == 3)
		{
			ItemStack item = playersao->getWieldedItem();

			// Reset build time counter
			if(pointed.type == POINTEDTHING_NODE &&
					item.getDefinition(m_itemdef).type == ITEM_NODE)
				getClient(peer_id)->m_time_from_building = 0.0;

			if(pointed.type == POINTEDTHING_OBJECT)
			{
				// Right click object

				// Skip if object has been removed
				if(pointed_object->m_removed)
					return;

/* android bug - too many
				actionstream<<player->getName()<<" right-clicks object "
						<<pointed.object_id<<": "
						<<pointed_object->getDescription()<<std::endl;
*/

				// Do stuff
				pointed_object->rightClick(playersao);
			}
			else if(m_script->item_OnPlace(
					item, playersao, pointed))
			{
				// Placement was handled in lua

				// Apply returned ItemStack
				playersao->setWieldedItem(item);
			}

			// If item has node placement prediction, always send the
			// blocks to make sure the client knows what exactly happened
			if(item.getDefinition(m_itemdef).node_placement_prediction != ""){
				RemoteClient *client = getClient(peer_id);
				v3s16 blockpos = getNodeBlockPos(floatToInt(pointed_pos_above, BS));
				client->SetBlockNotSent(blockpos);
				v3s16 blockpos2 = getNodeBlockPos(floatToInt(pointed_pos_under, BS));
				if(blockpos2 != blockpos){
					client->SetBlockNotSent(blockpos2);
				}
			}
		} // action == 3

		/*
			4: use
		*/
		else if(action == 4)
		{
			ItemStack item = playersao->getWieldedItem();

			actionstream<<player->getName()<<" uses "<<item.name
					<<", pointing at "<<pointed.dump()<<std::endl;

			if(m_script->item_OnUse(
					item, playersao, pointed))
			{
				// Apply returned ItemStack
				playersao->setWieldedItem(item);
			}

		} // action == 4
		

		/*
			Catch invalid actions
		*/
		else
		{
			infostream<<"WARNING: Server: Invalid action "
					<<action<<std::endl;
		}
	}
	else if(command == TOSERVER_REMOVED_SOUNDS)
	{
		std::vector<s32> removed_ids;
		packet[TOSERVER_REMOVED_SOUNDS_IDS].convert(&removed_ids);
		for (auto id : removed_ids) {
			std::map<s32, ServerPlayingSound>::iterator i =
					m_playing_sounds.find(id);
			if(i == m_playing_sounds.end())
				continue;
			ServerPlayingSound &psound = i->second;
			psound.clients.erase(peer_id);
			if(psound.clients.size() == 0)
				m_playing_sounds.erase(i);
		}
	}
	else if(command == TOSERVER_NODEMETA_FIELDS)
	{
		v3s16 p = packet[TOSERVER_NODEMETA_FIELDS_POS].as<v3s16>();
		std::string formname = packet[TOSERVER_NODEMETA_FIELDS_FORMNAME].as<std::string>();
		std::map<std::string, std::string> fields;
		packet[TOSERVER_NODEMETA_FIELDS_DATA].convert(&fields);

		// If something goes wrong, this player is to blame
		RollbackScopeActor rollback_scope(m_rollback,
				std::string("player:")+player->getName());

		// Check the target node for rollback data; leave others unnoticed
		RollbackNode rn_old(&m_env->getMap(), p, this);

		m_script->node_on_receive_fields(p, formname, fields,playersao);

		// Report rollback data
		RollbackNode rn_new(&m_env->getMap(), p, this);
		if(rollback() && rn_new != rn_old){
			RollbackAction action;
			action.setSetNode(p, rn_old, rn_new);
			rollback()->reportAction(action);
		}
	}
	else if(command == TOSERVER_INVENTORY_FIELDS)
	{
		std::string formname;
		std::map<std::string, std::string> fields;

		packet[TOSERVER_INVENTORY_FIELDS_FORMNAME].convert(&formname);
		packet[TOSERVER_INVENTORY_FIELDS_DATA].convert(&fields);

		m_script->on_playerReceiveFields(playersao, formname, fields);
	}
	else
	{
		infostream<<"Server::ProcessData(): Ignoring "
				"unknown command "<<command<<std::endl;
	}

	} //try
	catch(SendFailedException &e)
	{
		errorstream<<"Server::ProcessData(): SendFailedException: "
				<<"what="<<e.what()
				<<std::endl;
	}
}

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
	if(m_ignore_map_edit_events_area.contains(event->getArea()))
		return;
	MapEditEvent *e = event->clone();
	m_unsent_map_edit_queue.push_back(e);
}

Inventory* Server::getInventory(const InventoryLocation &loc)
{
	switch(loc.type){
	case InventoryLocation::UNDEFINED:
	{}
	break;
	case InventoryLocation::CURRENT_PLAYER:
	{}
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
		assert(0);
	}
	return NULL;
}
void Server::setInventoryModified(const InventoryLocation &loc)
{
	switch(loc.type){
	case InventoryLocation::UNDEFINED:
	{}
	break;
	case InventoryLocation::PLAYER:
	{
		Player *player = m_env->getPlayer(loc.name.c_str());
		if(!player)
			return;
		PlayerSAO *playersao = player->getPlayerSAO();
		if(!playersao)
			return;
		playersao->m_inventory_not_sent = true;
		playersao->m_wielded_item_not_sent = true;
	}
	break;
	case InventoryLocation::NODEMETA:
	{
		v3s16 blockpos = getNodeBlockPos(loc.p);

		MapBlock *block = m_env->getMap().getBlockNoCreateNoEx(blockpos);
		if(block)
			block->raiseModified(MOD_STATE_WRITE_NEEDED, "inventoryModified");

		setBlockNotSent(blockpos);
	}
	break;
	case InventoryLocation::DETACHED:
	{
		sendDetachedInventory(loc.name,PEER_ID_INEXISTENT);
	}
	break;
	default:
		assert(0);
	}
}

void Server::SetBlocksNotSent(std::map<v3s16, MapBlock *>& block)
{
	std::list<u16> clients = m_clients.getClientIDs();
	m_clients.Lock();
	// Set the modified blocks unsent for all the clients
	for (std::list<u16>::iterator
		 i = clients.begin();
		 i != clients.end(); ++i) {
			RemoteClient *client = m_clients.lockedGetClientNoEx(*i);
			if (client != NULL)
				client->SetBlocksNotSent(block);
		}
	m_clients.Unlock();
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
	m_peer_change_queue.push_back(c);
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
	m_peer_change_queue.push_back(c);
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
	m_clients.Lock();
	RemoteClient* client = m_clients.lockedGetClientNoEx(peer_id, CS_Invalid);

	if (client == NULL) {
		m_clients.Unlock();
		return false;
	}

	*uptime = client->uptime();
	*ser_vers = client->serialization_version;
	*prot_vers = client->net_proto_version;

	*major = client->getMajor();
	*minor = client->getMinor();
	*patch = client->getPatch();
	*vers_string = client->getPatch();

	m_clients.Unlock();

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

void Server::SendMovement(u16 peer_id)
{
	DSTACK(__FUNCTION_NAME);

	MSGPACK_PACKET_INIT(TOCLIENT_MOVEMENT, 12);

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

	// Send as reliable
	m_clients.send(peer_id, 0, buffer, true);
}

void Server::SendHP(u16 peer_id, u8 hp)
{
	DSTACK(__FUNCTION_NAME);
	std::ostringstream os(std::ios_base::binary);

	MSGPACK_PACKET_INIT(TOCLIENT_HP, 1);
	PACK(TOCLIENT_HP_HP, hp);

	// Send as reliable
	m_clients.send(peer_id, 0, buffer, true);
}

void Server::SendBreath(u16 peer_id, u16 breath)
{
	DSTACK(__FUNCTION_NAME);
	MSGPACK_PACKET_INIT(TOCLIENT_BREATH, 1);
	PACK(TOCLIENT_BREATH_BREATH, breath);
	// Send as reliable
	m_clients.send(peer_id, 0, buffer, true);
}

void Server::SendAccessDenied(u16 peer_id,const std::string &reason)
{
	DSTACK(__FUNCTION_NAME);
	MSGPACK_PACKET_INIT(TOCLIENT_ACCESS_DENIED, 1);
	PACK(TOCLIENT_ACCESS_DENIED_REASON, reason);

	// Send as reliable
	m_clients.send(peer_id, 0, buffer, true);
}

void Server::SendDeathscreen(u16 peer_id,bool set_camera_point_target,
		v3f camera_point_target)
{
	DSTACK(__FUNCTION_NAME);

	MSGPACK_PACKET_INIT(TOCLIENT_DEATHSCREEN, 2);
	PACK(TOCLIENT_DEATHSCREEN_SET_CAMERA, set_camera_point_target);
	PACK(TOCLIENT_DEATHSCREEN_CAMERA_POINT, camera_point_target);

	// Send as reliable
	m_clients.send(peer_id, 0, buffer, true);
}

void Server::SendItemDef(u16 peer_id,
		IItemDefManager *itemdef, u16 protocol_version)
{
	DSTACK(__FUNCTION_NAME);
	MSGPACK_PACKET_INIT(TOCLIENT_ITEMDEF, 1);
	PACK(TOCLIENT_ITEMDEF_DEFINITIONS, *itemdef);

	m_clients.send(peer_id, 0, buffer, true);
}

void Server::SendNodeDef(u16 peer_id,
		INodeDefManager *nodedef, u16 protocol_version)
{
	DSTACK(__FUNCTION_NAME);

	MSGPACK_PACKET_INIT(TOCLIENT_NODEDEF, 1);
	PACK(TOCLIENT_NODEDEF_DEFINITIONS, *nodedef);

	// Send as reliable
	m_clients.send(peer_id, 0, buffer, true);
}

/* not merged to mt
void Server::SendAnimations(u16 peer_id)
{
	DSTACK(__FUNCTION_NAME);

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

void Server::SendInventory(u16 peer_id)
{
	DSTACK(__FUNCTION_NAME);

	PlayerSAO *playersao = getPlayerSAO(peer_id);
	assert(playersao);

	playersao->m_inventory_not_sent = false;

	/*
		Serialize it
	*/

	std::ostringstream os;
	playersao->getInventory()->serialize(os);

	std::string s = os.str();

	MSGPACK_PACKET_INIT(TOCLIENT_INVENTORY, 1);
	PACK(TOCLIENT_INVENTORY_DATA, s);

	// Send as reliable
	m_clients.send(peer_id, 0, buffer, true);
}

void Server::SendChatMessage(u16 peer_id, const std::string &message)
{
	DSTACK(__FUNCTION_NAME);

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
	DSTACK(__FUNCTION_NAME);

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
	DSTACK(__FUNCTION_NAME);

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
	DSTACK(__FUNCTION_NAME);

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
	DSTACK(__FUNCTION_NAME);

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
	DSTACK(__FUNCTION_NAME);

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
	DSTACK(__FUNCTION_NAME);
	PlayerSAO *playersao = getPlayerSAO(peer_id);
	assert(playersao);
	playersao->m_hp_not_sent = false;
	SendHP(peer_id, playersao->getHP());
	m_script->player_event(playersao,"health_changed");

	// Send to other clients
	std::string str = gob_cmd_punched(playersao->readDamage(), playersao->getHP());
	ActiveObjectMessage aom(playersao->getId(), true, str);
	playersao->m_messages_out.push_back(aom);
}

void Server::SendPlayerBreath(u16 peer_id)
{
	DSTACK(__FUNCTION_NAME);
	PlayerSAO *playersao = getPlayerSAO(peer_id);
	assert(playersao);
	playersao->m_breath_not_sent = false;
	m_script->player_event(playersao,"breath_changed");
	SendBreath(peer_id, playersao->getBreath());
}

void Server::SendMovePlayer(u16 peer_id)
{
	DSTACK(__FUNCTION_NAME);
	Player *player = m_env->getPlayer(peer_id);
	assert(player);

	MSGPACK_PACKET_INIT(TOCLIENT_MOVE_PLAYER, 3);
	PACK(TOCLIENT_MOVE_PLAYER_POS, player->getPosition());
	PACK(TOCLIENT_MOVE_PLAYER_PITCH, player->getPitch());
	PACK(TOCLIENT_MOVE_PLAYER_YAW, player->getYaw());

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
	assert(player);
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
	assert(player);
	if(player->peer_id == PEER_ID_INEXISTENT)
		return;

	MSGPACK_PACKET_INIT(TOCLIENT_INVENTORY_FORMSPEC, 1);
	PACK(TOCLIENT_INVENTORY_FORMSPEC_DATA, FORMSPEC_VERSION_STRING + player->inventory_formspec);

	// Send as reliable
	m_clients.send(peer_id, 0, buffer, true);
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
	std::list<u16> dst_clients;
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
		std::list<u16> clients = m_clients.getClientIDs();

		for(std::list<u16>::iterator
				i = clients.begin(); i != clients.end(); ++i)
		{
			Player *player = m_env->getPlayer(*i);
			if(!player)
				continue;
			if(pos_exists){
				if(player->getPosition().getDistanceFrom(pos) >
						params.max_hear_distance)
					continue;
			}
			dst_clients.push_back(*i);
		}
	}
	if(dst_clients.size() == 0)
		return -1;

	// Create the sound
	s32 id = m_next_sound_id++;
	// The sound will exist as a reference in m_playing_sounds
	m_playing_sounds[id] = ServerPlayingSound();
	ServerPlayingSound &psound = m_playing_sounds[id];
	psound.params = params;
	for(std::list<u16>::iterator i = dst_clients.begin();
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
	for(std::list<u16>::iterator i = dst_clients.begin();
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
	std::list<u16> *far_players, float far_d_nodes)
{
	float maxd = far_d_nodes*BS;
	v3f p_f = intToFloat(p, BS);

	// Create packet
	MSGPACK_PACKET_INIT(TOCLIENT_REMOVENODE, 1);
	PACK(TOCLIENT_REMOVENODE_POS, p);

	std::list<u16> clients = m_clients.getClientIDs();
	for(std::list<u16>::iterator
		i = clients.begin();
		i != clients.end(); ++i)
	{
		if(far_players)
		{
			// Get player
			Player *player = m_env->getPlayer(*i);
			if(player)
			{
				// If player is far away, only set modified blocks not sent
				v3f player_pos = player->getPosition();
				if(player_pos.getDistanceFrom(p_f) > maxd)
				{
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
		std::list<u16> *far_players, float far_d_nodes,
		bool remove_metadata)
{
	float maxd = far_d_nodes*BS;
	v3f p_f = intToFloat(p, BS);

	std::list<u16> clients = m_clients.getClientIDs();
	for(std::list<u16>::iterator
				i = clients.begin();
		i != clients.end(); ++i)
	{

		if(far_players)
		{
			// Get player
			Player *player = m_env->getPlayer(*i);
			if(player)
			{
				// If player is far away, only set modified blocks not sent
				v3f player_pos = player->getPosition();
				if(player_pos.getDistanceFrom(p_f) > maxd)
				{
					far_players->push_back(*i);
					continue;
				}
			}
		}

		m_clients.Lock();
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
		m_clients.Unlock();
	}
}

void Server::setBlockNotSent(v3s16 p)
{
	std::list<u16> clients = m_clients.getClientIDs();
	m_clients.Lock();
	for(std::list<u16>::iterator
		i = clients.begin();
		i != clients.end(); ++i)
	{
		RemoteClient *client = m_clients.lockedGetClientNoEx(*i);
		client->SetBlockNotSent(p);
	}
	m_clients.Unlock();
}

void Server::SendBlockNoLock(u16 peer_id, MapBlock *block, u8 ver, u16 net_proto_version, bool reliable)
{
	DSTACK(__FUNCTION_NAME);

	g_profiler->add("Connection: blocks sent", 1);

	MSGPACK_PACKET_INIT(TOCLIENT_BLOCKDATA, 4);
	PACK(TOCLIENT_BLOCKDATA_POS, block->getPos());

	std::ostringstream os(std::ios_base::binary);
	block->serialize(os, ver, false);
	PACK(TOCLIENT_BLOCKDATA_DATA, os.str());

	PACK(TOCLIENT_BLOCKDATA_HEAT, (s16)block->heat);
	PACK(TOCLIENT_BLOCKDATA_HUMIDITY, (s16)block->humidity);

	JMutexAutoLock lock(m_env_mutex);
	/*
		Send packet
	*/
	m_clients.send(peer_id, 2, buffer, reliable);
}

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

		std::list<u16> clients = m_clients.getClientIDs();

		//m_clients.Lock();
		for(std::list<u16>::iterator
			i = clients.begin();
			i != clients.end(); ++i)
		{
			RemoteClient *client = m_clients.lockedGetClientNoEx(*i, CS_Active);

			if (!client)
				continue;

			total += client->GetNextBlocks(m_env,m_emerge, dtime, m_uptime.get() + m_env->m_game_time_start, queue);
		}
		//m_clients.Unlock();
	}

	// Sort.
	// Lowest priority number comes first.
	// Lowest is most important.
	std::sort(queue.begin(), queue.end());

	//m_clients.Lock();
	for(u32 i=0; i<queue.size(); i++)
	{
		//TODO: Calculate limit dynamically

		PrioritySortedBlockTransfer q = queue[i];

		MapBlock *block = NULL;
		try
		{
			block = m_env->getMap().getBlockNoCreate(q.pos);
		}
		catch(InvalidPositionException &e)
		{
			continue;
		}

		RemoteClient *client = m_clients.lockedGetClientNoEx(q.peer_id, CS_Active);

		if(!client)
			continue;

		// maybe sometimes blocks will not load (must wait 1+ minute), but reduce network load: q.priority<=4
		SendBlockNoLock(q.peer_id, block, client->serialization_version, client->net_proto_version, 1);

		client->SentBlock(q.pos, m_uptime.get() + m_env->m_game_time_start);
		++total;
	}
	//m_clients.Unlock();
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
}

struct SendableMediaAnnouncement
{
	std::string name;
	std::string sha1_digest;

	SendableMediaAnnouncement(const std::string &name_="",
	                          const std::string &sha1_digest_=""):
		name(name_),
		sha1_digest(sha1_digest_)
	{}
};

void Server::sendMediaAnnouncement(u16 peer_id)
{
	DSTACK(__FUNCTION_NAME);

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

void Server::sendRequestedMedia(u16 peer_id,
		const std::list<std::string> &tosend)
{
	DSTACK(__FUNCTION_NAME);

	verbosestream<<"Server::sendRequestedMedia(): "
			<<"Sending files to client"<<std::endl;

	/* Read files */
	// TODO: optimize
	MediaData media_data;

	for(std::list<std::string>::const_iterator i = tosend.begin();
			i != tosend.end(); ++i)
	{
		const std::string &name = *i;

		if(m_media.find(name) == m_media.end()){
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
	}

	MSGPACK_PACKET_INIT(TOCLIENT_MEDIA, 1);
	PACK(TOCLIENT_MEDIA_MEDIA, media_data);

	// Send as reliable
	m_clients.send(peer_id, 2, buffer, true);
}

void Server::sendDetachedInventory(const std::string &name, u16 peer_id)
{
	if(m_detached_inventories.count(name) == 0){
		errorstream<<__FUNCTION_NAME<<": \""<<name<<"\" not found"<<std::endl;
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
	assert(playersao);

	infostream<<"Server::DiePlayer(): Player "
			<<playersao->getPlayer()->getName()
			<<" dies"<<std::endl;

	playersao->setHP(0);

	// Trigger scripted stuff
	m_script->on_dieplayer(playersao);

	SendPlayerHP(peer_id);
	SendDeathscreen(peer_id, false, v3f(0,0,0));
}

void Server::RespawnPlayer(u16 peer_id)
{
	DSTACK(__FUNCTION_NAME);

	PlayerSAO *playersao = getPlayerSAO(peer_id);
	assert(playersao);

	infostream<<"Server::RespawnPlayer(): Player "
			<<playersao->getPlayer()->getName()
			<<" respawns"<<std::endl;

	playersao->setHP(PLAYER_MAX_HP);

	bool repositioned = m_script->on_respawnplayer(playersao);
	if(!repositioned){
		v3f pos = findSpawnPos(m_env->getServerMap());
		playersao->setPos(pos);
	}
}

void Server::DenyAccess(u16 peer_id, const std::string &reason)
{
	DSTACK(__FUNCTION_NAME);

	SendAccessDenied(peer_id, reason);
	m_clients.event(peer_id, CSE_SetDenied);
	m_con.DisconnectPeer(peer_id);
}

void Server::DenyAccess(u16 peer_id, const std::wstring &reason)
{
    DenyAccess(peer_id, wide_to_narrow(reason));
}

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
			if(psound.clients.size() == 0)
				m_playing_sounds.erase(i++);
			else
				i++;
		}

		Player *player = m_env->getPlayer(peer_id);

		// Collect information about leaving in chat
		{
			if(player != NULL && reason != CDR_DENY)
			{
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

				JMutexAutoLock env_lock(m_env_mutex);
				m_script->on_leaveplayer(playersao);

				playersao->disconnected();
			}
		}

		/*
			Print out action
		*/
		{
			if(player != NULL && reason != CDR_DENY)
			{
				std::ostringstream os(std::ios_base::binary);
				std::list<u16> clients = m_clients.getClientIDs();

				for(std::list<u16>::iterator
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
			JMutexAutoLock env_lock(m_env_mutex);
			m_clients.DeleteClient(peer_id);
		}
	}

	// Send leave chat message to all remaining clients
	if(message.length() != 0)
		SendChatMessage(PEER_ID_INEXISTENT,message);
}

void Server::UpdateCrafting(u16 peer_id)
{
	DSTACK(__FUNCTION_NAME);

	Player* player = m_env->getPlayer(peer_id);
	assert(player);

	// Get a preview for crafting
	ItemStack preview;
	InventoryLocation loc;
	loc.setPlayer(player->getName());
	getCraftingResult(&player->inventory, preview, false, this);
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
	os<<"version="<<minetest_version_simple;
	// Uptime
	os<<", uptime="<<m_uptime.get();
	// Max lag estimate
	os<<", max_lag="<<m_env->getMaxLagEstimate();
	// Information about clients
	bool first = true;
	os<<", clients={";
	std::list<u16> clients = m_clients.getClientIDs();
	for(std::list<u16>::iterator i = clients.begin();
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
		std::list<u16> clients = m_clients.getClientIDs();
		for(std::list<u16>::iterator
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
	Player *player = m_env->getPlayer(name);
	if(!player)
		return;

	if (player->peer_id == PEER_ID_INEXISTENT)
		return;

	SendChatMessage(player->peer_id, std::string("\vffffff") + msg);
}

bool Server::showFormspec(const char *playername, const std::string &formspec, const std::string &formname)
{
	Player *player = m_env->getPlayer(playername);

	if(!player)
	{
		infostream<<"showFormspec: couldn't find player:"<<playername<<std::endl;
		return false;
	}

	SendShowFormspecMessage(player->peer_id, formspec, formname);
	return true;
}

u32 Server::hudAdd(Player *player, HudElement *form) {
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

bool Server::hudChange(Player *player, u32 id, HudElementStat stat, void *data) {
	if (!player)
		return false;

	SendHUDChange(player->peer_id, id, stat, data);
	return true;
}

bool Server::hudSetFlags(Player *player, u32 flags, u32 mask) {
	if (!player)
		return false;

	SendHUDSetFlags(player->peer_id, flags, mask);
	player->hud_flags = flags;

	m_script->player_event(player->getPlayerSAO(),"hud_changed");
	return true;
}

bool Server::hudSetHotbarItemcount(Player *player, s32 hotbar_itemcount) {
	if (!player)
		return false;
	if (hotbar_itemcount <= 0 || hotbar_itemcount > HUD_HOTBAR_ITEMCOUNT_MAX)
		return false;

	std::ostringstream os(std::ios::binary);
	writeS32(os, hotbar_itemcount);
	SendHUDSetParam(player->peer_id, HUD_PARAM_HOTBAR_ITEMCOUNT, os.str());
	return true;
}

void Server::hudSetHotbarImage(Player *player, std::string name) {
	if (!player)
		return;

	SendHUDSetParam(player->peer_id, HUD_PARAM_HOTBAR_IMAGE, name);
}

void Server::hudSetHotbarSelectedImage(Player *player, std::string name) {
	if (!player)
		return;

	SendHUDSetParam(player->peer_id, HUD_PARAM_HOTBAR_SELECTED_IMAGE, name);
}

bool Server::setLocalPlayerAnimations(Player *player, v2s32 animation_frames[4], f32 frame_speed)
{
	if (!player)
		return false;

	SendLocalPlayerAnimations(player->peer_id, animation_frames, frame_speed);
	return true;
}

bool Server::setPlayerEyeOffset(Player *player, v3f first, v3f third)
{
	if (!player)
		return false;

	SendEyeOffset(player->peer_id, first, third);
	return true;
}

bool Server::setSky(Player *player, const video::SColor &bgcolor,
		const std::string &type, const std::vector<std::string> &params)
{
	if (!player)
		return false;

	SendSetSky(player->peer_id, bgcolor, type, params);
	return true;
}

bool Server::overrideDayNightRatio(Player *player, bool do_override,
		float ratio)
{
	if (!player)
		return false;

	SendOverrideDayNightRatio(player->peer_id, do_override, ratio);
	return true;
}

void Server::notifyPlayers(const std::string &msg)
{
	SendChatMessage(PEER_ID_INEXISTENT,msg);
}

void Server::spawnParticle(const char *playername, v3f pos,
		v3f velocity, v3f acceleration,
		float expirationtime, float size, bool
		collisiondetection, bool vertical, std::string texture)
{
	Player *player = m_env->getPlayer(playername);
	if(!player)
		return;
	SendSpawnParticle(player->peer_id, pos, velocity, acceleration,
			expirationtime, size, collisiondetection, vertical, texture);
}

void Server::spawnParticleAll(v3f pos, v3f velocity, v3f acceleration,
		float expirationtime, float size,
		bool collisiondetection, bool vertical, std::string texture)
{
	SendSpawnParticle(PEER_ID_INEXISTENT,pos, velocity, acceleration,
			expirationtime, size, collisiondetection, vertical, texture);
}

u32 Server::addParticleSpawner(const char *playername,
		u16 amount, float spawntime,
		v3f minpos, v3f maxpos,
		v3f minvel, v3f maxvel,
		v3f minacc, v3f maxacc,
		float minexptime, float maxexptime,
		float minsize, float maxsize,
		bool collisiondetection, bool vertical, std::string texture)
{
	Player *player = m_env->getPlayer(playername);
	if(!player)
		return -1;

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

	SendAddParticleSpawner(player->peer_id, amount, spawntime,
		minpos, maxpos, minvel, maxvel, minacc, maxacc,
		minexptime, maxexptime, minsize, maxsize,
		collisiondetection, vertical, texture, id);

	return id;
}

u32 Server::addParticleSpawnerAll(u16 amount, float spawntime,
		v3f minpos, v3f maxpos,
		v3f minvel, v3f maxvel,
		v3f minacc, v3f maxacc,
		float minexptime, float maxexptime,
		float minsize, float maxsize,
		bool collisiondetection, bool vertical, std::string texture)
{
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

	SendAddParticleSpawner(PEER_ID_INEXISTENT, amount, spawntime,
		minpos, maxpos, minvel, maxvel, minacc, maxacc,
		minexptime, maxexptime, minsize, maxsize,
		collisiondetection, vertical, texture, id);

	return id;
}

void Server::deleteParticleSpawner(const char *playername, u32 id)
{
	Player *player = m_env->getPlayer(playername);
	if(!player)
		return;

	m_particlespawner_ids.erase(
			std::remove(m_particlespawner_ids.begin(),
			m_particlespawner_ids.end(), id),
			m_particlespawner_ids.end());
	SendDeleteParticleSpawner(player->peer_id, id);
}

void Server::deleteParticleSpawnerAll(u32 id)
{
	m_particlespawner_ids.erase(
			std::remove(m_particlespawner_ids.begin(),
			m_particlespawner_ids.end(), id),
			m_particlespawner_ids.end());
	SendDeleteParticleSpawner(PEER_ID_INEXISTENT, id);
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

void Server::deleteDetachedInventory(const std::string &name)
{
	if(m_detached_inventories.count(name) > 0){
		infostream<<"Server deleting detached inventory \""<<name<<"\""<<std::endl;
		delete m_detached_inventories[name];
		m_detached_inventories.erase(name);
	}
}

class BoolScopeSet
{
public:
	BoolScopeSet(bool *dst, bool val):
		m_dst(dst)
	{
		m_orig_state = *m_dst;
		*m_dst = val;
	}
	~BoolScopeSet()
	{
		*m_dst = m_orig_state;
	}
private:
	bool *m_dst;
	bool m_orig_state;
};

// actions: time-reversed list
// Return value: success/failure
bool Server::rollbackRevertActions(const std::list<RollbackAction> &actions,
		std::list<std::string> *log)
{
	infostream<<"Server::rollbackRevertActions(len="<<actions.size()<<")"<<std::endl;
	ServerMap *map = (ServerMap*)(&m_env->getMap());
	// Disable rollback report sink while reverting
	BoolScopeSet rollback_scope_disable(&m_rollback_sink_enabled, false);

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
IItemDefManager* Server::getItemDefManager()
{
	return m_itemdef;
}
INodeDefManager* Server::getNodeDefManager()
{
	return m_nodedef;
}
ICraftDefManager* Server::getCraftDefManager()
{
	return m_craftdef;
}
ITextureSource* Server::getTextureSource()
{
	return NULL;
}
IShaderSource* Server::getShaderSource()
{
	return NULL;
}
u16 Server::allocateUnknownNodeId(const std::string &name)
{
	return m_nodedef->allocateDummy(name);
}
ISoundManager* Server::getSoundManager()
{
	return &dummySoundManager;
}
MtEventManager* Server::getEventManager()
{
	return m_event;
}
IRollbackReportSink* Server::getRollbackReportSink()
{
	if(!m_enable_rollback_recording)
		return NULL;
	if(!m_rollback_sink_enabled)
		return NULL;
	return m_rollback;
}

IWritableItemDefManager* Server::getWritableItemDefManager()
{
	return m_itemdef;
}
IWritableNodeDefManager* Server::getWritableNodeDefManager()
{
	return m_nodedef;
}
IWritableCraftDefManager* Server::getWritableCraftDefManager()
{
	return m_craftdef;
}

const ModSpec* Server::getModSpec(const std::string &modname)
{
	for(std::vector<ModSpec>::iterator i = m_mods.begin();
			i != m_mods.end(); i++){
		const ModSpec &mod = *i;
		if(mod.name == modname)
			return &mod;
	}
	return NULL;
}
void Server::getModNames(std::list<std::string> &modlist)
{
	for(std::vector<ModSpec>::iterator i = m_mods.begin(); i != m_mods.end(); i++)
	{
		modlist.push_back(i->name);
	}
}
std::string Server::getBuiltinLuaPath()
{
	return porting::path_share + DIR_DELIM + "builtin";
}

v3f findSpawnPos(ServerMap &map)
{
	//return v3f(50,50,50)*BS;

	v3s16 nodepos;

#if 0
	nodepos = v2s16(0,0);
	groundheight = 20;
#endif

#if 1
	s16 water_level = map.getWaterLevel();

	// Try to find a good place a few times
	for(s32 i=0; i<1000; i++)
	{
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
		bool is_good = false;
		s32 air_count = 0;
		for (s32 i = 0; i < 10; i++) {
			v3s16 blockpos = getNodeBlockPos(nodepos);
			map.emergeBlock(blockpos, true);
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
		if(is_good){
			// Found a good place
			//infostream<<"Searched through "<<i<<" places."<<std::endl;
			break;
		}
	}
#endif

	return intToFloat(nodepos, BS);
}

PlayerSAO* Server::emergePlayer(const char *name, u16 peer_id)
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

	// Load player if it isn't already loaded
	if (!player) {
		player = static_cast<RemotePlayer*>(m_env->loadPlayer(name));
	}

	// Create player if it doesn't exist
	if (!player) {
		newplayer = true;
		player = new RemotePlayer(this);
		player->updateName(name);
		/* Set player position */
		infostream<<"Server: Finding spawn place for player \""
				<<name<<"\""<<std::endl;
		v3f pos = findSpawnPos(m_env->getServerMap());
		player->setPosition(pos);

		/* Add player to environment */
		m_env->addPlayer(player);
	}

	// Create a new player active object
	PlayerSAO *playersao = new PlayerSAO(m_env, player, peer_id,
			getPlayerEffectivePrivs(player->getName()),
			isSingleplayer());

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
#if USE_CURL
			if(g_settings->getBool("server_announce") == true)
				ServerList::sendAnnounce("delete");
#endif
			break;
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


