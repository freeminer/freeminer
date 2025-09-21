#include <string>
#include <tuple>
#include <utility>
#include <optional>
#include <random>
#include <algorithm>

// Project headers
#include "args.h"
#include "block_definitions.h"
#include "bresenham.h"
#include "../element_processing/tree.h"
#include "floodfill.h"
#include "osm_parser.h"
#include "world_editor.h"
#include "../../arnis_adapter.h"
namespace arnis
{

namespace leisure
{


void generate_leisure(WorldEditor& editor, const ProcessedWay& element, const Args& args) {
    auto leisure_it = element.tags.find(std::string("leisure"));
    if (leisure_it != element.tags.end()) {
        const std::string& leisure_type = leisure_it->second;
        std::optional<std::pair<int, int>> previous_node = std::nullopt;
        std::tuple<int, int, int> corner_addup = std::make_tuple(0, 0, 0);
        std::vector<std::pair<int, int>> current_leisure;

        Block block_type = GRASS_BLOCK;
        if (leisure_type == "park" || leisure_type == "nature_reserve" || leisure_type == "garden" ||
            leisure_type == "disc_golf_course" || leisure_type == "golf_course") {
            block_type = GRASS_BLOCK;
        } else if (leisure_type == "schoolyard") {
            block_type = BLACK_CONCRETE;
        } else if (leisure_type == "playground" || leisure_type == "recreation_ground" ||
                   leisure_type == "pitch" || leisure_type == "beach_resort" || leisure_type == "dog_park") {
            auto surf_it = element.tags.find(std::string("surface"));
            if (surf_it != element.tags.end()) {
                const std::string& surface = surf_it->second;
                if (surface == "clay") {
                    block_type = TERRACOTTA;
                } else if (surface == "sand") {
                    block_type = SAND;
                } else if (surface == "tartan") {
                    block_type = RED_TERRACOTTA;
                } else if (surface == "grass") {
                    block_type = GRASS_BLOCK;
                } else if (surface == "dirt") {
                    block_type = DIRT;
                } else if (surface == "pebblestone" || surface == "cobblestone" || surface == "unhewn_cobblestone") {
                    block_type = COBBLESTONE;
                } else {
                    block_type = GREEN_STAINED_HARDENED_CLAY;
                }
            } else {
                block_type = GREEN_STAINED_HARDENED_CLAY;
            }
        } else if (leisure_type == "swimming_pool" || leisure_type == "swimming_area") {
            block_type = WATER;
        } else if (leisure_type == "bathing_place") {
            block_type = SMOOTH_SANDSTONE;
        } else if (leisure_type == "outdoor_seating") {
            block_type = SMOOTH_STONE;
        } else if (leisure_type == "water_park" || leisure_type == "slipway") {
            block_type = LIGHT_GRAY_CONCRETE;
        } else if (leisure_type == "ice_rink") {
            block_type = PACKED_ICE;
        } else {
            block_type = GRASS_BLOCK;
        }

        for (const ProcessedNode& node : element.nodes) {
            if (previous_node.has_value()) {
                std::pair<int,int> prev = previous_node.value();
                std::vector<std::tuple<int,int,int>> bresenham_points =
                    bresenham_line(prev.first, 0, prev.second, node.x, 0, node.z);
                for (const std::tuple<int,int,int>& t : bresenham_points) {
                    int bx = std::get<0>(t);
                    int bz = std::get<2>(t);
                    editor.set_block(block_type, bx, 0, bz,
                                     std::optional<std::vector<Block>>({
                                         GRASS_BLOCK, STONE_BRICKS, SMOOTH_STONE,
                                         LIGHT_GRAY_CONCRETE, COBBLESTONE, GRAY_CONCRETE
                                     }),
                                     std::nullopt);
                }

                current_leisure.push_back(std::make_pair(node.x, node.z));
                std::get<0>(corner_addup) += node.x;
                std::get<1>(corner_addup) += node.z;
                std::get<2>(corner_addup) += 1;
            }
            previous_node = std::make_pair(node.x, node.z);
        }

        if (corner_addup != std::make_tuple(0, 0, 0)) {
            std::vector<std::pair<int,int>> polygon_coords;
            polygon_coords.reserve(element.nodes.size());
            for (const ProcessedNode& n : element.nodes) {
                polygon_coords.push_back(std::make_pair(n.x, n.z));
            }

            auto timeout_opt = args.timeout;
            std::vector<std::pair<int,int>> filled_area = flood_fill_area(polygon_coords, timeout_opt);

            for (const std::pair<int,int>& p : filled_area) {
                int x = p.first;
                int z = p.second;
                editor.set_block(block_type, x, 0, z, std::optional<std::vector<Block>>({GRASS_BLOCK}), std::nullopt);

                if ((leisure_type == "park" || leisure_type == "garden" || leisure_type == "nature_reserve") &&
                    editor.check_for_block(x, 0, z, std::optional<std::vector<Block>>({GRASS_BLOCK}))) {

                    std::random_device rd;
                    std::mt19937 rng(rd());
                    std::uniform_int_distribution<int> dist(0, 999);
                    int random_choice = dist(rng);

                    if (random_choice >= 0 && random_choice < 30) {
                        Block flower_choice = WHITE_FLOWER;
                        if (random_choice < 10) {
                            flower_choice = RED_FLOWER;
                        } else if (random_choice < 20) {
                            flower_choice = YELLOW_FLOWER;
                        } else {
                            flower_choice = BLUE_FLOWER;
                        }
                        editor.set_block(flower_choice, x, 1, z, std::nullopt, std::nullopt);
                    } else if (random_choice >= 30 && random_choice < 90) {
                        editor.set_block(GRASS, x, 1, z, std::nullopt, std::nullopt);
                    } else if (random_choice >= 90 && random_choice < 105) {
                        editor.set_block(OAK_LEAVES, x, 1, z, std::nullopt, std::nullopt);
                    } else if (random_choice >= 105 && random_choice < 120) {
                        Tree::create(editor, std::make_tuple(x, 1, z));
                    }
                }

                if (leisure_type == "playground" || leisure_type == "recreation_ground") {
                    std::random_device rd2;
                    std::mt19937 rng2(rd2());
                    std::uniform_int_distribution<int> dist2(0, 4999);
                    int rc = dist2(rng2);

                    if (rc >= 0 && rc < 10) {
                        for (int y = 1; y <= 3; ++y) {
                            editor.set_block(OAK_FENCE, x - 1, y, z, std::nullopt, std::nullopt);
                            editor.set_block(OAK_FENCE, x + 1, y, z, std::nullopt, std::nullopt);
                        }
                        editor.set_block(OAK_PLANKS, x - 1, 4, z, std::nullopt, std::nullopt);
                        editor.set_block(OAK_SLAB, x, 4, z, std::nullopt, std::nullopt);
                        editor.set_block(OAK_PLANKS, x + 1, 4, z, std::nullopt, std::nullopt);
                        editor.set_block(STONE_BLOCK_SLAB, x, 2, z, std::nullopt, std::nullopt);
                    } else if (rc >= 10 && rc < 20) {
                        editor.set_block(OAK_SLAB, x, 1, z, std::nullopt, std::nullopt);
                        editor.set_block(OAK_SLAB, x + 1, 2, z, std::nullopt, std::nullopt);
                        editor.set_block(OAK_SLAB, x + 2, 3, z, std::nullopt, std::nullopt);
                        editor.set_block(OAK_PLANKS, x + 2, 2, z, std::nullopt, std::nullopt);
                        editor.set_block(OAK_PLANKS, x + 2, 1, z, std::nullopt, std::nullopt);
                        editor.set_block(LADDER, x + 2, 2, z - 1, std::nullopt, std::nullopt);
                        editor.set_block(LADDER, x + 2, 1, z - 1, std::nullopt, std::nullopt);
                    } else if (rc >= 20 && rc < 30) {
                        editor.fill_blocks(SAND, x - 3, 0, z - 3, x + 3, 0, z + 3,
                                           std::optional<std::vector<Block>>({GREEN_STAINED_HARDENED_CLAY}),
                                           std::nullopt);
                    }
                }
            }
        }
    }
}

void generate_leisure_from_relation(WorldEditor& editor, const ProcessedRelation& rel, const Args& args) {
    auto leisure_it = rel.tags.find(std::string("leisure"));
    if (leisure_it != rel.tags.end() && leisure_it->second == "park") {
        for (const ProcessedMember& member : rel.members) {
            if (member.role == ProcessedMemberRole::Outer) {
                generate_leisure(editor, member.way, args);
            }
        }

        std::vector<ProcessedNode> combined_nodes;
        for (const ProcessedMember& member : rel.members) {
            if (member.role == ProcessedMemberRole::Outer) {
                combined_nodes.insert(combined_nodes.end(), member.way.nodes.begin(), member.way.nodes.end());
            }
        }

        ProcessedWay combined_way;
        combined_way.id = rel.id;
        combined_way.nodes = std::move(combined_nodes);
        combined_way.tags = rel.tags;

        generate_leisure(editor, combined_way, args);
    }
}

        
}

}