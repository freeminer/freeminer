#include <algorithm>
#include <array>
#include <random>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>
#include <optional>

#include "args.h"
#include "block_definitions.h"
#include "../element_processing/tree.h"
#include "floodfill.h"
#include "osm_parser.h"
#include "world_editor.h"


#include "../../arnis_adapter.h"
namespace arnis
{

namespace landuse
{
using Node = ProcessedNode;



void generate_landuse(WorldEditor & editor, ProcessedWay const & element, Args const & args) {
    const std::string binding = std::string();
    const std::string landuse_tag = [&]() -> std::string {
        auto it = element.tags.find(std::string("landuse"));
        return (it != element.tags.end()) ? it->second : binding;
    }();

    Block block_type = GRASS_BLOCK;
    if (landuse_tag == "greenfield" || landuse_tag == "meadow" || landuse_tag == "grass" || landuse_tag == "orchard" || landuse_tag == "forest") {
        block_type = GRASS_BLOCK;
    } else if (landuse_tag == "farmland") {
        block_type = FARMLAND;
    } else if (landuse_tag == "cemetery") {
        block_type = PODZOL;
    } else if (landuse_tag == "construction") {
        block_type = COARSE_DIRT;
    } else if (landuse_tag == "traffic_island") {
        block_type = STONE_BLOCK_SLAB;
    } else if (landuse_tag == "residential") {
        auto it = element.tags.find(std::string("residential"));
        std::string residential_tag = (it != element.tags.end()) ? it->second : binding;
        if (residential_tag == "rural") {
            block_type = GRASS_BLOCK;
        } else {
            block_type = STONE_BRICKS;
        }
    } else if (landuse_tag == "commercial") {
        block_type = SMOOTH_STONE;
    } else if (landuse_tag == "education" || landuse_tag == "religious") {
        block_type = POLISHED_ANDESITE;
    } else if (landuse_tag == "industrial") {
        block_type = COBBLESTONE;
    } else if (landuse_tag == "military") {
        block_type = GRAY_CONCRETE;
    } else if (landuse_tag == "railway") {
        block_type = GRAVEL;
    } else if (landuse_tag == "landfill") {
        auto it = element.tags.find(std::string("man_made"));
        std::string manmade_tag = (it != element.tags.end()) ? it->second : binding;
        if (manmade_tag == "spoil_heap" || manmade_tag == "heap") {
            block_type = GRAVEL;
        } else {
            block_type = COARSE_DIRT;
        }
    } else if (landuse_tag == "quarry") {
        block_type = STONE;
    } else {
        block_type = GRASS_BLOCK;
    }

    std::vector<std::pair<int, int>> polygon_coords;
    polygon_coords.reserve(element.nodes.size());
    for (auto const & n : element.nodes) {
        polygon_coords.emplace_back(n.x, n.z);
    }

    auto & timeout = args.timeout;
    std::vector<std::pair<int, int>> floor_area = flood_fill_area(polygon_coords, timeout);

    std::mt19937 rng(std::random_device{}());

    for (auto const & coord : floor_area) {
        int x = coord.first;
        int z = coord.second;

        if (landuse_tag == "traffic_island") {
            editor.set_block(block_type, x, 1, z, std::optional<std::vector<Block>>(), std::optional<std::vector<Block>>());
        } else if (landuse_tag == "construction" || landuse_tag == "railway") {
            editor.set_block(block_type, x, 0, z, std::optional<std::vector<Block>>(), std::optional<std::vector<Block>>{ std::vector<Block>{ SPONGE } });
        } else {
            editor.set_block(block_type, x, 0, z, std::optional<std::vector<Block>>(), std::optional<std::vector<Block>>());
        }

        if (landuse_tag == "cemetery") {
            if ((x % 3 == 0) && (z % 3 == 0)) {
                std::uniform_int_distribution<int> dist100(0, 99);
                int random_choice = dist100(rng);
                if (random_choice < 15) {
                    if (editor.check_for_block(x, 0, z, std::optional<std::vector<Block>>{ std::vector<Block>{ PODZOL } })) {
                        std::bernoulli_distribution coin(0.5);
                        if (coin(rng)) {
                            editor.set_block(COBBLESTONE, x - 1, 1, z, std::optional<std::vector<Block>>(), std::optional<std::vector<Block>>());
                            editor.set_block(STONE_BRICK_SLAB, x - 1, 2, z, std::optional<std::vector<Block>>(), std::optional<std::vector<Block>>());
                            editor.set_block(STONE_BRICK_SLAB, x, 1, z, std::optional<std::vector<Block>>(), std::optional<std::vector<Block>>());
                            editor.set_block(STONE_BRICK_SLAB, x + 1, 1, z, std::optional<std::vector<Block>>(), std::optional<std::vector<Block>>());
                        } else {
                            editor.set_block(COBBLESTONE, x, 1, z - 1, std::optional<std::vector<Block>>(), std::optional<std::vector<Block>>());
                            editor.set_block(STONE_BRICK_SLAB, x, 2, z - 1, std::optional<std::vector<Block>>(), std::optional<std::vector<Block>>());
                            editor.set_block(STONE_BRICK_SLAB, x, 1, z, std::optional<std::vector<Block>>(), std::optional<std::vector<Block>>());
                            editor.set_block(STONE_BRICK_SLAB, x, 1, z + 1, std::optional<std::vector<Block>>(), std::optional<std::vector<Block>>());
                        }
                    }
                } else if (random_choice < 30) {
                    if (editor.check_for_block(x, 0, z, std::optional<std::vector<Block>>{ std::vector<Block>{ PODZOL } })) {
                        editor.set_block(RED_FLOWER, x, 1, z, std::optional<std::vector<Block>>(), std::optional<std::vector<Block>>());
                    }
                } else if (random_choice < 33) {
                    Tree::create(editor, std::make_tuple(x, 1, z));
                } else if (random_choice < 35) {
                    editor.set_block(OAK_LEAVES, x, 1, z, std::optional<std::vector<Block>>(), std::optional<std::vector<Block>>());
                }
            }
        } else if (landuse_tag == "forest") {
            if (editor.check_for_block(x, 0, z, std::optional<std::vector<Block>>{ std::vector<Block>{ GRASS_BLOCK } })) {
                std::uniform_int_distribution<int> dist30(0, 29);
                int random_choice = dist30(rng);
                if (random_choice == 20) {
                    Tree::create(editor, std::make_tuple(x, 1, z));
                } else if (random_choice == 2) {
                    std::uniform_int_distribution<int> dist5(1, 5);
                    int pick = dist5(rng);
                    Block flower_block = OAK_LEAVES;
                    if (pick == 2) flower_block = RED_FLOWER;
                    else if (pick == 3) flower_block = BLUE_FLOWER;
                    else if (pick == 4) flower_block = YELLOW_FLOWER;
                    else if (pick == 5) flower_block = WHITE_FLOWER;
                    editor.set_block(flower_block, x, 1, z, std::optional<std::vector<Block>>(), std::optional<std::vector<Block>>());
                } else if (random_choice <= 12) {
                    editor.set_block(GRASS, x, 1, z, std::optional<std::vector<Block>>(), std::optional<std::vector<Block>>());
                }
            }
        } else if (landuse_tag == "farmland") {
            if (!editor.check_for_block(x, 0, z, std::optional<std::vector<Block>>{ std::vector<Block>{ WATER } })) {
                if (x % 9 == 0 && z % 9 == 0) {
                    editor.set_block(WATER, x, 0, z, std::optional<std::vector<Block>>{ std::vector<Block>{ FARMLAND } }, std::optional<std::vector<Block>>());
                } else {
                    std::uniform_int_distribution<int> dist76(0, 75);
                    int r = dist76(rng);
                    if (r == 0) {
                        std::uniform_int_distribution<int> dist10(1, 10);
                        int special_choice = dist10(rng);
                        if (special_choice <= 4) {
                            editor.set_block(HAY_BALE, x, 1, z, std::optional<std::vector<Block>>(), std::optional<std::vector<Block>>{ std::vector<Block>{ SPONGE } });
                        } else {
                            editor.set_block(OAK_LEAVES, x, 1, z, std::optional<std::vector<Block>>(), std::optional<std::vector<Block>>{ std::vector<Block>{ SPONGE } });
                        }
                    } else {
                        if (editor.check_for_block(x, 0, z, std::optional<std::vector<Block>>{ std::vector<Block>{ FARMLAND } })) {
                            std::uniform_int_distribution<int> cropDist(0, 2);
                            int crop_choice = cropDist(rng);
                            Block crop = WHEAT;
                            if (crop_choice == 1) crop = CARROTS;
                            else if (crop_choice == 2) crop = POTATOES;
                            editor.set_block(crop, x, 1, z, std::optional<std::vector<Block>>(), std::optional<std::vector<Block>>());
                        }
                    }
                }
            }
        } else if (landuse_tag == "construction") {
            std::uniform_int_distribution<int> dist1500(0, 1500);
            int random_choice = dist1500(rng);
            if (random_choice < 15) {
                editor.set_block(SCAFFOLDING, x, 1, z, std::optional<std::vector<Block>>(), std::optional<std::vector<Block>>());
                if (random_choice < 2) {
                    editor.set_block(SCAFFOLDING, x, 2, z, std::optional<std::vector<Block>>(), std::optional<std::vector<Block>>());
                    editor.set_block(SCAFFOLDING, x, 3, z, std::optional<std::vector<Block>>(), std::optional<std::vector<Block>>());
                } else if (random_choice < 4) {
                    editor.set_block(SCAFFOLDING, x, 2, z, std::optional<std::vector<Block>>(), std::optional<std::vector<Block>>());
                    editor.set_block(SCAFFOLDING, x, 3, z, std::optional<std::vector<Block>>(), std::optional<std::vector<Block>>());
                    editor.set_block(SCAFFOLDING, x, 4, z, std::optional<std::vector<Block>>(), std::optional<std::vector<Block>>());
                    editor.set_block(SCAFFOLDING, x, 1, z + 1, std::optional<std::vector<Block>>(), std::optional<std::vector<Block>>());
                } else {
                    editor.set_block(SCAFFOLDING, x, 2, z, std::optional<std::vector<Block>>(), std::optional<std::vector<Block>>());
                    editor.set_block(SCAFFOLDING, x, 3, z, std::optional<std::vector<Block>>(), std::optional<std::vector<Block>>());
                    editor.set_block(SCAFFOLDING, x, 4, z, std::optional<std::vector<Block>>(), std::optional<std::vector<Block>>());
                    editor.set_block(SCAFFOLDING, x, 5, z, std::optional<std::vector<Block>>(), std::optional<std::vector<Block>>());
                    editor.set_block(SCAFFOLDING, x - 1, 1, z, std::optional<std::vector<Block>>(), std::optional<std::vector<Block>>());
                    editor.set_block(SCAFFOLDING, x + 1, 1, z - 1, std::optional<std::vector<Block>>(), std::optional<std::vector<Block>>());
                }
            } else if (random_choice < 55) {
                std::array<Block, 13> construction_items = {
                    OAK_LOG,
                    COBBLESTONE,
                    GRAVEL,
                    GLOWSTONE,
                    STONE,
                    COBBLESTONE_WALL,
                    BLACK_CONCRETE,
                    SAND,
                    OAK_PLANKS,
                    DIRT,
                    BRICK,
                    CRAFTING_TABLE,
                    FURNACE
                };
                std::uniform_int_distribution<std::size_t> idxDist(0, construction_items.size() - 1);
                editor.set_block(construction_items[idxDist(rng)], x, 1, z, std::optional<std::vector<Block>>(), std::optional<std::vector<Block>>());
            } else if (random_choice < 65) {
                if (random_choice < 60) {
                    editor.set_block(DIRT, x, 1, z, std::optional<std::vector<Block>>(), std::optional<std::vector<Block>>());
                    editor.set_block(DIRT, x, 2, z, std::optional<std::vector<Block>>(), std::optional<std::vector<Block>>());
                    editor.set_block(DIRT, x + 1, 1, z, std::optional<std::vector<Block>>(), std::optional<std::vector<Block>>());
                    editor.set_block(DIRT, x, 1, z + 1, std::optional<std::vector<Block>>(), std::optional<std::vector<Block>>());
                } else {
                    editor.set_block(DIRT, x, 1, z, std::optional<std::vector<Block>>(), std::optional<std::vector<Block>>());
                    editor.set_block(DIRT, x, 2, z, std::optional<std::vector<Block>>(), std::optional<std::vector<Block>>());
                    editor.set_block(DIRT, x - 1, 1, z, std::optional<std::vector<Block>>(), std::optional<std::vector<Block>>());
                    editor.set_block(DIRT, x, 1, z - 1, std::optional<std::vector<Block>>(), std::optional<std::vector<Block>>());
                }
            } else if (random_choice < 100) {
                editor.set_block(GRAVEL, x, 0, z, std::optional<std::vector<Block>>(), std::optional<std::vector<Block>>{ std::vector<Block>{ SPONGE } });
            } else if (random_choice < 115) {
                editor.set_block(SAND, x, 0, z, std::optional<std::vector<Block>>(), std::optional<std::vector<Block>>{ std::vector<Block>{ SPONGE } });
            } else if (random_choice < 125) {
                editor.set_block(DIORITE, x, 0, z, std::optional<std::vector<Block>>(), std::optional<std::vector<Block>>{ std::vector<Block>{ SPONGE } });
            } else if (random_choice < 145) {
                editor.set_block(BRICK, x, 0, z, std::optional<std::vector<Block>>(), std::optional<std::vector<Block>>{ std::vector<Block>{ SPONGE } });
            } else if (random_choice < 155) {
                editor.set_block(GRANITE, x, 0, z, std::optional<std::vector<Block>>(), std::optional<std::vector<Block>>{ std::vector<Block>{ SPONGE } });
            } else if (random_choice < 180) {
                editor.set_block(ANDESITE, x, 0, z, std::optional<std::vector<Block>>(), std::optional<std::vector<Block>>{ std::vector<Block>{ SPONGE } });
            } else if (random_choice < 565) {
                editor.set_block(COBBLESTONE, x, 0, z, std::optional<std::vector<Block>>(), std::optional<std::vector<Block>>{ std::vector<Block>{ SPONGE } });
            }
        } else if (landuse_tag == "grass") {
            if (editor.check_for_block(x, 0, z, std::optional<std::vector<Block>>{ std::vector<Block>{ GRASS_BLOCK } })) {
                std::uniform_int_distribution<int> dist200(0, 199);
                int r = dist200(rng);
                if (r == 0) {
                    editor.set_block(OAK_LEAVES, x, 1, z, std::optional<std::vector<Block>>(), std::optional<std::vector<Block>>());
                } else if (r <= 170) {
                    editor.set_block(GRASS, x, 1, z, std::optional<std::vector<Block>>(), std::optional<std::vector<Block>>());
                }
            }
        } else if (landuse_tag == "greenfield") {
            if (editor.check_for_block(x, 0, z, std::optional<std::vector<Block>>{ std::vector<Block>{ GRASS_BLOCK } })) {
                std::uniform_int_distribution<int> dist200(0, 199);
                int r = dist200(rng);
                if (r == 0) {
                    editor.set_block(OAK_LEAVES, x, 1, z, std::optional<std::vector<Block>>(), std::optional<std::vector<Block>>());
                } else if (r <= 17) {
                    editor.set_block(GRASS, x, 1, z, std::optional<std::vector<Block>>(), std::optional<std::vector<Block>>());
                }
            }
        } else if (landuse_tag == "meadow") {
            if (editor.check_for_block(x, 0, z, std::optional<std::vector<Block>>{ std::vector<Block>{ GRASS_BLOCK } })) {
                std::uniform_int_distribution<int> dist1000(0, 1000);
                int random_choice = dist1000(rng);
                if (random_choice < 5) {
                    Tree::create(editor, std::make_tuple(x, 1, z));
                } else if (random_choice < 6) {
                    editor.set_block(RED_FLOWER, x, 1, z, std::optional<std::vector<Block>>(), std::optional<std::vector<Block>>());
                } else if (random_choice < 9) {
                    editor.set_block(OAK_LEAVES, x, 1, z, std::optional<std::vector<Block>>(), std::optional<std::vector<Block>>());
                } else if (random_choice < 800) {
                    editor.set_block(GRASS, x, 1, z, std::optional<std::vector<Block>>(), std::optional<std::vector<Block>>());
                }
            }
        } else if (landuse_tag == "orchard") {
            if (x % 18 == 0 && z % 10 == 0) {
                Tree::create(editor, std::make_tuple(x, 1, z));
            } else if (editor.check_for_block(x, 0, z, std::optional<std::vector<Block>>{ std::vector<Block>{ GRASS_BLOCK } })) {
                std::uniform_int_distribution<int> dist100(0, 99);
                int r = dist100(rng);
                if (r == 0) {
                    editor.set_block(OAK_LEAVES, x, 1, z, std::optional<std::vector<Block>>(), std::optional<std::vector<Block>>());
                } else if (r <= 20) {
                    editor.set_block(GRASS, x, 1, z, std::optional<std::vector<Block>>(), std::optional<std::vector<Block>>());
                }
            }
        } else if (landuse_tag == "quarry") {
            editor.set_block(STONE, x, -1, z, std::optional<std::vector<Block>>{ std::vector<Block>{ STONE } }, std::optional<std::vector<Block>>());
            editor.set_block(STONE, x, -2, z, std::optional<std::vector<Block>>{ std::vector<Block>{ STONE } }, std::optional<std::vector<Block>>());
            auto it = element.tags.find(std::string("resource"));
            if (it != element.tags.end()) {
                Block ore_block = STONE;
                std::string const & resource = it->second;
                if (resource == "iron_ore") ore_block = IRON_ORE;
                else if (resource == "coal") ore_block = COAL_ORE;
                else if (resource == "copper") ore_block = COPPER_ORE;
                else if (resource == "gold") ore_block = GOLD_ORE;
                else if (resource == "clay" || resource == "kaolinite") ore_block = CLAY;
                int abs_y = editor.get_absolute_y(x, 0, z);
                std::uniform_int_distribution<int> distResource(0, 99 + abs_y);
                int random_choice = distResource(rng);
                if (random_choice < 5) {
                    editor.set_block(ore_block, x, 0, z, std::optional<std::vector<Block>>{ std::vector<Block>{ STONE } }, std::optional<std::vector<Block>>());
                }
            }
        }
    }
}

void generate_landuse_from_relation(WorldEditor & editor, ProcessedRelation const & rel, Args const & args) {
    if (rel.tags.find(std::string("landuse")) != rel.tags.end()) {
        for (auto const & member : rel.members) {
            if (member.role == ProcessedMemberRole::Outer) {
                generate_landuse(editor, member.way, args);
            }
        }

        std::vector<Node> combined_nodes;
        for (auto const & member : rel.members) {
            if (member.role == ProcessedMemberRole::Outer) {
                combined_nodes.insert(combined_nodes.end(), member.way.nodes.begin(), member.way.nodes.end());
            }
        }

        if (!combined_nodes.empty()) {
            ProcessedWay combined_way { rel.id, combined_nodes, rel.tags };
            generate_landuse(editor, combined_way, args);
        }
    }
}

}
}