#include <vector>
#include <string>
#include <unordered_map>
#include <optional>
#include <tuple>
#include <cstdlib>

#include "../../arnis_adapter.h"


/*
namespace crate {
namespace args { struct Args { std::optional<int> timeout; }; }
namespace block_definitions { struct Block {}; extern const Block CAULDRON, IRON_BLOCK, OAK_PLANKS, STONE_BLOCK_SLAB, OAK_FENCE, STONE_BRICK_SLAB, WATER, GRAY_CONCRETE, BLACK_CONCRETE, LIGHT_GRAY_CONCRETE, COBBLESTONE_WALL, GLOWSTONE, SMOOTH_STONE, OAK_LOG; }
namespace bresenham { std::vector<std::tuple<int,int,int>> bresenham_line(int,int,int,int,int,int); }
namespace coordinate_system { namespace cartesian { struct XZPoint { int x; int z; XZPoint(int xx=0,int zz=0):x(xx),z(zz){} }; } }
namespace floodfill { std::vector<std::pair<int,int>> flood_fill_area(const std::vector<std::pair<int,int>>& polygon, const std::optional<int>& timeout); }
namespace osm_parser {
    struct ProcessedNode { int x; int z; crate::coordinate_system::cartesian::XZPoint xz() const { return crate::coordinate_system::cartesian::XZPoint(x,z); } };
    struct ProcessedElement {
        const std::unordered_map<std::string,std::string>& tags() const { return _tags; }
        const std::vector<ProcessedNode>& nodes() const { return _nodes; }
        std::unordered_map<std::string,std::string> _tags;
        std::vector<ProcessedNode> _nodes;
    };
}
namespace world_editor {
    struct WorldEditor {
        void set_block(const crate::block_definitions::Block& b, int x, int y, int z, const std::optional<std::vector<const crate::block_definitions::Block*>>& alt = std::nullopt, const std::optional<int>& meta = std::nullopt) {}
    };
}
}
*/

namespace arnis
{

    namespace amenities
{


void generate_amenities(crate::world_editor::WorldEditor& editor, const crate::osm_parser::ProcessedElement& element, const crate::args::Args& args) {
    // Skip if 'layer' or 'level' is negative in the tags
    {
        const std::unordered_map<std::string,std::string>& t = element.tags();
        auto it_layer = t.find("layer");
        if (it_layer != t.end()) {
            try {
                int layer = std::stoi(it_layer->second);
                if (layer < 0) return;
            } catch (...) {}
        }
        auto it_level = t.find("level");
        if (it_level != t.end()) {
            try {
                int level = std::stoi(it_level->second);
                if (level < 0) return;
            } catch (...) {}
        }
    }

    const std::unordered_map<std::string,std::string>& tags = element.tags();
    auto it_amenity = tags.find("amenity");
    if (it_amenity == tags.end()) return;
    const std::string& amenity_type = it_amenity->second;

    std::optional<crate::coordinate_system::cartesian::XZPoint> first_node = std::nullopt;
    {
        const std::vector<crate::osm_parser::ProcessedNode>& nodes = element.nodes();
        if (!nodes.empty()) first_node.emplace( crate::coordinate_system::cartesian::XZPoint(nodes.front().x, nodes.front().z));
    }

    if (amenity_type == "waste_disposal" || amenity_type == "waste_basket") {
        if (first_node.has_value()) {
            editor.set_block(crate::block_definitions::CAULDRON, first_node->x, 1, first_node->z, std::nullopt, std::nullopt);
        }
        return;
    }

    if (amenity_type == "vending_machine" || amenity_type == "atm") {
        if (first_node.has_value()) {
            editor.set_block(crate::block_definitions::IRON_BLOCK, first_node->x, 1, first_node->z, std::nullopt, std::nullopt);
            editor.set_block(crate::block_definitions::IRON_BLOCK, first_node->x, 2, first_node->z, std::nullopt, std::nullopt);
        }
        return;
    }

    if (amenity_type == "bicycle_parking") {
        const crate::block_definitions::Block ground_block = crate::block_definitions::OAK_PLANKS;
        const crate::block_definitions::Block roof_block = crate::block_definitions::STONE_BLOCK_SLAB;

        std::vector<std::pair<int,int>> polygon_coords;
        for (const crate::osm_parser::ProcessedNode& n : element.nodes()) polygon_coords.emplace_back(n.x, n.z);
        if (polygon_coords.empty()) return;

        std::vector<std::pair<int,int>> floor_area = crate::floodfill::flood_fill_area(polygon_coords, args.timeout);

        for (const auto& p : floor_area) {
            editor.set_block(ground_block, p.first, 0, p.second, std::nullopt, std::nullopt);
        }

        for (const crate::osm_parser::ProcessedNode& node : element.nodes()) {
            int x = node.x; int z = node.z;
            editor.set_block(ground_block, x, 0, z, std::nullopt, std::nullopt);
            for (int y = 1; y <= 4; ++y) editor.set_block(crate::block_definitions::OAK_FENCE, x, y, z, std::nullopt, std::nullopt);
            editor.set_block(roof_block, x, 5, z, std::nullopt, std::nullopt);
        }

        for (const auto& p : floor_area) {
            editor.set_block(roof_block, p.first, 5, p.second, std::nullopt, std::nullopt);
        }
        return;
    }

    if (amenity_type == "bench") {
        if (first_node.has_value()) {
            bool r = (std::rand() & 1) != 0;
            if (r) {
                editor.set_block(crate::block_definitions::SMOOTH_STONE, first_node->x, 1, first_node->z, std::nullopt, std::nullopt);
                editor.set_block(crate::block_definitions::OAK_LOG, first_node->x + 1, 1, first_node->z, std::nullopt, std::nullopt);
                editor.set_block(crate::block_definitions::OAK_LOG, first_node->x - 1, 1, first_node->z, std::nullopt, std::nullopt);
            } else {
                editor.set_block(crate::block_definitions::SMOOTH_STONE, first_node->x, 1, first_node->z, std::nullopt, std::nullopt);
                editor.set_block(crate::block_definitions::OAK_LOG, first_node->x, 1, first_node->z + 1, std::nullopt, std::nullopt);
                editor.set_block(crate::block_definitions::OAK_LOG, first_node->x, 1, first_node->z - 1, std::nullopt, std::nullopt);
            }
        }
        return;
    }

    if (amenity_type == "shelter") {
        const crate::block_definitions::Block roof_block = crate::block_definitions::STONE_BRICK_SLAB;
        std::vector<std::pair<int,int>> polygon_coords;
        for (const crate::osm_parser::ProcessedNode& n : element.nodes()) polygon_coords.emplace_back(n.x, n.z);
        std::vector<std::pair<int,int>> roof_area = crate::floodfill::flood_fill_area(polygon_coords, args.timeout);

        for (const crate::osm_parser::ProcessedNode& node : element.nodes()) {
            int x = node.x; int z = node.z;
            for (int fence_height = 1; fence_height <= 4; ++fence_height) editor.set_block(crate::block_definitions::OAK_FENCE, x, fence_height, z, std::nullopt, std::nullopt);
            editor.set_block(roof_block, x, 5, z, std::nullopt, std::nullopt);
        }

        for (const auto& p : roof_area) {
            editor.set_block(roof_block, p.first, 5, p.second, std::nullopt, std::nullopt);
        }
        return;
    }

    if (amenity_type == "parking" || amenity_type == "fountain") {
        std::optional<crate::coordinate_system::cartesian::XZPoint> previous_node = std::nullopt;
        std::tuple<int,int,int> corner_addup = std::make_tuple(0,0,0);
        std::vector<std::pair<int,int>> current_amenity;

        const crate::block_definitions::Block block_type = (amenity_type == "fountain") ? crate::block_definitions::WATER : crate::block_definitions::GRAY_CONCRETE;

        for (const crate::osm_parser::ProcessedNode& node : element.nodes()) {
            crate::coordinate_system::cartesian::XZPoint pt = node.xz();
            if (previous_node.has_value()) {
                std::vector<std::tuple<int,int,int>> bresenham_points = crate::bresenham::bresenham_line(previous_node->x, 0, previous_node->z, pt.x, 0, pt.z);
                for (const auto& t : bresenham_points) {
                    int bx = std::get<0>(t);
                    int bz = std::get<2>(t);
                    editor.set_block(block_type, bx, 0, bz, std::optional<std::vector<const crate::block_definitions::Block*>>(std::nullopt), std::nullopt);
                    if (amenity_type == "fountain") {
                        for (int dx = -1; dx <= 1; ++dx) for (int dz = -1; dz <= 1; ++dz) if (!(dx == 0 && dz == 0)) {
                            editor.set_block(crate::block_definitions::LIGHT_GRAY_CONCRETE, bx + dx, 0, bz + dz, std::nullopt, std::nullopt);
                        }
                    }
                    current_amenity.emplace_back(node.x, node.z);
                    std::get<0>(corner_addup) += node.x;
                    std::get<1>(corner_addup) += node.z;
                    std::get<2>(corner_addup) += 1;
                }
            }
            previous_node.emplace(pt);
        }

        if (std::get<2>(corner_addup) > 0) {
            std::vector<std::pair<int,int>> polygon_coords = current_amenity;
            std::vector<std::pair<int,int>> flood_area = crate::floodfill::flood_fill_area(polygon_coords, args.timeout);

            for (const auto& p : flood_area) {
                int x = p.first; int z = p.second;
                editor.set_block(block_type, x, 0, z, std::optional<std::vector<const crate::block_definitions::Block*>>(std::vector<const crate::block_definitions::Block*>{ &crate::block_definitions::BLACK_CONCRETE, &crate::block_definitions::GRAY_CONCRETE }), std::nullopt);

                if (amenity_type == "parking") {
                    int space_width = 4;
                    int space_length = 6;
                    int lane_width = 5;
                    int zone_x = (x >= 0 ? x : x - (space_width-1)) / space_width;
                    int zone_z = (z >= 0 ? z : z - (space_length + lane_width -1)) / (space_length + lane_width);
                    int local_x = x % space_width; if (local_x < 0) local_x += space_width;
                    int local_z = z % (space_length + lane_width); if (local_z < 0) local_z += (space_length + lane_width);

                    if (local_z < space_length) {
                        if (local_x == 0) {
                            editor.set_block(crate::block_definitions::LIGHT_GRAY_CONCRETE, x, 0, z, std::optional<std::vector<const crate::block_definitions::Block*>>(std::vector<const crate::block_definitions::Block*>{ &crate::block_definitions::BLACK_CONCRETE, &crate::block_definitions::GRAY_CONCRETE }), std::nullopt);
                        } else if (local_z == 0) {
                            editor.set_block(crate::block_definitions::LIGHT_GRAY_CONCRETE, x, 0, z, std::optional<std::vector<const crate::block_definitions::Block*>>(std::vector<const crate::block_definitions::Block*>{ &crate::block_definitions::BLACK_CONCRETE, &crate::block_definitions::GRAY_CONCRETE }), std::nullopt);
                        }
                    } else if (local_z == space_length) {
                        editor.set_block(crate::block_definitions::LIGHT_GRAY_CONCRETE, x, 0, z, std::optional<std::vector<const crate::block_definitions::Block*>>(std::vector<const crate::block_definitions::Block*>{ &crate::block_definitions::BLACK_CONCRETE, &crate::block_definitions::GRAY_CONCRETE }), std::nullopt);
                    } else if (local_z > space_length && local_z < space_length + lane_width) {
                        editor.set_block(crate::block_definitions::BLACK_CONCRETE, x, 0, z, std::optional<std::vector<const crate::block_definitions::Block*>>(std::vector<const crate::block_definitions::Block*>{ &crate::block_definitions::GRAY_CONCRETE }), std::nullopt);
                    }

                    if (local_x == 0 && local_z == 0 && (zone_x % 3 + 3) % 3 == 0 && (zone_z % 2 + 2) % 2 == 0) {
                        editor.set_block(crate::block_definitions::COBBLESTONE_WALL, x, 1, z, std::nullopt, std::nullopt);
                        for (int dy = 2; dy <= 4; ++dy) editor.set_block(crate::block_definitions::OAK_FENCE, x, dy, z, std::nullopt, std::nullopt);
                        editor.set_block(crate::block_definitions::GLOWSTONE, x, 5, z, std::nullopt, std::nullopt);
                    }
                }
            }
        }
        return;
    }

    return;
}
}
}