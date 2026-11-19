#pragma once

#include "arnis_block.h"

#include <algorithm>
#include <array>
#include <optional>
#include <vector>

namespace arnis::voxelibre
{
inline content_t chest_id = CONTENT_IGNORE;
inline content_t bookshelf_id = CONTENT_IGNORE;
inline content_t banner_id = CONTENT_IGNORE;

struct RootedSeagrass
{
	bool enabled = false;
	Block water;
	std::array<Block, 4> substrates;
	std::array<Block, 4> plants;
};

inline RootedSeagrass seagrass;

struct RootedOcean
{
	bool enabled = false;
	bool kelp_enabled = false;
	bool pickle_enabled = false;
	Block water;
	std::array<Block, 4> substrates;
	std::array<Block, 4> kelp;
	std::array<Block, 5> corals;
	std::array<Block, 5> fans;
	Block pickle;
};

inline RootedOcean ocean;

// Convert Java-style structure state to the node variants used by VoxeLibre.
inline std::optional<std::pair<std::string, u8>> node_state(
		std::string name, u8 param2, const BlockProperties &properties)
{
	auto get = [&](const char *key) -> std::string {
		const auto it = properties.find(key);
		return it == properties.end() ? "" : it->second;
	};
	const auto facing = get("facing");
	if (!facing.empty())
		param2 = facing == "east" ? 1 : facing == "south" ? 2 : facing == "west" ? 3 : 0;
	if (name == "mcl_books:chiseled_bookshelf") {
		if (!facing.empty())
			param2 = facing == "north"	? 2
					 : facing == "east" ? 3
					 : facing == "west" ? 1
										: 0;
		return std::pair{name, param2};
	}
	if (name == "mcl_lightning_rods:rod" || name == "mcl_end:end_rod") {
		if (!facing.empty())
			param2 = facing == "down"	 ? 20
					 : facing == "east"	 ? 12
					 : facing == "west"	 ? 16
					 : facing == "north" ? 8
					 : facing == "south" ? 4
										 : 0;
		return std::pair{name, param2};
	}
	if (name.starts_with("mcl_lush_caves:dripleaf_"))
		return std::pair{name, param2};
	const auto half = get("half");
	const auto type = get("type");
	if (name == "mcl_banners:hanging_banner") {
		if (facing == "west")
			param2 = 2;
		else if (facing == "east")
			param2 = 3;
		else if (facing == "south")
			param2 = 4;
		else if (facing == "north")
			param2 = 5;
		return std::pair{name, param2};
	}
	if (name.starts_with("mcl_beds:bed_") &&
			(name.ends_with("_bottom") || name.ends_with("_top"))) {
		const auto part = get("part");
		if (!part.empty()) {
			if (name.ends_with("_bottom"))
				name.resize(name.size() - 7);
			else
				name.resize(name.size() - 4);
			name += part == "head" ? "_top" : "_bottom";
		}
		return std::pair{name, param2};
	}
	if (name.starts_with("mcl_stairs:slab_")) {
		const bool was_top = name.ends_with("_top");
		const bool was_double = name.ends_with("_double");
		if (name.ends_with("_top"))
			name.resize(name.size() - 4);
		else if (name.ends_with("_double"))
			name.resize(name.size() - 7);
		if (type == "top" || (type.empty() && (half == "top" || half == "upper" ||
													  (half.empty() && was_top))))
			name += "_top";
		else if (type == "double" || (type.empty() && half.empty() && was_double))
			name += "_double";
		return std::pair{name, param2};
	}
	if (name.starts_with("mcl_stairs:stair_")) {
		if (half == "top" || half == "upper")
			param2 = static_cast<u8>((param2 % 4) + 20);
		return std::pair{name, param2};
	}
	if (name.starts_with("mcl_doors:") && name.size() > 4 &&
			(name.ends_with("_b_1") || name.ends_with("_t_1"))) {
		const bool upper = half == "upper" || half == "top" ||
						   (half.empty() && name.ends_with("_t_1"));
		const bool mirrored = get("hinge") == "right";
		const bool open = get("open") == "true";
		name.resize(name.size() - 4);
		name += upper ? "_t_" : "_b_";
		name += char('1' + (mirrored ? 2 : 0) + (open ? 1 : 0));
		if (open)
			param2 = static_cast<u8>((param2 + (mirrored ? 3 : 1)) % 4);
		return std::pair{name, param2};
	}
	if ((name.starts_with("mcl_doors:") || name.starts_with("mcl_crimson:")) &&
			name.find("trapdoor") != std::string::npos) {
		const bool was_open = name.ends_with("_open");
		if (name.ends_with("_open"))
			name.resize(name.size() - 5);
		if (get("open") == "true" || (get("open").empty() && was_open))
			name += "_open";
		if (half == "top" || half == "upper")
			param2 = static_cast<u8>((param2 % 4) + 20);
		return std::pair{name, param2};
	}
	return std::nullopt;
}

template <typename Editor>
bool place_rooted(Editor &editor, Block plant, u8 param2, Block water, pos_t x,
		pos_t bed_y, pos_t z, int height = 1)
{
	const auto bed = editor.get_block_absolute(x, bed_y, z);
	if (!bed || plant.id() == CONTENT_IGNORE || plant.id() == CONTENT_AIR)
		return false;
	for (int y = 1; y <= height; ++y) {
		const auto above = editor.get_block_absolute(x, bed_y + y, z);
		if (!above || above->id() != water.id())
			return false;
	}
	plant.setParam2(param2);
	return editor.try_set_block_absolute(
			plant, x, bed_y, z, std::optional<std::vector<Block>>({*bed}), std::nullopt);
}

template <typename Editor>
void place_kelp(Editor &editor, const RootedOcean &nodes, pos_t x, pos_t bed_y, pos_t z,
		int height)
{
	const auto bed = editor.get_block_absolute(x, bed_y, z);
	if (!nodes.enabled || !bed)
		return;
	for (std::size_t i = 0; i < nodes.substrates.size(); ++i) {
		if (bed->id() == nodes.substrates[i].id()) {
			height = std::clamp(height, 1, 15);
			place_rooted(editor, nodes.kelp[i], static_cast<u8>(height * 16), nodes.water,
					x, bed_y, z, height);
			return;
		}
	}
}

// A rooted plant replaces its substrate, leaving the water above untouched.
template <typename Editor>
void place_seagrass(
		Editor &editor, const RootedSeagrass &nodes, pos_t x, pos_t bed_y, pos_t z)
{
	const auto bed = editor.get_block_absolute(x, bed_y, z);
	const auto above = editor.get_block_absolute(x, bed_y + 1, z);
	if (!nodes.enabled || !bed || !above || above->id() != nodes.water.id())
		return;
	for (std::size_t i = 0; i < nodes.substrates.size(); ++i) {
		if (bed->id() != nodes.substrates[i].id())
			continue;
		Block plant = nodes.plants[i];
		plant.setParam2(3); // VoxeLibre's plantlike_rooted mesh options
		editor.try_set_block_absolute(plant, x, bed_y, z,
				std::optional<std::vector<Block>>({*bed}), std::nullopt);
		return;
	}
}

// Arnis uses negative Z for north; VoxeLibre's fourdir north is positive Z.
// Order: north-south, east-west, ascending E/W/N/S, NE/NW/SE/SW.
inline std::array<Block, 10> rail_shapes(Block straight, Block corner, Block slope)
{
	auto rotated = [](Block block, u8 param2) {
		block.setParam2(param2);
		return block;
	};
	return {rotated(straight, 0), rotated(straight, 1), rotated(slope, 1),
			rotated(slope, 3), rotated(slope, 2), rotated(slope, 0), rotated(corner, 0),
			rotated(corner, 1), rotated(corner, 3), rotated(corner, 2)};
}
}
