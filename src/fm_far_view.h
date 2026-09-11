// SPDX-License-Identifier: LGPL-2.1-or-later
#pragma once

#include <algorithm>
#include <array>
#include <cmath>
#include <memory>
#include "constants.h"
#include "irr_v3d.h"

namespace farmesh
{

// Immutable camera snapshot, in node coordinates. Shared by one grid's scanners
// and samplers; the main thread never changes an in-flight grid's projection.
struct View
{
	v3opos_t position{};
	v3opos_t direction{0, 0, 1};
	std::array<v3opos_t, 5> planes{};
	std::array<v3opos_t, 5> view_planes{};
	static constexpr unsigned max_levels = 7;
	double tan_x{};
	double tan_y{};
	unsigned levels{};

	static View capture(v3opos_t position, v3opos_t direction, double fov_x, double fov_y,
			double normal_fov_degrees, unsigned level_limit = max_levels)
	{
		View view;
		view.position = position;
		view.direction = direction.normalize();
		view.tan_x = std::tan(fov_x * 0.5);
		view.tan_y = std::tan(fov_y * 0.5);
		if (view.tan_x <= 0 || view.tan_y <= 0)
			return view;
		const double aspect = view.tan_x / view.tan_y;
		const double normal_y = normal_fov_degrees * (3.141592653589793 / 180.0) *
								std::clamp(std::sqrt(1.6 / aspect), 1.0, 1.4);
		const double magnification = std::tan(normal_y * 0.5) / view.tan_y;
		// Ignore small FOV changes from movement and rounding.
		if (magnification > 1.1)
			view.levels = std::min({unsigned(std::ceil(std::log2(magnification))),
					level_limit, max_levels});
		auto right = v3opos_t(0, 1, 0).crossProduct(view.direction);
		if (right.getLengthSQ() < 0.000001)
			right = v3opos_t(1, 0, 0);
		right.normalize();
		const auto up = view.direction.crossProduct(right);
		view.view_planes = {view.direction, view.direction * view.tan_x + right,
				view.direction * view.tan_x - right, view.direction * view.tan_y + up,
				view.direction * view.tan_y - up};
		// Twenty percent preload margin on either screen dimension.
		view.planes = {view.direction, view.direction * (view.tan_x * 1.2) + right,
				view.direction * (view.tan_x * 1.2) - right,
				view.direction * (view.tan_y * 1.2) + up,
				view.direction * (view.tan_y * 1.2) - up};
		return view;
	}

	template <typename Vector, typename Size>
	bool intersects(const Vector &block_min, Size block_side, bool preload = true) const
	{
		const double half = double(block_side) * MAP_BLOCKSIZE * 0.5;
		const v3opos_t delta(double(block_min.X) * MAP_BLOCKSIZE + half - position.X,
				double(block_min.Y) * MAP_BLOCKSIZE + half - position.Y,
				double(block_min.Z) * MAP_BLOCKSIZE + half - position.Z);
		for (const auto &normal : (preload ? planes : view_planes)) {
			const double radius =
					half * (std::abs(normal.X) + std::abs(normal.Y) + std::abs(normal.Z));
			if (delta.dotProduct(normal) + radius < 0)
				return false;
		}
		return true;
	}

	unsigned preloadLevels() const
	{
		return levels ? std::max(1u, levels > 2 ? levels - 2 : 0u) : 0;
	}

	template <typename Vector, typename Size>
	unsigned refinementLevels(const Vector &block_min, Size block_side) const
	{
		if (!levels)
			return 0;
		if (intersects(block_min, block_side, false))
			return levels;
		return intersects(block_min, block_side) ? preloadLevels() : 0;
	}

	// Project a vertical terrain column into the frustum without guessing its
	// surface height. Intersect the possible Y intervals from all five planes;
	// independent plane/AABB tests alone greatly overestimate pitched views.
	// Bounds are the canonical 3-D tree's vertical extent, in block coordinates.
	template <typename Vector, typename Size>
	bool intersectsColumn(const Vector &block_min, Size block_side, double min_y,
			double max_y, bool preload = true) const
	{
		const double half = double(block_side) * MAP_BLOCKSIZE * 0.5;
		const double dx = double(block_min.X) * MAP_BLOCKSIZE + half - position.X;
		const double dz = double(block_min.Z) * MAP_BLOCKSIZE + half - position.Z;
		double low = min_y * MAP_BLOCKSIZE - position.Y;
		double high = max_y * MAP_BLOCKSIZE - position.Y;
		for (const auto &normal : (preload ? planes : view_planes)) {
			const double support = dx * normal.X + dz * normal.Z +
								   half * (std::abs(normal.X) + std::abs(normal.Z));
			if (normal.Y > 0)
				low = std::max(low, -support / normal.Y);
			else if (normal.Y < 0)
				high = std::min(high, -support / normal.Y);
			else if (support < 0)
				return false;
			if (low > high)
				return false;
		}
		return true;
	}

	template <typename Vector, typename Size>
	unsigned columnRefinementLevels(
			const Vector &block_min, Size block_side, double min_y, double max_y) const
	{
		if (!levels)
			return 0;
		if (intersectsColumn(block_min, block_side, min_y, max_y, false))
			return levels;
		return intersectsColumn(block_min, block_side, min_y, max_y) ? preloadLevels()
																	 : 0;
	}

	bool changed(const View &other) const
	{
		if (levels != other.levels)
			return true;
		if (!levels)
			return false;
		// Retain the expanded view until a small turn uses its preload margin.
		const double angle =
				std::min(0.0872664626, 0.1 * std::atan(std::min(tan_x, tan_y)));
		return direction.dotProduct(other.direction) < std::cos(angle) ||
			   std::abs(tan_x - other.tan_x) > tan_x * 0.05 ||
			   std::abs(tan_y - other.tan_y) > tan_y * 0.05;
	}
};
using ViewPtr = std::shared_ptr<const View>;

} // namespace farmesh
