#pragma once

#include "voxelibre_metadata.h"
#include "inventory.h"
#include "nodemetadata.h"

#include <string_view>
#include <unordered_map>
#include <unordered_set>

namespace voxelibre_metadata
{
inline std::string color_name(std::string_view color)
{
	static const std::unordered_map<std::string, std::string> colors{{"white", "white"},
			{"gray", "grey"}, {"grey", "grey"}, {"light_gray", "silver"},
			{"light_grey", "silver"}, {"silver", "silver"}, {"black", "black"},
			{"red", "red"}, {"yellow", "yellow"}, {"green", "green"}, {"cyan", "cyan"},
			{"blue", "blue"}, {"magenta", "magenta"}, {"orange", "orange"},
			{"purple", "purple"}, {"brown", "brown"}, {"pink", "pink"}, {"lime", "lime"},
			{"light_blue", "light_blue"}};
	const auto it = colors.find(std::string(color));
	return it == colors.end() ? "white" : it->second;
}

inline std::string color_group(std::string_view color)
{
	const auto c = color_name(color);
	if (c == "grey")
		return "unicolor_darkgrey";
	if (c == "silver")
		return "unicolor_grey";
	if (c == "green")
		return "unicolor_dark_green";
	if (c == "purple")
		return "unicolor_violet";
	if (c == "magenta")
		return "unicolor_red_violet";
	return "unicolor_" + c;
}

inline std::string banner_layers(
		const std::vector<std::pair<std::string, std::string>> &patterns)
{
	static const std::unordered_set<std::string> names{"border", "bricks", "circle",
			"cross", "curly_border", "diagonal_up_left", "diagonal_up_right",
			"diagonal_right", "diagonal_left", "flower", "gradient", "gradient_up",
			"half_horizontal_bottom", "half_horizontal", "half_vertical",
			"half_vertical_right", "thing", "rhombus", "skull", "small_stripes",
			"square_bottom_left", "square_bottom_right", "square_top_left",
			"square_top_right", "stalker", "straight_cross", "stripe_bottom",
			"stripe_center", "stripe_downleft", "stripe_downright", "stripe_left",
			"stripe_middle", "stripe_right", "stripe_top", "triangle_bottom",
			"triangle_top", "triangles_bottom", "triangles_top"};
	std::string result = "return {";
	for (const auto &[color, source] : patterns) {
		std::string name = source;
		if (name.starts_with("minecraft:"))
			name.erase(0, 10);
		if (name == "stripe_small")
			name = "small_stripes";
		if (name == "mojang")
			name = "thing";
		if (!names.contains(name))
			continue;
		result += "{pattern=\"" + name + "\",color=\"" + color_group(color) + "\"},";
	}
	return result + "}";
}

inline void initialize(
		NodeMetadata &meta, IItemDefManager *idef, const VoxeLibreGeneratedMetadata &data)
{
	Inventory *inventory = meta.getInventory();
	if (data.kind == VoxeLibreGeneratedMetadata::Kind::Chest) {
		inventory->addList("main", 27);
		inventory->addList("input", 1);
		return;
	}
	if (data.kind == VoxeLibreGeneratedMetadata::Kind::Bookshelf) {
		inventory->addList("main", 6);
		meta.setString("last_slot_used", "0");
		return;
	}
	ItemStack banner("mcl_banners:banner_item_" + color_name(data.color), 1, 0, idef);
	banner.metadata.setString("layers", banner_layers(data.patterns));
	inventory->addList("banner", 1)->changeItem(0, banner);
	meta.setString("rotation_level", std::to_string(data.rotation));
}
}
