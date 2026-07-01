
// https://heck.ai/
// write in c++ without explanation and examples, use full namespaces, prefer std::optional instead pointers, do not use static functions :

#pragma once
#include <algorithm>
#include <cmath>
#include <cstdint>
#include <limits>
#include <optional>
#include <osmium/osm/entity.hpp>
#include <osmium/osm/location.hpp>
#include <osmium/osm/node.hpp>
#include <osmium/osm/relation.hpp>
#include <osmium/osm/way.hpp>
#include <variant>
#include <tuple>
#include <utility>
#include "../../irr_v2d.h"
#include "map.h"
#include "mapgen/mapgen_earth.h"

#include "../../debug/dump.h"

#undef stoi
#undef stof

#include "arnis-cpp/src/args.h"
#include "arnis_block.h"
#include "arnis-cpp/src/land_cover.h"

namespace arnis
{

namespace block_definitions
{
extern Block LIGHT_GRAY_WALL_BANNER;
extern Block WATER;
}

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
	Inner,
	Part
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

	const static std::vector<ProcessedNode> dummy_nodes;
	const std::vector<ProcessedNode> &nodes() const
	{
		if (is_way())
			return as_way().nodes;
		return dummy_nodes;
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
	std::optional<land_cover::LandCoverData> land_cover;
	std::size_t land_cover_world_width = 0;
	std::size_t land_cover_world_height = 0;

	int get_absolute_y(int x_input, int y_offset, int z_input) const
	{
		//if (ground) {
		int relative_x = x_input; //- xzbbox.min_x();
		int relative_z = z_input; //- xzbbox.min_z();
		return level(XZPoint(relative_x, relative_z)) + y_offset;
	}

	// Return the minimum ground level among points
	// Return std::nullopt if no valid data, to match the Rust’s Option
	std::optional<int> min_level(const std::vector<XZPoint> &points) const
	{
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
		++mg->stat.level;
		const auto h = mg->get_height(pos.X, pos.Y);
		if (h < 1) {
			return 1;
		}
		return h;
	}

	bool has_land_cover() const
	{
		return land_cover.has_value() && land_cover->width > 0 && land_cover->height > 0 &&
			   land_cover_world_width > 0 && land_cover_world_height > 0;
	}

	void set_land_cover_data(land_cover::LandCoverData data,
			std::size_t world_width, std::size_t world_height)
	{
		// Rust parity: src/ground.rs land-cover accessors.
		// Divergence: C++ currently receives an OSM-derived grid; ESA COG fetch is not ported.
		data.width = data.grid.empty() ? 0 : data.grid.front().size();
		data.height = data.grid.size();
		data.water_distance =
				land_cover::compute_water_distance(data.grid, data.width, data.height);
		data.refresh_water_blend_grid();
		land_cover = std::move(data);
		land_cover_world_width = world_width;
		land_cover_world_height = world_height;
	}

	std::pair<std::size_t, std::size_t> land_cover_index(const XZPoint &coord) const
	{
		// Rust parity: src/ground.rs::cover_class / water_distance sampling.
		const auto &lc = *land_cover;
		const double x_ratio = std::clamp(static_cast<double>(coord.x) /
						static_cast<double>(std::max<std::size_t>(1, land_cover_world_width - 1)),
				0.0, 1.0);
		const double z_ratio = std::clamp(static_cast<double>(coord.z) /
						static_cast<double>(std::max<std::size_t>(1, land_cover_world_height - 1)),
				0.0, 1.0);
		const auto x = std::min<std::size_t>(
				static_cast<std::size_t>(std::llround(x_ratio * static_cast<double>(lc.width - 1))),
				lc.width - 1);
		const auto z = std::min<std::size_t>(
				static_cast<std::size_t>(std::llround(z_ratio * static_cast<double>(lc.height - 1))),
				lc.height - 1);
		return {x, z};
	}

	uint8_t cover_class(const XZPoint &coord) const
	{
		if (!has_land_cover())
			return 0;
		const auto [x, z] = land_cover_index(coord);
		return land_cover->grid[z][x];
	}

	uint8_t water_distance(const XZPoint &coord) const
	{
		if (!has_land_cover())
			return 0;
		const auto [x, z] = land_cover_index(coord);
		if (z >= land_cover->water_distance.size() ||
				x >= land_cover->water_distance[z].size())
			return 0;
		return land_cover->water_distance[z][x];
	}

	double water_blend(const XZPoint &coord) const
	{
		if (!has_land_cover())
			return 0.0;
		const auto &lc = *land_cover;
		if (lc.water_blend_grid.empty())
			return 0.0;

		const double fx = std::clamp(static_cast<double>(coord.x) /
						static_cast<double>(std::max<std::size_t>(1, land_cover_world_width - 1)),
				0.0, 1.0) *
				static_cast<double>(lc.width - 1);
		const double fz = std::clamp(static_cast<double>(coord.z) /
						static_cast<double>(std::max<std::size_t>(1, land_cover_world_height - 1)),
				0.0, 1.0) *
				static_cast<double>(lc.height - 1);
		const auto x0 = std::min<std::size_t>(static_cast<std::size_t>(std::floor(fx)), lc.width - 1);
		const auto z0 = std::min<std::size_t>(static_cast<std::size_t>(std::floor(fz)), lc.height - 1);
		const auto x1 = std::min<std::size_t>(x0 + 1, lc.width - 1);
		const auto z1 = std::min<std::size_t>(z0 + 1, lc.height - 1);
		const double tx = fx - std::floor(fx);
		const double tz = fz - std::floor(fz);
		const double w00 = lc.water_blend_grid[z0][x0];
		const double w10 = lc.water_blend_grid[z0][x1];
		const double w01 = lc.water_blend_grid[z1][x0];
		const double w11 = lc.water_blend_grid[z1][x1];
		const double top = w00 * (1.0 - tx) + w10 * tx;
		const double bottom = w01 * (1.0 - tx) + w11 * tx;
		return top * (1.0 - tz) + bottom * tz;
	}

	std::optional<std::tuple<int, int, int, int>> lc_water_block_bounds() const
	{
		// Rust parity: src/ground.rs::lc_water_block_bounds.
		// Used by water_depth to avoid scanning the full world bbox.
		if (!has_land_cover())
			return std::nullopt;
		const auto &lc = *land_cover;
		std::size_t gx0 = std::numeric_limits<std::size_t>::max();
		std::size_t gz0 = std::numeric_limits<std::size_t>::max();
		std::size_t gx1 = 0;
		std::size_t gz1 = 0;
		bool any = false;
		for (std::size_t z = 0; z < lc.height; ++z) {
			for (std::size_t x = 0; x < lc.width; ++x) {
				if (lc.grid[z][x] != land_cover::LC_WATER)
					continue;
				gx0 = std::min(gx0, x);
				gx1 = std::max(gx1, x);
				gz0 = std::min(gz0, z);
				gz1 = std::max(gz1, z);
				any = true;
			}
		}
		if (!any)
			return std::nullopt;

		auto span = [](std::size_t g_lo, std::size_t g_hi,
						 std::size_t world_dim, std::size_t grid_dim) {
			if (grid_dim <= 1 || world_dim <= 1)
				return std::pair<int, int>{0, static_cast<int>(world_dim - 1)};
			const double f = static_cast<double>(world_dim - 1) /
					static_cast<double>(grid_dim - 1);
			const int lo = static_cast<int>(std::floor((static_cast<double>(g_lo) - 0.5) * f)) - 1;
			const int hi = static_cast<int>(std::ceil((static_cast<double>(g_hi) + 0.5) * f)) + 1;
			return std::pair<int, int>{
					std::max(0, lo), std::min(static_cast<int>(world_dim - 1), hi)};
		};
		const auto [x0, x1] = span(gx0, gx1, land_cover_world_width, lc.width);
		const auto [z0, z1] = span(gz0, gz1, land_cover_world_height, lc.height);
		return std::tuple<int, int, int, int>{x0, z0, x1, z1};
	}

	int water_level(const XZPoint &coord) const
	{
		return level(coord);
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
		const auto yg = ground->level({x, z});

		return set_block_absolute(block, x, yg + y, z, replace_with, avoid);
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
			bool should_set = true;
			const auto current = mg->vm->getNode(pos);
			const auto content = current.getContent();
			if (maybe_variants) {
				should_set = std::any_of(maybe_variants->begin(), maybe_variants->end(),
						[content](const Block &b) { return b.getContent() == content; });
			} else if (maybe_replacements) {
				should_set = std::none_of(maybe_replacements->begin(), maybe_replacements->end(),
						[content](const Block &b) { return b.getContent() == content; });
			}
			if (should_set) {
				mg->vm->setNode(pos, block);
				++mg->stat.set;
			}
		} else {
			++mg->stat.miss;
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

		++mg->stat.check;

		if (!mg->vm->exists(pos))
			return false;

		const auto n = mg->vm->getNode(pos);
		const auto content = n.getContent();
		if (content == CONTENT_AIR || content == CONTENT_IGNORE)
			return false;
		if (blocks) {
			return std::any_of(blocks->begin(), blocks->end(),
					[content](const Block &b) { return b.getContent() == content; });
		}
		return true;
	}

	//inline auto node_to_xz(const osmium::NodeRef &node)
	inline auto node_to_xz(const auto &node)
	{
		const auto pos2 = mg->ll_to_pos(
				{static_cast<ll_t>(node.y()) /
								static_cast<ll_t>(osmium::detail::coordinate_precision),
						static_cast<ll_t>(node.x()) /
								static_cast<ll_t>(osmium::detail::coordinate_precision)});
		// TODO: scale y
		return std::make_pair(pos2.X, pos2.Y);
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
	int get_ground_level(int x, int z) const { return ground ? ground->level({x, z}) : 0; }
	int get_water_level(int x, int z) const
	{
		return ground ? ground->water_level({x, z}) : get_ground_level(x, z);
	}
	bool is_lc_water(int x, int z) const
	{
		if (ground && ground->has_land_cover()) {
			return ground->cover_class({x - mg->node_min.X, z - mg->node_min.Z}) ==
				   land_cover::LC_WATER;
		}
		if (!mg || !mg->vm)
			return false;
		const v3pos_t pos{
				static_cast<pos_t>(x), static_cast<pos_t>(get_water_level(x, z)),
				static_cast<pos_t>(z)};
		if (!mg->vm->exists(pos))
			return false;
		return mg->vm->getNode(pos).getContent() ==
			   block_definitions::WATER.getContent();
	}
	uint8_t water_distance(int x, int z) const
	{
		if (ground && ground->has_land_cover())
			return ground->water_distance({x - mg->node_min.X, z - mg->node_min.Z});
		return is_lc_water(x, z) ? 0 : 15;
	}

	bool check_for_block_absolute(int x, int y, int z,
			const std::optional<std::vector<Block>> &blocks = {},
			const std::optional<std::vector<Block>> &avoid = {})
	{
		const v3pos_t pos{
				static_cast<pos_t>(x), static_cast<pos_t>(y), static_cast<pos_t>(z)};
		++mg->stat.check;
		if (!mg || !mg->vm || !mg->vm->exists(pos))
			return false;
		const auto n = mg->vm->getNode(pos);
		const auto content = n.getContent();
		if (content == CONTENT_AIR || content == CONTENT_IGNORE)
			return false;
		if (blocks) {
			return std::any_of(blocks->begin(), blocks->end(),
					[content](const Block &b) { return b.getContent() == content; });
		}
		if (avoid) {
			return std::any_of(avoid->begin(), avoid->end(),
					[content](const Block &b) { return b.getContent() == content; });
		}
		return true;
	}

	bool block_exists_absolute(int x, int y, int z)
	{
		return check_for_block_absolute(x, y, z);
	}

	void set_block_if_absent_absolute(const Block &block, int x, int y, int z)
	{
		if (!check_for_block_absolute(x, y, z))
			set_block_absolute(block, x, y, z);
	}

	void fill_column_absolute(const Block &block, int x, int z, int min_y, int max_y,
			bool skip_existing)
	{
		if (max_y < min_y)
			return;
		for (int y = min_y; y <= max_y; ++y) {
			if (skip_existing && check_for_block_absolute(x, y, z))
				continue;
			set_block_absolute(block, x, y, z);
		}
	}

	void place_wall_banner(int x, int y, int z, const std::string &facing,
			const std::vector<std::pair<std::string, std::string>> &patterns)
	{
		(void)facing;
		(void)patterns;
		set_block_absolute(block_definitions::LIGHT_GRAY_WALL_BANNER, x, y, z);
	}

	void fill_blocks(const Block &block, std::int32_t x1, std::int32_t y1,
			std::int32_t z1, std::int32_t x2, std::int32_t y2, std::int32_t z2,
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
		++mg->stat.fill;
	}

	void fill_blocks(const Block &block, std::int32_t x1, std::int32_t y1,
			std::int32_t z1, std::int32_t x2, std::int32_t y2, std::int32_t z2,
			const std::optional<std::vector<Block>> &override_whitelist,
			const std::optional<int> override_blacklist)
	{
		return fill_blocks(block, x1, y1, z1, x2, y2, z2, override_whitelist,
				std::optional<std::vector<Block>>{});
	}

	void fill_blocks(const Block &block, std::int32_t x1, std::int32_t y1,
			std::int32_t z1, std::int32_t x2, std::int32_t y2, std::int32_t z2,
			const std::optional<std::vector<Block>> &override_whitelist, std::nullopt_t)
	{
		return fill_blocks(block, x1, y1, z1, x2, y2, z2, override_whitelist,
				std::optional<std::vector<Block>>{});
	}

	void fill_blocks(const Block &block, std::int32_t x1, std::int32_t y1,
			std::int32_t z1, std::int32_t x2, std::int32_t y2, std::int32_t z2,
			std::nullopt_t, std::nullopt_t)
	{
		return fill_blocks(block, x1, y1, z1, x2, y2, z2,
				std::optional<std::vector<Block>>{}, std::optional<std::vector<Block>>{});
	}

	void fill_blocks(const Block &block, std::int32_t x1, std::int32_t y1,
			std::int32_t z1, std::int32_t x2, std::int32_t y2, std::int32_t z2)
	{
		return fill_blocks(block, x1, y1, z1, x2, y2, z2,
				std::optional<std::vector<Block>>{}, std::optional<std::vector<Block>>{});
	}
};

}

using namespace world_editor;
}

#include "arnis-cpp/src/block_definitions.h"

namespace arnis
{
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
using ProcessedWay = ProcessedWay;
using Way = ProcessedWay;
}
using Node = ProcessedNode;
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

#include "arnis-cpp/src/bresenham.h"
