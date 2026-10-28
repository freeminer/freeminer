#pragma once

#include "mapgen/mapgen.h"
#include <string>
#include <vector>

// Custom gennotify values use core.serialize's Lua representation. Only
// integer coordinates are emitted; the existing API decodes this as a table.
inline void fm_notify_arnis_rails(
		GenerateNotifier &notifier, const std::vector<v3pos_t> &positions)
{
	if (!notifier.setCustom("freeminer:arnis_rails", "return {}"))
		return;
	std::string data = "return {";
	data.reserve(positions.size() * 40 + 10);
	for (const auto &pos : positions) {
		data += "{x=" + std::to_string(pos.X) + ",y=" + std::to_string(pos.Y) +
				",z=" + std::to_string(pos.Z) + "},";
	}
	data += '}';
	notifier.setCustom("freeminer:arnis_rails", data);
}
