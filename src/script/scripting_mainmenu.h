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
#include "cpp_api/s_async.h"

/*****************************************************************************/
/* Scripting <-> Main Menu Interface                                         */
/*****************************************************************************/

class MainMenuScripting
		: virtual public ScriptApiBase,
		  public ScriptApiMainMenu
{
public:
	MainMenuScripting(GUIEngine* guiengine);

	// Global step handler to pass back async events
	void step();

	// Pass async events from engine to async threads
	unsigned int queueAsync(std::string serialized_func,
			std::string serialized_params);
private:
	void initializeModApi(lua_State *L, int top);

	AsyncEngine asyncEngine;
};


#endif /* SCRIPTING_MAINMENU_H_ */
