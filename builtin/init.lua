--
-- This file contains built-in stuff in Minetest implemented in Lua.
--
-- It is always loaded and executed after registration of the C API,
-- before loading and running any mods.
--

-- Initialize some very basic things
print = core.debug
math.randomseed(os.time())
os.setlocale("C", "numeric")
minetest = core
freeminer = core

-- Load other files
local scriptdir = core.get_builtin_path()..DIR_DELIM
local gamepath = scriptdir.."game"..DIR_DELIM
local commonpath = scriptdir.."common"..DIR_DELIM
local asyncpath = scriptdir.."async"..DIR_DELIM

--dofile(scriptdir.."profiler.lua") --TODO: repair me

dofile(commonpath.."serialize.lua")
dofile(commonpath.."misc_helpers.lua")

dofile(scriptdir.."key_value_storage.lua")

if INIT == "game" then
	dofile(gamepath.."init.lua")
elseif INIT == "mainmenu" then
	dofile(core.get_mainmenu_path()..DIR_DELIM.."init.lua")
elseif INIT == "async" then
	dofile(asyncpath.."init.lua")
else
	error(("Unrecognized builtin initialization type %s!"):format(tostring(INIT)))
end

