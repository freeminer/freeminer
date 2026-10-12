#include "arnis_world_editor.h"
#include "map.h"
#include "emerge.h"
#include "mapgen/mapgen_earth.h"
#include "arnis-cpp/src/ground_decoration.h"
#include "arnis-cpp/src/world_editor/floor_state.h"

#include <algorithm>
#include <array>
#include <bit>
#include <cmath>
#include <limits>
#include <string_view>

namespace arnis
{
namespace world_editor
{

void WorldEditor::set_ground_origin(int x, int z)
{
	ground_origin_x = x;
	ground_origin_z = z;
	ground_origin_set = true;
	ground_level_cache.clear();
	ground_level_overflow.clear();
	ground_cache_width = ground_cache_height = 0;
}
void WorldEditor::reserve_ground_level_cache()
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
XZPoint WorldEditor::ground_point(int x, int z) const
{
	const int origin_x = ground_origin_set ? ground_origin_x : (mg ? mg->node_min.X : 0);
	const int origin_z = ground_origin_set ? ground_origin_z : (mg ? mg->node_min.Z : 0);
	return {x - origin_x, z - origin_z};
}
bool WorldEditor::place_facade_panel(int x, int y, int z, std::int8_t facing,
		const std::vector<std::uint8_t> &pixels, std::uint32_t width,
		std::uint32_t height)
{
	// Tile editors render overlapping halos. The owning tile alone records
	// backend-side panel metadata; block writes from halo tiles are merged later.
	if (!owns(x, z) || !width || !height ||
			pixels.size() != std::size_t(width) * height * 3)
		return false;
	if (facade_panel_sink && !facade_panel_sink(x, y, z, facing, pixels, width, height))
		return false;
	record_facade_panel(x, y, z, facing, pixels, width, height);
	return true;
}
void WorldEditor::record_facade_panel(int x, int y, int z, std::int8_t facing,
		const std::vector<std::uint8_t> &pixels, std::uint32_t width,
		std::uint32_t height)
{
	if (!width || !height || pixels.size() != std::size_t(width) * height * 3)
		return;
	placed_facade_panels.push_back(FacadePanel{x, y, z, facing, width, height, pixels});
}
void WorldEditor::set_chest_with_items_absolute(
		int x, int y, int z, const std::vector<std::tuple<std::string, int, int>> &items)
{
	if (try_set_block_absolute(
				block_definitions::CHEST, x, y, z, std::nullopt, std::nullopt) &&
			chest_sink && owns(x, z))
		chest_sink(x, y, z, items);
}
void WorldEditor::set_barrel_with_items_absolute(
		int x, int y, int z, const std::vector<std::tuple<std::string, int, int>> &items)
{
	BlockWithProperties barrel{block_definitions::BARREL, {{"facing", "up"}}};
	if (try_set_block_with_properties_absolute(
				barrel, x, y, z, std::nullopt, std::nullopt) &&
			barrel_sink && owns(x, z))
		barrel_sink(x, y, z, items);
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
	} else if (barrel_sink) {
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
	return mg->queueGeneratedSign(
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
	if (!entity_sink || !std::isfinite(x) || !std::isfinite(absolute_y) ||
			!std::isfinite(z))
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

	// Match Rust's build_deterministic_uuid("minecraft:item_display", bx, by,
	// bz, seed), using unsigned operations to make wrapping explicit.
	std::uint64_t hash = 17;
	for (const unsigned char byte : std::string_view("minecraft:item_display"))
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
	nlohmann::json entity = {{"id", "minecraft:item_display"},
			{"Pos", {x, absolute_y, z}}, {"Motion", {0.0, 0.0, 0.0}},
			{"Rotation", {0.0f, 0.0f}}, {"OnGround", 1}, {"FallDistance", 0.0f},
			{"Fire", -20}, {"Air", 300}, {"PortalCooldown", 0},
			{"UUID", {uuid_word(seed_a >> 32), uuid_word(seed_a), uuid_word(seed_b >> 32),
							 uuid_word(seed_b)}}};
	for (auto it = extra.begin(); it != extra.end(); ++it)
		entity[it.key()] = it.value();
	return entity_sink(entity);
}
bool WorldEditor::add_entity(
		const std::string &id, int x, int y, int z, const nlohmann::json &extra)
{
	if (!entity_sink || !owns(x, z) ||
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
	return entity_sink(entity);
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
	return frame_cells.contains({x, y, z});
}
bool WorldEditor::place_map_decal_ex(int x, int y, int z, std::int8_t facing, int map_id,
		std::int8_t rotation, bool glow, bool require_air)
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
std::tuple<int, int, int, int> WorldEditor::panel_axes(std::int8_t facing)
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
bool WorldEditor::place_decal_panel(int x, int y, int z, std::int8_t facing,
		const decals::DecalKey &key, bool glow, bool require_hosts)
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
					frame_cells.contains({fx, fy, fz}) || !cell_open_at(fx, fy, fz) ||
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
	if (!ground || !pos_ok(x, z))
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

	if (!mg->writeTileOverlay(pos, block))
		mg->vm->setNode(pos, block);
	written_cells.insert(key);
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
	// Block states are carried by the tile's block output, but this legacy
	// side-channel is not part of TileOverlay and must only be emitted once.
	if (block_properties_sink && owns(x, z) && !bwp.property_values().empty())
		block_properties_sink(bwp, x, y, z);
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
	if (!ground || !pos_ok(x, z))
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
int WorldEditor::get_ground_level(int x, int z) const
{
	const int local_x = x - ground_cache_min_x;
	const int local_z = z - ground_cache_min_z;
	if (local_x >= 0 && local_z >= 0 && std::size_t(local_x) < ground_cache_width &&
			std::size_t(local_z) < ground_cache_height) {
		const std::size_t index =
				std::size_t(local_z) * ground_cache_width + std::size_t(local_x);
		int &cached = ground_level_cache[index];
		if (cached != std::numeric_limits<int>::min())
			return cached;
		if (!ground)
			return 0;
		// Raster elevation uses coordinates relative to the ground origin;
		// Freeminer's get_height provider takes absolute world coordinates.
		cached = ground->level(ground->mg && !ground->elevation_enabled
									   ? XZPoint{x, z}
									   : ground_point(x, z));
		return cached;
	}
	const std::pair<int, int> position{x, z};
	if (const auto it = ground_level_overflow.find(position);
			it != ground_level_overflow.end())
		return it->second;
	if (!ground)
		return 0;
	const int level =
			ground->level(ground->mg && !ground->elevation_enabled ? XZPoint{x, z}
																   : ground_point(x, z));
	ground_level_overflow.emplace(position, level);
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
	const int local_x = x - ground_cache_min_x;
	const int local_z = z - ground_cache_min_z;
	if (local_x >= 0 && local_z >= 0 && std::size_t(local_x) < ground_cache_width &&
			std::size_t(local_z) < ground_cache_height) {
		ground_level_cache[std::size_t(local_z) * ground_cache_width +
						   std::size_t(local_x)] = y;
		return;
	}
	ground_level_overflow[{x, z}] = y;
}
bool WorldEditor::water_source_is_enclosed(int x, int z) const
{
	const int base = get_ground_level(x, z);
	return get_ground_level(x + 1, z) >= base && get_ground_level(x - 1, z) >= base &&
		   get_ground_level(x, z + 1) >= base && get_ground_level(x, z - 1) >= base;
}
biome::Climate WorldEditor::climate() const
{
	return ground ? ground->climate() : biome::Climate::Temperate;
}
int WorldEditor::get_water_level(int x, int z) const
{
	return ground ? ground->water_level(ground->mg && !ground->elevation_enabled
												? XZPoint{x, z}
												: ground_point(x, z))
				  : get_ground_level(x, z);
}
bool WorldEditor::is_lc_water(int x, int z) const
{
	// Match Rust's is_lc_water: this answers whether the terrain source marks
	// the column as ESA water, not whether an earlier generation pass wrote a
	// water block there.  The distinction matters while reconstructing water
	// areas, before their own fill decisions have completed.
	return ground && ground->cover_class(ground_point(x, z)) == land_cover::LC_WATER;
}
bool WorldEditor::is_steep_land(int x, int z) const
{
	if (!ground)
		return false;
	const auto point = ground_point(x, z);
	const auto cover = ground->cover_class(point);
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
	const auto cover = ground->cover_class(point);
	return cover == land_cover::LC_TREE_COVER || cover == land_cover::LC_SHRUBLAND;
}
std::optional<trees::TreeSize> WorldEditor::canopy_size_hint(int x, int z) const
{
	if (!ground)
		return std::nullopt;
	const auto height = ground->canopy_height_m(ground_point(x, z));
	if (!height || *height < canopy::CANOPY_MIN_M)
		return std::nullopt;
	return trees::size_for_canopy_m(*height);
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

void WorldEditor::register_support_column(int x, int z, const Block &block)
{
	const auto key = (static_cast<std::int64_t>(x) << 32) ^ static_cast<std::uint32_t>(z);
	support_columns[key] = block;
}

std::optional<Block> WorldEditor::support_column(int x, int z) const
{
	const auto key = (static_cast<std::int64_t>(x) << 32) ^ static_cast<std::uint32_t>(z);
	const auto it = support_columns.find(key);
	return it == support_columns.end() ? std::nullopt : std::optional<Block>(it->second);
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
		const auto overlay = mg->readTileOverlay(pos);
		if (skip_existing) {
			++mg->stat.check;
			if (overlay || written_cells.contains(key)) {
				const auto content = overlay.value_or(mg->vm->getNode(pos)).getContent();
				if (content != CONTENT_AIR && content != CONTENT_IGNORE)
					continue;
			}
		}
		if (!mg->writeTileOverlay(pos, block))
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
					const auto overlay = mg->readTileOverlay(pos);
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
					if (!mg->writeTileOverlay(pos, block))
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
	if (try_set_block_with_properties_absolute(
				banner, x, y, z, std::nullopt, std::nullopt) &&
			banner_sink && owns(x, z))
		banner_sink(x, y, z, facing, base_color, patterns);
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
