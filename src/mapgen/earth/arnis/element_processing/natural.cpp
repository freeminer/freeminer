#include <vector>
#include <string>
#include <unordered_map>
#include <optional>
#include <random>
#include <tuple>
#include <cmath>
#include <algorithm>


#include "tree.h"
#include "../../arnis_adapter.h"
#undef stoi
namespace arnis
{

    namespace natural
{


#if 0 
namespace osm {

// Block definitions
enum class Block {
    GRASS_BLOCK,
    SAND,
    GRAVEL,
    WATER,
    STONE,
    COBBLESTONE,
    PACKED_ICE,
    MUD,
    DIRT,
    GRANITE,
    DIORITE,
    ANDESITE,
    MOSS_BLOCK,
    GRASS,
    OAK_LEAVES,
    RED_FLOWER,
    BLUE_FLOWER,
    YELLOW_FLOWER,
    WHITE_FLOWER,
    TALL_GRASS_BOTTOM,
    TALL_GRASS_TOP,
    DEAD_BUSH
};

// Forward declarations
struct WorldEditor;
struct Tree;

// Simple Processed structures
struct ProcessedNode {
    int x;
    int z;
    std::unordered_map<std::string, std::string> tags;
};

struct ProcessedWay {
    long long id;
    std::vector<ProcessedNode> nodes;
    std::unordered_map<std::string, std::string> tags;
};

enum class ProcessedMemberRole { Outer, Inner };

struct ProcessedMember {
    ProcessedMemberRole role;
    ProcessedWay way;
};

struct ProcessedRelation {
    long long id;
    std::vector<ProcessedMember> members;
    std::unordered_map<std::string, std::string> tags;
};

struct ProcessedElement {
    enum class Type { Node, Way } type;
    ProcessedNode node;
    ProcessedWay way;

    const std::unordered_map<std::string, std::string>& tags() const {
        if (type == Type::Node) {
            return node.tags;
        } else {
            return way.tags;
        }
    }

    static ProcessedElement FromNode(const ProcessedNode& n) {
        ProcessedElement e;
        e.type = Type::Node;
        e.node = n;
        return e;
    }

    static ProcessedElement FromWay(const ProcessedWay& w) {
        ProcessedElement e;
        e.type = Type::Way;
        e.way = w;
        return e;
    }
};

struct Args {
    std::optional<int> timeout;
};

// WorldEditor simplified interface
struct WorldEditor {
    // set_block(block, x, y, z, allowed_blocks_optional, unused_optional)
    void set_block(Block block, int x, int y, int z,
                   const std::optional<std::vector<Block>>& allowed = std::nullopt,
                   const std::optional<int>& = std::nullopt) {
        // Implementation would place block in the world.
        // Placeholder: no-op for this conversion.
        (void)block; (void)x; (void)y; (void)z; (void)allowed;
    }

    // check_for_block(x,y,z, optional list of blocks to check against)
    bool check_for_block(int x, int y, int z, const std::optional<std::vector<Block>>& blocks = std::nullopt) const {
        // Real implementation inspects world contents.
        // Placeholder returns false (no match) for simplicity.
        (void)x; (void)y; (void)z; (void)blocks;
        return false;
    }
};
// Tree creation utility
struct Tree {
    static void create(WorldEditor& editor, const std::tuple<int,int,int>& pos) {
        int x = std::get<0>(pos);
        int y = std::get<1>(pos);
        int z = std::get<2>(pos);
        // Minimalistic tree: trunk and a small canopy
        editor.set_block(WOOD /* not defined as constant in original, use OAK_LEAVES as placeholder if needed */, x, y, z, std::nullopt, std::nullopt);
        (void)y;
        (void)z;
        (void)x;
        // Note: original code references Tree::create externally; here we leave a placeholder call.
    }
};

// Bresenham 2D line returning (x,y,z) tuples; y preserved/ignored in algorithm (use input ys)
static std::vector<std::tuple<int,int,int>> bresenham_line(int x1, int y1, int z1, int x2, int y2, int z2) {
    std::vector<std::tuple<int,int,int>> points;
    int x = x1;
    int z = z1;
    int dx = std::abs(x2 - x1);
    int dz = std::abs(z2 - z1);
    int sx = (x1 < x2) ? 1 : -1;
    int sz = (z1 < z2) ? 1 : -1;
    int err = (dx > dz ? dx : -dz) / 2;
    while (true) {
        points.emplace_back(x, y1, z);
        if (x == x2 && z == z2) break;
        int e2 = err;
        if (e2 > -dx) { err -= dz; x += sx; }
        if (e2 < dz)  { err += dx; z += sz; }
    }
    return points;
}

// Point-in-polygon test (ray casting)
static bool point_in_polygon(int px, int pz, const std::vector<std::pair<int,int>>& poly) {
    bool inside = false;
    std::size_t n = poly.size();
    for (std::size_t i = 0, j = n - 1; i < n; j = i++) {
        int xi = poly[i].first, zi = poly[i].second;
        int xj = poly[j].first, zj = poly[j].second;
        bool intersect = ((zi > pz) != (zj > pz)) &&
                         (px < (xj - xi) * (pz - zi) / double(zj - zi) + xi);
        if (intersect) inside = !inside;
    }
    return inside;
}

// Flood fill area by filling integer coords inside polygon bounding box using point-in-polygon.
// Respect optional timeout (ignored in this simple implementation but kept for signature compatibility).
static std::vector<std::pair<int,int>> flood_fill_area(const std::vector<std::pair<int,int>>& polygon_coords,
                                                      const std::optional<int>& timeout) {
    (void)timeout;
    std::vector<std::pair<int,int>> result;
    if (polygon_coords.empty()) return result;
    int minx = polygon_coords[0].first, maxx = polygon_coords[0].first;
    int minz = polygon_coords[0].second, maxz = polygon_coords[0].second;
    for (const auto& p : polygon_coords) {
        minx = std::min(minx, p.first);
        maxx = std::max(maxx, p.first);
        minz = std::min(minz, p.second);
        maxz = std::max(maxz, p.second);
    }
    for (int x = minx; x <= maxx; ++x) {
        for (int z = minz; z <= maxz; ++z) {
            if (point_in_polygon(x, z, polygon_coords)) {
                result.emplace_back(x, z);
            }
        }
    }
    return result;
}

#endif

// Generate natural area for single element
//static 
void generate_natural(WorldEditor& editor, const ProcessedElement& element, const Args& args) {
    const auto& tags = element.tags();
    auto it_nat = tags.find("natural");
    if (it_nat == tags.end()) return;
    const std::string natural_type = it_nat->second;

    if (natural_type == "tree") {
        if (element.is_node()) {
            int x = element.as_node().x;
            int z = element.as_node().z;
            Tree::create(editor, Coord{x, 1, z});
        }
        return;
    }

    // Determine block type based on natural tag
    Block block_type = GRASS_BLOCK;
    if (natural_type == "scrub" || natural_type == "grassland" || natural_type == "wood" || natural_type == "heath" || natural_type == "tree_row") {
        block_type = GRASS_BLOCK;
    } else if (natural_type == "sand" || natural_type == "dune") {
        block_type = SAND;
    } else if (natural_type == "beach" || natural_type == "shoal") {
        auto it_surface = tags.find("natural");
        std::string surface = (it_surface == tags.end()) ? std::string() : it_surface->second;
        if (surface == "gravel") block_type = GRAVEL;
        else block_type = SAND;
    } else if (natural_type == "water" || natural_type == "reef") {
        block_type = WATER;
    } else if (natural_type == "bare_rock") {
        block_type = STONE;
    } else if (natural_type == "blockfield") {
        block_type = COBBLESTONE;
    } else if (natural_type == "glacier") {
        block_type = PACKED_ICE;
    } else if (natural_type == "mud" || natural_type == "wetland") {
        block_type = MUD;
    } else if (natural_type == "mountain_range") {
        block_type = COBBLESTONE;
    } else if (natural_type == "saddle" || natural_type == "ridge") {
        block_type = STONE;
    } else if (natural_type == "shrubbery" || natural_type == "tundra" || natural_type == "hill") {
        block_type = GRASS_BLOCK;
    } else if (natural_type == "cliff") {
        block_type = STONE;
    } else {
        block_type = GRASS_BLOCK;
    }

    if (element.type != ProcessedElement::Type::Way) {
        return;
    }
    const ProcessedWay& way = element.as_way();

    std::optional<std::pair<int,int>> previous_node;
    std::tuple<int,int,int> corner_addup = std::make_tuple(0,0,0);
    std::vector<std::pair<int,int>> current_natural;

    for (const auto& node : way.nodes) {
        int x = node.x;
        int z = node.z;
        if (previous_node.has_value()) {
            auto prev = previous_node.value();
            std::vector<std::tuple<int,int,int>> bres = bresenham_line(prev.first, 0, prev.second, x, 0, z);
            for (const auto& t : bres) {
                int bx = std::get<0>(t);
                int bz = std::get<2>(t);
                editor.set_block(block_type, bx, 0, bz, std::nullopt, std::nullopt);
            }
            current_natural.emplace_back(x, z);
            std::get<0>(corner_addup) += x;
            std::get<1>(corner_addup) += z;
            std::get<2>(corner_addup) += 1;
        }
        previous_node = std::make_pair(x, z);
    }

    if (std::get<2>(corner_addup) != 0) {
        std::vector<std::pair<int,int>> polygon_coords;
        polygon_coords.reserve(way.nodes.size());
        for (const auto& n : way.nodes) {
            polygon_coords.emplace_back(n.x, n.z);
        }

        std::vector<std::pair<int,int>> filled_area = flood_fill_area(polygon_coords, args.timeout);

        std::random_device rd;
        std::mt19937 rng(rd());

        for (const auto& p : filled_area) {
            int x = p.first;
            int z = p.second;
            editor.set_block(block_type, x, 0, z, std::nullopt, std::nullopt);

            if (natural_type == "beach" || natural_type == "sand" || natural_type == "dune" || natural_type == "shoal") {
                editor.set_block(SAND, x, 0, z, std::nullopt, std::nullopt);
            } else if (natural_type == "glacier") {
                editor.set_block(PACKED_ICE, x, 0, z, std::nullopt, std::nullopt);
                editor.set_block(STONE, x, -1, z, std::nullopt, std::nullopt);
            } else if (natural_type == "bare_rock") {
                editor.set_block(STONE, x, 0, z, std::nullopt, std::nullopt);
            }

            if (editor.check_for_block(x, 0, z, std::optional<std::vector<Block>>{ std::vector<Block>{WATER} })) {
                continue;
            }

            if (natural_type == "grassland") {
                if (!editor.check_for_block(x, 0, z, std::optional<std::vector<Block>>{ std::vector<Block>{GRASS_BLOCK} })) {
                    continue;
                }
                std::bernoulli_distribution d(0.6);
                if (d(rng)) {
                    editor.set_block(GRASS, x, 1, z, std::nullopt, std::nullopt);
                }
            } else if (natural_type == "heath") {
                if (!editor.check_for_block(x, 0, z, std::optional<std::vector<Block>>{ std::vector<Block>{GRASS_BLOCK} })) {
                    continue;
                }
                std::uniform_int_distribution<int> uni500(0, 499);
                int random_choice = uni500(rng);
                if (random_choice < 33) {
                    if (random_choice <= 2) {
                        editor.set_block(COBBLESTONE, x, 0, z, std::nullopt, std::nullopt);
                    } else if (random_choice < 6) {
                        editor.set_block(OAK_LEAVES, x, 1, z, std::nullopt, std::nullopt);
                    } else {
                        editor.set_block(GRASS, x, 1, z, std::nullopt, std::nullopt);
                    }
                }
            } else if (natural_type == "scrub") {
                if (!editor.check_for_block(x, 0, z, std::optional<std::vector<Block>>{ std::vector<Block>{GRASS_BLOCK} })) {
                    continue;
                }
                std::uniform_int_distribution<int> uni500(0, 499);
                int random_choice = uni500(rng);
                if (random_choice == 0) {
                    Tree::create(editor, std::make_tuple(x, 1, z));
                } else if (random_choice == 1) {
                    std::uniform_int_distribution<int> flower_dist(1, 4);
                    int f = flower_dist(rng);
                    Block flower_block = RED_FLOWER;
                    if (f == 1) flower_block = RED_FLOWER;
                    else if (f == 2) flower_block = BLUE_FLOWER;
                    else if (f == 3) flower_block = YELLOW_FLOWER;
                    else flower_block = WHITE_FLOWER;
                    editor.set_block(flower_block, x, 1, z, std::nullopt, std::nullopt);
                } else if (random_choice < 40) {
                    editor.set_block(OAK_LEAVES, x, 1, z, std::nullopt, std::nullopt);
                    if (random_choice < 15) {
                        editor.set_block(OAK_LEAVES, x, 2, z, std::nullopt, std::nullopt);
                    }
                } else if (random_choice < 300) {
                    if (random_choice < 250) {
                        editor.set_block(GRASS, x, 1, z, std::nullopt, std::nullopt);
                    } else {
                        editor.set_block(TALL_GRASS_BOTTOM, x, 1, z, std::nullopt, std::nullopt);
                        editor.set_block(TALL_GRASS_TOP, x, 2, z, std::nullopt, std::nullopt);
                    }
                }
            } else if (natural_type == "tree_row" || natural_type == "wood") {
                if (!editor.check_for_block(x, 0, z, std::optional<std::vector<Block>>{ std::vector<Block>{GRASS_BLOCK} })) {
                    continue;
                }
                std::uniform_int_distribution<int> uni30(0, 29);
                int random_choice = uni30(rng);
                if (random_choice == 0) {
                    Tree::create(editor, std::make_tuple(x, 1, z));
                } else if (random_choice == 1) {
                    std::uniform_int_distribution<int> flower_dist(1, 4);
                    int f = flower_dist(rng);
                    Block flower_block = RED_FLOWER;
                    if (f == 1) flower_block = RED_FLOWER;
                    else if (f == 2) flower_block = BLUE_FLOWER;
                    else if (f == 3) flower_block = YELLOW_FLOWER;
                    else flower_block = WHITE_FLOWER;
                    editor.set_block(flower_block, x, 1, z, std::nullopt, std::nullopt);
                } else if (random_choice <= 12) {
                    editor.set_block(GRASS, x, 1, z, std::nullopt, std::nullopt);
                }
            } else if (natural_type == "sand") {
                std::uniform_int_distribution<int> uni100(0, 99);
                if (editor.check_for_block(x, 0, z, std::optional<std::vector<Block>>{ std::vector<Block>{SAND} })
                    && uni100(rng) == 1) {
                    editor.set_block(DEAD_BUSH, x, 1, z, std::nullopt, std::nullopt);
                }
            } else if (natural_type == "shoal") {
                std::bernoulli_distribution d(0.05);
                if (d(rng)) {
                    editor.set_block(WATER, x, 0, z, std::optional<std::vector<Block>>{ std::vector<Block>{SAND, GRAVEL} }, std::nullopt);
                }
            } else if (natural_type == "wetland") {
                auto it_wet = tags.find("wetland");
                if (it_wet != tags.end()) {
                    const std::string& wetland_type = it_wet->second;
                    if (wetland_type == "wet_meadow" || wetland_type == "fen") {
                        std::bernoulli_distribution d(0.3);
                        if (d(rng)) {
                            editor.set_block(GRASS_BLOCK, x, 0, z, std::optional<std::vector<Block>>{ std::vector<Block>{MUD} }, std::nullopt);
                        }
                        editor.set_block(GRASS, x, 1, z, std::nullopt, std::nullopt);
                        continue;
                    }
                    std::bernoulli_distribution dwater(0.3);
                    if (dwater(rng)) {
                        editor.set_block(WATER, x, 0, z, std::optional<std::vector<Block>>{ std::vector<Block>{MUD, GRASS_BLOCK} }, std::nullopt);
                        continue;
                    }
                    if (!editor.check_for_block(x, 0, z, std::optional<std::vector<Block>>{ std::vector<Block>{MUD, MOSS_BLOCK} })) {
                        continue;
                    }
                    if (wetland_type == "reedbed") {
                        editor.set_block(TALL_GRASS_BOTTOM, x, 1, z, std::nullopt, std::nullopt);
                        editor.set_block(TALL_GRASS_TOP, x, 2, z, std::nullopt, std::nullopt);
                    } else if (wetland_type == "swamp" || wetland_type == "mangrove") {
                        std::uniform_int_distribution<int> uni40(0, 39);
                        int random_choice = uni40(rng);
                        if (random_choice == 0) {
                            Tree::create(editor, std::make_tuple(x, 1, z));
                        } else if (random_choice < 35) {
                            editor.set_block(GRASS, x, 1, z, std::nullopt, std::nullopt);
                        }
                    } else if (wetland_type == "bog") {
                        std::bernoulli_distribution dbog1(0.2);
                        if (dbog1(rng)) {
                            editor.set_block(MOSS_BLOCK, x, 0, z, std::optional<std::vector<Block>>{ std::vector<Block>{MUD} }, std::nullopt);
                        }
                        std::bernoulli_distribution dbog2(0.15);
                        if (dbog2(rng)) {
                            editor.set_block(GRASS, x, 1, z, std::nullopt, std::nullopt);
                        }
                    } else if (wetland_type == "tidalflat") {
                        continue;
                    } else {
                        editor.set_block(GRASS, x, 1, z, std::nullopt, std::nullopt);
                    }
                } else {
                    std::bernoulli_distribution dwater(0.3);
                    if (dwater(rng)) {
                        editor.set_block(WATER, x, 0, z, std::optional<std::vector<Block>>{ std::vector<Block>{MUD} }, std::nullopt);
                        continue;
                    }
                    editor.set_block(GRASS, x, 1, z, std::nullopt, std::nullopt);
                }
            } else if (natural_type == "mountain_range") {
                std::uniform_int_distribution<int> uni1000(0, 999);
                int cluster_chance = uni1000(rng);
                if (cluster_chance < 50) {
                    std::uniform_int_distribution<int> uni7(0, 6);
                    int cb = uni7(rng);
                    Block cluster_block = DIRT;
                    if (cb == 0) cluster_block = DIRT;
                    else if (cb == 1) cluster_block = STONE;
                    else if (cb == 2) cluster_block = GRAVEL;
                    else if (cb == 3) cluster_block = GRANITE;
                    else if (cb == 4) cluster_block = DIORITE;
                    else if (cb == 5) cluster_block = ANDESITE;
                    else cluster_block = GRASS_BLOCK;

                    std::uniform_int_distribution<int> cluster_size_dist(5, 10);
                    int cluster_size = cluster_size_dist(rng);

                    std::uniform_real_distribution<float> frand(0.0f, 1.0f);

                    for (int dx = -cluster_size; dx <= cluster_size; ++dx) {
                        for (int dz = -cluster_size; dz <= cluster_size; ++dz) {
                            int cluster_x = x + dx;
                            int cluster_z = z + dz;
                            float distance = std::sqrt(float(dx*dx + dz*dz));
                            if (distance <= static_cast<float>(cluster_size)) {
                                float place_prob = 1.0f - (distance / static_cast<float>(cluster_size));
                                if (frand(rng) < place_prob) {
                                    editor.set_block(cluster_block, cluster_x, 0, cluster_z, std::nullopt, std::nullopt);
                                    if (cluster_block == GRASS_BLOCK) {
                                        std::uniform_int_distribution<int> vegetation_chance_dist(0, 99);
                                        int vegetation_chance = vegetation_chance_dist(rng);
                                        if (vegetation_chance == 0) {
                                            Tree::create(editor, std::make_tuple(cluster_x, 1, cluster_z));
                                        } else if (vegetation_chance < 15) {
                                            editor.set_block(GRASS, cluster_x, 1, cluster_z, std::nullopt, std::nullopt);
                                        } else if (vegetation_chance < 25) {
                                            editor.set_block(OAK_LEAVES, cluster_x, 1, cluster_z, std::nullopt, std::nullopt);
                                        }
                                    }
                                }
                            }
                        }
                    }
                }
            } else if (natural_type == "saddle") {
                std::uniform_int_distribution<int> uni100(0, 99);
                int terrain_chance = uni100(rng);
                if (terrain_chance < 30) {
                    editor.set_block(STONE, x, 0, z, std::nullopt, std::nullopt);
                } else if (terrain_chance < 50) {
                    editor.set_block(GRAVEL, x, 0, z, std::nullopt, std::nullopt);
                } else {
                    editor.set_block(GRASS_BLOCK, x, 0, z, std::nullopt, std::nullopt);
                    std::bernoulli_distribution dgrass(0.4);
                    if (dgrass(rng)) {
                        editor.set_block(GRASS, x, 1, z, std::nullopt, std::nullopt);
                    }
                }
            } else if (natural_type == "ridge") {
                std::uniform_int_distribution<int> uni100(0, 99);
                int ridge_chance = uni100(rng);
                if (ridge_chance < 60) {
                    std::uniform_int_distribution<int> uni4(0, 3);
                    int rock = uni4(rng);
                    Block rock_type = STONE;
                    if (rock == 0) rock_type = STONE;
                    else if (rock == 1) rock_type = COBBLESTONE;
                    else if (rock == 2) rock_type = GRANITE;
                    else rock_type = ANDESITE;
                    editor.set_block(rock_type, x, 0, z, std::nullopt, std::nullopt);
                } else {
                    editor.set_block(GRASS_BLOCK, x, 0, z, std::nullopt, std::nullopt);
                    std::uniform_int_distribution<int> uni100b(0, 99);
                    int vegetation_chance = uni100b(rng);
                    if (vegetation_chance < 20) {
                        editor.set_block(GRASS, x, 1, z, std::nullopt, std::nullopt);
                    } else if (vegetation_chance < 25) {
                        editor.set_block(OAK_LEAVES, x, 1, z, std::nullopt, std::nullopt);
                    }
                }
            } else if (natural_type == "shrubbery") {
                editor.set_block(OAK_LEAVES, x, 1, z, std::nullopt, std::nullopt);
                editor.set_block(OAK_LEAVES, x, 2, z, std::nullopt, std::nullopt);
            } else if (natural_type == "tundra") {
                if (!editor.check_for_block(x, 0, z, std::optional<std::vector<Block>>{ std::vector<Block>{GRASS_BLOCK} })) {
                    continue;
                }
                std::uniform_int_distribution<int> uni100(0, 99);
                int tundra_chance = uni100(rng);
                if (tundra_chance < 40) {
                    editor.set_block(GRASS, x, 1, z, std::nullopt, std::nullopt);
                } else if (tundra_chance < 60) {
                    editor.set_block(MOSS_BLOCK, x, 0, z, std::optional<std::vector<Block>>{ std::vector<Block>{GRASS_BLOCK} }, std::nullopt);
                } else if (tundra_chance < 70) {
                    editor.set_block(DEAD_BUSH, x, 1, z, std::nullopt, std::nullopt);
                }
            } else if (natural_type == "cliff") {
                std::uniform_int_distribution<int> uni100(0, 99);
                int cliff_chance = uni100(rng);
                if (cliff_chance < 90) {
                    std::uniform_int_distribution<int> uni4(0, 3);
                    int stone_type_choice = uni4(rng);
                    Block stone_type = STONE;
                    if (stone_type_choice == 0) stone_type = STONE;
                    else if (stone_type_choice == 1) stone_type = COBBLESTONE;
                    else if (stone_type_choice == 2) stone_type = ANDESITE;
                    else stone_type = DIORITE;
                    editor.set_block(stone_type, x, 0, z, std::nullopt, std::nullopt);
                } else {
                    editor.set_block(GRAVEL, x, 0, z, std::nullopt, std::nullopt);
                }
            } else if (natural_type == "hill") {
                if (!editor.check_for_block(x, 0, z, std::optional<std::vector<Block>>{ std::vector<Block>{GRASS_BLOCK} })) {
                    continue;
                }
                std::uniform_int_distribution<int> uni1000(0, 999);
                int hill_chance = uni1000(rng);
                if (hill_chance == 0) {
                    Tree::create(editor, std::make_tuple(x, 1, z));
                } else if (hill_chance < 50) {
                    std::uniform_int_distribution<int> flower_dist(1, 4);
                    int f = flower_dist(rng);
                    Block flower_block = RED_FLOWER;
                    if (f == 1) flower_block = RED_FLOWER;
                    else if (f == 2) flower_block = BLUE_FLOWER;
                    else if (f == 3) flower_block = YELLOW_FLOWER;
                    else flower_block = WHITE_FLOWER;
                    editor.set_block(flower_block, x, 1, z, std::nullopt, std::nullopt);
                } else if (hill_chance < 600) {
                    editor.set_block(GRASS, x, 1, z, std::nullopt, std::nullopt);
                } else if (hill_chance < 650) {
                    editor.set_block(TALL_GRASS_BOTTOM, x, 1, z, std::nullopt, std::nullopt);
                    editor.set_block(TALL_GRASS_TOP, x, 2, z, std::nullopt, std::nullopt);
                }
            }
        }
    }
}

// Generate natural from relation
//static
 void generate_natural_from_relation(WorldEditor& editor, const ProcessedRelation& rel, const Args& args) {
    if (rel.tags.find("natural") == rel.tags.end()) return;

    for (const auto& member : rel.members) {
        if (member.role == ProcessedMemberRole::Outer) {
            ProcessedElement elem = ProcessedElement::FromWay(member.way);
            generate_natural(editor, elem, args);
        }
    }

    std::vector<ProcessedNode> combined_nodes;
    for (const auto& member : rel.members) {
        if (member.role == ProcessedMemberRole::Outer) {
            combined_nodes.insert(combined_nodes.end(), member.way.nodes.begin(), member.way.nodes.end());
        }
    }

    if (!combined_nodes.empty()) {
        ProcessedWay combined_way;
        combined_way.id = rel.id;
        combined_way.nodes = combined_nodes;
        combined_way.tags = rel.tags;
        ProcessedElement combined_elem = ProcessedElement::FromWay(combined_way);
        generate_natural(editor, combined_elem, args);
    }
}

} // namespace osm
}