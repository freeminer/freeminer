#pragma once

#include <array>
#include <vector>
#include <span>
#include <utility>
#include <random>
#include <optional>
#include <functional>


#include "../../arnis_adapter.h"

using namespace arnis;

namespace arnis
{


struct Coord { int x; int y; int z; };

static constexpr std::array<Coord, 8> ROUND1_PATTERN = {{
    { -2, 0, 0 },
    {  2, 0, 0 },
    {  0, 0, -2 },
    {  0, 0,  2 },
    { -1, 0, -1 },
    {  1, 0,  1 },
    {  1, 0, -1 },
    { -1, 0,  1 },
}};

static constexpr std::array<Coord, 12> ROUND2_PATTERN = {{
    {  3, 0,  0 },
    {  2, 0, -1 },
    {  2, 0,  1 },
    {  1, 0, -2 },
    {  1, 0,  2 },
    { -3, 0,  0 },
    { -2, 0, -1 },
    { -2, 0,  1 },
    { -1, 0,  2 },
    { -1, 0, -2 },
    {  0, 0, -3 },
    {  0, 0,  3 },
}};

static constexpr std::array<Coord, 12> ROUND3_PATTERN = {{
    {  3, 0, -1 },
    {  3, 0,  1 },
    {  2, 0, -2 },
    {  2, 0,  2 },
    {  1, 0, -3 },
    {  1, 0,  3 },
    { -3, 0, -1 },
    { -3, 0,  1 },
    { -2, 0, -2 },
    { -2, 0,  2 },
    { -1, 0,  3 },
    { -1, 0, -3 },
}};

static const std::array<std::span<const Coord>, 3> ROUND_PATTERNS = {
    std::span<const Coord>(ROUND1_PATTERN),
    std::span<const Coord>(ROUND2_PATTERN),
    std::span<const Coord>(ROUND3_PATTERN)
};

static const std::array<std::pair<Coord, Coord>, 5> OAK_LEAVES_FILL = {{
    { { -1, 3,0 }, { -1, 9, 0 } },
    { {  1, 3, 0 }, {  1, 9, 0 } },
    { {  0, 3, -1 }, {  0, 9, -1 } },
    { {  0, 3, 1 }, {  0, 9, 1 } },
    { {  0, 9, 0 }, {  0, 10, 0 } },
}};

static const std::array<std::pair<Coord, Coord>, 6> SPRUCE_LEAVES_FILL = {{
    { { -1, 3, 0 }, { -1, 10, 0 } },
    { {  0, 3, -1 }, {  0, 10, -1 } },
    { {  1, 3, 0 }, {  1, 10, 0 } },
    { {  0, 3, -1 }, {  0, 10, -1 } },
    { {  0, 3, 1 }, {  0, 10, 1 } },
    { {  0, 11, 0 }, {  0, 11, 0 } },
}};

static const std::array<std::pair<Coord, Coord>, 5> BIRCH_LEAVES_FILL = {{
    { { -1, 2, 0 }, { -1, 7, 0 } },
    { {  1, 2, 0 }, {  1, 7, 0 } },
    { {  0, 2, -1 }, {  0, 7, -1 } },
    { {  0, 2, 1 }, {  0, 7, 1 } },
 { {  0, 7, 0 }, {  0, 8, 0 } },
}};

enum class TreeType { Oak = 0, Spruce = 1, Birch = 2 };

struct Tree {
    Block log_block;
    int log_height;
    Block leaves_block;
    std::span<const std::pair<Coord, Coord>> leaves_fill;
    std::array<std::vector<int>, 3> round_ranges;

    static void round(WorldEditor& editor, Block material, const Coord& center, std::span<const Coord> block_pattern) {
        for (const Coord& d : block_pattern) {
            editor.set_block(material, center.x + d.x, center.y + d.y, center.z + d.z, std::nullopt, std::nullopt);
        }
    }

    static Tree get_tree(TreeType kind) {
        switch (kind) {
            case TreeType::Oak: {
                Tree t;                t.log_block = OAK_LOG;
                t.log_height = 8;

 t.leaves_block = OAK_LEAVES;
                t.leaves_fill = std::span<const std::pair<Coord, Coord>>(OAK_LEAVES_FILL);
                t.round_ranges[0].reserve(6);
                for (int v = 8; v >= 3; --v) t.round_ranges[0].push_back(v);
                t.round_ranges[1].reserve(4);
                for (int v = 7; v >= 4; --v) t.round_ranges[1].push_back(v);
                t.round_ranges[2].reserve(2);
                for (int v = 6; v >= 5; --v) t.round_ranges[2].push_back(v);
                return t;
            }

            case TreeType::Spruce: {
                Tree t;
                t.log_block = SPRUCE_LOG;
                t.log_height = 9;
                t.leaves_block = BIRCH_LEAVES; // TODO: mirrored from original
                t.leaves_fill = std::span<const std::pair<Coord, Coord>>(SPRUCE_LEAVES_FILL);
                t.round_ranges[0] = std::vector<int>{9, 7, 6, 4, 3};
                t.round_ranges[1] = std::vector<int>{6, 3};
                t.round_ranges[2] = std::vector<int>{};
                return t;
            }

            case TreeType::Birch: {
                Tree t;
                t.log_block = BIRCH_LOG;
                t.log_height = 6;
                t.leaves_block = BIRCH_LEAVES;
                t.leaves_fill = std::span<const std::pair<Coord, Coord>>(BIRCH_LEAVES_FILL);
 for (int v = 6; v >= 2; --v) t.round_ranges[0].push_back(v);
                for (int v = 2; v <= 4; ++v) t.round_ranges[1].push_back(v);
                t.round_ranges[2] = std::vector<int>{};
                return t;
            }
        }
        // fallback (should not happen)
        return Tree{};
    }

    static std::vector<Block> get_building_wall_blocks() {
        return std::vector<Block>{
            BLACKSTONE,
            BLACK_TERRACOTTA,
            BRICK,
            BROWN_CONCRETE,
            BROWN_TERRACOTTA,
            DEEPSLATE_BRICKS,
            END_STONE_BRICKS,
            GRAY_CONCRETE,
            GRAY_TERRACOTTA,
            LIGHT_BLUE_TERRACOTTA,
            LIGHT_GRAY_CONCRETE,
            MUD_BRICKS,
            NETHER_BRICK,
            NETHERITE_BLOCK,
            POLISHED_ANDESITE,
            POLISHED_BLACKSTONE,
            POLISHED_BLACKSTONE_BRICKS,
            POLISHED_DEEPSLATE,
            POLISHED_GRANITE,
            QUARTZ_BLOCK,
            QUARTZ_BRICKS,
            SANDSTONE,
            SMOOTH_SANDSTONE,
            SMOOTH_STONE,
            STONE_BRICKS,
            WHITE_CONCRETE,
            WHITE_TERRACOTTA,
            ORANGE_TERRACOTTA,
            GREEN_STAINED_HARDENED_CLAY,
            BLUE_TERRACOTTA,
            YELLOW_TERRACOTTA,
            BLACK_CONCRETE,
            WHITE_CONCRETE,
            GRAY_CONCRETE,
            LIGHT_GRAY_CONCRETE,
            BROWN_CONCRETE,
            RED_CONCRETE,
            ORANGE_TERRACOTTA,
            YELLOW_CONCRETE,
            LIME_CONCRETE,
            GREEN_STAINED_HARDENED_CLAY,
            CYAN_CONCRETE,
            LIGHT_BLUE_CONCRETE,
            BLUE_CONCRETE,
            PURPLE_CONCRETE,
            MAGENTA_CONCRETE,
            RED_TERRACOTTA,
        };
    }

    static std::vector<Block> get_building_floor_blocks() {
        return std::vector<Block>{
            GRAY_CONCRETE,
            LIGHT_GRAY_CONCRETE,
            WHITE_CONCRETE,
            SMOOTH_STONE,
            POLISHED_ANDESITE,
            STONE_BRICKS,
        };
    }

    static std::vector<Block> get_structural_blocks() {
        return std::vector<Block>{
            // Fences
            OAK_FENCE,
            // Walls
            COBBLESTONE_WALL,
            ANDESITE_WALL,
            STONE_BRICK_WALL,
            // Stairs
            OAK_STAIRS,
            // Slabs
            OAK_SLAB,
            STONE_BLOCK_SLAB,
            STONE_BRICK_SLAB,
            // Rails
            RAIL,
            RAIL_NORTH_SOUTH,
            RAIL_EAST_WEST,
            RAIL_ASCENDING_EAST,
            RAIL_ASCENDING_WEST,
            RAIL_ASCENDING_NORTH,
            RAIL_ASCENDING_SOUTH,
            RAIL_NORTH_EAST,
            RAIL_NORTH_WEST,
            RAIL_SOUTH_EAST,
            RAIL_SOUTH_WEST,
            // Doors and trapdoors
            OAK_DOOR,
            DARK_OAK_DOOR_LOWER,
            DARK_OAK_DOOR_UPPER,
            OAK_TRAPDOOR,
            // Ladders
            LADDER,
        };
    }

    static std::vector<Block> get_functional_blocks() {
        return std::vector<Block>{
            // Furniture and functional blocks
            CHEST,
            CRAFTING_TABLE,            FURNACE,
            ANVIL,
            BREWING_STAND,
            NOTE_BLOCK,

 BOOKSHELF,
            CAULDRON,
            // Beds
            RED_BED_NORTH_HEAD,
            RED_BED_NORTH_FOOT,
            RED_BED_EAST_HEAD,
            RED_BED_EAST_FOOT,
            RED_BED_SOUTH_HEAD,
            RED_BED_SOUTH_FOOT,
            RED_BED_WEST_HEAD,
            RED_BED_WEST_FOOT,
            // Pressure plates and signs
            OAK_PRESSURE_PLATE,
            SIGN,

 // Glass blocks (windows)
            GLASS,
            WHITE_STAINED_GLASS,
            GRAY_STAINED_GLASS,
            LIGHT_GRAY_STAINED_GLASS,
            BROWN_STAINED_GLASS,
            TINTED_GLASS,
            // Carpets
            WHITE_CARPET,
            RED_CARPET,
            // Other structural/building blocks
            IRON_BARS,
            IRON_BLOCK,
           SCAFFOLDING,
            BEDROCK,
        };
    }

    
    static void create(WorldEditor& editor, const Coord& pos) {
        std::vector<Block> blacklist;
        auto bw = get_building_wall_blocks();
        blacklist.insert(blacklist.end(), bw.begin(), bw.end());
        auto bf = get_building_floor_blocks();
        blacklist.insert(blacklist.end(), bf.begin(), bf.end());
        auto sb = get_structural_blocks();
        blacklist.insert(blacklist.end(), sb.begin(), sb.end());
        auto fb = get_functional_blocks();
        blacklist.insert(blacklist.end(), fb.begin(), fb.end());
        blacklist.push_back(WATER);

        std::random_device rd;
        std::mt19937 gen(rd());
        std::uniform_int_distribution<int> dist(1, 3);
        int pick = dist(gen);

        TreeType chosen = TreeType::Oak;
        if (pick == 1) chosen = TreeType::Oak;
        else if (pick == 2) chosen = TreeType::Spruce;
        else if (pick == 3) chosen = TreeType::Birch;

        Tree tree = get_tree(chosen);

        // Build the logs
        editor.fill_blocks(
            tree.log_block,
            pos.x,
           pos.y,
            pos.z,
            pos.x,
            pos.y + tree.log_height,
            pos.z,
            std::nullopt,
            std::optional<std::reference_wrapper<const std::vector<Block>>>(std::cref(blacklist))
        );

        // Fill in the leaves
        for (const auto& pr : tree.leaves_fill) {
            const Coord& a = pr.first;
            const Coord& b = pr.second;
            editor.fill_blocks(
                tree.leaves_block,
                pos.x + a.x,                pos.y + a.y,
                pos.z + a.z,
                pos.x + b.x,

 pos.y + b.y,
                pos.z + b.z,
                std::nullopt,
                std::nullopt
            );
        }

        // Do the three rou
        for (std::size_t idx = 0; idx < tree.round_ranges.size(); ++idx) {
            const std::vector<int>& range = tree.round_ranges[idx];
            std::span<const Coord> pattern = ROUND_PATTERNS[idx];
            for (int offset : range) {
                Coord center { pos.x, pos.y + offset, pos.z };
                round(editor, tree.leaves_block, center, pattern);
            }
        }
    }
    
    static void create(WorldEditor& editor, const std::tuple<int,int,int>& pos) {
        return create(editor,Coord{std::get<0>(pos), std::get<1>(pos), std::get<2>(pos)});
    }

};

}
