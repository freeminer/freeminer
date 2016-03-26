--Minetest
--Copyright (C) 2013 sapier
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

local labels = {
	leaves = {
		fgettext("Opaque Leaves"),
		fgettext("Simple Leaves"),
		fgettext("Fancy Leaves")
	},
	node_highlighting = {
		fgettext("Node Outlining"),
		fgettext("Node Highlighting")
	},
	filters = {
		fgettext("No Filter"),
		fgettext("Bilinear Filter"),
		fgettext("Trilinear Filter")
	},
	mipmap = {
		fgettext("No Mipmap"),
		fgettext("Mipmap"),
		fgettext("Mipmap + Aniso. Filter")
	},
	antialiasing = {
		fgettext("None"),
		fgettext("2x"),
		fgettext("4x"),
		fgettext("8x")
	}
}

local dd_options = {
	leaves = {
		table.concat(labels.leaves, ","),
		{"opaque", "simple", "fancy"}
	},
	node_highlighting = {
		table.concat(labels.node_highlighting, ","),
		{"box", "halo"}
	},
	filters = {
		table.concat(labels.filters, ","),
		{"", "bilinear_filter", "trilinear_filter"}
	},
	mipmap = {
		table.concat(labels.mipmap, ","),
		{"", "mip_map", "anisotropic_filter"}
	},
	antialiasing = {
		table.concat(labels.antialiasing, ","),
		{"0", "2", "4", "8"}
	}
}

local getSettingIndex = {
	Leaves = function()
		local style = core.setting_get("leaves_style")
		for idx, name in pairs(dd_options.leaves[2]) do
			if style == name then return idx end
		end
		return 1
	end,
	NodeHighlighting = function()
		local style = core.setting_get("node_highlighting")
		for idx, name in pairs(dd_options.node_highlighting[2]) do
			if style == name then return idx end
		end
		return 1
	end,
	Filter = function()
		if core.setting_get(dd_options.filters[2][3]) == "true" then
			return 3
		elseif core.setting_get(dd_options.filters[2][3]) == "false" and
				core.setting_get(dd_options.filters[2][2]) == "true" then
			return 2
		end
		return 1
	end,
	Mipmap = function()
		if core.setting_get(dd_options.mipmap[2][3]) == "true" then
			return 3
		elseif core.setting_get(dd_options.mipmap[2][3]) == "false" and
				core.setting_get(dd_options.mipmap[2][2]) == "true" then
			return 2
		end
		return 1
	end,
	Antialiasing = function()
		local antialiasing_setting = core.setting_get("fsaa")
		for i = 1, #dd_options.antialiasing[2] do
			if antialiasing_setting == dd_options.antialiasing[2][i] then
				return i
			end
		end
		return 1
	end
}

local function antialiasing_fname_to_name(fname)
	for i = 1, #labels.antialiasing do
		if fname == labels.antialiasing[i] then
			return dd_options.antialiasing[2][i]
		end
	end
	return 0
end

local function dlg_confirm_reset_formspec(data)
	return  "size[8,3]" ..
		"label[1,1;" .. fgettext("Are you sure to reset your singleplayer world?") .. "]" ..
		"button[1,2;2.6,0.5;dlg_reset_singleplayer_confirm;" .. fgettext("Yes") .. "]" ..
		"button[4,2;2.8,0.5;dlg_reset_singleplayer_cancel;" .. fgettext("No") .. "]"
end

local function dlg_confirm_reset_btnhandler(this, fields, dialogdata)

	if fields["dlg_reset_singleplayer_confirm"] ~= nil then
		local worldlist = core.get_worlds()
		local found_singleplayerworld = false

		for i = 1, #worldlist do
			if worldlist[i].name == "singleplayerworld" then
				found_singleplayerworld = true
				gamedata.worldindex = i
			end
		end

		if found_singleplayerworld then
			core.delete_world(gamedata.worldindex)
		end

		core.create_world("singleplayerworld", 1)
		worldlist = core.get_worlds()
		found_singleplayerworld = false

		for i = 1, #worldlist do
			if worldlist[i].name == "singleplayerworld" then
				found_singleplayerworld = true
				gamedata.worldindex = i
			end
		end
	end

	this.parent:show()
	this:hide()
	this:delete()
	return true
end

local function showconfirm_reset(tabview)
	local new_dlg = dialog_create("reset_spworld",
		dlg_confirm_reset_formspec,
		dlg_confirm_reset_btnhandler,
		nil)
	new_dlg:set_parent(tabview)
	tabview:hide()
	new_dlg:show()
end

local function formspec(tabview, name, tabdata)
	local tab_string =
		"box[0,0;3.5,4.5;#999999]" ..
		"checkbox[0.25,0;cb_smooth_lighting;" .. fgettext("Smooth Lighting") .. ";"
				.. dump(core.setting_getbool("smooth_lighting")) .. "]" ..
		"checkbox[0.25,0.5;cb_particles;" .. fgettext("Particles") .. ";"
				.. dump(core.setting_getbool("enable_particles")) .. "]" ..
		"checkbox[0.25,1;cb_3d_clouds;" .. fgettext("3D Clouds") .. ";"
				.. dump(core.setting_getbool("enable_3d_clouds")) .. "]" ..
		"checkbox[0.25,1.5;cb_opaque_water;" .. fgettext("Opaque Water") .. ";"
				.. dump(core.setting_getbool("opaque_water")) .. "]" ..
		"checkbox[0.25,2.0;cb_connected_glass;" .. fgettext("Connected Glass") .. ";"
				.. dump(core.setting_getbool("connected_glass")) .. "]" ..
		"dropdown[0.25,2.8;3.3;dd_node_highlighting;" .. dd_options.node_highlighting[1] .. ";"
				.. getSettingIndex.NodeHighlighting() .. "]" ..
		"dropdown[0.25,3.6;3.3;dd_leaves_style;" .. dd_options.leaves[1] .. ";"
				.. getSettingIndex.Leaves() .. "]" ..
		"box[3.75,0;3.75,3.45;#999999]" ..
		"label[3.85,0.1;" .. fgettext("Texturing:") .. "]" ..
		"dropdown[3.85,0.55;3.85;dd_filters;" .. dd_options.filters[1] .. ";"
				.. getSettingIndex.Filter() .. "]" ..
		"dropdown[3.85,1.35;3.85;dd_mipmap;" .. dd_options.mipmap[1] .. ";"
				.. getSettingIndex.Mipmap() .. "]" ..
		"label[3.85,2.15;" .. fgettext("Antialiasing:") .. "]" ..
		"dropdown[3.85,2.6;3.85;dd_antialiasing;" .. dd_options.antialiasing[1] .. ";"
				.. getSettingIndex.Antialiasing() .. "]" ..
		"box[7.75,0;4,4.4;#999999]" ..
		"checkbox[8,0;cb_shaders;" .. fgettext("Shaders") .. ";"
				.. dump(core.setting_getbool("enable_shaders")) .. "]"

	tab_string = tab_string ..
		"button[8,4.75;3.75,0.5;btn_change_keys;" .. fgettext("Change keys") .. "]" ..
		"button[0,4.75;3.75,0.5;btn_advanced_settings;" .. fgettext("Advanced Settings") .. "]"


	if core.setting_get("touchscreen_threshold") ~= nil then
		tab_string = tab_string ..
			"label[4.3,4.1;" .. fgettext("Touchthreshold (px)") .. "]" ..
			"dropdown[3.85,4.55;3.85;dd_touchthreshold;0,10,20,30,40,50;" ..
			((tonumber(core.setting_get("touchscreen_threshold")) / 10) + 1) .. "]"
	end

	if core.setting_getbool("enable_shaders") then
		tab_string = tab_string ..
			"checkbox[8,0.5;cb_bumpmapping;" .. fgettext("Bump Mapping") .. ";"
					.. dump(core.setting_getbool("enable_bumpmapping")) .. "]" ..
			"checkbox[8,1;cb_tonemapping;" .. fgettext("Tone Mapping") .. ";"
					.. dump(core.setting_getbool("tone_mapping")) .. "]" ..
			"checkbox[8,1.5;cb_generate_normalmaps;" .. fgettext("Normal Mapping") .. ";"
					.. dump(core.setting_getbool("generate_normalmaps")) .. "]" ..
			"checkbox[8,2;cb_parallax;" .. fgettext("Parallax Occlusion") .. ";"
					.. dump(core.setting_getbool("enable_parallax_occlusion")) .. "]" ..
			"checkbox[8,2.5;cb_waving_water;" .. fgettext("Waving Water") .. ";"
					.. dump(core.setting_getbool("enable_waving_water")) .. "]" ..
			"checkbox[8,3;cb_waving_leaves;" .. fgettext("Waving Leaves") .. ";"
					.. dump(core.setting_getbool("enable_waving_leaves")) .. "]" ..
			"checkbox[8,3.5;cb_waving_plants;" .. fgettext("Waving Plants") .. ";"
					.. dump(core.setting_getbool("enable_waving_plants")) .. "]"
	else
		tab_string = tab_string ..
			"tablecolumns[color;text]" ..
			"tableoptions[background=#00000000;highlight=#00000000;border=false]" ..
			"table[8.33,0.7;3.5,4;shaders;" ..
				"#888888," .. fgettext("Bump Mapping") .. "," ..
				"#888888," .. fgettext("Tone Mapping") .. "," ..
				"#888888," .. fgettext("Normal Mapping") .. "," ..
				"#888888," .. fgettext("Parallax Occlusion") .. "," ..
				"#888888," .. fgettext("Waving Water") .. "," ..
				"#888888," .. fgettext("Waving Leaves") .. "," ..
				"#888888," .. fgettext("Waving Plants") .. "," ..
				";1]"
	end

	return tab_string
end

--------------------------------------------------------------------------------
local function handle_settings_buttons(this, fields, tabname, tabdata)

	if fields["btn_advanced_settings"] ~= nil then
		local adv_settings_dlg = create_adv_settings_dlg()
		adv_settings_dlg:set_parent(this)
		this:hide()
		adv_settings_dlg:show()
		--mm_texture.update("singleplayer", current_game())
		return true
	end
	if fields["cb_smooth_lighting"] then
		core.setting_set("smooth_lighting", fields["cb_smooth_lighting"])
		return true
	end
	if fields["cb_particles"] then
		core.setting_set("enable_particles", fields["cb_particles"])
		return true
	end
	if fields["cb_3d_clouds"] then
		core.setting_set("enable_3d_clouds", fields["cb_3d_clouds"])
		return true
	end
	if fields["cb_opaque_water"] then
		core.setting_set("opaque_water", fields["cb_opaque_water"])
		return true
	end
	if fields["cb_connected_glass"] then
		core.setting_set("connected_glass", fields["cb_connected_glass"])
		return true
	end
	if fields["cb_shaders"] then
		if (core.setting_get("video_driver") == "direct3d8" or
				core.setting_get("video_driver") == "direct3d9") then
			core.setting_set("enable_shaders", "false")
			gamedata.errormessage = fgettext("To enable shaders the OpenGL driver needs to be used.")
		else
			core.setting_set("enable_shaders", fields["cb_shaders"])
		end
		return true
	end
	if fields["cb_bumpmapping"] then
		core.setting_set("enable_bumpmapping", fields["cb_bumpmapping"])
		return true
	end
	if fields["cb_tonemapping"] then
		core.setting_set("tone_mapping", fields["cb_tonemapping"])
		return true
	end
	if fields["cb_generate_normalmaps"] then
		core.setting_set("generate_normalmaps", fields["cb_generate_normalmaps"])
		return true
	end
	if fields["cb_parallax"] then
		core.setting_set("enable_parallax_occlusion", fields["cb_parallax"])
		return true
	end
	if fields["cb_waving_water"] then
		core.setting_set("enable_waving_water", fields["cb_waving_water"])
		return true
	end
	if fields["cb_waving_leaves"] then
		core.setting_set("enable_waving_leaves", fields["cb_waving_leaves"])
	end
	if fields["cb_waving_plants"] then
		core.setting_set("enable_waving_plants", fields["cb_waving_plants"])
		return true
	end
	if fields["btn_change_keys"] then
		core.show_keys_menu()
		return true
	end
	if fields["cb_touchscreen_target"] then
		core.setting_set("touchtarget", fields["cb_touchscreen_target"])
		return true
	end
	if fields["btn_reset_singleplayer"] then
		showconfirm_reset(this)
		return true
	end

	--Note dropdowns have to be handled LAST!
	local ddhandled = false

	for i = 1, #labels.leaves do
		if fields["dd_leaves_style"] == labels.leaves[i] then
			core.setting_set("leaves_style", dd_options.leaves[2][i])
			ddhandled = true
		end
	end
	for i = 1, #labels.node_highlighting do
		if fields["dd_node_highlighting"] == labels.node_highlighting[i] then
			core.setting_set("node_highlighting", dd_options.node_highlighting[2][i])
			ddhandled = true
		end
	end
	if fields["dd_filters"] == labels.filters[1] then
		core.setting_set("bilinear_filter", "false")
		core.setting_set("trilinear_filter", "false")
		ddhandled = true
	elseif fields["dd_filters"] == labels.filters[2] then
		core.setting_set("bilinear_filter", "true")
		core.setting_set("trilinear_filter", "false")
		ddhandled = true
	elseif fields["dd_filters"] == labels.filters[3] then
		core.setting_set("bilinear_filter", "false")
		core.setting_set("trilinear_filter", "true")
		ddhandled = true
	end
	if fields["dd_mipmap"] == labels.mipmap[1] then
		core.setting_set("mip_map", "false")
		core.setting_set("anisotropic_filter", "false")
		ddhandled = true
	elseif fields["dd_mipmap"] == labels.mipmap[2] then
		core.setting_set("mip_map", "true")
		core.setting_set("anisotropic_filter", "false")
		ddhandled = true
	elseif fields["dd_mipmap"] == labels.mipmap[3] then
		core.setting_set("mip_map", "true")
		core.setting_set("anisotropic_filter", "true")
		ddhandled = true
	end
	if fields["dd_antialiasing"] then
		core.setting_set("fsaa",
			antialiasing_fname_to_name(fields["dd_antialiasing"]))
		ddhandled = true
	end
	if fields["dd_touchthreshold"] then
		core.setting_set("touchscreen_threshold", fields["dd_touchthreshold"])
		ddhandled = true
	end

	return ddhandled
end

tab_settings = {
	name = "settings",
	caption = fgettext("Settings"),
	cbf_formspec = formspec,
	cbf_button_handler = handle_settings_buttons
}
