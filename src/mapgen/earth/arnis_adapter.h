
// https://heck.ai/
// write in c++ without explanation and examples, use full namespaces, prefer std::optional instead pointers, do not use static functions :

#pragma once
#include <algorithm>
#include <array>
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
#include <filesystem>
#include <functional>
#include "../../irr_v2d.h"
#include "map.h"
#include "mapgen/mapgen_earth.h"

#include "../../debug/dump.h"

#undef stoi
#undef stof

#include "arnis-cpp/src/args.h"
#include "arnis_block.h"
#include "arnis-cpp/src/canopy/canopy.h"
#include "arnis-cpp/src/biome.h"
#include "arnis-cpp/src/urban_ground.h"
#include "arnis-cpp/src/land_cover/land_cover.h"
#include "arnis-cpp/src/decals/registry.h"

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
	struct RotationMask
	{
		double cx = 0, cz = 0, neg_sin = 0, cos = 1;
		int orig_min_x = 0, orig_max_x = 0, orig_min_z = 0, orig_max_z = 0;
	};
	MapgenEarth *mg = nullptr;
	std::optional<land_cover::LandCoverData> land_cover;
	std::size_t land_cover_world_width = 0;
	std::size_t land_cover_world_height = 0;
	std::optional<canopy::CanopyData> canopy_data;
	std::size_t canopy_world_width = 0, canopy_world_height = 0;
	double elevation_min_height_m = 0.0, elevation_blocks_per_meter = 0.0;
	std::optional<int> elevation_ground_level;
	int snow_threshold_y = std::numeric_limits<int>::max();
	std::optional<RotationMask> rotation_mask;
	biome::Climate climate_state = biome::Climate::Temperate;
	UrbanGroundLookup urban_lookup;

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

	// Rust's Ground exposes both aggregate queries.  Keep an empty input as
	// None; callers use that distinction when a feature has no geometry.
	std::optional<int> max_level(const std::vector<XZPoint> &points) const
	{
		if (points.empty())
			return std::nullopt;
		int maxY = std::numeric_limits<int>::min();
		for (const auto &pt : points)
			maxY = std::max(maxY, level(pt));
		return maxY;
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
		return land_cover.has_value() && land_cover->width > 0 &&
			   land_cover->height > 0 && land_cover_world_width > 0 &&
			   land_cover_world_height > 0;
	}
	bool has_canopy() const
	{
		return canopy_data.has_value() && canopy_world_width > 0 &&
			   canopy_world_height > 0;
	}
	// Names mirror ground.rs so library consumers do not need to know the
	// mapgen-host field layout.
	int snow_threshold() const { return snow_threshold_y; }
	int base_level(int fallback = -62) const
	{
		return elevation_ground_level.value_or(fallback);
	}
	std::pair<std::size_t, std::size_t> world_dims() const
	{
		return has_land_cover()
					   ? std::pair<std::size_t, std::size_t>{land_cover_world_width,
								 land_cover_world_height}
					   : std::pair<std::size_t, std::size_t>{
								 canopy_world_width, canopy_world_height};
	}
	void set_canopy_data(
			canopy::CanopyData data, std::size_t world_width, std::size_t world_height)
	{
		canopy_data = std::move(data);
		canopy_world_width = world_width;
		canopy_world_height = world_height;
	}
	std::pair<std::size_t, std::size_t> canopy_index(const XZPoint &coord) const
	{
		const auto &c = *canopy_data;
		const double xr = std::clamp(double(coord.x) / double(std::max<std::size_t>(1,
															   canopy_world_width - 1)),
				0.0, 1.0);
		const double zr = std::clamp(double(coord.z) / double(std::max<std::size_t>(1,
															   canopy_world_height - 1)),
				0.0, 1.0);
		return {std::min<std::size_t>(
						std::llround(xr * double(c.width - 1)), c.width - 1),
				std::min<std::size_t>(
						std::llround(zr * double(c.height - 1)), c.height - 1)};
	}
	std::optional<std::uint8_t> canopy_height_m(const XZPoint &coord) const
	{
		if (!has_canopy())
			return std::nullopt;
		const auto [x, z] = canopy_index(coord);
		return canopy_data->canopy_height_m(x, z);
	}
	std::optional<double> canopy_fraction(const XZPoint &coord, int spacing) const
	{
		// `spacing` is in world blocks.  Sampling world coordinates first keeps
		// the result identical whether the canopy raster is coarser or finer
		// than terrain, and excludes no-data columns from the denominator.
		if (!has_canopy() || spacing <= 0)
			return std::nullopt;
		std::uint32_t measured = 0, wooded = 0;
		for (int dz = 0; dz < spacing; ++dz)
			for (int dx = 0; dx < spacing; ++dx)
				if (const auto height = canopy_height_m({coord.x + dx, coord.z + dz})) {
					++measured;
					if (*height >= canopy::CANOPY_MIN_M)
						++wooded;
				}
		return measured ? std::optional<double>(double(wooded) / double(measured))
						: std::nullopt;
	}
	bool snow_capped(int y) const { return y >= snow_threshold_y; }
	void set_elevation_metadata(
			double min_height_m, double blocks_per_meter, int snow_y, int ground_level)
	{
		elevation_min_height_m = min_height_m;
		elevation_blocks_per_meter = blocks_per_meter;
		snow_threshold_y = snow_y;
		elevation_ground_level = ground_level;
	}
	void set_rotation_mask(RotationMask mask) { rotation_mask = mask; }
	bool inside_rotation_mask(int x, int z) const
	{
		if (!rotation_mask)
			return true;
		const auto &m = *rotation_mask;
		const double dx = x - m.cx, dz = z - m.cz;
		const double ox = dx * m.cos + dz * m.neg_sin + m.cx,
					 oz = -dx * m.neg_sin + dz * m.cos + m.cz;
		constexpr double eps = 1e-9;
		return ox >= m.orig_min_x - eps && ox <= m.orig_max_x + eps &&
			   oz >= m.orig_min_z - eps && oz <= m.orig_max_z + eps;
	}
	bool is_in_rotated_bounds(int x, int z) const { return inside_rotation_mask(x, z); }
	biome::Climate climate() const { return climate_state; }
	void set_climate(biome::Climate value) { climate_state = value; }
	void set_urban_lookup(UrbanGroundLookup lookup) { urban_lookup = std::move(lookup); }
	bool is_urban(int x, int z) const { return urban_lookup.is_urban(x, z); }

	void set_land_cover_data(land_cover::LandCoverData data, std::size_t world_width,
			std::size_t world_height)
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
		const double x_ratio = std::clamp(
				static_cast<double>(coord.x) / static_cast<double>(std::max<std::size_t>(
													   1, land_cover_world_width - 1)),
				0.0, 1.0);
		const double z_ratio = std::clamp(
				static_cast<double>(coord.z) / static_cast<double>(std::max<std::size_t>(
													   1, land_cover_world_height - 1)),
				0.0, 1.0);
		const auto x = std::min<std::size_t>(
				static_cast<std::size_t>(
						std::llround(x_ratio * static_cast<double>(lc.width - 1))),
				lc.width - 1);
		const auto z = std::min<std::size_t>(
				static_cast<std::size_t>(
						std::llround(z_ratio * static_cast<double>(lc.height - 1))),
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
											 static_cast<double>(std::max<std::size_t>(
													 1, land_cover_world_width - 1)),
								  0.0, 1.0) *
						  static_cast<double>(lc.width - 1);
		const double fz = std::clamp(static_cast<double>(coord.z) /
											 static_cast<double>(std::max<std::size_t>(
													 1, land_cover_world_height - 1)),
								  0.0, 1.0) *
						  static_cast<double>(lc.height - 1);
		const auto x0 = std::min<std::size_t>(
				static_cast<std::size_t>(std::floor(fx)), lc.width - 1);
		const auto z0 = std::min<std::size_t>(
				static_cast<std::size_t>(std::floor(fz)), lc.height - 1);
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

		auto span = [](std::size_t g_lo, std::size_t g_hi, std::size_t world_dim,
							std::size_t grid_dim) {
			if (grid_dim <= 1 || world_dim <= 1)
				return std::pair<int, int>{0, static_cast<int>(world_dim - 1)};
			const double f = static_cast<double>(world_dim - 1) /
							 static_cast<double>(grid_dim - 1);
			const int lo =
					static_cast<int>(std::floor((static_cast<double>(g_lo) - 0.5) * f)) -
					1;
			const int hi =
					static_cast<int>(std::ceil((static_cast<double>(g_hi) + 0.5) * f)) +
					1;
			return std::pair<int, int>{
					std::max(0, lo), std::min(static_cast<int>(world_dim - 1), hi)};
		};
		const auto [x0, x1] = span(gx0, gx1, land_cover_world_width, lc.width);
		const auto [z0, z1] = span(gz0, gz1, land_cover_world_height, lc.height);
		return std::tuple<int, int, int, int>{x0, z0, x1, z1};
	}

	// On gentle ground water follows the interpolated terrain.  Around small
	// DEM/land-cover alignment errors, Rust snaps a steep shoreline to the
	// local minimum; it deliberately does not cross a real cliff or waterfall.
	int slope(const XZPoint &coord) const
	{
		constexpr int step = 4;
		const int east = level({coord.x + step, coord.z}),
				  west = level({coord.x - step, coord.z}),
				  north = level({coord.x, coord.z - step}),
				  south = level({coord.x, coord.z + step});
		return std::max({east, west, north, south}) -
			   std::min({east, west, north, south});
	}
	int water_level(const XZPoint &coord) const
	{
		const int center = level(coord);
		if (slope(coord) <= 2)
			return center;
		constexpr int radius = 3;
		int lowest = center;
		for (int r = 1; r <= radius; ++r)
			for (const auto &[dx, dz] : std::array<std::pair<int, int>, 8>{{{-r, 0},
						 {r, 0}, {0, -r}, {0, r}, {-r, -r}, {-r, r}, {r, -r}, {r, r}}})
				lowest = std::min(lowest, level({coord.x + dx, coord.z + dz}));
		return center - lowest > radius ? center : lowest;
	}
};

namespace world_editor
{
// A “WorldEditor” that can set blocks in your map
struct WorldEditor
{
	struct DecalFrame
	{
		int x, y, z;
		std::int8_t facing, rotation;
		int map_id;
		bool glow;
	};
	struct FrameCellHash
	{
		std::size_t operator()(const std::tuple<int, int, int> &p) const noexcept
		{
			const auto [x, y, z] = p;
			std::size_t seed = std::hash<int>{}(x);
			seed ^= std::hash<int>{}(y) + 0x9e3779b9 + (seed << 6) + (seed >> 2);
			seed ^= std::hash<int>{}(z) + 0x9e3779b9 + (seed << 6) + (seed >> 2);
			return seed;
		}
	};
	MapgenEarth *mg{};
	Ground *ground{};
	// Generation-format state shared by the C++ orchestration layer.
	int generation_format = 0; // Java=0, Bedrock=1, Luanti=2
	bool bake_lighting = false, place_schematics = false, start_with_map = false,
		 map_decals = false;
	// Matches trees::RegionSelector::base_spacing() for the default pack; hosts
	// loading a differently scaled schematic pack may override it.
	int tree_slot_spacing = 5;
	std::function<bool(int, int, int, std::uint8_t)> regional_tree_placer;
	int gamemode = 0, world_time = 0;
	std::string level_name, projection_name;
	std::filesystem::path output_path;
	// Geographic bounds are owned by the mapgen host.  Keeping them with the
	// editor lets renderer-side pipeline ports (tree packs, previews, landmarks)
	// use the same metadata without depending on a Rust-only GenerationOptions.
	double min_lat = 0.0, max_lat = 0.0, min_lon = 0.0, max_lon = 0.0;
	bool flush_requested = false, save_requested = false;
	std::function<bool()> flush_sink, save_sink, preview_sink, map_item_sink,
			world_settings_sink;
	std::function<bool(int, int, int, int)> begin_tile_sink;
	std::function<bool(int, int, int, int)> merge_tile_sink;
	int spawn_x = 0, spawn_y = 0, spawn_z = 0;
	double projection_scale = 1.0;
	// A host that understands Java/Sponge block states can retain schematic
	// properties (facing, axis, slab type, etc.) instead of losing them at the
	// Freeminer Node boundary.  The default mapgen path still writes `block`.
	std::function<void(const BlockWithProperties &, int, int, int)> block_properties_sink;
	std::shared_ptr<const decals::DecalRegistry> decal_registry;
	std::function<bool(const DecalFrame &)> decal_frame_sink;
	std::unordered_set<std::tuple<int, int, int>, FrameCellHash> frame_cells;
	std::optional<std::tuple<int, int, int, int>> strict_bounds;
	void set_generation_format(int f) { generation_format = f; }
	int get_generation_format() const { return generation_format; }
	void set_bake_lighting(bool v) { bake_lighting = v; }
	void set_place_schematics(bool v) { place_schematics = v; }
	void set_tree_slot_spacing(int spacing) { tree_slot_spacing = std::max(1, spacing); }
	int get_tree_slot_spacing() const { return tree_slot_spacing; }
	void set_regional_tree_placer(std::function<bool(int, int, int, std::uint8_t)> placer)
	{
		regional_tree_placer = std::move(placer);
	}
	bool place_regional_tree(int x, int y, int z, std::uint8_t cover)
	{
		return regional_tree_placer && regional_tree_placer(x, y, z, cover);
	}
	void set_start_with_map(bool v) { start_with_map = v; }
	void set_map_decals(bool v) { map_decals = v; }
	void set_decal_registry(std::shared_ptr<const decals::DecalRegistry> registry)
	{
		decal_registry = std::move(registry);
	}
	void set_decal_frame_sink(std::function<bool(const DecalFrame &)> sink)
	{
		decal_frame_sink = std::move(sink);
	}
	void set_strict_bounds(int min_x, int min_z, int max_x, int max_z)
	{
		strict_bounds = std::tuple{min_x, min_z, max_x, max_z};
	}
	bool owns(int x, int z) const
	{
		if (!strict_bounds)
			return true;
		const auto [min_x, min_z, max_x, max_z] = *strict_bounds;
		return x >= min_x && x <= max_x && z >= min_z && z <= max_z;
	}
	bool signage_enabled() const { return map_decals && bool(decal_registry); }
	static std::tuple<int, int, int> decal_frame_cell(
			int x, int y, int z, std::int8_t facing)
	{
		switch (facing) {
		case 0:
			return {x, y - 1, z};
		case 1:
			return {x, y + 1, z};
		case 3:
			return {x, y, z + 1};
		case 4:
			return {x - 1, y, z};
		case 5:
			return {x + 1, y, z};
		default:
			return {x, y, z - 1};
		}
	}
	bool cell_has_frame(int x, int y, int z) const
	{
		return frame_cells.contains({x, y, z});
	}
	bool place_map_decal_ex(int x, int y, int z, std::int8_t facing, int map_id,
			std::int8_t rotation = 0, bool glow = false, bool require_air = false)
	{
		const auto [fx, fy, fz] = decal_frame_cell(x, y, z, facing);
		if (!owns(fx, fz) || fy - get_ground_level(fx, fz) < 1 ||
				frame_cells.contains({fx, fy, fz}))
			return false;
		if (require_air && check_for_block_absolute(fx, fy, fz, std::nullopt))
			return false;
		const DecalFrame frame{
				fx, fy, fz, facing, std::int8_t((rotation % 8 + 8) % 8), map_id, glow};
		if (decal_frame_sink && !decal_frame_sink(frame))
			return false;
		frame_cells.insert({fx, fy, fz});
		return true;
	}
	static std::tuple<int, int, int, int> panel_axes(std::int8_t facing)
	{
		switch (facing) {
		case 0:
		case 1:
			return {1, 0, 0, 1};
		case 2:
			return {-1, 0, -1, 0};
		case 3:
			return {1, 0, -1, 0};
		case 4:
			return {0, 1, -1, 0};
		default:
			return {0, -1, -1, 0};
		}
	}
	bool place_decal_panel(int x, int y, int z, std::int8_t facing,
			const decals::DecalKey &key, bool glow = false, bool require_hosts = false)
	{
		if (!signage_enabled())
			return false;
		const auto entry = decal_registry->get(key);
		if (!entry)
			return false;
		const auto [rx, rz, down_y, floor_z] = panel_axes(facing);
		for (int row = 0; row < int(entry->rows); ++row)
			for (int col = 0; col < int(entry->cols); ++col) {
				const int hx = x + rx * col, hy = y + (facing <= 1 ? 0 : down_y * row),
						  hz = z + rz * col + (facing <= 1 ? floor_z * row : 0);
				const auto [fx, fy, fz] = decal_frame_cell(hx, hy, hz, facing);
				if (!owns(fx, fz) || fy - get_ground_level(fx, fz) < 1 ||
						frame_cells.contains({fx, fy, fz}) ||
						(require_hosts &&
								!check_for_block_absolute(hx, hy, hz, std::nullopt)))
					return false;
			}
		for (int row = 0; row < int(entry->rows); ++row)
			for (int col = 0; col < int(entry->cols); ++col) {
				const int hx = x + rx * col, hy = y + (facing <= 1 ? 0 : down_y * row),
						  hz = z + rz * col + (facing <= 1 ? floor_z * row : 0);
				if (!place_map_decal_ex(
							hx, hy, hz, facing, entry->tile_id(col, row), 0, glow, false))
					return false;
			}
		return true;
	}
	bool place_decal(int x, int y, int z, std::int8_t facing, const decals::DecalKey &key)
	{
		return place_decal_panel(x, y, z, facing, key);
	}
	static std::pair<int, int> panel_left_anchor(
			int x, int z, std::int8_t facing, int cols)
	{
		const auto [rx, rz, down_y, floor_z] = panel_axes(facing);
		(void)down_y;
		(void)floor_z;
		const int half = (cols - 1) / 2;
		return {x - rx * half, z - rz * half};
	}
	static std::int8_t facing_for_normal(int nx, int nz)
	{
		return std::abs(nx) >= std::abs(nz) ? (nx >= 0 ? 5 : 4) : (nz >= 0 ? 3 : 2);
	}
	void set_game_settings(int mode, int time)
	{
		gamemode = mode;
		world_time = time;
	}
	void set_level_name(std::string n) { level_name = std::move(n); }
	void set_spawn(int x, int y, int z)
	{
		spawn_x = x;
		spawn_y = y;
		spawn_z = z;
	}
	void set_projection_info(std::string p, double s)
	{
		projection_name = std::move(p);
		projection_scale = s;
	}
	double scale() const { return projection_scale; }
	void set_geographic_bounds(
			double min_lat_, double max_lat_, double min_lon_, double max_lon_)
	{
		min_lat = min_lat_;
		max_lat = max_lat_;
		min_lon = min_lon_;
		max_lon = max_lon_;
	}
	std::array<double, 4> geographic_bounds() const
	{
		return {min_lat, max_lat, min_lon, max_lon};
	}
	void set_output_path(std::filesystem::path p) { output_path = std::move(p); }
	void set_block_properties_sink(
			std::function<void(const BlockWithProperties &, int, int, int)> sink)
	{
		block_properties_sink = std::move(sink);
	}
	void request_flush() { flush_requested = true; }
	void request_save() { save_requested = true; }
	void set_persistence_hooks(std::function<bool()> flush, std::function<bool()> save,
			std::function<bool()> preview = {}, std::function<bool()> map_item = {},
			std::function<bool()> world_settings = {})
	{
		flush_sink = std::move(flush);
		save_sink = std::move(save);
		preview_sink = std::move(preview);
		map_item_sink = std::move(map_item);
		world_settings_sink = std::move(world_settings);
	}
	bool finalize_persistence()
	{
		if (flush_requested && flush_sink && !flush_sink())
			return false;
		if (save_requested && save_sink && !save_sink())
			return false;
		if (world_settings_sink && !world_settings_sink())
			return false;
		if (start_with_map && map_item_sink && !map_item_sink())
			return false;
		if (preview_sink && !preview_sink())
			return false;
		return true;
	}
	void set_tile_hooks(std::function<bool(int, int, int, int)> begin_tile,
			std::function<bool(int, int, int, int)> merge_tile)
	{
		begin_tile_sink = std::move(begin_tile);
		merge_tile_sink = std::move(merge_tile);
	}
	bool begin_tile(int min_x, int min_z, int max_x, int max_z)
	{
		return !begin_tile_sink || begin_tile_sink(min_x, min_z, max_x, max_z);
	}
	bool merge_tile(int min_x, int min_z, int max_x, int max_z)
	{
		return !merge_tile_sink || merge_tile_sink(min_x, min_z, max_x, max_z);
	}
	bool flush_requested_now() const { return flush_requested; }
	bool save_requested_now() const { return save_requested; }
	void clear_flush_request() { flush_requested = false; }
	void clear_save_request() { save_requested = false; }
	std::filesystem::path schematic_asset_root;
	void set_schematic_asset_root(std::filesystem::path root)
	{
		schematic_asset_root = std::move(root);
	}
	const std::filesystem::path &get_schematic_asset_root() const
	{
		return schematic_asset_root;
	}
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
			const auto current = mg->readTileOverlay(pos).value_or(mg->vm->getNode(pos));
			const auto content = current.getContent();
			if (maybe_variants) {
				should_set = std::any_of(maybe_variants->begin(), maybe_variants->end(),
						[content](const Block &b) { return b.getContent() == content; });
			} else if (maybe_replacements) {
				should_set = std::none_of(maybe_replacements->begin(),
						maybe_replacements->end(),
						[content](const Block &b) { return b.getContent() == content; });
			}
			if (should_set) {
				if (!mg->writeTileOverlay(pos, block))
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
		if (block_properties_sink && !bwp.properties.empty())
			block_properties_sink(bwp, x, y, z);
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

		const auto n = mg->readTileOverlay(pos).value_or(mg->vm->getNode(pos));
		const auto content = n.getContent();
		if (content == CONTENT_AIR || content == CONTENT_IGNORE)
			return false;
		if (blocks) {
			return std::any_of(blocks->begin(), blocks->end(),
					[content](const Block &b) { return b.getContent() == content; });
		}
		return true;
	}

	// Rust WorldEditor::block_at: true for a present non-air node.  It is kept
	// separate from check_for_block because callers need an occupancy test,
	// rather than a material filter (notably vertically-grown wetland reeds).
	bool block_at(int x, int y, int z) const
	{
		if (!mg || !mg->vm)
			return false;
		const int absolute_y = ground ? ground->level({x, z}) + y : y;
		const v3pos_t pos{static_cast<pos_t>(x), static_cast<pos_t>(absolute_y),
				static_cast<pos_t>(z)};
		if (!mg->vm->exists(pos))
			return false;
		const auto content =
				mg->readTileOverlay(pos).value_or(mg->vm->getNode(pos)).getContent();
		return content != CONTENT_AIR && content != CONTENT_IGNORE;
	}

	// Highest occupied absolute Y in an inclusive column interval.  Tree canopy
	// placement samples this before writing leaves, just as Rust does, so a low
	// roof only culls intersecting leaves instead of the complete canopy.
	std::optional<int> highest_block_between(int x, int z, int min_y, int max_y) const
	{
		if (!mg || !mg->vm)
			return std::nullopt;
		if (min_y > max_y)
			std::swap(min_y, max_y);
		for (int y = max_y; y >= min_y; --y) {
			const v3pos_t pos{
					static_cast<pos_t>(x), static_cast<pos_t>(y), static_cast<pos_t>(z)};
			if (!mg->vm->exists(pos))
				continue;
			const auto c =
					mg->readTileOverlay(pos).value_or(mg->vm->getNode(pos)).getContent();
			if (c != CONTENT_AIR && c != CONTENT_IGNORE)
				return y;
		}
		return std::nullopt;
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
	int get_ground_level(int x, int z) const
	{
		return ground ? ground->level({x, z}) : 0;
	}
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
		const v3pos_t pos{static_cast<pos_t>(x),
				static_cast<pos_t>(get_water_level(x, z)), static_cast<pos_t>(z)};
		if (!mg->vm->exists(pos))
			return false;
		return mg->readTileOverlay(pos).value_or(mg->vm->getNode(pos)).getContent() ==
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
		const auto n = mg->readTileOverlay(pos).value_or(mg->vm->getNode(pos));
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

	void fill_column_absolute(
			const Block &block, int x, int z, int min_y, int max_y, bool skip_existing)
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
