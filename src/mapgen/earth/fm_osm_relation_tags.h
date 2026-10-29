#pragma once

#include "arnis_types.h"

#include <string>
#include <string_view>

namespace arnis::osm_parser
{
// Remove only area classifications repeated from a holed multipolygon relation.
// Keep independent tags on the member way so it can still describe other features.
inline void drop_repeated_area_tags(tags_t &way, const tags_t &relation)
{
	const auto is_water = [](const tags_t &tags) {
		return tags.get("natural") == "water" || tags.get("natural") == "bay";
	};
	if (relation.contains("water") || is_water(relation)) {
		if (way.contains("water") || is_water(way)) {
			if (is_water(way))
				way.erase("natural");
			way.erase("water");
		}
		return;
	}

	std::string_view key;
	if (relation.contains("natural"))
		key = "natural";
	else if (relation.contains("landuse"))
		key = "landuse";
	else if (relation.get("leisure") == "park")
		key = "leisure";
	else if (relation.contains("man_made"))
		key = "man_made";
	if (key.empty())
		return;
	const std::string owned_key(key);
	if (way.get(owned_key) == relation.get(owned_key))
		way.erase(owned_key);
}
} // namespace arnis::osm_parser
