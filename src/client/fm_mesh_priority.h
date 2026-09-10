// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include "util/numeric.h"
#include <algorithm>

// Recompute from the current camera, including work queued before a fast move.
// Preserve arrival order at equal distances and leave explicit urgency intact.
template <typename Queue, typename Position>
void sortMeshUpdatesNearFirst(Queue &queue, const v3bpos_t &camera, Position position)
{
	std::stable_sort(queue.begin(), queue.end(), [&](const auto &a, const auto &b) {
		return radius_box(position(a), camera) < radius_box(position(b), camera);
	});
}
