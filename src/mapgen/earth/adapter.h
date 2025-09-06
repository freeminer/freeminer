#pragma once
//using XZPoint = v2pos_t;
#include <osmium/osm/entity.hpp>
#include <osmium/osm/location.hpp>
#include <osmium/osm/node.hpp>
#include <osmium/osm/relation.hpp>
#include <osmium/osm/way.hpp>
#include "irr_v2d.h"
#include "map.h"
#include "mapgen/mapgen_earth.h"

using XZPoint = v2s32;
using Block = MapNode;

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
using ProcessedElement = osmium::OSMObject;

/*
class ProcessedElement : public osmium::OSMObject
{
	public:
	std::optional<std::string> get_value_by_key(
			const char *key, const char *default_value = nullptr) const noexcept
	{
		const auto *v = tags().get_value_by_key(key, default_value);
		if (!v)
			return {};
		return std::string{v};
	}
};
*/

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
	MapgenEarth *mg = nullptr;

	int get_absolute_y(int x_input, int y_offset, int z_input) const
	{
		//if (ground) {
		int relative_x = x_input; //- xzbbox.min_x();
		int relative_z = z_input; //- xzbbox.min_z();
		return level(XZPoint(relative_x, relative_z)) + y_offset;
		/*
		} else {
            return y_offset; // If no ground reference, use y_offset as absolute Y
        }
			*/
	}

	// Return the minimum ground level among points
	// Return std::nullopt if no valid data, to match the Rust’s Option
	std::optional<int> min_level(const std::vector<XZPoint> &points) const
	{
		//DUMP(points.size());
		if (points.empty()) {
			return std::nullopt;
		}
		// Example logic: just pick a fixed level or do some real logic
		int minY = 9999999;
		for (auto &pt : points) {
			// In a real implementation, you'd check your terrain data
			int y = level(pt);
			if (y < minY) {
				minY = y;
			}
		}
		return minY == 9999999 ? std::nullopt : std::optional<int>(minY);
	}

	// Return ground level for a single XZ point
	int level(const XZPoint &pos) const
	{
		// Example stub (always 64). Real code might do something more sophisticated.

		// TODO use const from osmium
		//constexpr double osmium_scale = 10000000;

		//DUMP(osmium_scale, x / osmium_scale - cpos.X, z / osmium_scale - cpos.Y);
		//DUMP(osmium_scale, cpos.X - x / osmium_scale , cpos.Y - z / osmium_scale );
		//const ll oll(x / osmium_scale, z / osmium_scale);
		//const ll oll(z / osmium_scale, x / osmium_scale);
		/*
		const ll oll(pos.Y / osmium::detail::coordinate_precision,
				pos.X / osmium::detail::coordinate_precision);

		const auto pos2 = mg->ll_to_pos(oll); // {pos.X, pos.Y}
		// TODO: scale y
		//const v3pos_t lpos{pos2.X, 0, pos2.Y};

		const auto h = mg->get_height(pos2.X, pos2.Y);
*/
		const auto h = mg->get_height(pos.X, pos.Y);
		//DUMP(pos, pos2, h);
		return h;
		//return 64;
	}
};

// A “WorldEditor” that can set blocks in your map
struct WorldEditor
{
	MapgenEarth *mg{};
	Ground *ground{};
	bool pos_ok(const v2pos_t &pos)
	{
		return (pos.X >= mg->node_min.X && pos.X < mg->node_max.X &&
				pos.Y >= mg->node_min.Z && pos.Y < mg->node_max.Z);
	};
	// Place a block at (x, y, z). The optional adjacency arguments
	// mimic the Rust code’s “Some(&[COBBLESTONE, COBBLESTONE_WALL])” idea.
	void set_block(const Block &block, int x, int y, int z,
			std::optional<const std::vector<Block> *> maybe_variants = {},
			std::optional<const std::vector<Block> *> maybe_replacements = {})
	{
		// Implementation for adding a block to the world
		/*
		(void)block;
		(void)x;
		(void)y;
		(void)z;
		(void)maybe_variants;
		(void)maybe_replacements;
*/
		//Block to_set = block;
		//if (maybe_variants.has_value() && !maybe_variants.value()->empty())
		//to_set = maybe_variants.value()->at(0);
		//DUMP(x, y, z, block);
		//DUMP(x-(mg->center.X+mg->node_min.X), y-(mg->center.Y+mg->node_min.Y), z - (mg->center.Z + mg->node_min.Z), block);

		//auto cpos = mg->ll_to_pos({mg->center.X, mg->center.Z});
		//auto cpos = mg->ll_to_pos_absolute({});
		//DUMP(mg->node_min, mg->center, cpos);

		// TODO use const from osmium
		//constexpr double osmium_scale = 10000000;

		//DUMP(osmium_scale, x / osmium_scale - cpos.X, z / osmium_scale - cpos.Y);
		//DUMP(osmium_scale, cpos.X - x / osmium_scale , cpos.Y - z / osmium_scale );
		//const ll oll(x / osmium_scale, z / osmium_scale);

		/*
		const ll oll(z / osmium::detail::coordinate_precision, x / osmium::detail::coordinate_precision);
		const auto pos2 = mg->ll_to_pos(oll);
		// TODO: scale y
		const v3pos_t pos{pos2.X, static_cast<pos_t>(y), pos2.Y};
*/

		const auto yg = ground->level({x, z});

		const v3pos_t pos{
				static_cast<pos_t>(x), static_cast<pos_t>(yg), static_cast<pos_t>(z)};
		//const v3pos_t chunk_pos = abs_pos - mg->node_min;
		//const auto chunk_pos = abs_pos;

		//if (!pos_ok(chunk_pos))return;

		//if (static auto i = 0; !(++i % 10000))DUMP(i, mg->node_min, mg->node_max, cpos, chunk_pos,oll, x,y,z); //.lat, oll.lon);

		//if (!pos_ok(cpos)) return;
		//DUMP(mg->node_min, mg->node_max, cpos, abs_pos, chunk_pos, oll); //.lat, oll.lon);
		if (!mg || !mg->vm) {
			DUMP("broken mg");
			return;
		}

		if (mg->vm->exists(pos)) {
			mg->vm->setNode(
					//{static_cast<pos_t>(x), static_cast<pos_t>(y), static_cast<pos_t>(z)},
					//{cpos.X, y, cpos.Y},
					//{cpos.X, static_cast<pos_t>(y), cpos.Y},
					//{cpos.X-mg->node_min.X, static_cast<pos_t>(y)-mg->node_min.Y, cpos.Y-mg->node_min.Z},
					pos, block);
		}
	}
	void set_block_absolute(const Block &block, int x, int y, int z,
			std::optional<const std::vector<Block> *> maybe_variants = {},
			std::optional<const std::vector<Block> *> maybe_replacements = {})
	{

		const v3pos_t pos{
				static_cast<pos_t>(x), static_cast<pos_t>(y), static_cast<pos_t>(z)};

		if (!mg || !mg->vm) {
			DUMP("broken mg");
			return;
		}
		if (mg->vm->exists(pos)) {
			mg->vm->setNode(pos, block);
		}
	}
	bool check_for_block(int x, int y, int z, const std::vector<Block> &blocks)
	{
		const v3pos_t pos{
				static_cast<pos_t>(x), static_cast<pos_t>(y), static_cast<pos_t>(z)};

		if (!mg->vm->exists(pos))
			return false;

		const auto n = mg->vm->getNode(pos);
		return !(n.getContent() == CONTENT_AIR || n.getContent() == CONTENT_IGNORE);
	}

	//inline auto node_to_xz(const osmium::NodeRef &node)
	inline auto node_to_xz(const auto &node)
	{
		//const auto &x = node.x();
		//const auto &z = node.y();

		//constexpr double osmium_scale = 10000000;

		//DUMP(osmium_scale, x / osmium_scale - cpos.X, z / osmium_scale - cpos.Y);
		//DUMP(osmium_scale, cpos.X - x / osmium_scale , cpos.Y - z / osmium_scale );
		//const ll oll(x / osmium_scale, z / osmium_scale);
		//const ll oll(z / osmium_scale, x / osmium_scale);
		//const auto pos2 = arnis::mg->ll_to_pos(oll);
		const auto pos2 = mg->ll_to_pos({static_cast<ll_t>(node.y()) /
												 osmium::detail::coordinate_precision,
				static_cast<ll_t>(node.x()) / osmium::detail::coordinate_precision});
		// TODO: scale y
		//const v3pos_t pos{pos2.X, static_cast<pos_t>(y), pos2.Y};

		return std::make_pair(pos2.X, pos2.Y);
		//return {pos2.X, pos2.Y};
	}
};

// Example “Args” type
struct Args
{
	double scale = 1;
	bool winter = false;
	// You can store your timeout as an optional or direct type
	std::optional<std::chrono::milliseconds> timeout{200};
};

namespace arnis
{

// Example constants to match the Rust block references
// You’ll need to define these properly in your code.
extern Block AIR;
extern Block BLACK_CONCRETE;
extern Block BRICK;
extern Block COBBLESTONE_WALL;
extern Block COBBLESTONE;
extern Block DIRT;
extern Block GLOWSTONE;
extern Block GRASS_BLOCK;
extern Block GRAY_CONCRETE;
extern Block GREEN_WOOL;
extern Block LIGHT_GRAY_CONCRETE;
extern Block OAK_FENCE_GATE;
extern Block OAK_FENCE;
extern Block OAK_PLANKS;
extern Block RED_WOOL;
extern Block SAND;
extern Block SMOOTH_STONE;
extern Block SNOW_LAYER;
extern Block STONE_BLOCK_SLAB;
extern Block STONE_BRICKS;
extern Block STONE;
extern Block WHITE_CONCRETE;
extern Block WHITE_STAINED_GLASS;
extern Block WHITE_WOOL;
extern Block YELLOW_WOOL;
extern Block STONE_BRICK_SLAB;
/*
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
*/

//extern MapgenEarth *mg;
void init(MapgenEarth *mg);

}

#include "floodfill.cpp"
