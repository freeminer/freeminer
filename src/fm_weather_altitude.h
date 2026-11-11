// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <algorithm>
#include <cmath>

namespace weather
{
// Coordinates are promoted before any subtraction, division or negation.
// Double preserves individual nodes throughout the 32-bit position range.
inline double relative_height(double height, double surface)
{
	return height - surface;
}

// Return a temperature adjustment to the local biome/monthly climate. Above
// -limit, retain the legacy lapse rate and hot-core ramp. Below it, cool over
// limit nodes, then warm over limit nodes, repeating without a discontinuity.
// core_heat controls the existing geothermal peak; deep_min is the adjustment
// at the cold layers. stepped_lapse preserves the biome path's integer division.
inline double altitude_heat(double height, double lapse_height, double core_depth,
		double deep_min, double core_heat, double limit, bool stepped_lapse = false)
{
	if (!std::isfinite(height) || !std::isfinite(limit) || limit <= 0.0)
		return 0.0;
	if (!std::isfinite(deep_min))
		deep_min = -40.0;
	if (!std::isfinite(core_heat))
		core_heat = 6000.0;
	deep_min = std::clamp(deep_min, -32768.0, 32767.0);
	core_heat = std::clamp(core_heat, 0.0, 32767.0);
	const auto legacy = [&](double y) {
		double lapse = std::isfinite(lapse_height) && lapse_height != 0.0
							   ? y / lapse_height
							   : 0.0;
		if (stepped_lapse)
			lapse = std::trunc(lapse);
		if (std::isfinite(core_depth) && core_depth > 0.0) {
			const double fraction = std::clamp(1.0 - (y + limit) / core_depth, 0.0, 1.0);
			lapse += core_heat * fraction;
		}
		return lapse;
	};
	if (height >= -limit)
		return legacy(height);

	const double warm = legacy(-limit);
	// An inverted range becomes a constant profile, rather than reversing the
	// requested cooling/warming order. Leave the legacy boundary unchanged.
	const double cold = std::min(deep_min, warm);
	const double phase = std::fmod(-height - limit, 2.0 * limit) / limit;
	const double cooling = phase <= 1.0 ? phase : 2.0 - phase;
	return warm + (cold - warm) * cooling;
}
} // namespace weather
