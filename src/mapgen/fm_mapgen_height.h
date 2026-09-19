// SPDX-License-Identifier: LGPL-2.1-or-later
#pragma once

#include "mapgen.h"
#include <cmath>

namespace fm_mapgen
{
// Bound fractal noise around its offset, including custom persistence/octaves.
inline float noiseAmplitude(const NoiseParams &np)
{
	float amplitude = 0.0f;
	float octave = std::fabs(np.scale);
	for (u16 i = 0; i < np.octaves; ++i) {
		amplitude += octave;
		octave *= std::fabs(np.persist);
	}
	return amplitude * 2.0f;
}

// A bounded surface approximation for distant terrain. Density can contain
// overhangs: bisection finds a transition, not necessarily the highest island.
// At most twelve 3D samples; ordinary terrain converges to one node sooner.
template <typename Solid>
pos_t surface(float lower, float upper, Solid solid)
{
	pos_t low = static_cast<pos_t>(std::fmax(-MAX_MAP_GENERATION_LIMIT,
			std::fmin(MAX_MAP_GENERATION_LIMIT, std::floor(lower) - 1.0f)));
	pos_t high = static_cast<pos_t>(std::fmax(-MAX_MAP_GENERATION_LIMIT,
			std::fmin(MAX_MAP_GENERATION_LIMIT, std::ceil(upper) + 1.0f)));
	for (unsigned i = 0; i < 12 && high - low > 1; ++i) {
		const pos_t middle = low + (high - low) / 2;
		if (solid(middle))
			low = middle;
		else
			high = middle;
	}
	return low;
}
} // namespace fm_mapgen
