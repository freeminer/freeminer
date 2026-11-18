#pragma once

#include "mapnode.h"

#include <string>
#include <utility>
#include <vector>

// Mapgen workers carry this payload to the server after their node writes merge.
struct VoxeLibreGeneratedMetadata
{
	enum class Kind { Chest, Banner };
	v3pos_t pos;
	content_t expected = CONTENT_IGNORE;
	Kind kind = Kind::Chest;
	std::string color;
	u8 rotation = 0;
	std::vector<std::pair<std::string, std::string>> patterns;
};
