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
#include "content/mods.h"
#include "cpp_api/s_internal.h"
#include "lua_api/l_base.h"
#include "lua_api/l_http.h"
#include "lua_api/l_mainmenu.h"
#include "lua_api/l_mainmenu_sound.h"
#include "lua_api/l_util.h"
#include "lua_api/l_settings.h"
#include "log.h"

extern "C" {
#include "lualib.h"
}
#define MAINMENU_NUM_ASYNC_THREADS 4


MainMenuScripting::MainMenuScripting(GUIEngine* guiengine):
		ScriptApiBase(ScriptingType::MainMenu)
{
	setGuiEngine(guiengine);

	SCRIPTAPI_PRECHECKHEADER

	lua_getglobal(L, "core");
	int top = lua_gettop(L);

	lua_newtable(L);
	lua_setglobal(L, "gamedata");

	// Initialize our lua_api modules
	initializeModApi(L, top);
	lua_pop(L, 1);

	// Push builtin initialization type
	lua_pushstring(L, "mainmenu");
	lua_setglobal(L, "INIT");

	infostream << "SCRIPTAPI: Initialized main menu modules" << std::endl;
}

/******************************************************************************/
void MainMenuScripting::initializeModApi(lua_State *L, int top)
{
	registerLuaClasses(L, top);

	// Initialize mod API modules
	ModApiMainMenu::Initialize(L, top);
	ModApiUtil::Initialize(L, top);
	ModApiMainMenuSound::Initialize(L, top);
	ModApiHttp::Initialize(L, top);

	asyncEngine.registerStateInitializer(registerLuaClasses);
	asyncEngine.registerStateInitializer(ModApiMainMenu::InitializeAsync);
	asyncEngine.registerStateInitializer(ModApiUtil::InitializeAsync);
	asyncEngine.registerStateInitializer(ModApiHttp::InitializeAsync);

	// Initialize async environment
	//TODO possibly make number of async threads configurable
	asyncEngine.initialize(MAINMENU_NUM_ASYNC_THREADS);
}

/******************************************************************************/
void MainMenuScripting::registerLuaClasses(lua_State *L, int top)
{
	LuaSettings::Register(L);
	MainMenuSoundHandle::Register(L);
}

/******************************************************************************/
void MainMenuScripting::step()
{
	asyncEngine.step(getStack());
}

/******************************************************************************/
u32 MainMenuScripting::queueAsync(std::string &&serialized_func,
		std::string &&serialized_param)
{
	return asyncEngine.queueAsyncJob(std::move(serialized_func), std::move(serialized_param));
}

