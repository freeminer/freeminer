-- Minetest: builtin/misc_register.lua

--
-- Make raw registration functions inaccessible to anyone except this file
--

local register_item_raw = minetest.register_item_raw
minetest.register_item_raw = nil

local register_alias_raw = minetest.register_alias_raw
minetest.register_item_raw = nil

--
-- Item / entity / ABM registration functions
--

minetest.registered_abms = {}
minetest.registered_entities = {}
minetest.registered_items = {}
minetest.registered_nodes = {}
minetest.registered_craftitems = {}
minetest.registered_tools = {}
minetest.registered_aliases = {}

-- For tables that are indexed by item name:
-- If table[X] does not exist, default to table[minetest.registered_aliases[X]]
local alias_metatable = {
	__index = function(t, name)
		return rawget(t, minetest.registered_aliases[name])
	end
}
setmetatable(minetest.registered_items, alias_metatable)
setmetatable(minetest.registered_nodes, alias_metatable)
setmetatable(minetest.registered_craftitems, alias_metatable)
setmetatable(minetest.registered_tools, alias_metatable)

-- These item names may not be used because they would interfere
-- with legacy itemstrings
local forbidden_item_names = {
	MaterialItem = true,
	MaterialItem2 = true,
	MaterialItem3 = true,
	NodeItem = true,
	node = true,
	CraftItem = true,
	craft = true,
	MBOItem = true,
	ToolItem = true,
	tool = true,
}

local function check_modname_prefix(name)
	if name:sub(1,1) == ":" then
		-- Escape the modname prefix enforcement mechanism
		return name:sub(2)
	else
		-- Modname prefix enforcement
		local expected_prefix = minetest.get_current_modname() .. ":"
		if name:sub(1, #expected_prefix) ~= expected_prefix then
			error("Name " .. name .. " does not follow naming conventions: " ..
				"\"modname:\" or \":\" prefix required")
		end
		local subname = name:sub(#expected_prefix+1)
		if subname:find("[^abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789_]") then
			error("Name " .. name .. " does not follow naming conventions: " ..
				"contains unallowed characters")
		end
		return name
	end
end

function minetest.register_abm(spec)
	-- Add to minetest.registered_abms
	minetest.registered_abms[#minetest.registered_abms+1] = spec
end

function minetest.register_entity(name, prototype)
	-- Check name
	if name == nil then
		error("Unable to register entity: Name is nil")
	end
	name = check_modname_prefix(tostring(name))

	prototype.name = name
	prototype.__index = prototype  -- so that it can be used as a metatable

	-- Add to minetest.registered_entities
	minetest.registered_entities[name] = prototype
end

function minetest.register_item(name, itemdef)
	-- Check name
	if name == nil then
		error("Unable to register item: Name is nil")
	end
	name = check_modname_prefix(tostring(name))
	if forbidden_item_names[name] then
		error("Unable to register item: Name is forbidden: " .. name)
	end
	itemdef.name = name

	-- Apply defaults and add to registered_* table
	if itemdef.type == "node" then
		-- Use the nodebox as selection box if it's not set manually
		if itemdef.drawtype == "nodebox" and not itemdef.selection_box then
			itemdef.selection_box = itemdef.node_box
		elseif itemdef.drawtype == "fencelike" and not itemdef.selection_box then
			itemdef.selection_box = {
				type = "fixed",
				fixed = {-1/8, -1/2, -1/8, 1/8, 1/2, 1/8},
			}
		end
		setmetatable(itemdef, {__index = minetest.nodedef_default})
		minetest.registered_nodes[itemdef.name] = itemdef
	elseif itemdef.type == "craft" then
		setmetatable(itemdef, {__index = minetest.craftitemdef_default})
		minetest.registered_craftitems[itemdef.name] = itemdef
	elseif itemdef.type == "tool" then
		setmetatable(itemdef, {__index = minetest.tooldef_default})
		minetest.registered_tools[itemdef.name] = itemdef
	elseif itemdef.type == "none" then
		setmetatable(itemdef, {__index = minetest.noneitemdef_default})
	else
		error("Unable to register item: Type is invalid: " .. dump(itemdef))
	end

	-- Flowing liquid uses param2
	if itemdef.type == "node" and itemdef.liquidtype == "flowing" and itemdef.paramtype2 == nil then
		itemdef.paramtype2 = "flowingliquid"
	end

	-- BEGIN Legacy stuff
	if itemdef.cookresult_itemstring ~= nil and itemdef.cookresult_itemstring ~= "" then
		minetest.register_craft({
			type="cooking",
			output=itemdef.cookresult_itemstring,
			recipe=itemdef.name,
			cooktime=itemdef.furnace_cooktime
		})
	end
	if itemdef.furnace_burntime ~= nil and itemdef.furnace_burntime >= 0 then
		minetest.register_craft({
			type="fuel",
			recipe=itemdef.name,
			burntime=itemdef.furnace_burntime
		})
	end
	-- END Legacy stuff

	-- Disable all further modifications
	getmetatable(itemdef).__newindex = {}

	--minetest.log("Registering item: " .. itemdef.name)
	minetest.registered_items[itemdef.name] = itemdef
	minetest.registered_aliases[itemdef.name] = nil
	register_item_raw(itemdef)
end

function minetest.register_node(name, nodedef)
	nodedef.type = "node"
	minetest.register_item(name, nodedef)
end

function minetest.register_craftitem(name, craftitemdef)
	craftitemdef.type = "craft"

	-- BEGIN Legacy stuff
	if craftitemdef.inventory_image == nil and craftitemdef.image ~= nil then
		craftitemdef.inventory_image = craftitemdef.image
	end
	-- END Legacy stuff

	minetest.register_item(name, craftitemdef)
end

function minetest.register_tool(name, tooldef)
	tooldef.type = "tool"
	tooldef.stack_max = 1

	-- BEGIN Legacy stuff
	if tooldef.inventory_image == nil and tooldef.image ~= nil then
		tooldef.inventory_image = tooldef.image
	end
	if tooldef.tool_capabilities == nil and
	   (tooldef.full_punch_interval ~= nil or
	    tooldef.basetime ~= nil or
	    tooldef.dt_weight ~= nil or
	    tooldef.dt_crackiness ~= nil or
	    tooldef.dt_crumbliness ~= nil or
	    tooldef.dt_cuttability ~= nil or
	    tooldef.basedurability ~= nil or
	    tooldef.dd_weight ~= nil or
	    tooldef.dd_crackiness ~= nil or
	    tooldef.dd_crumbliness ~= nil or
	    tooldef.dd_cuttability ~= nil) then
		tooldef.tool_capabilities = {
			full_punch_interval = tooldef.full_punch_interval,
			basetime = tooldef.basetime,
			dt_weight = tooldef.dt_weight,
			dt_crackiness = tooldef.dt_crackiness,
			dt_crumbliness = tooldef.dt_crumbliness,
			dt_cuttability = tooldef.dt_cuttability,
			basedurability = tooldef.basedurability,
			dd_weight = tooldef.dd_weight,
			dd_crackiness = tooldef.dd_crackiness,
			dd_crumbliness = tooldef.dd_crumbliness,
			dd_cuttability = tooldef.dd_cuttability,
		}
	end
	-- END Legacy stuff

	minetest.register_item(name, tooldef)
end

function minetest.register_alias(name, convert_to)
	if forbidden_item_names[name] then
		error("Unable to register alias: Name is forbidden: " .. name)
	end
	if minetest.registered_items[name] ~= nil then
		minetest.log("WARNING: Not registering alias, item with same name" ..
			" is already defined: " .. name .. " -> " .. convert_to)
	else
		--minetest.log("Registering alias: " .. name .. " -> " .. convert_to)
		minetest.registered_aliases[name] = convert_to
		register_alias_raw(name, convert_to)
	end
end

local register_biome_raw = minetest.register_biome
minetest.registered_biomes = {}
function minetest.register_biome(biome)
	minetest.registered_biomes[biome.name] = biome
	register_biome_raw(biome)
end

function minetest.on_craft(itemstack, player, old_craft_list, craft_inv)
	for _, func in ipairs(minetest.registered_on_crafts) do
		itemstack = func(itemstack, player, old_craft_list, craft_inv) or itemstack
	end
	return itemstack
end

function minetest.craft_predict(itemstack, player, old_craft_list, craft_inv)
	for _, func in ipairs(minetest.registered_craft_predicts) do
		itemstack = func(itemstack, player, old_craft_list, craft_inv) or itemstack
	end
	return itemstack
end

-- Alias the forbidden item names to "" so they can't be
-- created via itemstrings (e.g. /give)
local name
for name in pairs(forbidden_item_names) do
	minetest.registered_aliases[name] = ""
	register_alias_raw(name, "")
end


-- Deprecated:
-- Aliases for minetest.register_alias (how ironic...)
--minetest.alias_node = minetest.register_alias
--minetest.alias_tool = minetest.register_alias
--minetest.alias_craftitem = minetest.register_alias

--
-- Built-in node definitions. Also defined in C.
--

minetest.register_item(":unknown", {
	type = "none",
	description = "Unknown Item",
	inventory_image = "unknown_item.png",
	on_place = minetest.item_place,
	on_drop = minetest.item_drop,
	groups = {not_in_creative_inventory=1},
	diggable = true,
})

minetest.register_node(":air", {
	description = "Air (you hacker you!)",
	inventory_image = "unknown_node.png",
	wield_image = "unknown_node.png",
	drawtype = "airlike",
	paramtype = "light",
	sunlight_propagates = true,
	walkable = false,
	pointable = false,
	diggable = false,
	buildable_to = true,
	air_equivalent = true,
	drop = "",
	groups = {not_in_creative_inventory=1},
})

minetest.register_node(":ignore", {
	description = "Ignore (you hacker you!)",
	inventory_image = "unknown_node.png",
	wield_image = "unknown_node.png",
	drawtype = "airlike",
	paramtype = "none",
	sunlight_propagates = false,
	walkable = false,
	pointable = false,
	diggable = false,
	buildable_to = true, -- A way to remove accidentally placed ignores
	air_equivalent = true,
	drop = "",
	groups = {not_in_creative_inventory=1},
})

-- The hand (bare definition)
minetest.register_item(":", {
	type = "none",
	groups = {not_in_creative_inventory=1},
})


function minetest.override_item(name, redefinition)
	if redefinition.name ~= nil then
		error("Attempt to redefine name of "..name.." to "..dump(redefinition.name), 2)
	end
	if redefinition.type ~= nil then
		error("Attempt to redefine type of "..name.." to "..dump(redefinition.type), 2)
	end
	local item = minetest.registered_items[name]
	if not item then
		error("Attempt to override non-existent item "..name, 2)
	end
	for k, v in pairs(redefinition) do
		rawset(item, k, v)
	end
	register_item_raw(item)
end


function minetest.run_callbacks(callbacks, mode, ...)
	assert(type(callbacks) == "table")
	local cb_len = #callbacks
	if cb_len == 0 then
		if mode == 2 or mode == 3 then
			return true
		elseif mode == 4 or mode == 5 then
			return false
		end
	end
	local ret = nil
	for i = 1, cb_len do
		local cb_ret = callbacks[i](...)

		if mode == 0 and i == 1 then
			ret = cb_ret
		elseif mode == 1 and i == cb_len then
			ret = cb_ret
		elseif mode == 2 then
			if not cb_ret or i == 1 then
				ret = cb_ret
			end
		elseif mode == 3 then
			if cb_ret then
				return cb_ret
			end
			ret = cb_ret
		elseif mode == 4 then
			if (cb_ret and not ret) or i == 1 then
				ret = cb_ret
			end
		elseif mode == 5 and cb_ret then
			return cb_ret
		end
	end
	return ret
end

--
-- Callback registration
--

local function make_registration()
	local t = {}
	local registerfunc = function(func) table.insert(t, func) end
	return t, registerfunc
end

local function make_registration_reverse()
	local t = {}
	local registerfunc = function(func) table.insert(t, 1, func) end
	return t, registerfunc
end

minetest.registered_on_chat_messages, minetest.register_on_chat_message = make_registration()
minetest.registered_globalsteps, minetest.register_globalstep = make_registration()
minetest.registered_playerevents, minetest.register_playerevent = make_registration()
minetest.registered_on_mapgen_inits, minetest.register_on_mapgen_init = make_registration()
minetest.registered_on_shutdown, minetest.register_on_shutdown = make_registration()
minetest.registered_on_punchnodes, minetest.register_on_punchnode = make_registration()
minetest.registered_on_placenodes, minetest.register_on_placenode = make_registration()
minetest.registered_on_dignodes, minetest.register_on_dignode = make_registration()
minetest.registered_on_generateds, minetest.register_on_generated = make_registration()
minetest.registered_on_newplayers, minetest.register_on_newplayer = make_registration()
minetest.registered_on_dieplayers, minetest.register_on_dieplayer = make_registration()
minetest.registered_on_respawnplayers, minetest.register_on_respawnplayer = make_registration()
minetest.registered_on_prejoinplayers, minetest.register_on_prejoinplayer = make_registration()
minetest.registered_on_joinplayers, minetest.register_on_joinplayer = make_registration()
minetest.registered_on_leaveplayers, minetest.register_on_leaveplayer = make_registration()
minetest.registered_on_player_receive_fields, minetest.register_on_player_receive_fields = make_registration_reverse()
minetest.registered_on_cheats, minetest.register_on_cheat = make_registration()
minetest.registered_on_crafts, minetest.register_on_craft = make_registration()
minetest.registered_craft_predicts, minetest.register_craft_predict = make_registration()
minetest.registered_on_protection_violation, minetest.register_on_protection_violation = make_registration()

minetest.register_on_joinplayer(function(player)
	if minetest.is_singleplayer() then
		return
	end
	local player_name =  player:get_player_name()
	minetest.chat_send_all("*** " .. player_name .. " joined the game.")
end)

minetest.register_on_dieplayer(function(player)
	local player_name =  player:get_player_name()
	if minetest.is_singleplayer() then
		player_name = "You"
	end

	-- Idea from https://github.com/4Evergreen4/death_messages
	-- Death by lava
	local nodename = minetest.get_node(player:getpos()).name
	if nodename == "default:lava_source" or nodename == "default:lava_flowing" then
		minetest.chat_send_all(player_name .. " melted into a ball of fire.")
	-- Death by drowning
	elseif nodename == "default:water_source" or nodename == "default:water_flowing" then
		minetest.chat_send_all(player_name .. " ran out of air.")
	--Death by fire
	elseif nodename == "fire:basic_flame" then
		minetest.chat_send_all(player_name .. " burned up.")
	--Death by something else
	else
		minetest.chat_send_all(player_name .. " died.")
	end

end)
