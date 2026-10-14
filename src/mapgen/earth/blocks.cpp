#include "arnis_adapter.h"
#include "emerge.h"
#include "log.h"

#include <algorithm>
#include <cctype>
#include <initializer_list>
#include <mutex>
#include <string>
#include <unordered_map>
#include <vector>

namespace arnis
{

namespace block_definitions
{

Block ACACIA_PLANKS;
Block AIR;
Block ANDESITE;
Block ANDESITE_SLAB;
Block BAMBOO_SLAB;
Block BAMBOO_STAIRS;
Block BIRCH_LEAVES;
Block BIRCH_LOG;
Block BIRCH_BUTTON;
Block BIRCH_DOOR;
Block BIRCH_FENCE;
Block BIRCH_PRESSURE_PLATE;
Block BLACK_CONCRETE;
Block BLACKSTONE;
Block BLACKSTONE_STAIRS;
Block BLACKSTONE_WALL;
Block BLUE_ICE;
Block BLUE_FLOWER;
Block BLUE_TERRACOTTA;
Block BRICK;
Block CAULDRON;
Block CHISELED_STONE_BRICKS;
Block COBBLESTONE_WALL;
Block COBBLESTONE;
Block POLISHED_BLACKSTONE_BRICKS;
Block CHISELED_POLISHED_BLACKSTONE;
Block CHISELED_DEEPSLATE;
Block COAL_BLOCK;
Block COBBLESTONE_SLAB;
Block DARK_PRISMARINE;
Block DARK_PRISMARINE_SLAB;
Block DARK_PRISMARINE_STAIRS;
Block CRACKED_STONE_BRICKS;
Block CRIMSON_PLANKS;
Block CRIMSON_SLAB;
Block CRIMSON_STAIRS;
Block CUT_SANDSTONE;
Block CUT_SANDSTONE_SLAB;
Block CYAN_CARPET;
Block CYAN_CONCRETE;
Block DARK_OAK_PLANKS;
Block DARK_OAK_SLAB;
Block DARK_OAK_FENCE;
Block DEEPSLATE_BRICKS;
Block DEEPSLATE_TILES;
Block DEEPSLATE_TILE_SLAB;
Block DEEPSLATE_TILE_WALL;
Block DIORITE;
Block DIORITE_STAIRS;
Block DIORITE_WALL;
Block DIRT;
Block END_STONE_BRICKS;
Block END_STONE;
Block END_STONE_BRICK_SLAB;
Block END_STONE_BRICK_WALL;
Block FARMLAND;
Block GLASS;
Block GLOWSTONE;
Block GRANITE;
Block GRASS_BLOCK;
Block GRASS;
Block SNOWY_GRASS_BLOCK;
Block GRAVEL;
Block GRAY_CONCRETE;
Block GRAY_TERRACOTTA;
Block GREEN_STAINED_HARDENED_CLAY;
Block GREEN_WOOL;
Block HAY_BALE;
Block LANTERN;
Block SOUL_LANTERN;
Block SMOKER;
Block EMPTY_FLOWER_POT;
Block COMPOSTER;
Block HOPPER;
Block BLAST_FURNACE;
Block DISPENSER;
Block GRINDSTONE;
Block POLISHED_BLACKSTONE_SLAB;
Block REDSTONE_LAMP;
Block AMETHYST_CLUSTER;
Block CHISELED_QUARTZ_BLOCK;
Block IRON_BARS;
Block IRON_DOOR;
Block IRON_BLOCK;
Block JUNGLE_PLANKS;
Block JUNGLE_SLAB;
Block JUNGLE_STAIRS;
Block LADDER;
Block LIGHT_BLUE_CONCRETE;
Block LIGHT_BLUE_TERRACOTTA;
Block LIGHT_GRAY_CONCRETE;
Block MOSS_BLOCK;
Block MOSSY_COBBLESTONE;
Block MUD_BRICKS;
Block NETHER_BRICK;
Block NETHER_BRICK_FENCE;
Block NETHER_BRICK_WALL;
Block NETHER_WART_BLOCK;
Block NETHERITE_BLOCK;
Block OAK_FENCE;
Block OAK_LEAVES;
Block OAK_LOG;
Block OAK_PLANKS;
Block OAK_SLAB;
Block SPRUCE_SLAB;
Block SPRUCE_FENCE;
Block ORANGE_TERRACOTTA;
Block PODZOL;
Block SNOWY_PODZOL;
Block POLISHED_ANDESITE;
Block POLISHED_BASALT;
Block QUARTZ_BLOCK;
Block POLISHED_BLACKSTONE;
Block BLACKSTONE_SLAB;
Block POLISHED_DEEPSLATE;
Block POLISHED_DIORITE;
Block POLISHED_GRANITE;
Block PRISMARINE;
Block PURPUR_BLOCK;
Block PURPUR_PILLAR;
Block QUARTZ_BRICKS;
Block RAIL;
Block POWERED_RAIL;
Block RED_FLOWER;
Block RED_NETHER_BRICK;
Block RED_TERRACOTTA;
Block RED_WOOL;
Block SAND;
Block SANDSTONE;
Block SCAFFOLDING;
Block SMOOTH_QUARTZ;
Block SMOOTH_RED_SANDSTONE;
Block SMOOTH_SANDSTONE;
Block SMOOTH_STONE;
Block SPONGE;
Block SPRUCE_LOG;
Block SPRUCE_PLANKS;
Block STONE_BLOCK_SLAB;
Block STONE_BRICK_SLAB;
Block STONE_BRICKS;
Block STONE;
Block TERRACOTTA;
Block WARPED_PLANKS;
Block WARPED_STAIRS;
Block WARPED_TRAPDOOR;
Block WARPED_SLAB;
Block STRIPPED_WARPED_STEM;
Block STRIPPED_WARPED_HYPHAE;
Block WATER;
Block SEAGRASS;
Block KELP_PLANT;
Block MAGMA_BLOCK;
Block OBSIDIAN;
Block KELP;
Block TALL_SEAGRASS_BOTTOM;
Block TALL_SEAGRASS_TOP;
Block SEA_PICKLE;
Block SOUL_SAND;
Block EARTH_BENCH;
Block EARTH_TRASH_CAN;
Block EARTH_STREET_LAMP;
Block EARTH_WELL;
Block EARTH_BARBECUE;
Block EARTH_GRATING;
Block EARTH_FENCE_CHAINLINK;
Block EARTH_FENCE_BARBED;
Block EARTH_FENCE_PICKET;
Block EARTH_FENCE_WROUGHT;
Block WHITE_CONCRETE;
Block WHITE_FLOWER;
Block WHITE_STAINED_GLASS;
Block WHITE_TERRACOTTA;
Block WHITE_WOOL;
Block YELLOW_CONCRETE;
Block YELLOW_FLOWER;
Block YELLOW_WOOL;
Block LIME_CONCRETE;
Block CYAN_WOOL;
Block GRAY_WOOL;
Block BLUE_CONCRETE;
Block PURPLE_CONCRETE;
Block RED_CONCRETE;
Block MAGENTA_CONCRETE;
Block BROWN_WOOL;
Block OXIDIZED_COPPER;
Block YELLOW_TERRACOTTA;
Block SNOW_BLOCK;
Block SNOW_LAYER;
Block SIGN;
Block STEEL_SIGN;
Block TEXT_SIGN_SMALL;
Block TEXT_SIGN_MEDIUM;
Block TEXT_SIGN_LARGE;
bool STREETS_AVAILABLE = false;
Block ROAD_ASPHALT;
Block ROAD_SIDEWALK;
Block ROAD_MARK_DASHED_WHITE;
Block ROAD_MARK_DASHED_WHITE_R90;
Block ROAD_MARK_SOLID_WHITE_STRIPE;
Block ROAD_MARK_SOLID_WHITE_STRIPE_R90;
bool STREETS_MARKINGS_AVAILABLE = false;
Block STREETS_POLE;
Block STREETS_BOLLARD;
Block STREETS_GUARDRAIL;
std::array<Block, 3> STREETS_TRAFFIC_LIGHTS;
bool STREETS_RRXING_AVAILABLE = false;
Block STREETS_RRXING_BOTTOM;
Block STREETS_RRXING_MIDDLE;
Block STREETS_RRXING_TOP;
Block STREETS_EU_SIGN_STOP;
Block STREETS_EU_SIGN_YIELD;
Block STREETS_EU_SIGN_NO_ENTRY;
Block STREETS_EU_SIGN_CROSSING;
Block STREETS_EU_SIGN_CROSSBUCK;
std::array<Block, 6> STREETS_EU_SPEED_SIGNS;
Block STREETS_US_SIGN_STOP;
Block STREETS_US_SIGN_YIELD;
Block STREETS_US_SIGN_NO_ENTRY;
Block STREETS_US_SIGN_CROSSING;
Block STREETS_US_SIGN_CROSSBUCK;
bool STREET_SIGNS_AVAILABLE = false;
Block STREET_SIGN_BASIC;
Block STREET_SIGN_STOP;
Block STREET_SIGN_YIELD;
Block STREET_SIGN_SPEED_LIMIT;
Block STREET_SIGN_DO_NOT_ENTER;
Block STREET_SIGN_PEDESTRIAN_CROSSING;
Block STREET_SIGN_RR_CROSSBUCK;
Block STREET_SIGN_US_ROUTE;
Block STREET_SIGN_US_INTERSTATE;
Block DECAL_FRAME;
Block ANDESITE_WALL;
Block STONE_BRICK_WALL;
Block CARROTS;
Block DARK_OAK_DOOR_LOWER;
Block DARK_OAK_DOOR_UPPER;
Block DARK_OAK_LOG;
Block DARK_OAK_LEAVES;
Block JUNGLE_LOG;
Block JUNGLE_LEAVES;
Block ACACIA_LOG;
Block ACACIA_LEAVES;
Block POTATOES;
Block WHEAT;
Block BEDROCK;
Block RAIL_NORTH_SOUTH;
Block RAIL_EAST_WEST;
Block RAIL_ASCENDING_EAST;
Block RAIL_ASCENDING_WEST;
Block RAIL_ASCENDING_NORTH;
Block RAIL_ASCENDING_SOUTH;
Block RAIL_NORTH_EAST;
Block RAIL_NORTH_WEST;
Block RAIL_SOUTH_EAST;
Block RAIL_SOUTH_WEST;
Block ADV_RAIL_NORTH_SOUTH;
Block ADV_RAIL_EAST_WEST;
Block ADV_RAIL_DIAGONAL_NE_SW;
Block ADV_RAIL_DIAGONAL_NW_SE;
Block ADV_RAIL_STRAIGHT_0;
Block ADV_RAIL_STRAIGHT_30;
Block ADV_RAIL_STRAIGHT_45;
Block ADV_RAIL_STRAIGHT_60;
Block ADV_RAIL_CURVE_0;
Block ADV_RAIL_CURVE_30;
Block ADV_RAIL_CURVE_45;
Block ADV_RAIL_CURVE_60;
std::array<Block, 4> ADV_RAIL_SWITCH_LEFT_STRAIGHT;
std::array<Block, 4> ADV_RAIL_SWITCH_RIGHT_STRAIGHT;
std::array<Block, 4> ADV_RAIL_Y_TURNOUT;
std::array<Block, 4> ADV_RAIL_THREE_WAY_STRAIGHT;
std::array<Block, 4> ADV_RAIL_PERP_CROSSING;
std::array<Block, 6> ADV_RAIL_90_PLUS_CROSSING;
std::array<Block, 7> ADV_RAIL_DIAGONAL_CROSSING;
Block ADV_RAIL_SLOPE_UP;
Block ADV_RAIL_SLOPE_DOWN;
std::array<Block, 3> ADV_RAIL_GENTLE_SLOPE;
std::array<Block, 2> ADV_RAIL_DIAGONAL_SLOPE;
bool ADVTRAINS_DIAGONAL_SLOPES_AVAILABLE = false;
bool ADVTRAINS_JUNCTIONS_AVAILABLE = false;
bool ADVTRAINS_CROSSINGS_AVAILABLE = false;
bool ADVTRAINS_SLOPES_AVAILABLE = false;
bool ADVTRAINS_GENTLE_SLOPES_AVAILABLE = false;
bool ADVTRAINS_AVAILABLE = false;
Block ADV_PLATFORM_HIGH;
Block CACTUS;
Block COARSE_DIRT;
Block IRON_ORE;
Block COAL_ORE;
Block GOLD_ORE;
Block COPPER_ORE;
Block LAPIS_ORE;
Block REDSTONE_ORE;
Block DIAMOND_ORE;
Block DEEPSLATE_COAL_ORE;
Block DEEPSLATE_IRON_ORE;
Block DEEPSLATE_COPPER_ORE;
Block DEEPSLATE_GOLD_ORE;
Block DEEPSLATE_REDSTONE_ORE;
Block DEEPSLATE_LAPIS_ORE;
Block DEEPSLATE_DIAMOND_ORE;
Block CLAY;
Block DIRT_PATH;
Block ICE;
Block PACKED_ICE;
Block LAVA;
Block POWDER_SNOW;
Block AMETHYST_BLOCK;
Block BUDDING_AMETHYST;
Block CALCITE;
Block BASALT;
Block SMOOTH_BASALT;
Block CAVE_VINES;
Block CAVE_VINES_PLANT;
Block CAVE_VINES_UNLIT;
Block CAVE_VINES_PLANT_LIT;
Block SPORE_BLOSSOM;
Block AZALEA;
Block FLOWERING_AZALEA;
Block AZALEA_LEAVES;
Block FLOWERING_AZALEA_LEAVES;
Block WHITE_BED;
Block LECTERN;
Block CAKE;
Block MELON;
Block LOOM;
Block SMITHING_TABLE;
Block RED_MUSHROOM_BLOCK;
Block BROWN_MUSHROOM_BLOCK;
Block MUSHROOM_STEM;
Block SHROOMLIGHT;
Block TUBE_CORAL_BLOCK;
Block BRAIN_CORAL_BLOCK;
Block BUBBLE_CORAL_BLOCK;
Block FIRE_CORAL_BLOCK;
Block HORN_CORAL_BLOCK;
Block DEAD_TUBE_CORAL_BLOCK;
Block DEAD_BRAIN_CORAL_BLOCK;
Block DEAD_BUBBLE_CORAL_BLOCK;
Block DEAD_FIRE_CORAL_BLOCK;
Block DEAD_HORN_CORAL_BLOCK;
Block TUBE_CORAL;
Block BRAIN_CORAL;
Block BUBBLE_CORAL;
Block FIRE_CORAL;
Block HORN_CORAL;
Block TUBE_CORAL_FAN;
Block BRAIN_CORAL_FAN;
Block BUBBLE_CORAL_FAN;
Block FIRE_CORAL_FAN;
Block HORN_CORAL_FAN;
Block SMALL_AMETHYST_BUD;
Block MEDIUM_AMETHYST_BUD;
Block LARGE_AMETHYST_BUD;
Block DRIPSTONE_BLOCK;
Block POINTED_DRIPSTONE;
Block GLOW_LICHEN;
Block SCULK;
Block SCULK_VEIN;
Block SCULK_CATALYST;
Block SCULK_SENSOR;
Block SCULK_SHRIEKER;
Block BIG_DRIPLEAF;
Block BIG_DRIPLEAF_STEM;
Block SMALL_DRIPLEAF_LOWER;
Block SMALL_DRIPLEAF_UPPER;
Block MUD;
Block DEAD_BUSH;
Block MYCELIUM;
Block RED_MUSHROOM;
Block BROWN_MUSHROOM;
Block MOSS_CARPET;
Block SWEET_BERRY_BUSH;
Block PUMPKIN;
Block LILY_PAD;
Block TALL_GRASS_BOTTOM;
Block SUGAR_CANE;
Block TALL_GRASS_TOP;
Block CRAFTING_TABLE;
Block FURNACE;
Block WHITE_CARPET;
Block GREEN_CARPET;
Block LIGHT_BLUE_CARPET;
Block LIGHT_GRAY_CARPET;
Block BOOKSHELF;
Block OAK_PRESSURE_PLATE;
Block OAK_STAIRS;
Block CHEST;
Block RED_CARPET;
Block ANVIL;
Block NOTE_BLOCK;
Block OAK_DOOR;
Block BREWING_STAND;
Block RED_BED_NORTH_HEAD;
Block RED_BED_NORTH_FOOT;
Block RED_BED_EAST_HEAD;
Block RED_BED_EAST_FOOT;
Block RED_BED_SOUTH_HEAD;
Block RED_BED_SOUTH_FOOT;
Block RED_BED_WEST_HEAD;
Block RED_BED_WEST_FOOT;
Block GRAY_STAINED_GLASS;
Block LIGHT_GRAY_STAINED_GLASS;
Block BROWN_STAINED_GLASS;
Block TINTED_GLASS;
Block OAK_TRAPDOOR;
Block BROWN_CONCRETE;
Block BLACK_TERRACOTTA;
Block BROWN_TERRACOTTA;
Block STONE_BRICK_STAIRS;
Block MUD_BRICK_STAIRS;
Block POLISHED_BLACKSTONE_BRICK_STAIRS;
Block BRICK_STAIRS;
Block POLISHED_GRANITE_STAIRS;
Block END_STONE_BRICK_STAIRS;
Block POLISHED_DIORITE_STAIRS;
Block SMOOTH_SANDSTONE_STAIRS;
Block QUARTZ_STAIRS;
Block POLISHED_ANDESITE_STAIRS;
Block NETHER_BRICK_STAIRS;

Block COBWEB;
Block CHISELLED_BOOKSHELF_NORTH; // Chiseled Bookshelf
Block CHISELLED_BOOKSHELF_EAST;	 // Chiseled Bookshelf East
Block CHISELLED_BOOKSHELF_SOUTH; // Chiseled Bookshelf South
Block CHISELLED_BOOKSHELF_WEST;	 // Chiseled Bookshelf West
Block DAMAGED_ANVIL;			 // Damaged Anvil

Block CHAIN;
Block END_ROD;
Block LIGHTNING_ROD;
Block GOLD_BLOCK;
Block SEA_LANTERN;
Block ORANGE_CONCRETE;
Block ORANGE_WOOL;
Block BLUE_WOOL;
Block GREEN_CONCRETE;
Block BRICK_WALL;
Block REDSTONE_BLOCK;
Block CHAIN_X;
Block CHAIN_Z;
Block SPRUCE_DOOR_LOWER;
Block SPRUCE_DOOR_UPPER;
Block SMOOTH_STONE_SLAB;
Block GLASS_PANE;
Block LIGHT_GRAY_TERRACOTTA;
Block OAK_SLAB_TOP;
Block OAK_DOOR_UPPER;
Block SPRUCE_LEAVES;
Block CYAN_STAINED_GLASS;
Block BLUE_STAINED_GLASS;
Block LIGHT_BLUE_STAINED_GLASS;
Block DAYLIGHT_DETECTOR;
Block RED_STAINED_GLASS;
Block YELLOW_STAINED_GLASS;
Block PURPLE_STAINED_GLASS;
Block ORANGE_STAINED_GLASS;
Block MAGENTA_STAINED_GLASS;
Block FLOWER_POT;
Block OAK_TRAPDOOR_OPEN_NORTH;
Block OAK_TRAPDOOR_OPEN_SOUTH;
Block OAK_TRAPDOOR_OPEN_EAST;
Block OAK_TRAPDOOR_OPEN_WEST;
Block QUARTZ_SLAB_TOP;
Block DARK_OAK_TRAPDOOR;
Block SPRUCE_TRAPDOOR;
Block BIRCH_TRAPDOOR;
Block MUD_BRICK_SLAB;
Block BRICK_SLAB;
Block POTTED_RED_TULIP;
Block POTTED_DANDELION;
Block POTTED_BLUE_ORCHID;
Block BARREL;
Block FERN;
Block CHIPPED_ANVIL;
Block LARGE_FERN_LOWER;
Block LARGE_FERN_UPPER;
Block LEVER;
Block COBBLESTONE_STAIRS;
Block WAXED_CUT_COPPER_STAIRS;
Block MOSSY_STONE_BRICK_STAIRS;
Block MOSSY_COBBLESTONE_STAIRS;
Block DEEPSLATE_BRICK_STAIRS;
Block POLISHED_DEEPSLATE_STAIRS;
Block RED_NETHER_BRICKS;
Block SPRUCE_STAIRS;
Block DARK_OAK_STAIRS;
Block RED_NETHER_BRICK_STAIRS;
Block WAXED_OXIDIZED_CUT_COPPER_STAIRS;
Block WAXED_OXIDIZED_COPPER;
Block ANDESITE_STAIRS;
Block WAXED_EXPOSED_CUT_COPPER_STAIRS;
Block WHITE_WALL_BANNER;
Block BLUE_WALL_BANNER;
Block BLACK_WALL_BANNER;
Block RED_WALL_BANNER;
Block GREEN_WALL_BANNER;
Block MOSSY_STONE_BRICKS;
Block DEEPSLATE;
Block TUFF;
Block COBBLED_DEEPSLATE;
Block COBBLED_DEEPSLATE_SLAB;
Block COBBLED_DEEPSLATE_STAIRS;
Block WATER_CAULDRON;
Block WAXED_COPPER_BLOCK;
Block WAXED_EXPOSED_COPPER;
Block WAXED_EXPOSED_CHISELED_COPPER;
Block WAXED_EXPOSED_CUT_COPPER;
Block WAXED_EXPOSED_CUT_COPPER_SLAB;
Block LODESTONE;
Block MANGROVE_LOG;
Block MANGROVE_LEAVES;
Block CHERRY_LOG;
Block CHERRY_LEAVES;
Block CHERRY_PLANKS;
Block CHERRY_SLAB;
Block CHERRY_STAIRS;
Block GRAY_CONCRETE_POWDER;
Block LIGHT_GRAY_CONCRETE_POWDER;
Block BROWN_CONCRETE_POWDER;
Block CYAN_TERRACOTTA;
Block BLACK_WOOL;
Block LIGHT_GRAY_WALL_BANNER;
Block CORNFLOWER;
Block OXEYE_DAISY;
Block ALLIUM;
Block LILY_OF_THE_VALLEY;
Block RED_TULIP;
Block ORANGE_TULIP;
Block WHITE_TULIP;
Block PINK_TULIP;
Block SUNFLOWER_LOWER;
Block SUNFLOWER_UPPER;
Block LILAC_LOWER;
Block LILAC_UPPER;
Block ROSE_BUSH_LOWER;
Block ROSE_BUSH_UPPER;
Block PEONY_LOWER;
Block PEONY_UPPER;

Block BLACK_STAINED_GLASS;
Block BIRCH_FENCE_GATE;
Block DARK_OAK_BUTTON;
Block DARK_OAK_FENCE_GATE;
Block DARK_OAK_PRESSURE_PLATE;
Block BLUE_STAINED_GLASS_PANE;
Block GRAY_STAINED_GLASS_PANE;
Block GRAY_WALL_BANNER;
Block IRON_TRAPDOOR;
Block JUNGLE_FENCE;
Block JUNGLE_TRAPDOOR;
Block MOSSY_COBBLESTONE_SLAB;
Block MOSSY_STONE_BRICK_SLAB;
Block MOSSY_STONE_BRICK_WALL;
Block OAK_BUTTON;
Block OAK_FENCE_GATE;
Block PALE_OAK_TRAPDOOR;
Block POLISHED_ANDESITE_SLAB;
Block POLISHED_BLACKSTONE_BUTTON;
Block POLISHED_BLACKSTONE_PRESSURE_PLATE;
Block POLISHED_DEEPSLATE_SLAB;
Block POLISHED_DEEPSLATE_WALL;
Block POLISHED_DIORITE_SLAB;
Block PURPUR_SLAB;
Block PURPUR_STAIRS;
Block QUARTZ_PILLAR;
Block REDSTONE_TORCH;
Block REDSTONE_WALL_TORCH;
Block RED_NETHER_BRICK_SLAB;
Block SANDSTONE_WALL;
Block SMOOTH_QUARTZ_SLAB;
Block SMOOTH_QUARTZ_STAIRS;
Block SMOOTH_RED_SANDSTONE_SLAB;
Block SPRUCE_BUTTON;
Block SPRUCE_FENCE_GATE;
Block SPRUCE_WALL_SIGN;
Block STONE_BUTTON;
Block STONE_PRESSURE_PLATE;
Block STONE_STAIRS;
Block TRIPWIRE_HOOK;
Block ACACIA_TRAPDOOR;
Block &CHISELLED_BOOKSHELF = CHISELLED_BOOKSHELF_NORTH;
Block GRANITE_STAIRS;
Block &SMOOTH_STONE_BLOCK = SMOOTH_STONE;
}

namespace
{
std::mutex block_mapping_mutex;
const void *mapped_node_def_manager = nullptr;
}

void init(MapgenEarth *mg)
{
	if (!mg || !mg->m_emerge || !mg->m_emerge->ndef)
		return;

	const auto *node_def_manager = mg->m_emerge->ndef;
	const std::lock_guard<std::mutex> lock(block_mapping_mutex);
	if (mapped_node_def_manager == node_def_manager)
		return;

	const auto usable_solid = [&](content_t id) {
		return id != CONTENT_IGNORE && id != CONTENT_AIR && id != CONTENT_UNKNOWN &&
			   !node_def_manager->get(id).name.empty() &&
			   !node_def_manager->get(id).isLiquid();
	};
	content_t fallback_solid = CONTENT_IGNORE;
	for (const auto *name : {"mapgen_stone", "basenodes:stone", "mcl_core:stone",
				 "default:stone", "mapgen_dirt", "basenodes:dirt", "mcl_core:dirt",
				 "default:dirt", "default:cobble", "mcl_core:cobble"}) {
		const auto id = node_def_manager->getId(name);
		if (usable_solid(id)) {
			fallback_solid = id;
			break;
		}
	}
	// Some game/content packs have no conventional default:/mcl_core: terrain
	// names. Choose a registered solid ground node from that pack as the final
	// mapping fallback, rather than allowing unresolved blocks to become IGNORE.
	if (fallback_solid == CONTENT_IGNORE) {
		for (u32 raw_id = 0; raw_id < node_def_manager->size(); ++raw_id) {
			const auto id = static_cast<content_t>(raw_id);
			const auto &feature = node_def_manager->get(id);
			if (usable_solid(id) && feature.is_ground_content) {
				fallback_solid = id;
				if (feature.name.find("stone") != std::string::npos ||
						feature.name.find("rock") != std::string::npos)
					break;
			}
		}
	}
	if (fallback_solid == CONTENT_IGNORE) {
		for (u32 raw_id = 0; raw_id < node_def_manager->size(); ++raw_id) {
			const auto id = static_cast<content_t>(raw_id);
			if (usable_solid(id)) {
				fallback_solid = id;
				break;
			}
		}
	}
	const auto default_cobble = node_def_manager->getId("default:cobble");
	const auto def = usable_solid(default_cobble) ? default_cobble : fallback_solid;
	// Node definitions can be queried before a game's registrations are fully
	// populated. Do not cache an empty/incomplete mapping for this manager; the
	// next generation request will retry after the nodes are available.
	if (!usable_solid(def))
		return;
	// Packs often expose an equivalent node under a namespace the Arnis palette
	// does not know (for example, a game-specific `*:dirt` or `*:tube_coral`).
	// Build this index once per NodeDefManager so all mappings can try those
	// registered alternatives without rescanning every node for every block.
	std::unordered_map<std::string, std::vector<content_t>> nodes_by_suffix;
	for (u32 raw_id = 0; raw_id < node_def_manager->size(); ++raw_id) {
		const auto id = static_cast<content_t>(raw_id);
		const auto &name = node_def_manager->get(id).name;
		const auto separator = name.find(':');
		if (separator != std::string::npos && separator + 1 < name.size())
			nodes_by_suffix[name.substr(separator + 1)].push_back(id);
	}

	struct NodeResolver
	{
		MapgenEarth *mg;
		content_t fallback;
		bool allow_liquids = false;
		const std::unordered_map<std::string, std::vector<content_t>> *nodes_by_suffix =
				nullptr;

		bool usable(content_t id) const
		{
			// Freeminer also models loose building materials as liquids.
			// They cannot support walls or structures, so try a solid alternative.
			if (id == CONTENT_IGNORE || id == CONTENT_AIR || id == CONTENT_UNKNOWN)
				return false;
			const auto &feature = mg->m_emerge->ndef->get(id);
			return !feature.name.empty() &&
				   (allow_liquids || !mg->m_emerge->ndef->get(id).isLiquid());
		}

		content_t arnis_node(const char *name) const
		{
			// Native Arnis names use the canonical (namespace-free) block name.
			// Accept names passed either as "stone" or "default:stone" so every
			// resolver path observes the same Arnis-first precedence.
			const std::string full_name(name);
			const auto separator = full_name.find(':');
			const auto canonical_name = separator == std::string::npos
												? full_name
												: full_name.substr(separator + 1);
			std::string candidate = "arnis:" + canonical_name;
			std::transform(candidate.begin() + 6, candidate.end(), candidate.begin() + 6,
					[](unsigned char c) { return static_cast<char>(std::tolower(c)); });
			return mg->m_emerge->ndef->getId(candidate);
		}

		content_t same_name_in_any_namespace(const char *name) const
		{
			if (!nodes_by_suffix || !name)
				return CONTENT_IGNORE;
			const std::string full_name(name);
			const auto separator = full_name.find(':');
			const auto suffix = separator == std::string::npos
										? full_name
										: full_name.substr(separator + 1);
			const auto found = nodes_by_suffix->find(suffix);
			if (found == nodes_by_suffix->end())
				return CONTENT_IGNORE;
			for (const auto id : found->second)
				if (usable(id))
					return id;
			return CONTENT_IGNORE;
		}

		content_t find(const char *name) const
		{
			// Keep every single-node lookup on the same precedence path: native
			// Arnis block first, then the requested game-specific node name.
			const auto arnis_id = arnis_node(name);
			if (usable(arnis_id))
				return arnis_id;

			const auto id = mg->m_emerge->ndef->getId(name);
			if (usable(id))
				return id;
			return same_name_in_any_namespace(name);
		}

		content_t operator()(const char *name) const
		{
			const auto id = find(name);
			if (id == CONTENT_IGNORE) {
				actionstream << "Mapping node missing or liquid " << name << "\n";
				return fallback;
			}
			return id;
		}

		content_t operator()(std::initializer_list<const char *> names) const
		{
			// Resolve every canonical candidate in the Arnis namespace first.
			// Only after exhausting those names, try game-specific aliases. This
			// keeps Arnis blocks authoritative without losing compatibility with
			// Luanti/Mineclonia content packs.
			for (const auto *name : names) {
				const auto id = arnis_node(name);
				if (usable(id))
					return id;
			}
			for (const auto *name : names) {
				const auto id = mg->m_emerge->ndef->getId(name);
				if (usable(id))
					return id;
			}
			for (const auto *name : names) {
				const auto id = same_name_in_any_namespace(name);
				if (id != CONTENT_IGNORE)
					return id;
			}
			// Content packs commonly expose the same terrain under different
			// namespaces. After exhausting material-specific aliases, use a
			// generic solid fallback rather than resolving to CONTENT_IGNORE.
			// Do not apply this to liquid mappings (water/lava).
			if (!allow_liquids) {
				for (const auto *name : {"mapgen_stone", "basenodes:stone",
							 "mcl_core:stone", "default:stone", "mapgen_desert_stone",
							 "basenodes:desert_stone", "mcl_core:desert_stone",
							 "default:desert_stone", "mapgen_dirt", "basenodes:dirt",
							 "mcl_core:dirt", "default:dirt", "dirt",
							 "basenodes:dirt_with_grass", "mcl_core:dirt_with_grass",
							 "default:dirt_with_grass", "mapgen_dirt_with_grass",
							 "mapgen_dirt_with_snow", "mcl_core:coarse_dirt",
							 "default:dry_dirt", "mapgen_sand", "basenodes:sand",
							 "mcl_core:sand", "default:sand", "basenodes:desert_sand",
							 "mapgen_desert_sand", "desert_sand", "sand",
							 "mcl_core:red_sand", "mcl_core:redsand", "default:red_sand",
							 "default:redsand", "default:desert_sand", "mapgen_gravel",
							 "basenodes:gravel", "mcl_core:gravel", "default:gravel",
							 "gravel", "basenodes:cobble", "mcl_core:cobble",
							 "default:cobble"}) {
					const auto id = mg->m_emerge->ndef->getId(name);
					if (usable(id))
						return id;
					// Also recognize equivalent terrain nodes in namespaces not
					// explicitly listed above (e.g. another game's `*:stone`).
					const auto namespaced = same_name_in_any_namespace(name);
					if (namespaced != CONTENT_IGNORE)
						return namespaced;
				}
				if (fallback != CONTENT_IGNORE && usable(fallback))
					return fallback;
			}
			// Optional blocks are expected to be absent in many content packs.
			// At this point all named aliases and generic terrain alternatives were
			// tried; use the validated solid fallback rather than emitting an ERROR
			// for every unsupported palette entry. init() refuses to install this
			// resolver unless its global fallback is a registered solid node.
			return fallback;
		}

		content_t operator()(const char *block_name, const char *name) const
		{
			return (*this)(block_name, {name});
		}

		content_t operator()(
				const char *block_name, std::initializer_list<const char *> names) const
		{
			const auto id = arnis_node(block_name);
			if (usable(id))
				return id;
			return (*this)(names);
		}
	};
	const NodeResolver g{mg, def, false, &nodes_by_suffix};
	const NodeResolver liquid{mg, def, true, &nodes_by_suffix};
	const auto optional_g = [&](const char *block_name, Block fallback,
									std::initializer_list<const char *> names) {
		NodeResolver resolver{mg, fallback.id(), false, &nodes_by_suffix};
		if (const auto id = resolver.find(block_name); id != CONTENT_IGNORE)
			return Block{id};
		for (const auto *name : names)
			if (const auto id = resolver.find(name); id != CONTENT_IGNORE)
				return Block{id};
		return fallback;
	};
	const auto prefer_arnis = [&](const char *name, Block fallback) {
		const NodeResolver resolver{mg, fallback.id(), false, &nodes_by_suffix};
		const auto id = resolver.arnis_node(name);
		return Block{resolver.usable(id) ? id : fallback.id()};
	};
	const auto available_node = [&](const char *name) {
		const NodeResolver resolver{mg, CONTENT_AIR, false, &nodes_by_suffix};
		return resolver.find(name) != CONTENT_IGNORE;
	};
	const auto flower = [&](const char *block_name,
								std::initializer_list<const char *> names,
								Block fallback) {
		return Block{NodeResolver{mg, fallback.id(), false, &nodes_by_suffix}(
				block_name, names)};
	};

	ACACIA_PLANKS = g("acacia_planks", {"default:acacia_wood", "default:wood"});
	AIR = CONTENT_AIR;
	ANDESITE = g("andesite", "default:stone");
	BIRCH_LEAVES = g("birch_leaves", {"default:aspen_leaves", "default:leaves"});
	BIRCH_LOG = g("birch_log", {"default:aspen_tree", "default:tree"});
	BLACK_CONCRETE = g("black_concrete",
			{"wool:black", "basic_materials:concrete_block", "default:stone"});
	BLACKSTONE = g("blackstone", "default:obsidian");
	BLUE_FLOWER = g("blue_flower", {"flowers:geranium", "mcl_flowers:blue_orchid",
										   "mcl_flowers:azure_bluet", "default:grass_3"});
	RED_FLOWER =
			g("red_flower", {"flowers:tulip", "mcl_flowers:poppy", "default:grass_3"});
	WHITE_FLOWER =
			g("white_flower", {"flowers:dandelion_white", "mcl_flowers:dandelion",
									  "mcl_flowers:azure_bluet", "default:grass_3"});
	YELLOW_FLOWER = g("yellow_flower",
			{"flowers:dandelion_yellow", "mcl_flowers:dandelion", "default:grass_3"});
	CORNFLOWER = flower("cornflower",
			{"mcl_flowers:cornflower", "flowers:cornflower", "mcl_flowers:blue_orchid",
					"flowers:geranium", "default:grass_3"},
			BLUE_FLOWER);
	OXEYE_DAISY = flower("oxeye_daisy",
			{"mcl_flowers:oxeye_daisy", "flowers:oxeye_daisy", "mcl_flowers:azure_bluet",
					"flowers:dandelion_white", "default:grass_3"},
			WHITE_FLOWER);
	ALLIUM = flower("allium",
			{"mcl_flowers:allium", "flowers:allium", "mcl_flowers:poppy", "flowers:tulip",
					"default:grass_3"},
			RED_FLOWER);
	LILY_OF_THE_VALLEY = flower("lily_of_the_valley",
			{"mcl_flowers:lily_of_the_valley", "flowers:lily_of_the_valley",
					"mcl_flowers:azure_bluet", "default:grass_3"},
			WHITE_FLOWER);
	RED_TULIP = flower("red_tulip",
			{"mcl_flowers:tulip_red", "flowers:tulip_red", "flowers:tulip",
					"default:grass_3"},
			RED_FLOWER);
	ORANGE_TULIP = flower("orange_tulip",
			{"mcl_flowers:tulip_orange", "flowers:tulip_orange", "flowers:tulip",
					"default:grass_3"},
			YELLOW_FLOWER);
	WHITE_TULIP = flower("white_tulip",
			{"mcl_flowers:tulip_white", "flowers:tulip_white", "flowers:tulip",
					"default:grass_3"},
			WHITE_FLOWER);
	PINK_TULIP = flower("pink_tulip",
			{"mcl_flowers:tulip_pink", "flowers:tulip_pink", "flowers:tulip",
					"default:grass_3"},
			RED_FLOWER);
	BLUE_TERRACOTTA = g("blue_terracotta", "default:clay");
	BRICK = g("brick", "default:brick");
	CAULDRON = g("cauldron", "default:steelblock");
	CHISELED_STONE_BRICKS = g("chiseled_stone_bricks", "default:stonebrick");
	COBBLESTONE_WALL = g("cobblestone_wall", "default:cobble");
	COBBLESTONE = g("cobblestone", "default:cobble");
	POLISHED_BLACKSTONE_BRICKS = g("polished_blackstone_bricks", "default:obsidianbrick");
	CRACKED_STONE_BRICKS = g("cracked_stone_bricks", "default:stonebrick");
	CRIMSON_PLANKS = g("crimson_planks", "default:wood");
	CUT_SANDSTONE = g("cut_sandstone", "default:sandstone");
	CYAN_CONCRETE = g("cyan_concrete",
			{"wool:cyan", "basic_materials:concrete_block", "default:stone"});
	DARK_OAK_PLANKS = g("dark_oak_planks", {"default:wood", "default:junglewood"});
	DARK_OAK_SLAB = g("dark_oak_slab", {"mcl_stairs:slab_dark_oak", "stairs:slab_wood"});
	DARK_OAK_FENCE =
			g("dark_oak_fence", {"mcl_fences:dark_oak_fence", "default:fence_wood"});
	DEEPSLATE_BRICKS = g("deepslate_bricks", "default:stonebrick");
	DIORITE = g("diorite", "default:stone");
	DIRT = g("dirt",
			{"default:dirt", "dirt", "mcl_core:dirt", "mcl_core:dirt_with_grass",
					"mcl_core:coarse_dirt", "mcl_core:rooted_dirt", "mcl_mud:mud",
					"mcl_core:dirt_with_grass_snow", "mcl_core:podzol",
					"mcl_core:mycelium", "mcl_core:packed_mud", "mcl_nether:soul_soil",
					"mcl_core:dirt_with_snow", "mcl_core:dirt_with_coniferous_litter",
					"mcl_core:dirt_with_rainforest_litter",
					"mcl_mud:muddy_mangrove_roots", "mcl_mud:packed_mud",
					"basenodes:dirt", "spring:dirt", "default:dirt_with_grass",
					"basenodes:dirt_with_grass", "mcl_core:coarse_dirt",
					"mcl_core:rooted_dirt", "mcl_core:dirt_with_snow",
					"mcl_core:dirt_with_grass_snow", "mcl_farming:soil", "mcl_mud:mud",
					"mcl_farming:soil_wet", "mcl_farming:soil_sandy", "mapgen:dirt",
					"mapgen:soil", "mapgen:dirt_with_grass", "default:dirt_with_snow",
					"default:dry_dirt", "default:dirt_with_dry_grass",
					"default:dry_dirt_with_dry_grass", "ethereal:green_dirt",
					"ethereal:dry_dirt", "farming:soil", "mapgen_dirt_with_grass",
					"mapgen_dirt", "default:clay", "mcl_core:clay", "mcl_mud:mud",
					"default:stone", "basenodes:stone", "mcl_core:stone"});
	END_STONE_BRICKS = g("end_stone_bricks", "default:stonebrick");
	END_STONE = g("end_stone",
			{"default:endstone", "default:end_stone", "endstone", "end_stone",
					"mcl_end:end_stone", "mcl_end:end_stone_block", "mcl_end:stone_end",
					"mcl_end:end_stone_brick", "mcl_end:endstone_block",
					"mcl_end:endstone", "mcl_end:stone", "mcl_core:end_stone",
					"mcl_core:endstone", "mapgen:end_stone", "mapgen:endstone",
					"mcl_core:end_stone_bricks", "mcl_end:end_stone_bricks",
					"mcl_end:endstone_bricks", "mcl_end:purpur", "mcl_end:purpur_block",
					"mcl_core:purpur_block", "mapgen:stone", "mapgen_stone",
					"mcl_core:obsidian", "default:obsidian", "default:desert_stone",
					"default:sandstone", "mcl_core:sandstone", "mapgen_stone",
					"basenodes:stone", "mcl_core:stone", "default:stone"});
	FARMLAND = g("farmland",
			{"default:dirt", "mcl_farming:soil", "mcl_core:dirt", "farming:soil",
					"mcl_core:dirt_with_grass", "basenodes:dirt", "spring:dirt",
					"mapgen_dirt", "default:dirt_with_grass", "default:clay",
					"basenodes:stone", "default:stone"});
	GLASS = g("glass", "default:glass");
	GLOWSTONE = g("glowstone", "default:meselamp");
	GRANITE = g("granite", "default:stone");
	GRASS_BLOCK = g("grass_block", "default:grass_5");
	GRASS = g("grass", "default:grass_3");
	SNOWY_GRASS_BLOCK = g("snowy_grass_block",
			{"mcl_core:dirt_with_grass_snow", "mapgen_dirt_with_snow",
					"basenodes:dirt_with_snow", "default:dirt_with_snow",
					"default:grass_5"});
	GRAVEL = g(
			"gravel", {"default:gravel", "gravel", "mcl_core:gravel", "basenodes:gravel",
							  "mapgen_gravel", "mcl_core:stone_gravel", "mapgen:gravel",
							  "mcl_core:gravelstone", "mcl_core:stone_with_gravel",
							  "default:gravelstone", "default:stone_with_gravel",
							  "mcl_core:gravel_block", "mapgen:stone", "mapgen_stone",
							  "ethereal:gravel", "default:cobble", "mcl_core:cobble",
							  "basenodes:stone", "default:stone", "mcl_core:stone"});
	GRAY_CONCRETE = g("gray_concrete",
			{"basic_materials:concrete_block", "wool:grey", "default:stone"});
	GRAY_TERRACOTTA = g("gray_terracotta", "default:clay");
	GREEN_STAINED_HARDENED_CLAY = g("green_stained_hardened_clay", "default:clay");
	GREEN_WOOL = g("green_wool", "wool:green");
	HAY_BALE = g("hay_bale", "farming:straw");
	IRON_BARS = g("iron_bars",
			{"xpanes:bar_flat", "default:steelblock", "default:stone_with_iron"});
	IRON_BLOCK = g("iron_block", "default:steelblock");
	JUNGLE_PLANKS = g("jungle_planks", {"default:junglewood", "default:wood"});
	LADDER = g("ladder", "default:ladder_wood");
	LIGHT_BLUE_CONCRETE = g("light_blue_concrete",
			{"wool:cyan", "wool:blue", "basic_materials:concrete_block"});
	LIGHT_BLUE_TERRACOTTA = g("light_blue_terracotta", "default:clay");
	LIGHT_GRAY_CONCRETE = g("light_gray_concrete",
			{"basic_materials:concrete_block", "wool:grey", "default:stone"});
	MOSS_BLOCK = g("moss_block", "default:mossycobble");
	MOSSY_COBBLESTONE = g("mossy_cobblestone", "default:mossycobble");
	MUD_BRICKS = g("mud_bricks", "default:silver_sandstone_brick");
	NETHER_BRICK = g("nether_brick", "default:obsidianbrick");
	NETHERITE_BLOCK = g("netherite_block", "default:obsidian");
	OAK_FENCE = g("oak_fence", "default:fence_wood");
	OAK_LEAVES = g("oak_leaves", "default:leaves");
	OAK_LOG = g("oak_log", "default:tree");
	OAK_PLANKS = g("oak_planks", "default:wood");
	OAK_SLAB = g("oak_slab", "stairs:slab_wood");
	SPRUCE_SLAB = g("spruce_slab", {"mcl_stairs:slab_spruce", "stairs:slab_wood"});
	SPRUCE_FENCE = g("spruce_fence",
			{"mcl_fences:spruce_fence", "default:fence_pine_wood", "default:fence_wood"});
	ORANGE_TERRACOTTA = g("orange_terracotta", "default:clay");
	PODZOL = g("podzol", "default:dirt_with_coniferous_litter");
	SNOWY_PODZOL = g("snowy_podzol",
			{"mcl_core:podzol", "default:dirt_with_coniferous_litter", "default:dirt"});
	POLISHED_ANDESITE = g("polished_andesite", "default:stone");
	POLISHED_BASALT = g("polished_basalt", "default:stone");
	QUARTZ_BLOCK = g("quartz_block", "default:stone");
	POLISHED_BLACKSTONE = g("polished_blackstone", {"default:obsidian", "default:stone"});
	POLISHED_DEEPSLATE = g("polished_deepslate", "default:stone");
	POLISHED_DIORITE = g("polished_diorite", "default:stone");
	POLISHED_GRANITE = g("polished_granite", "default:stone");
	PRISMARINE = g("prismarine", "default:stone");
	PURPUR_BLOCK = g("purpur_block", "default:stone");
	PURPUR_PILLAR = g("purpur_pillar",
			{"mcl_end:purpur_pillar", "mcl_nether:purpur_pillar", "default:stone"});
	QUARTZ_BRICKS = g("quartz_bricks", "default:stone");
	// default:rail is only a Lua alias in Minetest Game. NodeDefManager lookups do
	// not reliably resolve item aliases, so prefer the actually registered node.
	RAIL = g("rail", {"carts:rail", "default:rail"});
	RED_NETHER_BRICK = g("red_nether_brick", "default:obsidianbrick");
	RED_TERRACOTTA = g("red_terracotta", "default:clay");
	RED_WOOL = g("red_wool", "wool:red");
	SAND = g("sand", {"default:sand", "mcl_core:sand", "basenodes:sand", "mapgen_sand",
							 "spring:sand", "mcl_core:red_sand", "mcl_core:redsand",
							 "default:red_sand", "default:redsand", "default:desert_sand",
							 "mcl_core:desert_sand", "basenodes:desert_sand",
							 "mapgen_desert_sand", "default:silver_sand", "ethereal:sand",
							 "default:silver_sand", "mcl_core:sandstone",
							 "default:sandstone", "basenodes:stone", "default:stone"});
	SANDSTONE = g("sandstone", "default:sandstone");
	SCAFFOLDING = g("scaffolding", "default:ladder_steel");
	SMOOTH_QUARTZ = g("smooth_quartz", "default:stone");
	SMOOTH_RED_SANDSTONE = g("smooth_red_sandstone", "default:sandstone");
	SMOOTH_SANDSTONE = g("smooth_sandstone", "default:sandstone");
	SMOOTH_STONE = g("smooth_stone", "default:stone");
	SPONGE = g("sponge", "sponge:sponge");
	SPRUCE_LOG = g("spruce_log", {"default:pine_tree", "default:tree"});
	SPRUCE_PLANKS = g("spruce_planks", {"default:pine_wood", "default:wood"});
	STONE_BLOCK_SLAB = g("stone_block_slab",
			{"stairs:slab_stone_block", "default:stone_block", "default:stone"});
	STONE_BRICK_SLAB =
			g("stone_brick_slab", {"stairs:slab_stonebrick", "default:stonebrick"});
	STONE_BRICKS = g("stone_bricks", "default:stonebrick");
	STONE = g("stone", "default:stone");
	TERRACOTTA = g("terracotta", "default:clay");
	WARPED_PLANKS = g("warped_planks", "default:wood");
	WARPED_STAIRS =
			g("warped_stairs", {"mcl_stairs:stair_warped_planks",
									   "stairs:stair_warped_planks", "default:wood"});
	WARPED_TRAPDOOR = g("warped_trapdoor",
			{"mcl_doors:trapdoor_warped", "doors:trapdoor", "default:wood"});
	WARPED_SLAB = g(
			"warped_slab", {"mcl_stairs:slab_warped_planks", "stairs:slab_warped_planks",
								   "stairs:slab_wood", "default:wood"});
	STRIPPED_WARPED_STEM = g(
			"stripped_warped_stem", {"mcl_crimson:stripped_warped_stem", "default:tree"});
	STRIPPED_WARPED_HYPHAE = g("stripped_warped_hyphae",
			{"mcl_crimson:stripped_warped_hyphae", "default:tree"});
	WATER = liquid("water", "default:water_source");
	// Rust parity: water_depth.rs uses MineClone ocean/nether blocks.
	// Divergence: Earth game maps them to available Minetest Game nodes.
	SEAGRASS = g("seagrass", {"marinara:seagrass", "marinara:sand_with_seagrass",
									 "default:marram_grass_1", "default:grass_3"});
	KELP_PLANT =
			g("kelp_plant", {"marinara:sand_with_kelp", "default:sand_with_kelp",
									"default:marram_grass_3", "default:marram_grass_1"});
	MAGMA_BLOCK =
			g("magma_block", {"mcl_nether:magma", "default:obsidian", "default:stone"});
	OBSIDIAN = g("obsidian", {"mcl_core:obsidian", "default:obsidian", "default:stone"});
	KELP = prefer_arnis("kelp", KELP_PLANT);
	TALL_SEAGRASS_BOTTOM = g("tall_seagrass_bottom",
			{"marinara:sand_with_seagrass2", "default:marram_grass_2",
					"default:marram_grass_1"});
	TALL_SEAGRASS_TOP = g("tall_seagrass_top",
			{"marinara:seagrass2", "default:marram_grass_3", "default:marram_grass_1"});
	SEA_PICKLE =
			g("sea_pickle", {"marinara:seaanemone_tentacle", "marinara:hardcoral_green",
									"default:coral_green", "default:coral_cyan",
									"default:coral_skeleton"});
	SOUL_SAND = g("soul_sand",
			{"default:desert_sand", "mcl_core:desert_sand", "mcl_nether:soul_sand",
					"mcl_core:redsand", "mcl_core:red_sand", "default:redsand",
					"default:red_sand", "default:silver_sand", "ethereal:desert_sand",
					"mapgen_desert_sand", "basenodes:desert_sand", "default:sand",
					"mcl_core:sand", "mapgen_sand", "basenodes:sand", "spring:sand",
					"default:desert_sandstone", "default:sandstone", "basenodes:stone",
					"default:stone", "mcl_core:stone"});
	EARTH_BENCH = g("earth_bench",
			{"homedecor:simple_bench", "stairs:slab_wood", "default:wood"});
	EARTH_TRASH_CAN = g("earth_trash_can",
			{"homedecor:trash_can", "pipeworks:trashcan", "default:steelblock"});
	EARTH_STREET_LAMP = g("earth_street_lamp",
			{"streets:light_vertical_on", "morelights_vintage:lantern_f",
					"homedecor:ground_lantern_14", "default:meselamp"});
	EARTH_WELL = g("earth_well", {"homedecor:well", "default:stonebrick"});
	EARTH_BARBECUE = g(
			"earth_barbecue", {"homedecor:barbecue", "default:furnace", "default:stone"});
	EARTH_GRATING = g("earth_grating",
			{"pipeworks:grating", "xpanes:bar_flat", "default:steelblock"});
	EARTH_FENCE_CHAINLINK = g("earth_fence_chainlink",
			{"streets:fence_chainlink", "homedecor:fence_chainlink", "xpanes:bar_flat",
					"default:steelblock"});
	EARTH_FENCE_BARBED = g("earth_fence_barbed",
			{"homedecor:fence_barbed_wire", "xpanes:bar_flat", "default:steelblock"});
	EARTH_FENCE_PICKET = g("earth_fence_picket",
			{"homedecor:fence_picket", "default:fence_wood", "default:wood"});
	EARTH_FENCE_WROUGHT = g("earth_fence_wrought",
			{"homedecor:fence_wrought_iron_2", "xpanes:bar_flat", "default:steelblock"});
	WHITE_CONCRETE = g("white_concrete",
			{"wool:white", "basic_materials:concrete_block", "default:stone"});
	WHITE_STAINED_GLASS = prefer_arnis("white_stained_glass", GLASS);
	WHITE_TERRACOTTA = g("white_terracotta", "default:clay");
	WHITE_WOOL = g("white_wool", "wool:white");
	YELLOW_CONCRETE = g("yellow_concrete",
			{"wool:yellow", "basic_materials:concrete_block", "default:stone"});
	YELLOW_WOOL = g("yellow_wool", "wool:yellow");
	LIME_CONCRETE = g("lime_concrete",
			{"wool:green", "wool:yellow", "basic_materials:concrete_block"});
	CYAN_WOOL = g("cyan_wool", "wool:cyan");
	GRAY_WOOL = g("gray_wool",
			{"wool:grey", "wool:gray", "wool:light_grey", "wool:light_gray"});
	BLUE_CONCRETE = g("blue_concrete",
			{"wool:blue", "basic_materials:concrete_block", "default:stone"});
	PURPLE_CONCRETE = g("purple_concrete",
			{"wool:violet", "basic_materials:concrete_block", "default:stone"});
	RED_CONCRETE = g("red_concrete",
			{"wool:red", "basic_materials:concrete_block", "default:stone"});
	MAGENTA_CONCRETE = g("magenta_concrete",
			{"wool:magenta", "wool:violet", "basic_materials:concrete_block"});
	BROWN_WOOL = g("brown_wool", "wool:brown");
	OXIDIZED_COPPER = g("oxidized_copper", {"default:copperblock", "default:stone"});
	YELLOW_TERRACOTTA = g("yellow_terracotta", "default:clay");
	SNOW_BLOCK = g("snow_block", "default:snow");
	SNOW_LAYER = g("snow_layer", "default:snow");
	SIGN = g("sign", {"default:sign_wall_wood", "default:sign_wall", "default:wood"});
	STEEL_SIGN = g("steel_sign",
			{"default:sign_wall_steel", "default:sign_wall_wood", "default:steelblock"});
	TEXT_SIGN_SMALL = g("text_sign_small",
			{"street_signs:sign_highway_small_green", "default:sign_wall_steel",
					"default:sign_wall_wood", "default:steelblock"});
	TEXT_SIGN_MEDIUM = g("text_sign_medium",
			{"street_signs:sign_highway_medium_green", "default:sign_wall_steel",
					"default:sign_wall_wood", "default:steelblock"});
	TEXT_SIGN_LARGE = g("text_sign_large",
			{"street_signs:sign_highway_large_green", "default:sign_wall_steel",
					"default:sign_wall_wood", "default:steelblock"});
	auto optional_sign = [&](const char *name) {
		const NodeResolver resolver{mg, CONTENT_AIR, false, &nodes_by_suffix};
		const auto id = resolver.find(name);
		return Block{static_cast<content_t>(id == CONTENT_IGNORE ? CONTENT_AIR : id)};
	};
	STREETS_AVAILABLE = available_node("streets:asphalt");
	ROAD_ASPHALT = g("road_asphalt", {"streets:asphalt", "basic_materials:concrete_block",
											 "wool:grey", "default:stone"});
	ROAD_SIDEWALK = g("road_sidewalk",
			{"streets:sidewalk", "default:stone_block", "default:stone"});
	STREETS_POLE =
			g("streets_pole", {"streets:bigpole", "walls:cobble", "default:stone"});
	STREETS_BOLLARD = g("streets_bollard",
			{"streets:bollard_steel_manual_up", "walls:cobble", "default:cobble"});
	STREETS_GUARDRAIL = g(
			"streets_guardrail", {"streets:guardrail", "walls:cobble", "default:cobble"});
	STREETS_TRAFFIC_LIGHTS = {optional_sign("streets:trafficlight_top_red"),
			optional_sign("streets:trafficlight_top_yellow"),
			optional_sign("streets:trafficlight_top_green")};
	STREETS_MARKINGS_AVAILABLE =
			STREETS_AVAILABLE &&
			available_node("streets:mark_dashed_white_center_line_on_asphalt");
	ROAD_MARK_DASHED_WHITE =
			optional_sign("streets:mark_dashed_white_center_line_on_asphalt");
	ROAD_MARK_DASHED_WHITE_R90 =
			optional_sign("streets:mark_dashed_white_center_line_r90_on_asphalt");
	ROAD_MARK_SOLID_WHITE_STRIPE =
			optional_sign("streets:mark_solid_white_stripe_on_asphalt");
	ROAD_MARK_SOLID_WHITE_STRIPE_R90 =
			optional_sign("streets:mark_solid_white_stripe_r90_on_asphalt");
	STREETS_RRXING_AVAILABLE = available_node("streets:rrxing_bottom") &&
							   available_node("streets:rrxing_middle_center_off") &&
							   available_node("streets:rrxing_top");
	STREETS_RRXING_BOTTOM = optional_sign("streets:rrxing_bottom");
	STREETS_RRXING_MIDDLE = optional_sign("streets:rrxing_middle_center_off");
	STREETS_RRXING_TOP = optional_sign("streets:rrxing_top");
	STREETS_EU_SIGN_STOP = optional_sign("streets:sign_eu_stop_center");
	STREETS_EU_SIGN_YIELD = optional_sign("streets:sign_eu_yield_center");
	STREETS_EU_SIGN_NO_ENTRY = optional_sign("streets:sign_eu_noentry_center");
	STREETS_EU_SIGN_CROSSING = optional_sign("streets:sign_eu_pedestriancrossing_center");
	STREETS_EU_SIGN_CROSSBUCK = optional_sign("streets:sign_eu_standrews_center");
	const std::array<const char *, 6> eu_speeds{{"10", "30", "50", "70", "100", "120"}};
	for (std::size_t i = 0; i < eu_speeds.size(); ++i) {
		const std::string name =
				"streets:sign_eu_" + std::string(eu_speeds[i]) + "_center";
		STREETS_EU_SPEED_SIGNS[i] = optional_sign(name.c_str());
	}
	STREETS_US_SIGN_STOP = optional_sign("streets:sign_us_stop_center");
	STREETS_US_SIGN_YIELD = optional_sign("streets:sign_us_yield_center");
	STREETS_US_SIGN_NO_ENTRY = optional_sign("streets:sign_us_donotenter_center");
	STREETS_US_SIGN_CROSSING = optional_sign("streets:sign_us_pedwarning_center");
	STREETS_US_SIGN_CROSSBUCK = optional_sign("streets:sign_us_crossbuck_center");
	// signs_lib creates the *_onpole variants while registering street_signs.
	// Keep these optional: Arnis uses its generated decal signs in games which do
	// not load street_signs, but should prefer the native meshes when they exist.
	STREET_SIGNS_AVAILABLE = available_node("street_signs:sign_basic") &&
							 available_node("street_signs:sign_stop_onpole");
	STREET_SIGN_BASIC = optional_sign("street_signs:sign_basic");
	STREET_SIGN_STOP = optional_sign("street_signs:sign_stop_onpole");
	STREET_SIGN_YIELD = optional_sign("street_signs:sign_yield_onpole");
	STREET_SIGN_SPEED_LIMIT = optional_sign("street_signs:sign_speed_limit_onpole");
	STREET_SIGN_DO_NOT_ENTER = optional_sign("street_signs:sign_do_not_enter_onpole");
	STREET_SIGN_PEDESTRIAN_CROSSING =
			optional_sign("street_signs:sign_pedestrian_crossing_onpole");
	STREET_SIGN_RR_CROSSBUCK =
			optional_sign("street_signs:sign_rr_grade_crossbuck_onpole");
	STREET_SIGN_US_ROUTE = optional_sign("street_signs:sign_us_route_onpole");
	STREET_SIGN_US_INTERSTATE = optional_sign("street_signs:sign_us_interstate_onpole");
	DECAL_FRAME = g("decal_frame", {"freeminer:arnis_decal_frame", "air"});
	ANDESITE_WALL = g("andesite_wall", {"walls:cobble", "default:stone"});
	STONE_BRICK_WALL = g("stone_brick_wall", "default:stonebrick");
	CARROTS = g("carrots", {"farming:carrot_4", "mcl_farming:carrot_7",
								   "mcl_farming:carrot_4", "default:grass_5"});
	DARK_OAK_DOOR_LOWER = g("dark_oak_door_lower", "doors:door_wood_a");
	DARK_OAK_DOOR_UPPER = g("dark_oak_door_upper", "doors:door_wood_b");
	DARK_OAK_LOG = g("dark_oak_log", {"default:tree", "default:jungletree"});
	DARK_OAK_LEAVES = g("dark_oak_leaves", {"default:leaves", "default:jungleleaves"});
	JUNGLE_LOG = g("jungle_log", {"default:jungletree", "default:tree"});
	JUNGLE_LEAVES = g("jungle_leaves", {"default:jungleleaves", "default:leaves"});
	ACACIA_LOG = g("acacia_log", {"default:acacia_tree", "default:tree"});
	ACACIA_LEAVES = g("acacia_leaves", {"default:acacia_leaves", "default:leaves"});
	POTATOES = g("potatoes", {"farming:potato_4", "mcl_farming:potato_7",
									 "mcl_farming:potato_4", "default:grass_5"});
	WHEAT = g("wheat", {"farming:wheat_4", "mcl_farming:wheat_7", "mcl_farming:wheat_4",
							   "default:grass_5"});
	BEDROCK = g("bedrock", "default:obsidian");
	RAIL_NORTH_SOUTH = prefer_arnis("rail_north_south", RAIL);
	RAIL_EAST_WEST = prefer_arnis("rail_east_west", RAIL);
	RAIL_ASCENDING_EAST = prefer_arnis("rail_ascending_east", RAIL);
	RAIL_ASCENDING_WEST = prefer_arnis("rail_ascending_west", RAIL);
	RAIL_ASCENDING_NORTH = prefer_arnis("rail_ascending_north", RAIL);
	RAIL_ASCENDING_SOUTH = prefer_arnis("rail_ascending_south", RAIL);
	RAIL_NORTH_EAST = prefer_arnis("rail_north_east", RAIL);
	RAIL_NORTH_WEST = prefer_arnis("rail_north_west", RAIL);
	RAIL_SOUTH_EAST = prefer_arnis("rail_south_east", RAIL);
	RAIL_SOUTH_WEST = prefer_arnis("rail_south_west", RAIL);
	// Advtrains 2.5+ registers four angular variants and rotates each with
	// param2. Resolve the complete family only when the mod is present; the
	// railway renderer then selects either this family or carts for the whole
	// path, never a per-cell mixture.
	ADVTRAINS_AVAILABLE =
			mg->m_emerge->ndef->getId("advtrains:dtrack_st") != CONTENT_IGNORE &&
			mg->m_emerge->ndef->getId("advtrains:dtrack_cr") != CONTENT_IGNORE;
	ADVTRAINS_SLOPES_AVAILABLE =
			ADVTRAINS_AVAILABLE &&
			mg->m_emerge->ndef->getId("advtrains:dtrack_vst1") != CONTENT_IGNORE &&
			mg->m_emerge->ndef->getId("advtrains:dtrack_vst2") != CONTENT_IGNORE;
	ADVTRAINS_GENTLE_SLOPES_AVAILABLE =
			ADVTRAINS_AVAILABLE &&
			mg->m_emerge->ndef->getId("advtrains:dtrack_vst31") != CONTENT_IGNORE &&
			mg->m_emerge->ndef->getId("advtrains:dtrack_vst32") != CONTENT_IGNORE &&
			mg->m_emerge->ndef->getId("advtrains:dtrack_vst33") != CONTENT_IGNORE;
	ADVTRAINS_DIAGONAL_SLOPES_AVAILABLE =
			ADVTRAINS_AVAILABLE &&
			mg->m_emerge->ndef->getId("advtrains:dtrack_vst1_45") != CONTENT_IGNORE &&
			mg->m_emerge->ndef->getId("advtrains:dtrack_vst2_45") != CONTENT_IGNORE;
	if (ADVTRAINS_DIAGONAL_SLOPES_AVAILABLE)
		ADV_RAIL_DIAGONAL_SLOPE = {
				g("advtrains:dtrack_vst1_45"), g("advtrains:dtrack_vst2_45")};
	ADVTRAINS_JUNCTIONS_AVAILABLE =
			ADVTRAINS_AVAILABLE &&
			mg->m_emerge->ndef->getId("advtrains:dtrack_swlst") != CONTENT_IGNORE &&
			mg->m_emerge->ndef->getId("advtrains:dtrack_swrst") != CONTENT_IGNORE &&
			mg->m_emerge->ndef->getId("advtrains:dtrack_sy_l") != CONTENT_IGNORE &&
			mg->m_emerge->ndef->getId("advtrains:dtrack_s3_s") != CONTENT_IGNORE &&
			mg->m_emerge->ndef->getId("advtrains:dtrack_xing_st") != CONTENT_IGNORE;
	ADVTRAINS_CROSSINGS_AVAILABLE =
			ADVTRAINS_AVAILABLE &&
			mg->m_emerge->ndef->getId("advtrains:dtrack_xing90plusx_30l") !=
					CONTENT_IGNORE &&
			mg->m_emerge->ndef->getId("advtrains:dtrack_xingdiag_30l45r") !=
					CONTENT_IGNORE;
	if (ADVTRAINS_AVAILABLE) {
		ADV_RAIL_STRAIGHT_0 = g("adv_rail_straight_0", "advtrains:dtrack_st");
		ADV_RAIL_STRAIGHT_30 = g("adv_rail_straight_30", "advtrains:dtrack_st_30");
		ADV_RAIL_STRAIGHT_45 = g("adv_rail_straight_45", "advtrains:dtrack_st_45");
		ADV_RAIL_STRAIGHT_60 = g("adv_rail_straight_60", "advtrains:dtrack_st_60");
		ADV_RAIL_CURVE_0 = g("adv_rail_curve_0", "advtrains:dtrack_cr");
		ADV_RAIL_CURVE_30 = g("adv_rail_curve_30", "advtrains:dtrack_cr_30");
		ADV_RAIL_CURVE_45 = g("adv_rail_curve_45", "advtrains:dtrack_cr_45");
		ADV_RAIL_CURVE_60 = g("adv_rail_curve_60", "advtrains:dtrack_cr_60");
		if (ADVTRAINS_JUNCTIONS_AVAILABLE) {
			const std::array<std::string, 4> suffixes{{"", "_30", "_45", "_60"}};
			for (std::size_t i = 0; i < suffixes.size(); ++i) {
				const std::string left = "advtrains:dtrack_swlst" + suffixes[i];
				const std::string right = "advtrains:dtrack_swrst" + suffixes[i];
				const std::string y_turnout = "advtrains:dtrack_sy_l" + suffixes[i];
				const std::string three_way = "advtrains:dtrack_s3_s" + suffixes[i];
				const std::string crossing = "advtrains:dtrack_xing_st" + suffixes[i];
				ADV_RAIL_SWITCH_LEFT_STRAIGHT[i] = g(left.c_str());
				ADV_RAIL_SWITCH_RIGHT_STRAIGHT[i] = g(right.c_str());
				ADV_RAIL_Y_TURNOUT[i] = g(y_turnout.c_str());
				ADV_RAIL_THREE_WAY_STRAIGHT[i] = g(three_way.c_str());
				ADV_RAIL_PERP_CROSSING[i] = g(crossing.c_str());
			}
		}
		if (ADVTRAINS_CROSSINGS_AVAILABLE) {
			const std::array<const char *, 6> ninety{
					{"30l", "45l", "60l", "60r", "45r", "30r"}};
			for (std::size_t i = 0; i < ninety.size(); ++i) {
				const std::string name =
						"advtrains:dtrack_xing90plusx_" + std::string(ninety[i]);
				ADV_RAIL_90_PLUS_CROSSING[i] = g(name.c_str());
			}
			const std::array<const char *, 7> diagonal{{"30l45r", "60l30l", "60l45r",
					"60l60r", "60r45l", "60r30r", "30r45l"}};
			for (std::size_t i = 0; i < diagonal.size(); ++i) {
				const std::string name =
						"advtrains:dtrack_xingdiag_" + std::string(diagonal[i]);
				ADV_RAIL_DIAGONAL_CROSSING[i] = g(name.c_str());
			}
		}
		if (ADVTRAINS_SLOPES_AVAILABLE) {
			ADV_RAIL_SLOPE_UP = g("adv_rail_slope_up", "advtrains:dtrack_vst1");
			ADV_RAIL_SLOPE_DOWN = g("adv_rail_slope_down", "advtrains:dtrack_vst2");
		} else {
			ADV_RAIL_SLOPE_UP = prefer_arnis("adv_rail_slope_up", ADV_RAIL_STRAIGHT_0);
			ADV_RAIL_SLOPE_DOWN =
					prefer_arnis("adv_rail_slope_down", ADV_RAIL_STRAIGHT_0);
		}
		if (ADVTRAINS_GENTLE_SLOPES_AVAILABLE) {
			ADV_RAIL_GENTLE_SLOPE = {g("advtrains:dtrack_vst31"),
					g("advtrains:dtrack_vst32"), g("advtrains:dtrack_vst33")};
		} else {
			ADV_RAIL_GENTLE_SLOPE = {
					ADV_RAIL_SLOPE_UP, ADV_RAIL_SLOPE_DOWN, ADV_RAIL_SLOPE_DOWN};
		}
	} else {
		ADV_RAIL_STRAIGHT_0 = prefer_arnis("adv_rail_straight_0", RAIL);
		ADV_RAIL_STRAIGHT_30 = prefer_arnis("adv_rail_straight_30", RAIL);
		ADV_RAIL_STRAIGHT_45 = prefer_arnis("adv_rail_straight_45", RAIL);
		ADV_RAIL_STRAIGHT_60 = prefer_arnis("adv_rail_straight_60", RAIL);
		ADV_RAIL_CURVE_0 = prefer_arnis("adv_rail_curve_0", RAIL);
		ADV_RAIL_CURVE_30 = prefer_arnis("adv_rail_curve_30", RAIL);
		ADV_RAIL_CURVE_45 = prefer_arnis("adv_rail_curve_45", RAIL);
		ADV_RAIL_CURVE_60 = prefer_arnis("adv_rail_curve_60", RAIL);
		ADV_RAIL_SLOPE_UP = prefer_arnis("adv_rail_slope_up", RAIL);
		ADV_RAIL_SLOPE_DOWN = prefer_arnis("adv_rail_slope_down", RAIL);
		ADV_RAIL_GENTLE_SLOPE = {prefer_arnis("adv_rail_gentle_slope_0", RAIL),
				prefer_arnis("adv_rail_gentle_slope_1", RAIL),
				prefer_arnis("adv_rail_gentle_slope_2", RAIL)};
	}
	ADV_RAIL_NORTH_SOUTH = prefer_arnis("adv_rail_north_south", ADV_RAIL_STRAIGHT_0);
	ADV_RAIL_NORTH_SOUTH.setParam2(0);
	ADV_RAIL_EAST_WEST = prefer_arnis("adv_rail_east_west", ADV_RAIL_STRAIGHT_0);
	ADV_RAIL_EAST_WEST.setParam2(1);
	ADV_RAIL_DIAGONAL_NE_SW =
			prefer_arnis("adv_rail_diagonal_ne_sw", ADV_RAIL_STRAIGHT_45);
	ADV_RAIL_DIAGONAL_NE_SW.setParam2(0);
	ADV_RAIL_DIAGONAL_NW_SE =
			prefer_arnis("adv_rail_diagonal_nw_se", ADV_RAIL_STRAIGHT_45);
	ADV_RAIL_DIAGONAL_NW_SE.setParam2(1);
	ADV_PLATFORM_HIGH = g("adv_platform_high",
			{"advtrains:platform_high_stonebrick", "default:stonebrick"});
	CACTUS = g("cactus", {"default:cactus", "mcl_core:cactus"});
	COARSE_DIRT = g("coarse_dirt", "default:dry_dirt");
	IRON_ORE = g("iron_ore", "default:stone_with_iron");
	COAL_ORE = g("coal_ore", "default:stone_with_coal");
	GOLD_ORE = g("gold_ore", "default:stone_with_gold");
	COPPER_ORE = g("copper_ore", "default:stone_with_copper");
	// Luanti's default game has no vanilla lapis/redstone equivalents; mese is
	// the closest visible ore fallback while diamond is provided directly.
	LAPIS_ORE = g("lapis_ore", {"default:stone_with_mese", "default:stone_with_coal"});
	REDSTONE_ORE =
			g("redstone_ore", {"default:stone_with_mese", "default:stone_with_iron"});
	DIAMOND_ORE =
			g("diamond_ore", {"default:stone_with_diamond", "default:stone_with_gold"});
	DEEPSLATE_COAL_ORE = optional_g("deepslate_coal_ore", COAL_ORE,
			{"mcl_core:deepslate_coal_ore", "mcl_deepslate:deepslate_coal_ore",
					"mcl_deepslate:deepslate_with_coal"});
	DEEPSLATE_IRON_ORE = optional_g("deepslate_iron_ore", IRON_ORE,
			{"mcl_core:deepslate_iron_ore", "mcl_deepslate:deepslate_iron_ore",
					"mcl_deepslate:deepslate_with_iron"});
	DEEPSLATE_COPPER_ORE = optional_g("deepslate_copper_ore", COPPER_ORE,
			{"mcl_core:deepslate_copper_ore", "mcl_deepslate:deepslate_copper_ore",
					"mcl_deepslate:deepslate_with_copper"});
	DEEPSLATE_GOLD_ORE = optional_g("deepslate_gold_ore", GOLD_ORE,
			{"mcl_core:deepslate_gold_ore", "mcl_deepslate:deepslate_gold_ore",
					"mcl_deepslate:deepslate_with_gold"});
	DEEPSLATE_REDSTONE_ORE = optional_g("deepslate_redstone_ore", REDSTONE_ORE,
			{"mcl_core:deepslate_redstone_ore", "mcl_deepslate:deepslate_redstone_ore",
					"mcl_deepslate:deepslate_with_redstone"});
	DEEPSLATE_LAPIS_ORE = optional_g("deepslate_lapis_ore", LAPIS_ORE,
			{"mcl_core:deepslate_lapis_ore", "mcl_deepslate:deepslate_lapis_ore",
					"mcl_deepslate:deepslate_with_lapis"});
	DEEPSLATE_DIAMOND_ORE = optional_g("deepslate_diamond_ore", DIAMOND_ORE,
			{"mcl_core:deepslate_diamond_ore", "mcl_deepslate:deepslate_diamond_ore",
					"mcl_deepslate:deepslate_with_diamond"});
	CLAY = g("clay", "default:clay");
	DIRT_PATH = g("dirt_path", "default:dirt_with_grass_footsteps");
	ICE = g("ice", "default:ice");
	PACKED_ICE = g("packed_ice", "default:ice");
	BLUE_ICE = g("blue_ice", {"mcl_core:blue_ice", "default:ice"});
	LAVA = liquid("lava", {"mcl_core:lava_source", "default:lava_source"});
	POWDER_SNOW = g("powder_snow",
			{"mcl_powder_snow:powder_snow", "mcl_core:snow", "default:snow"});
	AMETHYST_BLOCK = g("amethyst_block",
			{"mcl_amethyst:amethyst_block", "default:stone", "mcl_core:stone"});
	BUDDING_AMETHYST = g("budding_amethyst",
			{"mcl_amethyst:budding_amethyst_block", "mcl_amethyst:budding_amethyst",
					"mcl_amethyst:amethyst_block", "default:stone", "mcl_core:stone"});
	AMETHYST_CLUSTER = g("amethyst_cluster",
			{"mcl_amethyst:amethyst_cluster", "mcl_amethyst:large_amethyst_bud",
					"default:stone", "mcl_core:stone"});
	CALCITE = g("calcite", {"mcl_amethyst:calcite", "default:stone", "mcl_core:stone"});
	BASALT = g("basalt", {"mcl_blackstone:basalt", "default:stone", "mcl_core:stone"});
	SMOOTH_BASALT =
			g("smooth_basalt", {"mcl_blackstone:basalt_smooth", "mcl_blackstone:basalt",
									   "default:stone", "mcl_core:stone"});
	CAVE_VINES = g("cave_vines",
			{"mcl_lush_caves:cave_vines_lit", "cave_vines_lit",
					"mcl_lush_caves:cave_vines", "cave_vines",
					"mcl_lush_caves:cave_vines_plant_with_berries",
					"mcl_lush_caves:cave_vines_plant_no_berries_lit",
					"mcl_lush_caves:cave_vines_body_with_berries",
					"mcl_lush_caves:cave_vines_body_lit",
					"mcl_lush_caves:cave_vines_body",
					"mcl_lush_caves:cave_vines_head_with_berries",
					"mcl_lush_caves:cave_vines_head_lit",
					"mcl_lush_caves:cave_vines_head",
					"mcl_lush_caves:cave_vines_plant_lit",
					"mcl_lush_caves:cave_vines_plant",
					"mcl_lush_caves:cave_vines_plant_no_berries",
					"mcl_lush_caves:cave_vines_plant_lit_no_berries",
					"mcl_lush_caves:cave_vines_berries",
					"mcl_lush_caves:cave_vines_with_berries",
					"mcl_lush_caves:cave_vines_plant_berries",
					"mcl_lush_caves:cave_vines_body_no_berries",
					"mcl_lush_caves:cave_vines_head_no_berries", "mcl_core:vine",
					"mcl_vines:vine", "mcl_vines:vine_with_berries", "default:vine",
					"flowers:vine", "default:leaves", "default:grass_5",
					"basenodes:stone", "default:stone"});
	CAVE_VINES_UNLIT = g("cave_vines_unlit",
			{"mcl_lush_caves:cave_vines", "cave_vines", "mcl_lush_caves:cave_vines_lit",
					"cave_vines_lit", "mcl_lush_caves:cave_vines_plant_with_berries",
					"mcl_lush_caves:cave_vines_plant_no_berries_lit",
					"mcl_lush_caves:cave_vines_body_with_berries",
					"mcl_lush_caves:cave_vines_body_lit",
					"mcl_lush_caves:cave_vines_body", "mcl_lush_caves:cave_vines_head",
					"mcl_lush_caves:cave_vines_head_with_berries",
					"mcl_lush_caves:cave_vines_head_lit",
					"mcl_lush_caves:cave_vines_plant",
					"mcl_lush_caves:cave_vines_plant_lit",
					"mcl_lush_caves:cave_vines_plant_no_berries",
					"mcl_lush_caves:cave_vines_plant_lit_no_berries",
					"mcl_lush_caves:cave_vines_berries",
					"mcl_lush_caves:cave_vines_plant_berries",
					"mcl_lush_caves:cave_vines_body_no_berries",
					"mcl_lush_caves:cave_vines_head_no_berries", "mcl_core:vine",
					"mcl_vines:vine", "mcl_vines:vine_with_berries", "default:vine",
					"default:leaves", "default:grass_5", "basenodes:stone",
					"default:stone"});
	CAVE_VINES_PLANT = prefer_arnis("cave_vines_plant", CAVE_VINES_UNLIT);
	CAVE_VINES_PLANT_LIT = prefer_arnis("cave_vines_plant_lit", CAVE_VINES);
	SPORE_BLOSSOM = g("spore_blossom",
			{"mcl_lush_caves:spore_blossom", "spore_blossom",
					"mcl_lush_caves:spore_blossom_hanging",
					"mcl_lush_caves:spore_blossom_hanging_bottom",
					"mcl_lush_caves:spore_blossom_top",
					"mcl_lush_caves:spore_blossom_bottom", "mcl_flowers:flower_rose",
					"flowers:rose", "flowers:viola", "mcl_flowers:flower_allium",
					"mcl_flowers:flower_dandelion", "mcl_flowers:flower_poppy",
					"flowers:dandelion", "flowers:mushroom_red", "flowers:mushroom_brown",
					"default:leaves", "basenodes:stone", "default:stone"});
	AZALEA = g("azalea",
			{"mcl_lush_caves:azalea", "azalea", "mcl_lush_caves:azalea_bush",
					"mcl_lush_caves:azalea_plant", "mcl_lush_caves:azalea_bush_leaves",
					"mcl_lush_caves:azalea_leaves", "mcl_lush_caves:azalea_leaves_bush",
					"mcl_trees:leaves_azalea", "mcl_trees:azalea_leaves",
					"mcl_trees:leaves_azalea_flowering", "mcl_flowers:flower_rose",
					"default:bush_leaves", "default:leaves", "basenodes:stone",
					"default:stone"});
	FLOWERING_AZALEA = g("flowering_azalea",
			{"mcl_lush_caves:azalea_flowering", "azalea_flowering",
					"mcl_lush_caves:azalea", "mcl_lush_caves:flowering_azalea_bush",
					"mcl_lush_caves:azalea_flowering_bush",
					"mcl_lush_caves:flowering_azalea", "mcl_flowers:flower_rose",
					"mcl_lush_caves:flowering_azalea_leaves",
					"mcl_trees:leaves_azalea_flowering",
					"mcl_trees:azalea_leaves_flowering", "default:bush_leaves",
					"flowers:rose", "default:leaves", "basenodes:stone",
					"default:stone"});
	AZALEA_LEAVES = g("azalea_leaves",
			{"mcl_lush_caves:azalea_leaves", "mcl_lush_caves:azalea_leaves_fruiting",
					"default:leaves", "basenodes:stone", "default:stone"});
	FLOWERING_AZALEA_LEAVES = g("flowering_azalea_leaves",
			{"mcl_lush_caves:azalea_leaves_flowering",
					"mcl_lush_caves:flowering_azalea_leaves",
					"mcl_lush_caves:azalea_leaves", "default:leaves", "basenodes:stone",
					"default:stone"});
	WHITE_BED = g("white_bed",
			{"mcl_beds:bed_white_bottom", "beds:bed_bottom", "default:wood"});
	LECTERN = g("lectern", {"mcl_lectern:lectern", "default:bookshelf", "default:wood"});
	CAKE = g("cake", {"mcl_cake:cake", "default:apple", "default:wood"});
	MELON = g("melon", {"mcl_farming:melon", "farming:melon", "default:wood"});
	LOOM = g("loom", {"mcl_loom:loom", "default:wood"});
	SMITHING_TABLE =
			g("smithing_table", {"mcl_smithing_table:table", "default:steelblock"});
	RED_MUSHROOM_BLOCK = g("red_mushroom_block",
			{"mcl_mushrooms:red_mushroom_block_cap_111111",
					"red_mushroom_block_cap_111111",
					"mcl_mushrooms:red_mushroom_block_cap_111110",
					"mcl_mushrooms:red_mushroom_block_cap_111100",
					"mcl_mushrooms:red_mushroom_block_cap_11111111",
					"mcl_mushrooms:red_mushroom_block_cap_1111111",
					"mcl_mushrooms:red_mushroom_block_cap",
					"mcl_mushrooms:red_mushroom_block_cap_all",
					"mcl_mushrooms:red_mushroom_block", "mcl_mushrooms:mushroom_block",
					"mcl_core:wood", "mcl_core:stone", "wool:red", "default:wood",
					"default:stone"});
	BROWN_MUSHROOM_BLOCK = g("brown_mushroom_block",
			{"mcl_mushrooms:brown_mushroom_block_cap_111111",
					"brown_mushroom_block_cap_111111",
					"mcl_mushrooms:brown_mushroom_block_cap_111110",
					"mcl_mushrooms:brown_mushroom_block_cap_111100",
					"mcl_mushrooms:brown_mushroom_block_cap_11111111",
					"mcl_mushrooms:brown_mushroom_block_cap_1111111",
					"mcl_mushrooms:brown_mushroom_block_cap",
					"mcl_mushrooms:brown_mushroom_block_cap_all",
					"mcl_mushrooms:brown_mushroom_block", "mcl_mushrooms:mushroom_block",
					"mcl_core:wood", "mcl_core:stone", "wool:brown", "default:wood",
					"default:stone"});
	MUSHROOM_STEM = g("mushroom_stem",
			{"mcl_mushrooms:brown_mushroom_block_stem_full",
					"brown_mushroom_block_stem_full",
					"mcl_mushrooms:brown_mushroom_block_stem",
					"mcl_mushrooms:red_mushroom_block_stem_full",
					"mcl_mushrooms:red_mushroom_block_stem",
					"mcl_mushrooms:mushroom_block_stem", "mcl_mushrooms:mushroom_stem",
					"mcl_mushrooms:brown_mushroom_stem",
					"mcl_mushrooms:brown_mushroom_block_stem_full",
					"mcl_mushrooms:mushroom_block", "mcl_core:wood", "mcl_core:stone",
					"default:aspen_tree", "default:aspen_wood", "default:wood",
					"default:stone"});
	SHROOMLIGHT = g(
			"shroomlight", {"mcl_crimson:shroomlight", "shroomlight",
								   "mcl_nether:shroomlight", "mcl_core:shroomlight",
								   "mcl_crimson:shroom_light", "mcl_core:glowstone",
								   "default:meselamp", "default:torch", "default:stone"});
	TUBE_CORAL_BLOCK = g("tube_coral_block",
			{"mcl_ocean:tube_coral_block", "tube_coral_block",
					"mcl_ocean:coral_block_tube", "mcl_ocean:tube_coral_block_dead",
					"mcl_ocean:coral_block_tube_dead", "mcl_core:stone",
					"mcl_ocean:coral_block_tube_dead", "mcl_ocean:coral_tube_block",
					"mcl_ocean:tube_coral_block_dead", "mcl_ocean:coral_tube",
					"marinara:hardcoral_blue", "default:coral_brown",
					"default:coral_skeleton", "default:stone"});
	BRAIN_CORAL_BLOCK = g("brain_coral_block",
			{"mcl_ocean:brain_coral_block", "brain_coral_block",
					"mcl_ocean:coral_block_brain", "mcl_ocean:brain_coral_block_dead",
					"mcl_ocean:coral_block_brain_dead", "mcl_ocean:coral_brain_block",
					"mcl_ocean:brain_coral_block_dead", "mcl_ocean:coral_brain",
					"mcl_core:stone", "marinara:hardcoral_pink", "default:coral_brown",
					"default:coral_skeleton", "default:stone"});
	BUBBLE_CORAL_BLOCK = g("bubble_coral_block",
			{"mcl_ocean:bubble_coral_block", "bubble_coral_block",
					"mcl_ocean:coral_block_bubble", "mcl_ocean:bubble_coral_block_dead",
					"mcl_ocean:coral_block_bubble_dead", "mcl_ocean:coral_bubble_block",
					"mcl_ocean:bubble_coral_block_dead", "mcl_ocean:coral_bubble",
					"mcl_core:stone", "marinara:hardcoral_violet", "default:coral_brown",
					"default:coral_skeleton", "default:stone"});
	FIRE_CORAL_BLOCK = g("fire_coral_block",
			{"mcl_ocean:fire_coral_block", "fire_coral_block",
					"mcl_ocean:coral_block_fire", "mcl_ocean:fire_coral_block_dead",
					"mcl_core:stone", "mcl_ocean:coral_block_fire_dead",
					"mcl_ocean:coral_fire_block", "mcl_ocean:fire_coral_block_dead",
					"mcl_ocean:coral_fire", "marinara:hardcoral_red",
					"default:coral_orange", "default:coral_skeleton", "default:stone"});
	HORN_CORAL_BLOCK = g("horn_coral_block",
			{"mcl_ocean:horn_coral_block", "horn_coral_block",
					"mcl_ocean:coral_block_horn", "mcl_ocean:horn_coral_block_dead",
					"mcl_core:stone", "mcl_ocean:coral_block_horn_dead",
					"mcl_ocean:coral_horn_block", "mcl_ocean:horn_coral_block_dead",
					"mcl_ocean:coral_horn", "marinara:hardcoral_yellow",
					"default:coral_brown", "default:coral_skeleton", "default:stone"});
	DEAD_TUBE_CORAL_BLOCK = g("dead_tube_coral_block",
			{"mcl_ocean:dead_tube_coral_block", "mcl_ocean:coral_block_tube_dead",
					"mcl_core:stone", "default:coral_skeleton", "marinara:hardcoral",
					"default:stone"});
	DEAD_BRAIN_CORAL_BLOCK = g("dead_brain_coral_block",
			{"mcl_ocean:dead_brain_coral_block", "mcl_ocean:coral_block_brain_dead",
					"mcl_core:stone", "default:coral_skeleton", "marinara:hardcoral",
					"default:stone"});
	DEAD_BUBBLE_CORAL_BLOCK = g("dead_bubble_coral_block",
			{"mcl_ocean:dead_bubble_coral_block", "mcl_ocean:coral_block_bubble_dead",
					"mcl_core:stone", "default:coral_skeleton", "marinara:hardcoral",
					"default:stone"});
	DEAD_FIRE_CORAL_BLOCK = g("dead_fire_coral_block",
			{"mcl_ocean:dead_fire_coral_block", "mcl_ocean:coral_block_fire_dead",
					"mcl_core:stone", "default:coral_skeleton", "marinara:hardcoral",
					"default:stone"});
	DEAD_HORN_CORAL_BLOCK = g("dead_horn_coral_block",
			{"mcl_ocean:dead_horn_coral_block", "mcl_ocean:coral_block_horn_dead",
					"mcl_core:stone", "default:coral_skeleton", "marinara:hardcoral",
					"default:stone"});
	TUBE_CORAL = g("tube_coral",
			{"mcl_ocean:tube_coral", "mcl_ocean:coral_tube", "mcl_core:stone",
					"default:coral_cyan", "marinara:hardcoral_blue",
					"default:coral_skeleton", "default:stone"});
	BRAIN_CORAL = g("brain_coral",
			{"mcl_ocean:brain_coral", "mcl_ocean:coral_brain", "mcl_core:stone",
					"default:coral_pink", "marinara:hardcoral_pink",
					"default:coral_skeleton", "default:stone"});
	BUBBLE_CORAL = g("bubble_coral",
			{"mcl_ocean:bubble_coral", "mcl_ocean:coral_bubble", "mcl_core:stone",
					"default:coral_pink", "marinara:hardcoral_violet",
					"default:coral_skeleton", "default:stone"});
	FIRE_CORAL = g("fire_coral",
			{"mcl_ocean:fire_coral", "mcl_ocean:coral_fire", "mcl_core:stone",
					"default:coral_orange", "marinara:hardcoral_red",
					"default:coral_skeleton", "default:stone"});
	HORN_CORAL = g("horn_coral",
			{"mcl_ocean:horn_coral", "mcl_ocean:coral_horn", "mcl_core:stone",
					"default:coral_brown", "marinara:hardcoral_yellow",
					"default:coral_skeleton", "default:stone"});
	TUBE_CORAL_FAN = g("tube_coral_fan",
			{"mcl_ocean:tube_coral_fan", "mcl_ocean:coral_fan_tube", "mcl_core:stone",
					"default:coral_cyan", "marinara:hardcoral_blue",
					"default:coral_skeleton", "default:stone"});
	BRAIN_CORAL_FAN = g("brain_coral_fan",
			{"mcl_ocean:brain_coral_fan", "mcl_ocean:coral_fan_brain", "mcl_core:stone",
					"default:coral_pink", "marinara:hardcoral_pink",
					"default:coral_skeleton", "default:stone"});
	BUBBLE_CORAL_FAN = g("bubble_coral_fan",
			{"mcl_ocean:bubble_coral_fan", "mcl_ocean:coral_fan_bubble", "mcl_core:stone",
					"default:coral_pink", "marinara:hardcoral_violet",
					"default:coral_skeleton", "default:stone"});
	FIRE_CORAL_FAN = g("fire_coral_fan",
			{"mcl_ocean:fire_coral_fan", "mcl_ocean:coral_fan_fire", "mcl_core:stone",
					"default:coral_orange", "marinara:hardcoral_red",
					"default:coral_skeleton", "default:stone"});
	HORN_CORAL_FAN = g("horn_coral_fan",
			{"mcl_ocean:horn_coral_fan", "mcl_ocean:coral_fan_horn", "mcl_core:stone",
					"default:coral_brown", "marinara:hardcoral_yellow",
					"default:coral_skeleton", "default:stone"});
	SMALL_AMETHYST_BUD = g("small_amethyst_bud",
			{"mcl_amethyst:small_amethyst_bud", "mcl_amethyst:amethyst_cluster",
					"default:stone", "mcl_core:stone"});
	MEDIUM_AMETHYST_BUD = g("medium_amethyst_bud",
			{"mcl_amethyst:medium_amethyst_bud", "mcl_amethyst:amethyst_cluster",
					"default:stone", "mcl_core:stone"});
	LARGE_AMETHYST_BUD = g("large_amethyst_bud",
			{"mcl_amethyst:large_amethyst_bud", "mcl_amethyst:amethyst_cluster",
					"default:stone", "mcl_core:stone"});
	DRIPSTONE_BLOCK = g("dripstone_block",
			{"mcl_dripstone:dripstone_block", "default:stone", "mcl_core:stone"});
	POINTED_DRIPSTONE = g("pointed_dripstone",
			{"mcl_dripstone:dripstone_bottom_tip", "mcl_dripstone:dripstone_top_tip",
					"default:stone", "mcl_core:stone"});
	GLOW_LICHEN = g("glow_lichen", {"mcl_core:glow_lichen_down", "mcl_core:glow_lichen_d",
										   "default:coral_green", "default:coral_cyan"});
	SCULK = g("sculk",
			{"mcl_sculk:sculk", "sculk", "mcl_sculk:sculk_block", "mcl_sculk:sculk_vein",
					"mcl_sculk:sculk_sensor", "mcl_deepslate:sculk", "mcl_core:stone",
					"default:obsidian", "default:stone"});
	SCULK_VEIN = g("sculk_vein",
			{"mcl_sculk:vein", "mcl_sculk:sculk_vein", "mcl_sculk:sculk",
					"mcl_core:glow_lichen_down", "default:coral_green", "default:stone"});
	SCULK_CATALYST = g("sculk_catalyst",
			{"mcl_sculk:catalyst", "sculk_catalyst", "mcl_sculk:sculk_catalyst",
					"mcl_sculk:sculk", "mcl_deepslate:sculk", "mcl_sculk:sculk_sensor",
					"mcl_core:stone", "default:obsidian", "default:stone"});
	SCULK_SENSOR = g("sculk_sensor",
			{"mcl_sculk:sensor", "sculk_sensor", "mcl_sculk:sculk_sensor",
					"mcl_sculk:sculk", "mcl_deepslate:sculk", "mcl_sculk:catalyst",
					"mcl_core:stone", "default:obsidian", "default:stone"});
	SCULK_SHRIEKER = g("sculk_shrieker",
			{"mcl_sculk:shrieker", "sculk_shrieker", "mcl_sculk:sculk_shrieker",
					"mcl_sculk:sculk", "mcl_deepslate:sculk", "mcl_sculk:sensor",
					"mcl_core:stone", "default:obsidian", "default:stone"});
	BIG_DRIPLEAF = g("big_dripleaf",
			{"mcl_lush_caves:big_dripleaf_1", "mcl_lush_caves:big_dripleaf",
					"mcl_flowers:double_grass", "default:grass_5", "default:stone"});
	BIG_DRIPLEAF_STEM = g("big_dripleaf_stem",
			{"mcl_lush_caves:big_dripleaf_stem_1", "mcl_lush_caves:big_dripleaf_stem",
					"mcl_lush_caves:big_dripleaf_1", "default:grass_5", "default:stone"});
	SMALL_DRIPLEAF_LOWER = g("small_dripleaf_lower",
			{"mcl_lush_caves:small_dripleaf_1", "mcl_lush_caves:small_dripleaf_bottom",
					"mcl_flowers:double_grass", "default:grass_5", "default:stone"});
	SMALL_DRIPLEAF_UPPER = g("small_dripleaf_upper",
			{"mcl_lush_caves:small_dripleaf_2", "mcl_lush_caves:small_dripleaf_top",
					"mcl_lush_caves:small_dripleaf_1", "default:grass_5",
					"default:stone"});
	MUD = g("mud",
			{"mcl_mud:mud", "mud", "mcl_mud:mud_block", "default:dirt", "mcl_core:dirt",
					"mcl_core:coarse_dirt", "mcl_core:packed_mud", "default:mud",
					"default:clay", "default:dirt_with_rainforest_litter",
					"default:dirt_with_grass", "default:stone"});
	DEAD_BUSH = g("dead_bush", "default:dry_shrub");
	MYCELIUM =
			g("mycelium", {"default:mycelium", "mycelium", "mcl_core:mycelium",
								  "mcl_core:mycelium_block", "mcl_mushrooms:mycelium",
								  "default:dirt_with_mycelium", "mcl_core:dirt",
								  "default:dirt", "default:dirt_with_coniferous_litter",
								  "default:dirt_with_rainforest_litter",
								  "default:dirt_with_grass", "default:stone"});
	RED_MUSHROOM =
			g("red_mushroom", {"flowers:mushroom_red", "mcl_mushrooms:red_mushroom"});
	BROWN_MUSHROOM = g(
			"brown_mushroom", {"flowers:mushroom_brown", "mcl_mushrooms:brown_mushroom"});
	MOSS_CARPET =
			g("moss_carpet", {"default:moss", "mcl_moss:moss_carpet", "mcl_moss:moss",
									 "mcl_lush_caves:moss", "default:mossycobble"});
	SWEET_BERRY_BUSH = g("sweet_berry_bush",
			{"farming:strawberry", "mcl_sweet_berry:bush",
					"mcl_farming:sweet_berry_bush_3", "mcl_farming:sweet_berry_bush_2",
					"default:grass_5"});
	PUMPKIN = g("pumpkin", {"farming:pumpkin", "mcl_farming:pumpkin", "default:stone"});
	LILY_PAD = g("lily_pad", {"flowers:waterlily", "mcl_flowers:waterlily"});
	TALL_GRASS_BOTTOM = g("tall_grass_bottom",
			{"default:grass_5", "mcl_flowers:tallgrass", "mcl_flowers:double_grass"});
	SUGAR_CANE =
			g("sugar_cane", {"farming:cotton_8", "default:papyrus", "default:grass_5"});
	TALL_GRASS_TOP = g("tall_grass_top",
			{"default:grass_5", "mcl_flowers:double_grass_top", "mcl_flowers:tallgrass"});
	SUNFLOWER_LOWER = flower("sunflower_lower",
			{"mcl_flowers:sunflower", "flowers:sunflower", "mcl_flowers:double_grass",
					"default:grass_5"},
			TALL_GRASS_BOTTOM);
	SUNFLOWER_UPPER = flower("sunflower_upper",
			{"mcl_flowers:sunflower_top", "flowers:sunflower_top",
					"mcl_flowers:double_grass_top", "default:grass_5"},
			TALL_GRASS_TOP);
	LILAC_LOWER = flower("lilac_lower",
			{"mcl_flowers:lilac", "flowers:lilac", "mcl_flowers:double_grass",
					"default:grass_5"},
			TALL_GRASS_BOTTOM);
	LILAC_UPPER = flower("lilac_upper",
			{"mcl_flowers:lilac_top", "flowers:lilac_top", "mcl_flowers:double_grass_top",
					"default:grass_5"},
			TALL_GRASS_TOP);
	ROSE_BUSH_LOWER = flower("rose_bush_lower",
			{"mcl_flowers:rose_bush", "flowers:rose_bush", "mcl_flowers:double_grass",
					"default:grass_5"},
			TALL_GRASS_BOTTOM);
	ROSE_BUSH_UPPER = flower("rose_bush_upper",
			{"mcl_flowers:rose_bush_top", "flowers:rose_bush_top",
					"mcl_flowers:double_grass_top", "default:grass_5"},
			TALL_GRASS_TOP);
	PEONY_LOWER = flower("peony_lower",
			{"mcl_flowers:peony", "flowers:peony", "mcl_flowers:double_grass",
					"default:grass_5"},
			TALL_GRASS_BOTTOM);
	PEONY_UPPER = flower("peony_upper",
			{"mcl_flowers:peony_top", "flowers:peony_top", "mcl_flowers:double_grass_top",
					"default:grass_5"},
			TALL_GRASS_TOP);
	CRAFTING_TABLE = g("crafting_table", "default:wood");
	FURNACE = g("furnace", "default:furnace");
	WHITE_CARPET = g("white_carpet", "wool:white");
	GREEN_CARPET = g("green_carpet", {"wool:green", "wool:lime", "wool:cyan"});
	LIGHT_BLUE_CARPET =
			g("light_blue_carpet", {"wool:light_blue", "wool:cyan", "wool:blue"});
	LIGHT_GRAY_CARPET =
			g("light_gray_carpet", {"mcl_wool:silver_carpet", "wool:light_grey",
										   "wool:light_gray", "wool:grey"});
	BOOKSHELF = g("bookshelf", "default:bookshelf");
	OAK_PRESSURE_PLATE = g("oak_pressure_plate",
			{"mesecons_pressureplates:pressure_plate_wood_off", "default:wood"});
	OAK_STAIRS = g("oak_stairs", {"stairs:stair_wood", "stairs:stair_pine_wood"});
	CHEST = g("chest", "default:chest");
	RED_CARPET = g("red_carpet", "wool:red");
	ANVIL = g("anvil", "default:steelblock");
	NOTE_BLOCK = g("note_block", "default:wood");
	OAK_DOOR = g("oak_door", "doors:door_wood_a");
	BREWING_STAND = g("brewing_stand", "default:steelblock");
	RED_BED_NORTH_HEAD = g("red_bed_north_head", "beds:bed_top");
	RED_BED_NORTH_FOOT = g("red_bed_north_foot", "beds:bed_bottom");
	RED_BED_EAST_HEAD = g("red_bed_east_head", "beds:bed_top");
	RED_BED_EAST_FOOT = g("red_bed_east_foot", "beds:bed_bottom");
	RED_BED_SOUTH_HEAD = g("red_bed_south_head", "beds:bed_top");
	RED_BED_SOUTH_FOOT = g("red_bed_south_foot", "beds:bed_bottom");
	RED_BED_WEST_HEAD = g("red_bed_west_head", "beds:bed_top");
	RED_BED_WEST_FOOT = g("red_bed_west_foot", "beds:bed_bottom");
	GRAY_STAINED_GLASS = prefer_arnis("gray_stained_glass", GLASS);
	LIGHT_GRAY_STAINED_GLASS = prefer_arnis("light_gray_stained_glass", GLASS);
	BROWN_STAINED_GLASS = prefer_arnis("brown_stained_glass", GLASS);
	TINTED_GLASS = prefer_arnis("tinted_glass", GLASS);
	OAK_TRAPDOOR = g("oak_trapdoor", "doors:trapdoor");
	BROWN_CONCRETE = g("brown_concrete",
			{"wool:brown", "basic_materials:concrete_block", "default:stone"});
	BLACK_TERRACOTTA = g("black_terracotta", "default:clay");
	BROWN_TERRACOTTA = g("brown_terracotta", "default:clay");
	STONE_BRICK_STAIRS = g("stone_brick_stairs", "stairs:stair_stonebrick");
	MUD_BRICK_STAIRS = g("mud_brick_stairs",
			{"stairs:stair_silver_sandstone_brick", "stairs:stair_stonebrick"});
	POLISHED_BLACKSTONE_BRICK_STAIRS = g("polished_blackstone_brick_stairs",
			{"stairs:stair_obsidianbrick", "stairs:stair_obsidian_block"});
	BRICK_STAIRS = g("brick_stairs", {"stairs:stair_brick", "stairs:stair_cobble"});
	POLISHED_GRANITE_STAIRS = prefer_arnis("polished_granite_stairs", STONE_BRICK_STAIRS);
	END_STONE_BRICK_STAIRS = prefer_arnis("end_stone_brick_stairs", STONE_BRICK_STAIRS);
	POLISHED_DIORITE_STAIRS = prefer_arnis("polished_diorite_stairs", STONE_BRICK_STAIRS);
	SMOOTH_SANDSTONE_STAIRS = g("smooth_sandstone_stairs", "stairs:stair_sandstone");
	QUARTZ_STAIRS = prefer_arnis("quartz_stairs", STONE_BRICK_STAIRS);
	POLISHED_ANDESITE_STAIRS =
			prefer_arnis("polished_andesite_stairs", STONE_BRICK_STAIRS);
	NETHER_BRICK_STAIRS = g("nether_brick_stairs",
			{"stairs:stair_obsidianbrick", "stairs:stair_stonebrick"});

	COBWEB = g("cobweb", {"farming:cotton_wild", "xpanes:pane_flat", "default:glass"});
	CHISELLED_BOOKSHELF_NORTH = prefer_arnis("chiseled_bookshelf", BOOKSHELF);
	CHISELLED_BOOKSHELF_EAST = prefer_arnis("chiseled_bookshelf", BOOKSHELF);
	CHISELLED_BOOKSHELF_SOUTH = prefer_arnis("chiseled_bookshelf", BOOKSHELF);
	CHISELLED_BOOKSHELF_WEST = prefer_arnis("chiseled_bookshelf", BOOKSHELF);
	DAMAGED_ANVIL = prefer_arnis("damaged_anvil", ANVIL);

	CHAIN = g("chain",
			{"basic_materials:chain_steel", "xpanes:bar_flat", "default:steelblock"});
	END_ROD = prefer_arnis("end_rod", GLOWSTONE);
	LIGHTNING_ROD = g("lightning_rod",
			{"freeminer:lamp_red", "default:meselamp", "default:copperblock",
					"basic_materials:brass_block", "default:steelblock"});
	GOLD_BLOCK = g("gold_block", "default:goldblock");
	SEA_LANTERN = prefer_arnis("sea_lantern", GLOWSTONE);
	LANTERN = g("lantern", {"mcl_lantern:lantern", "default:torch", "default:meselamp"});
	SOUL_LANTERN = g("soul_lantern",
			{"mcl_lantern:soul_lantern", "default:torch", "default:meselamp"});
	SMOKER = g("smoker", {"mcl_furnaces:smoker", "default:furnace"});
	EMPTY_FLOWER_POT =
			g("empty_flower_pot", {"mcl_flowerpots:flower_pot", "default:clay"});
	COMPOSTER = g("composter",
			{"mcl_composters:composter", "farming:composter", "default:chest"});
	HOPPER = g("hopper", {"mcl_hoppers:hopper", "hopper:hopper", "default:chest"});
	BLAST_FURNACE = g("blast_furnace", {"mcl_furnaces:blast_furnace", "default:furnace"});
	DISPENSER = g("dispenser",
			{"mcl_dispensers:dispenser", "mesecons:dispenser", "default:chest"});
	GRINDSTONE = g("grindstone", {"mcl_grindstone:grindstone", "default:stone"});
	POLISHED_BLACKSTONE_SLAB = g("polished_blackstone_slab",
			{"mcl_stairs:slab_polished_blackstone", "stairs:slab_stone_block",
					"default:stone"});
	REDSTONE_LAMP = g("redstone_lamp",
			{"mcl_redstone:redstone_lamp", "default:meselamp", "default:stone"});
	CHISELED_QUARTZ_BLOCK = g(
			"chiseled_quartz_block", {"mcl_core:chiseled_quartz_block", "default:stone"});
	ORANGE_CONCRETE = g("orange_concrete",
			{"wool:orange", "basic_materials:concrete_block", "default:stone"});
	ORANGE_WOOL = g("orange_wool", "wool:orange");
	BLUE_WOOL = g("blue_wool", "wool:blue");
	GREEN_CONCRETE = g("green_concrete",
			{"wool:green", "basic_materials:concrete_block", "default:stone"});
	BRICK_WALL = g("brick_wall", "default:brick");
	REDSTONE_BLOCK = g("redstone_block", "default:mese");
	CHAIN_X = prefer_arnis("chain", CHAIN);
	CHAIN_Z = prefer_arnis("chain", CHAIN);
	SPRUCE_DOOR_LOWER = prefer_arnis("spruce_door", OAK_DOOR);
	SPRUCE_DOOR_UPPER = prefer_arnis("spruce_door", OAK_DOOR);
	SMOOTH_STONE_SLAB = g("smooth_stone_slab",
			{"stairs:slab_stone_block", "stairs:slab_stone", "default:stone"});
	GLASS_PANE = g("glass_pane", {"xpanes:pane_flat", "default:glass"});
	LIGHT_GRAY_TERRACOTTA = g("light_gray_terracotta", "default:clay");
	OAK_SLAB_TOP = prefer_arnis("oak_slab", OAK_SLAB);
	OAK_DOOR_UPPER = prefer_arnis("oak_door", OAK_DOOR);
	SPRUCE_LEAVES = g("spruce_leaves", {"default:pine_needles", "default:leaves"});
	CYAN_STAINED_GLASS = prefer_arnis("cyan_stained_glass", GLASS);
	BLUE_STAINED_GLASS = prefer_arnis("blue_stained_glass", GLASS);
	LIGHT_BLUE_STAINED_GLASS = prefer_arnis("light_blue_stained_glass", GLASS);
	DAYLIGHT_DETECTOR = g("daylight_detector",
			{"mesecons_solarpanel:solar_panel_off", "default:glass"});
	RED_STAINED_GLASS = prefer_arnis("red_stained_glass", GLASS);
	YELLOW_STAINED_GLASS = prefer_arnis("yellow_stained_glass", GLASS);
	PURPLE_STAINED_GLASS = prefer_arnis("purple_stained_glass", GLASS);
	ORANGE_STAINED_GLASS = prefer_arnis("orange_stained_glass", GLASS);
	MAGENTA_STAINED_GLASS = prefer_arnis("magenta_stained_glass", GLASS);
	FLOWER_POT = g("flower_pot", "default:clay");
	OAK_TRAPDOOR_OPEN_NORTH = prefer_arnis("oak_trapdoor", OAK_TRAPDOOR);
	OAK_TRAPDOOR_OPEN_SOUTH = prefer_arnis("oak_trapdoor", OAK_TRAPDOOR);
	OAK_TRAPDOOR_OPEN_EAST = prefer_arnis("oak_trapdoor", OAK_TRAPDOOR);
	OAK_TRAPDOOR_OPEN_WEST = prefer_arnis("oak_trapdoor", OAK_TRAPDOOR);
	QUARTZ_SLAB_TOP = g("quartz_slab_top", "default:stone");
	DARK_OAK_TRAPDOOR = prefer_arnis("dark_oak_trapdoor", OAK_TRAPDOOR);
	SPRUCE_TRAPDOOR = prefer_arnis("spruce_trapdoor", OAK_TRAPDOOR);
	BIRCH_TRAPDOOR = prefer_arnis("birch_trapdoor", OAK_TRAPDOOR);
	MUD_BRICK_SLAB = g("mud_brick_slab",
			{"stairs:slab_silver_sandstone_brick", "default:silver_sandstone_brick"});
	BRICK_SLAB = g("brick_slab", {"stairs:slab_brick", "default:brick"});
	POTTED_RED_TULIP = g("potted_red_tulip", "flowers:rose");
	POTTED_DANDELION = g("potted_dandelion", "flowers:dandelion_yellow");
	POTTED_BLUE_ORCHID = g("potted_blue_orchid", "flowers:geranium");

	// Initialize missing blocks
	BARREL = g("barrel", "default:chest");
	FERN = g("fern", "default:fern_3");
	CHIPPED_ANVIL = prefer_arnis("chipped_anvil", ANVIL);
	LARGE_FERN_LOWER = g("large_fern_lower", "default:fern_2");
	LARGE_FERN_UPPER = g("large_fern_upper", "default:fern_3");
	LEVER = g("lever", {"mesecons_walllever:wall_lever_off", "default:steelblock"});
	COBBLESTONE_STAIRS = g("cobblestone_stairs", "stairs:stair_cobble");
	WAXED_CUT_COPPER_STAIRS = g("waxed_cut_copper_stairs",
			{"stairs:stair_copperblock", "stairs:stair_stonebrick"});
	MOSSY_STONE_BRICK_STAIRS = g("mossy_stone_brick_stairs",
			{"stairs:stair_mossycobble", "stairs:stair_stonebrick"});
	MOSSY_COBBLESTONE_STAIRS = g("mossy_cobblestone_stairs", "stairs:stair_mossycobble");
	DEEPSLATE_BRICK_STAIRS = prefer_arnis("deepslate_brick_stairs", STONE_BRICK_STAIRS);
	POLISHED_DEEPSLATE_STAIRS =
			prefer_arnis("polished_deepslate_stairs", STONE_BRICK_STAIRS);
	RED_NETHER_BRICKS = prefer_arnis("red_nether_bricks", NETHER_BRICK);
	SPRUCE_STAIRS = g("spruce_stairs", "stairs:stair_pine_wood");
	DARK_OAK_STAIRS = prefer_arnis("dark_oak_stairs", OAK_STAIRS);
	RED_NETHER_BRICK_STAIRS =
			prefer_arnis("red_nether_brick_stairs", NETHER_BRICK_STAIRS);
	WAXED_OXIDIZED_CUT_COPPER_STAIRS = g("waxed_oxidized_cut_copper_stairs",
			{"stairs:stair_copperblock", "stairs:stair_stonebrick"});
	WAXED_OXIDIZED_COPPER =
			g("waxed_oxidized_copper", {"default:copperblock", "default:stone"});
	ANDESITE_STAIRS = prefer_arnis("andesite_stairs", STONE_BRICK_STAIRS);
	WAXED_EXPOSED_CUT_COPPER_STAIRS = g("waxed_exposed_cut_copper_stairs",
			{"stairs:stair_copperblock", "stairs:stair_stonebrick"});
	WHITE_WALL_BANNER = prefer_arnis("white_wall_banner", WHITE_WOOL);
	BLUE_WALL_BANNER = prefer_arnis("blue_wall_banner", BLUE_WOOL);
	BLACK_WALL_BANNER = prefer_arnis("black_wall_banner", BLACK_CONCRETE);
	RED_WALL_BANNER = prefer_arnis("red_wall_banner", RED_WOOL);
	GREEN_WALL_BANNER = prefer_arnis("green_wall_banner", GREEN_WOOL);
	MOSSY_STONE_BRICKS =
			g("mossy_stone_bricks", {"default:mossycobble", "default:stonebrick"});
	DEEPSLATE = prefer_arnis("deepslate", STONE);
	TUFF = prefer_arnis("tuff", STONE);
	COBBLED_DEEPSLATE = prefer_arnis("cobbled_deepslate", COBBLESTONE);
	WATER_CAULDRON = prefer_arnis("cauldron", CAULDRON);
	WAXED_COPPER_BLOCK =
			g("waxed_copper_block", {"default:copperblock", "default:stone"});
	WAXED_EXPOSED_COPPER =
			g("waxed_exposed_copper", {"default:copperblock", "default:stone"});
	WAXED_EXPOSED_CHISELED_COPPER =
			g("waxed_exposed_chiseled_copper", {"default:copperblock", "default:stone"});
	WAXED_EXPOSED_CUT_COPPER =
			g("waxed_exposed_cut_copper", {"default:copperblock", "default:stone"});
	MANGROVE_LOG = g("mangrove_log", {"mcl_core:mangrove_log", "default:tree"});
	MANGROVE_LEAVES =
			g("mangrove_leaves", {"mcl_core:mangrove_leaves", "default:leaves"});
	CHERRY_LOG = g("cherry_log", {"default:aspen_tree", "default:tree"});
	CHERRY_LEAVES = g("cherry_leaves", {"default:aspen_leaves", "default:leaves"});
	GRAY_CONCRETE_POWDER =
			g("gray_concrete_powder", {"default:gravel", "mcl_core:gravel", "dye:grey"});
	LIGHT_GRAY_CONCRETE_POWDER = g("light_gray_concrete_powder",
			{"default:gravel", "mcl_core:gravel", "dye:light_grey", "dye:light_gray",
					"dye:silver"});
	BROWN_CONCRETE_POWDER = g(
			"brown_concrete_powder", {"default:gravel", "mcl_core:gravel", "dye:brown"});
	CYAN_TERRACOTTA = prefer_arnis("cyan_terracotta", CYAN_CONCRETE);
	BLACK_WOOL = g("black_wool", "wool:black");
	LIGHT_GRAY_WALL_BANNER = optional_g("light_gray_wall_banner", GRAY_WOOL,
			{"mcl_banners:light_gray_wall_banner", "mcl_banners:light_grey_wall_banner",
					"default:wall_sign"});

	// Keep dedicated Rust palette entries distinct where the host game has a
	// matching node. These used to be collapsed to generic material substitutes,
	// losing schematic and structure material fidelity.
	CHISELED_POLISHED_BLACKSTONE = g("chiseled_polished_blackstone",
			{"mcl_blackstone:chiseled_polished_blackstone",
					"mcl_blackstone:chiseled_blackstone",
					"mcl_core:polished_blackstone_bricks", "default:obsidianbrick"});
	CHISELED_DEEPSLATE = g("chiseled_deepslate",
			{"mcl_deepslate:chiseled_deepslate", "mcl_deepslate:deepslate_bricks",
					"default:stonebrick"});
	COAL_BLOCK = g("coal_block", {"mcl_core:coalblock", "mcl_core:coal_block",
										 "default:coalblock", "default:stone"});
	COBBLESTONE_SLAB = g("cobblestone_slab",
			{"mcl_stairs:slab_cobble", "stairs:slab_cobble", "default:cobble"});
	DARK_PRISMARINE = g("dark_prismarine",
			{"mcl_ocean:dark_prismarine", "mcl_ocean:prismarine_dark",
					"mcl_ocean:prismarine", "default:coral_skeleton", "default:stone"});
	DARK_PRISMARINE_SLAB = g("dark_prismarine_slab",
			{"mcl_stairs:slab_dark_prismarine", "mcl_stairs:slab_prismarine_dark",
					"stairs:slab_stone", "default:stone"});
	DARK_PRISMARINE_STAIRS = g("dark_prismarine_stairs",
			{"mcl_stairs:stair_dark_prismarine", "mcl_stairs:stair_prismarine_dark",
					"stairs:stair_stone", "default:stone"});
	IRON_DOOR = g("iron_door", {"mcl_doors:door_iron_a", "doors:door_steel_a",
									   "doors:door_iron_a", "default:steelblock"});
	LODESTONE = g("lodestone", {"mcl_nether:lodestone", "mcl_core:lodestone",
									   "default:steelblock", "default:stone"});
	NETHER_BRICK_FENCE = g("nether_brick_fence",
			{"mcl_walls:nether_brick_fence", "mcl_nether:nether_brick_fence",
					"default:obsidianbrick"});
	NETHER_BRICK_WALL = g("nether_brick_wall",
			{"mcl_walls:nether_brick_wall", "mcl_nether:nether_brick_wall",
					"default:obsidianbrick"});
	NETHER_WART_BLOCK = g("nether_wart_block",
			{"mcl_nether:nether_wart_block", "mcl_core:nether_wart_block", "default:wood",
					"default:stone"});
	BLACKSTONE_SLAB = g("blackstone_slab",
			{"mcl_stairs:slab_blackstone", "mcl_stairs:slab_polished_blackstone",
					"stairs:slab_obsidianbrick", "default:obsidianbrick"});
	POWERED_RAIL =
			g("powered_rail", {"mcl_minecarts:golden_rail", "mcl_minecarts:powered_rail",
									  "carts:powerrail", "carts:rail", "default:rail"});
	WAXED_EXPOSED_CUT_COPPER_SLAB = g("waxed_exposed_cut_copper_slab",
			{"mcl_stairs:slab_waxed_exposed_cut_copper", "mcl_stairs:slab_copper",
					"stairs:slab_copperblock", "default:copperblock"});

	ANDESITE_SLAB =
			g("andesite_slab", {"mcl_stairs:slab_andesite", "stairs:slab_andesite",
									   "stairs:slab_stone", "default:stone"});
	BAMBOO_SLAB = g("bamboo_slab",
			{"mcl_stairs:slab_bamboo", "mcl_bamboo:slab_bamboo", "stairs:slab_bamboo",
					"stairs:slab_wood", "default:wood"});
	BAMBOO_STAIRS = g("bamboo_stairs",
			{"mcl_stairs:stair_bamboo", "mcl_bamboo:stair_bamboo", "stairs:stair_bamboo",
					"stairs:stair_wood", "default:wood"});
	BIRCH_BUTTON =
			g("birch_button", {"mcl_buttons:birch_button_off", "mcl_buttons:birch_button",
									  "mesecons_button:button_off", "default:wood"});
	BIRCH_DOOR = g("birch_door", {"mcl_doors:door_birch_a", "doors:door_birch_a",
										 "doors:door_wood_a", "default:wood"});
	BIRCH_FENCE = g("birch_fence",
			{"mcl_fences:birch_fence", "fences:birch_fence", "default:fence_wood"});
	BIRCH_PRESSURE_PLATE = g("birch_pressure_plate",
			{"mcl_pressureplates:birch_off",
					"mesecons_pressureplates:pressure_plate_wood_off", "default:wood"});
	BLACKSTONE_STAIRS = g("blackstone_stairs",
			{"mcl_stairs:stair_blackstone", "mcl_stairs:stair_polished_blackstone",
					"stairs:stair_obsidianbrick", "default:obsidianbrick"});
	BLACKSTONE_WALL =
			g("blackstone_wall", {"mcl_walls:blackstone", "mcl_walls:polished_blackstone",
										 "default:obsidianbrick"});
	CHERRY_PLANKS =
			g("cherry_planks", {"mcl_core:cherry_planks", "mcl_trees:cherry_planks",
									   "default:aspen_wood", "default:wood"});
	CHERRY_SLAB = g("cherry_slab",
			{"mcl_stairs:slab_cherry", "mcl_core:cherry_slab", "stairs:slab_aspen_wood",
					"stairs:slab_wood", "default:wood"});
	CHERRY_STAIRS = g("cherry_stairs",
			{"mcl_stairs:stair_cherry", "mcl_core:cherry_stairs",
					"stairs:stair_aspen_wood", "stairs:stair_wood", "default:wood"});
	COBBLED_DEEPSLATE_SLAB = g("cobbled_deepslate_slab",
			{"mcl_stairs:slab_cobbled_deepslate", "mcl_stairs:slab_cobbled_deepslate",
					"stairs:slab_cobble", "default:cobble"});
	COBBLED_DEEPSLATE_STAIRS = g(
			"cobbled_deepslate_stairs", {"mcl_stairs:stair_cobbled_deepslate",
												"stairs:stair_cobble", "default:cobble"});
	CRIMSON_SLAB =
			g("crimson_slab", {"mcl_stairs:slab_crimson", "mcl_crimson:crimson_slab",
									  "stairs:slab_wood", "default:wood"});
	CRIMSON_STAIRS =
			g("crimson_stairs", {"mcl_stairs:stair_crimson", "mcl_crimson:crimson_stairs",
										"stairs:stair_wood", "default:wood"});
	CUT_SANDSTONE_SLAB = g(
			"cut_sandstone_slab", {"mcl_stairs:slab_cut_sandstone",
										  "stairs:slab_sandstone", "default:sandstone"});
	CYAN_CARPET =
			g("cyan_carpet", {"mcl_wool:cyan_carpet", "wool:cyan", "default:stone"});
	DEEPSLATE_TILES = g("deepslate_tiles",
			{"mcl_deepslate:deepslate_tiles", "mcl_deepslate:deepslate_tile",
					"mcl_deepslate:deepslate_bricks", "default:stonebrick"});
	DEEPSLATE_TILE_SLAB = g("deepslate_tile_slab",
			{"mcl_stairs:slab_deepslate_tiles", "mcl_stairs:slab_deepslate_tile",
					"mcl_stairs:slab_deepslate_brick", "stairs:slab_stonebrick"});
	DEEPSLATE_TILE_WALL = g("deepslate_tile_wall",
			{"mcl_walls:deepslate_tile", "mcl_walls:deepslate_tiles",
					"mcl_walls:deepslate_brick", "default:stonebrick"});
	DIORITE_STAIRS =
			g("diorite_stairs", {"mcl_stairs:stair_diorite", "stairs:stair_diorite",
										"stairs:stair_stone", "default:stone"});
	DIORITE_WALL = g("diorite_wall",
			{"mcl_walls:diorite", "mcl_walls:diorite_wall", "default:stone"});
	END_STONE_BRICK_SLAB = g("end_stone_brick_slab",
			{"mcl_stairs:slab_end_stone_brick", "mcl_stairs:slab_end_stone_bricks",
					"stairs:slab_stonebrick", "default:stonebrick"});
	END_STONE_BRICK_WALL = g("end_stone_brick_wall",
			{"mcl_walls:end_stone_brick", "mcl_walls:end_stone_bricks",
					"default:stonebrick"});
	JUNGLE_SLAB = g("jungle_slab", {"mcl_stairs:slab_jungle", "stairs:slab_junglewood",
										   "stairs:slab_wood", "default:wood"});
	JUNGLE_STAIRS =
			g("jungle_stairs", {"mcl_stairs:stair_jungle", "stairs:stair_junglewood",
									   "stairs:stair_wood", "default:wood"});
	ACACIA_TRAPDOOR =
			g("acacia_trapdoor", {"mcl_doors:trapdoor_acacia", "doors:trapdoor_acacia",
										 "doors:trapdoor", "default:wood"});
	BIRCH_FENCE_GATE = g("birch_fence_gate",
			{"mcl_fences:birch_fence_gate", "fences:gate_birch", "default:fence_wood"});
	BLACK_STAINED_GLASS = g("black_stained_glass",
			{"mcl_core:black_stained_glass", "mcl_glass:stained_glass_black",
					"default:glass"});
	BLUE_STAINED_GLASS_PANE = g("blue_stained_glass_pane",
			{"mcl_core:blue_stained_glass_pane", "mcl_glass:stained_glass_pane_blue",
					"xpanes:blue_flat", "default:glass"});
	DARK_OAK_BUTTON =
			g("dark_oak_button", {"mcl_buttons:dark_oak_button_off",
										 "mesecons_button:button_off", "default:wood"});
	DARK_OAK_FENCE_GATE = g(
			"dark_oak_fence_gate", {"mcl_fences:dark_oak_fence_gate",
										   "fences:gate_dark_oak", "default:fence_wood"});
	DARK_OAK_PRESSURE_PLATE = g("dark_oak_pressure_plate",
			{"mcl_pressureplates:dark_oak_off",
					"mesecons_pressureplates:pressure_plate_wood_off", "default:wood"});
	GRANITE_STAIRS =
			g("granite_stairs", {"mcl_stairs:stair_granite", "stairs:stair_granite",
										"stairs:stair_stone", "default:stone"});
	GRAY_STAINED_GLASS_PANE = g("gray_stained_glass_pane",
			{"mcl_core:gray_stained_glass_pane", "mcl_core:grey_stained_glass_pane",
					"mcl_glass:stained_glass_pane_gray", "xpanes:grey_flat",
					"default:glass"});
	GRAY_WALL_BANNER = g("gray_wall_banner",
			{"mcl_banners:gray_wall_banner", "mcl_banners:grey_wall_banner",
					"default:wall_sign"});
	IRON_TRAPDOOR = g("iron_trapdoor", {"mcl_doors:trapdoor_iron", "doors:trapdoor_steel",
											   "doors:trapdoor", "default:steelblock"});
	JUNGLE_FENCE = g("jungle_fence",
			{"mcl_fences:jungle_fence", "fences:jungle_fence", "default:fence_wood"});
	JUNGLE_TRAPDOOR =
			g("jungle_trapdoor", {"mcl_doors:trapdoor_jungle", "doors:trapdoor_jungle",
										 "doors:trapdoor", "default:wood"});
	MOSSY_COBBLESTONE_SLAB = g("mossy_cobblestone_slab",
			{"mcl_stairs:slab_mossycobble", "stairs:slab_mossycobble",
					"default:mossycobble"});
	MOSSY_STONE_BRICK_SLAB = g("mossy_stone_brick_slab",
			{"mcl_stairs:slab_mossystonebrick", "stairs:slab_mossystonebrick",
					"default:mossycobble"});
	MOSSY_STONE_BRICK_WALL = g("mossy_stone_brick_wall",
			{"mcl_walls:mossystonebrick", "mcl_walls:mossy_stone_brick",
					"default:mossycobble"});
	OAK_BUTTON = g("oak_button",
			{"mcl_buttons:oak_button_off", "mesecons_button:button_off", "default:wood"});
	OAK_FENCE_GATE = g("oak_fence_gate",
			{"mcl_fences:oak_fence_gate", "fences:gate_wood", "default:fence_wood"});
	PALE_OAK_TRAPDOOR = g("pale_oak_trapdoor",
			{"mcl_doors:trapdoor_pale_oak", "mcl_doors:trapdoor_birch", "doors:trapdoor",
					"default:wood"});
	POLISHED_ANDESITE_SLAB = g("polished_andesite_slab",
			{"mcl_stairs:slab_polished_andesite", "stairs:slab_stone", "default:stone"});
	POLISHED_BLACKSTONE_BUTTON = g("polished_blackstone_button",
			{"mcl_buttons:polished_blackstone_button_off", "mesecons_button:button_off",
					"default:obsidianbrick"});
	POLISHED_BLACKSTONE_PRESSURE_PLATE = g("polished_blackstone_pressure_plate",
			{"mcl_pressureplates:polished_blackstone_off",
					"mesecons_pressureplates:pressure_plate_stone_off",
					"default:obsidianbrick"});
	POLISHED_DEEPSLATE_SLAB = g("polished_deepslate_slab",
			{"mcl_stairs:slab_polished_deepslate", "stairs:slab_stonebrick",
					"default:stonebrick"});
	POLISHED_DEEPSLATE_WALL = g("polished_deepslate_wall",
			{"mcl_walls:polished_deepslate", "mcl_walls:polished_deepslate_wall",
					"default:stonebrick"});
	POLISHED_DIORITE_SLAB = g("polished_diorite_slab",
			{"mcl_stairs:slab_polished_diorite", "stairs:slab_stone", "default:stone"});
	PURPUR_SLAB = g("purpur_slab", {"mcl_stairs:slab_purpur", "mcl_end:purpur_slab",
										   "stairs:slab_stone", "default:stone"});
	PURPUR_STAIRS =
			g("purpur_stairs", {"mcl_stairs:stair_purpur", "mcl_end:purpur_stairs",
									   "stairs:stair_stone", "default:stone"});
	QUARTZ_PILLAR = g("quartz_pillar",
			{"mcl_nether:quartz_pillar", "mcl_core:quartz_pillar", "default:stone"});
	REDSTONE_TORCH = g("redstone_torch",
			{"mcl_redstone:redstone_torch_on", "mesecons_torch:redstone_torch_on",
					"default:torch"});
	REDSTONE_WALL_TORCH = g("redstone_wall_torch",
			{"mcl_redstone:redstone_torch_wall_on",
					"mesecons_torch:redstone_torch_wall_on", "default:torch"});
	RED_NETHER_BRICK_SLAB = g("red_nether_brick_slab",
			{"mcl_stairs:slab_red_nether_brick", "stairs:slab_obsidianbrick",
					"default:obsidianbrick"});
	SANDSTONE_WALL = g("sandstone_wall",
			{"mcl_walls:sandstone", "mcl_walls:sandstone_wall", "default:sandstone"});
	SMOOTH_QUARTZ_SLAB = g("smooth_quartz_slab",
			{"mcl_stairs:slab_smooth_quartz", "stairs:slab_stone", "default:stone"});
	SMOOTH_QUARTZ_STAIRS = g("smooth_quartz_stairs",
			{"mcl_stairs:stair_smooth_quartz", "stairs:stair_stone", "default:stone"});
	SMOOTH_RED_SANDSTONE_SLAB = g("smooth_red_sandstone_slab",
			{"mcl_stairs:slab_smooth_red_sandstone", "mcl_stairs:slab_red_sandstone",
					"stairs:slab_sandstone", "default:sandstone"});
	SPRUCE_BUTTON =
			g("spruce_button", {"mcl_buttons:spruce_button_off",
									   "mesecons_button:button_off", "default:wood"});
	SPRUCE_FENCE_GATE = g("spruce_fence_gate",
			{"mcl_fences:spruce_fence_gate", "fences:gate_pine_wood",
					"default:fence_pine_wood", "default:fence_wood"});
	SPRUCE_WALL_SIGN = g("spruce_wall_sign",
			{"mcl_signs:wall_sign_spruce", "signs:sign_wall", "default:sign_wall"});
	STONE_BUTTON =
			g("stone_button", {"mcl_buttons:stone_button_off",
									  "mesecons_button:button_off", "default:stone"});
	STONE_PRESSURE_PLATE = g("stone_pressure_plate",
			{"mcl_pressureplates:stone_off",
					"mesecons_pressureplates:pressure_plate_stone_off", "default:stone"});
	STONE_STAIRS = g("stone_stairs",
			{"mcl_stairs:stair_stone", "stairs:stair_stone", "default:stone"});
	TRIPWIRE_HOOK =
			g("tripwire_hook", {"mcl_redstone:tripwire_hook", "mcl_core:tripwire_hook",
									   "default:steelblock"});
	// Content IDs are local to a NodeDefManager. A process may open another
	// world/game without restarting, so remember which registry these mappings
	// belong to instead of treating initialization as process-global.
	mapped_node_def_manager = node_def_manager;
}

}
