--Minetest
--Copyright (C) 2014 sapier
--Copyright (C) 2018 rubenwardy <rw@rubenwardy.com>
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

local packages_raw
local packages

--------------------------------------------------------------------------------
local function get_formspec(tabview, name, tabdata)
<<<<<<< HEAD:builtin/mainmenu/tab_server.lua
	
	local index = menudata.worldlist:get_current_index(
				tonumber(core.settings:get("mainmenu_last_selected_world"))
				)

	local retval =
		"button[4,4.15;2.6,0.5;world_delete;" .. fgettext("Delete") .. "]" ..
		"button[6.5,4.15;2.8,0.5;world_create;" .. fgettext("New") .. "]" ..
		"button[9.2,4.15;2.55,0.5;world_configure;" .. fgettext("Configure") .. "]" ..
		"button[8.5,4.95;3.25,0.5;start_server;" .. fgettext("Start Game") .. "]" ..
		"label[4,-0.25;" .. fgettext("Select World:") .. "]" ..
		"checkbox[0.25,0.25;cb_creative_mode;" .. fgettext("Creative Mode") .. ";" ..
		dump(core.settings:get_bool("creative_mode")) .. "]" ..
		"checkbox[0.25,0.7;cb_enable_damage;" .. fgettext("Enable Damage") .. ";" ..
		dump(core.settings:get_bool("enable_damage")) .. "]" ..
		"checkbox[0.25,1.15;cb_server_announce;" .. fgettext("Public") .. ";" ..
		dump(core.settings:get_bool("server_announce")) .. "]" ..
		"label[0.25,2.2;" .. fgettext("Name/Password") .. "]" ..
		"field[0.55,3.2;3.5,0.5;te_playername;;" ..
		core.formspec_escape(core.settings:get("name")) .. "]" ..
		"pwdfield[0.55,4;3.5,0.5;te_passwd;]"

	local bind_addr = core.settings:get("bind_address")
	if bind_addr ~= nil and bind_addr ~= "" then
		retval = retval ..
			"field[0.55,5.2;2.25,0.5;te_serveraddr;" .. fgettext("Bind Address") .. ";" ..
			core.formspec_escape(core.settings:get("bind_address")) .. "]" ..
			"field[2.8,5.2;1.25,0.5;te_serverport;" .. fgettext("Port") .. ";" ..
			core.formspec_escape(core.settings:get("port")) .. "]"
	else
		retval = retval ..
			"field[0.55,5.2;3.5,0.5;te_serverport;" .. fgettext("Server Port") .. ";" ..
			core.formspec_escape(core.settings:get("port")) .. "]"
	end

	retval = retval ..
		"textlist[4,0.25;7.5,3.7;srv_worlds;" ..
		menu_render_worldlist() ..
		";" .. index .. "]"
	
=======
	if pkgmgr.global_mods == nil then
		pkgmgr.refresh_globals()
	end
	if pkgmgr.games == nil then
		pkgmgr.update_gamelist()
	end

	if packages == nil then
		packages_raw = {}
		table.insert_all(packages_raw, pkgmgr.games)
		table.insert_all(packages_raw, pkgmgr.get_texture_packs())
		table.insert_all(packages_raw, pkgmgr.global_mods:get_list())

		local function get_data()
			return packages_raw
		end

		local function is_equal(element, uid) --uid match
			return (element.type == "game" and element.id == uid) or
					element.name == uid
		end

		packages = filterlist.create(get_data, pkgmgr.compare_package,
				is_equal, nil, {})
	end

	if tabdata.selected_pkg == nil then
		tabdata.selected_pkg = 1
	end


	local retval =
		"label[0.05,-0.25;".. fgettext("Installed Packages:") .. "]" ..
		"tablecolumns[color;tree;text]" ..
		"table[0,0.25;5.1,4.3;pkglist;" ..
		pkgmgr.render_packagelist(packages) ..
		";" .. tabdata.selected_pkg .. "]" ..
		"button[0,4.85;5.25,0.5;btn_contentdb;".. fgettext("Browse online content") .. "]"


	local selected_pkg
	if filterlist.size(packages) >= tabdata.selected_pkg then
		selected_pkg = packages:get_list()[tabdata.selected_pkg]
	end

	if selected_pkg ~= nil then
		--check for screenshot beeing available
		local screenshotfilename = selected_pkg.path .. DIR_DELIM .. "screenshot.png"
		local screenshotfile, error = io.open(screenshotfilename, "r")

		local modscreenshot
		if error == nil then
			screenshotfile:close()
			modscreenshot = screenshotfilename
		end

		if modscreenshot == nil then
				modscreenshot = defaulttexturedir .. "no_screenshot.png"
		end

		local info = core.get_content_info(selected_pkg.path)
		local desc = fgettext("No package description available")
		if info.description and info.description:trim() ~= "" then
			desc = info.description
		end

		retval = retval ..
				"image[5.5,0;3,2;" .. core.formspec_escape(modscreenshot) .. "]" ..
				"label[8.25,0.6;" .. core.formspec_escape(selected_pkg.name) .. "]" ..
				"box[5.5,2.2;6.15,2.35;#000]"

		if selected_pkg.type == "mod" then
			if selected_pkg.is_modpack then
				retval = retval ..
					"button[8.65,4.65;3.25,1;btn_mod_mgr_rename_modpack;" ..
					fgettext("Rename") .. "]"
			else
				--show dependencies
				desc = desc .. "\n\n"
				local toadd_hard = table.concat(info.depends or {}, "\n")
				local toadd_soft = table.concat(info.optional_depends or {}, "\n")
				if toadd_hard == "" and toadd_soft == "" then
					desc = desc .. fgettext("No dependencies.")
				else
					if toadd_hard ~= "" then
						desc = desc ..fgettext("Dependencies:") ..
							"\n" .. toadd_hard
					end
					if toadd_soft ~= "" then
						if toadd_hard ~= "" then
							desc = desc .. "\n\n"
						end
						desc = desc .. fgettext("Optional dependencies:") ..
							"\n" .. toadd_soft
					end
				end
			end

		else
			if selected_pkg.type == "txp" then
				if selected_pkg.enabled then
					retval = retval ..
						"button[8.65,4.65;3.25,1;btn_mod_mgr_disable_txp;" ..
						fgettext("Disable Texture Pack") .. "]"
				else
					retval = retval ..
						"button[8.65,4.65;3.25,1;btn_mod_mgr_use_txp;" ..
						fgettext("Use Texture Pack") .. "]"
				end
			end
		end

		retval = retval .. "textarea[5.85,2.2;6.35,2.9;;" ..
			fgettext("Information:") .. ";" .. desc .. "]"

		if core.may_modify_path(selected_pkg.path) then
			retval = retval ..
				"button[5.5,4.65;3.25,1;btn_mod_mgr_delete_mod;" ..
				fgettext("Uninstall Package") .. "]"
		end
	end
>>>>>>> 5.5.0:builtin/mainmenu/tab_content.lua
	return retval
end

--------------------------------------------------------------------------------
<<<<<<< HEAD:builtin/mainmenu/tab_server.lua
local function main_button_handler(this, fields, name, tabdata)

	local world_doubleclick = false

	if fields["srv_worlds"] ~= nil then
		local event = core.explode_textlist_event(fields["srv_worlds"])
		local selected = core.get_textlist_index("srv_worlds")

		menu_worldmt_legacy(selected)

		if event.type == "DCL" then
			world_doubleclick = true
		end
		if event.type == "CHG" then
			core.settings:set("mainmenu_last_selected_world",
				menudata.worldlist:get_raw_index(core.get_textlist_index("srv_worlds")))
			return true
		end
	end

	if menu_handle_key_up_down(fields,"srv_worlds","mainmenu_last_selected_world") then
		return true
	end

	if fields["cb_creative_mode"] then
		core.settings:set("creative_mode", fields["cb_creative_mode"])
		local selected = core.get_textlist_index("srv_worlds")
		menu_worldmt(selected, "creative_mode", fields["cb_creative_mode"])

		return true
	end

	if fields["cb_enable_damage"] then
		core.settings:set("enable_damage", fields["cb_enable_damage"])
		local selected = core.get_textlist_index("srv_worlds")
		menu_worldmt(selected, "enable_damage", fields["cb_enable_damage"])

		return true
	end

	if fields["cb_server_announce"] then
		core.settings:set("server_announce", fields["cb_server_announce"])
		local selected = core.get_textlist_index("srv_worlds")
		menu_worldmt(selected, "server_announce", fields["cb_server_announce"])

		return true
	end

	if fields["start_server"] ~= nil or
		world_doubleclick or
		fields["key_enter"] then
		local selected = core.get_textlist_index("srv_worlds")
		gamedata.selected_world = menudata.worldlist:get_raw_index(selected)
		if selected ~= nil and gamedata.selected_world ~= 0 then
			gamedata.playername     = fields["te_playername"]
			gamedata.password       = fields["te_passwd"]
			gamedata.port           = fields["te_serverport"]
			gamedata.address        = ""

			core.settings:set("port",gamedata.port)
			if fields["te_serveraddr"] ~= nil then
				core.settings:set("bind_address",fields["te_serveraddr"])
			end

			--update last game
			local world = menudata.worldlist:get_raw_element(gamedata.selected_world)
			if world then
				local game, index = gamemgr.find_by_gameid(world.gameid)
				core.settings:set("menu_last_game", game.id)
			end
			
			core.start()
=======
local function handle_doubleclick(pkg)
	if pkg.type == "txp" then
		if core.settings:get("texture_path") == pkg.path then
			core.settings:set("texture_path", "")
>>>>>>> 5.5.0:builtin/mainmenu/tab_content.lua
		else
			core.settings:set("texture_path", pkg.path)
		end
		packages = nil
	end
end

--------------------------------------------------------------------------------
local function handle_buttons(tabview, fields, tabname, tabdata)
	if fields["pkglist"] ~= nil then
		local event = core.explode_table_event(fields["pkglist"])
		tabdata.selected_pkg = event.row
		if event.type == "DCL" then
			handle_doubleclick(packages:get_list()[tabdata.selected_pkg])
		end
		return true
	end

	if fields["btn_contentdb"] ~= nil then
		local dlg = create_store_dlg()
		dlg:set_parent(tabview)
		tabview:hide()
		dlg:show()
		packages = nil
		return true
	end

	if fields["btn_mod_mgr_rename_modpack"] ~= nil then
		local mod = packages:get_list()[tabdata.selected_pkg]
		local dlg_renamemp = create_rename_modpack_dlg(mod)
		dlg_renamemp:set_parent(tabview)
		tabview:hide()
		dlg_renamemp:show()
		packages = nil
		return true
	end

	if fields["btn_mod_mgr_delete_mod"] ~= nil then
		local mod = packages:get_list()[tabdata.selected_pkg]
		local dlg_delmod = create_delete_content_dlg(mod)
		dlg_delmod:set_parent(tabview)
		tabview:hide()
		dlg_delmod:show()
		packages = nil
		return true
	end

	if fields.btn_mod_mgr_use_txp then
		local txp = packages:get_list()[tabdata.selected_pkg]
		core.settings:set("texture_path", txp.path)
		packages = nil
		return true
	end


	if fields.btn_mod_mgr_disable_txp then
		core.settings:set("texture_path", "")
		packages = nil
		return true
	end

	return false
end

--------------------------------------------------------------------------------
return {
	name = "content",
	caption = fgettext("Content"),
	cbf_formspec = get_formspec,
	cbf_button_handler = handle_buttons,
	on_change = pkgmgr.update_gamelist
}
