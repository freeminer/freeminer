#include <vector>
#include <unordered_map>
#include <string>
#include <optional>
#include <tuple>
#include <cmath>
#include <algorithm>
#include <cstddef>

#include "../../arnis_adapter.h"
namespace arnis
{

namespace highways
{



#if 0
namespace crate {
namespace args {
struct Args {
    double scale = 1.0;
    std::optional<int> timeout;
};
} // namespace args

namespace bresenham {
std::vector<std::tuple<int, int, int>> bresenham_line(int x1, int y1, int z1, int x2, int y2, int z2);
} // namespace bresenham

namespace floodfill {
std::vector<std::pair<int, int>> flood_fill_area(const std::vector<std::pair<int, int>>& polygon, const std::optional<int>& timeout);
} // namespace floodfill

namespace coordinate_system {
namespace cartesian {
struct XZPoint {
    int x;
    int z;
};
} // namespace cartesian
} // namespace coordinate_system

namespace osm_parser {
struct ProcessedNode {
    int x;
    int z;
    coordinate_system::cartesian::XZPoint xz() const { return {x, z}; }
};

struct ProcessedWay {
    std::vector<ProcessedNode> nodes;
    std::unordered_map<std::string, std::string> tags;
};

enum class ElementType { Node, Way };

struct ProcessedElement {
    ElementType type;
    std::optional<ProcessedNode> node;
    std::optional<ProcessedWay> way;
    std::unordered_map<std::string, std::string> tags;

    const std::unordered_map<std::string, std::string>& tags_map() const { return tags; }
};
} // namespace osm_parser

namespace block_definitions {
enum Block {
    STONE,
    STONE_BRICKS,
    STONE_BRICK_SLAB,
    BLACK_CONCRETE,
    WHITE_CONCRETE,
    GRAY_CONCRETE,
    LIGHT_GRAY_CONCRETE,
    BRICK,
    OAK_PLANKS,
    DIRT,
    GRAVEL,
    SAND,
    GRASS_BLOCK,
    DIRT_PATH,
    COBBLESTONE_WALL,
    OAK_FENCE,
    GLOWSTONE,
    GREEN_WOOL,
    YELLOW_WOOL,
    RED_WOOL,
    WHITE_WOOL,
    SAND_TRAP, // placeholder
    STONE_BRICK_SLABS // placeholder
};
} // namespace block_definitions

namespace world_editor {
using crate::block_definitions::Block;

struct WorldEditor {
    void set_block(Block block, int x, int y, int z, const std::optional<std::vector<Block>>& overlay, const std::optional<std::vector<Block>>& palette) {
        // Implementation provided by host environment
        (void)block; (void)x; (void)y; (void)z; (void)overlay; (void)palette;
    }

    bool check_for_block(int x, int y, int z, const std::optional<std::vector<Block>>& blocks) const {
        // Implementation provided by host environment
        (void)x; (void)y; (void)z; (void)blocks;
        return false;
    }
};
} // namespace world_editor

} // namespace crate

#endif

// Hash for pair<int,int> used in unordered_map
struct PairHash {
    std::size_t operator()(const std::pair<int,int>& p) const noexcept {
        return std::hash<long long>()( (static_cast<long long>(p.first) << 32) ^ static_cast<unsigned long long>(p.second) );
    }
};

std::unordered_map<std::pair<int,int>, std::vector<int>, PairHash> build_highway_connectivity_map(const std::vector<crate::osm_parser::ProcessedElement>& elements) {
    std::unordered_map<std::pair<int,int>, std::vector<int>, PairHash> connectivity_map;
    for (const auto& element : elements) {
        if (element.type == crate::osm_parser::ElementType::Way /* && element.way.has_value()*/) {
            const crate::osm_parser::ProcessedWay& way = *element.way;
            auto it_highway = way.tags.find("highway");
            if (it_highway != way.tags.end()) {
                int layer_value = 0;
                auto it_layer = way.tags.find("layer");
                if (it_layer != way.tags.end()) {
                    try {
                        layer_value = std::stoi(it_layer->second);
                    } catch (...) {
                        layer_value = 0;
                    }
                }
                if (layer_value < 0) {
                    layer_value = 0;
                }
                if (!way.nodes.empty()) {
                    const crate::osm_parser::ProcessedNode& start_node = way.nodes.front();
                    const crate::osm_parser::ProcessedNode& end_node = way.nodes.back();
                    std::pair<int,int> start_coord = { start_node.x, start_node.z };
                    std::pair<int,int> end_coord = { end_node.x, end_node.z };
                    connectivity_map[start_coord].push_back(layer_value);
                    connectivity_map[end_coord].push_back(layer_value);
                }
            }
        }
    }
    return connectivity_map;
}

void add_highway_support_pillar(crate::world_editor::WorldEditor& editor, int x, int highway_y, int z, int dx, int dz, int /*_block_range*/) {
    using crate::block_definitions::STONE_BRICKS;
    if (dx == 0 && dz == 0 && ((x + z) % 8) == 0) {
        for (int y = 1; y < highway_y; ++y) {
            editor.set_block(STONE_BRICKS, x, y, z, std::optional<std::vector<crate::block_definitions::Block>>(), std::optional<std::vector<crate::block_definitions::Block>>());
        }
        for (int base_dx = -1; base_dx <= 1; ++base_dx) {
            for (int base_dz = -1; base_dz <= 1; ++base_dz) {
                editor.set_block(STONE_BRICKS, x + base_dx, 0, z + base_dz, std::optional<std::vector<crate::block_definitions::Block>>(), std::optional<std::vector<crate::block_definitions::Block>>());
            }
        }
    }
}

bool should_add_slope_at_node(const crate::osm_parser::ProcessedNode& node, int current_layer, const std::unordered_map<std::pair<int,int>, std::vector<int>, PairHash>& highway_connectivity) {
    std::pair<int,int> node_coord = { node.x, node.z };
    if (highway_connectivity.empty()) {
        return current_layer != 0;
    }
    auto it = highway_connectivity.find(node_coord);
    if (it != highway_connectivity.end()) {
        const std::vector<int>& connected_layers = it->second;
        std::size_t same_layer_count = 0;
        for (int layer : connected_layers) {
            if (layer == current_layer) {
                ++same_layer_count;
            }
        }
        if (same_layer_count <= 1) {
            return current_layer != 0;
        }
        return false;
    } else {
        return current_layer != 0;
    }
}

std::size_t calculate_way_length(const crate::osm_parser::ProcessedWay& way) {
    std::size_t total_length = 0;
    const crate::osm_parser::ProcessedNode* prev = nullptr;
    for (const auto& node : way.nodes) {
        if (prev != nullptr) {
            int dx = (node.x - prev->x);
            int dz = (node.z - prev->z);
            double seg = std::sqrt(static_cast<double>(dx*dx + dz*dz));
            total_length += static_cast<std::size_t>(seg);
        }
        prev = &node;
    }
    return total_length;
}

int calculate_point_elevation(std::size_t segment_index, std::size_t point_index, std::size_t segment_length, std::size_t total_segments, int base_elevation, bool needs_start_slope, bool needs_end_slope, std::size_t slope_length) {
    if (!needs_start_slope && !needs_end_slope) {
        return base_elevation;
    }
    std::size_t total_distance_from_start = segment_index * segment_length + point_index;
    std::size_t total_way_length = total_segments * segment_length;
    if (total_way_length == 0 || slope_length == 0) {
        return base_elevation;
    }
    if (needs_start_slope && total_distance_from_start <= slope_length) {
        float slope_progress = static_cast<float>(total_distance_from_start) / static_cast<float>(slope_length);
        int elevation_offset = static_cast<int>(static_cast<float>(base_elevation) * slope_progress);
        return elevation_offset;
    }
    if (needs_end_slope && total_distance_from_start >= (total_way_length > slope_length ? total_way_length - slope_length : 0)) {
        std::size_t distance_from_end = (total_way_length > total_distance_from_start) ? (total_way_length - total_distance_from_start) : 0;
        float slope_progress = static_cast<float>(distance_from_end) / static_cast<float>(slope_length);
        int elevation_offset = static_cast<int>(static_cast<float>(base_elevation) * slope_progress);
        return elevation_offset;
    }
    return base_elevation;
}

void generate_highways_internal(crate::world_editor::WorldEditor& editor, const crate::osm_parser::ProcessedElement& element, const crate::args::Args& args, const std::unordered_map<std::pair<int,int>, std::vector<int>, PairHash>& highway_connectivity) {
    using crate::block_definitions::Block;
    using crate::block_definitions::COBBLESTONE_WALL;
    using crate::block_definitions::OAK_FENCE;
    using crate::block_definitions::GLOWSTONE;
    using crate::block_definitions::GREEN_WOOL;
    using crate::block_definitions::YELLOW_WOOL;
    using crate::block_definitions::RED_WOOL;
    using crate::block_definitions::WHITE_WOOL;
    (void)COBBLESTONE_WALL; (void)OAK_FENCE; (void)GLOWSTONE; (void)GREEN_WOOL; (void)YELLOW_WOOL; (void)RED_WOOL; (void)WHITE_WOOL;

    auto it_highway = element.tags().find("highway");
    if (it_highway == element.tags().end()) {
        return;
    }
    const std::string& highway_type = it_highway->second;
    if (highway_type == "street_lamp") {
        if (element.type == crate::osm_parser::ElementType::Node && element.node.has_value()) {
            int x = element.node->x;
            int z = element.node->z;
            editor.set_block(crate::block_definitions::COBBLESTONE_WALL, x, 1, z, std::optional<std::vector<Block>>(), std::optional<std::vector<Block>>());
            for (int dy = 2; dy <= 4; ++dy) {
                editor.set_block(crate::block_definitions::OAK_FENCE, x, dy, z, std::optional<std::vector<Block>>(), std::optional<std::vector<Block>>());
            }
            editor.set_block(crate::block_definitions::GLOWSTONE, x, 5, z, std::optional<std::vector<Block>>(), std::optional<std::vector<Block>>());
        }
        return;
    } else if (highway_type == "crossing") {
        auto it_crossing = element.tags().find("crossing");
        if (it_crossing != element.tags().end() && it_crossing->second == "traffic_signals") {
            if (element.type == crate::osm_parser::ElementType::Node && element.node.has_value()) {
                int x = element.node->x;
                int z = element.node->z;
                for (int dy = 1; dy <= 3; ++dy) {
                    editor.set_block(crate::block_definitions::COBBLESTONE_WALL, x, dy, z, std::optional<std::vector<Block>>(), std::optional<std::vector<Block>>());
                }
                editor.set_block(GREEN_WOOL, x, 4, z, std::optional<std::vector<Block>>(), std::optional<std::vector<Block>>());
                editor.set_block(YELLOW_WOOL, x, 5, z, std::optional<std::vector<Block>>(), std::optional<std::vector<Block>>());
                editor.set_block(RED_WOOL, x, 6, z, std::optional<std::vector<Block>>(), std::optional<std::vector<Block>>());
            }
        }
        return;
    } else if (highway_type == "bus_stop") {
        if (element.type == crate::osm_parser::ElementType::Node && element.node.has_value()) {
            int x = element.node->x;
            int z = element.node->z;
            for (int dy = 1; dy <= 3; ++dy) {
                editor.set_block(crate::block_definitions::COBBLESTONE_WALL, x, dy, z, std::optional<std::vector<Block>>(), std::optional<std::vector<Block>>());
            }
            editor.set_block(crate::block_definitions::WHITE_WOOL, x, 4, z, std::optional<std::vector<Block>>(), std::optional<std::vector<Block>>());
            editor.set_block(crate::block_definitions::WHITE_WOOL, x + 1, 4, z, std::optional<std::vector<Block>>(), std::optional<std::vector<Block>>());
        }
        return;
    } else {
        auto it_area = element.tags().find("area");
        if (it_area != element.tags().end() && it_area->second == "yes") {
            if (element.type != crate::osm_parser::ElementType::Way || !element.way.has_value()) {
                return;
            }
            crate::block_definitions::Block surface_block = crate::block_definitions::STONE;
            auto it_surface = element.tags().find("surface");
            if (it_surface != element.tags().end()) {
                const std::string& surface = it_surface->second;
                if (surface == "paving_stones" || surface == "sett") surface_block = crate::block_definitions::STONE_BRICKS;
                else if (surface == "bricks") surface_block = crate::block_definitions::BRICK;
                else if (surface == "wood") surface_block = crate::block_definitions::OAK_PLANKS;
                else if (surface == "asphalt") surface_block = crate::block_definitions::BLACK_CONCRETE;
                else if (surface == "gravel" || surface == "fine_gravel") surface_block = crate::block_definitions::GRAVEL;
                else if (surface == "grass") surface_block = crate::block_definitions::GRASS_BLOCK;
                else if (surface == "dirt" || surface == "ground" || surface == "earth") surface_block = crate::block_definitions::DIRT;
                else if (surface == "sand") surface_block = crate::block_definitions::SAND;
                else if (surface == "concrete") surface_block = crate::block_definitions::LIGHT_GRAY_CONCRETE;
                else surface_block = crate::block_definitions::STONE;
            }
            const crate::osm_parser::ProcessedWay& way = *element.way;
            std::vector<std::pair<int,int>> polygon_coords;
            polygon_coords.reserve(way.nodes.size());
            for (const auto& n : way.nodes) {
                polygon_coords.emplace_back(n.x, n.z);
            }
            std::vector<std::pair<int,int>> filled_area = crate::floodfill::flood_fill_area(polygon_coords, args.timeout);
            for (const auto& p : filled_area) {
                editor.set_block(surface_block, p.first, 0, p.second, std::optional<std::vector<Block>>(), std::optional<std::vector<Block>>());
            }
            return;
        }
    }

    // Main highway/walkway processing below
    crate::block_definitions::Block block_type = crate::block_definitions::BLACK_CONCRETE;
    int block_range = 2;
    bool add_stripe = false;
    bool add_outline = false;
    double scale_factor = args.scale;

    int layer_value = 0;
    auto it_layer = element.tags().find("layer");
    if (it_layer != element.tags().end()) {
        try {
            layer_value = std::stoi(it_layer->second);
        } catch (...) {
            layer_value = 0;
        }
    }
    if (layer_value < 0) {
        layer_value = 0;
    }

    auto it_level = element.tags().find("level");
    if (it_level != element.tags().end()) {
        try {
            int level_val = std::stoi(it_level->second);
            if (level_val < 0) return;
        } catch (...) {
            // ignore parse errors
        }
    }

    if (highway_type == "footway" || highway_type == "pedestrian") {
        block_type = crate::block_definitions::GRAY_CONCRETE;
        block_range = 1;
    } else if (highway_type == "path") {
        block_type = crate::block_definitions::DIRT_PATH;
        block_range = 1;
    } else if (highway_type == "motorway" || highway_type == "primary" || highway_type == "trunk") {
        block_range = 5;
        add_stripe = true;
    } else if (highway_type == "secondary") {
        block_range = 4;
        add_stripe = true;
    } else if (highway_type == "tertiary") {
        add_stripe = true;
    } else if (highway_type == "track") {
        block_range = 1;
    } else if (highway_type == "service") {
        block_type = crate::block_definitions::GRAY_CONCRETE;
        block_range = 2;
    } else if (highway_type == "secondary_link" || highway_type == "tertiary_link") {
        block_type = crate::block_definitions::BLACK_CONCRETE;
        block_range = 1;
    } else if (highway_type == "escape") {
        block_type = crate::block_definitions::SAND;
        block_range = 1;
    } else if (highway_type == "steps") {
        block_type = crate::block_definitions::GRAY_CONCRETE;
        block_range = 1;
    } else {
        auto it_lanes = element.tags().find("lanes");
        if (it_lanes != element.tags().end()) {
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

    if (element.type != crate::osm_parser::ElementType::Way || !element.way.has_value()) {
        return;
    }
    const crate::osm_parser::ProcessedWay& way = *element.way;

    if (scale_factor < 1.0) {
        block_range = static_cast<int>(std::floor(static_cast<double>(block_range) * scale_factor));
    }

    const int LAYER_HEIGHT_STEP = 6;
    int base_elevation = layer_value * LAYER_HEIGHT_STEP;

    bool needs_start_slope = false;
    bool needs_end_slope = false;
    if (!way.nodes.empty()) {
        needs_start_slope = should_add_slope_at_node(way.nodes.front(), layer_value, highway_connectivity);
        needs_end_slope = should_add_slope_at_node(way.nodes.back(), layer_value, highway_connectivity);
    }

    std::size_t total_way_length = calculate_way_length(way);

    bool is_short_isolated_elevated = (needs_start_slope && needs_end_slope && layer_value > 0 && total_way_length <= 35);

    int effective_elevation = 0;
    bool effective_start_slope = false;
    bool effective_end_slope = false;
    if (is_short_isolated_elevated) {
        effective_elevation = 0;
        effective_start_slope = false;
        effective_end_slope = false;
    } else {
        effective_elevation = base_elevation;
        effective_start_slope = needs_start_slope;
        effective_end_slope = needs_end_slope;
    }

    std::size_t slope_length = static_cast<std::size_t>(std::clamp(static_cast<double>(total_way_length) * 0.35, 15.0, 50.0));

    std::optional<std::pair<int,int>> previous_node;
    std::size_t segment_index = 0;
    std::size_t total_segments = (way.nodes.size() > 0) ? (way.nodes.size() - 1) : 0;

    for (const auto& node : way.nodes) {
        if (previous_node.has_value()) {
            int x1 = previous_node->first;
            int z1 = previous_node->second;
            int x2 = node.x;
            int z2 = node.z;
            std::vector<std::tuple<int,int,int>> bresenham_points = crate::bresenham::bresenham_line(x1, 0, z1, x2, 0, z2);
            std::size_t segment_length = bresenham_points.size();

            int stripe_length_counter = 0;
            int dash_length = static_cast<int>(std::ceil(5.0 * scale_factor));
            int gap_length = static_cast<int>(std::ceil(5.0 * scale_factor));

            for (std::size_t point_index = 0; point_index < bresenham_points.size(); ++point_index) {
                int x = std::get<0>(bresenham_points[point_index]);
                int z = std::get<2>(bresenham_points[point_index]);
                int current_y = calculate_point_elevation(segment_index, point_index, segment_length, total_segments, effective_elevation, effective_start_slope, effective_end_slope, slope_length);

                for (int dx = -block_range; dx <= block_range; ++dx) {
                    for (int dz = -block_range; dz <= block_range; ++dz) {
                        int set_x = x + dx;
                        int set_z = z + dz;

                        bool zebra = false;
                        if (highway_type == "footway") {
                            auto it_footway = element.tags().find("footway");
                            if (it_footway != element.tags().end() && it_footway->second == "crossing") {
                                bool is_horizontal = (std::abs(x2 - x1) >= std::abs(z2 - z1));
                                if (is_horizontal) {
                                    if ((set_x % 2 + 2) % 2 < 1) zebra = true;
                                } else {
                                    if ((set_z % 2 + 2) % 2 < 1) zebra = true;
                                }
                            }
                        }

                        if (zebra) {
                            editor.set_block(crate::block_definitions::WHITE_CONCRETE, set_x, current_y, set_z, std::optional<std::vector<crate::block_definitions::Block>>({ crate::block_definitions::BLACK_CONCRETE }), std::optional<std::vector<crate::block_definitions::Block>>());
                        } else {
                            editor.set_block(block_type, set_x, current_y, set_z, std::optional<std::vector<crate::block_definitions::Block>>(), std::optional<std::vector<crate::block_definitions::Block>>({ crate::block_definitions::BLACK_CONCRETE, crate::block_definitions::WHITE_CONCRETE }));
                        }

                        if (effective_elevation > 0 && current_y > 0) {
                            editor.set_block(crate::block_definitions::STONE_BRICKS, set_x, current_y - 1, set_z, std::optional<std::vector<crate::block_definitions::Block>>(), std::optional<std::vector<crate::block_definitions::Block>>());
                        }

                        if (effective_elevation != 0 && current_y > 0) {
                            add_highway_support_pillar(editor, set_x, current_y, set_z, dx, dz, block_range);
                        }
                    }
                }

                if (add_outline) {
                    for (int dz = -block_range; dz <= block_range; ++dz) {
                        int outline_x = x - block_range - 1;
                        int outline_z = z + dz;
                        editor.set_block(crate::block_definitions::LIGHT_GRAY_CONCRETE, outline_x, current_y, outline_z, std::optional<std::vector<crate::block_definitions::Block>>(), std::optional<std::vector<crate::block_definitions::Block>>());
                    }
                    for (int dz = -block_range; dz <= block_range; ++dz) {
                        int outline_x = x + block_range + 1;
                        int outline_z = z + dz;
                        editor.set_block(crate::block_definitions::LIGHT_GRAY_CONCRETE, outline_x, current_y, outline_z, std::optional<std::vector<crate::block_definitions::Block>>(), std::optional<std::vector<crate::block_definitions::Block>>());
                    }
                }

                if (add_stripe) {
                    if (stripe_length_counter < dash_length) {
                        int stripe_x = x;
                        int stripe_z = z;
                        editor.set_block(crate::block_definitions::WHITE_CONCRETE, stripe_x, current_y, stripe_z, std::optional<std::vector<crate::block_definitions::Block>>({ crate::block_definitions::BLACK_CONCRETE }), std::optional<std::vector<crate::block_definitions::Block>>());
                    }
                    ++stripe_length_counter;
                    if (stripe_length_counter >= dash_length + gap_length) {
                        stripe_length_counter = 0;
                    }
                }
            }

            ++segment_index;
        }
        previous_node = std::make_optional(std::pair<int,int>{ node.x, node.z });
    }
}

void generate_highways(crate::world_editor::WorldEditor& editor, const crate::osm_parser::ProcessedElement& element, const crate::args::Args& args, const std::vector<crate::osm_parser::ProcessedElement>& all_elements) {
    auto highway_connectivity = build_highway_connectivity_map(all_elements);
    generate_highways_internal(editor, element, args, highway_connectivity);
}

void generate_siding(crate::world_editor::WorldEditor& editor, const crate::osm_parser::ProcessedWay& element) {
    std::optional<crate::coordinate_system::cartesian::XZPoint> previous_node;
    crate::block_definitions::Block siding_block = crate::block_definitions::STONE_BRICK_SLAB;
    for (const auto& node : element.nodes) {
        crate::coordinate_system::cartesian::XZPoint current_node = node.xz();
        if (previous_node.has_value()) {
            std::vector<std::tuple<int,int,int>> bresenham_points = crate::bresenham::bresenham_line(previous_node->x, 0, previous_node->z, current_node.x, 0, current_node.z);
            for (const auto& p : bresenham_points) {
                int bx = std::get<0>(p);
                int bz = std::get<2>(p);
                if (!editor.check_for_block(bx, 0, bz, std::optional<std::vector<crate::block_definitions::Block>>({ crate::block_definitions::BLACK_CONCRETE, crate::block_definitions::WHITE_CONCRETE }))) {
                    editor.set_block(siding_block, bx, 1, bz, std::optional<std::vector<crate::block_definitions::Block>>(), std::optional<std::vector<crate::block_definitions::Block>>());
                }
            }
        }
        previous_node.emplace(current_node);
    }
}

void generate_aeroway(crate::world_editor::WorldEditor& editor, const crate::osm_parser::ProcessedWay& way, const crate::args::Args& args) {
    std::optional<std::pair<int,int>> previous_node;
    crate::block_definitions::Block surface_block = crate::block_definitions::LIGHT_GRAY_CONCRETE;
    for (const auto& node : way.nodes) {
        if (previous_node.has_value()) {
            int x1 = previous_node->first;
            int z1 = previous_node->second;
            int x2 = node.x;
            int z2 = node.z;
            std::vector<std::tuple<int,int,int>> points = crate::bresenham::bresenham_line(x1, 0, z1, x2, 0, z2);
            int way_width = static_cast<int>(std::ceil(12.0 * args.scale));
            for (const auto& p : points) {
                int x = std::get<0>(p);
                int z = std::get<2>(p);
                for (int dx = -way_width; dx <= way_width; ++dx) {
                    for (int dz = -way_width; dz <= way_width; ++dz) {
                        int set_x = x + dx;
                        int set_z = z + dz;
                        editor.set_block(surface_block, set_x, 0, set_z, std::optional<std::vector<crate::block_definitions::Block>>(), std::optional<std::vector<crate::block_definitions::Block>>());
                    }
                }
            }
        }
        previous_node = std::make_optional(std::pair<int,int>{ node.x, node.z });
    }
}

}
}