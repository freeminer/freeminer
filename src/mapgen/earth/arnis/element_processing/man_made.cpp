#include <string>
#include <map>
#include <optional>
#include <tuple>
#include <algorithm>
#include <cmath>

#include "../../arnis_adapter.h"
namespace arnis
{

namespace man_made
{


#if 0
enum class Block {
    OAK_SLAB,
    OAK_LOG,
    IRON_BLOCK,
    IRON_BARS,
    GRAY_CONCRETE,
    BRICK,
    WATER,
    STONE_BRICKS,
    OAK_FENCE,
    POLISHED_ANDESITE,
    SMOOTH_STONE
};

struct Args {
    std::map<std::string, std::string> values;
};

struct ProcessedNode {
    int x{};
    int z{};
    std::map<std::string, std::string> tags;
};

struct ProcessedWay {
    std::vector<ProcessedNode> nodes;
    std::map<std::string, std::string> tags;
};

struct ProcessedElement {
    // Either way_nodes has content (Way) or single_node has content (Node)
    std::optional<std::vector<ProcessedNode>> way_nodes;
    std::optional<ProcessedNode> single_node;
    std::map<std::string, std::string> tags;

    const std::map<std::string, std::string>& tags_ref() const {
        return tags;
    }

    std::optional<std::string> tag(const std::string& key) const {
        auto it = tags.find(key);
        if (it != tags.end()) {
            return std::optional<std::string>(it->second);
        }
        return std::optional<std::string>();
    }

    std::optional<ProcessedNode> first_node() const {
        if (single_node.has_value()) {
            return single_node;
        }
        if (way_nodes.has_value() && !way_nodes->empty()) {
            return std::optional<ProcessedNode>(way_nodes->front());
        }
        return std::optional<ProcessedNode>();
    }

    const std::vector<ProcessedNode>& nodes_vec() const {
        static const std::vector<ProcessedNode> empty_vec{};
        if (way_nodes.has_value()) {
            return *way_nodes;
        }
        return empty_vec;
    }
};

class WorldEditor {
public:
    void set_block(Block block, int x, int y, int z,
                   std::optional<std::vector<Block>> = std::optional<std::vector<Block>>(),
                   std::optional<int> = std::optional<int>()) {
        // implementation-specific: place single block at (x,y,z)
    }

    void fill_blocks(Block block, int x1, int y1, int z1, int x2, int y2, int z2,
                     std::optional<std::vector<Block>> = std::optional<std::vector<Block>>(),
                     std::optional<int> = std::optional<int>()) {
        int xmin = std::min(x1, x2);
        int xmax = std::max(x1, x2);
        int ymin = std::min(y1, y2);
        int ymax = std::max(y1, y2);
        int zmin = std::min(z1, z2);
        int zmax = std::max(z1, z2);
        for (int x = xmin; x <= xmax; ++x) {
            for (int y = ymin; y <= ymax; ++y) {
                for (int z = zmin; z <= zmax; ++z) {
                    set_block(block, x, y, z, std::optional<std::vector<Block>>(), std::optional<int>());
                }
            }
        }
    }
};
#endif

std::optional<int> parse_int(const std::optional<std::string>& s) {
    if (!s.has_value()) return std::optional<int>();
    try {
        return std::optional<int>(std::stoi(s.value()));
    } catch (...) {
        return std::optional<int>();
    }
}

#if 0
std::vector<std::tuple<int,int,int>> bresenham_line(int x1, int y1, int z1, int x2, int /*y2*/, int z2) {
    // 2D Bresenham on X-Z plane. Y retained as y1 for all points.
    std::vector<std::tuple<int,int,int>> out;
    int dx = std::abs(x2 - x1);
    int dz = std::abs(z2 - z1);
    int sx = x1 < x2 ? 1 : -1;
    int sz = z1 < z2 ? 1 : -1;
    int err = (dx > dz ? dx : -dz) / 2;
    int x = x1;
    int z = z1;
    while (true) {
        out.emplace_back(x, y1, z);
        if (x == x2 && z == z2) break;
        int e2 = err;
        if (e2 > -dx) { err -= dz; x += sx; }
        if (e2 < dz)  { err += dx; z += sz; }
    }
    return out;
}
#endif

void generate_pier(WorldEditor& editor, const ProcessedElement& element) {
    const std::vector<ProcessedNode>& nodes = element.nodes_vec();
    if (nodes.size() < 2) return;

    int pier_width = 3;
    {
        std::optional<int> w = parse_int(element.tag("width"));
        if (w.has_value()) pier_width = w.value();
    }

    int pier_height = 1;
    int support_spacing = 4;

    for (std::size_t i = 0; i + 1 < nodes.size(); ++i) {
        const ProcessedNode& start_node = nodes[i];
        const ProcessedNode& end_node = nodes[i + 1];
        std::vector<std::tuple<int,int,int>> line_points = bresenham_line(start_node.x, 0, start_node.z, end_node.x, 0, end_node.z);
        for (std::size_t index = 0; index < line_points.size(); ++index) {
            int center_x = std::get<0>(line_points[index]);
            int center_z = std::get<2>(line_points[index]);

            int half_width = pier_width / 2;
            for (int x = center_x - half_width; x <= center_x + half_width; ++x) {
                for (int z = center_z - half_width; z <= center_z + half_width; ++z) {
                    editor.set_block(OAK_SLAB, x, pier_height, z, std::optional<std::vector<Block>>(), std::optional<int>());
                }
            }

            if ((index % support_spacing) == 0) {
                int hw = pier_width / 2;
                std::vector<std::pair<int,int>> support_positions = { {center_x - hw, center_z}, {center_x + hw, center_z} };
                for (const auto& p : support_positions) {
                    int pillar_x = p.first;
                    int pillar_z = p.second;
                    editor.set_block(OAK_LOG, pillar_x, 0, pillar_z, std::optional<std::vector<Block>>(), std::optional<int>());
                }
            }
        }
    }
}

void generate_antenna(WorldEditor& editor, const ProcessedElement& element) {
    std::optional<ProcessedNode> maybe_node = element.first_node();
    if (!maybe_node.has_value()) return;
    ProcessedNode node = maybe_node.value();
    int x = node.x;
    int z = node.z;

    int height = 20;
    std::optional<int> htag = parse_int(element.tag("height"));
    if (htag.has_value()) {
        height = std::min(htag.value(), 40);
    } else {
        std::optional<std::string> tower_type = element.tag("tower:type");
        if (tower_type.has_value()) {
            if (tower_type.value() == "communication") height = 20;
            else if (tower_type.value() == "cellular") height = 15;
            else height = 20;
        }
    }

    editor.set_block(IRON_BLOCK, x, 3, z, std::optional<std::vector<Block>>(), std::optional<int>());
    for (int y = 4; y < height; ++y) {
        editor.set_block(IRON_BARS, x, y, z, std::optional<std::vector<Block>>(), std::optional<int>());
    }

    for (int y = 7; y < height; y += 7) {
        editor.set_block(IRON_BLOCK, x, y, z, std::optional<std::vector<Block>>(std::vector<Block>{IRON_BARS}), std::optional<int>());
        std::vector<std::pair<int,int>> support_positions = { {1,0}, {-1,0}, {0,1}, {0,-1} };
        for (const auto& d : support_positions) {
            editor.set_block(IRON_BLOCK, x + d.first, y, z + d.second, std::optional<std::vector<Block>>(), std::optional<int>());
        }
    }

    editor.fill_blocks(GRAY_CONCRETE, x - 1, 1, z - 1, x + 1, 2, z + 1);
}

void generate_chimney(WorldEditor& editor, const ProcessedElement& element) {
    std::optional<ProcessedNode> maybe_node = element.first_node();
    if (!maybe_node.has_value()) return;
    ProcessedNode node = maybe_node.value();
    int x = node.x;
    int z = node.z;
    int height = 25;

    for (int y = 0; y < height; ++y) {
        for (int dx = -1; dx <= 1; ++dx) {
            for (int dz = -1; dz <= 1; ++dz) {
                if (dx == 0 && dz == 0) continue;
                editor.set_block(BRICK, x + dx, y, z + dz, std::optional<std::vector<Block>>(), std::optional<int>());
            }
        }
    }
}

void generate_water_well(WorldEditor& editor, const ProcessedElement& element) {
    std::optional<ProcessedNode> maybe_node = element.first_node();
    if (!maybe_node.has_value()) return;
    ProcessedNode node = maybe_node.value();
    int x = node.x;
    int z = node.z;

    for (int dx = -1; dx <= 1; ++dx) {
        for (int dz = -1; dz <= 1; ++dz) {
            if (dx == 0 && dz == 0) {
                editor.set_block(WATER, x, -1, z, std::optional<std::vector<Block>>(), std::optional<int>());
                editor.set_block(WATER, x, 0, z, std::optional<std::vector<Block>>(), std::optional<int>());
            } else {
                editor.set_block(STONE_BRICKS, x + dx, 0, z + dz, std::optional<std::vector<Block>>(), std::optional<int>());
                editor.set_block(STONE_BRICKS, x + dx, 1, z + dz, std::optional<std::vector<Block>>(), std::optional<int>());
            }
        }
    }

    editor.fill_blocks(OAK_LOG, x - 2, 1, z, x - 2, 4, z, std::optional<std::vector<Block>>(), std::optional<int>());
    editor.fill_blocks(OAK_LOG, x + 2, 1, z, x + 2, 4, z, std::optional<std::vector<Block>>(), std::optional<int>());

    editor.set_block(OAK_SLAB, x - 1, 5, z, std::optional<std::vector<Block>>(), std::optional<int>());
    editor.set_block(OAK_FENCE, x, 4, z, std::optional<std::vector<Block>>(), std::optional<int>());
    editor.set_block(OAK_SLAB, x, 5, z, std::optional<std::vector<Block>>(), std::optional<int>());
    editor.set_block(OAK_SLAB, x + 1, 5, z, std::optional<std::vector<Block>>(), std::optional<int>());

    editor.set_block(IRON_BLOCK, x, 3, z, std::optional<std::vector<Block>>(), std::optional<int>());
}

void generate_water_tower(WorldEditor& editor, const ProcessedElement& element) {
    std::optional<ProcessedNode> maybe_node = element.first_node();
    if (!maybe_node.has_value()) return;
    ProcessedNode node = maybe_node.value();
    int x = node.x;
    int z = node.z;
    int tower_height = 20;
    int tank_height = 6;

    std::vector<std::pair<int,int>> leg_positions = { {-2,-2}, {2,-2}, {-2,2}, {2,2} };
    for (const auto& p : leg_positions) {
        int dx = p.first;
        int dz = p.second;
        for (int y = 0; y < tower_height; ++y) {
            editor.set_block(IRON_BLOCK, x + dx, y, z + dz, std::optional<std::vector<Block>>(), std::optional<int>());
        }
    }

    for (int y = 5; y < tower_height; y += 5) {
        for (int dx = -1; dx <= 1; ++dx) {
            editor.set_block(SMOOTH_STONE, x + dx, y, z - 2, std::optional<std::vector<Block>>(), std::optional<int>());
            editor.set_block(SMOOTH_STONE, x + dx, y, z + 2, std::optional<std::vector<Block>>(), std::optional<int>());
        }
        for (int dz = -1; dz <= 1; ++dz) {
            editor.set_block(SMOOTH_STONE, x - 2, y, z + dz, std::optional<std::vector<Block>>(), std::optional<int>());
            editor.set_block(SMOOTH_STONE, x + 2, y, z + dz, std::optional<std::vector<Block>>(), std::optional<int>());
        }
    }

    editor.fill_blocks(POLISHED_ANDESITE, x - 3, tower_height, z - 3, x + 3, tower_height + tank_height, z + 3, std::optional<std::vector<Block>>(), std::optional<int>());

    for (int y = 0; y < tower_height; ++y) {
        editor.set_block(POLISHED_ANDESITE, x, y, z, std::optional<std::vector<Block>>(), std::optional<int>());
    }
}

void generate_man_made(WorldEditor& editor, const ProcessedElement& element, const Args& /*_args*/) {
    if (element.tag("layer").has_value()) {
        std::optional<int> layer = parse_int(element.tag("layer"));
        if (layer.has_value() && layer.value() < 0) return;
    }
    if (element.tag("level").has_value()) {
        std::optional<int> level = parse_int(element.tag("level"));
        if (level.has_value() && level.value() < 0) return;
    }

    std::optional<std::string> man_made_type = element.tag("man_made");
    if (!man_made_type.has_value()) return;
    const std::string& t = man_made_type.value();
    if (t == "pier") {
        generate_pier(editor, element);
    } else if (t == "antenna" || t == "mast") {
        generate_antenna(editor, element);
    } else if (t == "chimney") {
        generate_chimney(editor, element);
    } else if (t == "water_well") {
        generate_water_well(editor, element);
    } else if (t == "water_tower") {
        generate_water_tower(editor, element);
    } else {
        // unknown type -> ignore
    }
}

void generate_man_made_nodes(WorldEditor& editor, const ProcessedNode& node) {
    auto it = node.tags.find("man_made");
    if (it == node.tags.end()) return;
    
    ProcessedElement element(node);
    //element.single_node = node;
    //element.tags = node.tags;

    const std::string& t = it->second;
    if (t == "antenna" || t == "mast") {
        generate_antenna(editor, element);
    } else if (t == "chimney") {
        generate_chimney(editor, element);
    } else if (t == "water_well") {
        generate_water_well(editor, element);
    } else if (t == "water_tower") {
        generate_water_tower(editor, element);
    } else {
        // unknown -> ignore
    }
}
}
}