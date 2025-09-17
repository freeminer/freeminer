#include <vector>
#include <string>
#include <unordered_map>
#include <optional>
#include <variant>
#include <tuple>
#include <cmath>

#include "../bresenham.h"
#include "../../arnis_adapter.h"

using namespace arnis;

#undef stoi
namespace arnis
{

namespace highways
{


//using Block = int;
/*
constexpr Block COBBLESTONE_WALL = 1;
constexpr Block OAK_FENCE = 2;
constexpr Block GLOWSTONE = 3;
constexpr Block GREEN_WOOL = 4;
constexpr Block YELLOW_WOOL = 5;
constexpr Block RED_WOOL = 6;
constexpr Block WHITE_WOOL = 7;
constexpr Block WHITE_CONCRETE = 8;
constexpr Block BLACK_CONCRETE = 9;
constexpr Block GRAY_CONCRETE = 10;
constexpr Block DIRT_PATH = 11;
constexpr Block STONE = 12;
constexpr Block STONE_BRICKS = 13;
constexpr Block BRICK = 14;
constexpr Block OAK_PLANKS = 15;
constexpr Block BLACK_CONCRETE_ALIAS = 9;
constexpr Block GRAVEL = 16;
constexpr Block GRASS_BLOCK = 17;
constexpr Block DIRT = 18;
constexpr Block SAND = 19;
constexpr Block LIGHT_GRAY_CONCRETE = 20;
constexpr Block STONE_BRICK_SLAB = 21;
constexpr Block SAND_ALIAS = 19; // reuse
// Add any other block constants as needed...

struct Args {
    double scale;
    std::optional<std::string> timeout;
};

struct XZPoint {
    int x;
    int z;
};

struct ProcessedNode {
    int x;
    int z;
    std::unordered_map<std::string, std::string> tags;
    XZPoint xz() const { return XZPoint{x, z}; }
};

struct ProcessedWay {
    std::vector<ProcessedNode> nodes;
    std::unordered_map<std::string, std::string> tags;
};

using ProcessedElement = std::variant<ProcessedNode, ProcessedWay>;

struct WorldEditor {
    // set_block(block, x, y, z, optional replace_with list, optional avoid list)
    void set_block(Block block, int x, int y, int z,
                   const std::optional<std::vector<Block>>& replace_with,
                   const std::optional<std::vector<Block>>& avoid) {
        // implementation provided elsewhere
    }

    // check_for_block(x,y,z, optional list) -> bool
    bool check_for_block(int x, int y, int z, const std::optional<std::vector<Block>>& blocks) {
        // implementation provided elsewhere
        return false;
    }
};

// External utility functions (to be provided elsewhere)
std::vector<std::tuple<int,int,int>> bresenham_line(int x1, int y1, int z1, int x2, int y2, int z2);
std::vector<std::pair<int,int>> flood_fill_area(const std::vector<std::pair<int,int>>& polygon_coords, const std::optional<std::string>& timeout);
*/

// Helper to get tags map from a ProcessedElement
static const std::unordered_map<std::string, std::string>* elem_tags(const ProcessedElement& e) {
    if (const ProcessedNode* n = std::get_if< ProcessedNode>(&e)) return &n->tags;
    if (const ProcessedWay* w = std::get_if< ProcessedWay>(&e)) return &w->tags;
    return nullptr;
}

void generate_highways(WorldEditor& editor, const ProcessedElement& element, const Args& args) {
    const auto* tags = elem_tags(element);
    if (!tags) return;

    auto it_highway = tags->find("highway");
    if (it_highway == tags->end()) return;
    const std::string& highway_type = it_highway->second;

    if (highway_type == "street_lamp") {
        if (const ProcessedNode* node = std::get_if<ProcessedNode>(&element)) {
            int x = node->x;
            int z = node->z;
            editor.set_block(COBBLESTONE_WALL, x, 1, z, std::nullopt, std::nullopt);
            for (int dy = 2; dy <= 4; ++dy) {
                editor.set_block(OAK_FENCE, x, dy, z, std::nullopt, std::nullopt);
            }
            editor.set_block(GLOWSTONE, x, 5, z, std::nullopt, std::nullopt);
        }
        return;
    }

    if (highway_type == "crossing") {
        auto it_crossing = tags->find("crossing");
        if (it_crossing != tags->end() && it_crossing->second == "traffic_signals") {
            if (const ProcessedNode* node = std::get_if<ProcessedNode>(&element)) {
                int x = node->x;
                int z = node->z;
                for (int dy = 1; dy <= 3; ++dy) {
                    editor.set_block(COBBLESTONE_WALL, x, dy, z, std::nullopt, std::nullopt);
                }
                editor.set_block(GREEN_WOOL, x, 4, z, std::nullopt, std::nullopt);
                editor.set_block(YELLOW_WOOL, x, 5, z, std::nullopt, std::nullopt);
                editor.set_block(RED_WOOL, x, 6, z, std::nullopt, std::nullopt);
            }
        }
        return;
    }

    if (highway_type == "bus_stop") {
        if (const ProcessedNode* node = std::get_if<ProcessedNode>(&element)) {
            int x = node->x;
            int z = node->z;
            for (int dy = 1; dy <= 3; ++dy) {
                editor.set_block(COBBLESTONE_WALL, x, dy, z, std::nullopt, std::nullopt);
            }
            editor.set_block(WHITE_WOOL, x, 4, z, std::nullopt, std::nullopt);
            editor.set_block(WHITE_WOOL, x + 1, 4, z, std::nullopt, std::nullopt);
        }
        return;
    }

    auto it_area = tags->find("area");
    if (it_area != tags->end() && it_area->second == "yes") {
        const ProcessedWay* way = std::get_if<ProcessedWay>(&element);
        if (!way) return;

        Block surface_block = STONE;
        auto it_surface = tags->find("surface");
        if (it_surface != tags->end()) {
            const std::string& surface = it_surface->second;
            if (surface == "paving_stones" || surface == "sett") surface_block = STONE_BRICKS;
            else if (surface == "bricks") surface_block = BRICK;
            else if (surface == "wood") surface_block = OAK_PLANKS;
            else if (surface == "asphalt") surface_block = BLACK_CONCRETE;
            else if (surface == "gravel" || surface == "fine_gravel") surface_block = GRAVEL;
            else if (surface == "grass") surface_block = GRASS_BLOCK;
            else if (surface == "dirt" || surface == "ground" || surface == "earth") surface_block = DIRT;
            else if (surface == "sand") surface_block = SAND;
            else if (surface == "concrete") surface_block = LIGHT_GRAY_CONCRETE;
            else surface_block = STONE;
        }

        std::vector<std::pair<int,int>> polygon_coords;
        polygon_coords.reserve(way->nodes.size());
        for (const auto& n : way->nodes) polygon_coords.emplace_back(n.x, n.z);

        std::vector<std::pair<int,int>> filled_area = flood_fill_area(polygon_coords, &args.timeout);

        for (const auto& p : filled_area) {
            editor.set_block(surface_block, p.first, 0, p.second, std::nullopt, std::nullopt);
        }
        return;
    }

    // Default road handling (ways)
    int previous_x = 0;
    int previous_z = 0;
    bool has_previous = false;
    Block block_type = BLACK_CONCRETE;
    int block_range = 2;
    bool add_stripe = false;
    bool add_outline = false;
    double scale_factor = args.scale;

    auto it_layer = tags->find("layer");
    if (it_layer != tags->end()) {
        try {
            int layer_val = std::stoi(it_layer->second);
            if (layer_val < 0) return;
        } catch (...) {}
    }
    auto it_level = tags->find("level");
    if (it_level != tags->end()) {
        try {
            int level_val = std::stoi(it_level->second);
            if (level_val < 0) return;
        } catch (...) {}
    }

    if (highway_type == "footway" || highway_type == "pedestrian") {
        block_type = GRAY_CONCRETE;
        block_range = 1;
    } else if (highway_type == "path") {
        block_type = DIRT_PATH;
        block_range = 1;
    } else if (highway_type == "motorway" || highway_type == "primary") {
        block_range = 5;
        add_stripe = true;
    } else if (highway_type == "tertiary") {
        add_stripe = true;
    } else if (highway_type == "track") {
        block_range = 1;
    } else if (highway_type == "service") {
        block_type = GRAY_CONCRETE;
        block_range = 2;
    } else if (highway_type == "secondary_link" || highway_type == "tertiary_link") {
        block_type = BLACK_CONCRETE;
        block_range = 1;
    } else if (highway_type == "escape") {
        block_type = SAND;
        block_range = 1;
    } else if (highway_type == "steps") {
        block_type = GRAY_CONCRETE;
        block_range = 1;
    } else {
        auto it_lanes = tags->find("lanes");
        if (it_lanes != tags->end()) {
            const std::string& lanes = it_lanes->second;
            if (lanes == "2") {
                block_range = 3;
                add_stripe = true;
                add_outline = true;
            } else if (lanes != "1") {
                block_range = 4;
                add_stripe = true;
                add_outline = true;
            }
        }
    }

    const ProcessedWay* way = std::get_if<ProcessedWay>(&element);
    if (!way) return;

    if (scale_factor < 1.0) {
        block_range = static_cast<int>(std::floor(block_range * scale_factor));
    }

    for (const auto& node : way->nodes) {
        if (has_previous) {
            int x1 = previous_x;
            int z1 = previous_z;
            int x2 = node.x;
            int z2 = node.z;

            std::vector<std::tuple<int,int,int>> bresenham_points = bresenham_line(x1, 0, z1, x2, 0, z2);

            int stripe_length = 0;
            int dash_length = static_cast<int>(std::ceil(5.0 * scale_factor));
            int gap_length = static_cast<int>(std::ceil(5.0 * scale_factor));

            for (const auto& pt : bresenham_points) {
                int x = std::get<0>(pt);
                int z = std::get<2>(pt);

                for (int dx = -block_range; dx <= block_range; ++dx) {
                    for (int dz = -block_range; dz <= block_range; ++dz) {
                        int set_x = x + dx;
                        int set_z = z + dz;
                        if (highway_type == "footway") {
                            auto it_footway = tags->find("footway");
                            if (it_footway != tags->end() && it_footway->second == "crossing") {
                                bool is_horizontal = std::abs(x2 - x1) >= std::abs(z2 - z1);
                                if (is_horizontal) {
                                    if ((set_x % 2 + 2) % 2 < 1) {
                                        editor.set_block(WHITE_CONCRETE, set_x, 0, set_z, std::make_optional(std::vector<Block>{BLACK_CONCRETE}), std::nullopt);
                                    } else {
                                        editor.set_block(BLACK_CONCRETE, set_x, 0, set_z, std::nullopt, std::nullopt);
                                    }
                                } else {
                                    if ((set_z % 2 + 2) % 2 < 1) {
                                        editor.set_block(WHITE_CONCRETE, set_x, 0, set_z, std::make_optional(std::vector<Block>{BLACK_CONCRETE}), std::nullopt);
                                    } else {
                                        editor.set_block(BLACK_CONCRETE, set_x, 0, set_z, std::nullopt, std::nullopt);
                                    }
                                }
                                continue;
                            }
                        }
                        editor.set_block(block_type, set_x, 0, set_z, std::nullopt, std::make_optional(std::vector<Block>{BLACK_CONCRETE, WHITE_CONCRETE}));
                    }
                }

                if (add_outline) {
                    for (int dz = -block_range; dz <= block_range; ++dz) {
                        int outline_x = x - block_range - 1;
                        int outline_z = z + dz;
                        editor.set_block(LIGHT_GRAY_CONCRETE, outline_x, 0, outline_z, std::nullopt, std::nullopt);
                    }
                    for (int dz = -block_range; dz <= block_range; ++dz) {
                        int outline_x = x + block_range + 1;
                        int outline_z = z + dz;
                        editor.set_block(LIGHT_GRAY_CONCRETE, outline_x, 0, outline_z, std::nullopt, std::nullopt);
                    }
                }

                if (add_stripe) {
                    if (stripe_length < dash_length) {
                        int stripe_x = x;
                        int stripe_z = z;
                        editor.set_block(WHITE_CONCRETE, stripe_x, 0, stripe_z, std::make_optional(std::vector<Block>{BLACK_CONCRETE}), std::nullopt);
                    }
                    ++stripe_length;
                    if (stripe_length >= dash_length + gap_length) stripe_length = 0;
                }
            }
        }
        previous_x = node.x;
        previous_z = node.z;
        has_previous = true;
    }
}

void generate_siding(WorldEditor& editor, const ProcessedWay& element) {
    std::optional<XZPoint> previous_node;
    Block siding_block = STONE_BRICK_SLAB;

    for (const auto& node : element.nodes) {
        XZPoint current_node = node.xz();
        if (previous_node.has_value()) {
            XZPoint prev = previous_node.value();
            std::vector<std::tuple<int,int,int>> bresenham_points = bresenham_line(prev.x, 0, prev.z, current_node.x, 0, current_node.z);
            for (const auto& pt : bresenham_points) {
                int bx = std::get<0>(pt);
                int bz = std::get<2>(pt);
                if (!editor.check_for_block(bx, 0, bz, std::make_optional(std::vector<Block>{BLACK_CONCRETE, WHITE_CONCRETE}))) {
                    editor.set_block(siding_block, bx, 1, bz, std::nullopt, std::nullopt);
                }
            }
        }
        //previous_node = current_node;
        previous_node.emplace(current_node);
    }
}

void generate_aeroway(WorldEditor& editor, const ProcessedWay& way, const Args& args) {
    std::optional<std::pair<int,int>> previous_node;
    Block surface_block = LIGHT_GRAY_CONCRETE;

    for (const auto& node : way.nodes) {
        if (previous_node.has_value()) {
            int x1 = previous_node->first;
            int z1 = previous_node->second;
            int x2 = node.x;
            int z2 = node.z;
            std::vector<std::tuple<int,int,int>> points = bresenham_line(x1, 0, z1, x2, 0, z2);
            int way_width = static_cast<int>(std::ceil(12.0 * args.scale));
            for (const auto& pt : points) {
                int x = std::get<0>(pt);
                int z = std::get<2>(pt);
                for (int dx = -way_width; dx <= way_width; ++dx) {
                    for (int dz = -way_width; dz <= way_width; ++dz) {
                        int set_x = x + dx;
                        int set_z = z + dz;
                        editor.set_block(surface_block, set_x, 0, set_z, std::nullopt, std::nullopt);
                    }
                }
            }
        }
        previous_node = std::make_pair(node.x, node.z);
    }
}
}
}