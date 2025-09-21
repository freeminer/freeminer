#include <vector>
#include <tuple>
#include <cstddef>
#include <optional>
#include <string>

#include "block_definitions.hpp"
#include "bresenham.hpp"
#include "osm_parser.hpp"
#include "world_editor.hpp"

#include "../../arnis_adapter.h"

//using namespace arnis;
namespace arnis
{


void generate_bridges(world_editor::WorldEditor& editor, osm_parser::ProcessedWay const& element) {
    auto it = element.tags.find(std::string("bridge"));
    if (it != element.tags.end()) {
        int bridge_height = 3; // Fixed height

        for (std::size_t i = 1; i < element.nodes.size(); ++i) {
            const auto& prev = element.nodes[i - 1];
            const auto& cur = element.nodes[i];
            std::vector<std::tuple<int, int, int>> points = bresenham::bresenham_line(prev.x, 0, prev.z, cur.x, 0, cur.z);

            std::size_t total_length = points.size();
            std::size_t ramp_length = 6; // Length of ramp at each end

            for (std::size_t idx = 0; idx < points.size(); ++idx) {
                int x, y, z;
                std::tie(x, y, z) = points[idx];

                int height;
                if (idx < ramp_length) {
                    // Start ramp (rising)
                    height = static_cast<int>((static_cast<int>(idx) * bridge_height) / static_cast<int>(ramp_length));
                } else if (idx >= total_length - ramp_length) {
                    // End ramp (descending)
                    height = static_cast<int>(((static_cast<int>(total_length) - static_cast<int>(idx)) * bridge_height) / static_cast<int>(ramp_length));
                } else {
                    // Middle section (constant height)
                    height = bridge_height;
                }

                for (int dx = -2; dx <= 2; ++dx) {
                    editor.set_block(block_definitions::LIGHT_GRAY_CONCRETE, x + dx, height, z, std::optional<int>{}, std::optional<int>{});
                }
            }
        }
    }
}

}
