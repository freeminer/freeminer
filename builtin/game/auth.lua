-- Minetest: builtin/auth.lua

--
-- Builtin authentication handler
--

<<<<<<< HEAD
function core.string_to_privs(str, delim)
	if type(str) ~= "string" then return end
	delim = delim or ','
	local privs = {}
	for _, priv in pairs(string.split(str, delim)) do
		privs[priv:trim()] = true
	end
	return privs
end

function core.privs_to_string(privs, delim)
	assert(type(privs) == "table")
	delim = delim or ','
	local list = {}
	for priv, bool in pairs(privs) do
		if bool then
			list[#list + 1] = priv
		end
	end
	return table.concat(list, delim)
end

assert(core.string_to_privs("a,b").b == true)
assert(core.privs_to_string({a=true,b=true}) == "a,b")

core.auth_file_path = core.get_worldpath().."/auth.txt"
core.auth_table = {}

local hex={}
for i=0,255 do
    hex[string.format("%0x",i)]=string.char(i)
    hex[string.format("%0X",i)]=string.char(i)
end

local function uri_decode(str)
	str = string.gsub (str, "+", " ")
	return (str:gsub('%%(%x%x)',hex))
end

function uri_encode (str)
	str = string.gsub (str, "([^0-9a-zA-Z_ -])", function (c) return string.format ("%%%02X", string.byte(c)) end)
	str = string.gsub (str, " ", "+")
	return str
end

local function read_auth_file()
	local newtable = {}
	local file, errmsg = io.open(core.auth_file_path, 'rb')
	if not file then
		core.log("info", core.auth_file_path.." could not be opened for reading ("..errmsg.."); assuming new world")
		return
	end
	local n = 0
	for line in file:lines() do
		n = n + 1
		if line ~= "" then
			local fields = line:split(":", true)
			local name, password, privilege_string, last_login = unpack(fields)
			last_login = tonumber(last_login)
			if not (name and password and privilege_string) then
				print("Invalid line in auth.txt:" .. n .. " " .. dump(line))
			else
			local privileges = core.string_to_privs(privilege_string)
			newtable[uri_decode(name)] = {password=password, privileges=privileges, last_login=last_login}
			end
		end
	end
	io.close(file)
	core.auth_table = newtable
	core.notify_authentication_modified()
end

local function save_auth_file()
	local newtable = {}
	-- Check table for validness before attempting to save
	for name, stuff in pairs(core.auth_table) do
		assert(type(name) == "string")
		assert(name ~= "")
		assert(type(stuff) == "table")
		assert(type(stuff.password) == "string")
		assert(type(stuff.privileges) == "table")
		assert(stuff.last_login == nil or type(stuff.last_login) == "number")
	end
	local file, errmsg = io.open(core.auth_file_path, 'w+b')
	if not file then
		error(core.auth_file_path.." could not be opened for writing: "..errmsg)
	end
	for name, stuff in pairs(core.auth_table) do
		local priv_string = core.privs_to_string(stuff.privileges)
		local parts = {uri_encode(name), stuff.password, priv_string, stuff.last_login or ""}
		file:write(table.concat(parts, ":").."\n")
	end
	io.close(file)
end

read_auth_file()
=======
-- Make the auth object private, deny access to mods
local core_auth = core.auth
core.auth = nil
>>>>>>> 5.5.0

core.builtin_auth_handler = {
	get_auth = function(name)
		assert(type(name) == "string")
		local auth_entry = core_auth.read(name)
		-- If no such auth found, return nil
		if not auth_entry then
			return nil
		end
		-- Figure out what privileges the player should have.
		-- Take a copy of the privilege table
		local privileges = {}
		for priv, _ in pairs(auth_entry.privileges) do
			privileges[priv] = true
		end
		-- If singleplayer, give all privileges except those marked as give_to_singleplayer = false
		if core.is_singleplayer() then
			for priv, def in pairs(core.registered_privileges) do
				if def.give_to_singleplayer then
					privileges[priv] = true
				end
			end
		-- For the admin, give everything
		elseif name == core.settings:get("name") then
			for priv, def in pairs(core.registered_privileges) do
				if def.give_to_admin then
					privileges[priv] = true
				end
			end
		end
		-- All done
		return {
			password = auth_entry.password,
			privileges = privileges,
			last_login = auth_entry.last_login,
		}
	end,
	create_auth = function(name, password)
		assert(type(name) == "string")
		assert(type(password) == "string")
		core.log('info', "Built-in authentication handler adding player '"..name.."'")
<<<<<<< HEAD
		local privs = core.settings:get("default_privs")
		if core.setting_getbool("creative_mode") and core.setting_get("default_privs_creative") then
			privs = core.setting_get("default_privs_creative")
		end
		core.auth_table[name] = {
			password = password,
			privileges = core.string_to_privs(privs),
			last_login = os.time(),
		}
		save_auth_file()
=======
		return core_auth.create({
			name = name,
			password = password,
			privileges = core.string_to_privs(core.settings:get("default_privs")),
			last_login = -1,  -- Defer login time calculation until record_login (called by on_joinplayer)
		})
	end,
	delete_auth = function(name)
		assert(type(name) == "string")
		local auth_entry = core_auth.read(name)
		if not auth_entry then
			return false
		end
		core.log('info', "Built-in authentication handler deleting player '"..name.."'")
		return core_auth.delete(name)
>>>>>>> 5.5.0
	end,
	set_password = function(name, password)
		assert(type(name) == "string")
		assert(type(password) == "string")
		local auth_entry = core_auth.read(name)
		if not auth_entry then
			core.builtin_auth_handler.create_auth(name, password)
		else
			core.log('info', "Built-in authentication handler setting password of player '"..name.."'")
			auth_entry.password = password
			core_auth.save(auth_entry)
		end
		return true
	end,
	set_privileges = function(name, privileges)
		assert(type(name) == "string")
		assert(type(privileges) == "table")
		local auth_entry = core_auth.read(name)
		if not auth_entry then
			auth_entry = core.builtin_auth_handler.create_auth(name,
				core.get_password_hash(name,
					core.settings:get("default_password")))
<<<<<<< HEAD
=======
		end

		auth_entry.privileges = privileges

		core_auth.save(auth_entry)

		-- Run grant callbacks
		for priv, _ in pairs(privileges) do
			if not auth_entry.privileges[priv] then
				core.run_priv_callbacks(name, priv, nil, "grant")
			end
		end

		-- Run revoke callbacks
		for priv, _ in pairs(auth_entry.privileges) do
			if not privileges[priv] then
				core.run_priv_callbacks(name, priv, nil, "revoke")
			end
>>>>>>> 5.5.0
		end
		core.notify_authentication_modified(name)
	end,
	reload = function()
		core_auth.reload()
		return true
	end,
	record_login = function(name)
		assert(type(name) == "string")
		local auth_entry = core_auth.read(name)
		assert(auth_entry)
		auth_entry.last_login = os.time()
		core_auth.save(auth_entry)
	end,
	iterate = function()
		local names = {}
		local nameslist = core_auth.list_names()
		for k,v in pairs(nameslist) do
			names[v] = true
		end
		return pairs(names)
	end,
}

core.register_on_prejoinplayer(function(name, ip)
	if core.registered_auth_handler ~= nil then
		return -- Don't do anything if custom auth handler registered
	end
	local auth_entry = core_auth.read(name)
	if auth_entry ~= nil then
		return
	end

	local name_lower = name:lower()
	for k in core.builtin_auth_handler.iterate() do
		if k:lower() == name_lower then
			return string.format("\nCannot create new player called '%s'. "..
					"Another account called '%s' is already registered. "..
					"Please check the spelling if it's your account "..
					"or use a different nickname.", name, k)
		end
	end
end)

--
-- Authentication API
--

function core.register_authentication_handler(handler)
	if core.registered_auth_handler then
		error("Add-on authentication handler already registered by "..core.registered_auth_handler_modname)
	end
	core.registered_auth_handler = handler
	core.registered_auth_handler_modname = core.get_current_modname()
	handler.mod_origin = core.registered_auth_handler_modname
end

function core.get_auth_handler()
	return core.registered_auth_handler or core.builtin_auth_handler
end

local function auth_pass(name)
	return function(...)
		local auth_handler = core.get_auth_handler()
		if auth_handler[name] then
			return auth_handler[name](...)
		end
		return false
	end
end

core.set_player_password = auth_pass("set_password")
core.set_player_privs    = auth_pass("set_privileges")
core.remove_player_auth  = auth_pass("delete_auth")
core.auth_reload         = auth_pass("reload")

local record_login = auth_pass("record_login")
core.register_on_joinplayer(function(player)
	record_login(player:get_player_name())
end)
