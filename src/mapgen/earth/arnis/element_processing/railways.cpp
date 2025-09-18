#include <vector>
#include <tuple>
#include <string>
#include <unordered_map>
#include <optional>
#include <cmath>

using std::vector;
using std::tuple;
using std::get;
using std::string;
using std::unordered_map;
using std::optional;
using std::nullopt;
using std::pair;

#include "../bresenham.h"
#include "../../arnis_adapter.h"
#undef stoi
namespace arnis
{


namespace railways
{


// --- Block definitions (example) ---
#if 0
enum class Block {
    GRAVEL,
    OAK_LOG,
    RAIL_NORTH_SOUTH,
    RAIL_EAST_WEST,
    RAIL_NORTH_WEST,
    RAIL_NORTH_EAST,
    RAIL_SOUTH_WEST,
    RAIL_SOUTH_EAST,
    IRON_BLOCK,
    // ... add more as needed
};

// --- Minimal stubs for types used in functions ---
struct XZ {
    int x;
    int z;
};

struct Node {
    int bx;
    int bz;
    XZ xz() const { return {bx, bz}; }
};

struct ProcessedWay {
    vector<Node> nodes;
    unordered_map<string, string> tags;
};

// WorldEditor stub - replace with your real implementation
struct WorldEditor {
    // metadata arguments are optional placeholders to match Rust's None, None
    void set_block(Block block, int x, int y, int z, optional<int> = nullopt, optional<int> = nullopt) {
        // Implement block placement in your world/editor here.
        // This stub does nothing.
    }
};

// Assume this function exists somewhere in your codebase:
vector<tuple<int,int,int>> bresenham_line(int x1, int y1, int z1, int x2, int y2, int z2);

// --- Helper functions translated from Rust ---

#endif

vector<tuple<int,int,int>> smooth_diagonal_rails(const vector<tuple<int,int,int>>& points) {
    vector<tuple<int,int,int>> smoothed;
    smoothed.reserve(points.size() * 2);

    for (size_t i = 0; i < points.size(); ++i) {
        auto current = points[i];
        smoothed.push_back(current);

        if (i + 1 >= points.size()) continue;

        auto next = points[i + 1];
        int x1 = get<0>(current);
        int y1 = get<1>(current);
        int z1 = get<2>(current);
        int x2 = get<0>(next);
        int z2 = get<2>(next);

        // If points are diagonally adjacent
        if (std::abs(x2 - x1) == 1 && std::abs(z2 - z1) == 1) {
            optional<tuple<int,int,int>> look_ahead = (i + 2 < points.size()) ? optional<tuple<int,int,int>>(points[i + 2]) : nullopt;
            optional<tuple<int,int,int>> look_behind = (i > 0) ? optional<tuple<int,int,int>>(points[i - 1]) : nullopt;

            tuple<int,int,int> intermediate;
            if (look_behind) {
                int prev_x = get<0>(*look_behind);
                //int prev_z = get<2>(*look_behind);
                if (prev_x == x1) {
                    // Coming from vertical, keep x constant
                    intermediate = {x1, y1, z2};
                } else {
                    // Coming from horizontal, keep z constant
                    intermediate = {x2, y1, z1};
                }
            } else if (look_ahead) {
                int next_x = get<0>(*look_ahead);
                if (next_x == x2) {
                    // Going to vertical, keep x constant
                    intermediate = {x2, y1, z1};
                } else {
                    // Going to horizontal, keep z constant
                    intermediate = {x1, y1, z2};
                }
            } else {
                // Default to horizontal first if no context
                intermediate = {x2, y1, z1};
            }

            smoothed.push_back(intermediate);
        }
    }

    return smoothed;
}

Block determine_rail_direction(
    const pair<int,int>& current,
    const optional<pair<int,int>>& prev,
    const optional<pair<int,int>>& next
) {
    int x = current.first;
    int z = current.second;

    if (prev && next) {
        int px = prev->first;
        int pz = prev->second;
        int nx = next->first;
        int nz = next->second;

        if (px == nx) {
            return RAIL_NORTH_SOUTH;
        } else if (pz == nz) {
            return RAIL_EAST_WEST;
        } else {
            pair<int,int> from_prev = {px - x, pz - z};
            pair<int,int> to_next   = {nx - x, nz - z};

            // East to North or North to East
            if ((from_prev == pair<int,int>{-1,0} && to_next == pair<int,int>{0,-1})
                || (from_prev == pair<int,int>{0,-1} && to_next == pair<int,int>{-1,0})) {
                return RAIL_NORTH_WEST;
            }
            // West to North or North to West
            if ((from_prev == pair<int,int>{1,0} && to_next == pair<int,int>{0,-1})
                || (from_prev == pair<int,int>{0,-1} && to_next == pair<int,int>{1,0})) {
                return RAIL_NORTH_EAST;
            }
            // East to South or South to East
            if ((from_prev == pair<int,int>{-1,0} && to_next == pair<int,int>{0,1})
                || (from_prev == pair<int,int>{0,1} && to_next == pair<int,int>{-1,0})) {
                return RAIL_SOUTH_WEST;
            }
            // West to South or South to West
            if ((from_prev == pair<int,int>{1,0} && to_next == pair<int,int>{0,1})
                || (from_prev == pair<int,int>{0,1} && to_next == pair<int,int>{1,0})) {
                return RAIL_SOUTH_EAST;
            }

            if (std::abs(px - x) > std::abs(pz - z)) {
                return RAIL_EAST_WEST;
            } else {
                return RAIL_NORTH_SOUTH;
            }
        }
    } else if (prev || next) {
        pair<int,int> p = prev ? *prev : *next;
        int px = p.first;
        int pz = p.second;

        if (px == x) {
            return RAIL_NORTH_SOUTH;
        } else if (pz == z) {
            return RAIL_EAST_WEST;
        } else {
            return RAIL_NORTH_SOUTH;
        }
    } else {
        return RAIL_NORTH_SOUTH;
    }
}

// --- Main generation functions ---

void generate_railways(WorldEditor& editor, const ProcessedWay& element) {
    auto it = element.tags.find("railway");
    if (it == element.tags.end()) return;

    const string& railway_type = it->second;
    // Skip undesired railway types
    const vector<string> skip_types = {
        "proposed", "abandoned", "subway", "construction", "razed", "turntable"
    };
    for (const auto& s : skip_types) {
        if (railway_type == s) return;
    }

    if (auto it_subway = element.tags.find("subway"); it_subway != element.tags.end() && it_subway->second == "yes") {
        return;
    }
    if (auto it_tunnel = element.tags.find("tunnel"); it_tunnel != element.tags.end() && it_tunnel->second == "yes") {
        return;
    }

    if (element.nodes.size() < 2) return;

    for (size_t i = 1; i < element.nodes.size(); ++i) {
        XZ prev_node = element.nodes[i - 1].xz();
        XZ cur_node  = element.nodes[i].xz();

        auto points = bresenham_line(prev_node.x, 0, prev_node.z, cur_node.x, 0, cur_node.z);
        auto smoothed_points = smooth_diagonal_rails(points);

        for (size_t j = 0; j < smoothed_points.size(); ++j) {
            int bx = get<0>(smoothed_points[j]);
            //int by = get<1>(smoothed_points[j]);
            int bz = get<2>(smoothed_points[j]);

            editor.set_block(GRAVEL, bx, 0, bz, nullopt, nullopt);

            optional<pair<int,int>> prev_opt = nullopt;
            optional<pair<int,int>> next_opt = nullopt;

            if (j > 0) {
                int px = get<0>(smoothed_points[j - 1]);
                int pz = get<2>(smoothed_points[j - 1]);
                prev_opt = pair<int,int>{px, pz};
            }
            if (j + 1 < smoothed_points.size()) {
                int nx = get<0>(smoothed_points[j + 1]);
                int nz = get<2>(smoothed_points[j + 1]);
                next_opt = pair<int,int>{nx, nz};
            }

            Block rail_block = determine_rail_direction({bx, bz}, prev_opt, next_opt);
            editor.set_block(rail_block, bx, 1, bz, nullopt, nullopt);

            if ((bx % 4) == 0) {
                editor.set_block(OAK_LOG, bx, 0, bz, nullopt, nullopt);
            }
        }
    }
}

void generate_roller_coaster(WorldEditor& editor, const ProcessedWay& element) {
    auto it = element.tags.find("roller_coaster");
    if (it == element.tags.end()) return;

    if (it->second != "track") return;

    // Skip indoor
    if (auto it_indoor = element.tags.find("indoor"); it_indoor != element.tags.end() && it_indoor->second == "yes") {
        return;
    }

    // Skip negative layer
    if (auto it_layer = element.tags.find("layer"); it_layer != element.tags.end()) {
        try {
            int layer_value = std::stoi(it_layer->second);
            if (layer_value < 0) return;
        } catch (...) {
            // parse error -> ignore layer
        }
    }

    const int elevation_height = 4; // 4 blocks in the air
    const int pillar_interval = 6;  // supports every 6 blocks

    if (element.nodes.size() < 2) return;

    for (size_t i = 1; i < element.nodes.size(); ++i) {
        XZ prev_node = element.nodes[i - 1].xz();
        XZ cur_node  = element.nodes[i].xz();

        auto points = bresenham_line(prev_node.x, 0, prev_node.z, cur_node.x, 0, cur_node.z);
        auto smoothed_points = smooth_diagonal_rails(points);

        for (size_t j = 0; j < smoothed_points.size(); ++j) {
            int bx = get<0>(smoothed_points[j]);
            //int by = get<1>(smoothed_points[j]);
            int bz = get<2>(smoothed_points[j]);

            // Foundation at elevation_height
            editor.set_block(IRON_BLOCK, bx, elevation_height, bz, nullopt, nullopt);

            optional<pair<int,int>> prev_opt = nullopt;
            optional<pair<int,int>> next_opt = nullopt;

            if (j > 0) {
                int px = get<0>(smoothed_points[j - 1]);
                int pz = get<2>(smoothed_points[j - 1]);
                prev_opt = pair<int,int>{px, pz};
            }
            if (j + 1 < smoothed_points.size()) {
                int nx = get<0>(smoothed_points[j + 1]);
                int nz = get<2>(smoothed_points[j + 1]);
                next_opt = pair<int,int>{nx, nz};
            }

            Block rail_block = determine_rail_direction({bx, bz}, prev_opt, next_opt);
            // Place rail on top of foundation
            editor.set_block(rail_block, bx, elevation_height + 1, bz, nullopt, nullopt);

            // Place support pillars every pillar_interval blocks
            if ((bx % pillar_interval) == 0 && (bz % pillar_interval) == 0) {
                for (int y = 1; y < elevation_height; ++y) {
                    editor.set_block(IRON_BLOCK, bx, y, bz, nullopt, nullopt);
                }
            }
        }
    }
}

}
}