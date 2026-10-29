#pragma once

#include "arnis_types.h"

#include <string>
#include <string_view>

namespace arnis::osm_parser
{
// Rust suppresses a closed outer way only when its own tags repeat the relation's
// area classification and it carries no independent feature tags. Metadata may
// differ because it describes the same feature, not a separate rendering pass.
inline bool repeats_relation_area_tags(const tags_t &way, const tags_t &relation)
{
	const auto metadata = [](const std::string &key) {
		return key == "name" ||
			   (key.size() >= 5 && key.compare(key.size() - 5, 5, "_name") == 0) ||
			   key == "wikidata" || key == "wikipedia" || key == "area" || key == "type";
	};
	bool has_area_classification = false;
	for (const auto &[key, value] : way) {
		if (metadata(key))
			continue;
		has_area_classification = true;
		const auto relation_value = relation.find(key);
		if (relation_value == relation.end() || relation_value->second != value)
			return false;
	}
	return has_area_classification;
}
} // namespace arnis::osm_parser
