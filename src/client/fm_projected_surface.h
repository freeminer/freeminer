// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include "mapgen/fm_earth_projection.h"
#include <utility>

namespace farmesh
{
inline std::pair<double, double> surfaceAltitudeBounds(
		const fm_earth::Adapter &projection, const v3opos_t &center, double side)
{
	const double altitude = projection.altitude(center);
	const double radius = std::sqrt(3.0) * side * 0.5;
	return {altitude - radius, altitude + radius};
}

struct ProjectedSurface {
	v3opos_t position;
	v3d normal;
};

template <typename Vector>
int surfaceTriangleWinding(const Vector &a, const Vector &b, const Vector &c,
		const Vector &normal)
{
	const auto cross = (b - a).crossProduct(c - a);
	if (cross.getLengthSQ() == 0)
		return 0;
	return cross.dotProduct(normal) < 0 ? -1 : 1;
}

template <typename Elevation>
ProjectedSurface projectSurface(const fm_earth::Adapter &projection,
		const v3opos_t &world, double sea_level, Elevation elevation, bool water = false)
{
	const auto sample = projection.sample(world);
	const double altitude = water ? sea_level : std::max(sea_level, elevation(sample));
	const auto surface = projection.place(sample.lat, sample.lon, altitude);
	return {surface, projection.up(surface)};
}
}
