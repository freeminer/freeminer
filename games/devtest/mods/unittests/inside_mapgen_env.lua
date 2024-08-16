core.log("info", "Hello World")

local function do_tests()
	assert(core == minetest)
	-- stuff that should not be here
	assert(not core.get_player_by_name)
	assert(not core.object_refs)
	-- stuff that should be here
	assert(core.register_on_generated)
	assert(core.get_node)
	assert(core.spawn_tree)
	assert(ItemStack)
	local meta = ItemStack():get_meta()
	assert(type(meta) == "userdata")
	assert(type(meta.set_tool_capabilities) == "function")
	assert(core.registered_items[""])
	assert(core.save_gen_notify)
	-- alias handling
	assert(core.registered_items["unittests:steel_ingot_alias"].name ==
		"unittests:steel_ingot")
	-- fallback to item defaults
	assert(core.registered_items["unittests:description_test"].on_place == true)
end

-- there's no (usable) communcation path between mapgen and the regular env
-- so we just run the test unconditionally
do_tests()

core.register_on_generated(function(vm, pos1, pos2, blockseed)
	local n = tonumber(core.get_mapgen_setting("chunksize")) * 16 - 1
	assert(pos2:subtract(pos1) == vector.new(n, n, n))
end)
