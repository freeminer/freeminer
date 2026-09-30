#include "arnis_world_editor.h"
#include "map.h"

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
	if (!width || !height || pixels.size() != std::size_t(width) * height * 3)
		return false;
	if (facade_panel_sink && !facade_panel_sink(x, y, z, facing, pixels, width, height))
		return false;
	placed_facade_panels.push_back(FacadePanel{x, y, z, facing, width, height, pixels});
	return true;
}
void WorldEditor::set_chest_with_items_absolute(
		int x, int y, int z, const std::vector<std::tuple<std::string, int, int>> &items)
{
	if (try_set_block_absolute(
				block_definitions::CHEST, x, y, z, std::nullopt, std::nullopt) &&
			chest_sink)
		chest_sink(x, y, z, items);
}
void WorldEditor::set_barrel_with_items_absolute(
		int x, int y, int z, const std::vector<std::tuple<std::string, int, int>> &items)
{
	BlockWithProperties barrel{block_definitions::BARREL, {{"facing", "up"}}};
	if (try_set_block_with_properties_absolute(
				barrel, x, y, z, std::nullopt, std::nullopt) &&
			barrel_sink)
		barrel_sink(x, y, z, items);
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
	return mg && x >= mg->node_min.X && x <= mg->node_max.X && z >= mg->node_min.Z &&
		   z <= mg->node_max.Z && owns(x, z);
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
	if (block_properties_sink && !bwp.properties.empty())
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
	return get_block_absolute(x, get_absolute_y(x, y, z), z).has_value();
}
std::optional<int> WorldEditor::highest_block_between(
		int x, int z, int min_y, int max_y) const
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
std::optional<int> WorldEditor::terrain_level(int x, int z) const
{
	return ground ? std::optional<int>(ground->level({x, z})) : std::nullopt;
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
	return ground ? ground->water_level({x, z}) : get_ground_level(x, z);
}
bool WorldEditor::is_lc_water(int x, int z) const
{
	if (ground && ground->has_land_cover()) {
		return ground->cover_class(ground_point(x, z)) == land_cover::LC_WATER;
	}
	if (!mg || !mg->vm)
		return false;
	const v3pos_t pos{static_cast<pos_t>(x), static_cast<pos_t>(get_water_level(x, z)),
			static_cast<pos_t>(z)};
	if (!mg->vm->exists(pos))
		return false;
	return mg->readTileOverlay(pos).value_or(mg->vm->getNode(pos)).getContent() ==
		   block_definitions::WATER.getContent();
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
	// Preserve the whitelist predicate's AIR/IGNORE and ownership semantics.
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
bool WorldEditor::cell_open_at(int x, int y, int z) const
{
	return !get_block_absolute(x, y, z);
}
void WorldEditor::set_block_if_absent_absolute(const Block &block, int x, int y, int z)
{
	if (!check_for_block_absolute(x, y, z))
		set_block_absolute(block, x, y, z);
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
void WorldEditor::place_wall_banner(const Block &block, int x, int y, int z,
		const std::string &facing, const std::string &base_color,
		const std::vector<std::pair<std::string, std::string>> &patterns)
{
	const BlockWithProperties banner{block, {{"facing", facing}}};
	if (try_set_block_with_properties_absolute(
				banner, x, y, z, std::nullopt, std::nullopt) &&
			banner_sink)
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
