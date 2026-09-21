#pragma once

#include "../../mapgen/fm_earth_projection.h"

#include <cmath>

class MapgenEarth;

namespace arnis
{
struct ProjectionFrame
{
	MapgenEarth *mapgen = nullptr;
	double lat = 0.0, lon = 0.0, altitude = 0.0;
	v3opos_t origin{};
	v3d east{1, 0, 0}, north{0, 0, 1}, up{0, 1, 0};
	bool curved = false;

	void bind(MapgenEarth *mg, double latitude, double longitude, double height = 0.0);
	v3opos_t place(double east_offset, double up_offset, double north_offset) const;
};
} // namespace arnis
