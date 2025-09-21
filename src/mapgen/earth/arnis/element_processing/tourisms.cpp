#include <string>
#include <unordered_map>
#include <optional>
#include <stdexcept>


#include "../../arnis_adapter.h"
namespace arnis
{

namespace tourisms
{


#if 0

namespace crate {
namespace block_definitions {
constexpr int COBBLESTONE_WALL = 1;
constexpr int OAK_PLANKS = 2;
} // namespace block_definitions

namespace osm_parser {
struct ProcessedNode {
    std::unordered_map<std::string, std::string> tags;
    int x;
    int z;
};
} // namespace osm_parser

namespace world_editor {
struct WorldEditor {
    void set_block(int block, int x, int y, int z, std::optional<int> data, std::optional<int> meta) {
        // Implementation omitted.
    }
};
} // namespace world_editor

namespace generators {
#endif

std::optional<int> parse_int(const std::string& s) {
    try {
        size_t pos = 0;
        int v = std::stoi(s, &pos);
        if (pos != s.length()) {
            return std::nullopt;
        }
        return v;
    } catch (const std::exception&) {
        return std::nullopt;
    }
}


void generate_tourisms(crate::world_editor::WorldEditor& editor, const crate::osm_parser::ProcessedNode& element) {
    auto it_layer = element.tags.find("layer");
    if (it_layer != element.tags.end()) {
        auto opt_layer = parse_int(it_layer->second);
        if (opt_layer.has_value() && opt_layer.value() < 0) {
            return;
        }
    }

    auto it_level = element.tags.find("level");
    if (it_level != element.tags.end()) {
        auto opt_level = parse_int(it_level->second);
        if (opt_level.has_value() && opt_level.value() < 0) {
            return;
        }
    }

    auto it_tourism = element.tags.find("tourism");
    if (it_tourism != element.tags.end()) {
        const std::string& tourism_type = it_tourism->second;
        int x = element.x;
        int z = element.z;

        if (tourism_type == "information") {
            auto it_info = element.tags.find("information");
            if (it_info != element.tags.end()) {
                const std::string& info_type = it_info->second;
                if (info_type != "office" && info_type != "visitor_centre") {
                    editor.set_block(crate::block_definitions::COBBLESTONE_WALL, x, 1, z, std::nullopt, std::nullopt);
                    editor.set_block(crate::block_definitions::OAK_PLANKS, x, 2, z, std::nullopt, std::nullopt);
                }
            }
        }
    }
}

} // namespace generators
} // namespace crate


