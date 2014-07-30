
minetest.register_node("circuit:logic_gate", {
	description = "Gate",
	circuit_states = {16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16,
	                  16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16,
	                  0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
	                  0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0},
	tiles = {"default_logic_gate.png", "default_logic_gate_side.png"},
	groups = {snappy=2,choppy=3,oddly_breakable_by_hand=2,dig_immediate=3},
	paramtype2 = "facedir",
	sounds = default.node_sound_stone_defaults(),
})

minetest.register_node("circuit:logic_source", {
	description = "Source",
	circuit_states = {63, 63, 63, 63, 63, 63, 63, 63, 63, 63, 63, 63, 63, 63, 63, 63,
	                  63, 63, 63, 63, 63, 63, 63, 63, 63, 63, 63, 63, 63, 63, 63, 63,
	                  63, 63, 63, 63, 63, 63, 63, 63, 63, 63, 63, 63, 63, 63, 63, 63,
	                  63, 63, 63, 63, 63, 63, 63, 63, 63, 63, 63, 63, 63, 63, 63, 63},
	tiles = {"default_logic_source.png", "default_logic_source_side.png"},
	groups = {snappy=2,choppy=3,oddly_breakable_by_hand=2,dig_immediate=3},
	sounds = default.node_sound_stone_defaults(),
})

minetest.register_node("circuit:wire", {
	description = "Wire",
	is_wire = true,
	tiles = {"default_wire.png", "default_wire_side.png"},
	groups = {snappy=2,choppy=3,oddly_breakable_by_hand=2,dig_immediate=3},
	sounds = default.node_sound_stone_defaults(),
})


minetest.register_node("circuit:wire_connector", {
	description = "Wire connector",
	is_wire = true,
	is_connector = true,
	wire_connections = {8, 16, 32, 1, 2, 4},
	tiles = {"default_wire2.png", "default_wire2_side.png"},
	groups = {snappy=2,choppy=3,oddly_breakable_by_hand=2,dig_immediate=3},
	sounds = default.node_sound_stone_defaults(),
})


minetest.register_node("circuit:lever", {
	description = "Lever",
	circuit_states = {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
	                  0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
	                  0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
	                  0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0},
	on_rightclick = function(pos)
		node = minetest.get_node(pos)
		minetest.swap_node(pos, {name = "circuit:active_lever",
		                   param1 = node.param1, param2 = node.param2})
	end,
	tiles = {"default_lever.png", "default_lever_side.png"},
	groups = {snappy=2,choppy=3,oddly_breakable_by_hand=2,dig_immediate=3},
	sounds = default.node_sound_stone_defaults(),
})

minetest.register_node("circuit:active_lever", {
	description = "Active lever",
	circuit_states = {63, 63, 63, 63, 63, 63, 63, 63, 63, 63, 63, 63, 63, 63, 63, 63,
	                  63, 63, 63, 63, 63, 63, 63, 63, 63, 63, 63, 63, 63, 63, 63, 63,
	                  63, 63, 63, 63, 63, 63, 63, 63, 63, 63, 63, 63, 63, 63, 63, 63,
	                  63, 63, 63, 63, 63, 63, 63, 63, 63, 63, 63, 63, 63, 63, 63, 63},
	on_rightclick = function(pos)
		node = minetest.get_node(pos)
		minetest.swap_node(pos, {name = "circuit:lever",
		                   param1 = node.param1, param2 = node.param2})
	end,
	tiles = {"default_active_lever.png", "default_active_lever_side.png"},
	groups = {snappy=2,choppy=3,oddly_breakable_by_hand=2,dig_immediate=3},
	sounds = default.node_sound_stone_defaults(),
})

minetest.register_node("circuit:lamp", {
	description = "Lamp",
	on_activate = function(pos)
		node = minetest.get_node(pos)
		node.name = "circuit:active_lamp"
		minetest.swap_node(pos, node)
	end,
	tiles = {"default_lamp.png"},
	groups = {snappy=2,choppy=3,oddly_breakable_by_hand=2,dig_immediate=3},
	sounds = default.node_sound_stone_defaults(),
})

minetest.register_node("circuit:active_lamp", {
	description = "Active lamp",
	on_deactivate = function(pos)
		node = minetest.get_node(pos)
		minetest.swap_node(pos, {name = "circuit:lamp",
		                   param1 = node.param1, param2 = node.param2})
	end,
	tiles = {"default_active_lamp.png"},
	groups = {snappy=2,choppy=3,oddly_breakable_by_hand=2,dig_immediate=3},
	sounds = default.node_sound_stone_defaults(),
})
