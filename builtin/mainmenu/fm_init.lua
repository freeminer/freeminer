local menupath = core.get_mainmenu_path()..DIR_DELIM
local commonpath = core.get_builtin_path()..DIR_DELIM.."common"..DIR_DELIM

dofile(commonpath.."filterlist.lua")
dofile(commonpath.."async_event.lua")

dofile(menupath.."fm_modmgr.lua")
dofile(menupath.."fm_gamemgr.lua")

dofile(menupath.."modstore.lua")
dofile(menupath.."menubar.lua")

dofile(menupath .. DIR_DELIM .. "common.lua")

mt_color_grey  = "#AAAAAA"
mt_color_blue  = "#0000DD"
mt_color_green = "#00DD00"
mt_color_dark_green = "#003300"

menu = {}
menudata = menu

local tabbuilder = {}
local worldlist = nil

--------------------------------------------------------------------------------
-- Common/Global menu functions
--------------------------------------------------------------------------------
os.tempfolder = function()
	local filetocheck = os.tmpname()
	os.remove(filetocheck)

	local randname = "MTTempModFolder_" .. math.random(0,10000)
	if DIR_DELIM == "\\" then
		local tempfolder = os.getenv("TEMP")
		return tempfolder .. filetocheck
	else
		local backstring = filetocheck:reverse()
		return filetocheck:sub(0,filetocheck:len()-backstring:find(DIR_DELIM)+1) ..randname
	end

end

--------------------------------------------------------------------------------
function text2textlist(xpos,ypos,width,height,tl_name,textlen,text,transparency)
	local textlines = core.splittext(text,textlen)
	
	local retval = "textlist[" .. xpos .. "," .. ypos .. ";"
								.. width .. "," .. height .. ";"
								.. tl_name .. ";"
	
	for i=1, #textlines, 1 do
		textlines[i] = textlines[i]:gsub("\r","")
		retval = retval .. core.formspec_escape(textlines[i]) .. ","
	end
	
	retval = retval .. ";0;"
	
	if transparency then
		retval = retval .. "true"
	end
	
	retval = retval .. "]"

	return retval
end

--------------------------------------------------------------------------------
function menu.handle_key_up_down(fields,textlist,settingname)

	if fields["key_up"] then
		local oldidx = core.get_textlist_index(textlist)

		if oldidx ~= nil and oldidx > 1 then
			local newidx = oldidx -1
			core.setting_set(settingname,
				filterlist.get_raw_index(worldlist,newidx))
		end
	end

	if fields["key_down"] then
		local oldidx = core.get_textlist_index(textlist)

		if oldidx ~= nil and oldidx < filterlist.size(worldlist) then
			local newidx = oldidx + 1
			core.setting_set(settingname,
				filterlist.get_raw_index(worldlist,newidx))
		end
	end
end

--------------------------------------------------------------------------------
--[[ mt version used
function menu.asyncOnlineFavourites()
	menu.favorites = {}
	core.handle_async(
		function(param)
			return core.get_favorites("online")
		end,
		nil,
		function(result)
			menu.favorites = result
			core.event_handler("Refresh")
		end
		)
end
]]

--------------------------------------------------------------------------------
function menu.render_favorite(spec,render_details)
	local e = core.formspec_escape
	local text = ""

	if spec.name and spec.name ~= "" then
		text = text .. spec.name:trim()
	else
		if spec.address ~= "" then
			text = text .. spec.address:trim()

			if spec.port ~= "" and tostring(spec.port) ~= "30000" then
				text = text .. ":" .. spec.port
			end
		end
	end

--[[
	if not render_details then
		return "?,/,?,0,0,0,0," .. e(text)
	end
]]

	local row = ""
	if spec.clients ~= nil and spec.clients_max ~= nil then
		row = row .. e(spec.clients) .. ",/," .. e(spec.clients_max) .. ","
	else
		row = row .. "?,/,?,"
	end

	-- 'X and 1 or 0' will return 1 if X is true and 0 if it's false or nil
	row = row ..
		(spec.password and 1 or 0) .. "," ..
		(spec.creative and 1 or 0) .. "," ..
		(spec.damage and 1 or 0) .. "," ..
		(spec.pvp and 1 or 0) .. ","

	row = row .. e(text)

	return row
end

--------------------------------------------------------------------------------
function menu.render_world_list()
	local retval = ""

	local current_worldlist = filterlist.get_list(worldlist)

	for i,v in ipairs(current_worldlist) do
		if retval ~= "" then
			retval = retval ..","
		end

		retval = retval .. core.formspec_escape(v.name) ..
					" \\[" .. core.formspec_escape(v.gameid) .. "\\]"
	end

	return retval
end

--------------------------------------------------------------------------------
local function filter_texture_pack_list(list)
	retval = {"None"}
	for _,i in ipairs(list) do
		if i~="base" then
			table.insert(retval, i)
		end
	end
	return retval
end

--------------------------------------------------------------------------------
function menu.render_texture_pack_list(list)
	local retval = ""

	for i, v in ipairs(list) do
		if retval ~= "" then
			retval = retval ..","
		end

		retval = retval .. core.formspec_escape(v)
	end

	return retval
end

--------------------------------------------------------------------------------
function menu.lastgame()
	if menu.last_game > 0 and menu.last_game <= #gamemgr.games then
		return gamemgr.games[menu.last_game]
	end

	if #gamemgr.games >= 1 then
		menu.last_game = 1
		return gamemgr.games[menu.last_game]
	end

	--error case!!
	return nil
end



--------------------------------------------------------------------------------
-- Global tab functions
--------------------------------------------------------------------------------
function tabbuilder.gettab()
	local tsize = tabbuilder.tabsizes[tabbuilder.current_tab] or {width=12, height=5.2}
	local retval = ""

	if tabbuilder.show_buttons then
		retval = retval .. tabbuilder.tab_header()
	end

	local buildfunc = tabbuilder.tabfuncs[tabbuilder.current_tab]
	if buildfunc ~= nil then
		retval = retval .. buildfunc()
	end

	retval = retval .. modmgr.gettab(tabbuilder.current_tab)
	retval = retval .. gamemgr.gettab(tabbuilder.current_tab)
	retval = retval .. modstore.gettab(tabbuilder.current_tab)

	return retval
end

--------------------------------------------------------------------------------
function tabbuilder.tab_header()

	if tabbuilder.last_tab_index == nil then
		tabbuilder.last_tab_index = 1
	end

	local formspec = "image[-0.35," .. 1.8 + tabbuilder.last_tab_index .. ";" .. core.formspec_escape(menu.defaulttexturedir .. "selected.png") .. "]"

	for i = 1, #tabbuilder.current_buttons do
		formspec = formspec .. "label[0.35," .. 2 + i .. ";" .. tabbuilder.current_buttons[i].caption .. "]"
		formspec = formspec .. "image_button[-0.4," .. 1.85 + i .. ";6.7,1;" .. core.formspec_escape(menu.defaulttexturedir .. "blank.png") .. ";maintab_" .. i .. ";;true;false]"
	end
	return formspec
end

--------------------------------------------------------------------------------
function tabbuilder.handle_tab_buttons(fields)
	local index = nil
	local match = "maintab_"
	for idx, _ in pairs(fields) do
		if idx:sub(1, #match) == match then
			index = tonumber(idx:sub(#match + 1, #match + 1))
			break
		end
	end

	if index then
		tabbuilder.last_tab_index = index
		tabbuilder.current_tab = tabbuilder.current_buttons[index].name

		core.setting_set("main_menu_tab",tabbuilder.current_tab)
	end

	--handle tab changes
	if tabbuilder.current_tab ~= tabbuilder.old_tab then
		if tabbuilder.current_tab ~= "singleplayer" and not tabbuilder.is_dialog then
			menu.update_gametype(true)
		end
	end

	if tabbuilder.current_tab == "singleplayer" then
		menu.update_gametype()
	end

	tabbuilder.old_tab = tabbuilder.current_tab
end

--------------------------------------------------------------------------------
-- Dialogs
--------------------------------------------------------------------------------
function tabbuilder.dialog_create_world()
	local mapgens = core.get_mapgen_names()

	local current_seed = core.setting_get("fixed_map_seed") or ""
	local current_mg   = core.setting_get("mg_name")

	local mglist = ""
	local selindex = 1
	local i = 1
	for k,v in pairs(mapgens) do
		if current_mg == v then
			selindex = i
		end
		i = i + 1
		mglist = mglist .. v .. ","
	end
	mglist = mglist:sub(1, -2)

	gamemgr.update_gamelist()

	local retval =
		"label[6.5,0;" .. fgettext("World name") .. "]"..
		"field[9,0.4;6,0.5;te_world_name;;]" ..

		"label[6.5,1;" .. fgettext("Seed") .. "]"..
		"field[9,1.4;6,0.5;te_seed;;".. current_seed .. "]" ..

		"label[6.5,2;" .. fgettext("Mapgen") .. "]"..
		"dropdown[8.7,2;6.3;dd_mapgen;" .. mglist .. ";" .. selindex .. "]" ..

		"label[6.5,3;" .. fgettext("Game") .. "]"..
		"textlist[8.7,3;5.8,2.3;games;" .. gamemgr.gamelist() ..
		";" .. menu.last_game .. ";true]" ..

		"button[9.5,5.5;2.6,0.5;world_create_confirm;" .. fgettext("Create") .. "]" ..
		"button[12,5.5;2.8,0.5;world_create_cancel;" .. fgettext("Cancel") .. "]"

	return retval
end

--------------------------------------------------------------------------------
function tabbuilder.handle_create_world_buttons(fields)

	if fields["world_create_confirm"] or
		fields["key_enter"] then

		local worldname = fields["te_world_name"]
		local gameindex = core.get_textlist_index("games")

		if gameindex ~= nil and
			worldname ~= "" then

			local message = nil

			if not filterlist.uid_exists_raw(worldlist,worldname) then
				core.setting_set("mg_name",fields["dd_mapgen"])
				message = core.create_world(worldname,gameindex)
			else
				message = fgettext("A world named \"$1\" already exists", worldname)
			end

			core.setting_set("fixed_map_seed", fields["te_seed"])

			if message ~= nil then
				gamedata.errormessage = message
			else
				menu.last_game = gameindex
				core.setting_set("main_menu_last_game_idx",gameindex)

				filterlist.refresh(worldlist)
				core.setting_set("mainmenu_last_selected_world",
									filterlist.raw_index_by_uid(worldlist,worldname))
			end
		else
			gamedata.errormessage =
				fgettext("No worldname given or no game selected")
		end
	end

	if fields["games"] then
		tabbuilder.skipformupdate = true
		return
	end

	--close dialog
	tabbuilder.is_dialog = false
	tabbuilder.show_buttons = true
	tabbuilder.current_tab = core.setting_get("main_menu_tab")
end

--------------------------------------------------------------------------------
function tabbuilder.dialog_delete_world()
	return	"label[6.5,2;" ..
			fgettext("Delete World \"$1\"?", filterlist.get_raw_list(worldlist)[menu.world_to_del].name) .. "]"..
			"button[8,4.2;2.6,0.5;world_delete_confirm;" .. fgettext("Yes").. "]" ..
			"button[10.5,4.2;2.8,0.5;world_delete_cancel;" .. fgettext("No") .. "]"
end

--------------------------------------------------------------------------------
function tabbuilder.handle_delete_world_buttons(fields)

	if fields["world_delete_confirm"] then
		if menu.world_to_del > 0 and
			menu.world_to_del <= #filterlist.get_raw_list(worldlist) then
			core.delete_world(menu.world_to_del)
			menu.world_to_del = 0
			filterlist.refresh(worldlist)
		end
	end

	tabbuilder.is_dialog = false
	tabbuilder.show_buttons = true
	tabbuilder.current_tab = core.setting_get("main_menu_tab")
end


--------------------------------------------------------------------------------
-- Singleplayer tab
--------------------------------------------------------------------------------
function tabbuilder.tab_singleplayer()
	local gameidx = ''
	local formspec = ''
	local index = filterlist.get_current_index(worldlist,
				tonumber(core.setting_get("mainmenu_last_selected_world"))
				)
	if menu.lastgame() then
		gameidx = menu.lastgame().id
	end
	formspec = "label[0,2;Game: " .. core.formspec_escape(gameidx) .. "]" ..
			"button[7.1,8;3,0.5;world_delete;".. fgettext("Delete") .. "]" ..
			"label[7.1,0;".. fgettext("Select World:") .. "]"..
			"checkbox[7.1,7.1;cb_creative_mode;".. fgettext("Creative Mode") .. ";" ..
			dump(core.setting_getbool("creative_mode")) .. "]"..
			"checkbox[10.1,7.1;cb_enable_damage;".. fgettext("Enable Damage") .. ";" ..
			dump(core.setting_getbool("enable_damage")) .. "]"..
			"textlist[7.1,0.5;8.8,6.5;sp_worlds;" ..
			menu.render_world_list() ..
			";" .. index .. "]" ..
			menubar.formspec
	if #gamemgr.games > 0 then
		formspec = formspec .. "button[12.85,8.95;3.25,0.5;play;".. fgettext("Play") .. "]" ..
			"button[10.1,8;3,0.5;world_create;".. fgettext("New") .. "]" ..
			"button[13.1,8;3,0.5;world_configure;".. fgettext("Configure") .. "]"
	end
	return formspec
end

--------------------------------------------------------------------------------
function tabbuilder.handle_singleplayer_buttons(fields)

	local world_doubleclick = false

	if fields["sp_worlds"] ~= nil then
		local event = core.explode_textlist_event(fields["sp_worlds"])

		if event.type == "DCL" then
			world_doubleclick = true
		end

		if event.type == "CHG" then
			core.setting_set("mainmenu_last_selected_world",
				filterlist.get_raw_index(worldlist,core.get_textlist_index("sp_worlds")))
		end
	end

	menu.handle_key_up_down(fields,"sp_worlds","mainmenu_last_selected_world")

	if fields["cb_creative_mode"] then
		core.setting_set("creative_mode", fields["cb_creative_mode"])
	end

	if fields["cb_enable_damage"] then
		core.setting_set("enable_damage", fields["cb_enable_damage"])
	end

	if fields["play"] ~= nil or
		world_doubleclick or
		fields["key_enter"] then
		local selected = core.get_textlist_index("sp_worlds")
		if selected ~= nil then
			gamedata.selected_world	= filterlist.get_raw_index(worldlist,selected)
			gamedata.singleplayer	= true

			menu.update_last_game(gamedata.selected_world)

			core.start()
		end
	end

	-- is there any more hacky way to do that?
	local world_created = fields["world_create_confirm"] ~= nil or fields["world_create_cancel"] ~= nil or fields["key_enter"] ~= nil

	if (fields["world_create"] ~= nil or fields["dd_mapgen"] ~= nil) and not world_created then
		tabbuilder.current_tab = "dialog_create_world"
		tabbuilder.is_dialog = true
		tabbuilder.show_buttons = true
	end

	if fields["world_delete"] ~= nil then
		local selected = core.get_textlist_index("sp_worlds")
		if selected ~= nil and
			selected <= filterlist.size(worldlist) then
			local world = filterlist.get_list(worldlist)[selected]
			if world ~= nil and
				world.name ~= nil and
				world.name ~= "" then
				menu.world_to_del = filterlist.get_raw_index(worldlist,selected)
				tabbuilder.current_tab = "dialog_delete_world"
				tabbuilder.is_dialog = true
				tabbuilder.show_buttons = false
			else
				menu.world_to_del = 0
			end
		end
	end

	if fields["world_configure"] ~= nil then
		selected = core.get_textlist_index("sp_worlds")
		if selected ~= nil then
			modmgr.world_config_selected_world = filterlist.get_raw_index(worldlist,selected)
			if modmgr.init_worldconfig() then
				tabbuilder.current_tab = "dialog_configure_world"
				tabbuilder.is_dialog = true
				tabbuilder.show_buttons = false
			end
		end
	end
end



--------------------------------------------------------------------------------
-- Multiplayer tab
--------------------------------------------------------------------------------
local selected_server = {}

function tabbuilder.tab_multiplayer()
	local e = core.formspec_escape
	local retval =
		"field[7.35,9;6.75,0.5;te_address;" .. fgettext("Address") .. ";" ..core.setting_get("address") .."]" ..
		"field[14.05,9;2.3,0.5;te_port;" .. fgettext("Port") .. ";" ..core.setting_get("remote_port") .."]" ..
		"checkbox[7.1,-0.43;cb_public_serverlist;".. fgettext("Public Serverlist") .. ";" ..
		dump(core.setting_getbool("public_serverlist")) .. "]"

	if not core.setting_getbool("public_serverlist") then
		retval = retval ..
			"button[13.85,3.95;2.25,0.5;btn_delete_favorite;".. fgettext("Delete") .. "]"
	end

	retval = retval ..
		"button[13.35,11;2.75,0.5;btn_mp_connect;".. fgettext("Connect") .. "]" ..
		"field[7.35,10.4;5.5,0.5;te_name;" .. fgettext("Name") .. ";" ..(selected_server.playername or  core.setting_get("name")) .."]" ..
		"pwdfield[12.9,10.4;3.45,0.5;te_pwd;" .. fgettext("Password") ..(selected_server.playerpassword and (";"..selected_server.playerpassword) or "").. "]" ..
		"textarea[7.35,6.5;8.8,2.5;;"
	if menu.fav_selected ~= nil and
		menu.favorites[menu.fav_selected] ~= nil and
		menu.favorites[menu.fav_selected].description ~= nil then
		retval = retval ..
			core.formspec_escape(menu.favorites[menu.fav_selected].description,true)
	end

	retval = retval ..
		";]"
	retval = retval .. "tablecolumns[" ..
		"text,tooltip=Online,align=center;" ..
		"text,align=center;" ..
		"text,tooltip=Slots,align=center;" ..
		"image,tooltip=Requires non-empty password,1=" .. e(menu.defaulttexturedir .. "server_flags_password.png") .. ";" ..
		"image,tooltip=Creative,1=" .. e(menu.defaulttexturedir .. "server_flags_creative.png") .. ";" ..
		"image,tooltip=Damage enabled,1=" .. e(menu.defaulttexturedir .. "server_flags_damage.png") .. ";" ..
		"image,tooltip=PvP enabled,1=" .. e(menu.defaulttexturedir .. "server_flags_pvp.png") .. ";" ..
		"text" ..
		"]"
	retval = retval .. "table[" ..
		"7.1,0.35;8.8,6;favourites;"

	local render_details = 1 -- core.setting_getbool("public_serverlist")

	if menu.favorites and #menu.favorites > 0 then
		retval = retval .. menu.render_favorite(menu.favorites[1],render_details)

		for i=2,#menu.favorites,1 do
			retval = retval .. "," .. menu.render_favorite(menu.favorites[i],render_details)
		end
	end

	if menu.fav_selected ~= nil then
		retval = retval .. ";" .. menu.fav_selected .. "]"
	else
		retval = retval .. ";0]"
	end

	return retval
end

--------------------------------------------------------------------------------
function tabbuilder.handle_multiplayer_buttons(fields)

	if fields["te_name"] ~= nil then
		gamedata.playername = fields["te_name"]
		core.setting_set("name", fields["te_name"])
	end

	if fields["favourites"] ~= nil then
		local event = core.explode_table_event(fields["favourites"])
		event.index = event.row
		if event.type == "DCL" then
			if event.index <= #menu.favorites then
				selected_server = menu.favorites[event.index]
				gamedata.address = menu.favorites[event.index].address
				gamedata.port = menu.favorites[event.index].port
				gamedata.playername		= fields["te_name"]
				if fields["te_pwd"] ~= nil then
					gamedata.password		= fields["te_pwd"]
				end

				gamedata.selected_world = 0

				if menu.favorites ~= nil then
					gamedata.servername = menu.favorites[event.index].name
					gamedata.serverdescription = menu.favorites[event.index].description
				end

				if gamedata.address ~= nil and
					gamedata.port ~= nil then
					core.setting_set("address",gamedata.address)
					core.setting_set("remote_port",gamedata.port)
					core.start()
				end
			end
		end

		if event.type == "CHG" then
			if event.index <= #menu.favorites then
				selected_server = menu.favorites[event.index]
				local address = menu.favorites[event.index].address
				local port = menu.favorites[event.index].port

				if address ~= nil and
					port ~= nil then
					core.setting_set("address",address)
					core.setting_set("remote_port",port)
				end

				menu.fav_selected = event.index
			end
		end
		return
	end

	if fields["key_up"] ~= nil or
		fields["key_down"] ~= nil then

		local fav_idx = core.get_textlist_index("favourites")

		if fav_idx ~= nil then
			if fields["key_up"] ~= nil and fav_idx > 1 then
				fav_idx = fav_idx -1
			elseif fields["key_down"] and fav_idx < #menu.favorites then
				fav_idx = fav_idx +1
			end

			local address = menu.favorites[fav_idx].address
			local port = menu.favorites[fav_idx].port

			if address ~= nil and
				port ~= nil then
				core.setting_set("address",address)
				core.setting_set("remote_port",port)
			end

		end

		menu.fav_selected = fav_idx
		return
	end

	if fields["cb_public_serverlist"] ~= nil then
		core.setting_set("public_serverlist", fields["cb_public_serverlist"])

		if core.setting_getbool("public_serverlist") then
			asyncOnlineFavourites()
		else
			menu.favorites = core.get_favorites("local")
		end
		menu.fav_selected = nil
		return
	end

	if fields["btn_delete_favorite"] ~= nil then
		local current_favourite = core.get_textlist_index("favourites")
		if current_favourite == nil then return end
		core.delete_favorite(current_favourite)
		menu.favorites = core.get_favorites()
		menu.fav_selected = nil

		core.setting_set("address","")
		core.setting_set("remote_port","30000")

		return
	end

	if fields["btn_mp_connect"] ~= nil or
		fields["key_enter"] ~= nil then

		gamedata.playername		= fields["te_name"]
		gamedata.password		= fields["te_pwd"]
		gamedata.address		= fields["te_address"]
		gamedata.port			= fields["te_port"]

		if selected_server.playerpassword and gamedata.password == "" then gamedata.password = selected_server.playerpassword end

		local fav_idx = core.get_textlist_index("favourites")

		if fav_idx ~= nil and fav_idx <= #menu.favorites and
			menu.favorites[fav_idx].address == fields["te_address"] and
			menu.favorites[fav_idx].port == fields["te_port"] then

			gamedata.servername			= menu.favorites[fav_idx].name
			gamedata.serverdescription	= menu.favorites[fav_idx].description
		else
			gamedata.servername = ""
			gamedata.serverdescription = ""
		end

		gamedata.selected_world = 0

		if fields["te_address"] and fields["te_port"] then
			core.setting_set("address", fields["te_address"])
			core.setting_set("remote_port", fields["te_port"])
		end

		core.start()
		return
	end
end

--------------------------------------------------------------------------------
-- Server tab
--------------------------------------------------------------------------------
function tabbuilder.tab_server()

	local index = filterlist.get_current_index(worldlist,
				tonumber(core.setting_get("mainmenu_last_selected_world"))
				)

	local retval =
		"button[7.1,7.15;2.6,0.5;world_delete;".. fgettext("Delete") .. "]" ..
		"button[9.6,7.15;2.8,0.5;world_create;".. fgettext("New") .. "]" ..
		"button[12.3,7.15;2.55,0.5;world_configure;".. fgettext("Configure") .. "]" ..
		"button[11,11;3.25,0.5;start_server;".. fgettext("Start Game") .. "]" ..
		"label[7.1,-0.25;".. fgettext("Select World:") .. "]"..
		"checkbox[7.1,7.5;cb_creative_mode;".. fgettext("Creative Mode") .. ";" ..
		dump(core.setting_getbool("creative_mode")) .. "]"..
		"checkbox[9.6,7.5;cb_enable_damage;".. fgettext("Enable Damage") .. ";" ..
		dump(core.setting_getbool("enable_damage")) .. "]"..
		"checkbox[12.3,7.5;cb_server_announce;".. fgettext("Public") .. ";" ..
		dump(core.setting_getbool("server_announce")) .. "]"..
		"field[7.3,9;4.5,0.5;te_playername;".. fgettext("Name") .. ";" ..
		core.setting_get("name") .. "]" ..
		"pwdfield[11.8,9;3.3,0.5;te_passwd;".. fgettext("Password") .. "]"
		
-- TODO !!!!
	local bind_addr = core.setting_get("bind_address")
	if bind_addr ~= nil and bind_addr ~= "" then
		retval = retval ..
			"field[7.3,10.3;2.25,0.5;te_serveraddr;".. fgettext("Bind Address") .. ";" ..
			core.setting_get("bind_address") .."]" ..
			"field[11.8,10.3;1.25,0.5;te_serverport;".. fgettext("Port") .. ";" ..
			core.setting_get("port") .."]"
	else
		retval = retval ..
			"field[7.3,10.3;3,0.5;te_serverport;".. fgettext("Server Port") .. ";" ..
			core.setting_get("port") .."]"
	end
	
	retval = retval ..
		"textlist[7.1,0.25;7.5,6.7;srv_worlds;" ..
		menu.render_world_list() ..
		";" .. index .. "]"

	if not core.setting_get("menu_last_game") then
		local default_game = core.setting_get("default_game") or "default"
		core.setting_set("menu_last_game", default_game )
	end
	return retval
end

--------------------------------------------------------------------------------
function tabbuilder.handle_server_buttons(fields)

	local world_doubleclick = false

	if fields["srv_worlds"] ~= nil then
		local event = core.explode_textlist_event(fields["srv_worlds"])

		if event.type == "DCL" then
			world_doubleclick = true
		end
		if event.type == "CHG" then
			core.setting_set("mainmenu_last_selected_world",
				filterlist.get_raw_index(worldlist,core.get_textlist_index("srv_worlds")))
		end
	end

	menu.handle_key_up_down(fields,"srv_worlds","mainmenu_last_selected_world")

	if fields["cb_creative_mode"] then
		core.setting_set("creative_mode", fields["cb_creative_mode"])
	end

	if fields["cb_enable_damage"] then
		core.setting_set("enable_damage", fields["cb_enable_damage"])
	end

	if fields["cb_server_announce"] then
		core.setting_set("server_announce", fields["cb_server_announce"])
	end

	if fields["start_server"] ~= nil or
		world_doubleclick or
		fields["key_enter"] then
		local selected = core.get_textlist_index("srv_worlds")
		if selected ~= nil then
			gamedata.playername		= fields["te_playername"]
			gamedata.password		= fields["te_passwd"]
			gamedata.port			= fields["te_serverport"]
			gamedata.address		= ""
			gamedata.selected_world	= filterlist.get_raw_index(worldlist,selected)

			core.setting_set("port",gamedata.port)
			if fields["te_serveraddr"] ~= nil then
				core.setting_set("bind_address",fields["te_serveraddr"])
			end

			menu.update_last_game(gamedata.selected_world)
			core.start()
		end
	end

	-- is there any more hacky way to do that?
	local world_created = fields["world_create_confirm"] ~= nil or fields["world_create_cancel"] ~= nil or fields["key_enter"] ~= nil
	if (fields["world_create"] ~= nil or fields["dd_mapgen"] ~= nil) and not world_created then
		tabbuilder.current_tab = "dialog_create_world"
		tabbuilder.is_dialog = true
		tabbuilder.show_buttons = true
	end

	if fields["world_delete"] ~= nil then
		local selected = core.get_textlist_index("srv_worlds")
		if selected ~= nil and
			selected <= filterlist.size(worldlist) then
			local world = filterlist.get_list(worldlist)[selected]
			if world ~= nil and
				world.name ~= nil and
				world.name ~= "" then
				menu.world_to_del = filterlist.get_raw_index(worldlist,selected)
				tabbuilder.current_tab = "dialog_delete_world"
				tabbuilder.is_dialog = true
				tabbuilder.show_buttons = false
			else
				menu.world_to_del = 0
			end
		end
	end

	if fields["world_configure"] ~= nil then
		selected = core.get_textlist_index("srv_worlds")
		if selected ~= nil then
			modmgr.world_config_selected_world = filterlist.get_raw_index(worldlist,selected)
			if modmgr.init_worldconfig() then
				tabbuilder.current_tab = "dialog_configure_world"
				tabbuilder.is_dialog = true
				tabbuilder.show_buttons = false
			end
		end
	end
end

--------------------------------------------------------------------------------
-- Texture packs tab
--------------------------------------------------------------------------------
function tabbuilder.tab_texture_packs()
	local retval = "label[7.1,-0.25;".. fgettext("Select texture pack:") .. "]"..
			"textlist[7.1,0.25;7.5,5.0;TPs;"

	local current_texture_path = core.setting_get("texture_path")
	local list = filter_texture_pack_list(core.get_dir_list(core.get_texturepath(), true))
	local index = tonumber(core.setting_get("mainmenu_last_selected_TP"))

	if index == nil then index = 1 end

	if current_texture_path == "" then
		retval = retval ..
			menu.render_texture_pack_list(list) ..
			";" .. index .. "]"
		return retval
	end

	local infofile = current_texture_path ..DIR_DELIM.."info.txt"
	local infotext = ""
	local f = io.open(infofile, "r")
	if f==nil then
		infotext = fgettext("No information available")
	else
		infotext = f:read("*all")
		f:close()
	end

	local screenfile = current_texture_path..DIR_DELIM.."screenshot.png"
	local no_screenshot = nil
	if not file_exists(screenfile) then
		screenfile = nil
		no_screenshot = menu.defaulttexturedir .. "no_screenshot.png"
	end

	return	retval ..
			menu.render_texture_pack_list(list) ..
			";" .. index .. "]" ..
			"image[7.1,4.5;4.0,3.7;"..core.formspec_escape(screenfile or no_screenshot).."]"..
			"textarea[7.35,7.5;7.5,5;;"..core.formspec_escape(infotext or "")..";]"
end

--------------------------------------------------------------------------------
function tabbuilder.handle_texture_pack_buttons(fields)
	if fields["TPs"] ~= nil then
		local event = core.explode_textlist_event(fields["TPs"])
		if event.type == "CHG" or event.type == "DCL" then
			local index = core.get_textlist_index("TPs")
			core.setting_set("mainmenu_last_selected_TP",
				index)
			local list = filter_texture_pack_list(core.get_dir_list(core.get_texturepath(), true))
			local current_index = core.get_textlist_index("TPs")
			if current_index ~= nil and #list >= current_index then
				local new_path = core.get_texturepath()..DIR_DELIM..list[current_index]
				if list[current_index] == "None" then new_path = "" end

				core.setting_set("texture_path", new_path)
			end
		end
	end
end

--------------------------------------------------------------------------------
-- Settings tab
--------------------------------------------------------------------------------
function tabbuilder.tab_settings()

	local tab_string = ""
	local pos_x_offset, pos_y_offset = 7.1, -0.43
	local pos_x, pos_y = pos_x_offset, pos_y_offset

	local calc_next_pos = function()
		pos_y = pos_y + 0.5
		if(pos_y >= 11) then
			pos_y = pos_y_offset
			pos_x = pos_x + 5
		end
	end

	local add_checkbox = function(name, config, text, disabled)
		if(disabled == nil or disabled == false) then
		    tab_string = tab_string ..
			    "checkbox[" .. pos_x .. "," .. pos_y ..  ";" .. name .. ";".. fgettext(text) .. ";"
					    .. dump(core.setting_getbool(config)) .. "]"
		else
		    tab_string = tab_string ..
			    "textlist[" .. pos_x+0.33 .. "," .. pos_y+0.2 .. ";4,1;;#888888" .. fgettext(text) .. ";0;true]"
		end
		calc_next_pos()
	end

	local add_title = function(text)
		-- add free space before title
		if(pos_y > pos_y_offset) then
			calc_next_pos()
		end
		tab_string = tab_string ..
			"textlist[" .. pos_x-0.1 .. "," .. pos_y+0.2 .. ";4,1;;#ffffff" .. fgettext(text) .. ";0;true]"
		calc_next_pos()
	end

	-- UI settings
	add_title("UI settings")
	add_checkbox( "cb_enable_node_highlighting",  "enable_node_highlighting",   "Node Highlighting"     )
	add_checkbox( "cb_hotbar_cycling",            "hotbar_cycling",             "Hotbar Cycling"        )
	add_checkbox( "cb_enable_minimap",            "enable_minimap",             "Show minimap"          )

	local disable_minimap_group = not core.setting_getbool("enable_minimap");
	add_checkbox( "cb_minimap_shape_round",       "minimap_shape_round",        "Minimap shape round",  disable_minimap_group )

	-- Graphics settings
	add_title("Graphics settings")
	add_checkbox( "cb_mipmapping",                "mip_map",                    "Mip-Mapping"           )
	add_checkbox( "cb_anisotrophic",              "anisotropic_filter",         "Anisotropic Filtering" )
	add_checkbox( "cb_bilinear",                  "bilinear_filter",            "Bi-Linear Filtering"   )
	add_checkbox( "cb_trilinear",                 "trilinear_filter",           "Tri-Linear Filtering"  )

	add_checkbox( "cb_smooth_lighting",           "smooth_lighting",            "Smooth Lighting"       )
	add_checkbox( "cb_fancy_trees",               "new_style_leaves",           "Fancy trees"           )
	add_checkbox( "cb_opaque_water",              "opaque_water",               "Opaque Water"          )
	add_checkbox( "cb_connected_glass",           "connected_glass",            "Connected glass"       )
	add_checkbox( "cb_3d_clouds",                 "enable_3d_clouds",           "3D Clouds"             )
	add_checkbox( "cb_farmesh",                   "farmesh",                    "Farmesh (dev)"         )

	-- Effects settings
	add_title("Effects settings")
	add_checkbox( "cb_particles",                 "enable_particles",           "Enable Particles"      )
	add_checkbox( "cb_shaders",                   "enable_shaders",             "Shaders"               )

	-- Enviroment settings
	add_title("Enviroment settings")
	add_checkbox( "cb_liquid_real",               "liquid_real",                "Real Liquid"           )
	add_checkbox( "cb_weather",                   "weather",                    "Weather"               )

	local disable_shaders_group = not core.setting_getbool("enable_shaders");

	add_checkbox( "cb_bumpmapping",               "enable_bumpmapping",         "Bumpmapping",          disable_shaders_group )
	add_checkbox( "cb_parallax",                  "enable_parallax_occlusion",  "Parallax Occlusion",   disable_shaders_group )
	add_checkbox( "cb_generate_normalmaps",       "generate_normalmaps",        "Generate Normalmaps",  disable_shaders_group )
	add_checkbox( "cb_waving_water",              "enable_waving_water",        "Waving Water",         disable_shaders_group )
	add_checkbox( "cb_waving_leaves",             "enable_waving_leaves",       "Waving Leaves",        disable_shaders_group )
	add_checkbox( "cb_waving_plants",             "enable_waving_plants",       "Waving Plants",        disable_shaders_group )

	-- Input setup
	tab_string = tab_string ..
		"button[" .. pos_x_offset .. ",11.5;3,0.5;btn_change_keys;".. fgettext("Change keys") .. "]"

	return tab_string
end

--------------------------------------------------------------------------------
function tabbuilder.handle_settings_buttons(fields)

	-- TODO: refactor this

	if fields["cb_fancy_trees"] then
		core.setting_set("new_style_leaves", fields["cb_fancy_trees"])
	end
	if fields["cb_smooth_lighting"] then
		core.setting_set("smooth_lighting", fields["cb_smooth_lighting"])
	end
	if fields["cb_enable_node_highlighting"] then
		core.setting_set("enable_node_highlighting", fields["cb_enable_node_highlighting"])
	end
	if fields["cb_3d_clouds"] then
		core.setting_set("enable_3d_clouds", fields["cb_3d_clouds"])
	end
	if fields["cb_opaque_water"] then
		core.setting_set("opaque_water", fields["cb_opaque_water"])
	end
	if fields["cb_connected_glass"] then
		core.setting_set("connected_glass", fields["cb_connected_glass"])
	end
	if fields["cb_farmesh"] then
		if fields["cb_farmesh"] == "true" then
			core.setting_set("farmesh", 3)
		else
			core.setting_set("farmesh", 0)
		end
	end

	if fields["cb_mipmapping"] then
		core.setting_set("mip_map", fields["cb_mipmapping"])
	end
	if fields["cb_anisotrophic"] then
		core.setting_set("anisotropic_filter", fields["cb_anisotrophic"])
	end
	if fields["cb_bilinear"] then
		core.setting_set("bilinear_filter", fields["cb_bilinear"])
	end
	if fields["cb_trilinear"] then
		core.setting_set("trilinear_filter", fields["cb_trilinear"])
	end

	if fields["cb_shaders"] then
		if (core.setting_get("video_driver") == "direct3d8" or core.setting_get("video_driver") == "direct3d9") then
			core.setting_set("enable_shaders", "false")
			gamedata.errormessage = fgettext("To enable shaders the OpenGL driver needs to be used.")
		else
			core.setting_set("enable_shaders", fields["cb_shaders"])
		end
	end
	if fields["cb_particles"] then
		core.setting_set("enable_particles", fields["cb_particles"])
	end
	if fields["cb_liquid_real"] then
		core.setting_set("liquid_real", fields["cb_liquid_real"])
	end
	if fields["cb_weather"] then
		core.setting_set("weather", fields["cb_weather"])
	end
	if fields["cb_hotbar_cycling"] then
		core.setting_set("hotbar_cycling", fields["cb_hotbar_cycling"])
	end
	if fields["cb_enable_minimap"] then
		core.setting_set("enable_minimap", fields["cb_enable_minimap"])
	end
	if fields["cb_minimap_shape_round"] then
		core.setting_set("minimap_shape_round", fields["cb_minimap_shape_round"])
	end
	if fields["cb_bumpmapping"] then
		core.setting_set("enable_bumpmapping", fields["cb_bumpmapping"])
	end
	if fields["cb_parallax"] then
		core.setting_set("enable_parallax_occlusion", fields["cb_parallax"])
	end
	if fields["cb_generate_normalmaps"] then
		core.setting_set("generate_normalmaps", fields["cb_generate_normalmaps"])
	end
	if fields["cb_waving_water"] then
		core.setting_set("enable_waving_water", fields["cb_waving_water"])
	end
	if fields["cb_waving_leaves"] then
		core.setting_set("enable_waving_leaves", fields["cb_waving_leaves"])
	end
	if fields["cb_waving_plants"] then
		core.setting_set("enable_waving_plants", fields["cb_waving_plants"])
	end
	if fields["btn_change_keys"] ~= nil then
		core.show_keys_menu()
	end
end

--------------------------------------------------------------------------------
-- Credits tab
--------------------------------------------------------------------------------
function tabbuilder.tab_credits()
	return	"label[7.1,0;Freeminer " .. core.get_version() .. "]" ..
			"label[7.1,0.3;http://freeminer.org]" ..
			"label[7.1,1.3;Contributors:]" ..
			"label[7.1,1.7;https://github.com/freeminer/freeminer/graphs/contributors]"
end




--------------------------------------------------------------------------------
-- Update functions
--------------------------------------------------------------------------------
function menu.update()
	local formspec

	-- Handle errors
	if gamedata.errormessage ~= nil then
		formspec = "size[12,5.2,true]" ..
			"textarea[1,2;10,2;;ERROR: " ..
			core.formspec_escape(gamedata.errormessage) ..
			";]"..
			"button[4.5,4.2;3,0.5;btn_error_confirm;" .. fgettext("Ok") .. "]"
	else
		-- General size and information about what we want the formspec to be like on the menu
		formspec = "size[15.5,11.625,true]"
		-- Retrieve menu image from base/pack
		if tabbuilder.show_buttons then
			formspec = formspec .. "image[-0.35,-0.675;" .. core.formspec_escape(menu.defaulttexturedir .. "menu.png") .. "]"
		end
		-- Load background from base/pack
		-- TODO: Allow games to set backgrounds again in menu/background.png
		formspec = formspec .. "background[-50,-50;100,100;" .. core.formspec_escape(menu.defaulttexturedir .. "background.png") .. "]"
		formspec = formspec .. tabbuilder.gettab()
		-- Set clouds to true to avoid brown background on loading page if menu_clouds = false
		-- TODO: Remove menu_clouds, use the base/pack background on loading screen?
		core.set_clouds(true)
	end

	core.update_formspec(formspec)
end

--------------------------------------------------------------------------------
function menu.update_gametype(reset)
	local game = menu.lastgame()

	if reset or game == nil then
		--core.set_topleft_text("")
		filterlist.set_filtercriteria(worldlist,nil)
	else
		--core.set_topleft_text(game.name)
		filterlist.set_filtercriteria(worldlist,game.id)
	end
end

--------------------------------------------------------------------------------
function menu.update_last_game()

	local current_world = filterlist.get_raw_element(worldlist,
							core.setting_get("mainmenu_last_selected_world")
							)

	if current_world == nil then
		return
	end

	local gamespec, i = gamemgr.find_by_gameid(current_world.gameid)
	if i ~= nil then
		menu.last_game = i
		core.setting_set("main_menu_last_game_idx",menu.last_game)
	end
end

--------------------------------------------------------------------------------
-- Initial Load
--------------------------------------------------------------------------------
function menu.init()
	--init gamedata
	gamedata.worldindex = 0

	worldlist = filterlist.create(
					core.get_worlds,
					compare_worlds,
					function(element,uid)
						if element.name == uid then
							return true
						end
						return false
					end, --unique id compare fct
					function(element,gameid)
						if element.gameid == gameid then
							return true
						end
						return false
					end --filter fct
					)

	filterlist.add_sort_mechanism(worldlist,"alphabetic",sort_worlds_alphabetic)
	filterlist.set_sortmode(worldlist,"alphabetic")
	
	
	--init menu data
	gamemgr.update_gamelist()

	menu.last_game	= tonumber(core.setting_get("main_menu_last_game_idx"))

	if type(menu.last_game) ~= "number" then
		menu.last_game = 1
	end

	if core.setting_getbool("public_serverlist") then
		asyncOnlineFavourites()
	else
		menu.favorites = core.get_favorites("local")
	end

	menu.defaulttexturedir = core.get_texturepath_share() .. DIR_DELIM .. "base" ..
					DIR_DELIM .. "pack" .. DIR_DELIM

	updater_init()

end

--------------------------------------------------------------------------------
function tabbuilder.init()
	tabbuilder.tabfuncs = {
		singleplayer  = tabbuilder.tab_singleplayer,
		multiplayer   = tabbuilder.tab_multiplayer,
		server        = tabbuilder.tab_server,
		texture_packs = tabbuilder.tab_texture_packs,
		settings      = tabbuilder.tab_settings,
		credits       = tabbuilder.tab_credits,
		dialog_create_world = tabbuilder.dialog_create_world,
		dialog_delete_world = tabbuilder.dialog_delete_world
	}

	tabbuilder.tabsizes = {
		dialog_create_world = {width=12, height=7},
		dialog_delete_world = {width=12, height=5.2}
	}

	tabbuilder.current_tab = core.setting_get("main_menu_tab")

	if tabbuilder.current_tab == nil or
		tabbuilder.current_tab == "" then
		tabbuilder.current_tab = "singleplayer"
		core.setting_set("main_menu_tab",tabbuilder.current_tab)
	end

	--initialize tab buttons
	tabbuilder.last_tab = nil
	tabbuilder.show_buttons = true

	tabbuilder.current_buttons = {}
	table.insert(tabbuilder.current_buttons,{name="singleplayer", caption=fgettext("Singleplayer")})
	table.insert(tabbuilder.current_buttons,{name="multiplayer", caption=fgettext("Multiplayer")})
	table.insert(tabbuilder.current_buttons,{name="server", caption=fgettext("Create Server")})
	table.insert(tabbuilder.current_buttons,{name="texture_packs", caption=fgettext("Texture Packs")})

	if core.setting_getbool("main_menu_game_mgr") then
		table.insert(tabbuilder.current_buttons,{name="game_mgr", caption=fgettext("Games")})
	end

	if core.setting_getbool("main_menu_mod_mgr") then
		table.insert(tabbuilder.current_buttons,{name="mod_mgr", caption=fgettext("Mods")})
	end
	table.insert(tabbuilder.current_buttons,{name="settings", caption=fgettext("Settings")})
	table.insert(tabbuilder.current_buttons,{name="credits", caption=fgettext("Credits")})


	for i=1,#tabbuilder.current_buttons,1 do
		if tabbuilder.current_buttons[i].name == tabbuilder.current_tab then
			tabbuilder.last_tab_index = i
		end
	end

	if tabbuilder.current_tab ~= "singleplayer" then
		menu.update_gametype(true)
	else
		menu.update_gametype()
	end
end



--------------------------------------------------------------------------------
-- Initialize callbacks
--------------------------------------------------------------------------------
core.button_handler = function(fields)
	if fields["btn_error_confirm"] then
		gamedata.errormessage = nil
	end

	local retval = modmgr.handle_buttons(tabbuilder.current_tab,fields)
	tabbuilder.checkretval(retval)

	retval = gamemgr.handle_buttons(tabbuilder.current_tab,fields)
	tabbuilder.checkretval(retval)

	retval = modstore.handle_buttons(tabbuilder.current_tab,fields)
	tabbuilder.checkretval(retval)

	if tabbuilder.current_tab == "dialog_create_world" then
		tabbuilder.handle_create_world_buttons(fields)
	end

	if tabbuilder.current_tab == "dialog_delete_world" then
		tabbuilder.handle_delete_world_buttons(fields)
	end

	if tabbuilder.current_tab == "singleplayer" then
		tabbuilder.handle_singleplayer_buttons(fields)
	end

	if tabbuilder.current_tab == "texture_packs" then
		tabbuilder.handle_texture_pack_buttons(fields)
	end

	if tabbuilder.current_tab == "multiplayer" then
		tabbuilder.handle_multiplayer_buttons(fields)
	end

	if tabbuilder.current_tab == "settings" then
		tabbuilder.handle_settings_buttons(fields)
	end

	if tabbuilder.current_tab == "server" then
		tabbuilder.handle_server_buttons(fields)
	end

	--tab buttons
	tabbuilder.handle_tab_buttons(fields)

	--menubar buttons
	menubar.handle_buttons(fields)

	if not tabbuilder.skipformupdate then
		--update menu
		menu.update()
	else
		tabbuilder.skipformupdate = false
	end
end

--------------------------------------------------------------------------------
function tabbuilder.checkretval(retval)

	if retval ~= nil then
		if retval.current_tab ~= nil then
			tabbuilder.current_tab = retval.current_tab
		end

		if retval.is_dialog ~= nil then
			tabbuilder.is_dialog = retval.is_dialog
		end

		if retval.show_buttons ~= nil then
			tabbuilder.show_buttons = retval.show_buttons
		end

		if retval.skipformupdate ~= nil then
			tabbuilder.skipformupdate = retval.skipformupdate
		end

		if retval.ignore_menu_quit == true then
			tabbuilder.ignore_menu_quit = true
		else
			tabbuilder.ignore_menu_quit = false
		end
	end
end

--------------------------------------------------------------------------------
core.event_handler = function(event)
	if event == "MenuQuit" then
		if tabbuilder.is_dialog then
			if tabbuilder.ignore_menu_quit then
				return
			end

			tabbuilder.is_dialog = false
			tabbuilder.show_buttons = true
			tabbuilder.current_tab = core.setting_get("main_menu_tab")
			menu.update_gametype()
			menu.update()
		else
			core.close()
		end
	end

	if event == "Refresh" then
		menu.update()
	end
end

--------------------------------------------------------------------------------
-- Menu Startup
--------------------------------------------------------------------------------
menu.init()
tabbuilder.init()
modstore.init()

menubar.refresh()


core.sound_play("main_menu", true)

menu.update()
