#pragma once

// Compatibility facade for the ported Arnis API. Large implementation types
// live in focused headers so users can include only the layer they need.
#include "arnis_types.h"
#include "arnis_ground.h"
#include "arnis_world_editor.h"
#include "../../debug/dump.h"

#include "arnis-cpp/src/block_definitions.h"
#include "arnis_projection_frame.h"

namespace arnis
{

inline void ProjectionFrame::bind(
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
		north = {-std::sin(lat_radians) * std::cos(lon_radians),
				std::cos(lat_radians),
				-std::sin(lat_radians) * std::sin(lon_radians)};
}

inline v3opos_t ProjectionFrame::place(
		double east_offset, double up_offset, double north_offset) const
{
	if (!curved)
		return {static_cast<opos_t>(origin.X + east_offset),
				static_cast<opos_t>(origin.Y + up_offset),
				static_cast<opos_t>(origin.Z + north_offset)};
	const v3d point = v3d(origin.X, origin.Y, origin.Z) + east * east_offset + up * up_offset +
			north * north_offset;
	return {static_cast<opos_t>(point.X), static_cast<opos_t>(point.Y),
			static_cast<opos_t>(point.Z)};
}

void init(MapgenEarth *mg);
Block get_castle_wall_block();

namespace args
{
using Args = arnis::Args;
}

namespace osm_parser
{
using ElementType = arnis::ElementType;
using ProcessedElement = arnis::ProcessedElement;
using ProcessedNode = arnis::ProcessedNode;
using ProcessedRelation = arnis::ProcessedRelation;
using ProcessedWay = arnis::ProcessedWay;
using Way = arnis::ProcessedWay;
}

using Node = ProcessedNode;

namespace coordinate_system::cartesian
{
using XZPoint = arnis::XZPoint;
}

namespace block_definitions
{
using Block = arnis::Block;
}

}

namespace crate = arnis;

#include "arnis-cpp/src/bresenham.h"
