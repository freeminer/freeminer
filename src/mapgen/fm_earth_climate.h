// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <algorithm>
#include <cmath>
#include <cstdint>

namespace fm_earth_climate
{
// Old annual WorldClim exports erased the no-data mask. Black is ambiguous
// in those files only; masked v2 images retain valid black values.
inline bool valid_pixel(uint8_t r, uint8_t g, uint8_t b, uint8_t alpha, bool legacy)
{
	return alpha != 0 && !(legacy && r == 0 && g == 0 && b == 0);
}

inline float temperature(double latitude, float time, float year_days, bool seasonal)
{
	const double lat = std::clamp(latitude, -90.0, 90.0);
	const double fraction = std::abs(lat) / 90.0;
	const double annual = 27.0 - 47.0 * fraction * fraction;
	const double phase =
			std::isfinite(time) ? time / (86400.0 * std::max(1.0f, year_days)) : 0.0;
	const double season = seasonal ? 15.0 * fraction *
											 std::sin(lat * 0.017453292519943295) *
											 std::cos(6.283185307179586 * (phase - 0.55))
								   : 0.0;
	return static_cast<float>(annual + season);
}

inline float humidity(double latitude)
{
	return static_cast<float>(
			75.0 - 10.0 * std::abs(std::clamp(latitude, -90.0, 90.0)) / 90.0);
}
} // namespace fm_earth_climate
