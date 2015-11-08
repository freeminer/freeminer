/*
Minetest
Copyright (C) 2013 sapier, <sapier AT gmx DOT net>

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

#include <stdio.h>
#include <stdlib.h>

extern "C" {
#include "lua.h"
#include "lauxlib.h"
#include "lualib.h"
}

#include "server.h"
#include "s_async.h"
#include "log.h"
#include "filesys.h"
#include "porting.h"
#include "common/c_internal.h"

/******************************************************************************/
AsyncEngine::AsyncEngine() :
	initDone(false),
	jobIdCounter(0)
{
}

/******************************************************************************/
AsyncEngine::~AsyncEngine()
{

	// Request all threads to stop
	for (std::vector<AsyncWorkerThread *>::iterator it = workerThreads.begin();
			it != workerThreads.end(); it++) {
		(*it)->stop();
	}


	// Wake up all threads
	for (std::vector<AsyncWorkerThread *>::iterator it = workerThreads.begin();
			it != workerThreads.end(); it++) {
		jobQueueCounter.post();
	}

	// Wait for threads to finish
	for (std::vector<AsyncWorkerThread *>::iterator it = workerThreads.begin();
			it != workerThreads.end(); it++) {
		(*it)->wait();
	}

	// Force kill all threads
	for (std::vector<AsyncWorkerThread *>::iterator it = workerThreads.begin();
			it != workerThreads.end(); it++) {
		delete *it;
	}

	jobQueueMutex.lock();
	jobQueue.clear();
	jobQueueMutex.unlock();
	workerThreads.clear();
}

/******************************************************************************/
bool AsyncEngine::registerFunction(const char* name, lua_CFunction func)
{
	if (initDone) {
		return false;
	}
	functionList[name] = func;
	return true;
}

/******************************************************************************/
void AsyncEngine::initialize(unsigned int numEngines)
{
	initDone = true;

	for (unsigned int i = 0; i < numEngines; i++) {
		AsyncWorkerThread *toAdd = new AsyncWorkerThread(this,
			std::string("AsyncWorker-") + itos(i));
		workerThreads.push_back(toAdd);
		toAdd->start();
	}
}

/******************************************************************************/
unsigned int AsyncEngine::queueAsyncJob(std::string func, std::string params)
{
	jobQueueMutex.lock();
	LuaJobInfo toAdd;
	toAdd.id = jobIdCounter++;
	toAdd.serializedFunction = func;
	toAdd.serializedParams = params;

	jobQueue.push_back(toAdd);

	jobQueueCounter.post();

	jobQueueMutex.unlock();

	return toAdd.id;
}

/******************************************************************************/
LuaJobInfo AsyncEngine::getJob()
{
	jobQueueCounter.wait();
	jobQueueMutex.lock();

	LuaJobInfo retval;
	retval.valid = false;

	if (!jobQueue.empty()) {
		retval = jobQueue.front();
		jobQueue.pop_front();
		retval.valid = true;
	}
	jobQueueMutex.unlock();

	return retval;
}

/******************************************************************************/
void AsyncEngine::putJobResult(LuaJobInfo result)
{
	resultQueueMutex.lock();
	resultQueue.push_back(result);
	resultQueueMutex.unlock();
}

/******************************************************************************/
void AsyncEngine::step(lua_State *L)
{
	int error_handler = PUSH_ERROR_HANDLER(L);
	lua_getglobal(L, "core");
	resultQueueMutex.lock();
	while (!resultQueue.empty()) {
		LuaJobInfo jobDone = resultQueue.front();
		resultQueue.pop_front();

		lua_getfield(L, -1, "async_event_handler");

		if (lua_isnil(L, -1)) {
			FATAL_ERROR("Async event handler does not exist!");
		}

		luaL_checktype(L, -1, LUA_TFUNCTION);

		lua_pushinteger(L, jobDone.id);
		lua_pushlstring(L, jobDone.serializedResult.data(),
				jobDone.serializedResult.size());

		PCALL_RESL(L, lua_pcall(L, 2, 0, error_handler));
	}
	resultQueueMutex.unlock();
	lua_pop(L, 2); // Pop core and error handler
}

/******************************************************************************/
void AsyncEngine::pushFinishedJobs(lua_State* L) {
	// Result Table
	MutexAutoLock l(resultQueueMutex);

	unsigned int index = 1;
	lua_createtable(L, resultQueue.size(), 0);
	int top = lua_gettop(L);

	while (!resultQueue.empty()) {
		LuaJobInfo jobDone = resultQueue.front();
		resultQueue.pop_front();

		lua_createtable(L, 0, 2);  // Pre-allocate space for two map fields
		int top_lvl2 = lua_gettop(L);

		lua_pushstring(L, "jobid");
		lua_pushnumber(L, jobDone.id);
		lua_settable(L, top_lvl2);

		lua_pushstring(L, "retval");
		lua_pushlstring(L, jobDone.serializedResult.data(),
			jobDone.serializedResult.size());
		lua_settable(L, top_lvl2);

		lua_rawseti(L, top, index++);
	}
}

/******************************************************************************/
void AsyncEngine::prepareEnvironment(lua_State* L, int top)
{
	for (std::map<std::string, lua_CFunction>::iterator it = functionList.begin();
			it != functionList.end(); it++) {
		lua_pushstring(L, it->first.c_str());
		lua_pushcfunction(L, it->second);
		lua_settable(L, top);
	}
}

/******************************************************************************/
AsyncWorkerThread::AsyncWorkerThread(AsyncEngine* jobDispatcher,
		const std::string &name) :
	Thread(name),
	ScriptApiBase(),
	jobDispatcher(jobDispatcher)
{
	lua_State *L = getStack();

	// Prepare job lua environment
	lua_getglobal(L, "core");
	int top = lua_gettop(L);

	// Push builtin initialization type
	lua_pushstring(L, "async");
	lua_setglobal(L, "INIT");

	jobDispatcher->prepareEnvironment(L, top);
}

/******************************************************************************/
AsyncWorkerThread::~AsyncWorkerThread()
{
	sanity_check(!isRunning());
}

/******************************************************************************/
void* AsyncWorkerThread::run()
{
	lua_State *L = getStack();

	std::string script = getServer()->getBuiltinLuaPath() + DIR_DELIM + "init.lua";
	try {
		loadScript(script);
	} catch (const ModError &e) {
		errorstream << "Execution of async base environment failed: "
			<< e.what() << std::endl;
		FATAL_ERROR("Execution of async base environment failed");
	}

	int error_handler = PUSH_ERROR_HANDLER(L);

	lua_getglobal(L, "core");
	if (lua_isnil(L, -1)) {
		FATAL_ERROR("Unable to find core within async environment!");
	}

	// Main loop
	while (!stopRequested()) {
		// Wait for job
		LuaJobInfo toProcess = jobDispatcher->getJob();

		if (toProcess.valid == false || stopRequested()) {
			continue;
		}

		lua_getfield(L, -1, "job_processor");
		if (lua_isnil(L, -1)) {
			FATAL_ERROR("Unable to get async job processor!");
		}

		luaL_checktype(L, -1, LUA_TFUNCTION);

		// Call it
		lua_pushlstring(L,
				toProcess.serializedFunction.data(),
				toProcess.serializedFunction.size());
		lua_pushlstring(L,
				toProcess.serializedParams.data(),
				toProcess.serializedParams.size());

		int result = lua_pcall(L, 2, 1, error_handler);
		if (result) {
			PCALL_RES(result);
			toProcess.serializedResult = "";
		} else {
			// Fetch result
			size_t length;
			const char *retval = lua_tolstring(L, -1, &length);
			toProcess.serializedResult = std::string(retval, length);
		}

		lua_pop(L, 1);  // Pop retval

		// Put job result
		jobDispatcher->putJobResult(toProcess);
	}

	lua_pop(L, 2);  // Pop core and error handler

	return 0;
}

