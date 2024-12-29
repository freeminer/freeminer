/*
Copyright (C) 2024 proller <proler@gmail.com>
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

#include "fm_server.h"

#include <chrono>
#include <cstddef>
#include <cstdint>
#include <unistd.h>
#include <unordered_map>
#include <utility>
#include "database/database.h"
#include "emerge.h"
#include "filesys.h"
#include "irrlichttypes.h"
#include "porting.h"
#include "fm_world_merge.h"
#include "irrTypes.h"
#include "irr_v3d.h"
#include "log.h"
#include "map.h"
#include "mapblock.h"
#include "mapnode.h"
#include "network/fm_networkprotocol.h"
#include "profiler.h"
#include "server.h"
#include "debug/stacktrace.h"
#include "util/timetaker.h"

ServerThreadBase::ServerThreadBase(Server *server, const std::string &name,
		int priority) : thread_vector{name, 2}, m_server{server}
{
}

void *ServerThreadBase::run()
{
	// If something wrong with init order
	std::this_thread::sleep_for(std::chrono::milliseconds(sleep_start));

	BEGIN_DEBUG_EXCEPTION_HANDLER

	auto time_last = porting::getTimeMs();

	while (!stopRequested()) {
		try {
			const auto time_now = porting::getTimeMs();
			const auto result = step((time_now - time_last) / 1000.0);
			time_last = time_now;
			std::this_thread::sleep_for(
					std::chrono::milliseconds(result ? sleep_result : sleep_nothing));
#if !EXCEPTION_DEBUG
		} catch (const std::exception &e) {
			errorstream << m_name << ": exception: " << e.what() << std::endl
						<< stacktrace() << std::endl;
		} catch (...) {
			errorstream << m_name << ": Unknown unhandled exception at "
						<< __PRETTY_FUNCTION__ << ":" << __LINE__ << std::endl
						<< stacktrace() << std::endl;
#else
		} catch (int) { // nothing
#endif
		}
	}
	END_DEBUG_EXCEPTION_HANDLER
	return nullptr;
}

ServerThread::ServerThread(Server *server) : thread_vector{"Server", 40}, m_server{server}
{
}

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
				if (!m_server->Receive(sleep / 1000.0)) {
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
			errorstream << m_name << ": Unknown unhandled exception at "
						<< __PRETTY_FUNCTION__ << ":" << __LINE__ << std::endl
						<< stacktrace() << std::endl;
#endif
		}
	}

	END_DEBUG_EXCEPTION_HANDLER

	return NULL;
}
MapThread::MapThread(Server *server) : thread_vector("Map", 15), m_server(server)
{
}

void *MapThread::run()
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
			errorstream << m_name << ": Unknown unhandled exception at "
						<< __PRETTY_FUNCTION__ << ":" << __LINE__ << std::endl
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

SendFarBlocksThread::SendFarBlocksThread(Server *server) :
		ServerThreadBase(server, "SendFarBlocks", 1)
{
}

size_t SendFarBlocksThread::step(float dtime)
{
	return m_server->SendFarBlocks(dtime);
}

SendBlocksThread::SendBlocksThread(Server *server) :
		ServerThreadBase{server, "SendBlocks", 30}
{
	sleep_start = 100;
	sleep_result = 5;
	sleep_nothing = 200;
}

size_t SendBlocksThread::step(float dtime)
{
	return m_server->SendBlocks(dtime);
}

LiquidThread::LiquidThread(Server *server) : thread_vector{"Liquid", 4}, m_server{server}
{
}

void *LiquidThread::run()
{
	BEGIN_DEBUG_EXCEPTION_HANDLER

	unsigned int max_cycle_ms = 1000;
	while (!stopRequested()) {
		try {
			const auto time_start = porting::getTimeMs();
			m_server->getEnv().getMap().getBlockCacheFlush();
			std::map<v3bpos_t, MapBlock *> modified_blocks; // not used by fm
			const auto processed = m_server->getEnv().getServerMap().transformLiquids(
					modified_blocks, &m_server->getEnv(), m_server, max_cycle_ms);
			const auto time_spend = porting::getTimeMs() - time_start;

			thread_local const auto static liquid_step =
					g_settings->getBool("liquid_step");
			const auto sleep = (processed < 10 ? 100 : 3) +
							   (time_spend > liquid_step ? 1 : liquid_step - time_spend);
			std::this_thread::sleep_for(std::chrono::milliseconds(sleep));

#if !EXCEPTION_DEBUG
		} catch (const std::exception &e) {
			errorstream << m_name << ": exception: " << e.what() << std::endl
						<< stacktrace() << std::endl;
		} catch (...) {
			errorstream << m_name << ": Unknown unhandled exception at "
						<< __PRETTY_FUNCTION__ << ":" << __LINE__ << std::endl
						<< stacktrace() << std::endl;
#else
		} catch (int) { // nothing
#endif
		}
	}
	END_DEBUG_EXCEPTION_HANDLER
	return nullptr;
}

EnvThread::EnvThread(Server *server) : thread_vector{"Env", 20}, m_server{server}
{
}

void *EnvThread::run()
{
	unsigned int max_cycle_ms = 1000;
	auto time = porting::getTimeMs();
	while (!stopRequested()) {
		try {
			m_server->getEnv().getMap().getBlockCacheFlush();
			auto ctime = porting::getTimeMs();
			auto dtimems = ctime - time;
			time = ctime;
			m_server->getEnv().step(
					dtimems / 1000.0f, m_server->m_uptime_counter->get(), max_cycle_ms);
			std::this_thread::sleep_for(
					std::chrono::milliseconds(dtimems > 100 ? 1 : 100 - dtimems));
#if !EXCEPTION_DEBUG
		} catch (const std::exception &e) {
			errorstream << m_name << ": exception: " << e.what() << std::endl
						<< stacktrace() << std::endl;
		} catch (...) {
			errorstream << m_name << ": Unknown unhandled exception at "
						<< __PRETTY_FUNCTION__ << ":" << __LINE__ << std::endl
						<< stacktrace() << std::endl;
#else
		} catch (int) { // nothing
#endif
		}
	}
	return nullptr;
}

AbmThread::AbmThread(Server *server) : thread_vector{"Abm", 20}, m_server{server}
{
}

void *AbmThread::run()
{
	BEGIN_DEBUG_EXCEPTION_HANDLER

	unsigned int max_cycle_ms = 10000;
	auto time = porting::getTimeMs();
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
			errorstream << m_name << ": exception: " << e.what() << '\n'
						<< stacktrace() << '\n';
		} catch (...) {
			errorstream << m_name << ": Unknown unhandled exception at "
						<< __PRETTY_FUNCTION__ << ":" << __LINE__ << std::endl
						<< stacktrace() << '\n';
#else
		} catch (int) { // nothing
#endif
		}
	}
	END_DEBUG_EXCEPTION_HANDLER
	return nullptr;
}

int Server::AsyncRunMapStep(float dtime, float dedicated_server_step, bool async)
{
	TimeTaker timer_step("Server map step");
	g_profiler->add("Server::AsyncRunMapStep (num)", 1);

	int ret = 0;

	m_env->getServerMap().time_life = m_uptime_counter->get() + m_env->m_game_time_start;

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
	m_env->blocks_with_abm.close();
	stat.close();
	actionstream << "Server: Starting maintenance: bases closed now." << std::endl;
};

void Server::maintenance_end()
{
	// fmtodo:m_env->getServerMap().dbase->open();
	stat.open();
	m_env->blocks_with_abm.open();
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
	if (!m_key_value_storage.contains(name)) {
		m_key_value_storage.emplace(std::piecewise_construct, std::forward_as_tuple(name),
				std::forward_as_tuple(getGameDef()->m_path_world, name));
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

void Server::handleCommand_InitFm(NetworkPacket *pkt)
{
	if (!pkt->packet_unpack())
		return;
	auto &packet = *(pkt->packet);

	session_t peer_id = pkt->getPeerId();
	RemoteClient *client = getClient(peer_id, CS_Created);
	packet[TOSERVER_INIT_FM_VERSION].convert(client->net_proto_version_fm);
}

void Server::handleCommand_Drawcontrol(NetworkPacket *pkt)
{
	const auto peer_id = pkt->getPeerId();
	if (!pkt->packet) {
		if (!pkt->packet_unpack()) {
			return;
		}
	}
	auto &packet = *(pkt->packet);
	/*
	auto player = m_env->getPlayer(pkt->getPeerId());
	if (!player) {
		//m_con->DisconnectPeer(pkt->getPeerId());
		return;
	}
	*/

	//auto playersao = player->getPlayerSAO();
	/*
	if (!playersao) {
		m_con.DisconnectPeer(pkt->getPeerId());
		return;
	}*/

	auto client = getClientNoEx(peer_id, CS_Created);
	if (!client) {
		return;
	}
	{
		const auto lock = client->lock_unique_rec();
		if (packet.contains(TOSERVER_DRAWCONTROL_WANTED_RANGE))
			client->wanted_range =
					packet[TOSERVER_DRAWCONTROL_WANTED_RANGE].as<uint32_t>();
		if (packet.contains(TOSERVER_DRAWCONTROL_RANGE_ALL))
			client->range_all = packet[TOSERVER_DRAWCONTROL_RANGE_ALL].as<bool>();
		if (packet.contains(TOSERVER_DRAWCONTROL_FARMESH))
			client->farmesh = packet[TOSERVER_DRAWCONTROL_FARMESH].as<uint32_t>();
		//client->lodmesh = packet[TOSERVER_DRAWCONTROL_LODMESH].as<u32>();
		if (packet.contains(TOSERVER_DRAWCONTROL_FARMESH_QUALITY)) {
			client->farmesh_quality =
					packet[TOSERVER_DRAWCONTROL_FARMESH_QUALITY].as<uint8_t>();
			client->have_farmesh_quality = true;
		}
		if (packet.contains(TOSERVER_DRAWCONTROL_FARMESH_ALL_CHANGED)) {
			client->farmesh_all_changed =
					packet[TOSERVER_DRAWCONTROL_FARMESH_ALL_CHANGED].as<pos_t>();
		}
	}
	//client->block_overflow = packet[TOSERVER_DRAWCONTROL_BLOCK_OVERFLOW].as<bool>();

	// minetest compat, fmtodo: make one place
	/*
	if (playersao) {
		playersao->setFov(client->fov);
		playersao->setWantedRange(client->wanted_range/MAPBLOCK_SIZE);
	
	}
	*/
}

void Server::handleCommand_GetBlocks(NetworkPacket *pkt)
{
	if (!pkt->packet_unpack())
		return;
	auto client = m_clients.getClient(pkt->getPeerId());
	if (!client)
		return;
	auto &packet = *(pkt->packet);
	WITH_UNIQUE_LOCK(client->far_blocks_requested_mutex)
	{
		ServerMap::far_blocks_req_t blocks;
		packet[TOSERVER_GET_BLOCKS_BLOCKS].convert(blocks);
		for (const auto &[bpos, step_iteration] : blocks) {
			const auto &[step, iteation] = step_iteration;
			if (step >= FARMESH_STEP_MAX - 1) {
				continue;
			}
			if (client->far_blocks_requested.size() < step) {
				client->far_blocks_requested.resize(step);
			}
			client->far_blocks_requested[step][bpos].first = step;
			client->far_blocks_requested[step][bpos].second = iteation;
		}
	}
}

MapDatabase *GetFarDatabase(MapDatabase *dbase, ServerMap::far_dbases_t &far_dbases,
		const std::string &savedir, block_step_t step)
{
	if (step <= 0) {
		if (dbase) {
			return dbase;
		}
	}

	if (step >= far_dbases.size()) {
		return {};
	}

	if (const auto dbase = far_dbases[step].get()) {
		return dbase;
	}

	if (savedir.empty()) {
		// Only with enable_local_map_saving on local game
		// errorstream << "No path for save database with step " << (short)step << "\n";
		return {};
	}

	// Determine which database backend to use
	std::string conf_path = savedir + DIR_DELIM + "world.mt";
	Settings conf;
	bool succeeded = conf.readConfigFile(conf_path.c_str());
	if (!succeeded || !conf.exists("backend")) {
// fall back to sqlite3
#if USE_LEVELDB
		conf.set("backend", "leveldb");
#elif USE_SQLITE3
		conf.set("backend", "sqlite3");
#elif USE_REDIS
		conf.set("backend", "redis");
#endif
	}
	std::string backend = conf.get("backend");
	auto path = savedir + DIR_DELIM + "merge_" + std::to_string(step);

	if (!fs::PathExists(path)) {
		fs::CreateDir(path);
	}

	far_dbases[step].reset(ServerMap::createDatabase(backend, path, conf));
	return far_dbases[step].get();
};

MapBlockPtr loadBlockNoStore(Map *smap, MapDatabase *dbase, const v3bpos_t &bpos)
{
	try {
		MapBlockPtr block{smap->createBlankBlockNoInsert(bpos)};
		std::string blob;
		dbase->loadBlock(bpos, &blob);
		if (!blob.length()) {
			return {};
		}

		std::istringstream is(blob, std::ios_base::binary);

		u8 version = SER_FMT_VER_INVALID;
		is.read((char *)&version, 1);

		if (is.fail()) {
			return {};
		}

		// Read basic data
		if (!block->deSerialize(is, version, true)) {
			return {};
		}
		return block;
	} catch (const std::exception &ex) {
		errorstream << "Block load fail " << bpos << " : " << ex.what() << "\n";
	}
	return {};
}

void Server::SendBlockFm(session_t peer_id, MapBlockPtr block, u8 ver,
		u16 net_proto_version, SerializedBlockCache *cache)
{
	thread_local const int net_compression_level =
			rangelim(g_settings->getS16("map_compression_level_net"), -1, 9);

	g_profiler->add("Connection: blocks sent", 1);

	MSGPACK_PACKET_INIT((int)TOCLIENT_BLOCKDATA_FM, 8);
	PACK(TOCLIENT_BLOCKDATA_POS, block->getPos());

	std::ostringstream os(std::ios_base::binary);
	block->serialize(os, ver, false, net_compression_level, net_proto_version >= 1);
	block->serializeNetworkSpecific(os);

	PACK(TOCLIENT_BLOCKDATA_DATA, os.str());
	PACK(TOCLIENT_BLOCKDATA_HEAT, (s16)(block->heat + block->heat_add));
	PACK(TOCLIENT_BLOCKDATA_HUMIDITY, (s16)(block->humidity + block->humidity_add));
	PACK(TOCLIENT_BLOCKDATA_STEP, block->far_step);
	PACK(TOCLIENT_BLOCKDATA_CONTENT_ONLY,
			block->content_only.load(std::memory_order_relaxed));

	PACK(TOCLIENT_BLOCKDATA_CONTENT_ONLY_PARAM1, block->content_only_param1);
	PACK(TOCLIENT_BLOCKDATA_CONTENT_ONLY_PARAM2, block->content_only_param2);

	NetworkPacket pkt(TOCLIENT_BLOCKDATA_FM, buffer.size(), peer_id);
	pkt.putLongString({buffer.data(), buffer.size()});
	auto s = std::string{pkt.getString(0), pkt.getSize()};
	Send(&pkt);
}

uint32_t Server::SendFarBlocks(float dtime)
{
	ScopeProfiler sp(g_profiler, "Server: Far blocks send");
	uint32_t sent{};
	for (const auto &client : m_clients.getClientList()) {
		if (!client.second)
			continue;
		sent += client.second->SendFarBlocks();
	}
	return sent;
}

WorldMergeThread::WorldMergeThread(Server *server) :
		thread_vector("WorldMerge", 20), m_server(server)
{
}

void *WorldMergeThread::run()
{
	BEGIN_DEBUG_EXCEPTION_HANDLER

	u64 world_merge = 1;
	g_settings->getU64NoEx("world_merge", world_merge);
	if (!world_merge) {
		return {};
	}

	std::this_thread::sleep_for(std::chrono::seconds(3));

	WorldMerger merger{
			.stop_func{[this]() { return stopRequested(); }},
			.throttle_func{[&]() {
				return (m_server->getEnv().getPlayerCount() >
						merger.world_merge_max_clients);
			}},
			.get_time_func{[this]() { return m_server->getEnv().getGameTime(); }},
			.ndef{m_server->getNodeDefManager()},
			.smap{m_server->getEnv().m_map.get()},
			.far_dbases{m_server->far_dbases},
			.dbase{m_server->getEnv().m_map->m_db.dbase},
			.save_dir{m_server->getEnv().m_map->m_savedir},
	};

	{
		g_settings->getU32NoEx("world_merge_throttle", merger.world_merge_throttle);
		merger.world_merge_max_clients = m_server->isSingleplayer() ? 1 : 0;
		g_settings->getU32NoEx("world_merge_max_clients", merger.world_merge_max_clients);
		g_settings->getU32NoEx("world_merge_lazy_up", merger.lazy_up);

		{
			merger.world_merge_load_all = -1;
			g_settings->getS16NoEx("world_merge_load_all", merger.world_merge_load_all);
			merger.world_merge_throttle = m_server->isSingleplayer() ? 10 : 0;
			uint64_t world_merge_all = 0;
			g_settings->getU64NoEx("world_merge_all", world_merge_all);
			if (world_merge_all) {
				merger.merge_all();
			}
		}
	}
	merger.world_merge_load_all = 0;
	merger.partial = true;

	// Minimum blocks changed for periodic merge
	uint64_t world_merge_min = 100;
	g_settings->getU64NoEx("world_merge_min", world_merge_min);

	while (!stopRequested()) {
		if (merger.throttle()) {
			tracestream << "World merge wait" << '\n';
			sleep(10);
			continue;
		}
		if (merger.merge_server_diff(
					m_server->getEnv().getServerMap().changed_blocks_for_merge,
					world_merge_min)) {
			break;
		}

		sleep(60);
	}

	{
		// unbreakable at max speed
		merger.stop_func = {};
		merger.throttle_func = {};
		merger.world_merge_throttle = 0;

		if (!m_server->getEnv().getServerMap().changed_blocks_for_merge.empty()) {
			actionstream
					<< "Merge last changed blocks "
					<< m_server->getEnv().getServerMap().changed_blocks_for_merge.size()
					<< "\n";
		}
		merger.merge_server_diff(
				m_server->getEnv().getServerMap().changed_blocks_for_merge);
	}

	END_DEBUG_EXCEPTION_HANDLER;
	return {};
}

void Server::SetBlocksNotSent()
{
	std::vector<session_t> clients = m_clients.getClientIDs();
	ClientInterface::AutoLock clientlock(m_clients);
	// Set the modified blocks unsent for all the clients
	for (const session_t client_id : clients) {
		if (RemoteClient *client = m_clients.lockedGetClientNoEx(client_id))
			client->SetBlocksNotSent(/*block*/);
	}
}
