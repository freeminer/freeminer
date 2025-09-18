#include <mutex>
#include <vector>
#include <string>
#include <random>
#include <algorithm>
#include <cstddef>
#include <cstdint>

#include "../arnis_adapter.h"

namespace arnis
{


/*
enum class Block {
    GLASS,
    GRAY_STAINED_GLASS,
    LIGHT_GRAY_STAINED_GLASS,
    BROWN_STAINED_GLASS,
    WHITE_STAINED_GLASS,
    TINTED_GLASS,
    WHITE_CONCRETE,
    GRAY_CONCRETE,
    LIGHT_GRAY_CONCRETE,
    POLISHED_ANDESITE,
    SMOOTH_STONE,
    STONE_BRICKS,
    MUD_BRICKS,
    OAK_PLANKS,
    BRICK,
    NETHER_BRICK,
    POLISHED_BLACKSTONE_BRICKS,
    BLACKSTONE,
    DEEPSLATE_BRICKS,
    LIGHT_BLUE_TERRACOTTA,
    POLISHED_BLACKSTONE,
    END_STONE_BRICKS,
    SANDSTONE,
    SMOOTH_SANDSTONE,
    BROWN_TERRACOTTA,
    BROWN_CONCRETE,
    GRAY_TERRACOTTA,
    NETHERITE_BLOCK,
    POLISHED_DEEPSLATE,
    POLISHED_GRANITE,
    QUARTZ_BRICKS,
    BLUE_TERRACOTTA,
    QUARTZ_BLOCK,
    WHITE_TERRACOTTA,
    BLACK_TERRACOTTA,
    CHISELED_STONE_BRICKS,
    CRACKED_STONE_BRICKS,
    COBBLESTONE,
    MOSSY_COBBLESTONE,
    ANDESITE
};
struct RGB {
    int r;
    int g;
    int b;
};
*/

namespace colors {
    inline std::int64_t rgb_distance(const RGB& a, const RGB& b) {
        /*
        std::int64_t dr = static_cast<std::int64_t>(a.r) - static_cast<std::int64_t>(b.r);
        std::int64_t dg = static_cast<std::int64_t>(a.g) - static_cast<std::int64_t>(b.g);
        std::int64_t db = static_cast<std::int64_t>(a.b) - static_cast<std::int64_t>(b.b);
        return dr * dr + dg * dg + db * db;
*/
	auto [ar, ag, ab] = a;
	auto [br, bg, bb] = b;
	// Simple Euclidean or squared difference
	int dr = int(ar) - int(br);
	int dg = int(ag) - int(bg);
	int db = int(ab) - int(bb);
	return dr * dr + dg * dg + db * db;
    }
}

static std::mt19937& global_rng() {
    static std::random_device rd;
    static std::mt19937 rng(rd());
    return rng;
}




//template <typename BlockT, typename ValueT>
class _______BlockWithProperties {
public:
    Block block;
    //std::optional<ValueT> properties;
/*
    BlockWithProperties() = default;
    BlockWithProperties(BlockWithProperties const&) = default;
    BlockWithProperties(BlockWithProperties&&) = default;
    BlockWithProperties& operator=(BlockWithProperties const&) = default;
    BlockWithProperties& operator=(BlockWithProperties&&) = default;
    ~BlockWithProperties() = default;

    BlockWithProperties(BlockT const& block_, std::optional<ValueT> properties_)
        : block(block_), properties(std::move(properties_)) {}

    BlockWithProperties(BlockT&& block_, std::optional<ValueT> properties_)
        : block(std::move(block_)), properties(std::move(properties_)) {}

    static BlockWithProperties simple(BlockT const& block_) {
        return BlockWithProperties(block_, std::nullopt);
    }

    static BlockWithProperties simple(BlockT&& block_) {
        return BlockWithProperties(std::move(block_), std::nullopt);
    }
        */
};



struct StairKeyHash {
    std::size_t operator()(std::tuple<std::uint8_t, StairFacing, StairShape> const& t) const noexcept {
        std::size_t h1 = std::hash<std::uint8_t>{}(std::get<0>(t));
        std::size_t h2 = std::hash<int>{}(static_cast<int>(std::get<1>(t)));
        std::size_t h3 = std::hash<int>{}(static_cast<int>(std::get<2>(t)));
        // boost::hash_combine style
        h1 ^= h2 + 0x9e3779b97f4a7c15ULL + (h1 << 6) + (h1 >> 2);
        h1 ^= h3 + 0x9e3779b97f4a7c15ULL + (h1 << 6) + (h1 >> 2);
        return h1;
    }
};

static std::mutex STAIR_CACHE_MUTEX;
static std::unordered_map<std::tuple<std::uint8_t, StairFacing, StairShape>, BlockWithProperties, StairKeyHash> STAIR_CACHE;
 struct Value { static Value String(std::string); static Value Compound(std::unordered_map<std::string, Value>); };

BlockWithProperties create_stair_with_properties(const Block& base_stair_block, StairFacing facing, StairShape shape) {
    auto cache_key = std::make_tuple(base_stair_block.id(), facing, shape);

    // Check cache first
    {
        std::lock_guard<std::mutex> lock(STAIR_CACHE_MUTEX);
        auto it = STAIR_CACHE.find(cache_key);
        if (it != STAIR_CACHE.end()) {
            return it->second;
        }
    }
/*
    // Create properties
    std::unordered_map<std::string, Value> map;
    map.emplace("facing", Value::String(std::string(StairFacing_as_str(facing))));

    // Only add shape if it's not straight (default)
    if (!(shape == StairShape::Straight)) {
        map.emplace("shape", Value::String(std::string(StairShape_as_str(shape))));
    }

    Value properties = Value::Compound(std::move(map));
    BlockWithProperties block_with_props(base_stair_block, std::optional<Value>(properties));
*/
    BlockWithProperties block_with_props(base_stair_block);

    // Cache the result
    {
        std::lock_guard<std::mutex> lock(STAIR_CACHE_MUTEX);
        STAIR_CACHE.emplace(cache_key, block_with_props);
    }

    return block_with_props;
}


Block get_stair_block_for_material(const Block &material) {
    switch (material) {
        /*
        case STONE_BRICKS:
            return STONE_BRICK_STAIRS;
        case MUD_BRICKS:
            return MUD_BRICK_STAIRS;
        case OAK_PLANKS:
            return OAK_STAIRS;
        case POLISHED_ANDESITE:
            return STONE_BRICK_STAIRS;
        case SMOOTH_STONE:
            return POLISHED_ANDESITE_STAIRS;
        case ANDESITE:
            return STONE_BRICK_STAIRS;
        case CHISELED_STONE_BRICKS:
            return STONE_BRICK_STAIRS;
        case BLACK_TERRACOTTA:
            return POLISHED_BLACKSTONE_BRICK_STAIRS;
        case BLACKSTONE:
            return POLISHED_BLACKSTONE_BRICK_STAIRS;
        case BLUE_TERRACOTTA:
            return MUD_BRICK_STAIRS;
        case BRICK:
            return BRICK_STAIRS;
        case BROWN_CONCRETE:
            return MUD_BRICK_STAIRS;
        case BROWN_TERRACOTTA:
            return MUD_BRICK_STAIRS;
        case DEEPSLATE_BRICKS:
            return STONE_BRICK_STAIRS;
        case END_STONE_BRICKS:
            return END_STONE_BRICK_STAIRS;
        case GRAY_CONCRETE:
            return POLISHED_BLACKSTONE_BRICK_STAIRS;
        case GRAY_TERRACOTTA:
            return MUD_BRICK_STAIRS;
        case LIGHT_BLUE_TERRACOTTA:
            return STONE_BRICK_STAIRS;
        case LIGHT_GRAY_CONCRETE:
            return STONE_BRICK_STAIRS;
        case NETHER_BRICK:
            return NETHER_BRICK_STAIRS;
        case POLISHED_BLACKSTONE:
            return POLISHED_BLACKSTONE_BRICK_STAIRS;
        case POLISHED_BLACKSTONE_BRICKS:
            return POLISHED_BLACKSTONE_BRICK_STAIRS;
        case POLISHED_DEEPSLATE:
            return STONE_BRICK_STAIRS;
        case POLISHED_GRANITE:
            return POLISHED_GRANITE_STAIRS;
        case QUARTZ_BLOCK:
            return POLISHED_DIORITE_STAIRS;
        case QUARTZ_BRICKS:
            return POLISHED_DIORITE_STAIRS;
        case SANDSTONE:
            return SMOOTH_SANDSTONE_STAIRS;
        case SMOOTH_SANDSTONE:
            return SMOOTH_SANDSTONE_STAIRS;
        case WHITE_CONCRETE:
            return QUARTZ_STAIRS;
        case WHITE_TERRACOTTA:
            return MUD_BRICK_STAIRS;
*/
            default:
            return STONE;
            //return STONE_BRICK_STAIRS;
    }
}




static const std::array<Block, 7> WINDOW_VARIATIONS = {
    GLASS,

    GRAY_STAINED_GLASS,
    LIGHT_GRAY_STAINED_GLASS,
    GRAY_STAINED_GLASS,
    BROWN_STAINED_GLASS,
    WHITE_STAINED_GLASS,
    TINTED_GLASS
    
};

static const std::vector<std::pair<RGB, std::vector<Block>>> DEFINED_COLORS = {
    { {233, 107, 57},  { BRICK, NETHER_BRICK } },
    { {18, 12, 13},    { POLISHED_BLACKSTONE_BRICKS, BLACKSTONE, DEEPSLATE_BRICKS } },
    { {76, 127, 153},  { LIGHT_BLUE_TERRACOTTA } },
    { {0, 0, 0},       { DEEPSLATE_BRICKS, BLACKSTONE, POLISHED_BLACKSTONE } },
    { {186, 195, 142}, { END_STONE_BRICKS, SANDSTONE, SMOOTH_SANDSTONE, LIGHT_GRAY_CONCRETE } },
    { {57, 41, 35},    { BROWN_TERRACOTTA, BROWN_CONCRETE, MUD_BRICKS, BRICK } },
    { {112, 108, 138}, { LIGHT_BLUE_TERRACOTTA, GRAY_TERRACOTTA, GRAY_CONCRETE } },
    { {122, 92, 66},   { MUD_BRICKS, BROWN_TERRACOTTA, SANDSTONE, BRICK } },
    { {24, 13, 14},    { NETHER_BRICK, BLACKSTONE, DEEPSLATE_BRICKS } },
    { {159, 82, 36},   { BROWN_TERRACOTTA, BRICK, POLISHED_GRANITE, BROWN_CONCRETE, NETHERITE_BLOCK, POLISHED_DEEPSLATE } },
    { {128, 128, 128}, { POLISHED_ANDESITE, LIGHT_GRAY_CONCRETE, SMOOTH_STONE, STONE_BRICKS } },
    { {174, 173, 174}, { POLISHED_ANDESITE, LIGHT_GRAY_CONCRETE, SMOOTH_STONE, STONE_BRICKS } },
    { {141, 101, 142}, { STONE_BRICKS, BRICK, MUD_BRICKS } },
    { {142, 60, 46},   { BLACK_TERRACOTTA, NETHERITE_BLOCK, NETHER_BRICK, POLISHED_GRANITE, POLISHED_DEEPSLATE, BROWN_TERRACOTTA } },
    { {153, 83, 28},   { BLACK_TERRACOTTA, POLISHED_GRANITE, BROWN_CONCRETE, BROWN_TERRACOTTA, STONE_BRICKS } },
    { {224, 216, 175}, { SMOOTH_SANDSTONE, LIGHT_GRAY_CONCRETE, POLISHED_ANDESITE, SMOOTH_STONE } },
    { {188, 182, 179}, { SMOOTH_SANDSTONE, LIGHT_GRAY_CONCRETE, QUARTZ_BRICKS, POLISHED_ANDESITE, SMOOTH_STONE } },
    { {35, 86, 85},    { POLISHED_BLACKSTONE_BRICKS, BLUE_TERRACOTTA, LIGHT_BLUE_TERRACOTTA } },
    { {255, 255, 255}, { WHITE_CONCRETE, QUARTZ_BRICKS, QUARTZ_BLOCK } },
    { {209, 177, 161}, { WHITE_TERRACOTTA, SMOOTH_SANDSTONE, SMOOTH_STONE, SANDSTONE, LIGHT_GRAY_CONCRETE } },
    { {191, 147, 42},  { SMOOTH_SANDSTONE, SANDSTONE, SMOOTH_STONE } }
    };

/*
inline Block get_fallback_building_block();

inline Block random_choice_from_vector(const std::vector<Block>& v) {
    if (v.empty()) return get_fallback_building_block();
    std::mt19937& rng = global_rng();
    std::uniform_int_distribution<std::size_t> dist(0, v.size() - 1);
    return v[dist(rng)];
}

inline Block random_choice_from_array(const std::vector<Block>::const_iterator begin_it, std::size_t len) {
    if (len == 0) return get_fallback_building_block();
    std::mt19937& rng = global_rng();
    std::uniform_int_distribution<std::size_t> dist(0, len - 1);
    return *(begin_it + dist(rng));
}
*/

Block get_window_block_for_building_type(const std::string& building_type) {
    std::mt19937& rng = global_rng();
    if (building_type == "residential" || building_type == "house" || building_type == "apartment") {
        const std::vector<Block> residential_windows = {
            GLASS,
            WHITE_STAINED_GLASS,
            LIGHT_GRAY_STAINED_GLASS,
            BROWN_STAINED_GLASS
        };
        std::uniform_int_distribution<std::size_t> dist(0, residential_windows.size() - 1);
        return residential_windows[dist(rng)];
    }
    if (building_type == "hospital" || building_type == "school" || building_type == "university") {
        const std::vector<Block> institutional_windows = {
            GLASS,
            WHITE_STAINED_GLASS,
            LIGHT_GRAY_STAINED_GLASS
        };
        std::uniform_int_distribution<std::size_t> dist(0, institutional_windows.size() - 1);
        return institutional_windows[dist(rng)];
    }
    if (building_type == "hotel" || building_type == "restaurant") {
        const std::vector<Block> hospitality_windows = {
            GLASS,
            WHITE_STAINED_GLASS
        };
        std::uniform_int_distribution<std::size_t> dist(0, hospitality_windows.size() - 1);
        return hospitality_windows[dist(rng)];
    }
    if (building_type == "industrial" || building_type == "warehouse") {
        const std::vector<Block> industrial_windows = {
            GLASS,
            GRAY_STAINED_GLASS,
            LIGHT_GRAY_STAINED_GLASS,
            BROWN_STAINED_GLASS
        };
        std::uniform_int_distribution<std::size_t> dist(0, industrial_windows.size() - 1);
        return industrial_windows[dist(rng)];
    }
    {
        std::uniform_int_distribution<std::size_t> dist(0, WINDOW_VARIATIONS.size() - 1);
        return WINDOW_VARIATIONS[dist(rng)];
    }
}


Block get_random_floor_block() {
    //const std::array<Block, 8> floor_options = {
    const std::vector<Block> floor_options = {
        WHITE_CONCRETE,
        GRAY_CONCRETE,
        LIGHT_GRAY_CONCRETE,
        POLISHED_ANDESITE,
        SMOOTH_STONE,
        STONE_BRICKS,
        MUD_BRICKS,
        OAK_PLANKS
    };
    std::mt19937& rng = global_rng();
    std::uniform_int_distribution<std::size_t> dist(0, floor_options.size() - 1);
    return floor_options[dist(rng)];
}

Block get_building_wall_block_for_color(const RGB& color) {
    if (DEFINED_COLORS.empty()) {
        return get_fallback_building_block();
    }
    auto best_it = std::min_element(
        DEFINED_COLORS.begin(),
        DEFINED_COLORS.end(),
        [&color](const std::pair<RGB, std::vector<Block>>& a, const std::pair<RGB, std::vector<Block>>& b) {
            return colors::rgb_distance(color, a.first) < colors::rgb_distance(color, b.first);
        }
    );
    if (best_it != DEFINED_COLORS.end() && !best_it->second.empty()) {
        std::mt19937& rng = global_rng();
        std::uniform_int_distribution<std::size_t> dist(0, best_it->second.size() - 1);
        return best_it->second[dist(rng)];
    }
    return get_fallback_building_block();
}

Block get_fallback_building_block() {
    //const std::array<Block, 27> fallback_options = {
    const std::vector<Block> fallback_options = {
        BLACKSTONE,
        BLACK_TERRACOTTA,
        BRICK,
        BROWN_CONCRETE,
        BROWN_TERRACOTTA,
        DEEPSLATE_BRICKS,
        END_STONE_BRICKS,
        GRAY_CONCRETE,
        GRAY_TERRACOTTA,
        LIGHT_BLUE_TERRACOTTA,
        LIGHT_GRAY_CONCRETE,
        MUD_BRICKS,
        NETHER_BRICK,
        POLISHED_ANDESITE,
        POLISHED_BLACKSTONE,
        POLISHED_BLACKSTONE_BRICKS,
        POLISHED_DEEPSLATE,
        POLISHED_GRANITE,
        QUARTZ_BLOCK,
        QUARTZ_BRICKS,
        SANDSTONE,
        SMOOTH_SANDSTONE,
        SMOOTH_STONE,
        STONE_BRICKS,
        WHITE_CONCRETE,
        WHITE_TERRACOTTA,
        OAK_PLANKS
    };
    std::mt19937& rng = global_rng();
    std::uniform_int_distribution<std::size_t> dist(0, fallback_options.size() - 1);
    return fallback_options[dist(rng)];
}

Block get_castle_wall_block() {
    //const std::array<Block, 10> castle_wall_options = {
    const std::vector<Block> castle_wall_options = {
        STONE_BRICKS,
        CHISELED_STONE_BRICKS,
        CRACKED_STONE_BRICKS,
        
        COBBLESTONE,
        MOSSY_COBBLESTONE,
        DEEPSLATE_BRICKS,
        POLISHED_ANDESITE,
        ANDESITE,
        
        SMOOTH_STONE,
        BRICK
    };
    std::mt19937& rng = global_rng();
    std::uniform_int_distribution<std::size_t> dist(0, castle_wall_options.size() - 1);
    return castle_wall_options[dist(rng)];
}


}