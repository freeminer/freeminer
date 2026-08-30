#pragma once

#include <cstdint>
#include <optional>
#include <stdexcept>
#include <string>
#include <unordered_map>
#include <variant>
#include <vector>

#include "../../irr_v2d.h"

namespace arnis
{

struct XZPoint;

struct XZ : public v2s32
{
	XZ(int x, int z) : v2s32{x, z} {}
	XZ(const XZ &other) noexcept : v2s32{other.X, other.Y} {}
	XZ(XZ &&other) noexcept : v2s32{other.X, other.Y} {}
	XZ &operator=(const XZ &other) noexcept
	{
		X = other.X;
		Y = other.Y;
		return *this;
	}
	XZ &operator=(XZ &&other) noexcept { return operator=(other); }
	operator XZPoint();
	int &x = X;
	int &z = Y;
};
struct XZPoint : public XZ
{
	XZPoint() noexcept : XZ(0, 0) {}					 //= default;
	XZPoint(const XZPoint &p) noexcept : XZ{p.x, p.z} {} //=  default;
	XZPoint(XZPoint &&p) noexcept : XZ{p.x, p.z} {}		 //= default;
	XZPoint(int x, int z) : XZ{x, z} {}
	XZPoint &operator=(const XZPoint &other) noexcept
	{
		XZ::operator=(other);
		return *this;
	}
	XZPoint &operator=(XZPoint &&other) noexcept
	{
		XZ::operator=(other);
		return *this;
	}
	static XZPoint new_point(int x_, int z_) { return XZPoint(x_, z_); }
};

inline XZ::operator XZPoint()
{
	return XZPoint{X, Y};
}

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
	std::uint64_t id;
	tags_t tags;
	int x;
	int z;
	XZ xz() const { return {x, z}; }
};
struct ProcessedWay
{
	std::uint64_t id;
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
	std::uint64_t id;
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

	ProcessedElement(ProcessedNode const &n) : variant_t(n) {}

	ProcessedElement(ProcessedWay const &w) : variant_t(w) {}

	ProcessedElement(ProcessedRelation const &r) : variant_t(r) {}

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

	std::uint64_t id() const noexcept
	{
		if (is_node()) {
			return std::get<ProcessedNode>(*this).id;
		} else if (is_way()) {
			return std::get<ProcessedWay>(*this).id;
		} else { // relation
			return std::get<ProcessedRelation>(*this).id;
		}
	}

	const tags_t &tags() const
	{
		if (is_node()) {
			return as_node().tags;
		} else if (is_way()) {
			return as_way().tags;
		}
		return as_relation().tags;
	}

	inline static const std::vector<ProcessedNode> dummy_nodes{};
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

	static ProcessedElement FromRelation(const ProcessedRelation &r)
	{
		return ProcessedElement(r);
	}

	const std::string &kind() const noexcept
	{
		static const std::string node_kind = "node";
		static const std::string way_kind = "way";
		static const std::string relation_kind = "relation";
		return is_node() ? node_kind : is_way() ? way_kind : relation_kind;
	}

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

}
