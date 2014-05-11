local ffi

-- there's no ffi in Lua, only in LuaJIT
-- when Freeminer is run with Lua the profiler is not available because we can't access precise timer
if not pcall(function() ffi = require("ffi") end) then
    return
end

ffi.cdef[[
unsigned int get_time_us();
]]

local modpath = core.get_builtin_path()..DIR_DELIM

package.path = package.path .. ";" .. modpath .. "/?.lua"
ProFi = require 'ProFi'

local function get_time_precise()
    return ffi.C.get_time_us() / 1000000
end

ProFi:setGetTimeMethod(get_time_precise)

local started = false

local function start_profiler()
    ProFi:start()
    started = true
end

if freeminer.setting_getbool("profiler_autostart") then
    start_profiler()
    freeminer.after(3, function()
        freeminer.chat_send_all("The profiler was started. If you don't want this, set " .. freeminer.colorize("61ad6d", "profiler_autostart")
            .. " to false in " .. freeminer.colorize("61ad6d", "freeminer.conf"))
    end)
end

freeminer.register_chatcommand("profiler_stop", {
    description = "stop the profiler and write report",
    privs = {server=true},
    func = function(name)
        if not started then
            freeminer.chat_send_player(name, "Profiler has not been started. You can start it using " .. freeminer.colorize("00ffff", "/profiler_start"))
            return
        end

        ProFi:stop()
        ProFi:writeReport("profile.txt")

        freeminer.chat_send_player(name, "Profiler is stopped.")
    end,
})

freeminer.register_chatcommand("profiler_start", {
    description = "start the profiler",
    privs = {server=true},
    func = function(name)
        if started then
            freeminer.chat_send_player(name, "Profiler is already running.")
            return
        end
        start_profiler()
        freeminer.chat_send_player(name, "Profiler is started.")
    end
})
