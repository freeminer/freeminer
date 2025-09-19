#include "block_definitions.h"
#include "bresenham.h"
#include "osm_parser.h"
#include "world_editor.h"

#include <string>
#include <unordered_map>
#include <vector>
#include <tuple>
#include <cmath>
#include <optional>


#include "../../arnis_adapter.h"

#undef stoi
#undef stof
namespace arnis
{

namespace barriers {

void generate_barriers(world_editor::WorldEditor& editor, osm_parser::ProcessedElement const& element) {
    block_definitions::Block barrier_material = block_definitions::COBBLESTONE_WALL;
    int barrier_height = 2;

    {
        const std::unordered_map<std::string, std::string>& tags = element.tags();
        auto it = tags.find("barrier");
        if (it != tags.end()) {
            const std::string& val = it->second;
            if (val == "bollard") {
                barrier_material = block_definitions::COBBLESTONE_WALL;
                barrier_height = 1;
            } else if (val == "kerb") {
                return;
            } else if (val == "hedge") {
                barrier_material = block_definitions::OAK_LEAVES;
                barrier_height = 2;
            } else if (val == "fence") {
                auto fit = tags.find("fence_type");
                if (fit != tags.end()) {
                    const std::string& f = fit->second;
                    if (f == "railing" || f == "bars" || f == "krest") {
                        barrier_material = block_definitions::STONE_BRICK_WALL;
                        barrier_height = 1;
                    } else if (f == "chain_link" || f == "metal" || f == "wire" || f == "barbed_wire" || f == "corrugated_metal" || f == "electric" || f == "metal_bars") {
                        barrier_material = block_definitions::STONE_BRICK_WALL;
                        barrier_height = 2;
                    } else if (f == "slatted" || f == "paling") {
                        barrier_material = block_definitions::OAK_FENCE;
                        barrier_height = 1;
                    } else if (f == "wood" || f == "split_rail" || f == "panel" || f == "pole") {
                        barrier_material = block_definitions::OAK_FENCE;
                        barrier_height = 2;
                    } else if (f == "concrete" || f == "stone") {
                        barrier_material = block_definitions::STONE_BRICK_WALL;
                        barrier_height = 2;
                    } else if (f == "glass") {
                        barrier_material = block_definitions::GLASS;
                        barrier_height = 1;
                    }
                }
            } else if (val == "wall") {
                barrier_material = block_definitions::STONE_BRICK_WALL;
                barrier_height = 3;
            }
        }
    }

    {
        const std::unordered_map<std::string, std::string>& tags = element.tags();
        auto mit = tags.find("material");
        if (mit != tags.end()) {
            const std::string& mat = mit->second;
            if (mat == "brick") {
                barrier_material = block_definitions::BRICK;
            }
            if (mat == "concrete") {
                barrier_material = block_definitions::LIGHT_GRAY_CONCRETE;
            }
            if (mat == "metal") {
                barrier_material = block_definitions::STONE_BRICK_WALL;
            }
        }
    }

    std::optional<osm_parser::Way> maybe_way = element.as_way();
    if (!maybe_way.has_value()) {
        return;
    }
    const osm_parser::Way& way = maybe_way.value();

    int wall_height = barrier_height;
    {
        const std::unordered_map<std::string, std::string>& tags = element.tags();
        auto hit = tags.find("height");
        if (hit != tags.end()) {
            try {
                float h = std::stof(hit->second);
                wall_height = static_cast<int>(std::lround(h));
            } catch (...) {
                wall_height = barrier_height;
            }
        }
    }

    for (std::size_t i = 1; i < way.nodes.size(); ++i) {
        const osm_parser::ProcessedNode& prev = way.nodes[i - 1];
        int x1 = prev.x;
        int z1 = prev.z;

        const osm_parser::ProcessedNode& cur = way.nodes[i];
        int x2 = cur.x;
        int z2 = cur.z;

        std::vector<std::tuple<int, int, int>> bresenham_points = bresenham::bresenham_line(x1, 0, z1, x2, 0, z2);
        for (const auto& pt : bresenham_points) {
            int bx = std::get<0>(pt);
            int bz = std::get<2>(pt);

            for (int y = 1; y <= wall_height; ++y) {
                editor.set_block(barrier_material, bx, y, bz, std::optional<std::vector<block_definitions::Block>>(), std::optional<int>());
            }

            if (wall_height > 1) {
                editor.set_block(block_definitions::STONE_BRICK_SLAB, bx, wall_height + 1, bz, std::optional<std::vector<block_definitions::Block>>(), std::optional<int>());
            }
        }
    }
}

void generate_barrier_nodes(world_editor::WorldEditor& editor, osm_parser::ProcessedNode const& node) {
    auto it = node.tags.find("barrier");
    if (it == node.tags.end()) {
        return;
    }
    const std::string& val = it->second;
    if (val == "bollard") {
        editor.set_block(block_definitions::COBBLESTONE_WALL, node.x, 1, node.z, std::optional<std::vector<block_definitions::Block>>(), std::optional<int>());
    } else if (val == "stile" || val == "gate" || val == "swing_gate" || val == "lift_gate") {
        // intentionally left blank (original code had commented behavior)
    } else if (val == "block") {
        editor.set_block(block_definitions::STONE, node.x, 1, node.z, std::optional<std::vector<block_definitions::Block>>(), std::optional<int>());
    } else if (val == "entrance") {
        editor.set_block(block_definitions::AIR, node.x, 1, node.z, std::optional<std::vector<block_definitions::Block>>(), std::optional<int>());
    }
}

}
}  