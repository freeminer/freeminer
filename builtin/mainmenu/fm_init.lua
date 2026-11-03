-- Use the current main menu and its dialogs with Freeminer's green sidebar.
fm_mainmenu_sidebar_width = 4.4
function fm_customize_mainmenu(tabview)
	local original_get_formspec = tabview.get_formspec
	local original_handle_tab_buttons = tabview.handle_tab_buttons
	local original_set_game = mm_game_theme.set_game
	local original_set_engine = mm_game_theme.set_engine
	local sidebar_width = fm_mainmenu_sidebar_width
	local game_banner
	local base_height = MAIN_TAB_H
	local stretch_elements = {table = true, textlist = true, textarea = true,
		box = true, scroll_container = true}

	local function show_clouds()
		mm_game_theme.clear_single("background")
		mm_game_theme.clear_single("overlay")
		core.set_clouds(true)
	end

	function mm_game_theme.set_engine(...)
		original_set_engine(...)
		show_clouds()
	end

	show_clouds()

	local function stretch_content(formspec, scale)
		return (formspec:gsub("([%a_]+)%[([^%]]*)%]", function(element, body)
			local x, y, rest = body:match("^([%-%d%.]+),([%-%d%.]+)(;.*)$")
			if not x then
				return element .. "[" .. body .. "]"
			end
			if stretch_elements[element] then
				rest = rest:gsub("^;([%d%.]+),([%d%.]+);", function(w, h)
					return (";%s,%f;"):format(w, tonumber(h) * scale)
				end, 1)
			end
			return ("%s[%s,%f%s]"):format(element, x, tonumber(y) * scale, rest)
		end))
	end

	function mm_game_theme.set_game(game)
		local set_background = core.set_background
		if mm_game_theme.gameid ~= game.id then
			game_banner = nil
		end
		core.set_background = function(layer, path, ...)
			local loaded = set_background(layer, path, ...)
			if layer == "header" and path ~= "" and loaded then
				game_banner = path
			end
			return loaded
		end
		local ok, err = pcall(original_set_game, game)
		core.set_background = set_background
		if not ok then
			error(err)
		end
		core.set_topleft_text("")
		mm_game_theme.clear_single("header")
		show_clouds()
	end

	function tabview:tab_header(size)
		local result = {
			("style_type[label;font=bold;font_size=+8]label[%f,%f;FREEMINER]" ..
					"style_type[label;font=;font_size=]"):format(
				-sidebar_width + 0.3, -TABHEADER_H + 0.45),
			("box[%f,%f;%f,0.025;#a3d1aaff]"):format(
				-sidebar_width, 0.4, sidebar_width),
		}
		if self.current_tab == "local" and mm_game_theme.gameid then
			if game_banner then
				result[#result + 1] = ("style_type[image;noclip=true]" ..
						"image[0.2,-2.1;%f,0.9;%s]style_type[image;noclip=false]"):format(
						math.min(self.width - 0.4, 10), core.formspec_escape(game_banner))
			end
			local game = pkgmgr.find_by_gameid(mm_game_theme.gameid)
			if game then
				result[#result + 1] = ("label[%f,0.1;%s]"):format(
						-sidebar_width + 0.3, core.formspec_escape(game.name))
			end
		end
		for i, tab in ipairs(self.tablist) do
			local caption = type(tab.caption) == "function" and tab.caption(self) or tab.caption
			local name = "fm_maintab_" .. i
			local y = 0.65 + (i - 1) * 0.7
			if i == self.last_tab_index then
				result[#result + 1] = ("box[%f,%f;%f,0.65;#438b4aff]"):format(
						-sidebar_width + 0.15, y, sidebar_width - 0.3)
			end
			result[#result + 1] = ("label[%f,%f;%s]"):format(-sidebar_width + 0.4, y + 0.32, caption)
			result[#result + 1] = ("image_button[%f,%f;%f,0.65;%s;%s;;true;false]"):format(
				-sidebar_width + 0.15, y, sidebar_width - 0.3,
				core.formspec_escape(defaulttexturedir .. "blank.png"), name)
		end
		if self.end_button then
			local y = 0.65 + #self.tablist * 0.7
			result[#result + 1] = ("label[%f,%f;%s]"):format(
				-sidebar_width + 0.4, y + 0.32, self.end_button.label)
			result[#result + 1] = ("image_button[%f,%f;%f,0.65;%s;%s;;true;false]"):format(
				-sidebar_width + 0.15, y, sidebar_width - 0.3,
				core.formspec_escape(defaulttexturedir .. "blank.png"), self.end_button.name)
		end
		return table.concat(result)
	end

	function tabview:handle_tab_buttons(fields)
		for i = 1, #self.tablist do
			if fields["fm_maintab_" .. i] then
				return original_handle_tab_buttons(self, {[self.name] = tostring(i)})
			end
		end
		return original_handle_tab_buttons(self, fields)
	end

	function tabview:get_formspec()
		local window = core.get_window_info()
		local offset = core.settings:get_bool("touch_gui") and GAMEBAR_OFFSET_TOUCH or GAMEBAR_OFFSET_DESKTOP
		MAIN_TAB_H = math.max(base_height, window.max_formspec_size.y * 0.8 -
				TABHEADER_H - offset - GAMEBAR_H)
		self.height = MAIN_TAB_H
		local bar = ui.find_by_name("game_button_bar")
		if bar then
			bar.pos.y = MAIN_TAB_H + TABHEADER_H + offset
		end
		local tab = self.tablist[self.last_tab_index]
		local get_content = tab.get_formspec
		tab.get_formspec = function(...)
			local content, prepend = get_content(...)
			return stretch_content(content, MAIN_TAB_H / base_height), prepend
		end
		local ok, formspec = pcall(original_get_formspec, self)
		tab.get_formspec = get_content
		if not ok then
			error(formspec)
		end
		local height
		formspec = formspec:gsub("size%[([%d.]+),([%d.]+),", function(width, h)
			height = tonumber(h)
			return ("size[%f,%s,"):format(tonumber(width) + sidebar_width, h)
		end, 1)
		if not height then
			return formspec
		end
		formspec = formspec:gsub("anchor%[0%.5,", "position[0,0.5]anchor[0,", 1)
		formspec = formspec:gsub("bgcolor%[;neither%]container%[0,", function()
			return ("bgcolor[;neither]style_type[box;noclip=true]" ..
					"box[-100,-100;%f,200;#61b16eff]style_type[box;noclip=false]" ..
					"box[%f,0;%f,%f;#101010ff]container[%f,"):format(
				100 + sidebar_width, sidebar_width, self.width, height, sidebar_width)
		end, 1)
		formspec = formspec:gsub("#0000008C", "#101010ff", 1)
		-- The current tabview also draws its settings icon in the header.
		formspec = formspec:gsub("style%[open_settings;noclip=true;border=false%].-image_button%[[^%]]+%]", "", 1)
		return formspec
	end
end

dofile(core.get_mainmenu_path() .. DIR_DELIM .. "init.lua")
