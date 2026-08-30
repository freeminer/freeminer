#pragma once

#include <algorithm>
#include <cstdint>
#include <filesystem>
#include <functional>
#include <limits>
#include <memory>
#include <optional>
#include <string>
#include <tuple>
#include <unordered_map>
#include <unordered_set>
#include <utility>
#include <vector>

#include <osmium/osm/location.hpp>

#include "map.h"
#include "arnis_ground.h"
#include "arnis_block.h"
#include "arnis-cpp/src/args.h"
#include "arnis-cpp/src/decals/registry.h"
#include "arnis-cpp/src/trees/tree_library.h"

#ifdef stoi
#undef stoi
#endif
#ifdef stof
#undef stof
#endif

namespace arnis
{

namespace signage
{
struct SignageContext;
}

namespace block_definitions
{
extern Block CHEST;
extern Block BARREL;
extern Block LIGHT_GRAY_WALL_BANNER;
extern Block WATER;
extern Block SIGN;
extern Block STEEL_SIGN;
extern Block TEXT_SIGN_SMALL;
extern Block TEXT_SIGN_MEDIUM;
extern Block TEXT_SIGN_LARGE;
extern Block DECAL_FRAME;
extern Block EARTH_BENCH;
extern Block EARTH_TRASH_CAN;
extern Block EARTH_STREET_LAMP;
extern Block EARTH_WELL;
extern Block EARTH_BARBECUE;
extern Block EARTH_GRATING;
extern Block EARTH_FENCE_CHAINLINK;
extern Block EARTH_FENCE_BARBED;
extern Block EARTH_FENCE_PICKET;
extern Block EARTH_FENCE_WROUGHT;
extern Block ADV_RAIL_NORTH_SOUTH;
extern Block ADV_RAIL_EAST_WEST;
extern Block ADV_RAIL_DIAGONAL_NE_SW;
extern Block ADV_RAIL_DIAGONAL_NW_SE;
extern Block ADV_RAIL_STRAIGHT_0;
extern Block ADV_RAIL_STRAIGHT_30;
extern Block ADV_RAIL_STRAIGHT_45;
extern Block ADV_RAIL_STRAIGHT_60;
extern Block ADV_RAIL_CURVE_0;
extern Block ADV_RAIL_CURVE_30;
extern Block ADV_RAIL_CURVE_45;
extern Block ADV_RAIL_CURVE_60;
extern Block ADV_RAIL_SLOPE_UP;
extern Block ADV_RAIL_SLOPE_DOWN;
extern bool ADVTRAINS_SLOPES_AVAILABLE;
extern bool ADVTRAINS_AVAILABLE;
extern Block ADV_PLATFORM_HIGH;
}

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
	struct XZCellHash
	{
		std::size_t operator()(const std::pair<int, int> &p) const noexcept
		{
			const auto key = (std::uint64_t(static_cast<std::uint32_t>(p.first)) << 32) |
					std::uint32_t(p.second);
			return std::hash<std::uint64_t>{}(key);
		}
	};
	MapgenEarth *mg{};
	Ground *ground{};
	// Generation-format state shared by the C++ orchestration layer.
	int generation_format = 0; // Java=0, Bedrock=1, Luanti=2
	bool bake_lighting = false;
	bool start_with_map = false;
	bool place_schematics_enabled = true;
	bool map_decals = true;
	// Matches trees::RegionSelector::base_spacing() for the default pack; hosts
	// loading a differently scaled schematic pack may override it.
	int tree_slot_spacing_blocks = 5;
	std::function<bool(int, int, int, std::uint8_t)> regional_tree_placer;
	GameMode gamemode = GameMode::Creative;
	std::int64_t world_time = 6000;
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
	std::function<void(
			int, int, int, const std::vector<std::tuple<std::string, int, int>> &)>
			chest_sink;
	std::function<void(
			int, int, int, const std::vector<std::tuple<std::string, int, int>> &)>
			barrel_sink;
	std::function<void(int, int, int)> bed_sink;
	std::function<void(int, int, int, const std::string &)> item_frame_sink;
	std::function<void(int, int, int, const std::string &, const std::string &,
			const std::vector<std::pair<std::string, std::string>> &)>
			banner_sink;
	std::shared_ptr<const decals::DecalRegistry> decal_registry;
	// Per-generation signage state.  Rust keeps this in WorldEditor so parallel
	// emerge threads never exchange intersection indexes or regional styles.
	std::shared_ptr<const signage::SignageContext> signage_context;
	std::function<bool(const DecalFrame &)> decal_frame_sink;
	std::unordered_set<std::tuple<int, int, int>, FrameCellHash> frame_cells;
	std::vector<DecalFrame> placed_frames;
	std::unordered_set<std::tuple<int, int, int>, FrameCellHash> written_cells;
	// Effective terrain/road elevation cache. The dense part covers the mapchunk
	// and its OSM halo; only unusual out-of-halo queries use the sparse fallback.
	// Road registration overwrites an existing sampled terrain entry.
	mutable std::vector<int> ground_level_cache;
	mutable std::unordered_map<std::pair<int, int>, int, XZCellHash>
			ground_level_overflow;
	int ground_cache_min_x = 0, ground_cache_min_z = 0;
	std::size_t ground_cache_width = 0, ground_cache_height = 0;
	std::optional<std::tuple<int, int, int, int>> strict_bounds;
	int ground_origin_x = 0, ground_origin_z = 0;
	bool ground_origin_set = false;
	void set_generation_format(int f) { generation_format = f; }
	int get_generation_format() const { return generation_format; }
	int format() const { return generation_format; }
	void set_bake_lighting(bool v) { bake_lighting = v; }
	void set_place_schematics(bool v) { place_schematics_enabled = v; }
	bool place_schematics() const { return place_schematics_enabled; }
	void set_tree_slot_spacing(int spacing)
	{
		tree_slot_spacing_blocks = std::max(1, spacing);
	}
	int get_tree_slot_spacing() const { return tree_slot_spacing_blocks; }
	int tree_slot_spacing() const { return tree_slot_spacing_blocks; }
	void set_ground_origin(int x, int z)
	{
		ground_origin_x = x;
		ground_origin_z = z;
		ground_origin_set = true;
		ground_level_cache.clear();
		ground_level_overflow.clear();
		ground_cache_width = ground_cache_height = 0;
	}
	void reserve_ground_level_cache()
	{
		ground_level_cache.clear();
		ground_level_overflow.clear();
		if (mg) {
			// OSM generation includes a two-mapblock halo around the core mapchunk.
			ground_cache_min_x = mg->node_min.X - 32;
			ground_cache_min_z = mg->node_min.Z - 32;
			ground_cache_width =
					std::size_t(std::max(1, mg->node_max.X - mg->node_min.X + 65));
			ground_cache_height =
					std::size_t(std::max(1, mg->node_max.Z - mg->node_min.Z + 65));
			const std::size_t columns = ground_cache_width * ground_cache_height;
			if (columns <= 262144) {
				ground_level_cache.assign(columns, std::numeric_limits<int>::min());
				ground_level_overflow.reserve(1024);
				return;
			}
		}
		ground_cache_width = ground_cache_height = 0;
		ground_level_cache.clear();
		ground_level_overflow.reserve(4096);
	}
	XZPoint ground_point(int x, int z) const
	{
		const int origin_x =
				ground_origin_set ? ground_origin_x : (mg ? mg->node_min.X : 0);
		const int origin_z =
				ground_origin_set ? ground_origin_z : (mg ? mg->node_min.Z : 0);
		return {x - origin_x, z - origin_z};
	}
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
	void set_signage_context(std::shared_ptr<const signage::SignageContext> context)
	{
		signage_context = std::move(context);
	}
	void set_decal_frame_sink(std::function<bool(const DecalFrame &)> sink)
	{
		decal_frame_sink = std::move(sink);
	}
	void set_chest_sink(std::function<void(int, int, int,
					const std::vector<std::tuple<std::string, int, int>> &)>
					sink)
	{
		chest_sink = std::move(sink);
	}
	void set_chest_with_items_absolute(int x, int y, int z,
			const std::vector<std::tuple<std::string, int, int>> &items)
	{
		if (try_set_block_absolute(
					block_definitions::CHEST, x, y, z, std::nullopt, std::nullopt) &&
				chest_sink)
			chest_sink(x, y, z, items);
	}
	void set_barrel_sink(std::function<void(int, int, int,
					const std::vector<std::tuple<std::string, int, int>> &)>
					sink)
	{
		barrel_sink = std::move(sink);
	}
	void set_barrel_with_items_absolute(int x, int y, int z,
			const std::vector<std::tuple<std::string, int, int>> &items)
	{
		BlockWithProperties barrel{block_definitions::BARREL, {{"facing", "up"}}};
		if (try_set_block_with_properties_absolute(
					barrel, x, y, z, std::nullopt, std::nullopt) &&
				barrel_sink)
			barrel_sink(x, y, z, items);
	}
	void set_bed_sink(std::function<void(int, int, int)> sink)
	{
		bed_sink = std::move(sink);
	}
	void set_item_frame_sink(std::function<void(int, int, int, const std::string &)> sink)
	{
		item_frame_sink = std::move(sink);
	}
	void set_banner_sink(
			std::function<void(int, int, int, const std::string &, const std::string &,
					const std::vector<std::pair<std::string, std::string>> &)>
					sink)
	{
		banner_sink = std::move(sink);
	}
	void set_bed_block_entity_absolute(int x, int y, int z)
	{
		if (bed_sink && block_exists_absolute(x, y, z))
			bed_sink(x, y, z);
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
	bool place_sign_node(Block sign, int x, int y, int z, std::int8_t param2,
			const std::string &text = {})
	{
		if (!owns(x, z) || sign.id() == CONTENT_AIR)
			return false;
		sign.setParam2(static_cast<std::uint8_t>(param2));
		if (!try_set_block_absolute(sign, x, y, z, std::nullopt, std::nullopt))
			return false;
		if (text.empty())
			return true;
		if (!mg || !mg->active_block_data ||
			x < std::numeric_limits<pos_t>::min() ||
			x > std::numeric_limits<pos_t>::max() ||
			y < std::numeric_limits<pos_t>::min() ||
			y > std::numeric_limits<pos_t>::max() ||
			z < std::numeric_limits<pos_t>::min() ||
			z > std::numeric_limits<pos_t>::max())
			return false;
		return mg->queueGeneratedSign(
				{static_cast<pos_t>(x), static_cast<pos_t>(y), static_cast<pos_t>(z)},
				text);
	}
	bool place_text_sign(
			int x, int y, int z, std::int8_t facing, const std::string &text, bool steel)
	{
		if (text.empty())
			return false;
		Block sign = block_definitions::SIGN;
		if (steel) {
			std::size_t lines = 1;
			std::size_t line_length = 0;
			std::size_t max_line_length = 0;
			for (char c : text) {
				if (c == '\n') {
					++lines;
					max_line_length = std::max(max_line_length, line_length);
					line_length = 0;
				} else {
					++line_length;
				}
			}
			max_line_length = std::max(max_line_length, line_length);
			if (lines <= 3 && max_line_length <= 50)
				sign = block_definitions::TEXT_SIGN_SMALL;
			else if (lines <= 6 && max_line_length <= 50)
				sign = block_definitions::TEXT_SIGN_MEDIUM;
			else
				sign = block_definitions::TEXT_SIGN_LARGE;
		}
		return place_sign_node(sign, x, y, z, facing, text);
	}
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
		placed_frames.push_back(frame);
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
	std::vector<DecalFrame> item_frames() const { return placed_frames; }
	void set_game_settings(GameMode mode, std::int64_t time)
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

	bool pos_ok(int x, int z) const
	{
		return mg && x >= mg->node_min.X && x <= mg->node_max.X && z >= mg->node_min.Z &&
			   z <= mg->node_max.Z && owns(x, z);
	};
	// Place a block at (x, y, z). The optional adjacency arguments
	// mimic the Rust code’s “Some(&[COBBLESTONE, COBBLESTONE_WALL])” idea.
	void set_block(const Block &block, int x, int y, int z,
			const std::optional<std::vector<Block>> &replace_with = {},
			const std::optional<std::vector<Block>> &avoid = {})
	{
		if (!ground || !pos_ok(x, z))
			return;
		return set_block_absolute(
				block, x, get_absolute_y(x, y, z), z, replace_with, avoid);
	}

	bool try_set_block_absolute(const Block &block, int x, int y, int z,
			const std::optional<std::vector<Block>> &maybe_variants = {},
			const std::optional<std::vector<Block>> &maybe_replacements = {})
	{
		if (x < std::numeric_limits<pos_t>::min() ||
				x > std::numeric_limits<pos_t>::max() ||
				y < std::numeric_limits<pos_t>::min() ||
				y > std::numeric_limits<pos_t>::max() ||
				z < std::numeric_limits<pos_t>::min() ||
				z > std::numeric_limits<pos_t>::max())
			return false;
		const v3pos_t pos{
				static_cast<pos_t>(x), static_cast<pos_t>(y), static_cast<pos_t>(z)};

		if (!mg || !mg->vm || !pos_ok(x, z) || !mg->vm->exists(pos)) {
			if (mg)
				++mg->stat.miss;
			return false;
		}

		const auto key = std::tuple{x, y, z};
		const auto overlay = mg->readTileOverlay(pos);
		bool should_set = true;
		if (!maybe_variants && !maybe_replacements) {
			// Rust's None/None path records only the first generated block at a cell.
			should_set = !overlay.has_value() && !written_cells.contains(key);
		} else {
			const bool generated = overlay.has_value() || written_cells.contains(key);
			if (generated) {
				const auto current = overlay.value_or(mg->vm->getNode(pos));
				const auto content = current.getContent();
				if (maybe_variants) {
					should_set = std::any_of(maybe_variants->begin(),
							maybe_variants->end(), [content](const Block &b) {
								return b.getContent() == content;
							});
				} else {
					should_set = std::none_of(maybe_replacements->begin(),
							maybe_replacements->end(), [content](const Block &b) {
								return b.getContent() == content;
							});
				}
			}
		}
		if (!should_set)
			return false;

		if (!mg->writeTileOverlay(pos, block))
			mg->vm->setNode(pos, block);
		written_cells.insert(key);
		++mg->stat.set;
		return true;
	}

	void set_block_absolute(const Block &block, int x, int y, int z,
			const std::optional<std::vector<Block>> &maybe_variants = {},
			const std::optional<std::vector<Block>> &maybe_replacements = {})
	{
		(void)try_set_block_absolute(block, x, y, z, maybe_variants, maybe_replacements);
	}

	void set_block_absolute(const Block &block, int x, int y, int z,
			const std::vector<Block> *variants, const std::vector<Block> *replacements)
	{
		const std::optional<std::vector<Block>> whitelist =
				variants ? std::optional<std::vector<Block>>(*variants) : std::nullopt;
		const std::optional<std::vector<Block>> blacklist =
				replacements ? std::optional<std::vector<Block>>(*replacements)
							 : std::nullopt;
		set_block_absolute(block, x, y, z, whitelist, blacklist);
	}

	bool try_set_block_with_properties_absolute(const BlockWithProperties &bwp, int32_t x,
			int32_t y, int32_t z, const std::optional<std::vector<Block>> &variants,
			const std::optional<std::vector<Block>> &replacements)
	{
		if (!try_set_block_absolute(bwp.block, x, y, z, variants, replacements))
			return false;
		if (block_properties_sink && !bwp.properties.empty())
			block_properties_sink(bwp, x, y, z);
		return true;
	}

	void set_block_with_properties_absolute(const BlockWithProperties &bwp, int32_t x,
			int32_t y, int32_t z, const std::optional<std::vector<Block>> &variants,
			const std::optional<std::vector<Block>> &replacements)
	{
		(void)try_set_block_with_properties_absolute(
				bwp, x, y, z, variants, replacements);
	}

	void set_block_with_properties_absolute(const BlockWithProperties &bwp, int32_t x,
			int32_t y, int32_t z, const std::vector<Block> *variants,
			const std::vector<Block> *replacements)
	{
		const std::optional<std::vector<Block>> whitelist =
				variants ? std::optional<std::vector<Block>>(*variants) : std::nullopt;
		const std::optional<std::vector<Block>> blacklist =
				replacements ? std::optional<std::vector<Block>>(*replacements)
							 : std::nullopt;
		set_block_with_properties_absolute(bwp, x, y, z, whitelist, blacklist);
	}
	bool check_for_block(
			int x, int y, int z, const std::optional<std::vector<Block>> &blocks)
	{
		if (!ground || !pos_ok(x, z))
			return false;
		return check_for_block_absolute(x, get_absolute_y(x, y, z), z, blocks);
	}

	// Rust WorldEditor::block_at: true for a present non-air node.  It is kept
	// separate from check_for_block because callers need an occupancy test,
	// rather than a material filter (notably vertically-grown wetland reeds).
	bool block_at(int x, int y, int z) const
	{
		return get_block_absolute(x, get_absolute_y(x, y, z), z).has_value();
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
			if (get_block_absolute(x, y, z))
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
	int get_absolute_y(int x, int y, int z) const { return get_ground_level(x, z) + y; }
	int get_ground_level(int x, int z) const
	{
		const int local_x = x - ground_cache_min_x;
		const int local_z = z - ground_cache_min_z;
		if (local_x >= 0 && local_z >= 0 &&
				std::size_t(local_x) < ground_cache_width &&
				std::size_t(local_z) < ground_cache_height) {
			const std::size_t index =
					std::size_t(local_z) * ground_cache_width + std::size_t(local_x);
			int &cached = ground_level_cache[index];
			if (cached != std::numeric_limits<int>::min())
				return cached;
			if (!ground)
				return 0;
			cached = ground->level({x, z});
			return cached;
		}
		const std::pair<int, int> position{x, z};
		if (const auto it = ground_level_overflow.find(position);
				it != ground_level_overflow.end())
			return it->second;
		if (!ground)
			return 0;
		const int level = ground->level({x, z});
		ground_level_overflow.emplace(position, level);
		return level;
	}
	std::optional<int> terrain_level(int x, int z) const
	{
		return ground ? std::optional<int>(ground->level({x, z})) : std::nullopt;
	}
	void register_road_surface_y(int x, int z, int y)
	{
		const int local_x = x - ground_cache_min_x;
		const int local_z = z - ground_cache_min_z;
		if (local_x >= 0 && local_z >= 0 &&
				std::size_t(local_x) < ground_cache_width &&
				std::size_t(local_z) < ground_cache_height) {
			ground_level_cache[std::size_t(local_z) * ground_cache_width +
					std::size_t(local_x)] = y;
			return;
		}
		ground_level_overflow[{x, z}] = y;
	}
	bool water_source_is_enclosed(int x, int z) const
	{
		const int base = get_ground_level(x, z);
		return get_ground_level(x + 1, z) >= base && get_ground_level(x - 1, z) >= base &&
			   get_ground_level(x, z + 1) >= base && get_ground_level(x, z - 1) >= base;
	}
	biome::Climate climate() const
	{
		return ground ? ground->climate() : biome::Climate::Temperate;
	}
	int get_water_level(int x, int z) const
	{
		return ground ? ground->water_level({x, z}) : get_ground_level(x, z);
	}
	bool is_lc_water(int x, int z) const
	{
		if (ground && ground->has_land_cover()) {
			return ground->cover_class(ground_point(x, z)) == land_cover::LC_WATER;
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
	bool land_cover_backs_trees(int x, int z) const
	{
		if (!ground)
			return true;
		const auto point = ground_point(x, z);
		if (ground->has_canopy()) {
			if (const auto height = ground->canopy_height_m(point))
				return *height >= canopy::CANOPY_MIN_M;
		}
		if (!ground->has_land_cover())
			return true;
		const auto cover = ground->cover_class(point);
		return cover == land_cover::LC_TREE_COVER || cover == land_cover::LC_SHRUBLAND;
	}
	std::optional<trees::TreeSize> canopy_size_hint(int x, int z) const
	{
		if (!ground)
			return std::nullopt;
		const auto height = ground->canopy_height_m(ground_point(x, z));
		if (!height || *height < canopy::CANOPY_MIN_M)
			return std::nullopt;
		return trees::size_for_canopy_m(*height);
	}
	uint8_t water_distance(int x, int z) const
	{
		if (ground && ground->has_land_cover())
			return ground->water_distance(ground_point(x, z));
		return 0;
	}

	bool check_for_block_absolute(int x, int y, int z,
			const std::optional<std::vector<Block>> &blocks = {},
			const std::optional<std::vector<Block>> &avoid = {})
	{
		const v3pos_t pos{
				static_cast<pos_t>(x), static_cast<pos_t>(y), static_cast<pos_t>(z)};
		if (!mg || !mg->vm || !pos_ok(x, z) || !mg->vm->exists(pos))
			return false;
		++mg->stat.check;
		const auto overlay = mg->readTileOverlay(pos);
		if (!overlay && !written_cells.contains({x, y, z}))
			return false;
		const auto n = overlay.value_or(mg->vm->getNode(pos));
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

	std::optional<Block> get_block_absolute(int x, int y, int z) const
	{
		if (!mg || !mg->vm || !pos_ok(x, z))
			return std::nullopt;
		const v3pos_t pos{
				static_cast<pos_t>(x), static_cast<pos_t>(y), static_cast<pos_t>(z)};
		if (!mg->vm->exists(pos))
			return std::nullopt;
		const auto overlay = mg->readTileOverlay(pos);
		if (!overlay && !written_cells.contains({x, y, z}))
			return std::nullopt;
		return Block(overlay.value_or(mg->vm->getNode(pos)).getContent());
	}

	bool cell_open_at(int x, int y, int z) const { return !get_block_absolute(x, y, z); }

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
			if (skip_existing)
				set_block_absolute(block, x, y, z);
			else
				set_block_absolute(block, x, y, z, std::nullopt,
						std::optional<std::vector<Block>>(std::vector<Block>{}));
		}
	}

	void place_wall_banner(const Block &block, int x, int y, int z,
			const std::string &facing, const std::string &base_color,
			const std::vector<std::pair<std::string, std::string>> &patterns)
	{
		const BlockWithProperties banner{block, {{"facing", facing}}};
		if (try_set_block_with_properties_absolute(
					banner, x, y, z, std::nullopt, std::nullopt) &&
				banner_sink)
			banner_sink(x, y, z, facing, base_color, patterns);
	}

	void place_wall_banner(int x, int y, int z, const std::string &facing,
			const std::vector<std::pair<std::string, std::string>> &patterns)
	{
		place_wall_banner(block_definitions::LIGHT_GRAY_WALL_BANNER, x, y, z, facing,
				"light_gray", patterns);
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

	void fill_blocks_absolute(const Block &block, std::int32_t x1, std::int32_t y1,
			std::int32_t z1, std::int32_t x2, std::int32_t y2, std::int32_t z2,
			const std::optional<std::vector<Block>> &override_whitelist = {},
			const std::optional<std::vector<Block>> &override_blacklist = {})
	{
		auto [min_x, max_x] = std::minmax(x1, x2);
		auto [min_y, max_y] = std::minmax(y1, y2);
		auto [min_z, max_z] = std::minmax(z1, z2);
		for (std::int32_t x = min_x; x <= max_x; ++x)
			for (std::int32_t y = min_y; y <= max_y; ++y)
				for (std::int32_t z = min_z; z <= max_z; ++z)
					set_block_absolute(
							block, x, y, z, override_whitelist, override_blacklist);
		if (mg)
			++mg->stat.fill;
	}

	void fill_blocks(const Block &block, std::int32_t x1, std::int32_t y1,
			std::int32_t z1, std::int32_t x2, std::int32_t y2, std::int32_t z2)
	{
		return fill_blocks(block, x1, y1, z1, x2, y2, z2,
				std::optional<std::vector<Block>>{}, std::optional<std::vector<Block>>{});
	}
};

}

using WorldEditor = world_editor::WorldEditor;
}
