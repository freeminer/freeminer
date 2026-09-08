local S = core.get_translator("__builtin")

local builtin_shared = ...

core.register_chatcommand( "stat", {
	params = "[name]",
	description = "show in-game action statistics",
	func = function(name, param)
		if param == "" then
			param = name
		elseif not core.get_player_by_name(param) then
			return false, "No such player."
		end
		local formspec = core.stat_formspec(param)
		core.show_formspec(name, 'stat', formspec)
	end
})

core.register_chatcommand("emerge_smart", {
    params = "radius [in_flight]",
    description = "Generate mapblock columns outwards in 2D rings and upwards through structures.",
    privs = {
        server = true,
    },
    func = function(name, params)
        return core.emerge_smart(name, params)
    end,
})
