/*
guiMainMenu.h
Copyright (C) 2010-2013 celeron55, Perttu Ahola <celeron55@gmail.com>
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

#pragma once

#include "irrlichttypes_extrabloated.h"
#include <string>
#include <list>

struct MainMenuDataForScript {

	MainMenuDataForScript() = default;

	// Whether the server has requested a reconnect
	bool reconnect_requested = false;
	std::string errormessage = "";
};

struct MainMenuData {
	// Client options
	std::string servername;
	std::string serverdescription;
	std::string address;
	std::string port;
	std::string name;
	std::string password;
	// Whether to reconnect
	bool do_reconnect = false;

	// Server options
	int selected_world = 0;
	bool simple_singleplayer_mode = false;

	// Data to be passed to the script
	MainMenuDataForScript script_data;

	MainMenuData() = default;
};
