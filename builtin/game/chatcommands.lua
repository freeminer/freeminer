-- Minetest: builtin/chatcommands.lua

--
-- Chat command handler
--

core.chatcommands = {}
function core.register_chatcommand(cmd, def)
	def = def or {}
	def.params = def.params or ""
	def.description = def.description or ""
	def.privs = def.privs or {}
	core.chatcommands[cmd] = def
end

core.register_on_chat_message(function(name, message)
	local cmd, param = string.match(message, "^/([^ ]+) *(.*)")
	if not param then
		param = ""
	end
	local cmd_def = core.chatcommands[cmd]
	if cmd_def then
		local has_privs, missing_privs = core.check_player_privs(name, cmd_def.privs)
		if has_privs then
			cmd_def.func(name, param)
		else
			core.chat_send_player(name, "You don't have permission to run this command (missing privileges: "..table.concat(missing_privs, ", ")..")")
		end
		return true -- handled chat message
	end
	return false
end)

--
-- Chat commands
--
core.register_chatcommand("me", {
	params = "<action>",
	description = "chat action (eg. /me orders a pizza)",
	privs = {shout=true},
	func = function(name, param)
		core.chat_send_all("* " .. name .. " " .. param)
	end,
})

core.register_chatcommand("help", {
	privs = {},
	params = "(nothing)/all/privs/<cmd>",
	description = "Get help for commands or list privileges",
	func = function(name, param)
		local format_help_line = function(cmd, def)
			local msg = freeminer.colorize("00ffff", "/"..cmd)
			if def.params and def.params ~= "" then msg = msg .. " " .. freeminer.colorize("eeeeee", def.params) end
			if def.description and def.description ~= "" then msg = msg .. ": " .. def.description end
			return msg
		end
		if param == "" then
			local msg = ""
			cmds = {}
			for cmd, def in pairs(core.chatcommands) do
				if core.check_player_privs(name, def.privs) then
					table.insert(cmds, cmd)
				end
			end
			core.chat_send_player(name, "Available commands: "..table.concat(cmds, " "))
			core.chat_send_player(name, "Use '/help <cmd>' to get more information, or '/help all' to list everything.")
		elseif param == "all" then
			core.chat_send_player(name, "Available commands:")
			for cmd, def in pairs(core.chatcommands) do
				if core.check_player_privs(name, def.privs) then
					core.chat_send_player(name, format_help_line(cmd, def))
				end
			end
		elseif param == "privs" then
			core.chat_send_player(name, "Available privileges:")
			for priv, def in pairs(core.registered_privileges) do
				core.chat_send_player(name, priv..": "..def.description)
			end
		else
			local cmd = param
			def = core.chatcommands[cmd]
			if not def then
				core.chat_send_player(name, "Command not available: "..cmd)
			else
				core.chat_send_player(name, format_help_line(cmd, def))
			end
		end
	end,
})
core.register_chatcommand("privs", {
	params = "<name>",
	description = "print out privileges of player",
	func = function(name, param)
		if param == "" then
			param = name
		else
			--[[if not core.check_player_privs(name, {privs=true}) then
				core.chat_send_player(name, "Privileges of "..param.." are hidden from you.")
				return
			end]]
		end
		core.chat_send_player(name, "Privileges of "..param..": "..core.privs_to_string(core.get_player_privs(param), ' '))
	end,
})
core.register_chatcommand("grant", {
	params = "<name> <privilege>|all",
	description = "Give privilege to player",
	privs = {},
	func = function(name, param)
		if not core.check_player_privs(name, {privs=true}) and
				not core.check_player_privs(name, {basic_privs=true}) then
			core.chat_send_player(name, "Your privileges are insufficient.")
			return
		end
		local grantname, grantprivstr = string.match(param, "([^ ]+) (.+)")
		if not grantname or not grantprivstr then
			core.chat_send_player(name, "Invalid parameters (see /help grant)")
			return
		elseif not core.auth_table[grantname] then
			core.chat_send_player(name, "Player "..grantname.." does not exist.")
			return
		end
		local grantprivs = core.string_to_privs(grantprivstr)
		if grantprivstr == "all" then
			grantprivs = core.registered_privileges
		end
		local privs = core.get_player_privs(grantname)
		local privs_known = true
		for priv, _ in pairs(grantprivs) do
			if priv ~= "interact" and priv ~= "shout" and priv ~= "interact_extra" and not core.check_player_privs(name, {privs=true}) then
				core.chat_send_player(name, "Your privileges are insufficient.")
				return
			end
			if not core.registered_privileges[priv] then
				core.chat_send_player(name, "Unknown privilege: "..priv)
				privs_known = false
			end
			privs[priv] = true
		end
		if not privs_known then
			return
		end
		core.set_player_privs(grantname, privs)
		core.log(name..' granted ('..core.privs_to_string(grantprivs, ', ')..') privileges to '..grantname)
		core.chat_send_player(name, "Privileges of "..grantname..": "..core.privs_to_string(core.get_player_privs(grantname), ' '))
		if grantname ~= name then
			core.chat_send_player(grantname, name.." granted you privileges: "..core.privs_to_string(grantprivs, ' '))
		end
	end,
})
core.register_chatcommand("revoke", {
	params = "<name> <privilege>|all",
	description = "Remove privilege from player",
	privs = {},
	func = function(name, param)
		if not core.check_player_privs(name, {privs=true}) and
				not core.check_player_privs(name, {basic_privs=true}) then
			core.chat_send_player(name, "Your privileges are insufficient.")
			return
		end
		local revokename, revokeprivstr = string.match(param, "([^ ]+) (.+)")
		if not revokename or not revokeprivstr then
			core.chat_send_player(name, "Invalid parameters (see /help revoke)")
			return
		elseif not core.auth_table[revokename] then
			core.chat_send_player(name, "Player "..revokename.." does not exist.")
			return
		end
		local revokeprivs = core.string_to_privs(revokeprivstr)
		local privs = core.get_player_privs(revokename)
		for priv, _ in pairs(revokeprivs) do
			if priv ~= "interact" and priv ~= "shout" and priv ~= "interact_extra" and not core.check_player_privs(name, {privs=true}) then
				core.chat_send_player(name, "Your privileges are insufficient.")
				return
			end
		end
		if revokeprivstr == "all" then
			privs = {}
		else
			for priv, _ in pairs(revokeprivs) do
				privs[priv] = nil
			end
		end
		core.set_player_privs(revokename, privs)
		core.log(name..' revoked ('..core.privs_to_string(revokeprivs, ', ')..') privileges from '..revokename)
		core.chat_send_player(name, "Privileges of "..revokename..": "..core.privs_to_string(core.get_player_privs(revokename), ' '))
		if revokename ~= name then
			core.chat_send_player(revokename, name.." revoked privileges from you: "..core.privs_to_string(revokeprivs, ' '))
		end
	end,
})
core.register_chatcommand("setpassword", {
	params = "<name> <password>",
	description = "set given password",
	privs = {password=true},
	func = function(name, param)
		local toname, raw_password = string.match(param, "^([^ ]+) +(.+)$")
		if not toname then
			toname = string.match(param, "^([^ ]+) *$")
			raw_password = nil
		end
		if not toname then
			core.chat_send_player(name, "Name field required")
			return
		end
		local actstr = "?"
		if not raw_password then
			core.set_player_password(toname, "")
			actstr = "cleared"
		else
			core.set_player_password(toname, core.get_password_hash(toname, raw_password))
			actstr = "set"
		end
		core.chat_send_player(name, "Password of player \""..toname.."\" "..actstr)
		if toname ~= name then
			core.chat_send_player(toname, "Your password was "..actstr.." by "..name)
		end
	end,
})
core.register_chatcommand("clearpassword", {
	params = "<name>",
	description = "set empty password",
	privs = {password=true},
	func = function(name, param)
		toname = param
		if toname == "" then
			core.chat_send_player(name, "Name field required")
			return
		end
		core.set_player_password(toname, '')
		core.chat_send_player(name, "Password of player \""..toname.."\" cleared")
	end,
})

core.register_chatcommand("auth_reload", {
	params = "",
	description = "reload authentication data",
	privs = {server=true},
	func = function(name, param)
		local done = core.auth_reload()
		if done then
			core.chat_send_player(name, "Done.")
		else
			core.chat_send_player(name, "Failed.")
		end
	end,
})

core.register_chatcommand("teleport", {
	params = "<X>,<Y>,<Z> | <to_name> | <name> <X>,<Y>,<Z> | <name> <to_name>",
	description = "teleport to given position",
	privs = {teleport=true},
	func = function(name, param)
		-- Returns (pos, true) if found, otherwise (pos, false)
		local function find_free_position_near(pos)
			local tries = {
				{x=1,y=0,z=0},
				{x=-1,y=0,z=0},
				{x=0,y=0,z=1},
				{x=0,y=0,z=-1},
			}
			for _, d in ipairs(tries) do
				local p = {x = pos.x+d.x, y = pos.y+d.y, z = pos.z+d.z}
				local n = core.get_node_or_nil(p)
				if n and n.name then
					local def = core.registered_nodes[n.name]
					if def and not def.walkable then
						return p, true
					end
				end
			end
			return pos, false
		end

		local teleportee = nil
		local p = {}
		p.x, p.y, p.z = string.match(param, "^([%d.-]+)[, ] *([%d.-]+)[, ] *([%d.-]+)$")
		p.x = tonumber(p.x)
		p.y = tonumber(p.y)
		p.z = tonumber(p.z)
		teleportee = core.get_player_by_name(name)
		if teleportee and p.x and p.y and p.z then
			core.chat_send_player(name, "Teleporting to ("..p.x..", "..p.y..", "..p.z..")")
			teleportee:setpos(p)
			return
		end
		
		local teleportee = nil
		local p = nil
		local target_name = nil
		target_name = string.match(param, "^([^ ]+)$")
		teleportee = core.get_player_by_name(name)
		if target_name then
			local target = core.get_player_by_name(target_name)
			if target then
				p = target:getpos()
			end
		end
		if teleportee and p then
			p = find_free_position_near(p)
			core.chat_send_player(name, "Teleporting to "..target_name.." at ("..p.x..", "..p.y..", "..p.z..")")
			teleportee:setpos(p)
			return
		end
		
		if core.check_player_privs(name, {bring=true}) then
			local teleportee = nil
			local p = {}
			local teleportee_name = nil
			teleportee_name, p.x, p.y, p.z = string.match(param, "^([^ ]+) +([%d.-]+)[, ] *([%d.-]+)[, ] *([%d.-]+)$")
			p.x = tonumber(p.x)
			p.y = tonumber(p.y)
			p.z = tonumber(p.z)
			if teleportee_name then
				teleportee = core.get_player_by_name(teleportee_name)
			end
			if teleportee and p.x and p.y and p.z then
				core.chat_send_player(name, "Teleporting "..teleportee_name.." to ("..p.x..", "..p.y..", "..p.z..")")
				teleportee:setpos(p)
				return
			end
			
			local teleportee = nil
			local p = nil
			local teleportee_name = nil
			local target_name = nil
			teleportee_name, target_name = string.match(param, "^([^ ]+) +([^ ]+)$")
			if teleportee_name then
				teleportee = core.get_player_by_name(teleportee_name)
			end
			if target_name then
				local target = core.get_player_by_name(target_name)
				if target then
					p = target:getpos()
				end
			end
			if teleportee and p then
				p = find_free_position_near(p)
				core.chat_send_player(name, "Teleporting "..teleportee_name.." to "..target_name.." at ("..p.x..", "..p.y..", "..p.z..")")
				teleportee:setpos(p)
				return
			end
		end

		core.chat_send_player(name, "Invalid parameters (\""..param.."\") or player not found (see /help teleport)")
		return
	end,
})

core.register_chatcommand("set", {
	params = "[-n] <name> <value> | <name>",
	description = "set or read server configuration setting",
	privs = {server=true},
	func = function(name, param)
		local arg, setname, setvalue = string.match(param, "(-[n]) ([^ ]+) (.+)")
		if arg and arg == "-n" and setname and setvalue then
			core.setting_set(setname, setvalue)
			core.chat_send_player(name, setname.." = "..setvalue)
			return
		end
		local setname, setvalue = string.match(param, "([^ ]+) (.+)")
		if setname and setvalue then
			if not core.setting_get(setname) then
				core.chat_send_player(name, "Failed. Use '/set -n <name> <value>' to create a new setting.")
				return
			end
			core.setting_set(setname, setvalue)
			core.chat_send_player(name, setname.." = "..setvalue)
			return
		end
		local setname = string.match(param, "([^ ]+)")
		if setname then
			local setvalue = core.setting_get(setname)
			if not setvalue then
				setvalue = "<not set>"
			end
			core.chat_send_player(name, setname.." = "..setvalue)
			return
		end
		core.chat_send_player(name, "Invalid parameters (see /help set)")
	end,
})

core.register_chatcommand("mods", {
	params = "",
	description = "lists mods installed on the server",
	privs = {},
	func = function(name, param)
		local response = ""
		local modnames = core.get_modnames()
		for i, mod in ipairs(modnames) do
			response = response .. mod
			-- Add space if not at the end
			if i ~= #modnames then
				response = response .. " "
			end
		end
		core.chat_send_player(name, response)
	end,
})

local function handle_give_command(cmd, giver, receiver, stackstring)
	core.log("action", giver.." invoked "..cmd..', stackstring="'
			..stackstring..'"')
	core.log(cmd..' invoked, stackstring="'..stackstring..'"')
	local itemstack = ItemStack(stackstring)
	if itemstack:is_empty() then
		core.chat_send_player(giver, 'error: cannot give an empty item')
		return
	elseif not itemstack:is_known() then
		core.chat_send_player(giver, 'error: cannot give an unknown item')
		return
	end
	local receiverref = core.get_player_by_name(receiver)
	if receiverref == nil then
		core.chat_send_player(giver, receiver..' is not a known player')
		return
	end
	local leftover = receiverref:get_inventory():add_item("main", itemstack)
	if leftover:is_empty() then
		partiality = ""
	elseif leftover:get_count() == itemstack:get_count() then
		partiality = "could not be "
	else
		partiality = "partially "
	end
	-- The actual item stack string may be different from what the "giver"
	-- entered (e.g. big numbers are always interpreted as 2^16-1).
	stackstring = itemstack:to_string()
	if giver == receiver then
		core.chat_send_player(giver, '"'..stackstring
			..'" '..partiality..'added to inventory.');
	else
		core.chat_send_player(giver, '"'..stackstring
			..'" '..partiality..'added to '..receiver..'\'s inventory.');
		core.chat_send_player(receiver, '"'..stackstring
			..'" '..partiality..'added to inventory.');
	end
end

core.register_chatcommand("give", {
	params = "<name> <itemstring>",
	description = "give item to player",
	privs = {give=true},
	func = function(name, param)
		local toname, itemstring = string.match(param, "^([^ ]+) +(.+)$")
		if not toname or not itemstring then
			core.chat_send_player(name, "name and itemstring required")
			return
		end
		handle_give_command("/give", name, toname, itemstring)
	end,
})
core.register_chatcommand("giveme", {
	params = "<itemstring>",
	description = "give item to yourself",
	privs = {give=true},
	func = function(name, param)
		local itemstring = string.match(param, "(.+)$")
		if not itemstring then
			core.chat_send_player(name, "itemstring required")
			return
		end
		handle_give_command("/giveme", name, name, itemstring)
	end,
})
core.register_chatcommand("spawnentity", {
	params = "<entityname>",
	description = "spawn entity at your position",
	privs = {give=true, interact=true},
	func = function(name, param)
		local entityname = string.match(param, "(.+)$")
		if not entityname then
			core.chat_send_player(name, "entityname required")
			return
		end
		core.log("action", '/spawnentity invoked, entityname="'..entityname..'"')
		local player = core.get_player_by_name(name)
		if player == nil then
			core.log("error", "Unable to spawn entity, player is nil")
			return true -- Handled chat message
		end
		local p = player:getpos()
		p.y = p.y + 1
		core.add_entity(p, entityname)
		core.chat_send_player(name, '"'..entityname
				..'" spawned.');
	end,
})
core.register_chatcommand("pulverize", {
	params = "",
	description = "delete item in hand",
	privs = {},
	func = function(name, param)
		local player = core.get_player_by_name(name)
		if player == nil then
			core.log("error", "Unable to pulverize, player is nil")
			return true -- Handled chat message
		end
		if player:get_wielded_item():is_empty() then
			core.chat_send_player(name, 'Unable to pulverize, no item in hand.')
		else
			player:set_wielded_item(nil)
			core.chat_send_player(name, 'An item was pulverized.')
		end
	end,
})

-- Key = player name
core.rollback_punch_callbacks = {}

core.register_on_punchnode(function(pos, node, puncher)
	local name = puncher:get_player_name()
	if core.rollback_punch_callbacks[name] then
		core.rollback_punch_callbacks[name](pos, node, puncher)
		core.rollback_punch_callbacks[name] = nil
	end
end)

core.register_chatcommand("rollback_check", {
	params = "[<range>] [<seconds>] [limit]",
	description = "check who has last touched a node or near it, "..
			"max. <seconds> ago (default range=0, seconds=86400=24h, limit=5)",
	privs = {rollback=true},
	func = function(name, param)
		local range, seconds, limit =
			param:match("(%d+) *(%d*) *(%d*)")
		range = tonumber(range) or 0
		seconds = tonumber(seconds) or 86400
		limit = tonumber(limit) or 5
		if limit > 100 then
			core.chat_send_player(name, "That limit is too high!")
			return
		end
		core.chat_send_player(name, "Punch a node (range="..
				range..", seconds="..seconds.."s, limit="..limit..")")

		core.rollback_punch_callbacks[name] = function(pos, node, puncher)
			local name = puncher:get_player_name()
			core.chat_send_player(name, "Checking "..core.pos_to_string(pos).."...")
			local actions = core.rollback_get_node_actions(pos, range, seconds, limit)
			local num_actions = #actions
			if num_actions == 0 then
				core.chat_send_player(name, "Nobody has touched the "..
						"specified location in "..seconds.." seconds")
				return
			end
			local time = os.time()
			for i = num_actions, 1, -1 do
				local action = actions[i]
				core.chat_send_player(name,
					("%s %s %s -> %s %d seconds ago.")
						:format(
							core.pos_to_string(action.pos),
							action.actor,
							action.oldnode.name,
							action.newnode.name,
							time - action.time))
			end
		end
	end,
})

core.register_chatcommand("rollback", {
	params = "<player name> [<seconds>] | :<actor> [<seconds>]",
	description = "revert actions of a player; default for <seconds> is 60",
	privs = {rollback=true},
	func = function(name, param)
		local target_name, seconds = string.match(param, ":([^ ]+) *(%d*)")
		if not target_name then
			local player_name = nil
			player_name, seconds = string.match(param, "([^ ]+) *(%d*)")
			if not player_name then
				core.chat_send_player(name, "Invalid parameters. See /help rollback and /help rollback_check")
				return
			end
			target_name = "player:"..player_name
		end
		seconds = tonumber(seconds) or 60
		core.chat_send_player(name, "Reverting actions of "..
				target_name.." since "..seconds.." seconds.")
		local success, log = core.rollback_revert_actions_by(
				target_name, seconds)
		if #log > 100 then
			core.chat_send_player(name, "(log is too long to show)")
		else
			for _, line in pairs(log) do
				core.chat_send_player(name, line)
			end
		end
		if success then
			core.chat_send_player(name, "Reverting actions succeeded.")
		else
			core.chat_send_player(name, "Reverting actions FAILED.")
		end
	end,
})

core.register_chatcommand("status", {
	params = "",
	description = "print server status line",
	privs = {},
	func = function(name, param)
		core.chat_send_player(name, core.get_server_status())
	end,
})

core.register_chatcommand("time", {
	params = "<0...24000>",
	description = "set time of day",
	privs = {settime=true},
	func = function(name, param)
		if param == "" then
			core.chat_send_player(name, "Missing parameter")
			return
		end
		local newtime = tonumber(param)
		if newtime == nil then
			core.chat_send_player(name, "Invalid time")
		else
			core.set_timeofday((newtime % 24000) / 24000)
			core.chat_send_player(name, "Time of day changed.")
			core.log("action", name .. " sets time " .. newtime)
		end
	end,
})

core.register_chatcommand("shutdown", {
	params = "",
	description = "shutdown server",
	privs = {server=true},
	func = function(name, param)
		core.log("action", name .. " shuts down server")
		core.request_shutdown()
		core.chat_send_all("*** Server shutting down (operator request).")
	end,
})

core.register_chatcommand("ban", {
	params = "<name>",
	description = "ban IP of player",
	privs = {ban=true},
	func = function(name, param)
		if param == "" then
			core.chat_send_player(name, "Ban list: " .. core.get_ban_list())
			return
		end
		if not core.get_player_by_name(param) then
			core.chat_send_player(name, "No such player")
			return
		end
		if not core.ban_player(param) then
			core.chat_send_player(name, "Failed to ban player")
		else
			local desc = core.get_ban_description(param)
			core.chat_send_player(name, "Banned " .. desc .. ".")
			core.log("action", name .. " bans " .. desc .. ".")
		end
	end,
})

core.register_chatcommand("unban", {
	params = "<name/ip>",
	description = "remove IP ban",
	privs = {ban=true},
	func = function(name, param)
		if not core.unban_player_or_ip(param) then
			core.chat_send_player(name, "Failed to unban player/IP")
		else
			core.chat_send_player(name, "Unbanned " .. param)
			core.log("action", name .. " unbans " .. param)
		end
	end,
})

core.register_chatcommand("kick", {
	params = "<name> [reason]",
	description = "kick a player",
	privs = {kick=true},
	func = function(name, param)
		local tokick, reason = string.match(param, "([^ ]+) (.+)")
		if not tokick then
			tokick = param
		end
		if not core.kick_player(tokick, reason) then
			core.chat_send_player(name, "Failed to kick player " .. tokick)
		else
			core.chat_send_player(name, "kicked " .. tokick)
			core.log("action", name .. " kicked " .. tokick)
		end
	end,
})

core.register_chatcommand("clearobjects", {
	params = "",
	description = "clear all objects in world",
	privs = {server=true},
	func = function(name, param)
		core.log("action", name .. " clears all objects")
		core.chat_send_all("Clearing all objects.  This may take long.  You may experience a timeout.  (by " .. name .. ")")
		core.clear_objects()
		core.log("action", "object clearing done")
		core.chat_send_all("*** Cleared all objects.")
	end,
})

core.register_chatcommand("msg", {
	params = "<name> <message>",
	description = "Send a private message",
	privs = {shout=true},
	func = function(name, param)
		local found, _, sendto, message = param:find("^([^%s]+)%s(.+)$")
		if found then
			if core.get_player_by_name(sendto) then
				core.log("action", "PM from "..name.." to "..sendto..": "..message)
				core.chat_send_player(sendto, "PM from "..name..": "..message)
				core.chat_send_player(name, "Message sent")
			else
				core.chat_send_player(name, "The player "..sendto.." is not online")
			end
		else
			core.chat_send_player(name, "Invalid usage, see /help msg")
		end
	end,
})

core.register_chatcommand("die", {
	params = "",
	description = "Kills yourself.",
	func = function(name, param)
		local player = core.get_player_by_name(name)
		if not player then
			return
		end
		player:set_hp(0)
	end,
})
