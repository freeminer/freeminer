-- converts numbers
-- to human readable format
local function human_readable_number(value)
	local symbol = { "P", "T", "G", "M", "k", }
	local multiples = { 10^15, 10^12, 10^9, 10^6, 10^3,  }

	for k,v in ipairs(multiples) do
		if value >= v then
			return string.format("%.3f "..symbol[k], value / v)
		end
	end
	return math.ceil(value)
end

local function show_stat(name, param)
	local pname = name
	if param ~= "" and not core.get_player_by_name(param) then
		return true, "Player not found"
	elseif param ~= "" then
		pname = param
	end

	local keys = {
		"dig",
		"place",
		"craft",
		"chat",
		"move",
		"drop",
		"damage",
		"punch",
		"use",
		"join",
		"die",
	}

	local x = { .25, 1.7, 3 } -- cols
	local y = -.1 -- where rows start
	local formspec = "size[4,5.5]"
		.."label["..x[1]..","..y..";Stat]"
		.."label["..x[2]..","..y..";Player]"
		.."label["..x[3]..","..y..";Total]"
		-- hacky hack xD
		.."label[-.25,"..(y+0.1)..";"..string.rep("_", 55).."]"
	y = y + 0.2
	for _, key in ipairs(keys) do
		y = y + 0.4
		formspec = formspec
			.."label["..x[1]..","..y..";"..key:gsub("^%l", string.upper).."]"
			.."label["..x[2]..","..y..";"
			..human_readable_number(core.stat_get("player|"..key.."|"..pname)).."]"
			.."label["..x[3]..","..y..";"
			..human_readable_number(core.stat_get("total|"..key)).."]"
	end
	formspec = formspec.."button_exit[1,5;2,1;exit;Close"
	core.show_formspec(name, 'stat', formspec)
	return true
end
