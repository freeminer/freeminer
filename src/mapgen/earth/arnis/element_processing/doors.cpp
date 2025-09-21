#include <string>
#include <optional>
#include <stdexcept>

#include "../../arnis_adapter.h"
namespace arnis
{

namespace doors
{


void generate_doors(crate::world_editor::WorldEditor& editor, crate::osm_parser::ProcessedNode const& element) {
    if (element.tags.find("door") != element.tags.end() || element.tags.find("entrance") != element.tags.end()) {
        auto it = element.tags.find("level");
        if (it != element.tags.end()) {
            try {
                int level = std::stoi(it->second);
                if (level != 0) {
                    return;
                }
            } catch (const std::invalid_argument&) {
                // ignore parse error
            } catch (const std::out_of_range&) {
                // ignore parse error
            }
        }

        int x = element.x;
        int z = element.z;

        editor.set_block(crate::block_definitions::GRAY_CONCRETE, x, 0, z, std::optional<int>{}, std::optional<int>{});
        editor.set_block(crate::block_definitions::DARK_OAK_DOOR_LOWER, x, 1, z, std::optional<int>{}, std::optional<int>{});
        editor.set_block(crate::block_definitions::DARK_OAK_DOOR_UPPER, x, 2, z, std::optional<int>{}, std::optional<int>{});
    }
}

}
}