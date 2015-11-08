/*
emerge.cpp
Copyright (C) 2010-2013 celeron55, Perttu Ahola <celeron55@gmail.com>
Copyright (C) 2010-2013 kwolekr, Ryan Kwolek <kwolekr@minetest.net>
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

#include "emerge.h"

#include <iostream>
#include <queue>

#include "util/container.h"
#include "util/thread.h"
#include "threading/event.h"

#include "config.h"
#include "constants.h"
#include "environment.h"
#include "log_types.h"
#include "map.h"
#include "mapblock.h"
#include "mapgen_fractal.h"
#include "mapgen_v5.h"
#include "mapgen_v6.h"
#include "mapgen_v7.h"
#include "mapgen_indev.h"
#include "mapgen_singlenode.h"
#include "mapgen_math.h"
#include "util/thread_pool.h"
#include "mg_biome.h"
#include "mg_ore.h"
#include "mg_decoration.h"
#include "mg_schematic.h"
#include "nodedef.h"
#include "profiler.h"
#include "scripting_game.h"
#include "server.h"
#include "serverobject.h"
#include "settings.h"
#include "voxel.h"


struct MapgenDesc {
	const char *name;
	MapgenFactory *factory;
	bool is_user_visible;
};

class EmergeThread : public thread_pool {
public:
	bool enable_mapgen_debug_info;
	int id;

	EmergeThread(Server *server, int ethreadid);
	~EmergeThread();

	void *run();
	void signal();

	// Requires queue mutex held
	bool pushBlock(v3s16 pos);

	void cancelPendingItems();

	static void runCompletionCallbacks(
		v3s16 pos, EmergeAction action,
		const EmergeCallbackList &callbacks);

private:
	Server *m_server;
	ServerMap *m_map;
	EmergeManager *m_emerge;
	Mapgen *m_mapgen;

	Event m_queue_event;
	std::queue<v3s16> m_block_queue;

	bool popBlockEmerge(v3s16 *pos, BlockEmergeData *bedata);

	EmergeAction getBlockOrStartGen(
		v3s16 pos, bool allow_gen, MapBlock **block, BlockMakeData *data);
	MapBlock *finishGen(v3s16 pos, BlockMakeData *bmdata,
		std::map<v3s16, MapBlock *> *modified_blocks);

	friend class EmergeManager;
};

////
//// Built-in mapgens
////

MapgenDesc g_reg_mapgens[] = {
	{"indev",      new MapgenFactoryIndev,      true},
	{"v5",         new MapgenFactoryV5,         true},
	{"v6",         new MapgenFactoryV6,         true},
	{"v7",         new MapgenFactoryV7,         true},
	{"math",       new MapgenFactoryMath,       true},
	{"fractal",    new MapgenFactoryFractal,    false},
	{"singlenode", new MapgenFactorySinglenode, false},
};

////
//// EmergeManager
////

EmergeManager::EmergeManager(IGameDef *gamedef)
{
	this->ndef      = gamedef->getNodeDefManager();
	this->biomemgr  = new BiomeManager(gamedef);
	this->oremgr    = new OreManager(gamedef);
	this->decomgr   = new DecorationManager(gamedef);
	this->schemmgr  = new SchematicManager(gamedef);
	this->gen_notify_on = 0;

	// Note that accesses to this variable are not synchronized.
	// This is because the *only* thread ever starting or stopping
	// EmergeThreads should be the ServerThread.
	this->m_threads_active = false;

	enable_mapgen_debug_info = g_settings->getBool("enable_mapgen_debug_info");

	// If unspecified, leave a proc for the main thread and one for
	// some other misc thread
	s16 nthreads = 0;
	if (!g_settings->getS16NoEx("num_emerge_threads", nthreads))
	{}
#if ENABLE_THREADS
	if (nthreads < 1)
		nthreads = Thread::getNumberOfProcessors() - 2;
#endif
	if (nthreads < 1)
		nthreads = 1;

	m_qlimit_total = g_settings->getU16("emergequeue_limit_total");
	if (!g_settings->getU16NoEx("emergequeue_limit_diskonly", m_qlimit_diskonly))
		{ }
	if (!g_settings->getU16NoEx("emergequeue_limit_generate", m_qlimit_generate))
		{ }
	//errorstream<<"==> qlimit_generate="<<qlimit_generate<<"  qlimit_diskonly="<<qlimit_diskonly<<" qlimit_total="<<qlimit_total<<std::endl;

	// don't trust user input for something very important like this
	if (m_qlimit_total < 1)
		m_qlimit_total = nthreads * 128;
	if (m_qlimit_diskonly < 1)
		m_qlimit_diskonly = nthreads * 100;
	if (m_qlimit_generate < 1)
		m_qlimit_generate = nthreads * 32;

	for (s16 i = 0; i < nthreads; i++)
		m_threads.push_back(new EmergeThread((Server *)gamedef, i));

	infostream << "EmergeManager: using " << nthreads << " threads" << std::endl;
}


EmergeManager::~EmergeManager()
{
	for (u32 i = 0; i != m_threads.size(); i++) {
		EmergeThread *thread = m_threads[i];

		if (m_threads_active) {
			thread->stop();
			thread->signal();
			thread->wait();
		}

		delete thread;
		delete m_mapgens[i];
	}

	delete biomemgr;
	delete oremgr;
	delete decomgr;
	delete schemmgr;

	delete params.sparams;
}


void EmergeManager::loadMapgenParams()
{
	params.load(*g_settings);

	biomemgr->mapgen_params = &params;
}


void EmergeManager::initMapgens()
{
	if (m_mapgens.size())
		return;

	MapgenFactory *mgfactory = getMapgenFactory(params.mg_name);
	if (!mgfactory) {
		errorstream << "EmergeManager: mapgen " << params.mg_name <<
			" not registered; falling back to " << DEFAULT_MAPGEN << std::endl;

		params.mg_name = DEFAULT_MAPGEN;

		mgfactory = getMapgenFactory(params.mg_name);
		FATAL_ERROR_IF(mgfactory == NULL, "Couldn't use any mapgen!");
	}

	if (!params.sparams) {
		params.sparams = mgfactory->createMapgenParams();
		params.sparams->readParams(g_settings);
	}

	for (u32 i = 0; i != m_threads.size(); i++) {
		Mapgen *mg = mgfactory->createMapgen(i, &params, this);
		if (!mg)
			continue;
		m_mapgens.push_back(mg);
	}
}


Mapgen *EmergeManager::getCurrentMapgen()
{
	for (u32 i = 0; i != m_threads.size(); i++) {
		if (m_threads[i]->isCurrentThread())
			return m_threads[i]->m_mapgen;
	}

	return NULL;
}


void EmergeManager::startThreads()
{
	if (m_threads_active)
		return;

	for (u32 i = 0; i != m_threads.size(); i++)
		m_threads[i]->start();

	m_threads_active = true;
}


void EmergeManager::stopThreads()
{
	if (!m_threads_active)
		return;

	// Request thread stop in parallel
	for (u32 i = 0; i != m_threads.size(); i++) {
		m_threads[i]->stop();
		m_threads[i]->signal();
	}

	// Then do the waiting for each
	for (u32 i = 0; i != m_threads.size(); i++)
		m_threads[i]->wait();

	m_threads_active = false;
}


bool EmergeManager::isRunning()
{
	return m_threads_active;
}


bool EmergeManager::enqueueBlockEmerge(
	u16 peer_id,
	v3s16 blockpos,
	bool allow_generate,
	bool ignore_queue_limits)
{
	u16 flags = 0;
	if (allow_generate)
		flags |= BLOCK_EMERGE_ALLOW_GEN;
	if (ignore_queue_limits)
		flags |= BLOCK_EMERGE_FORCE_QUEUE;

	return enqueueBlockEmergeEx(blockpos, peer_id, flags, NULL, NULL);
}


bool EmergeManager::enqueueBlockEmergeEx(
	v3s16 blockpos,
	u16 peer_id,
	u16 flags,
	EmergeCompletionCallback callback,
	void *callback_param)
{
	EmergeThread *thread = NULL;

	{
		MutexAutoLock queuelock(m_queue_mutex);

		if (!pushBlockEmergeData(blockpos, peer_id, flags,
				callback, callback_param))
			return false;

		thread = getOptimalThread();
		thread->pushBlock(blockpos);
	}

	thread->signal();

	return true;
}


//
// Mapgen-related helper functions
//

v3s16 EmergeManager::getContainingChunk(v3s16 blockpos)
{
	return getContainingChunk(blockpos, params.chunksize);
}


v3s16 EmergeManager::getContainingChunk(v3s16 blockpos, s16 chunksize)
{
	s16 coff = -chunksize / 2;
	v3s16 chunk_offset(coff, coff, coff);

	return getContainerPos(blockpos - chunk_offset, chunksize)
		* chunksize + chunk_offset;
}


int EmergeManager::getGroundLevelAtPoint(v2s16 p)
{
	if (m_mapgens.size() == 0 || !m_mapgens[0]) {
		errorstream << "EmergeManager: getGroundLevelAtPoint() called"
			" before mapgen init" << std::endl;
		return 0;
	}

	return m_mapgens[0]->getGroundLevelAtPoint(p);
}


bool EmergeManager::isBlockUnderground(v3s16 blockpos)
{
#if 0
	v2s16 p = v2s16((blockpos.X * MAP_BLOCKSIZE) + MAP_BLOCKSIZE / 2,
					(blockpos.Y * MAP_BLOCKSIZE) + MAP_BLOCKSIZE / 2);
	int ground_level = getGroundLevelAtPoint(p);
	return blockpos.Y * (MAP_BLOCKSIZE + 1) <= min(water_level, ground_level);
#endif

	// Use a simple heuristic; the above method is wildly inaccurate anyway.
	return blockpos.Y * (MAP_BLOCKSIZE + 1) <= params.water_level;
}


void EmergeManager::getMapgenNames(
	std::vector<const char *> *mgnames, bool include_hidden)
{
	for (u32 i = 0; i != ARRLEN(g_reg_mapgens); i++) {
		if (include_hidden || g_reg_mapgens[i].is_user_visible)
			mgnames->push_back(g_reg_mapgens[i].name);
	}
}


MapgenFactory *EmergeManager::getMapgenFactory(const std::string &mgname)
{
	for (u32 i = 0; i != ARRLEN(g_reg_mapgens); i++) {
		if (mgname == g_reg_mapgens[i].name)
			return g_reg_mapgens[i].factory;
	}

	return NULL;
}


bool EmergeManager::pushBlockEmergeData(
	v3s16 pos,
	u16 peer_requested,
	u16 flags,
	EmergeCompletionCallback callback,
	void *callback_param)
{
	u16 &count_peer = m_peer_queue_count[peer_requested];

	if ((flags & BLOCK_EMERGE_FORCE_QUEUE) == 0) {
		if (m_blocks_enqueued.size() >= m_qlimit_total)
			return false;

		if (peer_requested != PEER_ID_INEXISTENT) {
			u16 qlimit_peer = (flags & BLOCK_EMERGE_ALLOW_GEN) ?
				m_qlimit_generate : m_qlimit_diskonly;
			if (count_peer >= qlimit_peer)
				return false;
		}
	}

	std::pair<std::map<v3s16, BlockEmergeData>::iterator, bool> findres;
	findres = m_blocks_enqueued.insert(std::make_pair(pos, BlockEmergeData()));

	BlockEmergeData &bedata = findres.first->second;
	bool update_existing    = !findres.second;

	if (callback)
		bedata.callbacks.push_back(std::make_pair(callback, callback_param));

	if (update_existing) {
		bedata.flags |= flags;
	} else {
		bedata.flags = flags;
		bedata.peer_requested = peer_requested;

		count_peer++;
	}

	return true;
}


bool EmergeManager::popBlockEmergeData(
	v3s16 pos,
	BlockEmergeData *bedata)
{
	std::map<v3s16, BlockEmergeData>::iterator it;
	std::map<u16, u16>::iterator it2;

	it = m_blocks_enqueued.find(pos);
	if (it == m_blocks_enqueued.end())
		return false;

	*bedata = it->second;

	it2 = m_peer_queue_count.find(bedata->peer_requested);
	if (it2 == m_peer_queue_count.end())
		return false;

	u16 &count_peer = it2->second;
	assert(count_peer != 0);
	count_peer--;

	m_blocks_enqueued.erase(it);

	return true;
}


EmergeThread *EmergeManager::getOptimalThread()
{
	size_t nthreads = m_threads.size();

	FATAL_ERROR_IF(nthreads == 0, "No emerge threads!");

	size_t index = 0;
	size_t nitems_lowest = m_threads[0]->m_block_queue.size();

	for (size_t i = 1; i < nthreads; i++) {
		size_t nitems = m_threads[i]->m_block_queue.size();
		if (nitems < nitems_lowest) {
			index = i;
			nitems_lowest = nitems;
		}
	}

	return m_threads[index];
}


////
//// EmergeThread
////

EmergeThread::EmergeThread(Server *server, int ethreadid) :
	enable_mapgen_debug_info(false),
	id(ethreadid),
	m_server(server),
	m_map(NULL),
	m_emerge(NULL),
	m_mapgen(NULL)
{
	m_name = "Emerge-" + itos(ethreadid);
}


EmergeThread::~EmergeThread()
{
	//cancelPendingItems();
}


void EmergeThread::signal()
{
	m_queue_event.signal();
}


bool EmergeThread::pushBlock(v3s16 pos)
{
	m_block_queue.push(pos);
	return true;
}


void EmergeThread::cancelPendingItems()
{
	MutexAutoLock queuelock(m_emerge->m_queue_mutex);

	while (!m_block_queue.empty()) {
		BlockEmergeData bedata;
		v3s16 pos;

		pos = m_block_queue.front();
		m_block_queue.pop();

		m_emerge->popBlockEmergeData(pos, &bedata);

		runCompletionCallbacks(pos, EMERGE_CANCELLED, bedata.callbacks);
	}
}


void EmergeThread::runCompletionCallbacks(
	v3s16 pos,
	EmergeAction action,
	const EmergeCallbackList &callbacks)
{
	for (size_t i = 0; i != callbacks.size(); i++) {
		EmergeCompletionCallback callback;
		void *param;

		callback = callbacks[i].first;
		param    = callbacks[i].second;

		callback(pos, action, param);
	}
}


bool EmergeThread::popBlockEmerge(v3s16 *pos, BlockEmergeData *bedata)
{
	MutexAutoLock queuelock(m_emerge->m_queue_mutex);

	if (m_block_queue.empty())
		return false;

	*pos = m_block_queue.front();
	m_block_queue.pop();

	m_emerge->popBlockEmergeData(*pos, bedata);

	return true;
}




EmergeAction EmergeThread::getBlockOrStartGen(
	v3s16 pos, bool allow_gen, MapBlock **block, BlockMakeData *bmdata)
{
	//MutexAutoLock envlock(m_server->m_env_mutex);

	{
	MAP_NOTHREAD_LOCK(m_map);
	// 1). Attempt to fetch block from memory
	*block = m_map->getBlockNoCreateNoEx(pos);
	}
	if (*block && !(*block)->isDummy() && (*block)->isGenerated())
		return EMERGE_FROM_MEMORY;

	{
	MAP_NOTHREAD_LOCK(m_map);
	// 2). Attempt to load block from disk
	*block = m_map->loadBlock(pos);
	}

	if (*block && (*block)->isGenerated())
	{
		MAP_NOTHREAD_LOCK(m_map);
		m_map->prepareBlock(*block);
		return EMERGE_FROM_DISK;
	}

	{
	MAP_NOTHREAD_LOCK(m_map);
	// 3). Attempt to start generation
	if (allow_gen && m_map->initBlockMake(pos, bmdata))
		return EMERGE_GENERATED;
	}
	// All attempts failed; cancel this block emerge
	return EMERGE_CANCELLED;
}


MapBlock *EmergeThread::finishGen(v3s16 pos, BlockMakeData *bmdata,
	std::map<v3s16, MapBlock *> *modified_blocks)
{
	//MutexAutoLock envlock(m_server->m_env_mutex);
	ScopeProfiler sp(g_profiler,
		"EmergeThread: after Mapgen::makeChunk", SPT_AVG);

	/*
		Perform post-processing on blocks (invalidate lighting, queue liquid
		transforms, etc.) to finish block make
	*/
	m_map->finishBlockMake(bmdata, modified_blocks);

	MapBlock *block = m_map->getBlockNoCreateNoEx(pos);
	if (!block) {
		errorstream << "EmergeThread::finishGen: Couldn't grab block we "
			"just generated: " << PP(pos) << std::endl;
		return NULL;
	}

	v3s16 minp = bmdata->blockpos_min * MAP_BLOCKSIZE;
	v3s16 maxp = bmdata->blockpos_max * MAP_BLOCKSIZE +
				 v3s16(1,1,1) * (MAP_BLOCKSIZE - 1);

	// Ignore map edit events, they will not need to be sent
	// to anybody because the block hasn't been sent to anybody

/* thread unsafe
	MapEditEventAreaIgnorer ign(
		&m_server->m_ignore_map_edit_events_area,
		VoxelArea(minp, maxp));
*/

	/*
		Run Lua on_generated callbacks
	*/
	try {
		MAP_NOTHREAD_LOCK(m_map);
		m_server->getScriptIface()->environment_OnGenerated(
			minp, maxp, m_mapgen->blockseed);
	} catch (LuaError &e) {
		m_server->setAsyncFatalError("Lua: " + std::string(e.what()));
	}

	EMERGE_DBG_OUT("ended up with: " << analyze_block(block));

	/*
		Activate the block
	*/
	m_server->m_env->activateBlock(block, 0);

	return block;
}


void *EmergeThread::run()
{
	DSTACK(FUNCTION_NAME);
	BEGIN_DEBUG_EXCEPTION_HANDLER

	v3s16 pos;

	m_map    = (ServerMap *)&(m_server->m_env->getMap());
	m_emerge = m_server->m_emerge;
	m_mapgen = m_emerge->m_mapgens[id];
	enable_mapgen_debug_info = m_emerge->enable_mapgen_debug_info;

	reg("EmergeThread" + itos(id), 5);

	while (!stopRequested()) {
	try {
		std::map<v3s16, MapBlock *> modified_blocks;
		BlockEmergeData bedata;
		BlockMakeData bmdata;
		EmergeAction action;
		MapBlock *block;

		if (!popBlockEmerge(&pos, &bedata)) {
			m_queue_event.wait();
			continue;
		}

		if (blockpos_over_limit(pos))
			continue;

		bool allow_gen = bedata.flags & BLOCK_EMERGE_ALLOW_GEN;
		EMERGE_DBG_OUT("pos=" PP(pos) " allow_gen=" << allow_gen);

		action = getBlockOrStartGen(pos, allow_gen, &block, &bmdata);
		if (action == EMERGE_GENERATED) {
			{
				ScopeProfiler sp(g_profiler,
					"EmergeThread: Mapgen::makeChunk", SPT_AVG);
				TimeTaker t("mapgen::make_block()");

				m_mapgen->makeChunk(&bmdata);

				if (enable_mapgen_debug_info == false)
					t.stop(true); // Hide output
			}

			block = finishGen(pos, &bmdata, &modified_blocks);
		}

		runCompletionCallbacks(pos, action, bedata.callbacks);

		if (block) {
			//modified_blocks[pos] = block;
		} else if (allow_gen)
			infostream<<"nothing generated at "<<pos<<std::endl;



		if (modified_blocks.size() > 0)
			m_server->SetBlocksNotSent(/*modified_blocks*/);

		if (m_mapgen->heat_cache.size() > 1000) {
			m_mapgen->heat_cache.clear();
			m_mapgen->humidity_cache.clear();
		}
	} catch (VersionMismatchException &e) {
		std::ostringstream err;
		err << "World data version mismatch in MapBlock " << PP(pos) << std::endl
			<< "----" << std::endl
			<< "\"" << e.what() << "\"" << std::endl
			<< "See debug.txt." << std::endl
			<< "World probably saved by a newer version of " PROJECT_NAME_C "."
			<< std::endl;
		debug_stacks_print();
		m_server->setAsyncFatalError(err.str());
	} catch (SerializationError &e) {
		std::ostringstream err;
		err << "Invalid data in MapBlock " << PP(pos) << std::endl
			<< "----" << std::endl
			<< "\"" << e.what() << "\"" << std::endl
			<< "See debug.txt." << std::endl
			<< "You can ignore this using [ignore_world_load_errors = true]."
			<< std::endl;
		debug_stacks_print();
		m_server->setAsyncFatalError(err.str());
	} catch (std::exception &e) {
		errorstream << "emerge: exception at " << pos << " : " << e.what() << std::endl;
	}
	}

	END_DEBUG_EXCEPTION_HANDLER
	return NULL;
}
