--Minetest
--Copyright (C) 2014 sapier
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

--------------------------------------------------------------------------------

local function delete_content_formspec(dialogdata)
	local retval =
		"size[11.5,4.5,true]" ..
		"label[2,2;" ..
		fgettext("Are you sure you want to delete \"$1\"?", dialogdata.content.name) .. "]"..
		"style[dlg_delete_content_confirm;bgcolor=red]" ..
		"button[3.25,3.5;2.5,0.5;dlg_delete_content_confirm;" .. fgettext("Delete") .. "]" ..
		"button[5.75,3.5;2.5,0.5;dlg_delete_content_cancel;" .. fgettext("Cancel") .. "]"

	return retval
end

--------------------------------------------------------------------------------
local function delete_content_buttonhandler(this, fields)
	if fields["dlg_delete_content_confirm"] ~= nil then

		if this.data.content.path ~= nil and
				this.data.content.path ~= "" and
				this.data.content.path ~= core.get_modpath() and
				this.data.content.path ~= core.get_gamepath() and
				this.data.content.path ~= core.get_texturepath() then
			if not core.delete_dir(this.data.content.path) then
				gamedata.errormessage = fgettext("pkgmgr: failed to delete \"$1\"", this.data.content.path)
			end

<<<<<<< HEAD:builtin/mainmenu/tab_texturepacks.lua
			retval = retval .. core.formspec_escape(v)
		end
	end

	return retval
end

--------------------------------------------------------------------------------
local function get_formspec(tabview, name, tabdata)

	local retval = "label[4,-0.25;" .. fgettext("Select texture pack:") .. "]" ..
			"textlist[4,0.25;7.5,5.0;TPs;"

	local current_texture_path = core.settings:get("texture_path")
	local list = filter_texture_pack_list(core.get_dir_list(core.get_texturepath(), true))
	local index = tonumber(core.settings:get("mainmenu_last_selected_TP"))

	if not index then index = 1 end

	if current_texture_path == "" then
		retval = retval ..
			render_texture_pack_list(list) ..
			";" .. index .. "]"
		return retval
	end

	local infofile = current_texture_path .. DIR_DELIM .. "description.txt"
	-- This adds backwards compatibility for old texture pack description files named
	-- "info.txt", and should be removed once all such texture packs have been updated
	if not file_exists(infofile) then
		infofile = current_texture_path .. DIR_DELIM .. "info.txt"
		if file_exists(infofile) then
			core.log("deprecated", "info.txt is deprecated. description.txt should be used instead.")
		end
	end

	local infotext = ""
	local f = io.open(infofile, "r")
	if not f then
		infotext = fgettext("No information available")
	else
		infotext = f:read("*all")
		f:close()
	end

	local screenfile = current_texture_path .. DIR_DELIM .. "screenshot.png"
	local no_screenshot
	if not file_exists(screenfile) then
		screenfile = nil
		no_screenshot = defaulttexturedir .. "no_screenshot.png"
	end

	return	retval ..
			render_texture_pack_list(list) ..
			";" .. index .. "]" ..
			"image[0.25,0.25;4.05,2.7;" .. core.formspec_escape(screenfile or no_screenshot) .. "]" ..
			"textarea[0.6,2.85;3.7,1.5;;" .. core.formspec_escape(infotext or "") .. ";]"
end

--------------------------------------------------------------------------------
local function main_button_handler(tabview, fields, name, tabdata)
	if fields["TPs"] then
		local event = core.explode_textlist_event(fields["TPs"])
		if event.type == "CHG" or event.type == "DCL" then
			local index = core.get_textlist_index("TPs")
			core.settings:set("mainmenu_last_selected_TP", index)
			local list = filter_texture_pack_list(core.get_dir_list(core.get_texturepath(), true))
			local current_index = core.get_textlist_index("TPs")
			if current_index and #list >= current_index then
				local new_path = core.get_texturepath() .. DIR_DELIM .. list[current_index]
				if list[current_index] == fgettext("None") then
					new_path = ""
				end
				core.settings:set("texture_path", new_path)
=======
			if this.data.content.type == "game" then
				pkgmgr.update_gamelist()
			else
				pkgmgr.refresh_globals()
>>>>>>> 5.5.0:builtin/mainmenu/dlg_delete_content.lua
			end
		else
			gamedata.errormessage = fgettext("pkgmgr: invalid path \"$1\"", this.data.content.path)
		end
		this:delete()
		return true
	end

	if fields["dlg_delete_content_cancel"] then
		this:delete()
		return true
	end

	return false
end

--------------------------------------------------------------------------------
function create_delete_content_dlg(content)
	assert(content.name)

	local retval = dialog_create("dlg_delete_content",
					delete_content_formspec,
					delete_content_buttonhandler,
					nil)
	retval.data.content = content
	return retval
end
