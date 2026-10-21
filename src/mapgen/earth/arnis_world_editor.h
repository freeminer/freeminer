#pragma once

#include <algorithm>
#include <atomic>
#include <cmath>
#include <cstdint>
#include <filesystem>
#include <functional>
#include <memory>
#include <mutex>
#include <optional>
#include <string>
#include <tuple>
#include <unordered_map>
#include <unordered_set>
#include <utility>
#include <vector>

#include <nlohmann/json.hpp>
#include <osmium/osm/location.hpp>

#include "arnis_ground.h"
#include "arnis_block.h"
#include "arnis_projection_frame.h"
#include "arnis-cpp/src/args.h"
#include "arnis-cpp/src/decals/registry.h"
#include "arnis-cpp/src/trees/tree_library.h"
#include "mapgen/earth/arnis_projection_frame.h"

#ifdef stoi
#undef stoi
#endif
#ifdef stof
#undef stof
#endif

namespace arnis
{
class CoordinateBitmap;

namespace signage
{
struct SignageContext;
}

namespace highways
{
class SignalIndex;
namespace cars
{
class CarNetwork;
}
}

namespace trees::mapped
{
class MappedTrunks;
}
namespace trees
{
struct MappedRequest;
enum class Habitat;
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
	std::shared_ptr<const highways::SignalIndex> traffic_signal_index;
	std::shared_ptr<const highways::cars::CarNetwork> car_network;
	std::shared_ptr<const CoordinateBitmap> sealed_surface;
	void set_sealed_surface(std::shared_ptr<const CoordinateBitmap> mask)
	{
		sealed_surface = std::move(mask);
	}
	void release_sealed_surface()
	{
		sealed_surface.reset();
		mapped_trunks.reset();
	}
	bool under_mapped_crown(int x, int z) const;
	bool surface_is_sealed(int x, int z) const;
	bool nested_area_owns(int x, int z) const;
	uint8_t cover_class(int x, int z) const;
	std::optional<ecoregion::Ecoregion> ecoregion(int x, int z) const;
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
			const auto &[x, y, z] = p;
			std::size_t seed = std::hash<int>{}(x);
			seed ^= std::hash<int>{}(y) + 0x9e3779b9 + (seed << 6) + (seed >> 2);
			seed ^= std::hash<int>{}(z) + 0x9e3779b9 + (seed << 6) + (seed >> 2);
			return seed;
		}
	};
	struct FacadePanel
	{
		int x{}, y{}, z{};
		std::int8_t facing{};
		std::uint32_t width{}, height{};
		std::vector<std::uint8_t> pixels;
		// Stable item-model name for entity-backed photo facades; empty for
		// backends that consume the sprite through the facade-panel sink.
		std::string item_name;
	};
	struct PlacedBlockEntity
	{
		int x{}, y{}, z{};
		nlohmann::json nbt;
	};
	struct PlacedSchematicEntity
	{
		int x{}, y{}, z{};
		std::vector<std::uint8_t> nbt;
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
	// Projection-aware local frame. Flat worlds retain the legacy X/Z behavior.
	ProjectionFrame projection_frame;
	Ground *ground{};
	// Generation-format state shared by the C++ orchestration layer.
	int generation_format = 0; // Java=0, Bedrock=1, Luanti=2
	bool bake_lighting = false;
	bool start_with_map = false;
	bool place_schematics_enabled = true;
	bool schematic_tree_pack_available = false;
	bool map_decals = true;
	int first_decal_map_id = decals::DecalRegistry::FIRST_ID;
	// Matches trees::RegionSelector::base_spacing() for the default pack; hosts
	// loading a differently scaled schematic pack may override it.
	int tree_slot_spacing_blocks = 5;
	std::function<double(int)> tree_crown_columns;
	std::function<std::optional<bool>(WorldEditor &, int, int, int, std::uint8_t,
			std::optional<trees::Habitat>, bool, bool, bool, bool)>
			regional_tree_placer;
	std::function<std::optional<bool>(
			WorldEditor &, int, int, int, std::uint8_t, const trees::MappedRequest &)>
			mapped_regional_tree_placer;
	// Authored OSM trunks suppress procedural canopy crowns in the same cells,
	// matching Rust's MappedTrunks pass during streamed ground decoration.
	std::shared_ptr<const trees::mapped::MappedTrunks> mapped_trunks;
	std::shared_ptr<const std::vector<std::pair<int, int>>> mapped_cranes;
	GameMode gamemode = GameMode::Creative;
	std::int64_t world_time = 6000;
	std::string level_name, projection_name;
	std::filesystem::path output_path;
	// Geographic bounds are owned by the mapgen host.  Keeping them with the
	// editor lets renderer-side pipeline ports (tree packs, previews, landmarks)
	// use the same metadata without depending on a Rust-only GenerationOptions.
	double min_lat = 0.0, max_lat = 0.0, min_lon = 0.0, max_lon = 0.0;
	// Climate and regional palettes use the center of these area bounds, matching
	// the Rust writer for both normal generation and One World extensions.
	bool flush_requested = false, save_requested = false;
	std::function<bool()> flush_sink, save_sink, preview_sink, map_item_sink,
			world_settings_sink;
	std::function<void(const std::string &)> generation_error_sink;
	std::optional<std::uint64_t> active_tile_overlay;
	std::function<std::optional<std::uint64_t>(int, int, int, int)> begin_tile_sink;
	std::function<bool(std::uint64_t)> merge_tile_sink;
	bool tile_hooks_thread_safe = false;
	bool defer_tile_merges = false;
	std::vector<std::uint64_t> deferred_tile_overlays;
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
	// Full Java-style item compounds for backends that preserve item metadata
	// (Slot, damage, display color, and modern item components).
	std::function<void(int, int, int, const std::vector<nlohmann::json> &)>
			barrel_nbt_sink;
	// Block entities are retained when the corresponding backend sink is absent,
	// matching Rust's chunk-owned block_entities collection for library callers.
	std::vector<PlacedBlockEntity> placed_block_entities;
	std::function<void(int, int, int)> bed_sink;
	// Optional backend event for newly generated water/lava source blocks. Java
	// exporters serialize scheduled ticks; live-world hosts may handle flow via
	// their native simulation instead.
	std::function<void(const Block &, pos_t, pos_t, pos_t)> fluid_tick_sink;
	// Raw schematic block-entity payload. When set, the decoder passes payloads
	// through unchanged instead of applying lossy built-in compatibility parsing.
	std::function<bool(std::optional<std::uint64_t>, int, int, int,
			const std::vector<std::uint8_t> &)>
			schem_entity_sink;
	bool schem_entity_sink_thread_safe = false;
	// Original payloads retained when no raw sink is installed. If consuming both
	// this and placed_block_entities, prefer the raw record at matching coords.
	std::vector<PlacedSchematicEntity> placed_schematic_entities;
	std::function<void(int, int, int, const std::string &)> item_frame_sink;
	// Backend-native entity payload. Java-capable consumers can serialize this
	// directly to entity NBT; the mapgen host may leave it unset when entities
	// are not supported by its output format. If absent or if it rejects an
	// entity, the JSON is retained here so library callers can collect the same
	// generated records Rust keeps in its world data.
	std::function<bool(const nlohmann::json &)> entity_sink;
	std::vector<nlohmann::json> placed_entities;
	std::function<void(int, int, int, const std::string &, const std::string &,
			const std::vector<std::pair<std::string, std::string>> &)>
			banner_sink;
	std::shared_ptr<const decals::DecalRegistry> decal_registry;
	// Per-generation signage state.  Rust keeps this in WorldEditor so parallel
	// emerge threads never exchange intersection indexes or regional styles.
	std::shared_ptr<const signage::SignageContext> signage_context;
	std::function<bool(WorldEditor &, const DecalFrame &)> decal_frame_sink;
	bool decal_frame_sink_thread_safe = false;
	// Backend contract for preset facade panels.  The mapgen layer supplies the
	// already-cropped RGB panel; Java/Bedrock/Luanti hosts may encode or place
	// it according to their native entity/texture mechanism.
	std::function<bool(int, int, int, std::int8_t, const std::vector<std::uint8_t> &,
			std::uint32_t, std::uint32_t)>
			facade_panel_sink;
	std::vector<FacadePanel> placed_facade_panels;
	std::unordered_set<std::tuple<int, int, int>, FrameCellHash> frame_cells;
	std::vector<DecalFrame> placed_frames;
	std::unordered_set<std::tuple<int, int, int>, FrameCellHash> active_frame_cells;
	std::vector<DecalFrame> active_placed_frames;
	std::unordered_map<std::pair<int, int>, Block, XZCellHash> support_columns;
	std::unordered_map<std::pair<int, int>, Block, XZCellHash> active_support_columns;
	// Track hedge cells so later area cleanup (for example sports-pitch clearing)
	// does not erase a living boundary that was emitted by the barrier pass.
	std::unordered_set<std::pair<int, int>, XZCellHash> hedge_columns;
	// Claim each deterministic regional-tree trunk slot once per tile editor, as
	// in Rust, so several ground cells snapping to one schematic do not stamp it twice.
	std::unordered_set<std::pair<int, int>, XZCellHash> claimed_tree_slots;
	// Sparse, tile-local road elevations. Keep these separate from the terrain
	// sample cache so tile results can transfer only their own road surfaces.
	using RoadSurfaceOverrides = std::unordered_map<std::pair<int, int>, int, XZCellHash>;
	// Overrides emitted by the currently open tile are visible to that tile's
	// terrain passes, then transferred to the committed map at ordered merge.
	RoadSurfaceOverrides road_surface_overrides;
	RoadSurfaceOverrides active_road_surface_overrides;
	bool tile_active = false;
	using WrittenCellSet = std::unordered_set<std::tuple<int, int, int>, FrameCellHash>;
	WrittenCellSet written_cells;
	WrittenCellSet tile_merged_written_cells;
	bool tile_scope_active = false;
	// Effective terrain elevation cache. The dense part covers the mapchunk and
	// its OSM halo; unusual out-of-halo queries use the sparse fallback. Road
	// surface overrides are kept separately above and take precedence.
	struct GroundLevelCache
	{
		std::unique_ptr<std::atomic<int>[]> dense;
		std::size_t dense_size = 0;
		mutable std::mutex overflow_mutex;
		std::unordered_map<std::pair<int, int>, int, XZCellHash> overflow;
	};
	// Tile workers share immutable terrain samples and publish lazy cache entries
	// atomically, matching Rust's Arc-backed Ground and avoiding a dense-cache copy
	// per independent tile editor.
	std::shared_ptr<GroundLevelCache> ground_level_cache =
			std::make_shared<GroundLevelCache>();
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
	void set_schematic_tree_pack_available(bool available)
	{
		schematic_tree_pack_available = available;
	}
	bool has_schematic_tree_pack() const { return schematic_tree_pack_available; }
	int get_tree_slot_spacing() const { return tree_slot_spacing_blocks; }
	int tree_slot_spacing() const { return tree_slot_spacing_blocks; }
	void set_ground_origin(int x, int z);
	void reserve_ground_level_cache();
	XZPoint ground_point(int x, int z) const;
	void set_regional_tree_placer(
			std::function<std::optional<bool>(WorldEditor &, int, int, int, std::uint8_t,
					std::optional<trees::Habitat>, bool, bool, bool, bool)>
					placer,
			bool thread_safe = false)
	{
		regional_tree_placer = std::move(placer);
		regional_tree_placer_thread_safe = thread_safe;
	}
	bool regional_tree_placer_thread_safe = false;
	std::optional<bool> place_regional_tree(int x, int y, int z, std::uint8_t cover,
			std::optional<trees::Habitat> habitat_hint = std::nullopt,
			bool tagged = false, bool wet_ground = false, bool allow_on_paved = false,
			bool density_decided = false)
	{
		if (!regional_tree_placer)
			return std::nullopt;
		return regional_tree_placer(*this, x, y, z, cover, habitat_hint, tagged,
				wet_ground, allow_on_paved, density_decided);
	}
	void set_mapped_regional_tree_placer(
			decltype(mapped_regional_tree_placer) placer, bool thread_safe = false)
	{
		mapped_regional_tree_placer = std::move(placer);
		mapped_regional_tree_placer_thread_safe = thread_safe;
	}
	bool mapped_regional_tree_placer_thread_safe = false;
	std::optional<bool> place_mapped_regional_tree(
			int x, int y, int z, std::uint8_t cover, const trees::MappedRequest &request)
	{
		if (!mapped_regional_tree_placer)
			return std::nullopt;
		return mapped_regional_tree_placer(*this, x, y, z, cover, request);
	}
	void set_start_with_map(bool v) { start_with_map = v; }
	void set_map_decals(bool v) { map_decals = v; }
	bool map_decals_enabled() const { return map_decals; }
	void set_first_decal_map_id(int id) { first_decal_map_id = id; }
	void set_decal_registry(std::shared_ptr<const decals::DecalRegistry> registry)
	{
		decal_registry = std::move(registry);
	}
	void set_signage_context(std::shared_ptr<const signage::SignageContext> context)
	{
		signage_context = std::move(context);
	}
	void set_decal_frame_sink(std::function<bool(WorldEditor &, const DecalFrame &)> sink,
			bool thread_safe = false)
	{
		decal_frame_sink = std::move(sink);
		decal_frame_sink_thread_safe = thread_safe;
	}
	void set_facade_panel_sink(std::function<bool(int, int, int, std::int8_t,
					const std::vector<std::uint8_t> &, std::uint32_t, std::uint32_t)>
					sink)
	{
		facade_panel_sink = std::move(sink);
	}
	bool place_facade_panel(int x, int y, int z, std::int8_t facing,
			std::vector<std::uint8_t> pixels, std::uint32_t width, std::uint32_t height);
	void clear_facade_panels() { placed_facade_panels.clear(); }
	const std::vector<FacadePanel> &facade_panels() const { return placed_facade_panels; }
	void record_facade_panel(int x, int y, int z, std::int8_t facing,
			std::vector<std::uint8_t> pixels, std::uint32_t width, std::uint32_t height,
			const std::string &item_name = {});
	void set_chest_sink(std::function<void(int, int, int,
					const std::vector<std::tuple<std::string, int, int>> &)>
					sink)
	{
		chest_sink = std::move(sink);
	}
	void set_chest_with_items_absolute(int x, int y, int z,
			const std::vector<std::tuple<std::string, int, int>> &items);
	void set_barrel_sink(std::function<void(int, int, int,
					const std::vector<std::tuple<std::string, int, int>> &)>
					sink)
	{
		barrel_sink = std::move(sink);
	}
	void set_barrel_nbt_sink(
			std::function<void(int, int, int, const std::vector<nlohmann::json> &)> sink)
	{
		barrel_nbt_sink = std::move(sink);
	}
	void set_barrel_with_items_absolute(int x, int y, int z,
			const std::vector<std::tuple<std::string, int, int>> &items);
	void set_barrel_with_nbt_items_absolute(
			int x, int y, int z, const std::vector<nlohmann::json> &items);
	void set_bed_sink(std::function<void(int, int, int)> sink)
	{
		bed_sink = std::move(sink);
	}
	void set_fluid_tick_sink(std::function<void(const Block &, pos_t, pos_t, pos_t)> sink)
	{
		fluid_tick_sink = std::move(sink);
	}
	void schedule_fluid_tick(const Block &fluid, pos_t x, pos_t y, pos_t z)
	{
		if ((fluid == block_definitions::WATER || fluid == block_definitions::LAVA) &&
				fluid_tick_sink)
			fluid_tick_sink(fluid, x, y, z);
	}
	void set_schem_entity_sink(std::function<bool(std::optional<std::uint64_t>, int, int,
									   int, const std::vector<std::uint8_t> &)>
									   sink,
			bool thread_safe = false)
	{
		schem_entity_sink = std::move(sink);
		schem_entity_sink_thread_safe = thread_safe;
	}
	void retain_schematic_entity(
			int x, int y, int z, const std::vector<std::uint8_t> &nbt)
	{
		if (!owns(x, z))
			return;
		if (schem_entity_sink && schem_entity_sink(active_tile_overlay, x, y, z, nbt))
			return;
		placed_schematic_entities.push_back({x, y, z, nbt});
	}
	const std::vector<PlacedSchematicEntity> &get_placed_schematic_entities() const
	{
		return placed_schematic_entities;
	}
	void set_item_frame_sink(std::function<void(int, int, int, const std::string &)> sink)
	{
		item_frame_sink = std::move(sink);
	}
	void set_entity_sink(std::function<bool(const nlohmann::json &)> sink)
	{
		entity_sink = std::move(sink);
	}
	bool has_entity_sink() const { return static_cast<bool>(entity_sink); }
	bool add_entity(const std::string &id, int x, int y, int z,
			const nlohmann::json &extra = nlohmann::json::object());
	bool add_entity_exact(const std::string &id, double x, double absolute_y, double z,
			std::int64_t seed, nlohmann::json extra);
	bool add_item_display(double x, double absolute_y, double z, std::int64_t seed,
			nlohmann::json extra);
	bool add_block_display(double x, double absolute_y, double z, std::int64_t seed,
			nlohmann::json extra)
	{
		return add_entity_exact(
				"minecraft:block_display", x, absolute_y, z, seed, std::move(extra));
	}
	const std::vector<nlohmann::json> &get_placed_entities() const
	{
		return placed_entities;
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
		if (!block_exists_absolute(x, y, z) || !owns(x, z))
			return;
		if (bed_sink) {
			bed_sink(x, y, z);
			return;
		}
		placed_block_entities.push_back({x, y, z,
				{{"id", "minecraft:bed"}, {"x", x}, {"y", y}, {"z", z},
						{"keepPacked", 0}}});
	}
	const std::vector<PlacedBlockEntity> &get_placed_block_entities() const
	{
		return placed_block_entities;
	}
	// Inclusive tile ownership bounds. Geometry may write through the tile halo;
	// owns() is reserved for single-owner side effects and undergrowth filtering.
	void set_strict_bounds(int min_x, int min_z, int max_x, int max_z)
	{
		strict_bounds = std::tuple{min_x, min_z, max_x, max_z};
	}
	void clear_strict_bounds() { strict_bounds.reset(); }
	bool owns(int x, int z) const;
	void register_hedge(int x, int z) { hedge_columns.emplace(x, z); }
	bool is_hedge(int x, int z) const { return hedge_columns.contains({x, z}); }
	bool claim_tree_slot(int x, int z) { return claimed_tree_slots.emplace(x, z).second; }
	bool signage_enabled() const { return map_decals && bool(decal_registry); }
	bool place_sign_node(Block sign, int x, int y, int z, std::int8_t param2,
			const std::string &text = {});
	bool place_text_sign(
			int x, int y, int z, std::int8_t facing, const std::string &text, bool steel);
	static std::tuple<int, int, int> decal_frame_cell(
			int x, int y, int z, std::int8_t facing);
	bool cell_has_frame(int x, int y, int z) const;
	bool place_map_decal_ex(int x, int y, int z, std::int8_t facing, int map_id,
			std::int8_t rotation = 0, bool glow = false, bool require_air = false);
	static std::tuple<int, int, int, int> panel_axes(std::int8_t facing);
	bool place_decal_panel(int x, int y, int z, std::int8_t facing,
			const decals::DecalKey &key, bool glow = false, bool require_hosts = false);
	bool place_decal(
			int x, int y, int z, std::int8_t facing, const decals::DecalKey &key);
	static std::pair<int, int> panel_left_anchor(
			int x, int z, std::int8_t facing, int cols);
	static std::int8_t facing_for_normal(int nx, int nz);
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
	std::pair<double, double> geo_anchor() const;
	void set_output_path(std::filesystem::path p) { output_path = std::move(p); }
	void set_block_properties_sink(
			std::function<void(const BlockWithProperties &, int, int, int)> sink)
	{
		block_properties_sink = std::move(sink);
	}
	void request_flush() { flush_requested = true; }
	void request_save() { save_requested = true; }
	void set_generation_error_sink(std::function<void(const std::string &)> sink)
	{
		generation_error_sink = std::move(sink);
	}
	void report_generation_error(const std::string &message) const
	{
		if (generation_error_sink)
			generation_error_sink(message);
	}
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
	bool finalize_persistence();
	void set_tile_hooks(
			std::function<std::optional<std::uint64_t>(int, int, int, int)> begin_tile,
			std::function<bool(std::uint64_t)> merge_tile, bool thread_safe = false)
	{
		begin_tile_sink = std::move(begin_tile);
		merge_tile_sink = std::move(merge_tile);
		tile_hooks_thread_safe = thread_safe;
	}
	void set_defer_tile_merges(bool defer) { defer_tile_merges = defer; }
	bool flush_deferred_tile_merges()
	{
		bool merged = true;
		if (merge_tile_sink)
			for (const auto id : deferred_tile_overlays)
				merged = merge_tile_sink(id) && merged;
		deferred_tile_overlays.clear();
		return merged;
	}
	bool begin_tile(int min_x, int min_z, int max_x, int max_z)
	{
		tile_active = false;
		active_tile_overlay.reset();
		if (begin_tile_sink) {
			active_tile_overlay = begin_tile_sink(min_x, min_z, max_x, max_z);
			if (!active_tile_overlay)
				return false;
		}
		tile_active = true;
		active_road_surface_overrides.clear();
		active_support_columns.clear();
		active_frame_cells.clear();
		active_placed_frames.clear();
		if (!tile_scope_active) {
			tile_merged_written_cells.clear();
			tile_merged_written_cells.insert(written_cells.begin(), written_cells.end());
			tile_scope_active = true;
		}
		written_cells.clear();
		return true;
	}
	bool merge_tile(int, int, int, int)
	{
		if (merge_tile_sink) {
			if (!active_tile_overlay)
				return false;
			if (defer_tile_merges)
				deferred_tile_overlays.push_back(*active_tile_overlay);
			else if (!merge_tile_sink(*active_tile_overlay))
				return false;
		}
		for (const auto &[coordinate, y] : active_road_surface_overrides)
			road_surface_overrides.insert_or_assign(coordinate, y);
		active_road_surface_overrides.clear();
		for (const auto &[coordinate, block] : active_support_columns)
			support_columns.insert_or_assign(coordinate, block);
		active_support_columns.clear();
		frame_cells.insert(active_frame_cells.begin(), active_frame_cells.end());
		placed_frames.insert(placed_frames.end(), active_placed_frames.begin(),
				active_placed_frames.end());
		active_placed_frames.clear();
		active_frame_cells.clear();
		tile_active = false;
		active_tile_overlay.reset();
		if (tile_scope_active) {
			tile_merged_written_cells.insert(written_cells.begin(), written_cells.end());
			written_cells.clear();
		}
		return true;
	}
	WorldEditor make_tile_worker() const;
	bool tile_worker_callbacks_safe() const;
	bool merge_tile_worker(
			WorldEditor &&worker, int min_x, int min_z, int max_x, int max_z);
	void finish_tile_pass()
	{
		if (!tile_scope_active)
			return;
		tile_merged_written_cells.insert(written_cells.begin(), written_cells.end());
		written_cells.insert(
				tile_merged_written_cells.begin(), tile_merged_written_cells.end());
		tile_merged_written_cells.clear();
		tile_scope_active = false;
	}
	bool flush_requested_now() const { return flush_requested; }
	bool save_requested_now() const { return save_requested; }
	void clear_flush_request() { flush_requested = false; }
	void clear_save_request() { save_requested = false; }
	std::filesystem::path schematic_asset_root;
	std::filesystem::path cave_asset_root;
	void set_schematic_asset_root(std::filesystem::path root)
	{
		schematic_asset_root = std::move(root);
	}
	const std::filesystem::path &get_schematic_asset_root() const
	{
		return schematic_asset_root;
	}
	void set_cave_asset_root(std::filesystem::path root)
	{
		cave_asset_root = std::move(root);
	}
	const std::filesystem::path &get_cave_asset_root() const { return cave_asset_root; }
	Ground *get_ground() const { return ground; }; // may return nullptr

	bool pos_ok(int x, int z) const;
	// Place a block at (x, y, z). The optional adjacency arguments
	// mimic the Rust code’s “Some(&[COBBLESTONE, COBBLESTONE_WALL])” idea.
	void set_block(const Block &block, int x, int y, int z,
			const std::optional<std::vector<Block>> &replace_with = {},
			const std::optional<std::vector<Block>> &avoid = {});

	// Convert a feature-local east/up/north coordinate into engine coordinates.
	// Callers migrating curved placement should use this before absolute writes.
	v3pos_t projection_position(double east, double up, double north) const;

	const v3d &projection_up() const { return projection_frame.up; }

	bool try_set_block_absolute(const Block &block, int x, int y, int z,
			const std::optional<std::vector<Block>> &maybe_variants = {},
			const std::optional<std::vector<Block>> &maybe_replacements = {});

	void set_block_absolute(const Block &block, int x, int y, int z,
			const std::optional<std::vector<Block>> &maybe_variants = {},
			const std::optional<std::vector<Block>> &maybe_replacements = {})
	{
		try_set_block_absolute(block, x, y, z, maybe_variants, maybe_replacements);
	}

	// Place feature-local east/up/north coordinates through the active
	// projection frame. Absolute world writes deliberately remain separate.
	bool try_set_block_local(const Block &block, double east, double up, double north,
			const std::optional<std::vector<Block>> &maybe_variants = {},
			const std::optional<std::vector<Block>> &maybe_replacements = {});

	void set_block_local(const Block &block, double east, double up, double north,
			const std::optional<std::vector<Block>> &maybe_variants = {},
			const std::optional<std::vector<Block>> &maybe_replacements = {})
	{
		try_set_block_local(block, east, up, north, maybe_variants, maybe_replacements);
	}

	void set_block_absolute(const Block &block, int x, int y, int z,
			const std::vector<Block> *variants, const std::vector<Block> *replacements);

	bool try_set_block_with_properties_absolute(const BlockWithProperties &bwp, int32_t x,
			int32_t y, int32_t z, const std::optional<std::vector<Block>> &variants,
			const std::optional<std::vector<Block>> &replacements);

	void set_block_with_properties_absolute(const BlockWithProperties &bwp, int32_t x,
			int32_t y, int32_t z, const std::optional<std::vector<Block>> &variants,
			const std::optional<std::vector<Block>> &replacements);

	void set_block_with_properties_absolute(const BlockWithProperties &bwp, int32_t x,
			int32_t y, int32_t z, const std::vector<Block> *variants,
			const std::vector<Block> *replacements);
	bool check_for_block(
			int x, int y, int z, const std::optional<std::vector<Block>> &blocks);

	// Rust WorldEditor::block_at: true for a present non-air node.  It is kept
	// separate from check_for_block because callers need an occupancy test,
	// rather than a material filter (notably vertically-grown wetland reeds).
	bool block_at(int x, int y, int z) const;

	// Highest occupied absolute Y in an inclusive column interval.  Tree canopy
	// placement samples this before writing leaves, just as Rust does, so a low
	// roof only culls intersecting leaves instead of the complete canopy.
	std::optional<int> highest_block_between(int x, int z, int min_y, int max_y) const;

	//inline auto node_to_xz(const osmium::NodeRef &node)
	auto node_to_xz(const auto &node)
	{
		const auto pos2 = mg->ll_to_pos(
				{static_cast<ll_t>(node.y()) /
								static_cast<ll_t>(osmium::detail::coordinate_precision),
						static_cast<ll_t>(node.x()) /
								static_cast<ll_t>(osmium::detail::coordinate_precision)});
		// TODO: scale y
		return std::make_pair(pos2.X, pos2.Y);
	}

	// Full projected position for feature anchors. The legacy node_to_xz()
	// remains available for flat Arnis algorithms that only accept X/Z.
	inline v3pos_t node_to_position(const auto &node, double altitude = 0.0) const
	{
		return mg->ll_to_pos3({static_cast<ll_t>(node.location().lat()),
									  static_cast<ll_t>(node.location().lon())},
				altitude);
	}

	std::pair<int, int> get_min_coords() const;

	std::pair<int, int> get_max_coords() const;
	int get_absolute_y(int x, int y, int z) const;
	int get_ground_level(int x, int z) const;
	std::optional<int> terrain_level(int x, int z) const;
	void register_road_surface_y(int x, int z, int y);
	RoadSurfaceOverrides take_road_surface_overrides();
	void merge_road_surface_overrides(RoadSurfaceOverrides overrides);
	void merge_road_surface_overrides_in_regions(RoadSurfaceOverrides overrides,
			const std::unordered_set<std::pair<int, int>, XZCellHash> &regions);
	bool water_source_is_enclosed(int x, int z) const;
	biome::Climate climate() const;
	int get_water_level(int x, int z) const;
	bool is_lc_water(int x, int z) const;
	bool is_steep_land(int x, int z) const;
	bool land_cover_backs_trees(int x, int z) const;
	std::optional<std::optional<std::uint8_t>> canopy_top_m(
			int x, int z, int radius) const;
	std::optional<int> canopy_height_hint(int x, int z) const;
	std::optional<double> typical_mapped_tree_height_m() const;
	std::optional<trees::TreeSize> canopy_size_hint(int x, int z) const;
	uint8_t water_distance(int x, int z) const;

	bool check_for_block_absolute(int x, int y, int z,
			const std::optional<std::vector<Block>> &blocks = {},
			const std::optional<std::vector<Block>> &avoid = {}) const;

	bool block_exists_absolute(int x, int y, int z) const;
	bool check_for_block_type_absolute(int x, int y, int z, const Block &block) const;
	// Includes the writable voxel halo; empty without a voxel manipulator.
	std::pair<int, int> writable_y_bounds() const;

	std::optional<Block> get_block_absolute(int x, int y, int z) const;
	std::optional<std::string> block_name_absolute(int x, int y, int z) const;
	std::optional<std::string> block_name(const Block &block) const;

	bool cell_open_at(int x, int y, int z) const;

	bool set_block_if_absent_absolute(const Block &block, int x, int y, int z);
	bool set_block_with_properties_if_absent_absolute(
			const BlockWithProperties &bwp, int x, int y, int z);
	void register_support_column(int x, int z, const Block &block);
	std::optional<Block> support_column(int x, int z) const;

	void fill_column_absolute(
			const Block &block, int x, int z, int min_y, int max_y, bool skip_existing);
	// Fill a column with a height-dependent block, resolving world bounds and
	// column ownership once for the whole pass. Existing non-air cells are kept
	// when skip_existing is true, matching fill_column_absolute.
	void fill_column_with_absolute(int x, int z, int min_y, int max_y,
			const std::function<Block(int)> &block_at, bool skip_existing = true);
	// Fill completely untouched 16x16x16 sections below a terrain surface.
	// Returns true only when every section in the requested range was empty.
	bool bulk_fill_chunk_sections_below(int chunk_x, int chunk_z, int section_y_min,
			int section_y_max, const Block &block);

	void place_wall_banner(const Block &block, int x, int y, int z,
			const std::string &facing, const std::string &base_color,
			const std::vector<std::pair<std::string, std::string>> &patterns);

	void place_wall_banner(int x, int y, int z, const std::string &facing,
			const std::vector<std::pair<std::string, std::string>> &patterns);

	void fill_blocks(const Block &block, std::int32_t x1, std::int32_t y1,
			std::int32_t z1, std::int32_t x2, std::int32_t y2, std::int32_t z2,
			const std::optional<std::vector<Block>> &override_whitelist,
			const std::optional<std::vector<Block>> &override_blacklist);

	void fill_blocks_absolute(const Block &block, std::int32_t x1, std::int32_t y1,
			std::int32_t z1, std::int32_t x2, std::int32_t y2, std::int32_t z2,
			const std::optional<std::vector<Block>> &override_whitelist = {},
			const std::optional<std::vector<Block>> &override_blacklist = {});

	void fill_blocks(const Block &block, std::int32_t x1, std::int32_t y1,
			std::int32_t z1, std::int32_t x2, std::int32_t y2, std::int32_t z2);
};
}

using WorldEditor = world_editor::WorldEditor;
}
