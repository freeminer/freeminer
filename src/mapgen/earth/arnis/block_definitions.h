#pragma once

/*
struct RGB {
    int r;
    int g;
    int b;
};
*/
//usngg RGB =

namespace arnis
{

enum class StairFacing
{
	North,
	East,
	South,
	West,
};

enum class StairShape
{
	Straight,
	InnerLeft,
	InnerRight,
	OuterLeft,
	OuterRight,
};

inline const char *StairFacing_as_str(StairFacing f) noexcept
{
	switch (f) {
	case StairFacing::North:
		return "north";
	case StairFacing::East:
		return "east";
	case StairFacing::South:
		return "south";
	case StairFacing::West:
		return "west";
	}
	return "";
}

inline const char *StairShape_as_str(StairShape s) noexcept
{
	switch (s) {
	case StairShape::Straight:
		return "straight";
	case StairShape::InnerLeft:
		return "inner_left";
	case StairShape::InnerRight:
		return "inner_right";
	case StairShape::OuterLeft:
		return "outer_left";
	case StairShape::OuterRight:
		return "outer_right";
	}
	return "";
}

Block get_building_wall_block_for_color(const RGB &color);
Block get_fallback_building_block();
Block get_random_floor_block();
Block get_window_block_for_building_type(const std::string &building_type);
Block get_stair_block_for_material(const Block &material);
BlockWithProperties create_stair_with_properties(const Block& base_stair_block, StairFacing facing, StairShape shape) ;















namespace block_definitions
{

// Example constants to match the Rust block references
// You’ll need to define these properly in your code.

extern Block ACACIA_PLANKS;
extern Block AIR;
extern Block ANDESITE;
extern Block BIRCH_LEAVES;
extern Block BIRCH_LOG;
extern Block BLACK_CONCRETE;
extern Block BLACKSTONE;
extern Block BLUE_FLOWER;
extern Block BLUE_TERRACOTTA;
extern Block BRICK;
extern Block CAULDRON;
extern Block CHISELED_STONE_BRICKS;
extern Block COBBLESTONE_WALL;
extern Block COBBLESTONE;
extern Block POLISHED_BLACKSTONE_BRICKS;
extern Block CRACKED_STONE_BRICKS;
extern Block CRIMSON_PLANKS;
extern Block CUT_SANDSTONE;
extern Block CYAN_CONCRETE;
extern Block DARK_OAK_PLANKS;
extern Block DEEPSLATE_BRICKS;
extern Block DIORITE;
extern Block DIRT;
extern Block END_STONE_BRICKS;
extern Block FARMLAND;
extern Block GLASS;
extern Block GLOWSTONE;
extern Block GRANITE;
extern Block GRASS_BLOCK;
extern Block GRASS;
extern Block GRAVEL;
extern Block GRAY_CONCRETE;
extern Block GRAY_TERRACOTTA;
extern Block GREEN_STAINED_HARDENED_CLAY;
extern Block GREEN_WOOL;
extern Block HAY_BALE;
extern Block IRON_BARS;
extern Block IRON_BLOCK;
extern Block JUNGLE_PLANKS;
extern Block LADDER;
extern Block LIGHT_BLUE_CONCRETE;
extern Block LIGHT_BLUE_TERRACOTTA;
extern Block LIGHT_GRAY_CONCRETE;
extern Block MOSS_BLOCK;
extern Block MOSSY_COBBLESTONE;
extern Block MUD_BRICKS;
extern Block NETHER_BRICK;
extern Block NETHERITE_BLOCK;
extern Block OAK_FENCE;
extern Block OAK_LEAVES;
extern Block OAK_LOG;
extern Block OAK_PLANKS;
extern Block OAK_SLAB;
extern Block ORANGE_TERRACOTTA;
extern Block PODZOL;
extern Block POLISHED_ANDESITE;
extern Block POLISHED_BASALT;
extern Block QUARTZ_BLOCK;
extern Block POLISHED_BLACKSTONE;
extern Block POLISHED_DEEPSLATE;
extern Block POLISHED_DIORITE;
extern Block POLISHED_GRANITE;
extern Block PRISMARINE;
extern Block PURPUR_BLOCK;
extern Block PURPUR_PILLAR;
extern Block QUARTZ_BRICKS;
extern Block RAIL;
extern Block RED_FLOWER;
extern Block RED_NETHER_BRICK;
extern Block RED_TERRACOTTA;
extern Block RED_WOOL;
extern Block SAND;
extern Block SANDSTONE;
extern Block SCAFFOLDING;
extern Block SMOOTH_QUARTZ;
extern Block SMOOTH_RED_SANDSTONE;
extern Block SMOOTH_SANDSTONE;
extern Block SMOOTH_STONE;
extern Block SPONGE;
extern Block SPRUCE_LOG;
extern Block SPRUCE_PLANKS;
extern Block STONE_BLOCK_SLAB;
extern Block STONE_BRICK_SLAB;
extern Block STONE_BRICKS;
extern Block STONE;
extern Block TERRACOTTA;
extern Block WARPED_PLANKS;
extern Block WATER;
extern Block WHITE_CONCRETE;
extern Block WHITE_FLOWER;
extern Block WHITE_STAINED_GLASS;
extern Block WHITE_TERRACOTTA;
extern Block WHITE_WOOL;
extern Block YELLOW_CONCRETE;
extern Block YELLOW_FLOWER;
extern Block YELLOW_WOOL;
extern Block LIME_CONCRETE;
extern Block CYAN_WOOL;
extern Block BLUE_CONCRETE;
extern Block PURPLE_CONCRETE;
extern Block RED_CONCRETE;
extern Block MAGENTA_CONCRETE;
extern Block BROWN_WOOL;
extern Block OXIDIZED_COPPER;
extern Block YELLOW_TERRACOTTA;
extern Block SNOW_BLOCK;
extern Block SNOW_LAYER;
extern Block SIGN;
extern Block ANDESITE_WALL;
extern Block STONE_BRICK_WALL;
extern Block CARROTS;
extern Block DARK_OAK_DOOR_LOWER;
extern Block DARK_OAK_DOOR_UPPER;
extern Block POTATOES;
extern Block WHEAT;
extern Block BEDROCK;
extern Block RAIL_NORTH_SOUTH;
extern Block RAIL_EAST_WEST;
extern Block RAIL_ASCENDING_EAST;
extern Block RAIL_ASCENDING_WEST;
extern Block RAIL_ASCENDING_NORTH;
extern Block RAIL_ASCENDING_SOUTH;
extern Block RAIL_NORTH_EAST;
extern Block RAIL_NORTH_WEST;
extern Block RAIL_SOUTH_EAST;
extern Block RAIL_SOUTH_WEST;
extern Block COARSE_DIRT;
extern Block IRON_ORE;
extern Block COAL_ORE;
extern Block GOLD_ORE;
extern Block COPPER_ORE;
extern Block CLAY;
extern Block DIRT_PATH;
extern Block ICE;
extern Block PACKED_ICE;
extern Block MUD;
extern Block DEAD_BUSH;
extern Block TALL_GRASS_BOTTOM;
extern Block TALL_GRASS_TOP;
extern Block CRAFTING_TABLE;
extern Block FURNACE;
extern Block WHITE_CARPET;
extern Block BOOKSHELF;
extern Block OAK_PRESSURE_PLATE;
extern Block OAK_STAIRS;
extern Block CHEST;
extern Block RED_CARPET;
extern Block ANVIL;
extern Block NOTE_BLOCK;
extern Block OAK_DOOR;
extern Block BREWING_STAND;
extern Block RED_BED_NORTH_HEAD;
extern Block RED_BED_NORTH_FOOT;
extern Block RED_BED_EAST_HEAD;
extern Block RED_BED_EAST_FOOT;
extern Block RED_BED_SOUTH_HEAD;
extern Block RED_BED_SOUTH_FOOT;
extern Block RED_BED_WEST_HEAD;
extern Block RED_BED_WEST_FOOT;
extern Block GRAY_STAINED_GLASS;
extern Block LIGHT_GRAY_STAINED_GLASS;
extern Block BROWN_STAINED_GLASS;
extern Block TINTED_GLASS;
extern Block OAK_TRAPDOOR;
extern Block BROWN_CONCRETE;
extern Block BLACK_TERRACOTTA;
extern Block BROWN_TERRACOTTA;
extern Block STONE_BRICK_STAIRS;
extern Block MUD_BRICK_STAIRS;
extern Block POLISHED_BLACKSTONE_BRICK_STAIRS;
extern Block BRICK_STAIRS;
extern Block POLISHED_GRANITE_STAIRS;
extern Block END_STONE_BRICK_STAIRS;
extern Block POLISHED_DIORITE_STAIRS;
extern Block SMOOTH_SANDSTONE_STAIRS;
extern Block QUARTZ_STAIRS;
extern Block POLISHED_ANDESITE_STAIRS;
extern Block NETHER_BRICK_STAIRS;

extern Block &SMOOTH_STONE_BLOCK;// = SMOOTH_STONE;
}

using namespace block_definitions;

}