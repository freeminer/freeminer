-- Freeminer: builtin/game/mod_debugging.lua
-- by rubenwardy

local mod = {}
mod.recipes = {}
mod.aliases = {}

core.log("action", 'Mod debugging enabled')
-- Sees if there is a node with this group/these groups
-- Supports AND view group:name, name
local function group_exists(groupname)
	local flags = groupname:split(",")
	for name, def in pairs(core.registered_items) do
		local flag = true
		for k, v in pairs(flags) do
			local g = def.groups and def.groups[v:gsub('%group:', '')] or 0
			if not g or g <= 0 then
				flag = false
				break
			end
		end
		if flag then
			return true
		end
	end
	return false
end

-- Check that the item exists
function mod.assert(recipe, _name, output)
	local name = mod.strip_name(_name)
	if name == nil then
		core.log('error', 'nil in recipe for '..mod.strip_name(output))
		print(recipe.from)
		return
	end

	if mod.aliases[name] ~= nil then
		name = mod.aliases[name]
	end

	if core.registered_items[name] == nil and not group_exists(name) then
		core.log( 'error', 'missing item '..name.." in recipe for "..mod.strip_name(output) )
		print(recipe.from)
	end
end

-- Turns a itemstack name into just its item name
-- For example: "mod:item 99" -> "mod:item"
function mod.strip_name(name)
	if name == nil then
		return
	end

	res = name:gsub('%"', '')

	if res:sub(1, 1) == ":" then
    	res = table.concat{res:sub(1, 1-1), "", res:sub(1+1)}
	end

	for str in string.gmatch(res, "([^ ]+)") do
		if str ~= " " and str ~= nil then
			res=str
			break
		end
	end

	if res == nil then
		res=""
	end

	return res
end

-- Cycles through the recipe table, checking the items.
-- Recursive
function mod.check_recipe(recipe, table, output)
	if type(table) == "table" then
		for i=1,# table do
			mod.check_recipe(recipe, table[i], output)
		end
	else
		mod.assert(recipe, table,output)
	end
end

-- Check recipes once the game has loaded
core.after(0, function()
for i=1, #mod.recipes do
	if mod.recipes[i] and mod.recipes[i].output then
		mod.assert(mod.recipes[i], mod.recipes[i].output, mod.recipes[i].output)

		if type(mod.recipes[i].recipe) == "table" then
			for a=1,# mod.recipes[i].recipe do
				mod.check_recipe(mod.recipes[i], mod.recipes[i].recipe[a], mod.recipes[i].output)
			end
		else
			mod.assert(mod.recipes[i], mod.recipes[i].recipe, mod.recipes[i].output)
		end
	end
end
end)

-- Override register_craft to catch craft recipes
local register_craft = core.register_craft
core.register_craft = function(recipe)
	register_craft(recipe)

	local name = mod.strip_name(recipe.output)
	recipe.from = debug.traceback()
	if name~=nil then
		table.insert(mod.recipes, recipe)
	end
end

-- Override register_alias to catch aliases
local register_alias = core.register_alias
core.register_alias = function(new, old)
	register_alias(new, old)

	local name = mod.strip_name(new)
	local name2 = mod.strip_name(old)
	if name~=nil and name2~=nil then
		mod.aliases[new] = old
	end
end
