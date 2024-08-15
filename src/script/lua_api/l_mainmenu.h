/*
Minetest
Copyright (C) 2013 sapier

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

#include "lua_api/l_base.h"

class AsyncEngine;

/** Implementation of lua api support for mainmenu */
class ModApiMainMenu: public ModApiBase
{

private:
	/**
	 * read a text variable from gamedata table within lua stack
	 * @param L stack to read variable from
	 * @param name name of variable to read
	 * @return string value of requested variable
	 */
	static std::string getTextData(lua_State *L, const std::string &name);

	/**
	 * read an integer variable from gamedata table within lua stack
	 * @param L stack to read variable from
	 * @param name name of variable to read
	 * @return integer value of requested variable
	 */
	static int getIntegerData(lua_State *L, const std::string &name, bool& valid);

	/**
	 * read a bool variable from gamedata table within lua stack
	 * @param L stack to read variable from
	 * @param name name of variable to read
	 * @return bool value of requested variable
	 */
	static int getBoolData(lua_State *L, const std::string &name ,bool& valid);

	/**
	 * Checks if a path may be modified. Paths in the temp directory or the user
	 * games, mods, textures, or worlds directories may be modified.
	 * @param path path to check
	 * @return true if the path may be modified
	 */
	static bool mayModifyPath(std::string path);

	//api calls

	static int l_start(lua_State *L);

	static int l_close(lua_State *L);

	static int l_create_world(lua_State *L);

	static int l_delete_world(lua_State *L);

	static int l_get_worlds(lua_State *L);

	static int l_get_mapgen_names(lua_State *L);

	static int l_get_language(lua_State *L);

	static int l_gettext(lua_State *L);

	//packages

	static int l_get_games(lua_State *L);

	static int l_get_content_info(lua_State *L);

	static int l_check_mod_configuration(lua_State *L);

	static int l_get_content_translation(lua_State *L);

	//gui

	static int l_show_keys_menu(lua_State *L);

	static int l_show_path_select_dialog(lua_State *L);

	static int l_set_topleft_text(lua_State *L);

	static int l_set_clouds(lua_State *L);

	static int l_get_textlist_index(lua_State *L);

	static int l_get_table_index(lua_State *L);

	static int l_set_background(lua_State *L);

	static int l_update_formspec(lua_State *L);

	static int l_set_formspec_prepend(lua_State *L);

	static int l_get_window_info(lua_State *L);

	static int l_get_active_driver(lua_State *L);

	static int l_get_active_renderer(lua_State *L);

	static int l_get_active_irrlicht_device(lua_State *L);

	//filesystem

	static int l_get_mainmenu_path(lua_State *L);

	static int l_get_user_path(lua_State *L);

	static int l_get_modpath(lua_State *L);

	static int l_get_modpaths(lua_State *L);

	static int l_get_clientmodpath(lua_State *L);

	static int l_get_gamepath(lua_State *L);

	static int l_get_texturepath(lua_State *L);

	static int l_get_texturepath_share(lua_State *L);

	static int l_get_cache_path(lua_State *L);

	static int l_get_temp_path(lua_State *L);

	static int l_create_dir(lua_State *L);

	static int l_delete_dir(lua_State *L);

	static int l_copy_dir(lua_State *L);

	static int l_is_dir(lua_State *L);

	static int l_extract_zip(lua_State *L);

	static int l_may_modify_path(lua_State *L);

	static int l_download_file(lua_State *L);

	static int l_get_video_drivers(lua_State *L);

	//version compatibility
	static int l_get_min_supp_proto(lua_State *L);

	static int l_get_max_supp_proto(lua_State *L);

	// other
	static int l_open_url(lua_State *L);

	static int l_open_dir(lua_State *L);

	static int l_share_file(lua_State *L);

	// async
	static int l_do_async_callback(lua_State *L);

public:

	/**
	 * initialize this API module
	 * @param L lua stack to initialize
	 * @param top index (in lua stack) of global API table
	 */
	static void Initialize(lua_State *L, int top);

	static void InitializeAsync(lua_State *L, int top);

};
