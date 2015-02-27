-- TODO: move this function to builtin/common/misc_helpers.lua
--
-- Formats numbers according to SI multiplies and
-- appends a correspending prefix symbol (e.g. 1234000 -> 1.234 M)
function string.number_to_si(value, precision)
	precision = precision or 3
	local symbol = { "Y", "Z", "E", "P", "T", "G", "M", "k" }
	local multiplies = { 10^24, 10^21, 10^18, 10^15, 10^12, 10^9, 10^6, 10^3 }

	local abs_value = math.abs(value)
	for k,v in ipairs(multiplies) do
		if abs_value >= v then
			return string.format("%."..precision.."f "..symbol[k], value / v)
		end
	end
	return math.ceil(value)
end

-- returns formspec with table of stats
function core.stat_formspec(name)
	local stat_table = {
		chat = "Messages",
		craft = "Crafted",
		damage = "Damage",
		die = "Deaths",
		dig = "Digged",
		drop = "Dropped",
		join = "Join",
		move = "Traveled",
		place = "Placed",
		punch = "Punches",
		use = "Uses",
	}

	local formspec
	local y = -.1

	if core.is_singleplayer() then
		-- collumns
		local x = { .25, 1.8 }
		formspec = "size[3.2,4.6]"
			.."label["..x[1]..","..y..";Stat]"
			.."label["..x[2]..","..y..";Value]"
			.."label[-.25,"..(y+0.1)..";"..string.rep("_", 45).."]"
		y = y + 0.2
		for key, eng_name in pairs(stat_table) do
			-- leading
			y = y + 0.4
			formspec = formspec
				.."label["..x[1]..","..y..";"..eng_name.."]"
				.."label["..x[2]..","..y..";"
				..string.number_to_si(core.stat_get("player|"..key.."|"..name)).."]"
		end
	else
		-- collumns
		local x = { .25, 1.8, 3.5 }
		formspec = "size[4.9,4.6]"
			.."label["..x[1]..","..y..";Stat]"
			.."label["..x[2]..","..y..";Player]"
			.."label["..x[3]..","..y..";Total]"
			.."label[-.25,"..(y+0.1)..";"..string.rep("_", 60).."]"
		y = y + 0.2
		for key, eng_name in pairs(stat_table) do
			-- leading
			y = y + 0.4
			formspec = formspec
				.."label["..x[1]..","..y..";"..eng_name.."]"
				.."label["..x[2]..","..y..";"
				..string.number_to_si(core.stat_get("player|"..key.."|"..name)).."]"
				.."label["..x[3]..","..y..";"
				..string.number_to_si(core.stat_get("total|"..key), 4).."]"
		end
	end
	return formspec
end
