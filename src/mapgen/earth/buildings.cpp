// https://github.com/louis-e/arnis/blob/main/src/element_processing/buildings.rs + chatgpt

// TODO 

#include <climits>
#include <osmium/osm/node.hpp>
#include <osmium/osm/relation.hpp>
#include <osmium/osm/way.hpp>
#include <vector>
#include <string>
#include <unordered_map>
#include <optional>
#include <unordered_set>
#include <cmath>
#include <algorithm>
#include <random>
#include <chrono>
#include <functional>
#include "irr_v2d.h"
#include "irrlichttypes.h"

// -------------------------------------------------------------------
// Example stubs for the types and constants referenced in the Rust code
// -------------------------------------------------------------------

struct Block
{
    // Whatever internal data you use to represent a block
    std::string name;
};

// Example constants to match the Rust block references
// You’ll need to define these properly in your code.
static const Block OAK_FENCE            = {"oak_fence"};
static const Block STONE_BRICK_SLAB     = {"stone_brick_slab"};
static const Block COBBLESTONE_WALL     = {"cobblestone_wall"};
static const Block STONE_BRICKS         = {"stone_bricks"};
static const Block SMOOTH_STONE         = {"smooth_stone"};
static const Block COBBLESTONE          = {"cobblestone"};
static const Block WHITE_STAINED_GLASS  = {"white_stained_glass"};
static const Block LIGHT_GRAY_CONCRETE  = {"light_gray_concrete"};
static const Block GLOWSTONE            = {"glowstone"};
static const Block SNOW_LAYER           = {"snow_layer"};
static const Block AIR                  = {"air"};
static const Block STONE                = {"stone"};
static const Block OAK_PLANKS           = {"oak_planks"};
static const Block STONE_BLOCK_SLAB     = {"stone_block_slab"};
static const Block OAK_FENCE_GATE       = {"oak_fence_gate"}; // Example
// etc...

// The user might define multiple “variations” in arrays or vectors:
inline std::vector<Block> building_corner_variations()
{
    return { /* fill in with your corner-variation blocks */ };
}

inline std::vector<Block> building_wall_variations()
{
    return { /* fill in with your wall-variation blocks */ };
}

inline std::vector<Block> building_floor_variations()
{
    return { /* fill in with your floor-variation blocks */ };
}

// Example color-to-block map references:
using RGBTuple = std::tuple<uint8_t, uint8_t, uint8_t>;
inline std::vector<std::pair<RGBTuple, Block>> building_wall_color_map()
{
    return {
        // { {r, g, b}, Block }
        { {255, 255, 255}, WHITE_STAINED_GLASS },
        // ...
    };
}

inline std::vector<std::pair<RGBTuple, Block>> building_floor_color_map()
{
    return {
        // ...
    };
}

// Helper to compute distance between two RGBs
inline int rgb_distance(const RGBTuple& a, const RGBTuple& b)
{
    auto [ar, ag, ab] = a;
    auto [br, bg, bb] = b;
    // Simple Euclidean or squared difference
    int dr = int(ar) - int(br);
    int dg = int(ag) - int(bg);
    int db = int(ab) - int(bb);
    return dr*dr + dg*dg + db*db;
}

// Example “Args” type
struct Args
{
    double scale;
    bool winter;
    // You can store your timeout as an optional or direct type
    std::optional<std::chrono::milliseconds> timeout;
};

// Example “XZPoint” (cartesian coordinate)
/*
struct XZPoint
{
    int X;
    int Y;

    XZPoint(int x_, int z_) : X(x_), Y(z_) {}
};
*/
using XZPoint = v2pos_t;

/*
// A node from OSM processing
struct ProcessedNode
{
    int x;
    int z;

    // Convenience for BFS or geometry
    XZPoint xz() const { return XZPoint(x, z); }
};
*/
using ProcessedNode = osmium::Node;

// A way from OSM processing (list of nodes, plus string->string tags)
/*
struct ProcessedWay
{
    std::vector<ProcessedNode> nodes;
    std::unordered_map<std::string, std::string> tags;
};
*/
using ProcessedWay = osmium::Way;

// Relationship roles
enum class ProcessedMemberRole
{
    Outer,
    Inner
};

// A relation member referencing a way
struct ProcessedMember
{
    ProcessedWay way;
    ProcessedMemberRole role;
};

// A relation from OSM processing
/*
struct ProcessedRelation
{
    std::vector<ProcessedMember> members;
    std::unordered_map<std::string, std::string> tags;
};
*/
using ProcessedRelation = osmium::Relation;

// A “Ground” class that can return ground level from a set of points
struct Ground
{
    // Return the minimum ground level among points
    // Return std::nullopt if no valid data, to match the Rust’s Option
    std::optional<int> min_level(const std::vector<XZPoint>& points) const
    {
        if (points.empty()) {
            return std::nullopt;
        }
        // Example logic: just pick a fixed level or do some real logic
        int minY = 9999999;
        for (auto& pt : points) {
            // In a real implementation, you'd check your terrain data
            int y = level(pt); 
            if (y < minY) {
                minY = y;
            }
        }
        return minY == 9999999 ? std::nullopt : std::optional<int>(minY);
    }

    // Return ground level for a single XZ point
    int level(const XZPoint& pt) const
    {
        // Example stub (always 64). Real code might do something more sophisticated.
        return 64;
    }
};

// A “WorldEditor” that can set blocks in your map
struct WorldEditor
{
    // Place a block at (x, y, z). The optional adjacency arguments
    // mimic the Rust code’s “Some(&[COBBLESTONE, COBBLESTONE_WALL])” idea.
    void set_block(const Block& block,
                   int x, int y, int z,
                   std::optional<const std::vector<Block>*> maybe_variants,
                   std::optional<const std::vector<Block>*> maybe_replacements)
    {
        // Implementation for adding a block to the world
        (void)block;
        (void)x; (void)y; (void)z;
        (void)maybe_variants;
        (void)maybe_replacements;
    }
};

// -------------------------------------------------------------------
// Helper functions that correspond to those in the Rust code
// -------------------------------------------------------------------

// A Bresenham line function returning points (x, y, z). In the Rust code, 
// y is sometimes repeated or used differently, so adapt to your usage.
inline std::vector<std::tuple<int,int,int>> bresenham_line(int x0, int y0, int z0,
                                                           int x1, int y1, int z1)
{
    // This is a simplified 3D line or 2D Bresenham ignoring Y differences
    // if y0 == y1. If you need full 3D Bresenham, adapt accordingly.
    // For typical building edges, we might handle just XZ in 2D, with a constant Y.
    // For the sample code, we see that y0 == y1. We'll treat it as 2D + constant Y.

    std::vector<std::tuple<int,int,int>> result;
    if (x0 == x1 && z0 == z1) {
        result.emplace_back(x0, y0, z0);
        return result;
    }

    // 2D Bresenham on XZ
    int dx = std::abs(x1 - x0), sx = x0 < x1 ? 1 : -1;
    int dz = std::abs(z1 - z0), sz = z0 < z1 ? 1 : -1;
    int err = dx - dz;

    int cx = x0;
    int cz = z0;
    while (true) {
        result.emplace_back(cx, y0, cz);
        if (cx == x1 && cz == z1) break;

        int e2 = 2 * err;
        if (e2 > -dz) {
            err -= dz;
            cx += sx;
        }
        if (e2 < dx) {
            err += dx;
            cz += sz;
        }
    }
    return result;
}

// Flood fill area for a polygon in XZ. Return vector of (x, z).
// The Rust code calls “flood_fill_area(&polygon_coords, args.timeout.as_ref());”
inline std::vector<std::pair<int,int>> flood_fill_area(
    const std::vector<std::pair<int,int>>& polygonCoords,
    std::optional<std::chrono::milliseconds> maybeTimeout
)
{
    // You would implement your BFS or scanline fill here.
    // For demonstration, just return polygonCoords themselves or an empty vector.
    (void)maybeTimeout; // If you want a time limit, check it here.
    // Real logic would do a polygon fill or BFS.

    // For example, we might just return an empty vector to compile:
    // return {};

    // Or we might just return polygonCoords to mimic some placeholder:
    return polygonCoords;
}

// Convert a building-color text to an RGB. If it fails, return std::nullopt.
inline std::optional<RGBTuple> color_text_to_rgb_tuple(const std::string &colorText)
{
    // You could parse HTML color codes (#RRGGBB), named colors, etc.
    // For demonstration, suppose we only handle "#RRGGBB" or just return white.
    (void)colorText;
    // Example: always return {255, 255, 255}
    return RGBTuple(255, 255, 255);
}

// Finds the nearest block in a given color map
inline std::optional<Block>
find_nearest_block_in_color_map(const RGBTuple& rgb,
                                const std::vector<std::pair<RGBTuple, Block>>& color_map)
{
    int bestDistance = INT_MAX;
    std::optional<Block> bestBlock = std::nullopt;

    for (const auto& [entryRgb, block] : color_map) {
        int dist = rgb_distance(entryRgb, rgb);
        if (dist < bestDistance) {
            bestDistance = dist;
            bestBlock = block;
        }
    }
    return bestBlock;
}

// -------------------------------------------------------------------
// Now the main functions translated from Rust
// -------------------------------------------------------------------

// generate_bridge
void generate_bridge(WorldEditor &editor,
                     const ProcessedWay &element,
                     const Ground &ground,
                     std::optional<std::chrono::milliseconds> floodfill_timeout)
{
    Block floor_block   = STONE;
    Block railing_block = STONE_BRICKS;

    std::optional<XZPoint> previous_node = std::nullopt;

    for (auto &node : element.nodes()) {

        const auto& x = node.x();
        const auto& z = node.y();

        // Base: find ground level
        int bridge_level = ground.level({static_cast<pos_t>(x), static_cast<pos_t>(z)});

        // If "level" tag exists, adjust further
        if (element.tags().has_key("level") ) {
            try {
                int lvl = std::stoi(element.tags().get_value_by_key("level"));
                // The Rust code had (level * 3) + 1
                bridge_level += (lvl * 3) + 1;
            }
            catch (...) {
                // parse error => ignore
            }
        }

        // Use Bresenham to connect from the previous node
        if (previous_node.has_value()) {
            const auto &[px, pz] = previous_node.value();

            int prev_bridge_level = ground.level({px, pz});
            if (element.tags().has_key("level") ) {
                try {
                    int lvl = std::stoi(element.tags().get_value_by_key("level"));
                    prev_bridge_level += (lvl * 3) + 1;
                }
                catch (...) {}
            }

            auto bridge_points = bresenham_line(px, prev_bridge_level, pz, x, bridge_level, z);
            for (auto &pt : bridge_points) {
                auto [bx, by, bz] = pt;
                // Place railing
                editor.set_block(railing_block, bx, by + 1, bz, std::nullopt, std::nullopt);
                editor.set_block(railing_block, bx, by,     bz, std::nullopt, std::nullopt);
            }
        }

        previous_node = {static_cast<pos_t>(x), static_cast<pos_t>(z)};
    }

    // Flood fill the area between the bridge path nodes
    std::vector<std::pair<int,int>> polygon_coords;
    polygon_coords.reserve(element.nodes().size());
    for (auto &n : element.nodes()) {
        polygon_coords.emplace_back(n.x(), n.y());
    }

    auto bridge_area = flood_fill_area(polygon_coords, floodfill_timeout);

    for (auto &pt : bridge_area) {
        int x = pt.first;
        int z = pt.second;

        int bridge_level = ground.level({static_cast<pos_t>(x), static_cast<pos_t>(z)});
        if (element.tags().has_key("level") ) {
            try {
                int lvl = std::stoi(element.tags().get_value_by_key("level"));
                bridge_level += (lvl * 3) + 1;
            }
            catch (...) {}
        }

        editor.set_block(floor_block, x, bridge_level, z, std::nullopt, std::nullopt);
    }
}

// generate_buildings
void generate_buildings(WorldEditor &editor,
                        const ProcessedWay &element,
                        const Ground &ground,
                        const Args &args,
                        std::optional<int> relation_levels)
{
    // Get base Y from ground
    std::vector<XZPoint> nodePoints;
    nodePoints.reserve(element.nodes().size());
    for (const  auto &n : element.nodes()) {
        nodePoints.emplace_back(static_cast<pos_t>(n.x()), static_cast<pos_t>(n.y()));
    }
    auto maybeBaseY = ground.min_level(nodePoints);
    if (!maybeBaseY.has_value()) {
        return;
    }
    int base_y = maybeBaseY.value();

    // building:min_level
    int min_level = 0;
    if (element.tags().has_key("building:min_level") ) {
        try {
            min_level = std::stoi(element.tags().get_value_by_key("building:min_level"));
        }
        catch (...) {
            min_level = 0;
        }
    }

    // start_level
    int start_level = base_y + (min_level * 4);

    std::optional<std::pair<int,int>> previous_node = std::nullopt;
    std::tuple<int,int,int> corner_addup = {0, 0, 0};
    std::vector<std::pair<int,int>> current_building;

    // Randomly select block variations
    static std::mt19937 rng(std::random_device{}());
    {
        // corner
        const auto &corner_variations = building_corner_variations();
        std::uniform_int_distribution<size_t> distCorner(0, corner_variations.size()-1);
        size_t variation_index_corner = distCorner(rng);

        // wall
        const auto &wall_variations = building_wall_variations();
        std::uniform_int_distribution<size_t> distWall(0, wall_variations.size()-1);
        size_t variation_index_wall = distWall(rng);

        // floor
        const auto &floor_variations = building_floor_variations();
        std::uniform_int_distribution<size_t> distFloor(0, floor_variations.size()-1);
        size_t variation_index_floor = distFloor(rng);

        // Decide blocks
        Block corner_block = corner_variations[variation_index_corner];

        // building:colour
        Block wall_block;
        if (element.tags().has_key("building:colour") > 0) {
            auto building_colour = element.tags().get_value_by_key("building:colour");
            auto maybeRgb = color_text_to_rgb_tuple(building_colour);
            if (maybeRgb.has_value()) {
                auto rgb = maybeRgb.value();
                auto nearest = find_nearest_block_in_color_map(rgb, building_wall_color_map());
                if (nearest.has_value()) {
                    wall_block = nearest.value();
                }
                else {
                    // fallback
                    wall_block = wall_variations[variation_index_wall];
                }
            } else {
                wall_block = wall_variations[variation_index_wall];
            }
        } else {
            wall_block = wall_variations[variation_index_wall];
        }

        // roof:colour
        Block floor_block;
        if (element.tags().has_key("roof:colour") > 0) {
            auto roof_colour = element.tags().get_value_by_key("roof:colour");
            auto maybeRgb = color_text_to_rgb_tuple(roof_colour);
            if (maybeRgb.has_value()) {
                auto rgb = maybeRgb.value();
                auto nearest = find_nearest_block_in_color_map(rgb, building_floor_color_map());
                if (nearest.has_value()) {
                    floor_block = nearest.value();
                }
                else {
                    floor_block = LIGHT_GRAY_CONCRETE; 
                }
            } else {
                floor_block = LIGHT_GRAY_CONCRETE;
            }
        }
        else {
            // check building type
            Block fallback_floor = LIGHT_GRAY_CONCRETE;
            if (element.tags().has_key("building") > 0) {
                auto building_type = element.tags().get_value_by_key("building");
                if (building_type == "yes" || building_type == "house" ||
                    building_type == "detached" || building_type == "static_caravan" ||
                    building_type == "semidetached_house" || building_type == "bungalow" ||
                    building_type == "manor" || building_type == "villa") 
                {
                    floor_block = building_floor_variations()[variation_index_floor];
                }
                else {
                    floor_block = fallback_floor;
                }
            }
            else if (element.tags().has_key("building:part") > 0)
            {
                // same logic
                auto bpart = element.tags().get_value_by_key("building:part");
                // if it’s a typical house or something else, you can do your logic
                floor_block = building_floor_variations()[variation_index_floor];
            }
            else {
                floor_block = fallback_floor;
            }
        }

        Block window_block = WHITE_STAINED_GLASS;

        // Keep track of processed points in flood fill
        std::unordered_set<long long> processed_points; 
        // a simple pair-hash to store (x,z).
        auto pairToKey = [](int x, int z) {
            // combine 32 bits of x, z into 64
            // or use a real hash. Example:
            return ( (long long)x << 32 ) ^ (long long)(z & 0xffffffff);
        };

        double scale_factor = args.scale;

        // Default building height
        int building_height = std::max(static_cast<int>(6.0 * scale_factor), 3);

        // Skip if 'layer' or 'level' is negative
        if (element.tags().has_key("layer") > 0) {
            try {
                int layer_val = std::stoi(element.tags().get_value_by_key("layer"));
                if (layer_val < 0) {
                    return;
                }
            } catch(...) {}
        }
        if (element.tags().has_key("level") > 0) {
            try {
                int lvl_val = std::stoi(element.tags().get_value_by_key("level"));
                if (lvl_val < 0) {
                    return;
                }
            } catch(...) {}
        }

        // building:levels
        if (element.tags().has_key("building:levels") > 0) {
            try {
                int levels = std::stoi(element.tags().get_value_by_key("building:levels"));
                int lev = levels - min_level;
                if (lev >= 1) {
                    building_height = static_cast<int>((lev * 4 + 2) * scale_factor);
                    building_height = std::max(building_height, 3);
                }
            }
            catch(...) {}
        }

        // height
        if (element.tags().has_key("height") > 0) {
            std::string height_str = element.tags().get_value_by_key("height");
            // Possibly strip trailing "m"
            if (!height_str.empty()) {
                // e.g. remove trailing 'm'
                if (height_str.back() == 'm') {
                    height_str.pop_back();
                }
            }
            try {
                double h = std::stod(height_str);
                int scaledH = static_cast<int>(h * scale_factor);
                building_height = std::max(scaledH, 3);
            }
            catch(...) {}
        }

        // relation_levels override if available
        if (relation_levels.has_value()) {
            int rl = relation_levels.value();
            building_height = static_cast<int>((rl * 4 + 2) * scale_factor);
            building_height = std::max(building_height, 3);
        }

        // Check amenity = "shelter"
        if (element.tags().has_key("amenity") > 0) {
            auto amenity_type = element.tags().get_value_by_key("amenity");
            if (amenity_type == "shelter") {
                // handle special shelter logic:
                Block roof_block = STONE_BRICK_SLAB;

                // polygon coords
                std::vector<std::pair<int,int>> polygon_coords;
                polygon_coords.reserve(element.nodes().size());
                for (auto &n : element.nodes()) {
                    polygon_coords.emplace_back(n.x(), n.y());
                }
                auto roof_area = flood_fill_area(polygon_coords, args.timeout);

                // place fences and roof slabs
                for (auto &node : element.nodes()) {
                    int nx = node.x();
                    int nz = node.y();

                    auto maybeY = ground.min_level(nodePoints);
                    if(!maybeY.has_value()) {
                        return;
                    }
                    int y = maybeY.value();

                    for (int shelter_y = 1; shelter_y <= 4; ++shelter_y) {
                        editor.set_block(OAK_FENCE, nx, y + shelter_y, nz, std::nullopt, std::nullopt);
                    }
                    editor.set_block(roof_block, nx, y + 5, nz, std::nullopt, std::nullopt);
                }

                auto maybeY = ground.min_level(nodePoints);
                if(!maybeY.has_value()) {
                    return;
                }
                int ground_y = maybeY.value();
                int roof_height = ground_y + 5;
                for (auto &rc : roof_area) {
                    editor.set_block(roof_block, rc.first, roof_height, rc.second, std::nullopt, std::nullopt);
                }
                return;
            }
        }

        // Check building tag
        if (element.tags().has_key("building") > 0) {
            auto building_type = element.tags().get_value_by_key("building");

            if (building_type == "garage") {
                building_height = std::max(static_cast<int>(2.0 * scale_factor), 3);
            }
            else if (building_type == "shed") {
                building_height = std::max(static_cast<int>(2.0 * scale_factor), 3);

                // If bicycle_parking
                if (element.tags().has_key("bicycle_parking") > 0) {
                    // special shed logic
                    Block ground_block = OAK_PLANKS;
                    Block roof_block   = STONE_BLOCK_SLAB;

                    std::vector<std::pair<int,int>> polygon_coords;
                    polygon_coords.reserve(element.nodes().size());
                    for (auto &n : element.nodes()) {
                        polygon_coords.emplace_back(n.x(), n.y());
                    }
                    auto floor_area = flood_fill_area(polygon_coords, args.timeout);

                    auto maybeY = ground.min_level(nodePoints);
                    if(!maybeY.has_value()) {
                        return;
                    }
                    int y = maybeY.value();

                    // fill floor
                    for (auto &fa : floor_area) {
                        editor.set_block(ground_block, fa.first, y, fa.second, std::nullopt, std::nullopt);
                    }

                    // place fences, roof
                    for (auto &node : element.nodes()) {
                        int nx = node.x();
                        int nz = node.y();
                        for (int dy=1; dy<=4; ++dy) {
                            editor.set_block(OAK_FENCE, nx, y + dy, nz, std::nullopt, std::nullopt);
                        }
                        editor.set_block(roof_block, nx, y+5, nz, std::nullopt, std::nullopt);
                    }

                    // flood fill roof
                    int roof_height = y + 5;
                    for (auto &fa : floor_area) {
                        editor.set_block(roof_block, fa.first, roof_height, fa.second, std::nullopt, std::nullopt);
                    }
                    return;
                }
            }
            else if (building_type == "parking" ||
                     (element.tags().has_key("parking") > 0 && element.tags().get_value_by_key("parking") == "multi-storey"))
            {
                // multi-storey parking
                building_height = std::max(building_height, 16);

                std::vector<std::pair<int,int>> polygon_coords;
                polygon_coords.reserve(element.nodes().size());
                for (auto &n : element.nodes()) {
                    polygon_coords.emplace_back(n.x(), n.y());
                }
                auto floor_area = flood_fill_area(polygon_coords, args.timeout);

                auto maybeGroundLevel = ground.min_level(nodePoints);
                if(!maybeGroundLevel.has_value()) {
                    return;
                }
                int ground_level = maybeGroundLevel.value();

                // build levels
                for (int level = 0; level <= (building_height / 4); ++level) {
                    int current_level = ground_level + level * 4;

                    // outer walls
                    for (auto &node : element.nodes()) {
                        int nx = node.x();
                        int nz = node.y();
                        // build wall from current_level+1..current_level+4
                        for (int y = current_level+1; y <= current_level+4; ++y) {
                            editor.set_block(STONE_BRICKS, nx, y, nz, std::nullopt, std::nullopt);
                        }
                    }
                    // fill floor
                    for (auto &fa : floor_area) {
                        int fx = fa.first;
                        int fz = fa.second;
                        if (level == 0) {
                            editor.set_block(SMOOTH_STONE, fx, current_level, fz, std::nullopt, std::nullopt);
                        } else {
                            editor.set_block(COBBLESTONE,   fx, current_level, fz, std::nullopt, std::nullopt);
                        }
                    }
                }

                // Outline for each level
                for (int level = 0; level <= (building_height / 4); ++level) {
                    int current_level = ground_level + level * 4;

                    // Use the nodes to create outline
                    std::optional<std::pair<int,int>> prev_outline = std::nullopt;
                    for (auto &node : element.nodes()) {
                        int nx = node.x();
                        int nz = node.y();
                        if (prev_outline.has_value()) {
                            auto [px, pz] = prev_outline.value();
                            auto outline_pts = bresenham_line(px, current_level, pz,
                                                              nx, current_level, nz);
                            for (auto &op : outline_pts) {
                                auto [bx, by, bz] = op;
                                editor.set_block(SMOOTH_STONE, bx, by, bz, 
                                                 std::nullopt,
                                                 std::nullopt);
                                editor.set_block(STONE_BRICK_SLAB, bx, by+2, bz,
                                                 std::nullopt,
                                                 std::nullopt);
                                if ((bx % 2) == 0) {
                                    editor.set_block(COBBLESTONE_WALL, bx, by+1, bz,
                                                     std::nullopt,
                                                     std::nullopt);
                                }
                            }
                        }
                        prev_outline = std::make_pair(nx, nz);
                    }
                }
                return;
            }
            else if (building_type == "roof") {
                auto maybeGroundLevel = ground.min_level(nodePoints);
                if(!maybeGroundLevel.has_value()) {
                    return;
                }
                int ground_level = maybeGroundLevel.value();
                int roof_height = ground_level + 5;

                // edges w/ Bresenham
                for (auto &node : element.nodes()) {
                    int nx = node.x();
                    int nz = node.y();

                    if (previous_node.has_value()) {
                        auto [px, pz] = previous_node.value();
                        auto linePoints = bresenham_line(px, roof_height, pz,
                                                         nx, roof_height, nz);
                        for (auto &lp : linePoints) {
                            auto [bx, by, bz] = lp;
                            editor.set_block(STONE_BRICK_SLAB, bx, by, bz,
                                             std::nullopt, std::nullopt);
                        }
                    }
                    // vertical walls up to roof
                    for (int y = ground_level+1; y <= (roof_height-1); ++y) {
                        editor.set_block(COBBLESTONE_WALL, nx, y, nz, 
                                         std::nullopt, std::nullopt);
                    }

                    previous_node = std::make_pair(nx, nz);
                }

                // flood fill the interior
                std::vector<std::pair<int,int>> polygon_coords;
                polygon_coords.reserve(element.nodes().size());
                for (auto &n : element.nodes()) {
                    polygon_coords.emplace_back(n.x(), n.y());
                }
                auto roof_area = flood_fill_area(polygon_coords, args.timeout);

                // fill interior
                for (auto &ra : roof_area) {
                    int rx = ra.first;
                    int rz = ra.second;
                    editor.set_block(STONE_BRICK_SLAB, rx, roof_height, rz, std::nullopt, std::nullopt);
                }
                return;
            }
            else if (building_type == "apartments") {
                // If no user-specified height:
                int defaultH = std::max(static_cast<int>(6.0 * scale_factor), 3);
                if (building_height == defaultH) {
                    building_height = std::max(static_cast<int>(15.0 * scale_factor), 3);
                }
            }
            else if (building_type == "hospital") {
                int defaultH = std::max(static_cast<int>(6.0 * scale_factor), 3);
                if (building_height == defaultH) {
                    building_height = std::max(static_cast<int>(23.0 * scale_factor), 3);
                }
            }
            else if (building_type == "bridge") {
                // call generate_bridge
                generate_bridge(editor, element, ground, args.timeout);
                return;
            }
        }

        // Process the outer walls
        for (auto &node : element.nodes()) {
            int x = node.x();
            int z = node.y();

            if (previous_node.has_value()) {
                auto [px, pz] = previous_node.value();
                // bresenham from (px, start_level, pz) to (x, start_level, z)
                auto linePoints = bresenham_line(px, start_level, pz,
                                                 x, start_level, z);
                for (auto &lp : linePoints) {
                    auto [bx, by, bz] = lp;

                    // build vertical column
                    for (int h = start_level+1; h <= start_level+building_height; ++h) {
                        // corner detection in Rust code is not specifically robust here,
                        // adapt if you prefer. For demonstration, we check if it’s “the first node”
                        // or you can do a real corner check. The original code has a questionable check:
                        //   if element.nodes[0].x == bx && element.nodes[0].x == bz ...
                        // Possibly it’s a leftover snippet. We'll just treat everything as walls:
                        if (h > start_level+1 && (h % 4 != 0) && ((bx + bz) % 6 < 3)) {
                            editor.set_block(WHITE_STAINED_GLASS, bx, h, bz, std::nullopt, std::nullopt);
                        } else {
                            editor.set_block(wall_block, bx, h, bz, std::nullopt, std::nullopt);
                        }
                    }

                    // top
                    editor.set_block(COBBLESTONE, bx, start_level + building_height + 1, bz,
                                     std::nullopt, std::nullopt);

                    if (args.winter) {
                        editor.set_block(SNOW_LAYER, bx, start_level + building_height + 2, bz,
                                         std::nullopt, std::nullopt);
                    }

                    current_building.emplace_back(bx, bz);
                    auto & [cxSum, czSum, cCount] = corner_addup;
                    cxSum += bx;
                    czSum += bz;
                    cCount += 1;
                }
            }
            previous_node = std::make_pair(x, z);
        }

        // Flood-fill interior with floor
        auto [cxSum, czSum, cCount] = corner_addup;
        if (cCount != 0) {
            // gather polygon coords
            std::vector<std::pair<int,int>> polygon_coords;
            polygon_coords.reserve(element.nodes().size());
            for (auto &n : element.nodes()) {
                polygon_coords.emplace_back(n.x(), n.y());
            }
            auto floor_area = flood_fill_area(polygon_coords, args.timeout);

            for (auto &fa : floor_area) {
                int fx = fa.first;
                int fz = fa.second;
                long long key = pairToKey(fx, fz);
                // only place if not processed
                if (processed_points.find(key) == processed_points.end()) {
                    processed_points.insert(key);

                    // floor
                    editor.set_block(floor_block, fx, start_level, fz, std::nullopt, std::nullopt);

                    // intermediate ceilings if height > 4
                    if (building_height > 4) {
                        for (int h = start_level + 2 + 4; h < (start_level + building_height); h += 4) {
                            // place glowstone
                            if ((fx % 6 == 0) && (fz % 6 == 0)) {
                                editor.set_block(GLOWSTONE, fx, h, fz, std::nullopt, std::nullopt);
                            }
                            else {
                                editor.set_block(floor_block, fx, h, fz, std::nullopt, std::nullopt);
                            }
                        }
                    }
                    else {
                        if ((fx % 6 == 0) && (fz % 6 == 0)) {
                            editor.set_block(GLOWSTONE, fx, start_level + building_height, fz,
                                             std::nullopt, std::nullopt);
                        }
                    }

                    // top ceiling
                    editor.set_block(floor_block, fx, start_level + building_height + 1, fz,
                                     std::nullopt, std::nullopt);

                    if (args.winter) {
                        editor.set_block(SNOW_LAYER, fx, start_level + building_height + 2, fz,
                                         std::nullopt, std::nullopt);
                    }
                }
            }
        }
    }
}

// generate_building_from_relation
void generate_building_from_relation(WorldEditor &editor,
                                     const ProcessedRelation &relation,
                                     const Ground &ground,
                                     const Args &args)
{
    // building:levels from relation
    int relation_levels = 2; // default
    if (relation.tags().has_key("building:levels") > 0) {
        try {
            relation_levels = std::stoi(relation.tags().get_value_by_key("building:levels"));
        }
        catch(...) {
            relation_levels = 2;
        }
    }


    // For each outer member, build
    //for (auto &member : relation.members()) {
    for (auto &member :relation.subitems<osmium::Way>()){
        //if (member.role() == ProcessedMemberRole::Outer) {
        //if (member.role() == std::string{"outer"}) 
         //if (member.type() == osmium::item_type::inner_ring)
        { 
            generate_buildings(editor, member, ground, args, relation_levels);
        }
    }

    // If you want to handle “inner” ways to carve holes, adapt:
    /*
    for (auto &member : relation.members) {
        if (member.role == ProcessedMemberRole::Inner) {
            // Suppose you remove blocks in the hole polygon
            // The Rust code snippet is commented out, but here’s a translation:
            std::vector<std::pair<int,int>> polygon_coords;
            polygon_coords.reserve(member.way.nodes.size());
            for (auto &n : member.way.nodes) {
                polygon_coords.emplace_back(n.x(), n.y());
            }
            auto hole_area = flood_fill_area(polygon_coords, args.timeout);

            // For example, remove blocks at ground level
            // (Or do something else to create the hole.)
            auto maybeGround = ground.min_level(
                std::vector<XZPoint>(member.way.nodes.begin(),
                                     member.way.nodes.end())
            );
            if(!maybeGround.has_value()) {
                continue;
            }
            int holeY = maybeGround.value();

            for (auto &hpt : hole_area) {
                editor.set_block(AIR, hpt.first, holeY, hpt.second,
                                 std::nullopt, std::nullopt);
            }
        }
    }
    */
}
