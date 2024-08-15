/*
Minetest
Copyright (C) 2010-2013 celeron55, Perttu Ahola <celeron55@gmail.com>

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

#pragma once

#include "irrlichttypes_extrabloated.h"
#include "client/inputhandler.h"
#include "gameparams.h"

class RenderingEngine;

class ClientLauncher
{
public:
	ClientLauncher() = default;

	~ClientLauncher();

	bool run(GameStartData &start_data, const Settings &cmd_args);

private:
	void init_args(GameStartData &start_data, const Settings &cmd_args);
	bool init_engine();
	void init_input();

	static void setting_changed_callback(const std::string &name, void *data);
	void config_guienv();

	bool launch_game(std::string &error_message, bool reconnect_requested,
		GameStartData &start_data, const Settings &cmd_args);

	void main_menu(MainMenuData *menudata);

	bool skip_main_menu = false;
	bool random_input = false;
	RenderingEngine *m_rendering_engine = nullptr;
	InputHandler *input = nullptr;
	MyEventReceiver *receiver = nullptr;
};
