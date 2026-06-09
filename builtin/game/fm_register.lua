-- local builtin_shared = ...

function core.override_item_if_exists(name, redefinition, del_fields)
	if not core.registered_items[name] then
		return false
	end

	core.override_item(name, redefinition, del_fields)
	return true
end

function core.add_item_groups(name, groups)
	if type(groups) ~= "table" then
		error("Attempt to add invalid groups to item " .. name, 2)
	end

	local item = core.registered_items[name]
	if not item then
		error("Attempt to add groups to non-existent item " .. name, 2)
	end

	local merged_groups = {}
	for group, value in pairs(item.groups or {}) do
		merged_groups[group] = value
	end
	for group, value in pairs(groups) do
		merged_groups[group] = value
	end

	core.override_item(name, {groups = merged_groups})
end

local function update_node(name, groups, fields, del_fields)
	if groups then
		core.add_item_groups(name, groups)
	end
	if fields or del_fields then
		core.override_item_if_exists(name, fields or {}, del_fields)
	end
end
