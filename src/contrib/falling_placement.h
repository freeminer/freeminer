// SPDX-License-Identifier: LGPL-2.1-or-later
#pragma once

namespace epixel
{
// Prefer the collision source, then the closest neighboring supports.
// The caller validates both the support and the destination.
template <typename Position, typename IsUsable>
bool findFallingSupport(const Position &collision, IsUsable is_usable, Position &result)
{
	static constexpr int offsets[][2] = {
			{0, 0}, {-1, 0}, {1, 0}, {0, -1}, {0, 1}, {-1, -1}, {-1, 1}, {1, -1}, {1, 1}};
	for (const auto &offset : offsets) {
		Position candidate(collision.X + offset[0], collision.Y, collision.Z + offset[1]);
		if (is_usable(candidate)) {
			result = candidate;
			return true;
		}
	}
	return false;
}
}
