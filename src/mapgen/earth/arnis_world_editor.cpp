#include "arnis_world_editor.h"
#include "map.h"
#include "emerge.h"
#include "mapgen/mapgen_earth.h"
#include "arnis-cpp/src/ground_decoration.h"
#include "arnis-cpp/src/floodfill_cache.h"
#include "arnis-cpp/src/trees/mapped.h"
#include "arnis-cpp/src/world_editor/floor_state.h"
#include "arnis-cpp/src/climate.h"
#include "arnis-cpp/src/element_processing/signage.h"

#include <algorithm>
#include <array>
#include <bit>
#include <cmath>
#include <iterator>
#include <limits>
#include <map>
#include <string_view>
#include <tuple>

namespace arnis
{
namespace world_editor
{

bool WorldEditor::under_mapped_crown(int x, int z) const
{
	return mapped_trunks && mapped_trunks->under_crown(x, z);
}

bool WorldEditor::in_clear_corridor(int x, int z) const
{
	return clear_corridors && clear_corridors->contains(x, z);
}

uint8_t WorldEditor::cover_class(int x, int z) const
{
	return ground ? ground->cover_class(ground_point(x, z)) : 0;
}

std::optional<ecoregion::Ecoregion> WorldEditor::ecoregion(int x, int z) const
{
	return ground ? ground->ecoregion(ground_point(x, z)) : std::nullopt;
}

namespace
{
using EntityKey = std::tuple<int, int, int, bool, std::array<std::int32_t, 4>, int>;

std::optional<int> entity_value_to_int(const nlohmann::json &value)
{
	if (value.is_number_unsigned()) {
		const auto number = value.get<std::uint64_t>();
		if (number > static_cast<std::uint64_t>(std::numeric_limits<int>::max()))
			return std::nullopt;
		return static_cast<int>(number);
	}
	if (value.is_number_integer()) {
		const auto number = value.get<std::int64_t>();
		if (number < std::numeric_limits<int>::min() ||
				number > std::numeric_limits<int>::max())
			return std::nullopt;
		return static_cast<int>(number);
	}
	if (value.is_number_float()) {
		const double number = value.get<double>();
		if (!std::isfinite(number))
			return 0;
		return static_cast<int>(
				std::clamp(number, static_cast<double>(std::numeric_limits<int>::min()),
						static_cast<double>(std::numeric_limits<int>::max())));
	}
	return std::nullopt;
}

std::optional<EntityKey> entity_key(const nlohmann::json &entity)
{
	if (!entity.is_object())
		return std::nullopt;
	std::array<std::int32_t, 4> uuid{};
	bool has_uuid = false;
	if (const auto it = entity.find("UUID");
			it != entity.end() && it->is_array() && it->size() == uuid.size()) {
		has_uuid = true;
		for (std::size_t i = 0; i < uuid.size(); ++i) {
			const auto word = entity_value_to_int((*it)[i]);
			if (!word) {
				has_uuid = false;
				break;
			}
			uuid[i] = static_cast<std::int32_t>(*word);
		}
	}
	int facing = -1;
	if (const auto it = entity.find("Facing"); it != entity.end())
		facing = entity_value_to_int(*it).value_or(-1);

	std::array<int, 3> pos{};
	bool has_pos = false;
	if (const auto it = entity.find("Pos");
			it != entity.end() && it->is_array() && it->size() == pos.size()) {
		has_pos = true;
		for (std::size_t i = 0; i < pos.size(); ++i) {
			const auto coordinate = entity_value_to_int((*it)[i]);
			if (!coordinate) {
				has_pos = false;
				break;
			}
			pos[i] = *coordinate;
		}
	}
	if (!has_pos) {
		const auto x = entity.find("x");
		const auto y = entity.find("y");
		const auto z = entity.find("z");
		if (x == entity.end() || y == entity.end() || z == entity.end())
			return std::nullopt;
		const auto ix = entity_value_to_int(*x);
		const auto iy = entity_value_to_int(*y);
		const auto iz = entity_value_to_int(*z);
		if (!ix || !iy || !iz)
			return std::nullopt;
		pos = {*ix, *iy, *iz};
	}
	return EntityKey{pos[0], pos[1], pos[2], has_uuid, uuid, has_uuid ? -1 : facing};
}

void deduplicate_entities(std::vector<nlohmann::json> &entities)
{
	std::map<EntityKey, std::size_t> indexes;
	std::vector<nlohmann::json> unique;
	unique.reserve(entities.size());
	for (auto &entity : entities) {
		const auto key = entity_key(entity);
		if (!key) {
			unique.push_back(std::move(entity));
			continue;
		}
		const bool inserted = indexes.emplace(*key, unique.size()).second;
		if (inserted)
			unique.push_back(std::move(entity));
		// A duplicate is intentionally ignored: Rust's dedup_extend keeps the
		// destination copy and drops an incoming copy from a later tile halo.
	}
	entities = std::move(unique);
}

void deduplicate_block_entities(std::vector<WorldEditor::PlacedBlockEntity> &entities)
{
	using Key = std::tuple<int, int, int, bool, std::array<std::int32_t, 4>, int>;
	std::map<Key, std::size_t> indexes;
	std::vector<WorldEditor::PlacedBlockEntity> unique;
	unique.reserve(entities.size());
	for (auto &entity : entities) {
		std::array<std::int32_t, 4> uuid{};
		bool has_uuid = false;
		if (entity.nbt.is_object()) {
			const auto it = entity.nbt.find("UUID");
			if (it != entity.nbt.end() && it->is_array() && it->size() == uuid.size()) {
				has_uuid = true;
				for (std::size_t i = 0; i < uuid.size(); ++i) {
					const auto word = entity_value_to_int((*it)[i]);
					if (!word) {
						has_uuid = false;
						break;
					}
					uuid[i] = static_cast<std::int32_t>(*word);
				}
			}
		}
		int facing = -1;
		if (!has_uuid && entity.nbt.is_object()) {
			const auto it = entity.nbt.find("Facing");
			if (it != entity.nbt.end())
				facing = entity_value_to_int(*it).value_or(-1);
		}
		const Key key{
				entity.x, entity.y, entity.z, has_uuid, uuid, has_uuid ? -1 : facing};
		const bool inserted = indexes.emplace(key, unique.size()).second;
		if (inserted)
			unique.push_back(std::move(entity));
		// Match Rust's tile-list merge: the destination block entity wins when
		// two tile halos carry the same coordinate/facing identity.
	}
	entities = std::move(unique);
}

void deduplicate_schematic_entities(
		std::vector<WorldEditor::PlacedSchematicEntity> &entities)
{
	using Key = std::tuple<int, int, int>;
	std::map<Key, std::size_t> indexes;
	std::vector<WorldEditor::PlacedSchematicEntity> unique;
	unique.reserve(entities.size());
	for (auto &entity : entities) {
		const Key key{entity.x, entity.y, entity.z};
		const auto [it, inserted] = indexes.emplace(key, unique.size());
		if (inserted)
			unique.push_back(std::move(entity));
		else
			unique[it->second] = std::move(entity);
	}
	entities = std::move(unique);
}
} // namespace

bool WorldEditor::tile_worker_callbacks_safe() const
{
	// These sinks either bypass the tile overlay or have no ownership token,
	// so invoking them from independent tile workers could mutate a backend
	// concurrently and make overlapping tiles order-dependent. The schematic
	// entity sink and decal-frame sink are tile-aware and are safe to retain.
	return !tile_active && !active_tile_overlay &&
		   (!begin_tile_sink || tile_hooks_thread_safe) &&
		   (!merge_tile_sink || tile_hooks_thread_safe) &&
		   (!schem_entity_sink || schem_entity_sink_thread_safe) &&
		   (!decal_frame_sink || decal_frame_sink_thread_safe) &&
		   (!regional_tree_placer || regional_tree_placer_thread_safe) &&
		   (!mapped_regional_tree_placer || mapped_regional_tree_placer_thread_safe) &&
		   !flush_sink && !save_sink && !preview_sink && !map_item_sink &&
		   !world_settings_sink && !block_properties_sink && !chest_sink &&
		   !barrel_sink && !barrel_nbt_sink && !bed_sink && !fluid_tick_sink &&
		   !item_frame_sink && !entity_sink && !banner_sink && !facade_panel_sink;
}

WorldEditor WorldEditor::make_tile_worker() const
{
	WorldEditor worker;
	worker.traffic_signal_index = traffic_signal_index;
	worker.car_network = car_network;
	worker.train_network = train_network;
	worker.rail_heights = rail_heights;
	worker.rail_heights_mutex = rail_heights_mutex;
	worker.sealed_surface = sealed_surface;
	worker.clear_corridors = clear_corridors;
	worker.fuel_index = fuel_index;
	worker.mg = mg;
	worker.projection_frame = projection_frame;
	worker.ground = ground;
	worker.generation_format = generation_format;
	worker.bake_lighting = bake_lighting;
	worker.start_with_map = start_with_map;
	worker.place_schematics_enabled = place_schematics_enabled;
	worker.schematic_tree_pack_available = schematic_tree_pack_available;
	worker.map_decals = map_decals;
	worker.first_decal_map_id = first_decal_map_id;
	worker.tree_slot_spacing_blocks = tree_slot_spacing_blocks;
	worker.tree_crown_columns = tree_crown_columns;
	worker.regional_tree_placer = regional_tree_placer;
	worker.regional_tree_placer_thread_safe = regional_tree_placer_thread_safe;
	worker.mapped_regional_tree_placer = mapped_regional_tree_placer;
	worker.mapped_regional_tree_placer_thread_safe =
			mapped_regional_tree_placer_thread_safe;
	worker.mapped_trunks = mapped_trunks;
	worker.mapped_cranes = mapped_cranes;
	worker.gamemode = gamemode;
	worker.world_time = world_time;
	worker.level_name = level_name;
	worker.projection_name = projection_name;
	worker.output_path = output_path;
	worker.min_lat = min_lat;
	worker.max_lat = max_lat;
	worker.min_lon = min_lon;
	worker.max_lon = max_lon;
	worker.generation_error_sink = generation_error_sink;
	worker.begin_tile_sink = begin_tile_sink;
	worker.merge_tile_sink = merge_tile_sink;
	worker.tile_hooks_thread_safe = tile_hooks_thread_safe;
	worker.defer_tile_merges = true;
	worker.spawn_x = spawn_x;
	worker.spawn_y = spawn_y;
	worker.spawn_z = spawn_z;
	worker.projection_scale = projection_scale;
	worker.schem_entity_sink = schem_entity_sink;
	worker.schem_entity_sink_thread_safe = schem_entity_sink_thread_safe;
	worker.item_frame_sink = item_frame_sink;
	worker.entity_sink = entity_sink;
	worker.banner_sink = banner_sink;
	worker.decal_registry = decal_registry;
	worker.signage_context = signage_context;
	worker.decal_frame_sink = decal_frame_sink;
	worker.decal_frame_sink_thread_safe = decal_frame_sink_thread_safe;
	worker.facade_panel_sink = facade_panel_sink;
	worker.schematic_asset_root = schematic_asset_root;
	worker.cave_asset_root = cave_asset_root;
	worker.ground_level_cache = ground_level_cache;
	worker.ground_cache_min_x = ground_cache_min_x;
	worker.ground_cache_min_z = ground_cache_min_z;
	worker.ground_cache_width = ground_cache_width;
	worker.ground_cache_height = ground_cache_height;
	worker.ground_origin_x = ground_origin_x;
	worker.ground_origin_z = ground_origin_z;
	worker.ground_origin_set = ground_origin_set;
	worker.generation_bounds = generation_bounds;
	return worker;
}

bool WorldEditor::merge_tile_worker(WorldEditor &&worker, int min_x, int min_z, int max_x,
		int max_z, const std::set<std::pair<int, int>> *road_surface_regions)
{
	if (worker.tile_active && !worker.merge_tile(min_x, min_z, max_x, max_z))
		return false;

	if (merge_tile_sink) {
		for (const auto id : worker.deferred_tile_overlays) {
			if (defer_tile_merges)
				deferred_tile_overlays.push_back(id);
			else if (!merge_tile_sink(id))
				return false;
		}
	}
	worker.deferred_tile_overlays.clear();

	for (auto &[cell, y] : worker.road_surface_overrides) {
		const auto &[x, z] = cell;
		const bool owned = x >= min_x && x <= max_x && z >= min_z && z <= max_z;
		const bool outside_world = !generation_bounds || [&] {
			const auto [world_min_x, world_min_z, world_max_x, world_max_z] =
					*generation_bounds;
			return x < world_min_x || x > world_max_x || z < world_min_z ||
				   z > world_max_z;
		}();
		int region_x = x / 512;
		int region_z = z / 512;
		if (x % 512 < 0)
			--region_x;
		if (z % 512 < 0)
			--region_z;
		const bool needed_region = !road_surface_regions ||
								   road_surface_regions->contains({region_x, region_z});
		if ((owned || outside_world) && needed_region)
			road_surface_overrides.insert_or_assign(cell, y);
	}
	for (const auto &cell : worker.lamp_columns) {
		const auto &[x, z] = cell;
		const bool owned = x >= min_x && x <= max_x && z >= min_z && z <= max_z;
		const bool outside_world = !generation_bounds || [&] {
			const auto [world_min_x, world_min_z, world_max_x, world_max_z] =
					*generation_bounds;
			return x < world_min_x || x > world_max_x || z < world_min_z ||
				   z > world_max_z;
		}();
		if (owned || outside_world)
			lamp_columns.push_back(cell);
	}
	worker.lamp_columns.clear();
	for (auto &[cell, block] : worker.support_columns)
		support_columns.insert_or_assign(cell, std::move(block));
	frame_cells.insert(worker.frame_cells.begin(), worker.frame_cells.end());
	placed_frames.insert(placed_frames.end(),
			std::make_move_iterator(worker.placed_frames.begin()),
			std::make_move_iterator(worker.placed_frames.end()));
	placed_block_entities.insert(placed_block_entities.end(),
			std::make_move_iterator(worker.placed_block_entities.begin()),
			std::make_move_iterator(worker.placed_block_entities.end()));
	// Block entities have no UUID: Rust distinguishes same-cell hanging entries by
	// Facing, then retains the final payload for each coordinate/facing identity.
	deduplicate_block_entities(placed_block_entities);
	placed_schematic_entities.insert(placed_schematic_entities.end(),
			std::make_move_iterator(worker.placed_schematic_entities.begin()),
			std::make_move_iterator(worker.placed_schematic_entities.end()));
	// Schematic payloads are block entities; only one payload can own a block.
	deduplicate_schematic_entities(placed_schematic_entities);
	placed_entities.insert(placed_entities.end(),
			std::make_move_iterator(worker.placed_entities.begin()),
			std::make_move_iterator(worker.placed_entities.end()));
	// Rust's Java writer collapses halo duplicates by full UUID (or by cell and
	// Facing for UUID-less entities) while retaining distinct entities sharing
	// one block. Apply the same identity rule when tile-local records are merged.
	deduplicate_entities(placed_entities);
	placed_facade_panels.insert(placed_facade_panels.end(),
			std::make_move_iterator(worker.placed_facade_panels.begin()),
			std::make_move_iterator(worker.placed_facade_panels.end()));
	if (!tile_scope_active) {
		tile_merged_written_cells.clear();
		tile_merged_written_cells.insert(written_cells.begin(), written_cells.end());
		tile_scope_active = true;
	}
	tile_merged_written_cells.insert(worker.tile_merged_written_cells.begin(),
			worker.tile_merged_written_cells.end());
	tile_merged_written_cells.insert(
			worker.written_cells.begin(), worker.written_cells.end());
	auto merge_top_states = [&](const WrittenCellSet &written) {
		for (const auto &cell : written) {
			const auto &[x, y, z] = cell;
			if (!worker.owns(x, z))
				continue;
			if (worker.full_top_state_cells.contains(cell))
				full_top_state_cells.insert(cell);
			else
				full_top_state_cells.erase(cell);
		}
	};
	auto merge_block_properties = [&](const WrittenCellSet &written) {
		for (const auto &cell : written) {
			const auto &[x, y, z] = cell;
			if (!worker.owns(x, z))
				continue;
			if (const auto found = worker.block_properties_by_cell.find(cell);
					found != worker.block_properties_by_cell.end())
				block_properties_by_cell.insert_or_assign(cell, found->second);
			else
				block_properties_by_cell.erase(cell);
		}
	};
	merge_top_states(worker.tile_merged_written_cells);
	merge_top_states(worker.written_cells);
	merge_block_properties(worker.tile_merged_written_cells);
	merge_block_properties(worker.written_cells);
	flush_requested = flush_requested || worker.flush_requested;
	save_requested = save_requested || worker.save_requested;
	return true;
}

void WorldEditor::set_ground_origin(int x, int z)
{
	ground_origin_x = x;
	ground_origin_z = z;
	ground_origin_set = true;
	ground_level_cache = std::make_shared<GroundLevelCache>();
	ground_cache_width = ground_cache_height = 0;
}
void WorldEditor::reserve_ground_level_cache()
{
	ground_level_cache = std::make_shared<GroundLevelCache>();
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
			ground_level_cache->dense = std::make_unique<std::atomic<int>[]>(columns);
			ground_level_cache->dense_size = columns;
			for (std::size_t i = 0; i < columns; ++i)
				ground_level_cache->dense[i].store(
						std::numeric_limits<int>::min(), std::memory_order_relaxed);
			ground_level_cache->overflow.reserve(1024);
			return;
		}
	}
	ground_cache_width = ground_cache_height = 0;
	ground_level_cache->overflow.reserve(4096);
}
XZPoint WorldEditor::ground_point(int x, int z) const
{
	const int origin_x = ground_origin_set ? ground_origin_x : (mg ? mg->node_min.X : 0);
	const int origin_z = ground_origin_set ? ground_origin_z : (mg ? mg->node_min.Z : 0);
	return {x - origin_x, z - origin_z};
}
bool WorldEditor::place_facade_panel(int x, int y, int z, std::int8_t facing,
		std::vector<std::uint8_t> pixels, std::uint32_t width, std::uint32_t height)
{
	// Tile editors render overlapping halos. The owning tile alone records
	// backend-side panel metadata; block writes from halo tiles are merged later.
	if (!owns(x, z) || !width || !height ||
			pixels.size() != std::size_t(width) * height * 3)
		return false;
	if (facade_panel_sink && !facade_panel_sink(x, y, z, facing, pixels, width, height))
		return false;
	record_facade_panel(x, y, z, facing, std::move(pixels), width, height);
	return true;
}
void WorldEditor::record_facade_panel(int x, int y, int z, std::int8_t facing,
		std::vector<std::uint8_t> pixels, std::uint32_t width, std::uint32_t height,
		const std::string &item_name)
{
	if (!width || !height || pixels.size() != std::size_t(width) * height * 3)
		return;
	placed_facade_panels.push_back(
			FacadePanel{x, y, z, facing, width, height, std::move(pixels), item_name});
}
void WorldEditor::set_chest_with_items_absolute(
		int x, int y, int z, const std::vector<std::tuple<std::string, int, int>> &items)
{
	if (!try_set_block_absolute(
				block_definitions::CHEST, x, y, z, std::nullopt, std::nullopt) ||
			!owns(x, z))
		return;
	nlohmann::json nbt_items = nlohmann::json::array();
	for (const auto &[id, slot, count] : items)
		nbt_items.push_back({{"id", id}, {"Slot", slot}, {"Count", count}});
	if (chest_sink) {
		chest_sink(x, y, z, items);
		return;
	}
	placed_block_entities.push_back({x, y, z,
			{{"id", "minecraft:chest"}, {"x", x}, {"y", y}, {"z", z},
					{"Items", std::move(nbt_items)}, {"keepPacked", 0}}});
}
void WorldEditor::set_barrel_with_items_absolute(
		int x, int y, int z, const std::vector<std::tuple<std::string, int, int>> &items)
{
	BlockWithProperties barrel{block_definitions::BARREL, {{"facing", "up"}}};
	if (!try_set_block_with_properties_absolute(
				barrel, x, y, z, std::nullopt, std::nullopt) ||
			!owns(x, z))
		return;
	nlohmann::json nbt_items = nlohmann::json::array();
	for (const auto &[id, slot, count] : items)
		nbt_items.push_back({{"id", id}, {"Slot", slot}, {"Count", count}});
	if (barrel_nbt_sink) {
		barrel_nbt_sink(x, y, z, nbt_items);
	} else if (barrel_sink) {
		barrel_sink(x, y, z, items);
	} else {
		placed_block_entities.push_back({x, y, z,
				{{"id", "minecraft:barrel"}, {"x", x}, {"y", y}, {"z", z},
						{"Items", std::move(nbt_items)}, {"keepPacked", 0}}});
	}
}
void WorldEditor::set_barrel_with_nbt_items_absolute(
		int x, int y, int z, const std::vector<nlohmann::json> &items)
{
	BlockWithProperties barrel{block_definitions::BARREL, {{"facing", "up"}}};
	if (!try_set_block_with_properties_absolute(
				barrel, x, y, z, std::nullopt, std::nullopt))
		return;
	if (!owns(x, z))
		return;
	if (barrel_nbt_sink) {
		barrel_nbt_sink(x, y, z, items);
	} else {
		placed_block_entities.push_back({x, y, z,
				{{"id", "minecraft:barrel"}, {"x", x}, {"y", y}, {"z", z},
						{"Items", items}, {"keepPacked", 0}}});
		if (!barrel_sink)
			return;
		std::vector<std::tuple<std::string, int, int>> simple_items;
		simple_items.reserve(items.size());
		for (const auto &item : items) {
			if (!item.is_object() || !item.contains("id") || !item["id"].is_string())
				continue;
			const int slot = item.value("Slot", 0);
			const int count = item.value("Count", 1);
			simple_items.emplace_back(item["id"].get<std::string>(), slot, count);
		}
		barrel_sink(x, y, z, simple_items);
	}
}
bool WorldEditor::owns(int x, int z) const
{
	if (!strict_bounds)
		return true;
	const auto [min_x, min_z, max_x, max_z] = *strict_bounds;
	return x >= min_x && x <= max_x && z >= min_z && z <= max_z;
}
bool WorldEditor::place_sign_node(
		Block sign, int x, int y, int z, std::int8_t param2, const std::string &text)
{
	if (!owns(x, z) || sign.id() == CONTENT_AIR)
		return false;
	sign.setParam2(static_cast<std::uint8_t>(param2));
	if (!try_set_block_absolute(sign, x, y, z, std::nullopt, std::nullopt))
		return false;
	if (text.empty())
		return true;
	if (!mg || !mg->active_block_data || x < std::numeric_limits<pos_t>::min() ||
			x > std::numeric_limits<pos_t>::max() ||
			y < std::numeric_limits<pos_t>::min() ||
			y > std::numeric_limits<pos_t>::max() ||
			z < std::numeric_limits<pos_t>::min() ||
			z > std::numeric_limits<pos_t>::max())
		return false;
	return mg->queueGeneratedSign(active_tile_overlay,
			{static_cast<pos_t>(x), static_cast<pos_t>(y), static_cast<pos_t>(z)}, text);
}
bool WorldEditor::place_text_sign(
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
bool WorldEditor::add_item_display(
		double x, double absolute_y, double z, std::int64_t seed, nlohmann::json extra)
{
	return add_entity_exact(
			"minecraft:item_display", x, absolute_y, z, seed, std::move(extra));
}

bool WorldEditor::add_entity_exact(const std::string &id, double x, double absolute_y,
		double z, std::int64_t seed, nlohmann::json extra)
{
	if (!std::isfinite(x) || !std::isfinite(absolute_y) || !std::isfinite(z))
		return false;
	constexpr auto min_coord = static_cast<double>(std::numeric_limits<int>::min());
	constexpr auto max_coord = static_cast<double>(std::numeric_limits<int>::max());
	if (x < min_coord || x > max_coord || absolute_y < min_coord ||
			absolute_y > max_coord || z < min_coord || z > max_coord)
		return false;
	const auto bx = static_cast<int>(std::floor(x));
	const auto by = static_cast<int>(std::floor(absolute_y));
	const auto bz = static_cast<int>(std::floor(z));
	if (!owns(bx, bz) || (mg && (bx < mg->node_min.X || bx > mg->node_max.X ||
										bz < mg->node_min.Z || bz > mg->node_max.Z)))
		return false;
	if (!extra.is_object())
		return false;

	// Match Rust's build_deterministic_uuid(id, bx, by, bz, seed), using
	// unsigned operations to make wrapping explicit.
	std::uint64_t hash = 17;
	for (const unsigned char byte : id)
		hash = hash * 31 + byte;
	hash = hash * 31 + static_cast<std::uint64_t>(seed);
	const auto signed_x = static_cast<std::uint64_t>(static_cast<std::int64_t>(bx));
	const auto signed_y = static_cast<std::uint64_t>(static_cast<std::int64_t>(by));
	const auto signed_z = static_cast<std::uint64_t>(static_cast<std::int64_t>(bz));
	const std::uint64_t seed_a = hash ^ (signed_x << 32) ^ (signed_y * 17);
	const std::uint64_t seed_b = std::rotl(hash, 7) ^ (signed_z * 31) ^ (signed_x * 13);
	const auto uuid_word = [](std::uint64_t value) {
		return std::bit_cast<std::int32_t>(static_cast<std::uint32_t>(value));
	};
	nlohmann::json entity = {{"id", id}, {"Pos", {x, absolute_y, z}},
			{"Motion", {0.0, 0.0, 0.0}}, {"Rotation", {0.0f, 0.0f}}, {"OnGround", 1},
			{"FallDistance", 0.0f}, {"Fire", -20}, {"Air", 300}, {"PortalCooldown", 0},
			{"UUID", {uuid_word(seed_a >> 32), uuid_word(seed_a), uuid_word(seed_b >> 32),
							 uuid_word(seed_b)}}};
	for (auto it = extra.begin(); it != extra.end(); ++it)
		entity[it.key()] = it.value();
	if (entity_sink && entity_sink(entity))
		return true;
	placed_entities.push_back(std::move(entity));
	return true;
}
bool WorldEditor::add_entity(
		const std::string &id, int x, int y, int z, const nlohmann::json &extra)
{
	if (!owns(x, z) ||
			(mg && (x < mg->node_min.X || x > mg->node_max.X || z < mg->node_min.Z ||
						   z > mg->node_max.Z)) ||
			!extra.is_object())
		return false;
	const int absolute_y = get_absolute_y(x, y, z);
	std::int64_t face = -1;
	if (extra.contains("Facing") && extra["Facing"].is_number_integer())
		face = extra["Facing"].get<std::int64_t>();

	// Keep the same stable entity identity as Rust's build_deterministic_uuid.
	std::uint64_t hash = 17;
	for (const unsigned char byte : id)
		hash = hash * 31 + byte;
	hash = hash * 31 + static_cast<std::uint64_t>(face);
	const auto signed_x = static_cast<std::uint64_t>(static_cast<std::int64_t>(x));
	const auto signed_y =
			static_cast<std::uint64_t>(static_cast<std::int64_t>(absolute_y));
	const auto signed_z = static_cast<std::uint64_t>(static_cast<std::int64_t>(z));
	const std::uint64_t seed_a = hash ^ (signed_x << 32) ^ (signed_y * 17);
	const std::uint64_t seed_b = std::rotl(hash, 7) ^ (signed_z * 31) ^ (signed_x * 13);
	const auto uuid_word = [](std::uint64_t value) {
		return std::bit_cast<std::int32_t>(static_cast<std::uint32_t>(value));
	};
	nlohmann::json entity = {{"id", id},
			{"Pos", {x + 0.5, static_cast<double>(absolute_y), z + 0.5}},
			{"Motion", {0.0, 0.0, 0.0}}, {"Rotation", {0.0f, 0.0f}}, {"OnGround", 1},
			{"FallDistance", 0.0f}, {"Fire", -20}, {"Air", 300}, {"PortalCooldown", 0},
			{"UUID", {uuid_word(seed_a >> 32), uuid_word(seed_a), uuid_word(seed_b >> 32),
							 uuid_word(seed_b)}}};
	for (auto it = extra.begin(); it != extra.end(); ++it)
		entity[it.key()] = it.value();
	if (entity_sink && entity_sink(entity))
		return true;
	placed_entities.push_back(std::move(entity));
	return true;
}
std::tuple<int, int, int> WorldEditor::decal_frame_cell(
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
bool WorldEditor::cell_has_frame(int x, int y, int z) const
{
	const auto &frames = tile_active ? active_frame_cells : frame_cells;
	return frames.contains({x, y, z});
}
bool WorldEditor::place_map_decal_ex(int x, int y, int z, std::int8_t facing, int map_id,
		std::int8_t rotation, bool glow, bool require_air)
{
	const auto [fx, fy, fz] = decal_frame_cell(x, y, z, facing);
	if (!owns(fx, fz) || fy - get_ground_level(fx, fz) < 1 || cell_has_frame(fx, fy, fz))
		return false;
	if (require_air && check_for_block_absolute(fx, fy, fz, std::nullopt))
		return false;
	const DecalFrame frame{
			fx, fy, fz, facing, std::int8_t((rotation % 8 + 8) % 8), map_id, glow};
	if (decal_frame_sink && !decal_frame_sink(*this, frame))
		return false;
	auto &frames = tile_active ? active_frame_cells : frame_cells;
	frames.insert({fx, fy, fz});
	auto &placements = tile_active ? active_placed_frames : placed_frames;
	placements.push_back(frame);
	return true;
}
std::tuple<int, int, int, int> WorldEditor::panel_axes(std::int8_t facing)
{
	// Luanti's left-handed view reverses Minecraft's horizontal panel axes.
	// Keep tile columns and panel_left_anchor in the same viewer-right direction.
	switch (facing) {
	case 0:
	case 1:
		return {1, 0, 0, 1};
	case 2:
		return {1, 0, -1, 0};
	case 3:
		return {-1, 0, -1, 0};
	case 4:
		return {0, -1, -1, 0};
	default:
		return {0, 1, -1, 0};
	}
}
bool WorldEditor::place_decal_panel(int x, int y, int z, std::int8_t facing,
		const decals::DecalKey &key, bool glow, bool require_hosts)
{
	if (!signage_enabled())
		return false;
	// Prefer one registered sign over a potentially multi-tile entity panel.
	if (!glow &&
			signage::place_native_sign_panel(*this, x, y, z, facing, key, require_hosts))
		return true;
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
					cell_has_frame(fx, fy, fz) || !cell_open_at(fx, fy, fz) ||
					(require_hosts && cell_open_at(hx, hy, hz)))
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
bool WorldEditor::place_decal(
		int x, int y, int z, std::int8_t facing, const decals::DecalKey &key)
{
	return place_decal_panel(x, y, z, facing, key);
}
std::pair<int, int> WorldEditor::panel_left_anchor(
		int x, int z, std::int8_t facing, int cols)
{
	const auto [rx, rz, down_y, floor_z] = panel_axes(facing);
	(void)down_y;
	(void)floor_z;
	const int half = (cols - 1) / 2;
	return {x - rx * half, z - rz * half};
}
std::int8_t WorldEditor::facing_for_normal(int nx, int nz)
{
	return std::abs(nx) >= std::abs(nz) ? (nx >= 0 ? 5 : 4) : (nz >= 0 ? 3 : 2);
}
bool WorldEditor::finalize_persistence()
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
bool WorldEditor::pos_ok(int x, int z) const
{
	// A tile editor's strict bounds identify ownership, not its writable region:
	// Rust lets normal geometry write into the editor halo and resolves overlap
	// when the tile overlay is merged. owns() remains for single-owner side data
	// and undergrowth.
	return mg && x >= mg->node_min.X && x <= mg->node_max.X && z >= mg->node_min.Z &&
		   z <= mg->node_max.Z;
}
void WorldEditor::set_block(const Block &block, int x, int y, int z,
		const std::optional<std::vector<Block>> &replace_with,
		const std::optional<std::vector<Block>> &avoid)
{
	if (!pos_ok(x, z))
		return;
	return set_block_absolute(block, x, get_absolute_y(x, y, z), z, replace_with, avoid);
}
v3pos_t WorldEditor::projection_position(double east, double up, double north) const
{
	if (!projection_frame.curved)
		return {static_cast<pos_t>(std::llround(east)),
				static_cast<pos_t>(std::llround(up)),
				static_cast<pos_t>(std::llround(north))};
	const auto projected = projection_frame.place(east, up, north);
	return {static_cast<pos_t>(std::llround(projected.X)),
			static_cast<pos_t>(std::llround(projected.Y)),
			static_cast<pos_t>(std::llround(projected.Z))};
}
bool WorldEditor::try_set_block_absolute(const Block &block, int x, int y, int z,
		const std::optional<std::vector<Block>> &maybe_variants,
		const std::optional<std::vector<Block>> &maybe_replacements)
{
	// Match Rust WorldToModify: discard writes above the selected world's ceiling,
	// but clamp writes below its floor onto the floor section (bedrock placement
	// relies on this when the host VM exposes a deeper range).
	if (y > world_editor::world_max_y())
		return false;
	y = std::max(y, world_editor::min_y());
	if (x < std::numeric_limits<pos_t>::min() || x > std::numeric_limits<pos_t>::max() ||
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
	// Tile halos overlap so terrain features have context at their edges, but
	// loose plants belong only to the tile that owns their column. Otherwise a
	// halo can merge grass over a neighboring tile's water or trunk.
	if (!owns(x, z) && ground_decoration::is_undergrowth(block))
		return false;

	const auto key = std::tuple{x, y, z};
	const auto overlay = active_tile_overlay
								 ? mg->readTileOverlay(*active_tile_overlay, pos)
								 : std::nullopt;
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
				should_set = std::any_of(maybe_variants->begin(), maybe_variants->end(),
						[content](const Block &b) { return b.getContent() == content; });
			} else {
				should_set = std::none_of(maybe_replacements->begin(),
						maybe_replacements->end(),
						[content](const Block &b) { return b.getContent() == content; });
			}
		}
	}
	if (!should_set)
		return false;

	if (!active_tile_overlay || !mg->writeTileOverlay(*active_tile_overlay, pos, block))
		mg->vm->setNode(pos, block);
	written_cells.insert(key);
	full_top_state_cells.erase(key);
	block_properties_by_cell.erase(key);
	++mg->stat.set;
	return true;
}
bool WorldEditor::try_set_block_local(const Block &block, double east, double up,
		double north, const std::optional<std::vector<Block>> &maybe_variants,
		const std::optional<std::vector<Block>> &maybe_replacements)
{
	const auto pos = projection_position(east, up, north);
	return try_set_block_absolute(
			block, pos.X, pos.Y, pos.Z, maybe_variants, maybe_replacements);
}
void WorldEditor::set_block_absolute(const Block &block, int x, int y, int z,
		const std::vector<Block> *variants, const std::vector<Block> *replacements)
{
	const std::optional<std::vector<Block>> whitelist =
			variants ? std::optional<std::vector<Block>>(*variants) : std::nullopt;
	const std::optional<std::vector<Block>> blacklist =
			replacements ? std::optional<std::vector<Block>>(*replacements)
						 : std::nullopt;
	set_block_absolute(block, x, y, z, whitelist, blacklist);
}
bool WorldEditor::try_set_block_with_properties_absolute(const BlockWithProperties &bwp,
		int32_t x, int32_t y, int32_t z,
		const std::optional<std::vector<Block>> &variants,
		const std::optional<std::vector<Block>> &replacements)
{
	if (!try_set_block_absolute(bwp.block, x, y, z, variants, replacements))
		return false;
	const auto key =
			std::tuple{static_cast<int>(x), static_cast<int>(y), static_cast<int>(z)};
	if (bwp.property_values().empty())
		block_properties_by_cell.erase(key);
	else
		block_properties_by_cell.insert_or_assign(key, bwp.properties);
	// Block states are carried by the tile's block output, but this legacy
	// side-channel is not part of TileOverlay and must only be emitted once.
	if (block_properties_sink && owns(x, z) && !bwp.property_values().empty())
		block_properties_sink(bwp, x, y, z);
	const auto &properties = bwp.property_values();
	const auto type = properties.find("type");
	const auto half = properties.find("half");
	if (owns(x, z) && ((type != properties.end() &&
							   (type->second == "top" || type->second == "double")) ||
							  (half != properties.end() && half->second == "top")))
		full_top_state_cells.emplace(x, y, z);
	return true;
}
void WorldEditor::set_block_with_properties_absolute(const BlockWithProperties &bwp,
		int32_t x, int32_t y, int32_t z,
		const std::optional<std::vector<Block>> &variants,
		const std::optional<std::vector<Block>> &replacements)
{
	try_set_block_with_properties_absolute(bwp, x, y, z, variants, replacements);
}
void WorldEditor::set_block_with_properties_absolute(const BlockWithProperties &bwp,
		int32_t x, int32_t y, int32_t z, const std::vector<Block> *variants,
		const std::vector<Block> *replacements)
{
	const std::optional<std::vector<Block>> whitelist =
			variants ? std::optional<std::vector<Block>>(*variants) : std::nullopt;
	const std::optional<std::vector<Block>> blacklist =
			replacements ? std::optional<std::vector<Block>>(*replacements)
						 : std::nullopt;
	set_block_with_properties_absolute(bwp, x, y, z, whitelist, blacklist);
}
bool WorldEditor::check_for_block(
		int x, int y, int z, const std::optional<std::vector<Block>> &blocks)
{
	if (!pos_ok(x, z))
		return false;
	return check_for_block_absolute(x, get_absolute_y(x, y, z), z, blocks);
}
bool WorldEditor::block_at(int x, int y, int z) const
{
	return check_for_block_absolute(x, get_absolute_y(x, y, z), z);
}
std::optional<int> WorldEditor::highest_block_between(
		int x, int z, int min_y, int max_y) const
{
	if (!mg || !mg->vm)
		return std::nullopt;
	min_y = std::max(min_y, world_editor::min_y());
	max_y = std::min(max_y, world_editor::world_max_y());
	if (min_y > max_y)
		return std::nullopt;
	for (int y = max_y;; --y) {
		if (check_for_block_absolute(x, y, z))
			return y;
		if (y == min_y)
			break;
	}
	return std::nullopt;
}
std::pair<int, int> WorldEditor::get_min_coords() const
{
	return std::make_pair(mg->node_min.X, mg->node_min.Z);
}
std::pair<int, int> WorldEditor::get_max_coords() const
{
	return std::make_pair(mg->node_max.X, mg->node_max.Z);
}
int WorldEditor::get_absolute_y(int x, int y, int z) const
{
	return get_ground_level(x, z) + y;
}
std::optional<int> WorldEditor::road_surface_y(int x, int z) const
{
	if (!active_road_surface_overrides.empty()) {
		const auto road = active_road_surface_overrides.find({x, z});
		if (road != active_road_surface_overrides.end())
			return road->second;
	}
	if (!road_surface_overrides.empty()) {
		const auto road = road_surface_overrides.find({x, z});
		if (road != road_surface_overrides.end())
			return road->second;
	}
	return std::nullopt;
}
std::optional<int> WorldEditor::surface_override(int x, int z) const
{
	return road_surface_y(x, z);
}
int WorldEditor::get_ground_level(int x, int z) const
{
	if (const auto road = road_surface_y(x, z))
		return *road;
	const int local_x = x - ground_cache_min_x;
	const int local_z = z - ground_cache_min_z;
	if (local_x >= 0 && local_z >= 0 && std::size_t(local_x) < ground_cache_width &&
			std::size_t(local_z) < ground_cache_height) {
		const std::size_t index =
				std::size_t(local_z) * ground_cache_width + std::size_t(local_x);
		if (ground_level_cache && index < ground_level_cache->dense_size) {
			auto &cached = ground_level_cache->dense[index];
			const int value = cached.load(std::memory_order_acquire);
			if (value != std::numeric_limits<int>::min())
				return value;
			if (!ground)
				return 0;
			const int level = ground->level(ground->mg && !ground->elevation_enabled
													? XZPoint{x, z}
													: ground_point(x, z));
			int empty = std::numeric_limits<int>::min();
			if (cached.compare_exchange_strong(empty, level, std::memory_order_acq_rel,
						std::memory_order_acquire))
				return level;
			return empty;
		}
	}
	const std::pair<int, int> position{x, z};
	if (ground_level_cache) {
		std::lock_guard<std::mutex> lock(ground_level_cache->overflow_mutex);
		if (const auto it = ground_level_cache->overflow.find(position);
				it != ground_level_cache->overflow.end())
			return it->second;
	}
	if (!ground)
		return 0;
	const int level =
			ground->level(ground->mg && !ground->elevation_enabled ? XZPoint{x, z}
																   : ground_point(x, z));
	if (ground_level_cache) {
		std::lock_guard<std::mutex> lock(ground_level_cache->overflow_mutex);
		return ground_level_cache->overflow.emplace(position, level).first->second;
	}
	return level;
}
std::optional<int> WorldEditor::terrain_level(int x, int z) const
{
	if (!ground)
		return std::nullopt;
	return ground->level(ground->mg && !ground->elevation_enabled ? XZPoint{x, z}
																  : ground_point(x, z));
}
void WorldEditor::register_road_surface_y(int x, int z, int y)
{
	auto &overrides =
			tile_active ? active_road_surface_overrides : road_surface_overrides;
	overrides.insert_or_assign({x, z}, y);
}
WorldEditor::RoadSurfaceOverrides WorldEditor::take_road_surface_overrides()
{
	return std::exchange(
			tile_active ? active_road_surface_overrides : road_surface_overrides, {});
}
void WorldEditor::merge_road_surface_overrides(RoadSurfaceOverrides overrides)
{
	for (auto &[coordinate, y] : overrides)
		road_surface_overrides.insert_or_assign(coordinate, y);
}
void WorldEditor::merge_road_surface_overrides_in_regions(RoadSurfaceOverrides overrides,
		const std::unordered_set<std::pair<int, int>, XZCellHash> &regions)
{
	for (auto &[coordinate, y] : overrides) {
		const auto &[x, z] = coordinate;
		if (regions.contains({x >> 9, z >> 9}))
			road_surface_overrides.insert_or_assign(coordinate, y);
	}
}

void WorldEditor::release_road_surface_overrides(int region_x, int region_z)
{
	const auto x0 = std::int64_t(region_x) * 512;
	const auto z0 = std::int64_t(region_z) * 512;
	const auto erase_region = [&](RoadSurfaceOverrides &overrides) {
		for (std::int64_t z = z0; z < z0 + 512; ++z)
			for (std::int64_t x = x0; x < x0 + 512; ++x)
				overrides.erase({static_cast<int>(x), static_cast<int>(z)});
	};
	erase_region(road_surface_overrides);
	erase_region(active_road_surface_overrides);
}

void WorldEditor::release_road_surface_overrides()
{
	RoadSurfaceOverrides{}.swap(road_surface_overrides);
	RoadSurfaceOverrides{}.swap(active_road_surface_overrides);
}

bool WorldEditor::water_source_is_enclosed(int x, int z) const
{
	const int base = get_ground_level(x, z);
	return get_ground_level(x + 1, z) >= base && get_ground_level(x - 1, z) >= base &&
		   get_ground_level(x, z + 1) >= base && get_ground_level(x, z - 1) >= base;
}
biome::Climate WorldEditor::climate() const
{
	if (std::isfinite(min_lat) && std::isfinite(max_lat) && std::isfinite(min_lon) &&
			std::isfinite(max_lon) && max_lat > min_lat && max_lon > min_lon)
		return climate::classify_bbox(min_lat, min_lon, max_lat, max_lon);
	return ground ? ground->climate() : biome::Climate::Temperate;
}
std::pair<double, double> WorldEditor::geo_anchor() const
{
	return {(min_lat + max_lat) * .5, (min_lon + max_lon) * .5};
}
int WorldEditor::get_water_level(int x, int z) const
{
	return ground ? ground->water_level(ground->mg && !ground->elevation_enabled
												? XZPoint{x, z}
												: ground_point(x, z))
				  : 0;
}
bool WorldEditor::is_lc_water(int x, int z) const
{
	// Match Rust's is_lc_water: this answers whether the terrain source marks
	// the column as ESA water, not whether an earlier generation pass wrote a
	// water block there.  The distinction matters while reconstructing water
	// areas, before their own fill decisions have completed.
	return ground && cover_class(x, z) == land_cover::LC_WATER;
}
bool WorldEditor::is_steep_land(int x, int z) const
{
	if (!ground)
		return false;
	const auto point = ground_point(x, z);
	const auto cover = cover_class(x, z);
	return cover != 0 && cover != land_cover::LC_WATER && ground->slope(point) > 4;
}
bool WorldEditor::land_cover_backs_trees(int x, int z) const
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
	const auto cover = cover_class(x, z);
	return cover == land_cover::LC_TREE_COVER || cover == land_cover::LC_SHRUBLAND;
}
std::optional<trees::TreeSize> WorldEditor::canopy_size_hint(int x, int z) const
{
	if (!ground)
		return std::nullopt;
	const auto height = ground->canopy_height_m(ground_point(x, z));
	if (!height || *height < canopy::CANOPY_MIN_M)
		return std::nullopt;
	return trees::size_for_height(*height);
}
std::optional<int> WorldEditor::canopy_height_hint(int x, int z) const
{
	const int radius = std::max(1, static_cast<int>(std::lround(2.0 * scale())));
	const auto top = canopy_top_m(x, z, radius);
	if (!top)
		return std::nullopt;
	std::optional<std::uint8_t> measured_top = *top;
	if (!measured_top) {
		const int wider =
				std::max(radius + 1, static_cast<int>(std::lround(8.0 * scale())));
		if (const auto neighbours = canopy_top_m(x, z, wider); neighbours && *neighbours)
			measured_top = **neighbours;
	}
	double height_m =
			4.0 +
			4.0 * double(land_cover::coord_hash(x ^ 0x9A3, z ^ 0x51F) % 1000) / 1000.0;
	if (measured_top) {
		if (*measured_top < canopy::CANOPY_MIN_M)
			return std::nullopt;
		height_m = *measured_top;
	}
	return std::max(1, static_cast<int>(std::lround(height_m * scale())));
}
std::optional<std::optional<std::uint8_t>> WorldEditor::canopy_top_m(
		int x, int z, int radius) const
{
	if (!ground || !ground->has_canopy())
		return std::nullopt;
	bool measured = false;
	std::optional<std::uint8_t> top;
	for (int dz = -radius; dz <= radius; ++dz)
		for (int dx = -radius; dx <= radius; ++dx)
			if (const auto height =
							ground->canopy_height_m(ground_point(x + dx, z + dz))) {
				measured = true;
				if (*height >= canopy::CANOPY_MIN_M)
					top = std::max(top, height);
			}
	if (!measured)
		return std::nullopt;
	return std::optional<std::optional<std::uint8_t>>(std::in_place, top);
}
std::optional<double> WorldEditor::typical_mapped_tree_height_m() const
{
	return mapped_trunks ? mapped_trunks->typical_height_m() : std::nullopt;
}
uint8_t WorldEditor::water_distance(int x, int z) const
{
	if (ground && ground->has_land_cover())
		return ground->water_distance(ground_point(x, z));
	return 0;
}
bool WorldEditor::check_for_block_absolute(int x, int y, int z,
		const std::optional<std::vector<Block>> &blocks,
		const std::optional<std::vector<Block>> &avoid) const
{
	if (y > world_editor::world_max_y())
		return false;
	y = std::max(y, world_editor::min_y());
	const v3pos_t pos{
			static_cast<pos_t>(x), static_cast<pos_t>(y), static_cast<pos_t>(z)};
	if (!mg || !mg->vm || !pos_ok(x, z) || !mg->vm->exists(pos))
		return false;
	++mg->stat.check;
	const auto overlay = active_tile_overlay
								 ? mg->readTileOverlay(*active_tile_overlay, pos)
								 : std::nullopt;
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
bool WorldEditor::block_exists_absolute(int x, int y, int z) const
{
	return check_for_block_absolute(x, y, z);
}
std::pair<int, int> WorldEditor::writable_y_bounds() const
{
	if (!mg || !mg->vm)
		return {1, 0};
	return {mg->vm->m_area.MinEdge.Y, mg->vm->m_area.MaxEdge.Y};
}
bool WorldEditor::check_for_block_type_absolute(
		int x, int y, int z, const Block &block) const
{
	// Preserve the whitelist predicate's AIR/IGNORE semantics.
	const auto current = get_block_absolute(x, y, z);
	if (!current)
		return false;
	++mg->stat.check;
	const auto content = current->getContent();
	return content != CONTENT_AIR && content != CONTENT_IGNORE &&
		   content == block.getContent();
}
std::optional<Block> WorldEditor::get_block_absolute(int x, int y, int z) const
{
	if (y > world_editor::world_max_y())
		return std::nullopt;
	y = std::max(y, world_editor::min_y());
	if (!mg || !mg->vm || !pos_ok(x, z))
		return std::nullopt;
	const v3pos_t pos{
			static_cast<pos_t>(x), static_cast<pos_t>(y), static_cast<pos_t>(z)};
	if (!mg->vm->exists(pos))
		return std::nullopt;
	const auto overlay = active_tile_overlay
								 ? mg->readTileOverlay(*active_tile_overlay, pos)
								 : std::nullopt;
	if (!overlay && !written_cells.contains({x, y, z}))
		return std::nullopt;
	return Block(overlay.value_or(mg->vm->getNode(pos)));
}
std::optional<BlockWithProperties> WorldEditor::get_block_with_properties_absolute(
		int x, int y, int z) const
{
	const auto block = get_block_absolute(x, y, z);
	if (!block)
		return std::nullopt;
	if (const auto found = block_properties_by_cell.find({x, y, z});
			found != block_properties_by_cell.end())
		return BlockWithProperties{*block, found->second};
	if (const auto properties = block->properties())
		return BlockWithProperties{*block, *properties};
	return BlockWithProperties{*block};
}
std::optional<std::string> WorldEditor::block_name_absolute(int x, int y, int z) const
{
	const auto block = get_block_absolute(x, y, z);
	return block ? block_name(*block) : std::nullopt;
}
std::optional<std::string> WorldEditor::block_name(const Block &block) const
{
	if (!mg || !mg->m_emerge || !mg->m_emerge->ndef)
		return std::nullopt;
	const auto &name = mg->m_emerge->ndef->get(block.id()).name;
	return name.empty() ? std::nullopt : std::optional<std::string>{name};
}
bool WorldEditor::cell_open_at(int x, int y, int z) const
{
	const auto block = get_block_absolute(x, y, z);
	if (!block || block->getContent() == CONTENT_AIR ||
			block->getContent() == CONTENT_IGNORE)
		return true;
	const auto name = block_name_absolute(x, y, z);
	if (!name)
		return false;
	// Rust checks the canonical Minecraft block name. Freeminer node names vary
	// by backend, so compare the semantic suffix and retain aliases used by the
	// supported Luanti games.
	const std::string_view full_name = *name;
	const auto separator = full_name.find(':');
	const auto short_name = separator == std::string_view::npos
									? full_name
									: full_name.substr(separator + 1);
	static constexpr std::array<std::string_view, 44> passable{"grass", "short_grass",
			"tall_grass", "fern", "large_fern", "dead_bush", "seagrass", "tall_seagrass",
			"snow", "dandelion", "poppy", "blue_orchid", "azure_bluet", "cornflower",
			"oxeye_daisy", "allium", "lily_of_the_valley", "sweet_berry_bush",
			"sunflower", "lilac", "rose_bush", "peony", "brown_mushroom", "red_mushroom",
			"sugar_cane", "lily_pad", "grass_3", "tallgrass", "double_grass",
			"double_grass_top", "fern_2", "fern_3", "dry_shrub", "papyrus", "waterlily",
			"geranium", "dandelion_yellow", "dandelion_white", "mushroom_brown",
			"mushroom_red", "flower_tulip", "sweet_berry_bush_1", "sweet_berry_bush_2",
			"sweet_berry_bush_3"};
	return std::find(passable.begin(), passable.end(), short_name) != passable.end() ||
		   short_name.ends_with("_carpet") || short_name.ends_with("_tulip");
}
bool WorldEditor::set_block_if_absent_absolute(const Block &block, int x, int y, int z)
{
	if (!check_for_block_absolute(x, y, z))
		return try_set_block_absolute(block, x, y, z);
	return false;
}

bool WorldEditor::set_block_with_properties_if_absent_absolute(
		const BlockWithProperties &bwp, int x, int y, int z)
{
	// Rust's set_with_props_if_absent writes only to an AIR cell. Passing AIR as
	// the whitelist preserves that contract while retaining block properties.
	const std::optional<std::vector<Block>> air_only{{block_definitions::AIR}};
	return try_set_block_with_properties_absolute(bwp, x, y, z, air_only, std::nullopt);
}

void WorldEditor::register_support_column(int x, int z, const Block &block)
{
	auto &columns = tile_active ? active_support_columns : support_columns;
	columns.insert_or_assign({x, z}, block);
}

std::optional<Block> WorldEditor::support_column(int x, int z) const
{
	const auto &columns = tile_active ? active_support_columns : support_columns;
	if (columns.empty())
		return std::nullopt;
	const auto it = columns.find({x, z});
	return it == columns.end() ? std::nullopt : std::optional<Block>(it->second);
}
void WorldEditor::fill_column_absolute(
		const Block &block, int x, int z, int min_y, int max_y, bool skip_existing)
{
	if (min_y > world_editor::world_max_y() || !mg || !mg->vm || !pos_ok(x, z))
		return;
	min_y = std::clamp(min_y, world_editor::min_y(), world_editor::world_max_y());
	max_y = std::clamp(max_y, world_editor::min_y(), world_editor::world_max_y());
	if (max_y < min_y)
		return;
	static const std::optional<std::vector<Block>> replace_any{std::vector<Block>{}};
	for (int y = min_y; y <= max_y; ++y) {
		if (skip_existing && check_for_block_absolute(x, y, z))
			continue;
		try_set_block_absolute(block, x, y, z, std::nullopt, replace_any);
	}
}

void WorldEditor::fill_column_with_absolute(int x, int z, int min_y, int max_y,
		const std::function<Block(int)> &block_at, bool skip_existing)
{
	if (!block_at || min_y > world_editor::world_max_y() || !mg || !mg->vm ||
			!pos_ok(x, z))
		return;
	min_y = std::clamp(min_y, world_editor::min_y(), world_editor::world_max_y());
	max_y = std::clamp(max_y, world_editor::min_y(), world_editor::world_max_y());
	if (max_y < min_y)
		return;
	for (int y = min_y; y <= max_y; ++y) {
		const Block block = block_at(y);
		if (y < std::numeric_limits<pos_t>::min() ||
				y > std::numeric_limits<pos_t>::max())
			continue;
		const v3pos_t pos{
				static_cast<pos_t>(x), static_cast<pos_t>(y), static_cast<pos_t>(z)};
		if (!mg->vm->exists(pos)) {
			++mg->stat.miss;
			continue;
		}
		const auto key = std::tuple{x, y, z};
		const auto overlay = active_tile_overlay
									 ? mg->readTileOverlay(*active_tile_overlay, pos)
									 : std::nullopt;
		if (skip_existing) {
			++mg->stat.check;
			// Rust's WorldToModify::fill_column_with only fills AIR. The emerged
			// Freeminer VM may already contain nodes that were not written through
			// this editor, so written_cells alone is not enough to decide whether
			// the position is occupied. The tile overlay, when present, remains the
			// authoritative view of writes made by a parallel generation tile.
			const auto content = overlay.value_or(mg->vm->getNode(pos)).getContent();
			if (content != CONTENT_AIR && content != CONTENT_IGNORE)
				continue;
		}
		if (!active_tile_overlay ||
				!mg->writeTileOverlay(*active_tile_overlay, pos, block))
			mg->vm->setNode(pos, block);
		written_cells.insert(key);
		++mg->stat.set;
	}
}

bool WorldEditor::bulk_fill_chunk_sections_below(int chunk_x, int chunk_z,
		int section_y_min, int section_y_max, const Block &block)
{
	if (section_y_max < section_y_min)
		return true;
	if (!mg || !mg->vm)
		return false;

	const auto x0 = static_cast<std::int64_t>(chunk_x) * 16;
	const auto z0 = static_cast<std::int64_t>(chunk_z) * 16;
	if (x0 < std::numeric_limits<pos_t>::min() ||
			x0 + 15 > std::numeric_limits<pos_t>::max() ||
			z0 < std::numeric_limits<pos_t>::min() ||
			z0 + 15 > std::numeric_limits<pos_t>::max())
		return false;

	bool all_clean = true;
	for (int section_y = section_y_min; section_y <= section_y_max; ++section_y) {
		const int y0 = section_y * 16;
		bool empty = true;
		bool writable = true;
		for (int y = y0; y < y0 + 16 && empty && writable; ++y) {
			if (y < world_editor::min_y() || y > world_editor::world_max_y()) {
				writable = false;
				break;
			}
			for (int dx = 0; dx < 16 && empty && writable; ++dx) {
				for (int dz = 0; dz < 16; ++dz) {
					const int x = static_cast<int>(x0 + dx);
					const int z = static_cast<int>(z0 + dz);
					const v3pos_t pos{static_cast<pos_t>(x), static_cast<pos_t>(y),
							static_cast<pos_t>(z)};
					if (!pos_ok(x, z) || !mg->vm->exists(pos)) {
						writable = false;
						break;
					}
					const auto overlay =
							active_tile_overlay
									? mg->readTileOverlay(*active_tile_overlay, pos)
									: std::nullopt;
					if (!overlay && !written_cells.contains({x, y, z}))
						continue;
					const auto node = overlay.value_or(mg->vm->getNode(pos));
					const auto content = node.getContent();
					if (content != CONTENT_AIR && content != CONTENT_IGNORE) {
						empty = false;
						break;
					}
				}
			}
		}
		if (!writable || !empty) {
			all_clean = false;
			continue;
		}

		for (int y = y0; y < y0 + 16; ++y)
			for (int dx = 0; dx < 16; ++dx)
				for (int dz = 0; dz < 16; ++dz) {
					const int x = static_cast<int>(x0 + dx);
					const int z = static_cast<int>(z0 + dz);
					const v3pos_t pos{static_cast<pos_t>(x), static_cast<pos_t>(y),
							static_cast<pos_t>(z)};
					if (!active_tile_overlay ||
							!mg->writeTileOverlay(*active_tile_overlay, pos, block))
						mg->vm->setNode(pos, block);
					written_cells.emplace(x, y, z);
					++mg->stat.set;
				}
	}
	return all_clean;
}

void WorldEditor::place_wall_banner(const Block &block, int x, int y, int z,
		const std::string &facing, const std::string &base_color,
		const std::vector<std::pair<std::string, std::string>> &patterns)
{
	const BlockWithProperties banner{block, {{"facing", facing}}};
	if (!try_set_block_with_properties_absolute(
				banner, x, y, z, std::nullopt, std::nullopt) ||
			!owns(x, z))
		return;
	if (banner_sink) {
		banner_sink(x, y, z, facing, base_color, patterns);
		return;
	}
	if (generation_format == 2)
		return; // Rust's Luanti backend has no banner block-entity payload.
	if (generation_format == 1) {
		const auto color_id = [](const std::string &color) {
			static const std::unordered_map<std::string, int> ids{{"black", 0},
					{"red", 1}, {"green", 2}, {"brown", 3}, {"blue", 4}, {"purple", 5},
					{"cyan", 6}, {"light_gray", 7}, {"gray", 8}, {"pink", 9},
					{"lime", 10}, {"yellow", 11}, {"light_blue", 12}, {"magenta", 13},
					{"orange", 14}, {"white", 15}};
			const auto it = ids.find(color);
			return it == ids.end() ? 0 : it->second;
		};
		const auto pattern_code = [](std::string pattern) {
			if (pattern.starts_with("minecraft:"))
				pattern.erase(0, 10);
			static const std::unordered_map<std::string, std::string> codes{{"base", "b"},
					{"square_bottom_left", "bl"}, {"square_bottom_right", "br"},
					{"square_top_left", "tl"}, {"square_top_right", "tr"},
					{"stripe_bottom", "bs"}, {"stripe_top", "ts"}, {"stripe_left", "ls"},
					{"stripe_right", "rs"}, {"stripe_center", "cs"},
					{"stripe_middle", "ms"}, {"stripe_downright", "drs"},
					{"stripe_downleft", "dls"}, {"stripe_small", "ss"}, {"cross", "cr"},
					{"straight_cross", "sc"}, {"triangle_bottom", "bt"},
					{"triangle_top", "tt"}, {"triangles_bottom", "bts"},
					{"triangles_top", "tts"}, {"diagonal_left", "ld"},
					{"diagonal_right", "rd"}, {"diagonal_up_left", "lud"},
					{"diagonal_up_right", "rud"}, {"circle", "mc"}, {"rhombus", "mr"},
					{"half_vertical", "vh"}, {"half_vertical_right", "vhr"},
					{"half_horizontal", "hh"}, {"half_horizontal_bottom", "hhb"},
					{"border", "bo"}, {"curly_border", "cbo"}, {"gradient", "gra"},
					{"gradient_up", "gru"}, {"bricks", "bri"}, {"globe", "glb"},
					{"creeper", "cre"}, {"skull", "sku"}, {"flower", "flo"},
					{"mojang", "moj"}, {"piglin", "pig"}, {"flow", "flw"},
					{"guster", "gus"}};
			const auto it = codes.find(pattern);
			return it == codes.end() ? std::string("b") : it->second;
		};
		nlohmann::json entries = nlohmann::json::array();
		for (const auto &[color, pattern] : patterns)
			entries.push_back(
					{{"Color", color_id(color)}, {"Pattern", pattern_code(pattern)}});
		placed_block_entities.push_back({x, y, z,
				{{"id", "Banner"}, {"x", x}, {"y", y}, {"z", z},
						{"Base", color_id(base_color)}, {"Patterns", std::move(entries)},
						{"Type", 0}}});
		return;
	}
	nlohmann::json entries = nlohmann::json::array();
	for (const auto &[color, pattern] : patterns)
		entries.push_back({{"color", color}, {"pattern", pattern}});
	placed_block_entities.push_back({x, y, z,
			{{"id", "minecraft:banner"}, {"x", x}, {"y", y}, {"z", z}, {"keepPacked", 0},
					{"patterns", std::move(entries)},
					{"components", nlohmann::json::object()}}});
}
void WorldEditor::place_wall_banner(int x, int y, int z, const std::string &facing,
		const std::vector<std::pair<std::string, std::string>> &patterns)
{
	place_wall_banner(block_definitions::LIGHT_GRAY_WALL_BANNER, x, y, z, facing,
			"light_gray", patterns);
}
void WorldEditor::fill_blocks(const Block &block, std::int32_t x1, std::int32_t y1,
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
				this->set_block(block, x, y, z, override_whitelist, override_blacklist);
			}
		}
	}
	++mg->stat.fill;
}
void WorldEditor::fill_blocks_absolute(const Block &block, std::int32_t x1,
		std::int32_t y1, std::int32_t z1, std::int32_t x2, std::int32_t y2,
		std::int32_t z2, const std::optional<std::vector<Block>> &override_whitelist,
		const std::optional<std::vector<Block>> &override_blacklist)
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
void WorldEditor::fill_blocks(const Block &block, std::int32_t x1, std::int32_t y1,
		std::int32_t z1, std::int32_t x2, std::int32_t y2, std::int32_t z2)
{
	return fill_blocks(block, x1, y1, z1, x2, y2, z2, std::optional<std::vector<Block>>{},
			std::optional<std::vector<Block>>{});
}

}

}
