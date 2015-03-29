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
#include "server.h"
#include <iostream>
#include <queue>
#include "jthread/jevent.h"
#include "map.h"
#include "environment.h"
#include "util/container.h"
#include "util/thread.h"
#include "main.h"
#include "constants.h"
#include "voxel.h"
#include "config.h"
#include "mapblock.h"
#include "serverobject.h"
#include "settings.h"
#include "scripting_game.h"
#include "profiler.h"
#include "log_types.h"
#include "nodedef.h"
#include "mg_biome.h"
#include "mg_ore.h"
#include "mg_decoration.h"
#include "mg_schematic.h"
#include "mapgen_v5.h"
#include "mapgen_v6.h"
#include "mapgen_v7.h"
#include "mapgen_indev.h"
#include "mapgen_singlenode.h"
#include "mapgen_math.h"
#include "util/thread_pool.h"

struct MapgenDesc {
	const char *name;
	MapgenFactory *factory;
};

MapgenDesc reg_mapgens[] = {
	{"v5",         new MapgenFactoryV5},
	{"v6",         new MapgenFactoryV6},
	{"indev",      new MapgenFactoryIndev},
	{"v7",         new MapgenFactoryV7},
	{"math",       new MapgenFactoryMath},
	{"singlenode", new MapgenFactorySinglenode},
};

class EmergeThread : public thread_pool
{
public:
	Server *m_server;
	ServerMap *map;
	EmergeManager *emerge;
	Mapgen *mapgen;
	bool enable_mapgen_debug_info;
	int id;

	Event qevent;
	std::queue<v3s16> blockqueue;

	EmergeThread(Server *server, int ethreadid):
		m_server(server),
		map(NULL),
		emerge(NULL),
		mapgen(NULL),
		enable_mapgen_debug_info(false),
		id(ethreadid)
	{
	}

	void *Thread();
	bool popBlockEmerge(v3s16 *pos, u8 *flags);
	bool getBlockOrStartGen(v3s16 p, MapBlock **b,
			BlockMakeData *data, bool allow_generate);
};


/////////////////////////////// Emerge Manager ////////////////////////////////

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
	this->threads_active = false;

	mapgen_debug_info = g_settings->getBool("enable_mapgen_debug_info");

	// if unspecified, leave a proc for the main thread and one for
	// some other misc thread
	s16 nthreads = 0;
	if (!g_settings->getS16NoEx("num_emerge_threads", nthreads))
	{}
#if ENABLE_THREADS
	if (nthreads < 1)
		nthreads = porting::getNumberOfProcessors() - 2;
#endif
	if (nthreads < 1)
		nthreads = 1;

	qlimit_total = g_settings->getU16("emergequeue_limit_total");
	if (qlimit_total < 1)
		qlimit_total = nthreads*128;
	if (!g_settings->getU16NoEx("emergequeue_limit_diskonly", qlimit_diskonly))
		{}
	if (qlimit_diskonly < 1) {
		qlimit_diskonly = nthreads * 100;
	}
	if (!g_settings->getU16NoEx("emergequeue_limit_generate", qlimit_generate))
		{}
	if (qlimit_generate < 1) {
		qlimit_generate = nthreads * 32;
	}
	//errorstream<<"==> qlimit_generate="<<qlimit_generate<<"  qlimit_diskonly="<<qlimit_diskonly<<" qlimit_total="<<qlimit_total<<std::endl;

	// don't trust user input for something very important like this
	if (qlimit_total < 1)
		qlimit_total = 1;
	if (qlimit_diskonly < 1)
		qlimit_diskonly = 1;
	if (qlimit_generate < 1)
		qlimit_generate = 1;

	for (s16 i = 0; i < nthreads; i++)
		emergethread.push_back(new EmergeThread((Server *) gamedef, i));

	infostream << "EmergeManager: using " << nthreads << " threads" << std::endl;
}


EmergeManager::~EmergeManager()
{
	for (u32 i = 0; i != emergethread.size(); i++) {
		if (threads_active) {
			emergethread[i]->Stop();
			emergethread[i]->qevent.signal();
			emergethread[i]->Wait();
		}
		delete emergethread[i];
		delete mapgen[i];
	}
	emergethread.clear();
	mapgen.clear();

	delete biomemgr;
	delete oremgr;
	delete decomgr;
	delete schemmgr;

	if (params.sparams) {
		delete params.sparams;
		params.sparams = NULL;
	}
}


void EmergeManager::loadMapgenParams()
{
	params.load(*g_settings);

	biomemgr->mapgen_params = &params;
}


void EmergeManager::initMapgens()
{
	if (mapgen.size())
		return;

	if (!params.sparams) {
		params.sparams = createMapgenParams(params.mg_name);
		if (!params.sparams) {
			params.mg_name = DEFAULT_MAPGEN;
			params.sparams = createMapgenParams(params.mg_name);
			assert(params.sparams);
		}
		params.sparams->readParams(g_settings);
	}

	// Create the mapgens
	for (u32 i = 0; i != emergethread.size(); i++) {
		Mapgen *mg = createMapgen(params.mg_name, i, &params);
		if (!mg)
			continue;
		mapgen.push_back(mg);
	}
}


Mapgen *EmergeManager::getCurrentMapgen()
{
	for (u32 i = 0; i != emergethread.size(); i++) {
		if (emergethread[i]->IsSameThread())
			return emergethread[i]->mapgen;
	}

	return NULL;
}


void EmergeManager::startThreads()
{
	if (threads_active)
		return;

	for (u32 i = 0; i != emergethread.size(); i++)
		emergethread[i]->Start();

	threads_active = true;
}


void EmergeManager::stopThreads()
{
	if (!threads_active)
		return;

	// Request thread stop in parallel
	for (u32 i = 0; i != emergethread.size(); i++) {
		emergethread[i]->Stop();
		emergethread[i]->qevent.signal();
	}

	// Then do the waiting for each
	for (u32 i = 0; i != emergethread.size(); i++)
		emergethread[i]->Wait();

	threads_active = false;
}


bool EmergeManager::enqueueBlockEmerge(u16 peer_id, v3s16 p, bool allow_generate)
{
	std::map<v3s16, BlockEmergeData *>::const_iterator iter;
	BlockEmergeData *bedata;
	u16 count;
	u8 flags = 0;
	int idx = 0;

	if (allow_generate)
		flags |= BLOCK_EMERGE_ALLOWGEN;

	{
		JMutexAutoLock queuelock(queuemutex);

		count = blocks_enqueued.size();
		if (count >= qlimit_total)
			return false;

		count = peer_queue_count[peer_id];
		u16 qlimit_peer = allow_generate ? qlimit_generate : qlimit_diskonly;
		if (count >= qlimit_peer)
			return false;

		iter = blocks_enqueued.find(p);
		if (iter != blocks_enqueued.end()) {
			bedata = iter->second;
			bedata->flags |= flags;
			return true;
		}

		bedata = new BlockEmergeData;
		bedata->flags = flags;
		bedata->peer_requested = peer_id;
		blocks_enqueued.insert(std::make_pair(p, bedata));

		peer_queue_count[peer_id] = count + 1;

		// insert into the EmergeThread queue with the least items
		int lowestitems = emergethread[0]->blockqueue.size();
		for (u32 i = 1; i != emergethread.size(); i++) {
			int nitems = emergethread[i]->blockqueue.size();
			if (nitems < lowestitems) {
				idx = i;
				lowestitems = nitems;
			}
		}

		emergethread[idx]->blockqueue.push(p);
	}
	emergethread[idx]->qevent.signal();

	return true;
}


int EmergeManager::getGroundLevelAtPoint(v2s16 p)
{
	if (mapgen.size() == 0 || !mapgen[0]) {
		errorstream << "EmergeManager: getGroundLevelAtPoint() called"
			" before mapgen initialized" << std::endl;
		return 0;
	}

	return mapgen[0]->getGroundLevelAtPoint(p);
}


bool EmergeManager::isBlockUnderground(v3s16 blockpos)
{
	/*
	v2s16 p = v2s16((blockpos.X * MAP_BLOCKSIZE) + MAP_BLOCKSIZE / 2,
					(blockpos.Y * MAP_BLOCKSIZE) + MAP_BLOCKSIZE / 2);
	int ground_level = getGroundLevelAtPoint(p);
	return blockpos.Y * (MAP_BLOCKSIZE + 1) <= min(water_level, ground_level);
	*/

	//yuck, but then again, should i bother being accurate?
	//the height of the nodes in a single block is quite variable
	return blockpos.Y * (MAP_BLOCKSIZE + 1) <= params.water_level;
}


void EmergeManager::getMapgenNames(std::list<const char *> &mgnames)
{
	for (u32 i = 0; i != ARRLEN(reg_mapgens); i++)
		mgnames.push_back(reg_mapgens[i].name);
}


Mapgen *EmergeManager::createMapgen(const std::string &mgname, int mgid,
	MapgenParams *mgparams)
{
	u32 i;
	for (i = 0; i != ARRLEN(reg_mapgens) && mgname != reg_mapgens[i].name; i++);
	if (i == ARRLEN(reg_mapgens)) {
		errorstream << "EmergeManager; mapgen " << mgname <<
			" not registered" << std::endl;
		return NULL;
	}

	MapgenFactory *mgfactory = reg_mapgens[i].factory;
	return mgfactory->createMapgen(mgid, mgparams, this);
}


MapgenSpecificParams *EmergeManager::createMapgenParams(const std::string &mgname)
{
	u32 i;
	for (i = 0; i < ARRLEN(reg_mapgens) && mgname != reg_mapgens[i].name; i++);
	if (i == ARRLEN(reg_mapgens)) {
		errorstream << "EmergeManager: Mapgen " << mgname <<
			" not registered" << std::endl;
		return NULL;
	}

	MapgenFactory *mgfactory = reg_mapgens[i].factory;
	return mgfactory->createMapgenParams();
}


////////////////////////////// Emerge Thread //////////////////////////////////

bool EmergeThread::popBlockEmerge(v3s16 *pos, u8 *flags)
{
	std::map<v3s16, BlockEmergeData *>::iterator iter;
	JMutexAutoLock queuelock(emerge->queuemutex);

	if (blockqueue.empty())
		return false;
	v3s16 p = blockqueue.front();
	blockqueue.pop();

	*pos = p;

	iter = emerge->blocks_enqueued.find(p);
	if (iter == emerge->blocks_enqueued.end())
		return false; //uh oh, queue and map out of sync!!

	BlockEmergeData *bedata = iter->second;
	*flags = bedata->flags;

	emerge->peer_queue_count[bedata->peer_requested]--;

	delete bedata;
	emerge->blocks_enqueued.erase(iter);

	return true;
}


bool EmergeThread::getBlockOrStartGen(v3s16 p, MapBlock **b,
	BlockMakeData *data, bool allow_gen)
{
	//envlock: usually takes <=1ms, sometimes 90ms or ~400ms to acquire
	//JMutexAutoLock envlock(m_server->m_env_mutex);

	// Attempt to load block
	MapBlock *block = map->getBlockNoCreateNoEx(p);
	if (!block || block->isDummy()) {
		EMERGE_DBG_OUT("not in memory, attempting to load from disk ag="<<allow_gen<<" block="<<block<<" p="<<p);
		block = map->loadBlock(p);
		if (block && block->isGenerated())
			map->prepareBlock(block);
	}

	// If could not load and allowed to generate,
	// start generation inside this same envlock
	if (allow_gen && (!block)) {
		EMERGE_DBG_OUT("generating b="<<block);
		*b = block;
		return map->initBlockMake(data, p);
	}

	*b = block;
	return false;
}


void *EmergeThread::Thread()
{
	ThreadStarted();
	log_register_thread("EmergeThread" + itos(id));
	DSTACK(__FUNCTION_NAME);
	BEGIN_DEBUG_EXCEPTION_HANDLER

	v3s16 last_tried_pos(-32768,-32768,-32768); // For error output
	v3s16 p;
	u8 flags = 0;

	map       = (ServerMap *)&(m_server->m_env->getMap());
	emerge    = m_server->m_emerge;
	mapgen    = emerge->mapgen[id];
	enable_mapgen_debug_info = emerge->mapgen_debug_info;

	porting::setThreadName(("EmergeThread" + itos(id)).c_str());
	porting::setThreadPriority(5);

	while (!StopRequested())
	try {
		if (!popBlockEmerge(&p, &flags)) {
			qevent.wait();
			continue;
		}

		last_tried_pos = p;
		if (blockpos_over_limit(p))
			continue;

		bool allow_generate = flags & BLOCK_EMERGE_ALLOWGEN;
		EMERGE_DBG_OUT("p=" PP(p) " allow_generate=" << allow_generate);

		/*
			Try to fetch block from memory or disk.
			If not found and asked to generate, initialize generator.
		*/
		BlockMakeData data;
		MapBlock *block = NULL;
		std::map<v3s16, MapBlock *> modified_blocks;

		if (getBlockOrStartGen(p, &block, &data, allow_generate) && mapgen) {
			{
				ScopeProfiler sp(g_profiler, "EmergeThread: Mapgen::makeChunk", SPT_AVG);
				TimeTaker t("mapgen::make_block()");

				mapgen->makeChunk(&data);

				if (enable_mapgen_debug_info == false)
					t.stop(true); // Hide output
			}

			{
				//envlock: usually 0ms, but can take either 30 or 400ms to acquire
				//JMutexAutoLock envlock(m_server->m_env_mutex);
				ScopeProfiler sp(g_profiler, "EmergeThread: after "
						"Mapgen::makeChunk (envlock)", SPT_AVG);

				map->finishBlockMake(&data, modified_blocks);

				block = map->getBlockNoCreateNoEx(p);
				if (block) {
					/*
						Do some post-generate stuff
					*/
					v3s16 minp = data.blockpos_min * MAP_BLOCKSIZE;
					v3s16 maxp = data.blockpos_max * MAP_BLOCKSIZE +
								 v3s16(1,1,1) * (MAP_BLOCKSIZE - 1);

					// Ignore map edit events, they will not need to be sent
					// to anybody because the block hasn't been sent to anybody
					MapEditEventAreaIgnorer
						ign(&m_server->m_ignore_map_edit_events_area,
						VoxelArea(minp, maxp));
					try {  // takes about 90ms with -O1 on an e3-1230v2
						m_server->getScriptIface()->environment_OnGenerated(
								minp, maxp, mapgen->blockseed);
					} catch(LuaError &e) {
						m_server->setAsyncFatalError(e.what());
					}

					EMERGE_DBG_OUT("ended up with: " << analyze_block(block));
				}
			}

		/*
			Set sent status of modified blocks on clients
		*/
		// Add the originally fetched block to the modified list
		if (block)
			modified_blocks[p] = block;
		else if (allow_generate)
			infostream<<"nothing generated at "<<PP(p)<<std::endl;

		}

		if (modified_blocks.size() > 0) {
			m_server->SetBlocksNotSent(modified_blocks);
		}
		if (mapgen->heat_cache.size() > 1000) {
			mapgen->heat_cache.clear();
			mapgen->humidity_cache.clear();
		}
	}
	catch (VersionMismatchException &e) {
		std::ostringstream err;
		err << "World data version mismatch in MapBlock " << PP(last_tried_pos) << std::endl
			<< "----" << std::endl
			<< "\"" << e.what() << "\"" << std::endl
			<< "See debug.txt." << std::endl
			<< "World probably saved by a newer version of " PROJECT_NAME "."
			<< std::endl;
		m_server->setAsyncFatalError(err.str());
	}
	catch (SerializationError &e) {
		std::ostringstream err;
		err << "Invalid data in MapBlock " << PP(last_tried_pos) << std::endl
			<< "----" << std::endl
			<< "\"" << e.what() << "\"" << std::endl
			<< "See debug.txt." << std::endl
			<< "You can ignore this using [ignore_world_load_errors = true]."
			<< std::endl;
		m_server->setAsyncFatalError(err.str());
	}

	{
		JMutexAutoLock queuelock(emerge->queuemutex);
		while (!blockqueue.empty())
		{
			v3s16 p = blockqueue.front();
			blockqueue.pop();

			std::map<v3s16, BlockEmergeData *>::iterator iter;
			iter = emerge->blocks_enqueued.find(p);
			if (iter == emerge->blocks_enqueued.end())
				continue; //uh oh, queue and map out of sync!!

			BlockEmergeData *bedata = iter->second;
			delete bedata;
		}
	}

	END_DEBUG_EXCEPTION_HANDLER(errorstream)
	log_deregister_thread();
	return NULL;
}
