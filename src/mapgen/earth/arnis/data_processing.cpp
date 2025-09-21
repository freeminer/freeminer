//#include "adapter.cpp"
#include "data_processing.h"
#include <sys/types.h>

#include "../arnis_adapter.h"
//#include "element_processing/buildings.h"

#if 0
#include <iostream>
#include <vector>
#include <string>
#include <map>
#include <cmath>
#include <algorithm>

// Placeholder for your actual implementations
struct Args {
    std::string path;
    bool debug;
    bool fillground;
    // Add other fields as necessary
    // Example: spawn_point, scale, bbox, etc.
};

struct Coordinate {
    int x, y, z;
};

struct TagMap : public std::map<std::string, std::string> {};

struct ProcessedElement {
    enum class Type { Way, Node, Relation } type;
    std::string id;
    // Common tags
    TagMap tags;

    // Specific data for each type
    // For simplicity, only tags are used here
};

struct Ground {
    // Placeholder for ground-related data
};

struct XZBBox {
    int min_x, max_x;
    int min_z, max_z;

    int min_x() const { return min_x; }
    int max_x() const { return max_x; }
    int min_z() const { return min_z; }
    int max_z() const { return max_z; }

    // Placeholder for bounding rectangle
    struct Rect {
        int total_blocks() const {
            return (max_x - min_x + 1) * (max_z - min_z + 1);
        }
    };

    Rect bounding_rect() const {
        return Rect{(max_x - min_x + 1) * (max_z - min_z + 1)};
    }
};

class WorldEditor {
public:
    WorldEditor(const std::string& regionDir, const XZBBox& bbox) {
        // Initialize editor with region directory and bounding box
    }

    void set_ground(const Ground& ground) {
        // Set ground reference
    }

    bool check_for_block(int x, int y, int z, const std::vector<int>* block_types) {
        // Check if block exists at position
        return false; // Placeholder
    }

    void set_block(int block_type, int x, int y, int z,
                   const std::vector<int>* optional_blocks = nullptr,
                   const std::vector<int>* optional_blocks2 = nullptr) {
        // Set block at position
    }

    void fill_blocks_absolute(int block_type, int x1, int y1, int z1,
                              int x2, int y2, int z2,
                              const std::vector<int>* optional_blocks = nullptr,
                              const std::vector<int>* optional_blocks2 = nullptr) {
        // Fill blocks in region
    }

    int get_absolute_y(int x, int y, int z) {
        // Convert local to absolute Y
        return y; // Placeholder
    }

    void save() {
        // Save world data
    }

    // Additional methods as needed
};

// Placeholder functions for progress update and GUI
void emit_gui_progress_update(double progress, const std::string& message) {
    std::cout << "[Progress] " << progress << "%: " << message << std::endl;
}

// Constants
const int MIN_Y = -64;
const int BEDROCK = 7;     // Example block type for bedrock
const int DIRT = 3;        // Example block type for dirt
const int GRASS_BLOCK = 2; // Example block type for grass
const int STONE = 1;       // Example block type for stone

#endif

namespace arnis
{

// Placeholder namespaces for different generation functions
namespace buildings
{

void generate_building_from_relation(
		WorldEditor &editor, const ProcessedRelation &relation, const Args &args);
void generate_buildings(WorldEditor *editor, const ProcessedWay &element,
		const Args &args, const std::optional<int> &relation_levels);

}

namespace highways
{
void generate_highways(WorldEditor &editor, const ProcessedElement &element,
		const Args &args,
		const std::vector<crate::osm_parser::ProcessedElement> &all_elements);
void generate_aeroway(WorldEditor &editor, const ProcessedWay &way, const Args &args);
void generate_siding(WorldEditor &editor, const ProcessedWay &way);
}

namespace landuse
{
void generate_landuse(WorldEditor &editor, const ProcessedWay &way, const Args &args);

void generate_landuse_from_relation(
		WorldEditor &editor, const ProcessedRelation &rel, const Args &args);
}

namespace natural
{
void generate_natural(
		WorldEditor &editor, const ProcessedElement &element, const Args &args);

void generate_natural_from_relation(
		WorldEditor &editor, const ProcessedRelation &rel, const Args &args);
}

namespace amenities
{
void generate_amenities(
		WorldEditor &editor, const ProcessedElement &element, const Args &args);
}

namespace leisure
{
void generate_leisure(WorldEditor &editor, const ProcessedWay &way, const Args &args);
void generate_leisure_from_relation(
		WorldEditor &editor, const ProcessedRelation &rel, const Args &args);
}

namespace barriers
{
void generate_barriers(WorldEditor &editor, const ProcessedElement &element);
void generate_barrier_nodes(WorldEditor &editor, const ProcessedNode &node);
}

namespace waterways
{
void generate_waterways(WorldEditor &editor, const ProcessedWay &way);
}

namespace railways
{
void generate_roller_coaster(WorldEditor &editor, const ProcessedWay &way);
void generate_railways(WorldEditor &editor, const ProcessedWay &element);
}

namespace tourisms
{
void generate_tourisms(WorldEditor &editor, const ProcessedNode &node);
}

namespace man_made
{
void generate_man_made(
		WorldEditor &editor, const ProcessedElement &element, const Args &args);
void generate_man_made_nodes(WorldEditor &editor, const ProcessedNode &node);
}

namespace water_areas
{
void generate_water_areas(WorldEditor &editor, const ProcessedRelation &rel);
}

namespace doors
{
void generate_doors(WorldEditor &editor, const ProcessedNode &rel);
}

// Main generate_world function
//bool generate_world(MapgenEarth *mg, const std::vector<ProcessedElement> &elements,
//	const ProcessedElement &element,
//const XZBBox& xzbbox,
//const Ground& ground,
//const Args &args)
bool generate_world(WorldEditor &editor, const std::vector<ProcessedElement> &elements,
		const Args &args_)
{
	//arnis::init(arnis::mg);

	//ProcessedRelation relation;
/*
    Ground ground;
	// ground.mg = arnis::mg;
	ground.mg = mg;
	//Args args;
	WorldEditor editor;
	//editor.mg = arnis::mg;
	editor.mg = mg;
	editor.ground = &ground;
*/
#if 0

    std::string region_dir = args.path + "/region";
    WorldEditor editor(region_dir, xzbbox);


    std::cout << "[4/7] Processing data..." << std::endl;

    // Set ground reference
    editor.set_ground(ground);

    std::cout << "[5/7] Processing terrain..." << std::endl;
    emit_gui_progress_update(25.0, "Processing terrain...");

    size_t elements_count = elements.size();
    double total_progress = 25.0;
    double progress_increment_prcs = 45.0 / static_cast<double>(elements_count);
    double current_progress_prcs = 25.0;
    double last_emitted_progress = current_progress_prcs;
#endif

	for (auto const &element : elements) {
		/*
    process_pb.inc(1);
    current_progress_prcs += progress_increment_prcs;
    if (std::abs(current_progress_prcs - last_emitted_progress) > 0.25) {
        crate::progress::emit_gui_progress_update(current_progress_prcs, std::string(""));
        last_emitted_progress = current_progress_prcs;
    }

    if (args.debug) {
        process_pb.set_message(std::string("(Element ID: ") + std::to_string(element.id()) + std::string(" / Type: ") + element.kind() + std::string(")"));
    } else {
        process_pb.set_message(std::string(""));
    }
*/
		//DUMP(element.kind(), element.is_way());

		auto args = args_;
		//margs.ground_level = 1;

		if (element.is_way()) {

			auto const &way = element.as_way();

			if (!way.nodes.empty()) {
				args.ground_level = editor.ground->level(way.nodes.begin()->xz());
			}

			if (way.tags.contains("building") || way.tags.contains("building:part")) {
				buildings::generate_buildings(&editor, way, args, std::optional<int>{});
			} else if (way.tags.contains("highway")) {
				highways::generate_highways(editor, element, args, elements);
			} else if (way.tags.contains("landuse")) {
				landuse::generate_landuse(editor, way, args);
			} else if (way.tags.contains("natural")) {
				natural::generate_natural(editor, element, args);
			} else if (way.tags.contains("amenity")) {
				amenities::generate_amenities(editor, element, args);
			} else if (way.tags.contains("leisure")) {
				leisure::generate_leisure(editor, way, args);
			} else if (way.tags.contains("barrier")) {
				barriers::generate_barriers(editor, element);
			} else if (way.tags.contains("waterway")) {
				waterways::generate_waterways(editor, way);
			} else if (way.tags.contains("bridge")) {
				// bridges::generate_bridges(editor, way, ground_level); // TODO FIX
			} else if (way.tags.contains("railway")) {
				railways::generate_railways(editor, way);
			} else if (way.tags.contains("roller_coaster")) {
				railways::generate_roller_coaster(editor, way);
			} else if (way.tags.contains("aeroway") ||
					   way.tags.contains("area:aeroway")) {
				highways::generate_aeroway(editor, way, args);
			} else if (way.tags.get("service") ==
					   std::optional<std::string>(std::string("siding"))) {
				highways::generate_siding(editor, way);
			} else if (way.tags.contains("man_made")) {
				man_made::generate_man_made(editor, element, args);
			}
		} else if (element.is_node()) {
			auto const &node = element.as_node();

			args.ground_level = editor.ground->level(node.xz());
			//DUMP("n", args.ground_level, node.tags);
			if (node.tags.contains("door") || node.tags.contains("entrance")) {
				doors::generate_doors(editor, node);
			} else if (node.tags.contains("natural") &&
					   node.tags.get("natural") ==
							   std::optional<std::string>(std::string("tree"))) {
				natural::generate_natural(editor, element, args);
			} else if (node.tags.contains("amenity")) {
				amenities::generate_amenities(editor, element, args);
			} else if (node.tags.contains("barrier")) {
				barriers::generate_barrier_nodes(editor, node);
			} else if (node.tags.contains("highway")) {
				highways::generate_highways(editor, element, args, elements);
			} else if (node.tags.contains("tourism")) {
				tourisms::generate_tourisms(editor, node);
			} else if (node.tags.contains("man_made")) {
				man_made::generate_man_made_nodes(editor, node);
			}
		} else if (element.is_relation()) {
			auto const &rel = element.as_relation();

			if (!rel.members.empty() && !rel.members.begin()->way.nodes.empty()) {
				//args.ground_level = editor.ground->level(editor.node_to_xz(*rel.members.begin()->way.nodes.begin()));
				args.ground_level = editor.ground->level(
						rel.members.begin()->way.nodes.begin()->xz());
				DUMP(args.ground_level);

				//const auto nl = editor.node_to_xz(n);nodePoints.emplace_back(nl.first, nl.second);	auto maybeBaseY = ground.min_level(nodePoints);
			}
			DUMP("r", args.ground_level, rel.tags);

			if (rel.tags.contains("building") || rel.tags.contains("building:part")) {
				buildings::generate_building_from_relation(editor, rel, args);
			} else if (rel.tags.contains("water") ||
					   rel.tags.get("natural") ==
							   std::optional<std::string>(std::string("water"))) {
				water_areas::generate_water_areas(editor, rel);
			} else if (rel.tags.contains("natural")) {
				natural::generate_natural_from_relation(editor, rel, args);
			} else if (rel.tags.contains("landuse")) {
				landuse::generate_landuse_from_relation(editor, rel, args);
			} else if (rel.tags.get("leisure") ==
					   std::optional<std::string>(std::string("park"))) {
				leisure::generate_leisure_from_relation(editor, rel, args);
			} else if (rel.tags.contains("man_made")) {
				man_made::generate_man_made(editor, ProcessedElement(rel), args);
			}
		}
	}

#if 0
    // Generate ground layer
    auto rect = xzbbox.bounding_rect();
    uint64_t total_blocks = rect.total_blocks();
    uint64_t desired_updates = 1500;
    uint64_t batch_size = std::max<uint64_t>(1, total_blocks / desired_updates);

    uint64_t block_counter = 0;

    std::cout << "[6/7] Generating ground..." << std::endl;
    emit_gui_progress_update(70.0, "Generating ground...");

    double gui_progress_grnd = 70.0;
    double last_emitted_progress = gui_progress_grnd;
    double total_iterations_grnd = static_cast<double>(total_blocks);
    double progress_increment_grnd = 20.0 / total_iterations_grnd;
    int ground_layer = GRASS_BLOCK;

    for (int x = xzbbox.min_x(); x <= xzbbox.max_x(); ++x) {
        for (int z = xzbbox.min_z(); z <= xzbbox.max_z(); ++z) {
            // Add default ground layer if not present
            if (!editor.check_for_block(x, 0, z, nullptr)) {
                editor.set_block(ground_layer, x, 0, z);
                editor.set_block(DIRT, x, -1, z);
                editor.set_block(DIRT, x, -2, z);
            }

            // Fill underground with stone if needed
            if (args.fillground) {
                editor.fill_blocks_absolute(STONE, x, MIN_Y + 1, z, x, editor.get_absolute_y(x, -3, z), z, nullptr, nullptr);
            }

            // Generate bedrock at MIN_Y
            editor.set_block(BEDROCK, x, MIN_Y, z);

            block_counter++;
            if (block_counter % batch_size == 0) {
                // Update progress
            }

            gui_progress_grnd += progress_increment_grnd;
            if (std::abs(gui_progress_grnd - last_emitted_progress) > 0.25) {
                emit_gui_progress_update(gui_progress_grnd, "");
                last_emitted_progress = gui_progress_grnd;
            }
        }
    }

    // Save world
    editor.save();

    // Optional: update spawn point Y coordinate (not implemented here)

    emit_gui_progress_update(100.0, "Done! World generation completed.");
    std::cout << "Done! World generation completed." << std::endl;
#endif
	return true;
}

#if 0

int main() {
    // Example usage with dummy data
    std::vector<ProcessedElement> elements;
    // Populate elements...

    Args args;
    args.path = "world_directory";
    args.debug = false;
    args.fillground = true;

    XZBBox bbox{0, 10, 0, 10};
    Ground ground;

    generate_world(elements, bbox, ground, args);

    return 0;
}
#endif
}
