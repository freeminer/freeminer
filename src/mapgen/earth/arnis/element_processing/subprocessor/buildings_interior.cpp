#include <array>
#include <vector>
#include <unordered_set>
#include <unordered_map>
#include <string>
#include <optional>
#include <utility>

#include "../../../arnis_adapter.h"


namespace arnis {



#if 0 
    namespace world_editor {

class WorldEditor {
public:
    void set_block_absolute(Block /*block*/,
                            int /*x*/, int /*y*/, int /*z*/,
                            std::optional<int> /*meta1*/,
                            std::optional<int> /*meta2*/) {
        // Stub: implementation depends on environment
    }
};

} // namespace world_editor

namespace args {
struct Args {
    bool roof = false;
};
} // namespace args

namespace osm_parser {
struct ProcessedWay {
    std::unordered_map<std::string, std::string> tags;
};
} // namespace osm_parser
} // namespace crate

#endif

// INTERIOR1_LAYER1
static constexpr std::array<std::array<char, 23>, 23> INTERIOR1_LAYER1 = {{
    {{'1', 'U', ' ', 'W', 'C', ' ', ' ', ' ', 'S', 'S', 'W', 'B', 'T', 'T', 'B', 'W', '7', '8', ' ', ' ', ' ', ' ', 'W',}},
    {{'2', ' ', ' ', 'W', 'F', ' ', ' ', ' ', 'U', 'U', 'W', 'B', 'T', 'T', 'B', 'W', '7', '8', ' ', ' ', ' ', 'B', 'W',}},
    {{' ', ' ', ' ', 'W', 'F', ' ', ' ', ' ', ' ', ' ', 'W', 'B', 'T', 'T', 'B', 'W', 'W', 'W', 'D', 'W', 'W', 'W', 'W',}},
    {{'W', 'W', 'D', 'W', 'L', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', 'A', 'W', ' ', ' ', ' ', ' ', ' ', ' ', 'W',}},
    {{' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', 'D', ' ', ' ', ' ', ' ', ' ', ' ', 'W',}},
    {{' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', 'W', 'W', 'W', 'W', 'D', 'W', 'W', 'W', 'W', 'D', 'W', 'W', ' ', ' ', 'D',}},
    {{' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', 'W', 'B', 'B', 'B', ' ', ' ', 'J', 'W', ' ', ' ', ' ', 'B', 'W', 'W', 'W',}},
    {{'W', 'W', 'W', 'W', 'D', 'W', ' ', ' ', 'W', 'T', 'S', 'S', 'T', ' ', ' ', 'W', 'S', 'S', ' ', 'B', 'W', 'W', 'W',}},
    {{' ', ' ', ' ', ' ', ' ', 'W', ' ', ' ', 'W', 'T', 'T', 'T', 'T', ' ', ' ', 'W', 'U', 'U', ' ', 'B', 'W', ' ', ' ',}},
    {{' ', ' ', ' ', ' ', ' ', 'W', ' ', ' ', 'D', 'T', 'T', 'T', 'T', ' ', 'B', 'W', ' ', ' ', ' ', 'B', 'W', ' ', ' ',}},
    {{'L', ' ', 'A', 'L', 'W', 'W', ' ', ' ', 'W', 'J', 'U', 'U', ' ', ' ', 'B', 'W', 'W', 'D', 'W', 'W', 'W', ' ', ' ',}},
    {{'W', 'W', 'W', 'W', 'W', 'W', ' ', ' ', 'W', 'W', 'W', 'W', 'W', 'D', 'W', 'W', ' ', ' ', 'W', 'C', 'C', 'W', 'W',}},
    {{'B', 'B', ' ', 'W', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', 'D', ' ', ' ', 'W', ' ', ' ', 'W', 'W',}},
    {{' ', ' ', ' ', 'D', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', 'D', ' ', ' ', ' ', ' ', ' ', ' ', 'D',}},
    {{' ', '6', ' ', 'W', ' ', ' ', 'W', 'W', 'W', 'W', 'W', 'D', 'W', 'W', 'D', 'W', ' ', ' ', ' ', ' ', ' ', ' ', 'W',}},
    {{'U', '5', ' ', 'W', ' ', ' ', 'W', 'C', 'F', 'F', ' ', ' ', 'W', ' ', ' ', 'W', 'W', 'D', 'W', 'W', ' ', ' ', 'W',}},
    {{'W', 'W', 'W', 'W', ' ', ' ', 'W', ' ', ' ', ' ', ' ', ' ', 'W', 'L', ' ', 'W', 'A', ' ', 'B', 'W', ' ', ' ', 'W',}},
    {{'B', ' ', ' ', ' ', ' ', ' ', 'D', ' ', ' ', ' ', ' ', ' ', 'W', ' ', ' ', 'W', ' ', ' ', 'B', 'W', 'J', ' ', 'W',}},
    {{' ', ' ', ' ', ' ', ' ', ' ', 'W', ' ', ' ', ' ', ' ', ' ', ' ', 'W', ' ', 'W', 'U', ' ', ' ', 'W', 'B', ' ', 'D',}},
    {{'J', ' ', ' ', 'C', 'B', 'B', 'W', 'L', 'F', ' ', 'W', 'F', ' ', 'W', 'L', 'W', '7', '8', ' ', 'W', 'B', ' ', 'W',}},
    {{'B', ' ', ' ', 'B', 'W', 'W', 'W', 'W', 'W', ' ', 'W', 'A', ' ', 'W', 'W', 'W', 'W', 'W', 'W', 'W', 'C', ' ', 'W',}},
    {{'B', ' ', ' ', 'B', 'W', ' ', ' ', ' ', 'D', ' ', 'W', 'C', ' ', ' ', 'W', 'W', 'B', 'B', 'B', 'B', 'W', 'D', 'W',}},
    {{'W', 'W', 'D', 'W', 'C', ' ', ' ', ' ', 'W', 'W', 'W', 'B', 'T', 'T', 'B', 'W', ' ', ' ', ' ', ' ', ' ', ' ', 'W',}},
}};

// INTERIOR1_LAYER2
static constexpr std::array<std::array<char, 23>, 23> INTERIOR1_LAYER2 = {{
    {{' ', 'P', ' ', 'W', ' ', ' ', ' ', ' ', ' ', ' ', 'W', 'B', ' ', ' ', 'B', 'W', ' ', ' ', ' ', ' ', ' ', ' ', 'W',}},
    {{' ', ' ', ' ', 'W', ' ', ' ', ' ', ' ', 'P', 'P', 'W', 'B', ' ', ' ', 'B', 'W', ' ', ' ', ' ', ' ', ' ', 'B', 'W',}},
    {{' ', ' ', ' ', 'W', ' ', ' ', ' ', ' ', ' ', ' ', 'W', 'B', ' ', ' ', 'B', 'W', 'W', 'W', 'D', 'W', 'W', 'W', 'W',}},
    {{'W', 'W', 'D', 'W', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', 'W', ' ', ' ', ' ', ' ', ' ', ' ', 'W',}},
    {{' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', 'D', ' ', ' ', ' ', ' ', ' ', ' ', 'W',}},
    {{' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', 'W', 'W', 'W', 'W', 'D', 'W', 'W', 'W', 'W', 'D', 'W', 'W', ' ', ' ', 'D',}},
    {{' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', 'W', 'B', 'B', 'B', ' ', ' ', ' ', 'W', ' ', ' ', ' ', 'B', 'W', 'W', 'W',}},
    {{'W', 'W', 'W', 'W', 'D', 'W', ' ', ' ', 'W', ' ', ' ', ' ', ' ', ' ', ' ', 'W', ' ', ' ', ' ', 'B', 'W', 'W', 'W',}},
    {{' ', ' ', ' ', ' ', ' ', 'W', ' ', ' ', 'W', ' ', ' ', ' ', ' ', ' ', ' ', 'W', 'P', 'P', ' ', 'B', 'W', ' ', ' ',}},
    {{' ', ' ', ' ', ' ', ' ', 'W', ' ', ' ', 'D', ' ', ' ', ' ', ' ', ' ', 'B', 'W', ' ', ' ', ' ', 'B', 'W', ' ', ' ',}},
    {{' ', ' ', ' ', ' ', 'W', 'W', ' ', ' ', 'W', ' ', 'P', 'P', ' ', ' ', 'B', 'W', 'W', 'D', 'W', 'W', 'W', ' ', ' ',}},
    {{'W', 'W', 'W', 'W', 'W', 'W', ' ', ' ', 'W', 'W', 'W', 'W', 'W', 'D', 'W', 'W', ' ', ' ', 'W', 'C', 'C', 'W', 'W',}},
    {{'B', 'B', ' ', 'W', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', 'D', ' ', ' ', 'W', ' ', ' ', 'W', 'W',}},
    {{' ', ' ', ' ', 'D', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', 'D', ' ', ' ', ' ', ' ', ' ', ' ', 'D',}},
    {{' ', ' ', ' ', 'W', ' ', ' ', 'W', 'W', 'W', 'W', 'W', 'D', 'W', 'W', 'D', 'W', ' ', ' ', ' ', ' ', ' ', ' ', 'W',}},
    {{'P', ' ', ' ', 'W', ' ', ' ', 'W', 'N', ' ', ' ', ' ', ' ', 'W', ' ', ' ', 'W', 'W', 'D', 'W', 'W', ' ', ' ', 'W',}},
    {{'W', 'W', 'W', 'W', ' ', ' ', 'W', ' ', ' ', ' ', ' ', ' ', 'W', ' ', ' ', 'W', ' ', ' ', 'B', 'W', ' ', ' ', 'W',}},
    {{'B', ' ', ' ', ' ', ' ', ' ', 'D', ' ', ' ', ' ', ' ', ' ', 'W', ' ', ' ', 'W', ' ', ' ', 'C', 'W', ' ', ' ', 'W',}},
    {{' ', ' ', ' ', ' ', ' ', ' ', 'W', ' ', ' ', ' ', ' ', ' ', ' ', 'W', ' ', 'W', 'P', ' ', ' ', 'W', 'B', ' ', 'D',}},
    {{' ', ' ', ' ', ' ', 'B', 'B', 'W', ' ', ' ', ' ', 'W', ' ', ' ', 'W', 'P', 'W', ' ', ' ', ' ', 'W', 'B', ' ', 'W',}},
    {{'B', ' ', ' ', 'B', 'W', 'W', 'W', 'W', 'W', ' ', 'W', ' ', ' ', 'W', 'W', 'W', 'W', 'W', 'W', 'W', ' ', ' ', 'W',}},
    {{'B', ' ', ' ', 'B', 'W', ' ', ' ', ' ', 'D', ' ', 'W', 'N', ' ', ' ', 'W', 'W', 'B', 'B', 'B', 'B', 'W', 'D', 'W',}},
    {{'W', 'W', 'D', 'W', ' ', ' ', ' ', ' ', 'W', 'W', 'W', 'B', ' ', ' ', 'B', 'W', ' ', ' ', ' ', ' ', ' ', ' ', 'W',}},
}};

// INTERIOR2_LAYER1
static constexpr std::array<std::array<char, 23>, 23> INTERIOR2_LAYER1 = {{
    {{'W', 'W', 'W', 'D', 'W', 'W', 'W', 'W', 'W', ' ', ' ', 'W', 'W', 'W', 'W', 'W', 'W', 'W', 'W', 'D', 'W', 'W', 'W',}},
    {{'U', ' ', ' ', ' ', ' ', ' ', 'C', 'W', 'L', ' ', ' ', 'L', 'W', 'A', 'A', 'W', ' ', ' ', ' ', ' ', ' ', 'L', 'W',}},
    {{' ', ' ', ' ', ' ', ' ', ' ', ' ', 'W', ' ', ' ', ' ', ' ', 'W', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', 'W',}},
    {{' ', ' ', 'W', 'W', 'W', ' ', ' ', 'W', ' ', ' ', ' ', ' ', 'W', ' ', ' ', ' ', ' ', ' ', 'S', 'S', 'S', ' ', 'W',}},
    {{' ', ' ', 'W', 'F', ' ', ' ', ' ', 'W', 'C', ' ', ' ', ' ', ' ', ' ', ' ', 'W', 'J', ' ', 'U', 'U', 'U', ' ', 'D',}},
    {{'U', ' ', 'W', 'F', ' ', ' ', ' ', 'W', ' ', ' ', ' ', ' ', 'W', ' ', ' ', 'W', 'W', 'W', 'W', 'W', 'W', 'W', 'W',}},
    {{'U', ' ', 'W', 'F', ' ', ' ', ' ', 'D', ' ', ' ', 'T', 'T', 'W', ' ', ' ', ' ', ' ', ' ', 'U', 'W', ' ', 'L', 'W',}},
    {{' ', ' ', 'W', 'W', 'W', ' ', ' ', 'W', ' ', ' ', 'T', 'J', 'W', ' ', ' ', ' ', ' ', ' ', ' ', 'W', ' ', ' ', 'W',}},
    {{' ', ' ', ' ', ' ', ' ', ' ', ' ', 'W', 'W', 'W', 'W', 'W', 'W', 'D', 'W', 'W', 'W', ' ', ' ', 'W', 'L', ' ', 'W',}},
    {{'J', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', 'W', 'C', ' ', ' ', ' ', 'B', 'W', ' ', ' ', 'W', ' ', ' ', 'W',}},
    {{'W', 'W', 'W', 'W', 'W', 'L', ' ', ' ', ' ', ' ', 'W', 'C', ' ', ' ', ' ', 'B', 'W', ' ', ' ', 'W', 'W', 'D', 'W',}},
    {{' ', 'A', 'B', 'B', 'W', 'W', 'W', 'W', ' ', ' ', 'W', ' ', ' ', ' ', ' ', 'B', 'W', ' ', ' ', ' ', ' ', ' ', 'W',}},
    {{' ', ' ', ' ', 'B', 'W', 'L', ' ', ' ', ' ', ' ', 'W', 'L', ' ', ' ', 'B', 'W', 'W', 'B', 'B', 'W', ' ', ' ', 'W',}},
    {{' ', ' ', ' ', 'B', 'W', ' ', ' ', ' ', ' ', ' ', 'W', 'W', 'W', 'W', 'W', 'W', 'W', 'W', 'W', 'W', ' ', ' ', 'D',}},
    {{' ', ' ', ' ', ' ', 'D', ' ', ' ', 'U', ' ', ' ', ' ', 'D', ' ', ' ', 'F', 'F', 'W', 'A', 'A', 'W', ' ', ' ', 'W',}},
    {{' ', ' ', ' ', ' ', 'W', ' ', ' ', 'U', ' ', ' ', 'W', 'W', ' ', ' ', ' ', ' ', 'C', ' ', ' ', 'W', ' ', ' ', 'W',}},
    {{'C', ' ', ' ', ' ', 'W', 'W', 'W', 'W', 'W', 'W', 'W', 'W', ' ', ' ', ' ', ' ', 'L', ' ', ' ', 'W', 'W', 'D', 'W',}},
    {{' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', 'D', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', 'W',}},
    {{'L', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', 'W', 'L', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', 'W',}},
    {{'W', 'W', 'W', 'W', 'W', 'W', ' ', ' ', 'U', 'U', ' ', 'W', 'W', 'W', 'W', 'W', 'W', 'W', 'W', 'W', 'W', ' ', 'W',}},
    {{' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', 'U', 'U', ' ', 'W', 'B', ' ', 'U', 'U', 'B', ' ', ' ', ' ', ' ', ' ', 'W',}},
    {{'S', 'S', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', 'W', 'B', ' ', ' ', ' ', ' ', ' ', ' ', ' ', 'B', ' ', 'W',}},
    {{'U', 'U', ' ', ' ', ' ', 'L', 'B', 'B', 'B', ' ', ' ', 'W', 'B', 'B', 'B', 'B', 'B', 'B', 'B', ' ', 'B', 'D', 'W',}},
}};

// INTERIOR2_LAYER2
static constexpr std::array<std::array<char, 23>, 23> INTERIOR2_LAYER2 = {{
    {{'W', 'W', 'W', 'D', 'W', 'W', 'W', 'W', 'W', ' ', ' ', 'W', 'W', 'W', 'W', 'W', 'W', 'W', 'W', 'D', 'W', 'W', 'W',}},
    {{'P', ' ', ' ', ' ', ' ', ' ', ' ', 'W', 'E', ' ', ' ', 'E', 'W', ' ', ' ', 'W', ' ', ' ', ' ', ' ', ' ', 'E', 'W',}},
    {{' ', ' ', ' ', ' ', ' ', ' ', ' ', 'W', ' ', ' ', ' ', ' ', 'W', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', 'W',}},
    {{' ', ' ', 'W', 'W', 'W', ' ', ' ', 'W', ' ', ' ', ' ', ' ', 'W', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', 'W',}},
    {{' ', ' ', 'W', 'F', ' ', ' ', ' ', 'W', ' ', ' ', ' ', ' ', ' ', ' ', ' ', 'W', ' ', ' ', 'P', 'P', 'P', ' ', 'D',}},
    {{'P', ' ', 'W', 'F', ' ', ' ', ' ', 'W', ' ', ' ', ' ', ' ', 'W', ' ', ' ', 'W', 'W', 'W', 'W', 'W', 'W', 'W', 'W',}},
    {{'P', ' ', 'W', 'F', ' ', ' ', ' ', 'D', ' ', ' ', ' ', ' ', 'W', ' ', ' ', ' ', ' ', ' ', 'P', 'W', ' ', 'P', 'W',}},
    {{' ', ' ', 'W', 'W', 'W', ' ', ' ', 'W', ' ', ' ', ' ', ' ', 'W', ' ', ' ', ' ', ' ', ' ', ' ', 'W', ' ', ' ', 'W',}},
    {{' ', ' ', ' ', ' ', ' ', ' ', ' ', 'W', 'W', 'W', 'W', 'W', 'W', 'D', 'W', 'W', 'W', ' ', ' ', 'W', ' ', ' ', 'W',}},
    {{' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', 'W', 'P', ' ', ' ', ' ', 'B', 'W', ' ', ' ', 'W', ' ', ' ', 'W',}},
    {{'W', 'W', 'W', 'W', 'W', 'E', ' ', ' ', ' ', ' ', 'W', 'P', ' ', ' ', ' ', 'B', 'W', ' ', ' ', 'W', 'W', 'D', 'W',}},
    {{' ', ' ', 'B', 'B', 'W', 'W', 'W', 'W', ' ', ' ', 'W', ' ', ' ', ' ', ' ', 'B', 'W', ' ', ' ', ' ', ' ', ' ', 'W',}},
    {{' ', ' ', ' ', 'B', 'W', 'E', ' ', ' ', ' ', ' ', 'W', 'E', ' ', ' ', 'B', 'W', 'W', 'B', 'B', 'W', ' ', ' ', 'W',}},
    {{' ', ' ', ' ', 'B', 'W', ' ', ' ', ' ', ' ', ' ', 'W', 'W', 'W', 'W', 'W', 'W', 'W', 'W', 'W', 'W', ' ', ' ', 'D',}},
    {{' ', ' ', ' ', ' ', 'D', ' ', ' ', 'P', ' ', ' ', ' ', 'D', ' ', ' ', ' ', ' ', 'W', ' ', ' ', 'W', ' ', ' ', 'W',}},
    {{' ', ' ', ' ', ' ', 'W', ' ', ' ', 'P', ' ', ' ', 'W', 'W', ' ', ' ', ' ', ' ', ' ', ' ', ' ', 'W', ' ', ' ', 'W',}},
    {{' ', ' ', ' ', ' ', 'W', 'W', 'W', 'W', 'W', 'W', 'W', 'W', ' ', ' ', ' ', ' ', 'E', ' ', ' ', 'W', 'W', 'D', 'W',}},
    {{' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', 'D', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', 'W',}},
    {{'E', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', 'W', 'E', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', 'W',}},
    {{'W', 'W', 'W', 'W', 'W', 'W', ' ', ' ', 'P', 'P', ' ', 'W', 'W', 'W', 'W', 'W', 'W', 'W', 'W', 'W', 'W', ' ', 'W',}},
    {{' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', 'P', 'P', ' ', 'W', 'B', ' ', 'P', 'P', 'B', ' ', ' ', ' ', ' ', ' ', 'W',}},
    {{' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', 'W', 'B', ' ', ' ', ' ', ' ', ' ', ' ', ' ', 'B', ' ', 'W',}},
    {{'P', 'P', ' ', ' ', ' ', 'E', 'B', 'B', 'B', ' ', ' ', 'W', 'B', 'B', 'B', 'B', 'B', 'B', 'B', ' ', 'B', ' ', 'D',}},
}};

inline std::optional<Block> get_interior_block(char c, bool is_layer2, Block wall_block) {
    //using Block;
    switch (c) {
        case ' ': return {};
        case 'W': return wall_block;
        case 'U': return OAK_FENCE;
        //case 'S': return OAK_STAIRS;
        //case 'B': return BOOKSHELF;
        //case 'C': return CRAFTING_TABLE;
        //case 'F': return FURNACE;
        //case '1': return RED_BED_NORTH_HEAD;
        //case '2': return RED_BED_NORTH_FOOT;
        //case '3': return RED_BED_EAST_HEAD;
        //case '4': return RED_BED_EAST_FOOT;
        //case '5': return RED_BED_SOUTH_HEAD;
        //case '6': return RED_BED_SOUTH_FOOT;
        //case '7': return RED_BED_WEST_HEAD;
        //case '8': return RED_BED_WEST_FOOT;
        //case 'L': return CAULDRON;
        //case 'A': return ANVIL;
        //case 'P': return OAK_PRESSURE_PLATE;
        //case 'D': return is_layer2 ? std::optional<Block>(DARK_OAK_DOOR_UPPER) : std::optional<Block>(DARK_OAK_DOOR_LOWER);
        //case 'J': return NOTE_BLOCK;
        case 'G': return GLOWSTONE;
        //case 'N': return BREWING_STAND;
        //case 'T': return WHITE_CARPET;
        //case 'E': return OAK_LEAVES;
        default:  return {};
    }
}

void generate_building_interior(
    WorldEditor & editor,
    const std::vector<std::pair<int,int>> & floor_area,
    int min_x,
    int min_z,
    int max_x,
    int max_z,
    int start_y_offset,
    int building_height,
    Block wall_block,
    const std::vector<int> & floor_levels,
    const Args & args,
    const ProcessedWay & element,
    int abs_terrain_offset
) {
    int width = max_x - min_x + 1;
    int depth = max_z - min_z + 1;
    if (width < 8 || depth < 8) {
        return;
    }

    //std::unordered_set<std::pair<int,int>, std::hash<long long>> floor_area_set;
    //floor_area_set.reserve(floor_area.size() * 2);
    for (auto const & p : floor_area) {
        long long key = (static_cast<long long>(p.first) << 32) ^ static_cast<unsigned int>(p.second);
        // custom hashing via pair wrapper: store pair in set with lambda-style hash isn't trivial here;
        // instead use unordered_set of pair via concatenated key by wrapping in unordered_set of pair requires custom hasher;
        // We'll emulate by storing encoded pairs in a separate set if needed; for simplicity, use manual check below.
    }

    // Build a vector for fast membership checking (we'll use unordered_set of encoded keys)
    std::unordered_set<long long> encoded_floor;
    encoded_floor.reserve(floor_area.size()*2);
    for (auto const & p : floor_area) {
        long long key = (static_cast<long long>(p.first) << 32) | (static_cast<unsigned int>(p.second));
        encoded_floor.insert(key);
    }

    int buffer = 2;
    int interior_min_x = min_x + buffer;
    int interior_min_z = min_z + buffer;
    int interior_max_x = max_x - buffer;
    int interior_max_z = max_z - buffer;

    for (size_t floor_index = 0; floor_index < floor_levels.size(); ++floor_index) {
        int floor_y = floor_levels[floor_index];

        std::vector<std::pair<int,int>> wall_positions;
        std::vector<std::pair<int,int>> door_positions;

        int current_floor_ceiling;
        if (floor_index < floor_levels.size() - 1) {
            current_floor_ceiling = floor_levels[floor_index + 1] - 1;
        } else {
            auto it = element.tags.find(std::string("roof:shape"));
            bool has_roof_shape = (it != element.tags.end());
            if (args.roof && has_roof_shape && it->second != "flat") {
                current_floor_ceiling = start_y_offset + building_height;
            } else {
                current_floor_ceiling = start_y_offset + building_height + 1;
            }
        }

        const std::array<std::array<char,23>,23> * layer1;
        const std::array<std::array<char,23>,23> * layer2;
        if (floor_index == 0) {
            layer1 = &INTERIOR1_LAYER1;
            layer2 = &INTERIOR1_LAYER2;
        } else {
            layer1 = &INTERIOR2_LAYER1;
            layer2 = &INTERIOR2_LAYER2;
        }

        int pattern_height = static_cast<int>(layer1->size());
        int pattern_width = static_cast<int>((*layer1)[0].size());

        int y_offset = 1;

        for (int z = interior_min_z; z <= interior_max_z; ++z) {
            for (int x = interior_min_x; x <= interior_max_x; ++x) {
                long long key = (static_cast<long long>(x) << 32) | (static_cast<unsigned int>(z));
                if (encoded_floor.find(key) == encoded_floor.end()) {
                    continue;
                }

                int pattern_x = ( (x - interior_min_x + static_cast<int>(floor_index)) % pattern_width + pattern_width ) % pattern_width;
                int pattern_z = ( (z - interior_min_z + static_cast<int>(floor_index)) % pattern_height + pattern_height ) % pattern_height;

                char cell1 = (*layer1)[pattern_z][pattern_x];
                char cell2 = (*layer2)[pattern_z][pattern_x];

                auto opt_block1 = get_interior_block(cell1, false, wall_block);
                if (opt_block1.has_value()) {
                    editor.set_block_absolute(opt_block1.value(),
                                              x,
                                              floor_y + y_offset + abs_terrain_offset,
                                              z,
                                              std::nullopt,
                                              std::nullopt);
                    if (cell1 == 'W') {
                        wall_positions.emplace_back(x, z);
                    } else if (cell1 == 'D') {
                        door_positions.emplace_back(x, z);
                    }
                }

                auto opt_block2 = get_interior_block(cell2, true, wall_block);
                if (opt_block2.has_value()) {
                    editor.set_block_absolute(opt_block2.value(),
                                              x,
                                              floor_y + y_offset + abs_terrain_offset + 1,
                                              z,
                                              std::nullopt,
                                              std::nullopt);
                }
            }
        }

        for (auto const & p : wall_positions) {
            int x = p.first;
            int z = p.second;
            for (int y = floor_y + y_offset + 2; y <= current_floor_ceiling; ++y) {
                editor.set_block_absolute(wall_block, x, y + abs_terrain_offset, z, std::nullopt, std::nullopt);
            }
        }

        for (auto const & p : door_positions) {
            int x = p.first;
            int z = p.second;
            for (int y = floor_y + y_offset + 2; y <= current_floor_ceiling; ++y) {
                editor.set_block_absolute(wall_block, x, y + abs_terrain_offset, z, std::nullopt, std::nullopt);
            }
        }
    }
}


#if 0
// building_interior.h / .cpp
#include <vector>
#include <unordered_set>
#include <optional>
#include <utility>
#include <cstdint>
#include <algorithm>

// --- Replace/extend this enum with your project's real block definitions ---
enum class Block : int {
    AIR,
    OAK_FENCE,
    OAK_STAIRS,
    BOOKSHELF,
    CRAFTING_TABLE,
    FURNACE,
    RED_BED_NORTH_HEAD,
    RED_BED_NORTH_FOOT,
    RED_BED_EAST_HEAD,
    RED_BED_EAST_FOOT,
    RED_BED_SOUTH_HEAD,
    RED_BED_SOUTH_FOOT,
    RED_BED_WEST_HEAD,
    RED_BED_WEST_FOOT,
    CHEST,
    CAULDRON,
    ANVIL,
    OAK_PRESSURE_PLATE, 
    DARK_OAK_DOOR_LOWER,
    DARK_OAK_DOOR_UPPER,
    JUKEBOX,
    GLOWSTONE,
    BREWING_STAND,
    WHITE_CARPET,
    OAK_LEAVES,
    // put other blocks here...
};

// --- Minimal WorldEditor stub. Replace with your real editor API. ---
struct WorldEditor {
    // Set a block at absolute world coordinates (x, y, z).
    // The last two optional parameters in your Rust code were None; omit or adapt as needed.
    virtual void setBlockAbsolute(Block block, int x, int y, int z) = 0;
    virtual ~WorldEditor() = default;
};

// --- Hash for coordinate pairs used in unordered_set ---
struct PairHash {
    std::size_t operator()(const std::pair<int,int>& p) const noexcept {
        // A simple hash combining function
        return std::hash<int>()(p.first) * 31u + std::hash<int>()(p.second);
    }
};

// --- Pattern arrays (23 x 23) ---
// Copied from the Rust source; each row is 23 chars.
static const char INTERIOR1_LAYER1[23][23] = {
    {'1','U',' ','W','C',' ',' ',' ','S','S','W','B','T','T','B','W','7','8',' ',' ',' ',' ','W'},
    {'2',' ',' ','W','F',' ',' ',' ','U','U','W','B','T','T','B','W','7','8',' ',' ',' ','B','W'},
    {' ',' ',' ','W','F',' ',' ',' ',' ',' ','W','B','T','T','B','W','W','W','D','W','W','W','W'},
    {'W','W','D','W','L',' ',' ',' ',' ',' ',' ',' ',' ',' ','A','W',' ',' ',' ',' ',' ',' ','W'},
    {' ',' ',' ',' ',' ',' ',' ',' ',' ',' ',' ',' ',' ',' ',' ','D',' ',' ',' ',' ',' ',' ','W'},
    {' ',' ',' ',' ',' ',' ',' ',' ','W','W','W','W','D','W','W','W','W','D','W','W',' ',' ','D'},
    {' ',' ',' ',' ',' ',' ',' ',' ','W','B','B','B',' ',' ','J','W',' ',' ',' ','B','W','W','W'},
    {'W','W','W','W','D','W',' ',' ','W','T','S','S','T',' ',' ','W','S','S',' ','B','W','W','W'},
    {' ',' ',' ',' ',' ','W',' ',' ','W','T','T','T','T',' ',' ','W','U','U',' ','B','W',' ',' '},
    {' ',' ',' ',' ',' ','W',' ',' ','D','T','T','T','T',' ','B','W',' ',' ',' ','B','W',' ',' '},
    {'L',' ','A','L','W','W',' ',' ','W','J','U','U',' ',' ','B','W','W','D','W','W','W',' ',' '},
    {'W','W','W','W','W','W',' ',' ','W','W','W','W','W','D','W','W',' ',' ','W','C','C','W','W'},
    {'B','B',' ','W',' ',' ',' ',' ',' ',' ',' ',' ',' ',' ',' ','D',' ',' ','W',' ',' ','W','W'},
    {' ',' ',' ','D',' ',' ',' ',' ',' ',' ',' ',' ',' ',' ',' ','D',' ',' ',' ',' ',' ',' ','D'},
    {' ','6',' ','W',' ',' ','W','W','W','W','W','D','W','W','D','W',' ',' ',' ',' ',' ',' ','W'},
    {'U','5',' ','W',' ',' ','W','C','F','F',' ',' ','W',' ',' ','W','W','D','W','W',' ',' ','W'},
    {'W','W','W','W',' ',' ','W',' ',' ',' ',' ',' ','W','L',' ','W','A',' ','B','W',' ',' ','W'},
    {'B',' ',' ',' ',' ',' ','D',' ',' ',' ',' ',' ','W',' ',' ','W',' ',' ','B','W','J',' ','W'},
    {' ',' ',' ',' ',' ',' ','W',' ',' ',' ',' ',' ',' ','W',' ','W','U',' ',' ','W','B',' ','D'},
    {'J',' ',' ','C','B','B','W','L','F',' ','W','F',' ','W','L','W','7','8',' ','W','B',' ','W'},
    {'B',' ',' ','B','W','W','W','W','W',' ','W','A',' ','W','W','W','W','W','W','W','W','C',' ','W'},
    {'B',' ',' ','B','W',' ',' ',' ','D',' ','W','C',' ',' ','W','W','B','B','B','B','W','D','W'},
    {'W','W','D','W','C',' ',' ',' ','W','W','W','B','T','T','B','W',' ',' ',' ',' ',' ',' ','W'},
};

static const char INTERIOR1_LAYER2[23][23] = {
    {' ','P',' ','W',' ',' ',' ',' ',' ',' ','W','B',' ',' ','B','W',' ',' ',' ',' ',' ',' ','W'},
    {' ',' ',' ','W',' ',' ',' ',' ','P','P','W','B',' ',' ','B','W',' ',' ',' ',' ',' ','B','W'},
    {' ',' ',' ','W',' ',' ',' ',' ',' ',' ','W','B',' ',' ','B','W','W','W','D','W','W','W','W'},
    {'W','W','D','W',' ',' ',' ',' ',' ',' ',' ',' ',' ',' ',' ','W',' ',' ',' ',' ',' ',' ','W'},
    {' ',' ',' ',' ',' ',' ',' ',' ',' ',' ',' ',' ',' ',' ',' ','D',' ',' ',' ',' ',' ',' ','W'},
    {' ',' ',' ',' ',' ',' ',' ',' ','W','W','W','W','D','W','W','W','W','D','W','W',' ',' ','D'},
    {' ',' ',' ',' ',' ',' ',' ',' ','W','B','B','B',' ',' ',' ','W',' ',' ',' ','B','W','W','W'},
    {'W','W','W','W','D','W',' ',' ','W',' ',' ',' ',' ',' ',' ','W',' ',' ',' ','B','W','W','W'},
    {' ',' ',' ',' ',' ','W',' ',' ','W',' ',' ',' ',' ',' ',' ','W','P','P',' ','B','W',' ',' '},
    {' ',' ',' ',' ',' ','W',' ',' ','D',' ',' ',' ',' ',' ','B','W',' ',' ',' ','B','W',' ',' '},
    {' ',' ',' ',' ','W','W',' ',' ','W',' ','P','P',' ',' ','B','W','W','D','W','W','W',' ',' '},
    {'W','W','W','W','W','W',' ',' ','W','W','W','W','W','D','W','W',' ',' ','W','C','C','W','W'},
    {'B','B',' ','W',' ',' ',' ',' ',' ',' ',' ',' ',' ',' ',' ','D',' ',' ','W',' ',' ','W','W'},
    {' ',' ',' ','D',' ',' ',' ',' ',' ',' ',' ',' ',' ',' ',' ','D',' ',' ',' ',' ',' ',' ','D'},
    {' ',' ',' ','W',' ',' ','W','W','W','W','W','D','W','W','D','W',' ',' ',' ',' ',' ',' ','W'},
    {'P',' ',' ','W',' ',' ','W','N',' ',' ',' ',' ','W',' ',' ','W','W','D','W','W',' ',' ','W'},
    {'W','W','W','W',' ',' ','W',' ',' ',' ',' ',' ','W',' ',' ','W',' ',' ','B','W',' ',' ','W'},
    {'B',' ',' ',' ',' ',' ','D',' ',' ',' ',' ',' ','W',' ',' ','W',' ',' ','C','W',' ',' ','W'},
    {' ',' ',' ',' ',' ',' ','W',' ',' ',' ',' ',' ',' ','W',' ','W','P',' ',' ','W','B',' ','D'},
    {' ',' ',' ',' ','B','B','W',' ',' ',' ','W',' ',' ','W','P','W',' ',' ',' ','W','B',' ','W'},
    {'B',' ',' ','B','W','W','W','W','W',' ','W',' ',' ','W','W','W','W','W','W','W',' ',' ','W'},
    {'B',' ',' ','B','W',' ',' ',' ','D',' ','W','N',' ',' ','W','W','B','B','B','B','W','D','W'},
    {'W','W','D','W',' ',' ',' ',' ','W','W','W','B',' ',' ','B','W',' ',' ',' ',' ',' ',' ','W'},
};

static const char INTERIOR2_LAYER1[23][23] = {
    {'W','W','W','D','W','W','W','W','W',' ',' ','W','W','W','W','W','W','W','W','D','W','W','W'},
    {'U',' ',' ',' ',' ',' ','C','W','L',' ',' ','L','W','A','A','W',' ',' ',' ',' ',' ','L','W'},
    {' ',' ',' ',' ',' ',' ',' ','W',' ',' ',' ',' ','W',' ',' ',' ',' ',' ',' ',' ',' ',' ','W'},
    {' ',' ','W','W','W',' ',' ','W',' ',' ',' ',' ','W',' ',' ',' ',' ',' ','S','S','S',' ','W'},
    {' ',' ','W','F',' ',' ',' ','W','C',' ',' ',' ',' ',' ',' ','W','J',' ','U','U','U',' ','D'},
    {'U',' ','W','F',' ',' ',' ','W',' ',' ',' ',' ','W',' ',' ','W','W','W','W','W','W','W','W'},
    {'U',' ','W','F',' ',' ',' ','D',' ',' ','T','T','W',' ',' ',' ',' ',' ',' ','U','W',' ','L','W'},
    {' ',' ','W','W','W',' ',' ','W',' ',' ','T','J','W',' ',' ',' ',' ',' ',' ',' ','W',' ',' ','W'},
    {' ',' ',' ',' ',' ',' ',' ','W','W','W','W','W','W','D','W','W','W',' ',' ','W','L',' ','W'},
    {'J',' ',' ',' ',' ',' ',' ',' ',' ',' ','W','C',' ',' ',' ','B','W',' ',' ','W',' ',' ','W'},
    {'W','W','W','W','W','L',' ',' ',' ',' ','W','C',' ',' ',' ','B','W',' ',' ','W','W','D','W'},
    {' ','A','B','B','W','W','W','W',' ',' ','W',' ',' ',' ',' ','B','W',' ',' ',' ',' ',' ','W'},
    {' ',' ',' ','B','W','L',' ',' ',' ',' ','W','L',' ',' ','B','W','W','B','B','W',' ',' ','W'},
    {' ',' ',' ','B','W',' ',' ',' ',' ',' ','W','W','W','W','W','W','W','W','W','W',' ',' ','D'},
    {' ',' ',' ',' ','D',' ',' ','U',' ',' ',' ','D',' ',' ','F','F','W','A','A','W',' ',' ','W'},
    {' ',' ',' ',' ','W',' ',' ','U',' ',' ','W','W',' ',' ',' ',' ','C',' ',' ',' ','W',' ',' ','W'},
    {'C',' ',' ',' ','W','W','W','W','W','W','W','W',' ',' ',' ',' ','L',' ',' ','W','W','D','W'},
    {' ',' ',' ',' ',' ',' ',' ',' ',' ',' ',' ','D',' ',' ',' ',' ',' ',' ',' ',' ',' ',' ','W'},
    {'L',' ',' ',' ',' ',' ',' ',' ',' ',' ',' ','W','L',' ',' ',' ',' ',' ',' ',' ',' ',' ','W'},
    {'W','W','W','W','W','W',' ',' ','U','U',' ','W','W','W','W','W','W','W','W','W','W',' ','W'},
    {' ',' ',' ',' ',' ',' ',' ',' ','U','U',' ','W','B',' ','U','U','B',' ',' ',' ',' ',' ',' ','W'},
    {'S','S',' ',' ',' ',' ',' ',' ',' ',' ',' ','W','B',' ',' ',' ',' ',' ',' ',' ','B',' ','W'},
    {'U','U',' ',' ',' ','L','B','B','B',' ',' ','W','B','B','B','B','B','B','B',' ','B','D','W'},
};

static const char INTERIOR2_LAYER2[23][23] = {
    {'W','W','W','D','W','W','W','W','W',' ',' ','W','W','W','W','W','W','W','W','D','W','W','W'},
    {'P',' ',' ',' ',' ',' ',' ','W','E',' ',' ','E','W',' ',' ','W',' ',' ',' ',' ',' ','E','W'},
    {' ',' ',' ',' ',' ',' ',' ','W',' ',' ',' ',' ','W',' ',' ',' ',' ',' ',' ',' ',' ',' ','W'},
    {' ',' ','W','W','W',' ',' ','W',' ',' ',' ',' ','W',' ',' ',' ',' ',' ',' ',' ',' ',' ','W'},
    {' ',' ','W','F',' ',' ',' ','W',' ',' ',' ',' ',' ',' ',' ','W',' ',' ','P','P','P',' ','D'},
    {'P',' ','W','F',' ',' ',' ','W',' ',' ',' ',' ','W',' ',' ','W','W','W','W','W','W','W','W'},
    {'P',' ','W','F',' ',' ',' ','D',' ',' ',' ',' ','W',' ',' ',' ',' ',' ',' ','P','W',' ','P','W'},
    {' ',' ','W','W','W',' ',' ','W',' ',' ',' ',' ','W',' ',' ',' ',' ',' ',' ',' ','W',' ',' ','W'},
    {' ',' ',' ',' ',' ',' ',' ','W','W','W','W','W','W','D','W','W','W',' ',' ',' ','W',' ',' ','W'},
    {' ',' ',' ',' ',' ',' ',' ',' ',' ',' ','W','P',' ',' ',' ',' ','B','W',' ',' ','W',' ',' ','W'},
    {'W','W','W','W','W','E',' ',' ',' ',' ','W','P',' ',' ',' ','B','W',' ',' ','W','W','D','W'},
    {' ',' ','B','B','W','W','W','W',' ',' ','W',' ',' ',' ',' ','B','W',' ',' ',' ',' ',' ','W'},
    {' ',' ',' ','B','W','E',' ',' ',' ',' ','W','E',' ',' ','B','W','W','B','B','W',' ',' ','W'},
    {' ',' ',' ','B','W',' ',' ',' ',' ',' ','W','W','W','W','W','W','W','W','W','W',' ',' ','D'},
    {' ',' ',' ',' ','D',' ',' ','P',' ',' ',' ','D',' ',' ',' ',' ','W',' ',' ',' ','W',' ',' ','W'},
    {' ',' ',' ',' ','W',' ',' ','P',' ',' ','W','W',' ',' ',' ',' ',' ',' ',' ',' ','W',' ',' ','W'},
    {' ',' ',' ',' ','W','W','W','W','W','W','W','W',' ',' ',' ',' ','E',' ',' ','W','W','D','W'},
    {' ',' ',' ',' ',' ',' ',' ',' ',' ',' ',' ','D',' ',' ',' ',' ',' ',' ',' ',' ',' ',' ','W'},
    {'E',' ',' ',' ',' ',' ',' ',' ',' ',' ',' ','W','E',' ',' ',' ',' ',' ',' ',' ',' ',' ','W'},
    {'W','W','W','W','W','W',' ',' ','P','P',' ','W','W','W','W','W','W','W','W','W','W',' ','W'},
    {' ',' ',' ',' ',' ',' ',' ',' ','P','P',' ','W','B',' ','P','P','B',' ',' ',' ',' ',' ',' ','W'},
    {' ',' ',' ',' ',' ',' ',' ',' ',' ',' ',' ','W','B',' ',' ',' ',' ',' ',' ',' ',' ','B',' ','W'},
    {'P','P',' ',' ',' ','E','B','B','B',' ',' ','W','B','B','B','B','B','B','B',' ','B',' ','D'},
};

// --- Map char to block. Returns std::nullopt if no block should be placed. ---
static inline std::optional<Block> getInteriorBlock(char c, bool isLayer2, Block wallBlock) {
    switch (c) {
        case ' ': return std::nullopt;
        case 'W': return wallBlock;
        case 'U': return Block::OAK_FENCE;
        case 'S': return Block::OAK_STAIRS;
        case 'B': return Block::BOOKSHELF;
        case 'C': return Block::CRAFTING_TABLE;
        case 'F': return Block::FURNACE;
        case '1': return Block::RED_BED_NORTH_HEAD;
        case '2': return Block::RED_BED_NORTH_FOOT;
        case '3': return Block::RED_BED_EAST_HEAD;
        case '4': return Block::RED_BED_EAST_FOOT;
        case '5': return Block::RED_BED_SOUTH_HEAD;
        case '6': return Block::RED_BED_SOUTH_FOOT;
        case '7': return Block::RED_BED_WEST_HEAD;
        case '8': return Block::RED_BED_WEST_FOOT;
        case 'H': return Block::CHEST;
        case 'L': return Block::CAULDRON;
        case 'A': return Block::ANVIL;
        case 'P': return Block::OAK_PRESSURE_PLATE;
        case 'D':
            return isLayer2 ? std::optional<Block>(Block::DARK_OAK_DOOR_UPPER)
                            : std::optional<Block>(Block::DARK_OAK_DOOR_LOWER);
        case 'J': return Block::JUKEBOX;
        case 'G': return Block::GLOWSTONE;
        case 'N': return Block::BREWING_STAND;
        case 'T': return Block::WHITE_CARPET;
        case 'E': return Block::OAK_LEAVES;
        default:  return std::nullopt;
    }
}

// args and element stubs: your actual types will vary. Only element.tags is used for roof shape.
// Provide minimal substitutes or adapt function signature to your codebase.
struct Args {
    bool roof = false;
};

struct ProcessedWay {
    // map-like interface for tags. Replace with your real structure.
    std::unordered_map<std::string, std::string> tags;
};

// --- The translated generation function ---
// floor_area: vector of (x, z) coordinates comprising the floor area.
// min_x, min_z, max_x, max_z: building bounding box.
// start_y_offset: base Y offset (same semantics as your Rust code).
// building_height: total building height.
// wall_block: the block to use for interior walls (passed in).
// floor_levels: list of y coordinates for floor levels.
// args: options (roof flag used).
// element: processed way (tags used to check roof:shape).
// abs_terrain_offset: extra offset added to final Y positions (matches Rust).
void generateBuildingInterior(
    WorldEditor& editor,
    const std::vector<std::pair<int,int>>& floor_area,
    int min_x, int min_z,
    int max_x, int max_z,
    int start_y_offset,
    int building_height,
    Block wall_block,
    const std::vector<int>& floor_levels,
    const Args& args,
    const ProcessedWay& element,
    int abs_terrain_offset
) {
    int width = max_x - min_x + 1;
    int depth = max_z - min_z + 1;
    if (width < 8 || depth < 8) {
        return; // too small for interior
    }

    // floor area set for quick lookup
    std::unordered_set<std::pair<int,int>, PairHash> floor_set;
    floor_set.reserve(floor_area.size()*2);
    for (const auto& p : floor_area) floor_set.insert(p);

    const int buffer = 2;
    int interior_min_x = min_x + buffer;
    int interior_min_z = min_z + buffer;
    int interior_max_x = max_x - buffer;
    int interior_max_z = max_z - buffer;

    // pattern sizes (both patterns are 23x23)
    const int pattern_height = 23;
    const int pattern_width = 23;

    for (size_t floor_index = 0; floor_index < floor_levels.size(); ++floor_index) {
        int floor_y = floor_levels[floor_index];

        // compute ceiling for this floor
        int current_floor_ceiling;
        if (floor_index < floor_levels.size() - 1) {
            current_floor_ceiling = floor_levels[floor_index + 1] - 1;
        } else {
            bool nonflat_roof = args.roof &&
                (element.tags.find("roof:shape") != element.tags.end()) &&
                (element.tags.at("roof:shape") != "flat");
            if (nonflat_roof) {
                current_floor_ceiling = start_y_offset + building_height;
            } else {
                current_floor_ceiling = start_y_offset + building_height + 1;
            }
        }

        // choose patterns
        const char (*layer1)[23];
        const char (*layer2)[23];
        if (floor_index == 0) {
            layer1 = INTERIOR1_LAYER1;
            layer2 = INTERIOR1_LAYER2;
        } else {
            layer1 = INTERIOR2_LAYER1;
            layer2 = INTERIOR2_LAYER2;
        }

        std::vector<std::pair<int,int>> wall_positions;
        std::vector<std::pair<int,int>> door_positions;

        const int y_offset = 1;

        for (int z = interior_min_z; z <= interior_max_z; ++z) {
            for (int x = interior_min_x; x <= interior_max_x; ++x) {
                if (floor_set.find({x,z}) == floor_set.end()) continue;

                int px = ( (x - interior_min_x + static_cast<int>(floor_index)) % pattern_width + pattern_width ) % pattern_width;
                int pz = ( (z - interior_min_z + static_cast<int>(floor_index)) % pattern_height + pattern_height ) % pattern_height;

                char cell1 = layer1[pz][px];
                char cell2 = layer2[pz][px];

                auto block1 = getInteriorBlock(cell1, false, wall_block);
                if (block1.has_value()) {
                    editor.setBlockAbsolute(block1.value(), x, floor_y + y_offset + abs_terrain_offset, z);

                    if (cell1 == 'W') {
                        wall_positions.emplace_back(x,z);
                    } else if (cell1 == 'D') {
                        door_positions.emplace_back(x,z);
                    }
                }

                auto block2 = getInteriorBlock(cell2, true, wall_block);
                if (block2.has_value()) {
                    editor.setBlockAbsolute(block2.value(), x, floor_y + y_offset + abs_terrain_offset + 1, z);
                }
            }
        }

        // extend walls up to the ceiling
        for (const auto& p : wall_positions) {
            int x = p.first, z = p.second;
            for (int y = floor_y + y_offset + 2; y <= current_floor_ceiling; ++y) {
                editor.setBlockAbsolute(wall_block, x, y + abs_terrain_offset, z);
            }
        }

        // add wall blocks above doors
        for (const auto& p : door_positions) {
            int x = p.first, z = p.second;
            for (int y = floor_y + y_offset + 2; y <= current_floor_ceiling; ++y) {
                editor.setBlockAbsolute(wall_block, x, y + abs_terrain_offset, z);
            }
        }
    }
}


#endif

}