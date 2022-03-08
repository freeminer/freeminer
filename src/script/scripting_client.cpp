/*
threads.h
Copyright (C) 2013 celeron55, Perttu Ahola <celeron55@gmail.com>
<<<<<<< HEAD:src/threads.h
*/
=======
Copyright (C) 2017 nerzhul, Loic Blot <loic.blot@unix-experience.fr>
>>>>>>> 5.5.0:src/script/scripting_client.cpp

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

#include "scripting_client.h"
#include "client/client.h"
#include "cpp_api/s_internal.h"
#include "lua_api/l_client.h"
#include "lua_api/l_env.h"
#include "lua_api/l_item.h"
#include "lua_api/l_itemstackmeta.h"
#include "lua_api/l_minimap.h"
#include "lua_api/l_modchannels.h"
#include "lua_api/l_particles_local.h"
#include "lua_api/l_storage.h"
#include "lua_api/l_sound.h"
#include "lua_api/l_util.h"
#include "lua_api/l_item.h"
#include "lua_api/l_nodemeta.h"
#include "lua_api/l_localplayer.h"
#include "lua_api/l_camera.h"

<<<<<<< HEAD:src/threads.h
//
// Determine which threading APIs we will use
//
#if __cplusplus >= 201103L
	#define USE_CPP11_THREADS 1
#elif defined(_WIN32) || defined(WIN32)
	#define USE_WIN_THREADS 1
#else
	#define USE_POSIX_THREADS 1
#endif

#if defined(_WIN32)
	// Prefer critical section API because std::mutex is much slower on Windows
	#define USE_WIN_MUTEX 1
#elif __cplusplus >= 201103L
	#define USE_CPP11_MUTEX 1
#else
	#define USE_POSIX_MUTEX 1
#endif

///////////////


#if USE_CPP11_THREADS
	#include <thread>
#elif USE_POSIX_THREADS
	#include <pthread.h>
#else
	#ifndef WIN32_LEAN_AND_MEAN
		#define WIN32_LEAN_AND_MEAN
	#endif
	#include <windows.h>
#endif

#include "threading/mutex.h"

//
// threadid_t, threadhandle_t
//
#if USE_CPP11_THREADS
	//typedef std::thread::id threadid_t;
	typedef std::size_t threadid_t;
	typedef std::thread::native_handle_type threadhandle_t;
#elif USE_WIN_THREADS
	typedef DWORD threadid_t;
	typedef HANDLE threadhandle_t;
#elif USE_POSIX_THREADS
	typedef pthread_t threadid_t;
	typedef pthread_t threadhandle_t;
#endif

//
// ThreadStartFunc
//
#if USE_CPP11_THREADS || USE_POSIX_THREADS
	typedef void *ThreadStartFunc(void *param);
#elif defined(_WIN32_WCE)
	typedef DWORD ThreadStartFunc(LPVOID param);
#elif defined(_WIN32) || defined(WIN32)
	typedef DWORD WINAPI ThreadStartFunc(LPVOID param);
#endif


inline threadid_t thr_get_current_thread_id()
{
#if USE_CPP11_THREADS
	return std::hash<std::thread::id>()(std::this_thread::get_id());
/*
	return std::this_thread::get_id();
*/
#elif USE_WIN_THREADS
	return GetCurrentThreadId();
#elif USE_POSIX_THREADS
	return pthread_self();
#endif
}


inline bool thr_compare_thread_id(threadid_t thr1, threadid_t thr2)
=======
ClientScripting::ClientScripting(Client *client):
	ScriptApiBase(ScriptingType::Client)
{
	setGameDef(client);

	SCRIPTAPI_PRECHECKHEADER

	// Security is mandatory client side
	initializeSecurityClient();

	lua_getglobal(L, "core");
	int top = lua_gettop(L);

	lua_newtable(L);
	lua_setfield(L, -2, "ui");

	InitializeModApi(L, top);
	lua_pop(L, 1);

	// Push builtin initialization type
	lua_pushstring(L, "client");
	lua_setglobal(L, "INIT");

	infostream << "SCRIPTAPI: Initialized client game modules" << std::endl;
}

void ClientScripting::InitializeModApi(lua_State *L, int top)
>>>>>>> 5.5.0:src/script/scripting_client.cpp
{
	LuaItemStack::Register(L);
	ItemStackMetaRef::Register(L);
	LuaRaycast::Register(L);
	StorageRef::Register(L);
	LuaMinimap::Register(L);
	NodeMetaRef::RegisterClient(L);
	LuaLocalPlayer::Register(L);
	LuaCamera::Register(L);
	ModChannelRef::Register(L);

	ModApiUtil::InitializeClient(L, top);
	ModApiClient::Initialize(L, top);
	ModApiStorage::Initialize(L, top);
	ModApiEnvMod::InitializeClient(L, top);
	ModApiChannels::Initialize(L, top);
	ModApiParticlesLocal::Initialize(L, top);
}

void ClientScripting::on_client_ready(LocalPlayer *localplayer)
{
	LuaLocalPlayer::create(getStack(), localplayer);
}

void ClientScripting::on_camera_ready(Camera *camera)
{
	LuaCamera::create(getStack(), camera);
}

void ClientScripting::on_minimap_ready(Minimap *minimap)
{
	LuaMinimap::create(getStack(), minimap);
}
