/*
script/scripting_mainmenu.h
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

#ifndef SCRIPTING_MAINMENU_H_
#define SCRIPTING_MAINMENU_H_

#include "cpp_api/s_base.h"
#include "cpp_api/s_mainmenu.h"
#include "lua_api/l_async_events.h"

/*****************************************************************************/
/* Scripting <-> Main Menu Interface                                         */
/*****************************************************************************/

class MainMenuScripting
		: virtual public ScriptApiBase,
		  public ScriptApiMainMenu
{
public:
	MainMenuScripting(GUIEngine* guiengine);

	// use ScriptApiBase::loadMod() or ScriptApiBase::loadScript()
	// to load scripts

	/* global step handler to pass back async events */
	void Step();

	/* pass async events from engine to async threads */
	unsigned int DoAsync(std::string serialized_fct,
			std::string serialized_params);
private:
	void InitializeModApi(lua_State *L, int top);

	AsyncEngine m_AsyncEngine;
};


#endif /* SCRIPTING_MAINMENU_H_ */
