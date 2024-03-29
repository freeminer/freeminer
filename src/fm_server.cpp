#include <cstdint>
#include "database/database.h"
#include "emerge.h"
#include "irr_v3d.h"
#include "profiler.h"
#include "server.h"
#include "debug/stacktrace.h"
#include "util/string.h"
#include "util/timetaker.h"

class ServerThread : public thread_pool
{
public:
	ServerThread(Server *server) : thread_pool("Server", 40), m_server(server) {}

	void *run();

private:
	Server *m_server;
};

void *ServerThread::run()
{
	BEGIN_DEBUG_EXCEPTION_HANDLER

	f32 dedicated_server_step = g_settings->getFloat("dedicated_server_step");
	try {
		m_server->AsyncRunStep(0.1, true);
	} catch (const con::ConnectionBindFailed &e) {
		m_server->setAsyncFatalError(e.what());
	} catch (const LuaError &e) {
		m_server->setAsyncFatalError(e);
	} catch (const std::exception &e) {
		errorstream << m_name << ": exception: " << e.what() << std::endl;
	}

	auto time = porting::getTimeMs();
	while (!stopRequested()) {
		try {
			m_server->getEnv().getMap().getBlockCacheFlush();
			const auto time_now = porting::getTimeMs();
			{
				TimeTaker timer("Server AsyncRunStep()");
				m_server->AsyncRunStep((time_now - time) / 1000.0f);
			}
			time = time_now;

			/*
			TimeTaker timer("Server Receive()");
			*/
			// Loop used only when 100% cpu load or on old slow hardware.
			// usually only one packet recieved here
			auto end_ms = porting::getTimeMs();
			int64_t sleep = (1000 * dedicated_server_step) - (end_ms - time_now);
			auto sleep_min = m_server->overload ? 1000 : 50;
			if (sleep < sleep_min)
				sleep = sleep_min;
			end_ms += sleep; // u32(1000 * dedicated_server_step/2);
			for (u16 i = 0; i < 1000; ++i) {
				if (!m_server->Receive(sleep)) {
					// errorstream<<"Server: Recieve nothing="  << i << "
					// per="<<porting::getTimeMs()-(end_ms-sleep)<<"
					// sleep="<<sleep<<std::endl;
					break;
				}
				if (i > 50 && porting::getTimeMs() > end_ms) {
					// verbosestream<<"Server: Recieve queue overloaded: processed="  << i
					// << " per="<<porting::getTimeMs()-(end_ms-sleep)<<" sleep="<<sleep
					// << " eventssize=" << m_server->m_con->events_size()<<std::endl;
					break;
				}
			}
			auto events = m_server->m_con->events_size();
			if (events) {
				g_profiler->add("Server: Queue", events);
			}
			if (events > 500) {
				if (!m_server->overload)
					errorstream << "Server: Enabling overload mode queue=" << events
								<< "\n";
				if (m_server->overload < events)
					m_server->overload = events;
			} else {
				if (m_server->overload)
					errorstream << "Server: Disabling overload mode queue=" << events
								<< "\n";
				m_server->overload = 0;
			}
		} catch (const con::PeerNotFoundException &e) {
			infostream << "Server: PeerNotFoundException" << std::endl;
		} catch (const ClientNotFoundException &e) {
		} catch (const con::ConnectionBindFailed &e) {
			m_server->setAsyncFatalError(e.what());
#if !EXCEPTION_DEBUG
		} catch (const LuaError &e) {
			m_server->setAsyncFatalError("Lua: " + std::string(e.what()));
		} catch (const std::exception &e) {
			errorstream << m_name << ": exception: " << e.what() << std::endl
						<< stacktrace() << std::endl;
		} catch (...) {
			errorstream << m_name << ": Unknown unhandled exception at " << __PRETTY_FUNCTION__ << ":" << __LINE__ << std::endl
						<< stacktrace() << std::endl;
#endif
		}
	}

	END_DEBUG_EXCEPTION_HANDLER

	return NULL;
}

class MapThread : public thread_pool
{
	Server *m_server;

public:
	MapThread(Server *server) : thread_pool("Map", 15), m_server(server) {}

	void *run()
	{
		auto time = porting::getTimeMs();
		while (!stopRequested()) {
			auto time_now = porting::getTimeMs();
			try {
				m_server->getEnv().getMap().getBlockCacheFlush();
				if (!m_server->AsyncRunMapStep((time_now - time) / 1000.0f, 1))
					std::this_thread::sleep_for(std::chrono::milliseconds(200));
				else
					std::this_thread::sleep_for(std::chrono::milliseconds(10));
#if !EXCEPTION_DEBUG
			} catch (const std::exception &e) {
				errorstream << m_name << ": exception: " << e.what() << std::endl
							<< stacktrace() << std::endl;
			} catch (...) {
				errorstream << m_name << ": Unknown unhandled exception at " << __PRETTY_FUNCTION__ << ":" << __LINE__ << std::endl
							<< stacktrace() << std::endl;
#else
			} catch (int) { // nothing
#endif
			}
			time = time_now;
		}
		// END_DEBUG_EXCEPTION_HANDLER
		return nullptr;
	}
};

class SendBlocksThread : public thread_pool
{
	Server *m_server;

public:
	SendBlocksThread(Server *server) : thread_pool("SendBlocks", 30), m_server(server) {}

	void *run()
	{
		BEGIN_DEBUG_EXCEPTION_HANDLER

		auto time = porting::getTimeMs();
		while (!stopRequested()) {
			// infostream<<"S run d="<<m_server->m_step_dtime<< "
			// myt="<<(porting::getTimeMs() - time)/1000.0f<<std::endl;
			try {
				m_server->getEnv().getMap().getBlockCacheFlush();
				auto time_now = porting::getTimeMs();
				auto sent = m_server->SendBlocks((time_now - time) / 1000.0f);
				time = time_now;
				std::this_thread::sleep_for(std::chrono::milliseconds(sent ? 5 : 100));
#if !EXCEPTION_DEBUG
			} catch (const std::exception &e) {
				errorstream << m_name << ": exception: " << e.what() << std::endl
							<< stacktrace() << std::endl;
			} catch (...) {
				errorstream << m_name << ": Unknown unhandled exception at " << __PRETTY_FUNCTION__ << ":" << __LINE__ << std::endl
							<< stacktrace() << std::endl;
#else
			} catch (int) { // nothing
#endif
			}
		}
		END_DEBUG_EXCEPTION_HANDLER
		return nullptr;
	}
};

class LiquidThread : public thread_pool
{
	Server *m_server;

public:
	LiquidThread(Server *server) : thread_pool("Liquid", 4), m_server(server) {}

	void *run()
	{
		BEGIN_DEBUG_EXCEPTION_HANDLER

		unsigned int max_cycle_ms = 1000;
		while (!stopRequested()) {
			try {
				m_server->getEnv().getMap().getBlockCacheFlush();
				auto time_start = porting::getTimeMs();
				m_server->getEnv().getMap().getBlockCacheFlush();
				std::map<v3bpos_t, MapBlock *> modified_blocks; // not used by fm
				m_server->getEnv().getServerMap().transformLiquids(
						modified_blocks, &m_server->getEnv(), m_server, max_cycle_ms);
				auto time_spend = porting::getTimeMs() - time_start;
				std::this_thread::sleep_for(std::chrono::milliseconds(
						time_spend > 300 ? 1 : 300 - time_spend));

#if !EXCEPTION_DEBUG
			} catch (const std::exception &e) {
				errorstream << m_name << ": exception: " << e.what() << std::endl
							<< stacktrace() << std::endl;
			} catch (...) {
				errorstream << m_name << ": Unknown unhandled exception at " << __PRETTY_FUNCTION__ << ":" << __LINE__ << std::endl
							<< stacktrace() << std::endl;
#else
			} catch (int) { // nothing
#endif
			}
		}
		END_DEBUG_EXCEPTION_HANDLER
		return nullptr;
	}
};

class EnvThread : public thread_pool
{
	Server *m_server;

public:
	EnvThread(Server *server) : thread_pool("Env", 20), m_server(server) {}

	void *run()
	{
		unsigned int max_cycle_ms = 1000;
		auto time = porting::getTimeMs();
		while (!stopRequested()) {
			try {
				m_server->getEnv().getMap().getBlockCacheFlush();
				auto ctime = porting::getTimeMs();
				auto dtimems = ctime - time;
				time = ctime;
				m_server->getEnv().step(dtimems / 1000.0f,
						m_server->m_uptime_counter->get(), max_cycle_ms);
				std::this_thread::sleep_for(
						std::chrono::milliseconds(dtimems > 100 ? 1 : 100 - dtimems));
#if !EXCEPTION_DEBUG
			} catch (const std::exception &e) {
				errorstream << m_name << ": exception: " << e.what() << std::endl
							<< stacktrace() << std::endl;
			} catch (...) {
				errorstream << m_name << ": Unknown unhandled exception at " << __PRETTY_FUNCTION__ << ":" << __LINE__ << std::endl
							<< stacktrace() << std::endl;
#else
			} catch (int) { // nothing
#endif
			}
		}
		return nullptr;
	}
};

class AbmThread : public thread_pool
{
	Server *m_server;

public:
	AbmThread(Server *server) : thread_pool("Abm", 20), m_server(server) {}

	void *run()
	{
		BEGIN_DEBUG_EXCEPTION_HANDLER

		unsigned int max_cycle_ms = 10000;
		unsigned int time = porting::getTimeMs();
		while (!stopRequested()) {
			try {
				auto ctime = porting::getTimeMs();
				auto dtimems = ctime - time;
				time = ctime;
				m_server->getEnv().analyzeBlocks(dtimems / 1000.0f, max_cycle_ms);
				std::this_thread::sleep_for(
						std::chrono::milliseconds(dtimems > 1000 ? 100 : 1000 - dtimems));
#if !EXCEPTION_DEBUG
			} catch (const std::exception &e) {
				errorstream << m_name << ": exception: " << e.what() << std::endl
							<< stacktrace() << std::endl;
			} catch (...) {
				errorstream << m_name << ": Unknown unhandled exception at " << __PRETTY_FUNCTION__ << ":" << __LINE__ << std::endl
							<< stacktrace() << std::endl;
#else
			} catch (int) { // nothing
#endif
			}
		}
		END_DEBUG_EXCEPTION_HANDLER
		return nullptr;
	}
};

int Server::AsyncRunMapStep(float dtime, float dedicated_server_step, bool async)
{
	TimeTaker timer_step("Server map step");
	g_profiler->add("Server::AsyncRunMapStep (num)", 1);

	int ret = 0;

	m_env->getMap().time_life = m_uptime_counter->get() + m_env->m_game_time_start;

	/*
		float dtime;
		{
			MutexAutoLock lock1(m_step_dtime_mutex);
			dtime = m_step_dtime;
		}
	*/

	u32 max_cycle_ms = 1000 * dedicated_server_step; // async ? 500 : 200;

	static const float map_timer_and_unload_dtime = 10.92;
	if (!maintenance_status &&
			m_map_timer_and_unload_interval.step(dtime, map_timer_and_unload_dtime)) {
		TimeTaker timer_step("Server step: Run Map's timers and unload unused data");
		// MutexAutoLock lock(m_env_mutex);
		//  Run Map's timers and unload unused data
		ScopeProfiler sp(g_profiler, "Server: map timer and unload");
		if (m_env->getMap().timerUpdate(m_uptime_counter->get(),
					g_settings->getFloat("server_unload_unused_data_timeout"), -1, {},
					max_cycle_ms)) {
			m_map_timer_and_unload_interval.run_next(map_timer_and_unload_dtime);
			++ret;
		}
	}

	/* Transform liquids */
	m_liquid_transform_timer += dtime;
	{
#if !ENABLE_THREADS
		auto lockmapl = m_env->getMap().m_nothread_locker.try_lock_unique_rec();
		if (lockmapl->owns_lock())
#endif
			if (!m_more_threads && m_liquid_transform_timer >= m_liquid_transform_every) {

				TimeTaker timer_step("Server step: liquid transform");
				m_liquid_transform_timer -= m_liquid_transform_every;
				if (m_liquid_transform_timer > m_liquid_transform_every * 2)
					m_liquid_transform_timer = 0;

				// MutexAutoLock lock(m_env_mutex);

				ScopeProfiler sp(g_profiler, "Server: liquid transform");

				// not all liquid was processed per step, forcing on next step
				std::map<v3bpos_t, MapBlock *> modified_blocks;
				if (m_env->getServerMap().transformLiquids(
							modified_blocks, m_env, this, max_cycle_ms) > 0) {
					m_liquid_transform_timer = m_liquid_transform_every /*  *0.8  */;
					++ret;
				}
			}
	}
	/*
		Set the modified blocks unsent for all the clients
	*/

	m_liquid_send_timer += dtime;
	if (m_liquid_send_timer >= m_liquid_send_interval) {
		// TimeTaker timer_step("Server step: updateLighting");
		m_liquid_send_timer -= m_liquid_send_interval;
		if (m_liquid_send_timer > m_liquid_send_interval * 2)
			m_liquid_send_timer = 0;

		// concurrent_map<v3POS, MapBlock*> modified_blocks; //not used
		// if (m_env->getMap().updateLighting(m_env->getMap().lighting_modified_blocks,
		// modified_blocks, max_cycle_ms)) {
		if (m_env->getServerMap().updateLightingQueue(max_cycle_ms, ret)) {
			m_liquid_send_timer = m_liquid_send_interval;
			goto no_send;
		}
	}
no_send:

	ret += save(dtime, dedicated_server_step, true);

	return ret;
}

/* TODO
void Server::deleteDetachedInventory(const std::string &name) {
	if(m_detached_inventories.count(name) > 0) {
		infostream << "Server deleting detached inventory \"" << name << "\"" <<
std::endl; delete m_detached_inventories[name]; m_detached_inventories.erase(name);
	}
}
*/

void Server::maintenance_start()
{
	infostream << "Server: Starting maintenance: saving..." << std::endl;
	m_emerge->stopThreads();
	save(0.1);
	m_env->getServerMap().m_map_saving_enabled = false;
	m_env->getServerMap().m_map_loading_enabled = false;
	// fmtodo: m_env->getServerMap().dbase->close();
	m_env->m_key_value_storage.clear();
	stat.close();
	actionstream << "Server: Starting maintenance: bases closed now." << std::endl;
};

void Server::maintenance_end()
{
	// fmtodo:m_env->getServerMap().dbase->open();
	stat.open();
	m_env->getServerMap().m_map_saving_enabled = true;
	m_env->getServerMap().m_map_loading_enabled = true;
	m_emerge->startThreads();
	actionstream << "Server: Starting maintenance: ended." << std::endl;
};

#if MINETEST_PROTO
void Server::SendPunchPlayer(u16 peer_id, v3f speed)
{
}
#endif

KeyValueStorage &ServerEnvironment::getKeyValueStorage(std::string name)
{
	if (name.empty()) {
		name = "key_value_storage";
	}
	if (!m_key_value_storage.count(name)) {
		m_key_value_storage.emplace(std::piecewise_construct, std::forward_as_tuple(name),
				std::forward_as_tuple(m_path_world, name));
	}
	return m_key_value_storage.at(name);
}


void Server::SendFreeminerInit(session_t peer_id, u16 protocol_version)
{
	NetworkPacket pkt(TOCLIENT_FREEMINER_INIT, 0, peer_id);

	MSGPACK_PACKET_INIT((int)TOCLIENT_INIT_LEGACY, 4);

	Settings params;
	m_emerge->mgparams->MapgenParams::writeParams(&params);
	m_emerge->mgparams->writeParams(&params);
	PACK(TOCLIENT_INIT_MAP_PARAMS, params);
	PACK(TOCLIENT_INIT_GAMEID, m_gamespec.id);

	PACK(TOCLIENT_INIT_PROTOCOL_VERSION_FM, SERVER_PROTOCOL_VERSION_FM);

	PACK(TOCLIENT_INIT_WEATHER, g_settings->getBool("weather"));

	pkt.putLongString({buffer.data(), buffer.size()});

	verbosestream << "Server: Sending freeminer init to id(" << peer_id
				  << "): size=" << pkt.getSize() << std::endl;

	Send(&pkt);
}
