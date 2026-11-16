// SPDX-License-Identifier: LGPL-2.1-or-later
#pragma once

#include "fm_earth_projection.h"
#include "irr_v3d.h"
#include <json/json.h>
#include <algorithm>
#include <cmath>
#include <iomanip>
#include <limits>
#include <locale>
#include <optional>
#include <sstream>
#include <string>

namespace fm_earth
{
// The world geometry uses projection.origin, not the geographic center offset.
inline std::optional<std::string> defaultGravitySource(const Json::Value &params)
{
	const auto &projection = params["projection"];
	std::string type;
	if (projection.isString())
		type = projection.asString();
	else if (projection.isObject())
		type = projection.get("type", "flat").asString();
	else
		return std::nullopt;

	const bool inverted = type == "inverted_sphere" || type == "inside_sphere";
	if (!inverted && type != "sphere" && type != "spherical" && type != "cube" &&
			type != "torus")
		return std::nullopt;

	v3opos_t origin(0, 0, 0);
	if (projection.isObject() && projection["origin"].isObject()) {
		const auto &o = projection["origin"];
		origin = v3opos_t(static_cast<opos_t>(o.get("x", 0).asDouble()),
				static_cast<opos_t>(o.get("y", 0).asDouble()),
				static_cast<opos_t>(o.get("z", 0).asDouble()));
	}
	if (!std::isfinite(origin.X) || !std::isfinite(origin.Y) || !std::isfinite(origin.Z))
		return std::nullopt;

	if (type == "torus") {
		Parameters defaults;
		const double radius =
				projection.isObject()
						? projection.get("radius", defaults.radius).asDouble()
						: defaults.radius;
		const double major =
				projection.isObject()
						? projection.get("major_radius", defaults.major_radius).asDouble()
						: defaults.major_radius;
		if (!std::isfinite(radius) || !std::isfinite(major) || radius <= 0 ||
				major <= radius)
			return std::nullopt;

		constexpr double diagonal = 0.7071067811865475244;
		constexpr double directions[8][2] = {{1, 0}, {diagonal, diagonal}, {0, 1},
				{-diagonal, diagonal}, {-1, 0}, {-diagonal, -diagonal}, {0, -1},
				{diagonal, -diagonal}};
		const double falloff = std::max(radius * 0.2, 0.000001);
		// Tune the ring to 9.81 nodes/s^2 at the outer surface on +X.
		double unit_acceleration = 0;
		for (const auto &direction : directions) {
			const double dx = major + radius - major * direction[0];
			const double dz = major * direction[1];
			const double distance = std::hypot(dx, dz);
			unit_acceleration += 6.6743e-8 * (dx / distance) /
								 (distance * distance + falloff * falloff);
		}
		const double mass = 9.81 / unit_acceleration;
		if (!std::isfinite(mass) || mass <= 0)
			return std::nullopt;
		std::ostringstream sources;
		sources.imbue(std::locale::classic());
		sources << std::setprecision(std::numeric_limits<double>::max_digits10);
		bool first = true;
		for (const auto &direction : directions) {
			if (!first)
				sources << ';';
			first = false;
			sources << double(origin.X) + major * direction[0] << ',' << origin.Y << ','
					<< double(origin.Z) + major * direction[1] << ",mass=" << mass << ','
					<< falloff;
		}
		return sources.str();
	}

	// Use the physical Earth radius for softening so the mass remains bounded
	// even when a projection is scaled to a much smaller world.
	std::ostringstream source;
	source.imbue(std::locale::classic());
	source << std::setprecision(std::numeric_limits<opos_t>::max_digits10) << origin.X
		   << ',' << origin.Y << ',' << origin.Z
		   << ",mass=" << (inverted ? "-5.9722e21" : "5.9722e21") << ",6371000";
	return source.str();
}
} // namespace fm_earth
