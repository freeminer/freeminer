#include "arnis_projection_frame.h"
#include "mapgen/mapgen_earth.h"

namespace arnis
{

void ProjectionFrame::bind(
		MapgenEarth *mg, double latitude, double longitude, double height)
{
	mapgen = mg;
	lat = latitude;
	lon = longitude;
	altitude = height;
	curved = mg && mg->projection.curved;
	if (!curved)
		return;
	origin = mg->projection.place(lat, lon, altitude);
	up = mg->projection.up(origin);
	const double lat_radians = lat * fm_earth::pi / 180.0;
	const double lon_radians = lon * fm_earth::pi / 180.0;
	east = {-std::sin(lon_radians), 0.0, std::cos(lon_radians)};
	north = up.crossProduct(east).normalize();
	if (std::abs(up.Y) > 0.999999)
		north = {-std::sin(lat_radians) * std::cos(lon_radians), std::cos(lat_radians),
				-std::sin(lat_radians) * std::sin(lon_radians)};
}

v3opos_t ProjectionFrame::place(
		double east_offset, double up_offset, double north_offset) const
{
	if (!curved)
		return {static_cast<opos_t>(origin.X + east_offset),
				static_cast<opos_t>(origin.Y + up_offset),
				static_cast<opos_t>(origin.Z + north_offset)};
	const v3d point = v3d(origin.X, origin.Y, origin.Z) + east * east_offset +
					  up * up_offset + north * north_offset;
	return {static_cast<opos_t>(point.X), static_cast<opos_t>(point.Y),
			static_cast<opos_t>(point.Z)};
}

}