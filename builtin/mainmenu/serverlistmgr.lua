--Minetest
--Copyright (C) 2020 rubenwardy
--
--This program is free software; you can redistribute it and/or modify
--it under the terms of the GNU Lesser General Public License as published by
--the Free Software Foundation; either version 2.1 of the License, or
--(at your option) any later version.
--
--This program is distributed in the hope that it will be useful,
--but WITHOUT ANY WARRANTY; without even the implied warranty of
--MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
--GNU Lesser General Public License for more details.
--
--You should have received a copy of the GNU Lesser General Public License along
--with this program; if not, write to the Free Software Foundation, Inc.,
--51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.

serverlistmgr = {}

--------------------------------------------------------------------------------
<<<<<<< HEAD:builtin/mainmenu/tab_simple_main.lua
local function get_formspec(tabview, name, tabdata)
	-- Update the cached supported proto info,
	-- it may have changed after a change by the settings menu.
	common_update_cached_supp_proto()
	local fav_selected = menudata.favorites[tabdata.fav_selected]

	local retval =
		"label[9.5,0;".. fgettext("Name / Password") .. "]" ..
		"field[0.25,3.35;5.5,0.5;te_address;;" ..
			core.formspec_escape(core.settings:get("address")) .."]" ..
		"field[5.75,3.35;2.25,0.5;te_port;;" ..
			core.formspec_escape(core.settings:get("remote_port")) .."]" ..
		"button[10,2.6;2,1.5;btn_mp_connect;".. fgettext("Connect") .. "]" ..
		"field[9.8,1;2.6,0.5;te_name;;" ..
			core.formspec_escape(core.settings:get("name")) .."]" ..
		"pwdfield[9.8,2;2.6,0.5;te_pwd;]"


	if tabdata.fav_selected and fav_selected then
		if gamedata.fav then
			retval = retval .. "button[7.7,2.6;2.3,1.5;btn_delete_favorite;" ..
				fgettext("Del. Favorite") .. "]"
=======
local function order_server_list(list)
	local res = {}
	--orders the favorite list after support
	for i = 1, #list do
		local fav = list[i]
		if is_server_protocol_compat(fav.proto_min, fav.proto_max) then
			res[#res + 1] = fav
>>>>>>> 5.5.0:builtin/mainmenu/serverlistmgr.lua
		end
	end
	for i = 1, #list do
		local fav = list[i]
		if not is_server_protocol_compat(fav.proto_min, fav.proto_max) then
			res[#res + 1] = fav
		end
	end
	return res
end

local public_downloading = false

--------------------------------------------------------------------------------
function serverlistmgr.sync()
	if not serverlistmgr.servers then
		serverlistmgr.servers = {{
			name = fgettext("Loading..."),
			description = fgettext_ne("Try reenabling public serverlist and check your internet connection.")
		}}
	end

	local serverlist_url = core.settings:get("serverlist_url") or ""
	if not core.get_http_api or serverlist_url == "" then
		serverlistmgr.servers = {{
			name = fgettext("Public server list is disabled"),
			description = ""
		}}
		return
	end

	if public_downloading then
		return
	end
	public_downloading = true

	core.handle_async(
		function(param)
			local http = core.get_http_api()
			local url = ("%s/list?proto_version_min=%d&proto_version_max=%d"):format(
				core.settings:get("serverlist_url"),
				core.get_min_supp_proto(),
				core.get_max_supp_proto())

			local response = http.fetch_sync({ url = url })
			if not response.succeeded then
				return {}
			end

			local retval = core.parse_json(response.data)
			return retval and retval.list or {}
		end,
		nil,
		function(result)
			public_downloading = nil
			local favs = order_server_list(result)
			if favs[1] then
				serverlistmgr.servers = favs
			end
			core.event_handler("Refresh")
		end
<<<<<<< HEAD:builtin/mainmenu/tab_simple_main.lua
		retval = retval .. render_favorite(menudata.favorites[1], (#favs > 0))
		for i = 2, #menudata.favorites do
			retval = retval .. "," .. render_favorite(menudata.favorites[i], (i <= #favs))
		end
	end

	if tabdata.fav_selected then
		retval = retval .. ";" .. tabdata.fav_selected .. "]"
	else
		retval = retval .. ";0]"
	end

	-- separator
	retval = retval .. "box[-0.28,3.75;12.4,0.1;#FFFFFF]"

	-- checkboxes
	retval = retval ..
		"checkbox[8.0,3.9;cb_creative;".. fgettext("Creative Mode") .. ";" ..
			dump(core.settings:get_bool("creative_mode")) .. "]"..
		"checkbox[8.0,4.4;cb_damage;".. fgettext("Enable Damage") .. ";" ..
			dump(core.settings:get_bool("enable_damage")) .. "]"
	-- buttons
	retval = retval ..
		"button[0,3.7;8,1.5;btn_start_singleplayer;" .. fgettext("Start Singleplayer") .. "]" ..
		"button[0,4.5;8,1.5;btn_config_sp_world;" .. fgettext("Config mods") .. "]"

	return retval
=======
	)
>>>>>>> 5.5.0:builtin/mainmenu/serverlistmgr.lua
end

--------------------------------------------------------------------------------
local function get_favorites_path(folder)
	local base = core.get_user_path() .. DIR_DELIM .. "client" .. DIR_DELIM .. "serverlist" .. DIR_DELIM
	if folder then
		return base
	end
	return base .. core.settings:get("serverlist_file")
end

--------------------------------------------------------------------------------
local function save_favorites(favorites)
	local filename = core.settings:get("serverlist_file")
	-- If setting specifies legacy format change the filename to the new one
	if filename:sub(#filename - 3):lower() == ".txt" then
		core.settings:set("serverlist_file", filename:sub(1, #filename - 4) .. ".json")
	end

	assert(core.create_dir(get_favorites_path(true)))
	core.safe_file_write(get_favorites_path(), core.write_json(favorites))
end

--------------------------------------------------------------------------------
function serverlistmgr.read_legacy_favorites(path)
	local file = io.open(path, "r")
	if not file then
		return nil
	end

<<<<<<< HEAD:builtin/mainmenu/tab_simple_main.lua
				if address and port then
					core.settings:set("address", address)
					core.settings:set("remote_port", port)
				end
				tabdata.fav_selected = event.row
=======
	local lines = {}
	for line in file:lines() do
		lines[#lines + 1] = line
	end
	file:close()

	local favorites = {}

	local i = 1
	while i < #lines do
		local function pop()
			local line = lines[i]
			i = i + 1
			return line and line:trim()
		end

		if pop():lower() == "[server]" then
			local name = pop()
			local address = pop()
			local port = tonumber(pop())
			local description = pop()

			if name == "" then
				name = nil
>>>>>>> 5.5.0:builtin/mainmenu/serverlistmgr.lua
			end

<<<<<<< HEAD:builtin/mainmenu/tab_simple_main.lua
	if fields.btn_delete_favorite then
		local current_favourite = core.get_table_index("favourites")
		if not current_favourite then return end

		core.delete_favorite(current_favourite)
		asyncOnlineFavourites()
		tabdata.fav_selected = nil

		core.settings:set("address", "")
		core.settings:set("remote_port", "30000")
		return true
	end

	if fields.cb_creative then
		core.settings:set("creative_mode", fields.cb_creative)
		return true
	end

	if fields.cb_damage then
		core.settings:set("enable_damage", fields.cb_damage)
		return true
	end

	if fields.btn_mp_connect or fields.key_enter then
		gamedata.playername = fields.te_name
		gamedata.password   = fields.te_pwd
		gamedata.address    = fields.te_address
		gamedata.port	    = fields.te_port
		local fav_idx = core.get_textlist_index("favourites")

		if fav_idx and fav_idx <= #menudata.favorites and
				menudata.favorites[fav_idx].address == fields.te_address and
				menudata.favorites[fav_idx].port    == fields.te_port then
			local fav = menudata.favorites[fav_idx]
			gamedata.servername        = fav.name
			gamedata.serverdescription = fav.description

			if menudata.favorites_is_public and
					not is_server_protocol_compat_or_error(
						fav.proto_min, fav.proto_max, fav.proto) then
				return true
=======
			if description == "" then
				description = nil
			end

			if not address or #address < 3 then
				core.log("warning", "Malformed favorites file, missing address at line " .. i)
			elseif not port or port < 1 or port > 65535 then
				core.log("warning", "Malformed favorites file, missing port at line " .. i)
			elseif (name and name:upper() == "[SERVER]") or
					(address and address:upper() == "[SERVER]") or
					(description and description:upper() == "[SERVER]") then
				core.log("warning", "Potentially malformed favorites file, overran at line " .. i)
			else
				favorites[#favorites + 1] = {
					name = name,
					address = address,
					port = port,
					description = description
				}
>>>>>>> 5.5.0:builtin/mainmenu/serverlistmgr.lua
			end
		end
<<<<<<< HEAD:builtin/mainmenu/tab_simple_main.lua

		gamedata.selected_world = 0

		core.settings:set("address", fields.te_address)
		core.settings:set("remote_port", fields.te_port)
=======
	end
>>>>>>> 5.5.0:builtin/mainmenu/serverlistmgr.lua

	return favorites
end

--------------------------------------------------------------------------------
local function read_favorites()
	local path = get_favorites_path()

	-- If new format configured fall back to reading the legacy file
	if path:sub(#path - 4):lower() == ".json" then
		local file = io.open(path, "r")
		if file then
			local json = file:read("*all")
			file:close()
			return core.parse_json(json)
		end

		path = path:sub(1, #path - 5) .. ".txt"
	end

	local favs = serverlistmgr.read_legacy_favorites(path)
	if favs then
		save_favorites(favs)
		os.remove(path)
	end
	return favs
end

--------------------------------------------------------------------------------
local function delete_favorite(favorites, del_favorite)
	for i=1, #favorites do
		local fav = favorites[i]

		if fav.address == del_favorite.address and fav.port == del_favorite.port then
			table.remove(favorites, i)
			return
		end
	end
end

--------------------------------------------------------------------------------
function serverlistmgr.get_favorites()
	if serverlistmgr.favorites then
		return serverlistmgr.favorites
	end

	serverlistmgr.favorites = {}

	-- Add favorites, removing duplicates
	local seen = {}
	for _, fav in ipairs(read_favorites() or {}) do
		local key = ("%s:%d"):format(fav.address:lower(), fav.port)
		if not seen[key] then
			seen[key] = true
			serverlistmgr.favorites[#serverlistmgr.favorites + 1] = fav
		end
	end

	return serverlistmgr.favorites
end

--------------------------------------------------------------------------------
function serverlistmgr.add_favorite(new_favorite)
	assert(type(new_favorite.port) == "number")

	-- Whitelist favorite keys
	new_favorite = {
		name = new_favorite.name,
		address = new_favorite.address,
		port = new_favorite.port,
		description = new_favorite.description,
	}

	local favorites = serverlistmgr.get_favorites()
	delete_favorite(favorites, new_favorite)
	table.insert(favorites, 1, new_favorite)
	save_favorites(favorites)
end

--------------------------------------------------------------------------------
function serverlistmgr.delete_favorite(del_favorite)
	local favorites = serverlistmgr.get_favorites()
	delete_favorite(favorites, del_favorite)
	save_favorites(favorites)
end
