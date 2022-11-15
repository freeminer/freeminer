-- Craft recipes and textures are from mesecons mod

minetest.register_node("circuit:gate_not", {
	description = "NOT Gate",
	circuit_states = {16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16,
	                  16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16,
	                  0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
	                  0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0},
	tiles = {"circuit_gate_not.png", "circuit_gate_bottom.png", "circuit_gate_side.png"},
	groups = {snappy=2,choppy=3,oddly_breakable_by_hand=2,dig_immediate=3},
	paramtype2 = "facedir",
	sounds = default.node_sound_stone_defaults(),
})

minetest.register_node("circuit:gate_and", {
	description = "AND Gate",
	circuit_states = {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 16, 16, 16, 16,
                      0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 16, 16, 16, 16,
	                  0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 16, 16, 16, 16,
	                  0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 16, 16, 16, 16},
	tiles = {"circuit_gate_and.png", "circuit_gate_bottom.png", "circuit_gate_side.png"},
	groups = {snappy=2,choppy=3,oddly_breakable_by_hand=2,dig_immediate=3},
	paramtype2 = "facedir",
	sounds = default.node_sound_stone_defaults(),
})

minetest.register_node("circuit:gate_nand", {
	description = "NAND Gate",
	circuit_states = {16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 0, 0, 0, 0,
	                  16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 0, 0, 0, 0,
	                  16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 0, 0, 0, 0,
	                  16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 0, 0, 0, 0},
	tiles = {"circuit_gate_nand.png", "circuit_gate_bottom.png", "circuit_gate_side.png"},
	groups = {snappy=2,choppy=3,oddly_breakable_by_hand=2,dig_immediate=3},
	paramtype2 = "facedir",
	sounds = default.node_sound_stone_defaults(),
})

minetest.register_node("circuit:gate_or", {
	description = "OR Gate",
	circuit_states = {0, 0, 0, 0, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16,
	                  0, 0, 0, 0, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16,
	                  0, 0, 0, 0, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16,
	                  0, 0, 0, 0, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16},
	tiles = {"circuit_gate_or.png", "circuit_gate_bottom.png", "circuit_gate_side.png"},
	groups = {snappy=2,choppy=3,oddly_breakable_by_hand=2,dig_immediate=3},
	paramtype2 = "facedir",
	sounds = default.node_sound_stone_defaults(),
})

minetest.register_node("circuit:gate_nor", {
	description = "NOR Gate",
	circuit_states = {16, 16, 16, 16, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
	                  16, 16, 16, 16, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
	                  16, 16, 16, 16, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
	                  16, 16, 16, 16, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0},
	tiles = {"circuit_gate_nor.png", "circuit_gate_bottom.png", "circuit_gate_side.png"},
	groups = {snappy=2,choppy=3,oddly_breakable_by_hand=2,dig_immediate=3},
	paramtype2 = "facedir",
	sounds = default.node_sound_stone_defaults(),
})

minetest.register_node("circuit:gate_xor", {
	description = "XOR Gate",
	circuit_states = {0, 0, 0, 0, 16, 16, 16, 16, 16, 16, 16, 16, 0, 0, 0, 0,
	                  0, 0, 0, 0, 16, 16, 16, 16, 16, 16, 16, 16, 0, 0, 0, 0,
	                  0, 0, 0, 0, 16, 16, 16, 16, 16, 16, 16, 16, 0, 0, 0, 0,
	                  0, 0, 0, 0, 16, 16, 16, 16, 16, 16, 16, 16, 0, 0, 0, 0},
	tiles = {"circuit_gate_xor.png", "circuit_gate_bottom.png", "circuit_gate_side.png"},
	groups = {snappy=2,choppy=3,oddly_breakable_by_hand=2,dig_immediate=3},
	paramtype2 = "facedir",
	sounds = default.node_sound_stone_defaults(),
})

minetest.register_node("circuit:gate_xnor", {
	description = "XNOR Gate",
	circuit_states = {16, 16, 16, 16, 0, 0, 0, 0, 0, 0, 0, 0, 16, 16, 16, 16,
	                  16, 16, 16, 16, 0, 0, 0, 0, 0, 0, 0, 0, 16, 16, 16, 16,
	                  16, 16, 16, 16, 0, 0, 0, 0, 0, 0, 0, 0, 16, 16, 16, 16,
	                  16, 16, 16, 16, 0, 0, 0, 0, 0, 0, 0, 0, 16, 16, 16, 16},
	tiles = {"circuit_gate_xor.png", "circuit_gate_bottom.png", "circuit_gate_side.png"},
	groups = {snappy=2,choppy=3,oddly_breakable_by_hand=2,dig_immediate=3},
	paramtype2 = "facedir",
	sounds = default.node_sound_stone_defaults(),
})

minetest.register_node("circuit:source", {
	description = "Source",
	circuit_states = {63, 63, 63, 63, 63, 63, 63, 63, 63, 63, 63, 63, 63, 63, 63, 63,
	                  63, 63, 63, 63, 63, 63, 63, 63, 63, 63, 63, 63, 63, 63, 63, 63,
	                  63, 63, 63, 63, 63, 63, 63, 63, 63, 63, 63, 63, 63, 63, 63, 63,
	                  63, 63, 63, 63, 63, 63, 63, 63, 63, 63, 63, 63, 63, 63, 63, 63},
	tiles = {"circuit_source.png", "circuit_source.png", "circuit_source_side.png"},
	groups = {snappy=2,choppy=3,oddly_breakable_by_hand=2,dig_immediate=3},
	sounds = default.node_sound_stone_defaults(),
})

minetest.register_node("circuit:wire", {
	description = "Wire",
	is_wire = true,
	tiles = {"circuit_wire.png", "circuit_wire.png", "circuit_wire_side.png"},
	groups = {snappy=2,choppy=3,oddly_breakable_by_hand=2,dig_immediate=3},
	sounds = default.node_sound_stone_defaults(),
})


minetest.register_node("circuit:wire_connector", {
	description = "Wire connector",
	is_wire = true,
	is_connector = true,
	wire_connections = {2, 1, 8, 4, 32, 16},
	tiles = {"circuit_wire2.png", "circuit_wire2.png", "circuit_wire2_side.png"},
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
	tiles = {"circuit_lever.png", "circuit_lever.png", "circuit_lever_side.png"},
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
	drop = "circuit:lever",
	tiles = {"circuit_active_lever.png", "circuit_active_lever.png", "circuit_active_lever_side.png"},
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
	tiles = {"circuit_lamp.png"},
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
	drop = "circuit:lamp",
	tiles = {"circuit_active_lamp.png"},
	groups = {snappy=2,choppy=3,oddly_breakable_by_hand=2,dig_immediate=3},
	sounds = default.node_sound_stone_defaults(),
})

minetest.register_craftitem("circuit:silicon", {
	image = "jeija_silicon.png",
	on_place_on_ground = minetest.craftitem_place_item,
    	description="Silicon",
})

minetest.register_craft({
	output = "circuit:silicon 4",
	recipe = {
		{"default:sand", "default:sand"},
		{"default:sand", "default:steel_ingot"},
	}
})

minetest.register_craft({
		output = "circuit:wire 16",
		recipe = {
			   {"default:wood", "default:wood", "default:wood"},
			   {"default:steel_ingot", "default:steel_ingot", "default:steel_ingot"},
			   {"default:wood", "default:wood", "default:wood"},
		}
})

minetest.register_craft({
		output = "circuit:wire_connector 16",
		recipe = {
			   {"default:wood", "default:steel_ingot", "default:wood"},
			   {"default:steel_ingot", "default:steel_ingot", "default:steel_ingot"},
			   {"default:wood", "default:steel_ingot", "default:wood"},
		}
})

minetest.register_craft({
		output = "circuit:source 4",
		recipe = {
			   {"circuit:silicon", "circuit:silicon"},
			   {"circuit:silicon", "circuit:silicon"},
		}
})

minetest.register_craft({
		output = "circuit:lever 4",
		recipe = {
			   {"default:stick", "default:wood"},
			   {"circuit:silicon", "default:steel_ingot"},
		}
})

minetest.register_craft({
		output = "circuit:lamp 4",
		recipe = {
			   {"default:wood", "circuit:silicon"},
			   {"circuit:silicon", "default:wood"},
		}
})

minetest.register_craft({
		output = "circuit:gate_and 4",
		recipe = {
			   {"circuit:silicon"},
			   {"circuit:silicon"},
			   {"circuit:silicon"},
		}
})

minetest.register_craft({
		output = "circuit:gate_nand 4",
		recipe = {
			   {"circuit:silicon", ""},
			   {"circuit:silicon", "circuit:silicon"},
			   {"circuit:silicon", ""},
		}
})

minetest.register_craft({
		output = "circuit:gate_or 4",
		recipe = {
			   {"circuit:silicon"},
			   {""},
			   {"circuit:silicon"},
		}
})

minetest.register_craft({
		output = "circuit:gate_nor 4",
		recipe = {
			   {"circuit:silicon", ""},
			   {"", "circuit:silicon"},
			   {"circuit:silicon", ""},
		}
})

minetest.register_craft({
		output = "circuit:gate_xor 4",
		recipe = {
			   {"", "circuit:silicon"},
			   {"circuit:silicon", ""},
			   {"", "circuit:silicon"},
		}
})

minetest.register_craft({
		output = "circuit:gate_xnor 4",
		recipe = {
			   {"", "circuit:silicon"},
			   {"circuit:silicon", "circuit:silicon"},
			   {"", "circuit:silicon"},
		}
})

minetest.register_craft({
		output = "circuit:gate_not 4",
		recipe = {
			   {"circuit:silicon", "circuit:silicon"},
		}
})