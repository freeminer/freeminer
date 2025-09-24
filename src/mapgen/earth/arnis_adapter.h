

// https://heck.ai/
// write in c++ without explanation and examples, use full namespaces, prefer std::optional instead pointers, do not use static functions :

#pragma once
//using XZPoint = v2pos_t;
#include <cstdint>
#include <optional>
#include <osmium/osm/entity.hpp>
#include <osmium/osm/location.hpp>
#include <osmium/osm/node.hpp>
#include <osmium/osm/relation.hpp>
#include <osmium/osm/way.hpp>
#include <variant>
#include "../../irr_v2d.h"
#include "map.h"
#include "mapgen/mapgen_earth.h"
#include "emerge.h"

#include "../../debug/dump.h"

#undef stoi
#undef stof

class Block : public MapNode
{
public:
	Block(content_t c = {}) : MapNode{c} {}

	content_t id() const { return getContent(); }
};
class BlockWithProperties
{
public:
	Block block;
	static BlockWithProperties simple(Block b)
	{
		return BlockWithProperties{
				b /*, StairFacing::North, StairShape::Straight, false*/};
	}
};
struct XZPoint;

struct XZ : public v2s32
{
	XZ(int x, int y) : v2s32{x, y} {}
	operator XZPoint();
	int &x = X;
	int &z = Y;
};
struct XZPoint : public XZ
{
	XZPoint() noexcept : XZ(0, 0) {}					 //= default;
	XZPoint(const XZPoint &p) noexcept : XZ{p.x, p.z} {} //=  default;
	XZPoint(XZPoint &&p) noexcept : XZ{p.x, p.z} {}		 //= default;
	XZPoint(int x, int y) : XZ{x, y} {}
	static XZPoint new_point(int x_, int z_) { return XZPoint(x_, z_); }
};

struct tags_t : public std::unordered_map<std::string, std::string>
{
	std::string get(const std::string &k) const
	{
		if (const auto it = find(k); it != end())
			return it->second;
		return {};
	}
};

struct ProcessedNode
{
	std::int64_t id;
	tags_t tags;
	int x;
	int z;
	XZ xz() const { return {x, z}; }
};
struct ProcessedWay
{
	std::int64_t id;
	std::vector<ProcessedNode> nodes;
	tags_t tags;
};

enum class ProcessedMemberRole
{
	Outer,
	Inner
};
struct ProcessedMember
{
	ProcessedWay way;
	ProcessedMemberRole role;
};

struct ProcessedRelation
{
	std::int64_t id;
	tags_t tags;
	std::vector<ProcessedMember> members;
};

using variant_t = std::variant<ProcessedNode, ProcessedWay, ProcessedRelation>;

//enum class ElementType { Node, Way };
enum class ElementType : uint8_t
{
	Node,
	Way,
	Relation
};

class ProcessedElement : public variant_t
{
public:
	using Type = ElementType;
	Type type;

	ProcessedElement(ProcessedNode const &n) :
			variant_t(n), type{Type::Node}, kind_("node")
	{
		node = as_node();
	}

	ProcessedElement(ProcessedWay const &w) : variant_t(w), type{Type::Way}, kind_("way")
	{
		way = as_way();
	}

	ProcessedElement(ProcessedRelation const &r) :
			variant_t(r), type{Type::Relation}, kind_("relation")
	{
	}

	bool is_node() const noexcept { return std::holds_alternative<ProcessedNode>(*this); }

	bool is_way() const noexcept { return std::holds_alternative<ProcessedWay>(*this); }

	bool is_relation() const noexcept
	{
		return std::holds_alternative<ProcessedRelation>(*this);
	}

	ProcessedNode const &as_node() const
	{
		if (!is_node()) {
			throw std::runtime_error("ProcessedElement: not a Node");
		}
		return std::get<ProcessedNode>(*this);
	}

	ProcessedWay const &as_way() const
	{
		if (!is_way()) {
			throw std::runtime_error("ProcessedElement: not a Way");
		}
		return std::get<ProcessedWay>(*this);
	}

	ProcessedRelation const &as_relation() const
	{
		if (!is_relation()) {
			throw std::runtime_error("ProcessedElement: not a Relation");
		}
		return std::get<ProcessedRelation>(*this);
	}

	std::int64_t id() const noexcept
	{
		if (is_node()) {
			return std::get<ProcessedNode>(*this).id;
		} else if (is_way()) {
			return std::get<ProcessedWay>(*this).id;
		} else { // relation
			return std::get<ProcessedRelation>(*this).id;
		}
	}

	const std::unordered_map<std::string, std::string> &tags() const
	{
		if (is_node()) {
			return as_node().tags;
		} else {
			return as_way().tags;
		}
	}

	const std::vector<ProcessedNode> &nodes() const
	{
		if (is_way())
			return as_way().nodes;
	}

	static ProcessedElement FromNode(const ProcessedNode &n)
	{
		ProcessedElement e(n);
		return e;
	}

	static ProcessedElement FromWay(const ProcessedWay &w)
	{
		ProcessedElement e(w);
		return e;
	}

	std::string const &kind() const noexcept { return kind_; }
	std::string kind_;

	std::optional<ProcessedNode> node;
	std::optional<ProcessedWay> way;

	std::optional<std::string> tag(const std::string &key) const
	{
		auto it = tags().find(key);
		if (it != tags().end()) {
			return std::optional<std::string>(it->second);
		}
		return std::optional<std::string>();
	}

	std::optional<ProcessedNode> first_node() const
	{
		if (is_node())
			return as_node();
		if (is_way() && !as_way().nodes.empty()) {
			return std::optional<ProcessedNode>(as_way().nodes.front());
		}
		return std::optional<ProcessedNode>();
	}

	const std::vector<ProcessedNode> &nodes_vec() const
	{
		static const std::vector<ProcessedNode> empty_vec{};
		if (is_way()) {
			return as_way().nodes;
		}
		return empty_vec;
	}
};

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

namespace world_editor
{
// A “WorldEditor” that can set blocks in your map
struct WorldEditor
{
	MapgenEarth *mg{};
	Ground *ground{};
	Ground *get_ground() const { return ground; }; // may return nullptr

	bool pos_ok(const v2pos_t &pos)
	{
		return (pos.X >= mg->node_min.X && pos.X < mg->node_max.X &&
				pos.Y >= mg->node_min.Z && pos.Y < mg->node_max.Z);
	};
	// Place a block at (x, y, z). The optional adjacency arguments
	// mimic the Rust code’s “Some(&[COBBLESTONE, COBBLESTONE_WALL])” idea.
	void set_block(const Block &block, int x, int y, int z,
			const std::optional<std::vector<Block>> &replace_with = {},
			const std::optional<std::vector<Block>> &avoid = {})
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
				static_cast<pos_t>(x), static_cast<pos_t>(yg + y), static_cast<pos_t>(z)};
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

	void set_block(const Block &block, int x, int y, int z,
			const std::optional<std::vector<Block>> &replace_with, std::nullopt_t)
	{
		return set_block(block, x, y, z, replace_with);
	}

	void set_block(const Block &block, int x, int y, int z,
			const std::optional<std::vector<Block>> &replace_with, std::optional<int>)
	{
		return set_block(block, x, y, z, replace_with);
	}

	void set_block(const Block &b, int x, int y, int z,
			const std::optional<std::vector<const Block *>> &alt, std::nullopt_t)
	{
		return set_block(b, x, y, z);
	}

	void set_block(const Block &block, int x, int y, int z, std::optional<int>,
			std::optional<int>)
	{
		return set_block(block, x, y, z);
	}

	void set_block(
			const Block &block, int x, int y, int z, std::nullopt_t, std::nullopt_t)
	{
		return set_block(block, x, y, z);
	}

	void set_block_absolute(const Block &block, int x, int y, int z,
			std::optional<const std::vector<Block>> maybe_variants = {},
			std::optional<const std::vector<Block>> maybe_replacements = {})
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

	void set_block_absolute(const Block &block, int x, int y, int z, void *, void *)
	{
		return set_block_absolute(block, x, y, z);
	}

	void set_block_with_properties_absolute(
			BlockWithProperties bwp, int32_t x, int32_t y, int32_t z, void *a, void *b)
	{
		set_block_absolute(bwp.block, x, y, z);
	}
	bool check_for_block(
			int x, int y, int z, const std::optional<std::vector<Block>> &blocks)
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

	std::pair<int, int> get_min_coords() const
	{
		return std::make_pair(mg->node_min.X, mg->node_min.Z);
	};

	std::pair<int, int> get_max_coords() const
	{
		return std::make_pair(mg->node_max.X, mg->node_max.Z);
	};
	int get_absolute_y(int x, int y, int z) { return ground->get_absolute_y(x, y, z); }

	void fill_blocks(const Block& block, std::int32_t x1, std::int32_t y1, std::int32_t z1,
			std::int32_t x2, std::int32_t y2, std::int32_t z2,
			const std::optional<std::vector<Block>> &override_whitelist,
			const std::optional<std::vector<Block>> &override_blacklist)
	{
		auto [min_x, max_x] = std::minmax(x1, x2);
		auto [min_y, max_y] = std::minmax(y1, y2);
		auto [min_z, max_z] = std::minmax(z1, z2);

		for (std::int32_t x = min_x; x <= max_x; ++x) {
			for (std::int32_t y = min_y; y <= max_y; ++y) {
				for (std::int32_t z = min_z; z <= max_z; ++z) {
					this->set_block(
							block, x, y, z, override_whitelist, override_blacklist);
				}
			}
		}
	}

	
	void fill_blocks(const Block& block, std::int32_t x1, std::int32_t y1, std::int32_t z1,
			std::int32_t x2, std::int32_t y2, std::int32_t z2,
			const std::optional<std::vector<Block>> &override_whitelist,
			const std::optional<int> override_blacklist)
	{
		return fill_blocks(block, x1, y1, z1, x2, y2, z2, override_whitelist,
				std::optional<std::vector<Block>>{});
	}

	void fill_blocks(const Block& block, std::int32_t x1, std::int32_t y1, std::int32_t z1,
			std::int32_t x2, std::int32_t y2, std::int32_t z2,
			const std::optional<std::vector<Block>> &override_whitelist,
			std::nullopt_t)
	{
		return fill_blocks(block, x1, y1, z1, x2, y2, z2, override_whitelist,
				std::optional<std::vector<Block>>{});
	}
				
	
	void fill_blocks(const Block& block, std::int32_t x1, std::int32_t y1, std::int32_t z1,
			std::int32_t x2, std::int32_t y2, std::int32_t z2, std::nullopt_t,
			std::nullopt_t)
	{
		return fill_blocks(block, x1, y1, z1, x2, y2, z2,
				std::optional<std::vector<Block>>{}, std::optional<std::vector<Block>>{});
	}

	void fill_blocks(const Block& block, std::int32_t x1, std::int32_t y1, std::int32_t z1,
			std::int32_t x2, std::int32_t y2, std::int32_t z2)
	{
		return fill_blocks(block, x1, y1, z1, x2, y2, z2,
				std::optional<std::vector<Block>>{}, std::optional<std::vector<Block>>{});
	}
};

}
using namespace world_editor;

struct Args
{
	// Bounding box of the area (min_lat,min_lng,max_lat,max_lng) (required)
	//LLBBox bbox{};

	// JSON file containing OSM data (optional)
	std::optional<std::string> file{std::nullopt};

	// JSON file to save OSM data to (optional)
	std::optional<std::string> save_json_file{std::nullopt};

	// Path to the Minecraft world (required)
	std::string path{};

	// Downloader method (requests/curl/wget) (optional)
	std::string downloader{std::string("requests")};

	// World scale to use, in blocks per meter
	double scale{1.0};

	// Ground level to use in the Minecraft world
	int ground_level{-62};

	// Enable terrain (optional)
	bool terrain{false};

	// Enable interior generation (optional)
	bool interior{true};

	// Enable roof generation (optional)
	bool roof{true};

	// Enable filling ground (optional)
	bool fillground{false};

	// Enable debug mode (optional)
	bool debug{false};

	// Set floodfill timeout (seconds) (optional)
	//std::optional<std::chrono::duration<double>> timeout{std::nullopt};
	const std::chrono::milliseconds timeout{200};
	std::chrono::milliseconds timeout_ref() const { return timeout; }

	// Spawn point coordinates (lat, lng)
	std::optional<std::pair<double, double>> spawn_point{std::nullopt};
};

namespace arnis
{

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

Block get_castle_wall_block();

namespace args
{
using Args = Args;
}
namespace world_editor
{
using WorldEditor = WorldEditor;
}
namespace osm_parser
{
using ElementType = ElementType;
using ProcessedElement = ProcessedElement;
using ProcessedNode = ProcessedNode;
//using Node = ProcessedNode;
using ProcessedWay = ProcessedWay;
using Way = ProcessedWay;
}
namespace coordinate_system
{
namespace cartesian
{
using XZPoint = XZPoint;
}
}
namespace block_definitions
{
using Block = Block;
using namespace arnis::block_definitions;
}

}
namespace crate = arnis;

#include "arnis-cpp/src/floodfill.h"
#include "arnis-cpp/src/colors.h"
#include "arnis-cpp/src/block_definitions.h"
#include "arnis-cpp/src/bresenham.h"
#include "arnis-cpp/src/element_processing/subprocessor/buildings_interior.h"
