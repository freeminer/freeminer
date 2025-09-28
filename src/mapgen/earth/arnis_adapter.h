
// https://heck.ai/
// write in c++ without explanation and examples, use full namespaces, prefer std::optional instead pointers, do not use static functions :

#pragma once
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

#include "../../debug/dump.h"

#undef stoi
#undef stof

#include "arnis-cpp/src/args.h"
#include "arnis_block.h"

namespace arnis
{

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
		return h;
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
			mg->vm->setNode(pos, block);
			++mg->stat.set;
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
		return !(n.getContent() == CONTENT_AIR || n.getContent() == CONTENT_IGNORE);
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
