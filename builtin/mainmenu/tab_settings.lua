--Minetest
--Copyright (C) 2015 PilzAdam
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

local FILENAME = "settingtypes.txt"

local CHAR_CLASSES = {
	SPACE = "[%s]",
	VARIABLE = "[%w_%-%.]",
	INTEGER = "[+-]?[%d]",
	FLOAT = "[+-]?[%d%.]",
	FLAGS = "[%w_%-%.,]",
}

-- returns error message, or nil
local function parse_setting_line(settings, line, read_all, base_level, allow_secure)
	-- comment
	local comment = line:match("^#" .. CHAR_CLASSES.SPACE .. "*(.*)$")
	if comment then
		if settings.current_comment == "" then
			settings.current_comment = comment
		else
			settings.current_comment = settings.current_comment .. "\n" .. comment
		end
		return
	end

	-- clear current_comment so only comments directly above a setting are bound to it
	-- but keep a local reference to it for variables in the current line
	local current_comment = settings.current_comment
	settings.current_comment = ""

	-- empty lines
	if line:match("^" .. CHAR_CLASSES.SPACE .. "*$") then
		return
	end

	-- category
	local stars, category = line:match("^%[([%*]*)([^%]]+)%]$")
	if category then
		table.insert(settings, {
			name = category,
			level = stars:len() + base_level,
			type = "category",
		})
		return
	end

	-- settings
	local first_part, name, readable_name, setting_type = line:match("^"
			-- this first capture group matches the whole first part,
			--  so we can later strip it from the rest of the line
			.. "("
				.. "([" .. CHAR_CLASSES.VARIABLE .. "+)" -- variable name
				.. CHAR_CLASSES.SPACE .. "*"
				.. "%(([^%)]*)%)"  -- readable name
				.. CHAR_CLASSES.SPACE .. "*"
				.. "(" .. CHAR_CLASSES.VARIABLE .. "+)" -- type
				.. CHAR_CLASSES.SPACE .. "*"
			.. ")")

	if not first_part then
		return "Invalid line"
	end

	if name:match("secure%.[.]*") and not allow_secure then
		return "Tried to add \"secure.\" setting"
	end

	if readable_name == "" then
		readable_name = name
	end
	local remaining_line = line:sub(first_part:len() + 1)

	if setting_type == "int" then
		local default, min, max = remaining_line:match("^"
				-- first int is required, the last 2 are optional
				.. "(" .. CHAR_CLASSES.INTEGER .. "+)" .. CHAR_CLASSES.SPACE .. "*"
				.. "(" .. CHAR_CLASSES.INTEGER .. "*)" .. CHAR_CLASSES.SPACE .. "*"
				.. "(" .. CHAR_CLASSES.INTEGER .. "*)"
				.. "$")

		if not default or not tonumber(default) then
			return "Invalid integer setting"
		end

		min = tonumber(min)
		max = tonumber(max)
		table.insert(settings, {
			name = name,
			readable_name = readable_name,
			type = "int",
			default = default,
			min = min,
			max = max,
			comment = current_comment,
		})
		return
	end

	if setting_type == "string" or setting_type == "noise_params"
			or setting_type == "key" or setting_type == "v3f" then
		local default = remaining_line:match("^(.*)$")

		if not default then
			return "Invalid string setting"
		end
		if setting_type == "key" and not read_all then
			-- ignore key type if read_all is false
			return
		end

		table.insert(settings, {
			name = name,
			readable_name = readable_name,
			type = setting_type,
			default = default,
			comment = current_comment,
		})
		return
	end

	if setting_type == "bool" then
		if remaining_line == "0" then remaining_line = "false" end
		if remaining_line == "1" then remaining_line = "true" end
		if remaining_line ~= "false" and remaining_line ~= "true" then
			return "Invalid boolean setting"
		end

		table.insert(settings, {
			name = name,
			readable_name = readable_name,
			type = "bool",
			default = remaining_line,
			comment = current_comment,
		})
		return
	end

	if setting_type == "float" then
		local default, min, max = remaining_line:match("^"
				-- first float is required, the last 2 are optional
				.. "(" .. CHAR_CLASSES.FLOAT .. "+)" .. CHAR_CLASSES.SPACE .. "*"
				.. "(" .. CHAR_CLASSES.FLOAT .. "*)" .. CHAR_CLASSES.SPACE .. "*"
				.. "(" .. CHAR_CLASSES.FLOAT .. "*)"
				.."$")

		if not default or not tonumber(default) then
			return "Invalid float setting"
		end

		min = tonumber(min)
		max = tonumber(max)
		table.insert(settings, {
			name = name,
			readable_name = readable_name,
			type = "float",
			default = default,
			min = min,
			max = max,
			comment = current_comment,
		})
		return
	end

	if setting_type == "enum" then
		local default, values = remaining_line:match("^"
				-- first value (default) may be empty (i.e. is optional)
				.. "(" .. CHAR_CLASSES.VARIABLE .. "*)" .. CHAR_CLASSES.SPACE .. "*"
				.. "(" .. CHAR_CLASSES.FLAGS .. "+)"
				.. "$")

		if not default or values == "" then
			return "Invalid enum setting"
		end

		table.insert(settings, {
			name = name,
			readable_name = readable_name,
			type = "enum",
			default = default,
			values = values:split(",", true),
			comment = current_comment,
		})
		return
	end

	if setting_type == "path" then
		local default = remaining_line:match("^(.*)$")

		if not default then
			return "Invalid path setting"
		end

		table.insert(settings, {
			name = name,
			readable_name = readable_name,
			type = "path",
			default = default,
			comment = current_comment,
		})
		return
	end

	if setting_type == "flags" then
		local default, possible = remaining_line:match("^"
				-- first value (default) may be empty (i.e. is optional)
				-- this is implemented by making the last value optional, and
				-- swapping them around if it turns out empty.
				.. "(" .. CHAR_CLASSES.FLAGS .. "+)" .. CHAR_CLASSES.SPACE .. "*"
				.. "(" .. CHAR_CLASSES.FLAGS .. "*)"
				.. "$")

		if not default or not possible then
			return "Invalid flags setting"
		end

		if possible == "" then
			possible = default
			default = ""
		end

		table.insert(settings, {
			name = name,
			readable_name = readable_name,
			type = "flags",
			default = default,
			possible = possible,
			comment = current_comment,
		})
		return
	end

	return "Invalid setting type \"" .. setting_type .. "\""
end

local function parse_single_file(file, filepath, read_all, result, base_level, allow_secure)
	-- store this helper variable in the table so it's easier to pass to parse_setting_line()
	result.current_comment = ""

	local line = file:read("*line")
	while line do
		local error_msg = parse_setting_line(result, line, read_all, base_level, allow_secure)
		if error_msg then
			core.log("error", error_msg .. " in " .. filepath .. " \"" .. line .. "\"")
		end
		line = file:read("*line")
	end

	result.current_comment = nil
end

-- read_all: whether to ignore certain setting types for GUI or not
-- parse_mods: whether to parse settingtypes.txt in mods and games
local function parse_config_file(read_all, parse_mods)
	local builtin_path = core.get_builtin_path() .. DIR_DELIM .. FILENAME
	local file = io.open(builtin_path, "r")
	local settings = {}
	if not file then
		core.log("error", "Can't load " .. FILENAME)
		return settings
	end

	parse_single_file(file, builtin_path, read_all, settings, 0, true)

	file:close()

	if parse_mods then
		-- Parse games
		local games_category_initialized = false
		local index = 1
		local game = gamemgr.get_game(index)
		while game do
			local path = game.path .. DIR_DELIM .. FILENAME
			local file = io.open(path, "r")
			if file then
				if not games_category_initialized then
					local translation = fgettext_ne("Games"), -- not used, but needed for xgettext
					table.insert(settings, {
						name = "Games",
						level = 0,
						type = "category",
					})
					games_category_initialized = true
				end

				table.insert(settings, {
					name = game.name,
					level = 1,
					type = "category",
				})

				parse_single_file(file, path, read_all, settings, 2, false)

				file:close()
			end

			index = index + 1
			game = gamemgr.get_game(index)
		end

		-- Parse mods
		local mods_category_initialized = false
		local mods = {}
		get_mods(core.get_modpath(), mods)
		for _, mod in ipairs(mods) do
			local path = mod.path .. DIR_DELIM .. FILENAME
			local file = io.open(path, "r")
			if file then
				if not mods_category_initialized then
					local translation = fgettext_ne("Mods"), -- not used, but needed for xgettext
					table.insert(settings, {
						name = "Mods",
						level = 0,
						type = "category",
					})
					mods_category_initialized = true
				end

				table.insert(settings, {
					name = mod.name,
					level = 1,
					type = "category",
				})

				parse_single_file(file, path, read_all, settings, 2, false)

				file:close()
			end
		end
	end

	return settings
end

local settings = parse_config_file(false, true)
local selected_setting = 1

local function get_current_value(setting)
	local value = core.setting_get(setting.name)
	if value == nil then
		value = setting.default
	end
	return value
end

local function create_change_setting_formspec(dialogdata)
	local setting = settings[selected_setting]
	local formspec = "size[10,5.2,true]" ..
			"button[5,4.5;2,1;btn_done;" .. fgettext("Save") .. "]" ..
			"button[3,4.5;2,1;btn_cancel;" .. fgettext("Cancel") .. "]" ..
			"tablecolumns[color;text]" ..
			"tableoptions[background=#00000000;highlight=#00000000;border=false]" ..
			"table[0,0;10,3;info;"

	if setting.readable_name then
		formspec = formspec .. "#FFFF00," .. fgettext(setting.readable_name)
				.. " (" .. core.formspec_escape(setting.name) .. "),"
	else
		formspec = formspec .. "#FFFF00," .. core.formspec_escape(setting.name) .. ","
	end

	formspec = formspec .. ",,"

	local comment_text = ""

	if setting.comment == "" then
		comment_text = fgettext_ne("(No description of setting given)")
	else
		comment_text = fgettext_ne(setting.comment)
	end
	for _, comment_line in ipairs(comment_text:split("\n", true)) do
		formspec = formspec .. "," .. core.formspec_escape(comment_line) .. ","
	end

	if setting.type == "flags" then
		formspec = formspec .. ",,"
				.. "," .. fgettext("Please enter a comma seperated list of flags.") .. ","
				.. "," .. fgettext("Possible values are: ")
				.. core.formspec_escape(setting.possible:gsub(",", ", ")) .. ","
	elseif setting.type == "noise_params" then
		formspec = formspec .. ",,"
				.. "," .. fgettext("Format: <offset>, <scale>, (<spreadX>, <spreadY>, <spreadZ>), <seed>, <octaves>, <persistence>") .. ","
				.. "," .. fgettext("Optionally the lacunarity can be appended with a leading comma.") .. ","
	elseif setting.type == "v3f" then
		formspec = formspec .. ",,"
				.. "," .. fgettext_ne("Format is 3 numbers separated by commas and inside brackets.") .. ","
	end

	formspec = formspec:sub(1, -2) -- remove trailing comma

	formspec = formspec .. ";1]"

	if setting.type == "bool" then
		local selected_index
		if core.is_yes(get_current_value(setting)) then
			selected_index = 2
		else
			selected_index = 1
		end
		formspec = formspec .. "dropdown[0.5,3.5;3,1;dd_setting_value;"
				.. fgettext("Disabled") .. "," .. fgettext("Enabled") .. ";"
				.. selected_index .. "]"

	elseif setting.type == "enum" then
		local selected_index = 0
		formspec = formspec .. "dropdown[0.5,3.5;3,1;dd_setting_value;"
		for index, value in ipairs(setting.values) do
			-- translating value is not possible, since it's the value
			--  that we set the setting to
			formspec = formspec ..  core.formspec_escape(value) .. ","
			if get_current_value(setting) == value then
				selected_index = index
			end
		end
		if #setting.values > 0 then
			formspec = formspec:sub(1, -2) -- remove trailing comma
		end
		formspec = formspec .. ";" .. selected_index .. "]"

	elseif setting.type == "path" then
		local current_value = dialogdata.selected_path
		if not current_value then
			current_value = get_current_value(setting)
		end
		formspec = formspec .. "field[0.5,4;7.5,1;te_setting_value;;"
				.. core.formspec_escape(current_value) .. "]"
				.. "button[8,3.75;2,1;btn_browser_path;" .. fgettext("Browse") .. "]"

	else
		-- TODO: fancy input for float, int, flags, noise_params, v3f
		local width = 10
		local text = get_current_value(setting)
		if dialogdata.error_message then
			formspec = formspec .. "tablecolumns[color;text]" ..
			"tableoptions[background=#00000000;highlight=#00000000;border=false]" ..
			"table[5,3.9;5,0.6;error_message;#FF0000,"
					.. core.formspec_escape(dialogdata.error_message) .. ";0]"
			width = 5
			if dialogdata.entered_text then
				text = dialogdata.entered_text
			end
		end
		formspec = formspec .. "field[0.5,4;" .. width .. ",1;te_setting_value;;"
				.. core.formspec_escape(text) .. "]"
	end
	return formspec
end

local function handle_change_setting_buttons(this, fields)
	if fields["btn_done"] or fields["key_enter"] then
		local setting = settings[selected_setting]
		if setting.type == "bool" then
			local new_value = fields["dd_setting_value"]
			-- Note: new_value is the actual (translated) value shown in the dropdown
			core.setting_setbool(setting.name, new_value == fgettext("Enabled"))

		elseif setting.type == "enum" then
			local new_value = fields["dd_setting_value"]
			core.setting_set(setting.name, new_value)

		elseif setting.type == "int" then
			local new_value = tonumber(fields["te_setting_value"])
			if not new_value or math.floor(new_value) ~= new_value then
				this.data.error_message = fgettext_ne("Please enter a valid integer.")
				this.data.entered_text = fields["te_setting_value"]
				core.update_formspec(this:get_formspec())
				return true
			end
			if setting.min and new_value < setting.min then
				this.data.error_message = fgettext_ne("The value must be greater than $1.", setting.min)
				this.data.entered_text = fields["te_setting_value"]
				core.update_formspec(this:get_formspec())
				return true
			end
			if setting.max and new_value > setting.max then
				this.data.error_message = fgettext_ne("The value must be lower than $1.", setting.max)
				this.data.entered_text = fields["te_setting_value"]
				core.update_formspec(this:get_formspec())
				return true
			end
			core.setting_set(setting.name, new_value)

		elseif setting.type == "float" then
			local new_value = tonumber(fields["te_setting_value"])
			if not new_value then
				this.data.error_message = fgettext_ne("Please enter a valid number.")
				this.data.entered_text = fields["te_setting_value"]
				core.update_formspec(this:get_formspec())
				return true
			end
			core.setting_set(setting.name, new_value)

		elseif setting.type == "flags" then
			local new_value = fields["te_setting_value"]
			for _,value in ipairs(new_value:split(",", true)) do
				value = value:trim()
				local possible = "," .. setting.possible .. ","
				if not possible:find("," .. value .. ",", 0, true) then
					this.data.error_message = fgettext_ne("\"$1\" is not a valid flag.", value)
					this.data.entered_text = fields["te_setting_value"]
					core.update_formspec(this:get_formspec())
					return true
				end
			end
			core.setting_set(setting.name, new_value)

		else
			local new_value = fields["te_setting_value"]
			core.setting_set(setting.name, new_value)
		end
		core.setting_save()
		this:delete()
		return true
	end

	if fields["btn_cancel"] then
		this:delete()
		return true
	end

	if fields["btn_browser_path"] then
		core.show_file_open_dialog("dlg_browse_path", fgettext_ne("Select path"))
	end

	if fields["dlg_browse_path_accepted"] then
		this.data.selected_path = fields["dlg_browse_path_accepted"]
		core.update_formspec(this:get_formspec())
	end

	return false
end

local function create_settings_formspec(tabview, name, tabdata)
	local formspec = "tablecolumns[color;tree;text;text]" ..
					"tableoptions[background=#00000000;border=false]" ..
					"table[0,0;12,4.5;list_settings;"

	local current_level = 0
	for _, entry in ipairs(settings) do
		local name
		if not core.setting_getbool("main_menu_technical_settings") and entry.readable_name then
			name = fgettext_ne(entry.readable_name)
		else
			name = entry.name
		end

		if entry.type == "category" then
			current_level = entry.level
			formspec = formspec .. "#FFFF00," .. current_level .. "," .. fgettext(name) .. ",,"

		elseif entry.type == "bool" then
			local value = get_current_value(entry)
			if core.is_yes(value) then
				value = fgettext("Enabled")
			else
				value = fgettext("Disabled")
			end
			formspec = formspec .. "," .. (current_level + 1) .. "," .. core.formspec_escape(name) .. ","
					.. value .. ","

		elseif entry.type == "key" then
			-- ignore key settings, since we have a special dialog for them

		else
			formspec = formspec .. "," .. (current_level + 1) .. "," .. core.formspec_escape(name) .. ","
					.. core.formspec_escape(get_current_value(entry)) .. ","
		end
	end

	if #settings > 0 then
		formspec = formspec:sub(1, -2) -- remove trailing comma
	end
	formspec = formspec .. ";" .. selected_setting .. "]" ..
			"button[4,4.5;3,1;btn_change_keys;".. fgettext("Change keys") .. "]" ..
			"button[10,4.5;2,1;btn_edit;" .. fgettext("Edit") .. "]" ..
			"button[7,4.5;3,1;btn_restore;" .. fgettext("Restore Default") .. "]" ..
			"checkbox[0,4.5;cb_tech_settings;" .. fgettext("Show technical names") .. ";"
					.. dump(core.setting_getbool("main_menu_technical_settings")) .. "]"

	return formspec
end

local function handle_settings_buttons(this, fields, tabname, tabdata)
	local list_enter = false
	if fields["list_settings"] then
		selected_setting = core.get_table_index("list_settings")
		if  core.explode_table_event(fields["list_settings"]).type == "DCL" then
			-- Directly toggle booleans
			local setting = settings[selected_setting]
			if setting.type == "bool" then
				local current_value = get_current_value(setting)
				core.setting_setbool(setting.name, not core.is_yes(current_value))
				core.setting_save()
				return true
			else
				list_enter = true
			end
		else
			return true
		end
	end

	if fields["btn_edit"] or list_enter then
		local setting = settings[selected_setting]
		if setting.type ~= "category" then
			local edit_dialog = dialog_create("change_setting", create_change_setting_formspec,
					handle_change_setting_buttons)
			edit_dialog:set_parent(this)
			this:hide()
			edit_dialog:show()
		end
		return true
	end

	if fields["btn_restore"] then
		local setting = settings[selected_setting]
		if setting.type ~= "category" then
			core.setting_set(setting.name, setting.default)
			core.setting_save()
			core.update_formspec(this:get_formspec())
		end
		return true
	end

	if fields["btn_change_keys"] then
		core.show_keys_menu()
		return true
	end

	if fields["cb_tech_settings"] then
		core.setting_set("main_menu_technical_settings", fields["cb_tech_settings"])
		core.setting_save()
		core.update_formspec(this:get_formspec())
		return true
	end

	return false
end

tab_settings = {
	name = "settings",
	caption = fgettext("Settings"),
	cbf_formspec = create_settings_formspec,
	cbf_button_handler = handle_settings_buttons,
}

local function create_minetest_conf_example()
	local result = "#    This file contains a list of all available settings and their default value for freeminer.conf\n" ..
			"\n" ..
			"#    By default, all the settings are commented and not functional.\n" ..
			"#    Uncomment settings by removing the preceding #.\n" ..
			"\n" ..
			"#    freeminer.conf is read by default from:\n" ..
			"#    ../freeminer.conf\n" ..
			"#    ../../freeminer.conf\n" ..
			"#    Any other path can be chosen by passing the path as a parameter\n" ..
			"#    to the program, eg. \"freeminer.exe --config ../freeminer.conf.example\".\n" ..
			"\n" ..
			"#    Further documentation:\n" ..
			"#    http://wiki.minetest.net/\n" ..
			"\n"

	local settings = parse_config_file(true, false)
	for _, entry in ipairs(settings) do
		if entry.type == "category" then
			if entry.level == 0 then
				result = result .. "#\n# " .. entry.name .. "\n#\n\n"
			else
				for i = 1, entry.level do
					result = result .. "#"
				end
				result = result .. "# " .. entry.name .. "\n\n"
			end
		else
			if entry.comment ~= "" then
				for _, comment_line in ipairs(entry.comment:split("\n", true)) do
					result = result .."#    " .. comment_line .. "\n"
				end
			end
			result = result .. "#    type: " .. entry.type
			if entry.min then
				result = result .. " min: " .. entry.min
			end
			if entry.max then
				result = result .. " max: " .. entry.max
			end
			if entry.values then
				result = result .. " values: " .. table.concat(entry.values, ", ")
			end
			if entry.possible then
				result = result .. " possible values: " .. entry.possible:gsub(",", ", ")
			end
			result = result .. "\n"
			result = result .. "# " .. entry.name .. " = ".. entry.default .. "\n\n"
		end
	end
	return result
end

local function create_translation_file()
	local result = "// This file is automatically generated\n" ..
			"// It conatins a bunch of fake gettext calls, to tell xgettext about the strings in config files\n" ..
			"// To update it, refer to the bottom of builtin/mainmenu/tab_settings.lua\n\n" ..
			"fake_function() {\n"

	local settings = parse_config_file(true, false)
	for _, entry in ipairs(settings) do
		if entry.type == "category" then
			local name_escaped = entry.name:gsub("\"", "\\\"")
			result = result .. "\tgettext(\"" .. name_escaped .. "\");\n"
		else
			if entry.readable_name then
				local readable_name_escaped = entry.readable_name:gsub("\"", "\\\"")
				result = result .. "\tgettext(\"" .. readable_name_escaped .. "\");\n"
			end
			if entry.comment ~= "" then
				local comment_escaped = entry.comment:gsub("\n", "\\n")
				comment_escaped = comment_escaped:gsub("\"", "\\\"")
				result = result .. "\tgettext(\"" .. comment_escaped .. "\");\n"
			end
		end
	end
	result = result .. "}\n"
	return result
end

if false then
	local file = io.open("freeminer.conf.example", "w")
	if file then
		file:write(create_minetest_conf_example())
		file:close()
	end
end

if false then
	local file = io.open("src/settings_translation_file.cpp", "w")
	if file then
		file:write(create_translation_file())
		file:close()
	end
end
