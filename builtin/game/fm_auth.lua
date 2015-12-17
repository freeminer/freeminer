--
-- Authentication handler
--

function core.string_to_privs(str, delim)
	if type(str) ~= "string" then return end
	delim = delim or ','
	local privs = {}
	for _, priv in pairs(string.split(str, delim)) do
		privs[priv:trim()] = true
	end
	return privs
end

core.auth_file_path = core.get_worldpath().."/auth.txt"
core.auth_table = {}
core.auth_prefix = "auth_"

local function read_auth(name)
	core.auth_table[name] = core.kv_get(core.auth_prefix .. name, "player_auth")
	return core.auth_table[name]
	--core.notify_authentication_modified(name)
end

local function save_auth(name, data)
	if not data then data = core.auth_table[name] end
	return core.kv_put(core.auth_prefix .. name, data, "player_auth")
end


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

local function auth_convert(force)
	local newtable = {}
	local file, errmsg = io.open(core.auth_file_path, 'rb')
	if not file then
		--core.log("info", core.auth_file_path.." could not be opened for reading ("..errmsg.."); assuming new world")
		return
	end
	core.log("action", "Converting auth to kv " .. core.auth_file_path .. " ")
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
				name = uri_decode(name);
				local old = read_auth(name) --core.kv_get(core.auth_prefix .. name, "player_auth")
				--print("readed " .. name .. " d=" .. core.serialize(old))
				if old and not force then
					print("Player [" ..  name .. "] already converted, skipping")
				else
					--print("Saving player " .. name .. " p="..password)
					local privileges = core.string_to_privs(privilege_string)
					local data = {name=name, password=password, privileges=privileges, last_login=last_login}
					save_auth(name, data)
					--print("save res=  " .. core.serialize(read_auth(name)))
				end
			end
		end
	end
	io.close(file)
	os.rename(core.auth_file_path, core.auth_file_path..'.old')
	core.auth_table = {}
end

local converted = nil

core.builtin_auth_handler = {
	get_auth = function(name)
		if not conveeted then auth_convert(); converted = 1 end -- here because env needed
		assert(type(name) == "string")
		read_auth(name)

		-- Figure out what password to use for a new player (singleplayer
		-- always has an empty password, otherwise use default, which is
		-- usually empty too)
		local new_password_hash = ""
		-- If not in authentication table, return nil
		if not core.auth_table[name] then
			return nil
		end
		-- Figure out what privileges the player should have.
		-- Take a copy of the privilege table
		local privileges = {}
		for priv, _ in pairs(core.auth_table[name].privileges) do
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
		elseif name == core.setting_get("name") then
			for priv, def in pairs(core.registered_privileges) do
				privileges[priv] = true
			end
		end
		-- All done
		return {
			password = core.auth_table[name].password,
			privileges = privileges,
			-- Is set to nil if unknown
			last_login = core.auth_table[name].last_login,
		}
	end,
	create_auth = function(name, password)
		assert(type(name) == "string")
		assert(type(password) == "string")
		core.log('info', "Built-in authentication handler adding player '"..name.."'")
		local privs = core.setting_get("default_privs")
		if core.setting_getbool("creative_mode") and core.setting_get("default_privs_creative") then
			privs = core.setting_get("default_privs_creative")
		end
		core.auth_table[name] = {
			password = password,
			privileges = core.string_to_privs(privs),
			last_login = os.time(),
		}
		save_auth(name)
	end,
	set_password = function(name, password)
		assert(type(name) == "string")
		assert(type(password) == "string")
		read_auth(name)
		if not core.auth_table[name] then
			core.builtin_auth_handler.create_auth(name, password)
		else
			core.log('info', "Built-in authentication handler setting password of player '"..name.."'")
			core.auth_table[name].password = password
			save_auth(name)
		end
		return true
	end,
	set_privileges = function(name, privileges)
		assert(type(name) == "string")
		assert(type(privileges) == "table")
		read_auth(name)
		if not core.auth_table[name] then
			core.builtin_auth_handler.create_auth(name,
				core.get_password_hash(name,
					core.setting_get("default_password")))
		end
		core.auth_table[name].privileges = privileges
		core.notify_authentication_modified(name)
		save_auth(name)
	end,
	reload = function()
		--read_auth_file()
		return true
	end,
	record_login = function(name)
		assert(type(name) == "string")
		read_auth(name)
		assert(core.auth_table[name]).last_login = os.time()
		save_auth(name)
	end,
}

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
core.auth_reload         = auth_pass("reload")


local record_login = auth_pass("record_login")

core.register_on_joinplayer(function(player)
	record_login(player:get_player_name())
end)

