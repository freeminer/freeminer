/*
script/scripting_mainmenu.cpp
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

#include "scripting_mainmenu.h"
#include "log.h"
#include "filesys.h"
#include "cpp_api/s_internal.h"
#include "lua_api/l_base.h"
#include "lua_api/l_mainmenu.h"
#include "lua_api/l_util.h"
#include "lua_api/l_settings.h"

extern "C" {
#include "lualib.h"
}

#define MAINMENU_NUM_ASYNC_THREADS 4


MainMenuScripting::MainMenuScripting(GUIEngine* guiengine)
{
	setGuiEngine(guiengine);

	//TODO add security

	luaL_openlibs(getStack());

	SCRIPTAPI_PRECHECKHEADER

	lua_newtable(L);
	lua_setglobal(L, "engine");
	lua_getglobal(L, "engine");
	int top = lua_gettop(L);

	lua_pushstring(L, DIR_DELIM);
	lua_setglobal(L, "DIR_DELIM");

	lua_newtable(L);
	lua_setglobal(L, "gamedata");

	// Initialize our lua_api modules
	initializeModApi(L, top);
	lua_pop(L, 1);

	infostream << "SCRIPTAPI: Initialized main menu modules" << std::endl;
}

/******************************************************************************/
void MainMenuScripting::initializeModApi(lua_State *L, int top)
{
	// Initialize mod API modules
	ModApiMainMenu::Initialize(L, top);
	ModApiUtil::Initialize(L, top);

	// Register reference classes (userdata)
	LuaSettings::Register(L);

	// Register functions to async environment
	ModApiMainMenu::InitializeAsync(asyncEngine);
	ModApiUtil::InitializeAsync(asyncEngine);

	// Initialize async environment
	//TODO possibly make number of async threads configurable
	asyncEngine.initialize(MAINMENU_NUM_ASYNC_THREADS);
}

/******************************************************************************/
void MainMenuScripting::step() {
	asyncEngine.step(getStack(), m_errorhandler);
}

/******************************************************************************/
unsigned int MainMenuScripting::queueAsync(std::string serialized_func,
		std::string serialized_param) {
	return asyncEngine.queueAsyncJob(serialized_func, serialized_param);
}

