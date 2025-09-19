#include "block_definitions.h"
#include "bresenham.h"
#include "osm_parser.h"
#include "world_editor.h"

#include <vector>
#include <string>
#include <unordered_map>
#include <optional>
#include <utility>
#include <tuple>
#include <algorithm>
#include <stdexcept>


#include "../../arnis_adapter.h"
#undef stoi
#undef stof
namespace arnis
{

    namespace waterways{

std::pair<int, int> get_waterway_dimensions(const std::string& waterway_type) {
    if (waterway_type == "river") return std::pair<int,int>(8, 3);
    if (waterway_type == "canal") return std::pair<int,int>(6, 2);
    if (waterway_type == "stream") return std::pair<int,int>(3, 2);
    if (waterway_type == "fairway") return std::pair<int,int>(12, 3);
    if (waterway_type == "flowline") return std::pair<int,int>(2, 1);
    if (waterway_type == "brook") return std::pair<int,int>(2, 1);
    if (waterway_type == "ditch") return std::pair<int,int>(2, 1);
    if (waterway_type == "drain") return std::pair<int,int>(1, 1);
    return std::pair<int,int>(4, 2);
}

void create_water_channel(
    WorldEditor& editor,
    int center_x,
    int center_z,
    int width,
    int depth
) {
    int half_width = width / 2;
    for (int x = center_x - half_width - 1; x <= center_x + half_width + 1; ++x) {
        for (int z = center_z - half_width - 1; z <= center_z + half_width + 1; ++z) {
            int dx = std::abs(x - center_x);
            int dz = std::abs(z - center_z);
            int distance_from_center = std::max(dx, dz);

            if (distance_from_center <= half_width) {
                for (int y = 1 - depth; y <= 0; ++y) {
                    editor.set_block(WATER, x, y, z, std::nullopt, std::nullopt);
                }

                editor.set_block(DIRT, x, -depth, z, std::nullopt, std::nullopt);

                editor.set_block(
                    AIR,
                    x,
                    1,
                    z,
                    std::optional<std::vector<Block>>(std::vector<Block>{GRASS, WHEAT, CARROTS, POTATOES}),
                    std::nullopt
                );
            } else if (distance_from_center == half_width + 1 && depth > 1) {
                int slope_depth = std::max(depth - 1, 1);
                for (int y = 1 - slope_depth; y <= 0; ++y) {
                    if (y == 0) {
                        editor.set_block(WATER, x, y, z, std::nullopt, std::nullopt);
                    } else {
                        editor.set_block(AIR, x, y, z, std::nullopt, std::nullopt);
                    }
                }

                editor.set_block(DIRT, x, -slope_depth, z, std::nullopt, std::nullopt);

                editor.set_block(
                    AIR,
                    x,
                    1,
                    z,
                    std::optional<std::vector<Block>>(std::vector<Block>{GRASS, WHEAT, CARROTS, POTATOES}),
                    std::nullopt
                );
            }
        }
    }
}

void generate_waterways(WorldEditor& editor, const ProcessedWay& element) {
    auto it = element.tags.find("waterway");
    if (it == element.tags.end()) {
        return;
    }

    std::pair<int,int> dims = get_waterway_dimensions(it->second);
    int waterway_width = dims.first;
    int waterway_depth = dims.second;

    auto width_it = element.tags.find("width");
    if (width_it != element.tags.end()) {
        const std::string& width_str = width_it->second;
        try {
            waterway_width = std::stoi(width_str);
        } catch (const std::exception&) {
            try {
                float f = std::stof(width_str);
                waterway_width = static_cast<int>(f);
            } catch (const std::exception&) {
                // keep default width
            }
        }
    }

    auto layer_it = element.tags.find("layer");
    if (layer_it != element.tags.end()) {
        const std::string& layer_val = layer_it->second;
        if (layer_val == "-1" || layer_val == "-2" || layer_val == "-3") {
            return;
        }
    }

    for (std::size_t i = 0; i + 1 < element.nodes.size(); ++i) {
        auto prev_node = element.nodes[i].xz();
        auto current_node = element.nodes[i + 1].xz();

        std::vector<std::tuple<int,int,int>> bresenham_points = bresenham_line(
            prev_node.x, 0, prev_node.z,
            current_node.x, 0, current_node.z
        );

        for (const auto& pt : bresenham_points) {
            int bx = std::get<0>(pt);
            int bz = std::get<2>(pt);
            create_water_channel(editor, bx, bz, waterway_width, waterway_depth);
        }
    }
}

}
}