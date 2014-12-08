-- Formats numbers according to SI multiplies and
-- appends a correspending prefix symbol
-- e.g. 1234000 -> 1.234 M
local function human_readable_number(value, p)
	p = p or 3
	local symbol = { "P", "T", "G", "M", "k", }
	local multiplies = { 10^15, 10^12, 10^9, 10^6, 10^3, }

	for k,v in ipairs(multiplies) do
		if value >= v then
			return string.format("%."..p.."f "..symbol[k], value / v)
		end
	end
	return math.ceil(value)
end

function core.show_stat_summary(name, param)
	local pname = name
	if param ~= "" and not core.get_player_by_name(param) then
		return true, "Player not found"
	elseif param ~= "" then
		pname = param
	end

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

	local x = { .25, 1.8, 3.5 } -- cols
	local y = -.1 -- where rows start
	local formspec = "size[4.9,4.6]"
		.."label["..x[1]..","..y..";Stat]"
		.."label["..x[2]..","..y..";Player]"
		.."label["..x[3]..","..y..";Total]"
		-- hacky hack xD
		.."label[-.25,"..(y+0.1)..";"..string.rep("_", 60).."]"
	y = y + 0.2
	for key, eng_name in pairs(stat_table) do
		-- leading
		y = y + 0.4
		formspec = formspec
			.."label["..x[1]..","..y..";"..eng_name.."]"
			.."label["..x[2]..","..y..";"
			..human_readable_number(core.stat_get("player|"..key.."|"..pname)).."]"
			.."label["..x[3]..","..y..";"
			..human_readable_number(core.stat_get("total|"..key), 4).."]"
	end
	core.show_formspec(name, 'stat', formspec)
	return true
end
