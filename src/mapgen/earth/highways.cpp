
#include <osmium/osm/item_type.hpp>
#include <vector>
#include <map>
#include <string>
#include <optional>
#include <cmath>
#include <algorithm>

#include "bresenham.h"
#include "adapter.h"
using namespace arnis;

#undef stoi

namespace highways
{

/*
// Placeholder definitions for Block types
enum class Block {
    COBBLESTONE_WALL,
    OAK_FENCE,
    GLOWSTONE,
    GREEN_WOOL,
    YELLOW_WOOL,
    RED_WOOL,
    STONE,
    BRICK,
    OAK_PLANKS,
    BLACK_CONCRETE,
    SAND,
    GRASS_BLOCK,
    DIRT,
    LIGHT_GRAY_CONCRETE,
    GRAY_CONCRETE,
    DIRT_PATH,
    WHITE_WOOL,
    // Add other block types as needed
};

// Placeholder for 3D point
struct Point3D {
    int x;
    int y;
    int z;
};

// Placeholder for 2D point
struct Point2D {
    int x;
    int z;
};

// Placeholder for node in OSM
struct ProcessedNode {
    int x;
    int z;
};

// Placeholder for way in OSM
struct ProcessedWay {
    std::vector<ProcessedNode> nodes;
};

// Placeholder for processed element (node or way)
class ProcessedElement {
public:
    enum class Type { Node, Way };
    Type type;
    std::map<std::string, std::string> tags;
    ProcessedNode node;
    ProcessedWay way;

    // Constructors
    ProcessedElement(const ProcessedNode& n) : type(Type::Node), node(n) {}
    ProcessedElement(const ProcessedWay& w) : type(Type::Way), way(w) {}

    std::optional<std::string> get_tag(const std::string& key) const {
        auto it = tags.find(key);
        if (it != tags.end()) {
            return it->second;
        }
        return std::nullopt;
    }
};

// Placeholder for arguments
struct Args {
    double scale;
    std::optional<int> timeout;
};

// Placeholder for WorldEditor class
class WorldEditor {
public:
    void set_block(Block block_type, int x, int y, int z,
                   const std::optional<std::vector<Block>>& replace_blocks = std::nullopt,
                   const std::optional<std::vector<Block>>& exclude_blocks = std::nullopt) {
        // Implementation to set block in the world
    }

    bool check_for_block(int x, int y, int z, const std::vector<Block>& blocks) {
        // Check if a block at position matches any in blocks
        return false; // Placeholder
    }
};

// Placeholder for bresenham_line function
std::vector<Point3D> bresenham_line(int x1, int y1, int z1, int x2, int y2, int z2) {
    // Implement Bresenham's line algorithm here
    // For simplicity, returning a straight line between points
    std::vector<Point3D> points;
    // Basic implementation omitted for brevity
    return points;
}

// Placeholder for flood_fill_area function
std::vector<std::pair<int, int>> flood_fill_area(const std::vector<std::pair<int, int>>& polygon_coords, const std::optional<int>& timeout) {
    // Implement flood fill algorithm here
    // For now, return the polygon coords as a placeholder
    return polygon_coords;
}
*/

// Core functions translating from Rust to C++

void generate_highways(
		WorldEditor &editor, const ProcessedElement &element, const Args &args)
{
	std::string highway_type = element.get_value_by_key("highway");
	//DUMP(highway_type, element.type());
	if (highway_type.empty())
		return;
	//const std::string& highway_type = highway_type_opt.value();

	if (highway_type == "street_lamp") {
		if (element.type() == osmium::item_type::node) {
			const auto &node = *static_cast<const ProcessedNode *>(&element);
			//int x = element.node.x;
			//int z = element.node.z;

			const auto [x, z] = editor.node_to_xz(node.location());

			editor.set_block(COBBLESTONE_WALL, x, 1, z);
			for (int dy = 2; dy <= 4; ++dy) {
				editor.set_block(OAK_FENCE, x, dy, z);
			}
			editor.set_block(GLOWSTONE, x, 5, z);
		}
	} else if (highway_type == "crossing") {
		std::string crossing_type_opt = element.tags().get_value_by_key("crossing");
		if (crossing_type_opt == "traffic_signals") {
			if (element.type() == osmium::item_type::node) {
				//                int x = element.node.x;
				//              int z = element.node.z;
				const auto &node = *static_cast<const ProcessedNode *>(&element);
				const auto [x, z] = editor.node_to_xz(node.location());
				for (int dy = 1; dy <= 3; ++dy) {
					editor.set_block(COBBLESTONE_WALL, x, dy, z);
				}
				editor.set_block(GREEN_WOOL, x, 4, z);
				editor.set_block(YELLOW_WOOL, x, 5, z);
				editor.set_block(RED_WOOL, x, 6, z);
			}
		}
	} else if (highway_type == "bus_stop") {
		if (element.type() == osmium::item_type::node) {
			//const auto [x, z] = editor.node_to_xz(element);
			const auto &node = *static_cast<const ProcessedNode *>(&element);
			const auto [x, z] = editor.node_to_xz(node.location());
			for (int dy = 1; dy <= 3; ++dy) {
				editor.set_block(COBBLESTONE_WALL, x, dy, z);
			}
			editor.set_block(WHITE_WOOL, x, 4, z);
			editor.set_block(WHITE_WOOL, x + 1, 4, z);
		}
	} else if (element.tags().has_key("area") &&
			   element.tags().get_value_by_key("area") == std::string{"yes"}
			//std::string{element.tags().get_value_by_key("area")} == std::string{"yes"}
	) {
		if (element.type() != osmium::item_type::way)
			return;
		//const auto &way = element.way;
		const auto &way = *static_cast<const ProcessedWay *>(&element);
		Block surface_block = STONE; // default

		if (element.tags().has_key("surface"))
			if (const std::string surface = element.get_value_by_key("surface");
					!surface.empty()) {
				//const std::string &surface = surface_tag.value();
				if (surface == "paving_stones" || surface == "sett")
					surface_block = BRICK; // Assuming BRICK as placeholder
				else if (surface == "bricks")
					surface_block = BRICK;
				else if (surface == "wood")
					surface_block = OAK_PLANKS;
				else if (surface == "asphalt")
					surface_block = BLACK_CONCRETE;
				else if (surface == "gravel" || surface == "fine_gravel")
					surface_block = SAND; // Placeholder
				else if (surface == "grass")
					surface_block = GRASS_BLOCK;
				else if (surface == "dirt" || surface == "ground" || surface == "earth")
					surface_block = DIRT;
				else if (surface == "sand")
					surface_block = SAND;
				else if (surface == "concrete")
					surface_block = LIGHT_GRAY_CONCRETE;
				// default remains
			}

		std::vector<std::pair<int, int>> polygon_coords;
		for (const auto &node : way.nodes()) {
			polygon_coords.emplace_back(editor.node_to_xz(node));
		}

		auto filled_area =
				FloodFill::flood_fill_area(polygon_coords, &args.timeout.value());

		for (const auto &[x, z] : filled_area) {
			editor.set_block(surface_block, x, 0, z);
		}
	} else {
		// Default highway processing
		std::optional<std::pair<int, int>> previous_node = std::nullopt;
		Block block_type = BLACK_CONCRETE;
		int block_range = 2;
		bool add_stripe = false;
		bool add_outline = false;
		double scale_factor = args.scale;

		// Check layer and level tags for negative values
		if (element.tags().has_key("layer")) {
			std::string layer_tag = element.get_value_by_key("layer");
			if (!layer_tag.empty() && std::stoi(layer_tag) < 0) {
				DUMP("nola");

				return;
			}
		}

		if (element.tags().has_key("level")) {
			std::string level_tag = element.get_value_by_key("level");
			if (!level_tag.empty() && std::stoi(level_tag) < 0) {
				DUMP("nolt");

				return;
			}
		}

		if (element.type() != osmium::item_type::way) {
			DUMP("noway");
			return;
		}
		//const auto &way = element.way;
		const auto &way = *static_cast<const ProcessedWay *>(&element);

		// Determine block type and style based on highway type
		if (highway_type == "footway" || highway_type == "pedestrian") {
			block_type = GRAY_CONCRETE;
			block_range = 1;
		} else if (highway_type == "path") {
			block_type = DIRT;
			block_range = 1;
		} else if (highway_type == "motorway" || highway_type == "primary") {
			block_range = 5;
			add_stripe = true;
		} else if (highway_type == "tertiary") {
			add_stripe = true;
		} else if (highway_type == "track") {
			block_range = 1;
		} else if (highway_type == "service") {
			block_type = GRAY_CONCRETE;
			block_range = 2;
		} else if (highway_type == "secondary_link" || highway_type == "tertiary_link") {
			block_type = BLACK_CONCRETE;
			block_range = 1;
		} else if (highway_type == "escape") {
			block_type = SAND;
			block_range = 1;
		} else if (highway_type == "steps") {
			block_type = GRAY_CONCRETE;
			block_range = 1;
		} else {
			if (element.tags().has_key("lanes"))
				if (std::optional<std::string> lanes_tag =
								element.get_value_by_key("lanes")) {
					if (lanes_tag.value() == "2") {
						block_range = 3;
						add_stripe = true;
						add_outline = true;
					} else if (lanes_tag.value() != "1") {
						block_range = 4;
						add_stripe = true;
						add_outline = true;
					}
				}
		}

		if (scale_factor < 1.0) {
			block_range = static_cast<int>(std::floor(block_range * scale_factor));
		}

		for (const auto &node : way.nodes()) {
			if (previous_node.has_value()) {
				int x1 = previous_node->first;
				int z1 = previous_node->second;
				//int x2 = node.x;
				//nt z2 = node.z;
				const auto [x2, z2] = editor.node_to_xz(node.location());

				auto line_points = bresenham_line(x1, 0, z1, x2, 0, z2);

				int stripe_length = 0;
				int dash_length = static_cast<int>(std::ceil(5.0 * scale_factor));
				int gap_length = static_cast<int>(std::ceil(5.0 * scale_factor));

				for (const auto &point : line_points) {
					int x = point.X;
					int z = point.Z;
					//const auto [x, z] = editor.node_to_xz(point);

					// Draw surface for width
					for (int dx = -block_range; dx <= block_range; ++dx) {
						for (int dz = -block_range; dz <= block_range; ++dz) {
							int set_x = x + dx;
							int set_z = z + dz;

							// Zebra crossing logic
							if (element.tags().has_key("footway"))
								if (std::optional<std::string> footway_tag =
												element.get_value_by_key("footway");
										footway_tag.has_value() &&
										footway_tag.value() == "crossing") {
									bool is_horizontal =
											std::abs(x2 - x1) >= std::abs(z2 - z1);
									if (is_horizontal) {
										if (set_x % 2 == 0) {
											editor.set_block(WHITE_WOOL, set_x, 0, set_z);
										} else {
											editor.set_block(
													BLACK_CONCRETE, set_x, 0, set_z);
										}
									} else {
										if (set_z % 2 == 0) {
											editor.set_block(WHITE_WOOL, set_x, 0, set_z);
										} else {
											editor.set_block(
													BLACK_CONCRETE, set_x, 0, set_z);
										}
									}
								} else {
									editor.set_block(BLACK_CONCRETE, set_x, 0, set_z);
									// For multi-lane roads, you might add outline blocks here
									if (add_outline) {
										// Left outline
										editor.set_block(LIGHT_GRAY_CONCRETE,
												x - block_range - 1, 0, z);
										// Right outline
										editor.set_block(LIGHT_GRAY_CONCRETE,
												x + block_range + 1, 0, z);
									}
									// Dashed stripe
									if (add_stripe) {
										if (stripe_length < dash_length) {
											editor.set_block(WHITE_WOOL, x, 0, z);
										}
										stripe_length++;
										if (stripe_length >= dash_length + gap_length) {
											stripe_length = 0;
										}
									}
								}
						}
					}
				}
			}
			previous_node = editor.node_to_xz(node);
		}
	}
}

void generate_siding(WorldEditor &editor, const ProcessedElement &element_)
{
	const auto &element = *static_cast<const ProcessedWay *>(&element_);

	std::optional<Point2D> previous_node = std::nullopt;
	Block siding_block = STONE; // Assume STONE as placeholder

	for (const auto &node : element.nodes()) {
		const auto [x, z] = editor.node_to_xz(node);
		Point2D current_node{x, z};
		if (previous_node.has_value()) {
			auto line_points = bresenham_line(previous_node->x(), 0, previous_node->y(),
					current_node.x(), 0, current_node.y());
			for (const auto &pt : line_points) {
				if (!editor.check_for_block(
							pt.X, 0, pt.Z, {BLACK_CONCRETE, WHITE_CONCRETE})) {
					editor.set_block(siding_block, pt.X, 1, pt.Z);
				}
			}
		}
		previous_node = current_node;
	}
}

void generate_aeroway(
		WorldEditor &editor, const ProcessedElement &element, const Args &args)
{
	const auto &way = *static_cast<const ProcessedWay *>(&element);
	std::optional<std::pair<int, int>> previous_node = std::nullopt;
	Block surface_block = LIGHT_GRAY_CONCRETE;
	int way_width = static_cast<int>(std::ceil(12.0 * args.scale));

	for (const auto &node : way.nodes()) {
		const auto [x2, z2] = editor.node_to_xz(node);
		if (previous_node.has_value()) {
			int x1 = previous_node->first;
			int z1 = previous_node->second;
			//int x2 = node.x;
			//int z2 = node.z;

			auto points = bresenham_line(x1, 0, z1, x2, 0, z2);
			for (const auto &p : points) {
				for (int dx = -way_width; dx <= way_width; ++dx) {
					for (int dz = -way_width; dz <= way_width; ++dz) {
						editor.set_block(surface_block, p.X + dx, 0, p.Z + dz);
					}
				}
			}
		}
		previous_node = std::make_pair(x2, z2);
	}
}
/*
int main() {
    // Example usage
    WorldEditor editor;
    Args args{1.0, std::nullopt};

    // Create example ProcessedElement and call generate_highways
    ProcessedNode node{10, 20};
    ProcessedElement element(node);
    element.tags = {{"highway", "street_lamp"}};

    generate_highways(editor, element, args);

    // Additional calls as needed

    return 0;
}
*/
}
