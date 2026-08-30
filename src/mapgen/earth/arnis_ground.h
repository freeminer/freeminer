#pragma once

#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <limits>
#include <optional>
#include <utility>

#include "arnis_types.h"
#include "mapgen/mapgen_earth.h"
#include "arnis-cpp/src/biome.h"
#include "arnis-cpp/src/canopy/canopy.h"
#include "arnis-cpp/src/land_cover/land_cover.h"
#include "arnis-cpp/src/urban_ground.h"

namespace arnis
{

// A “Ground” class that can return ground level from a set of points
struct Ground
{
	struct RotationMask
	{
		double cx = 0, cz = 0, neg_sin = 0, cos = 1;
		int orig_min_x = 0, orig_max_x = 0, orig_min_z = 0, orig_max_z = 0;
	};
	MapgenEarth *mg = nullptr;
	std::optional<land_cover::LandCoverData> land_cover;
	std::size_t land_cover_world_width = 0;
	std::size_t land_cover_world_height = 0;
	std::optional<canopy::CanopyData> canopy_data;
	std::size_t canopy_world_width = 0, canopy_world_height = 0;
	double elevation_min_height_m = 0.0, elevation_blocks_per_meter = 0.0;
	std::optional<int> elevation_ground_level;
	int snow_threshold_y = std::numeric_limits<int>::max();
	std::optional<RotationMask> rotation_mask;
	biome::Climate climate_state = biome::Climate::Temperate;
	UrbanGroundLookup urban_lookup;

	int get_absolute_y(int x_input, int y_offset, int z_input) const
	{
		//if (ground) {
		int relative_x = x_input; //- xzbbox.min_x();
		int relative_z = z_input; //- xzbbox.min_z();
		return level(XZPoint(relative_x, relative_z)) + y_offset;
	}

	// Return the minimum ground level among points
	// Return std::nullopt if no valid data, to match the Rust’s Option
	std::optional<int> min_level(const std::vector<XZPoint> &points) const
	{
		if (points.empty()) {
			return std::nullopt;
		}
		// Example logic: just pick a fixed level or do some real logic
		int minY = 9999999;
		for (auto &pt : points) {
			// In a real implementation, you'd check your terrain data
			int y = level(pt);
			if (y < minY) {
				minY = y;
			}
		}
		return minY == 9999999 ? std::nullopt : std::optional<int>(minY);
	}

	// Rust's Ground exposes both aggregate queries.  Keep an empty input as
	// None; callers use that distinction when a feature has no geometry.
	std::optional<int> max_level(const std::vector<XZPoint> &points) const
	{
		if (points.empty())
			return std::nullopt;
		int maxY = std::numeric_limits<int>::min();
		for (const auto &pt : points)
			maxY = std::max(maxY, level(pt));
		return maxY;
	}

	// Return ground level for a single XZ point
	int level(const XZPoint &pos) const
	{
		if (!mg)
			return elevation_ground_level.value_or(0);
		++mg->stat.level;
		return mg->get_height(pos.X, pos.Y);
	}

	bool has_land_cover() const
	{
		return land_cover.has_value() && land_cover->width > 0 &&
			   land_cover->height > 0 && land_cover_world_width > 0 &&
			   land_cover_world_height > 0;
	}
	bool has_canopy() const
	{
		return canopy_data.has_value() && canopy_world_width > 0 &&
			   canopy_world_height > 0;
	}
	// Names mirror ground.rs so library consumers do not need to know the
	// mapgen-host field layout.
	int snow_threshold() const { return snow_threshold_y; }
	int base_level(int fallback = -62) const
	{
		return elevation_ground_level.value_or(fallback);
	}
	std::pair<std::size_t, std::size_t> world_dims() const
	{
		return has_land_cover()
					   ? std::pair<std::size_t, std::size_t>{land_cover_world_width,
								 land_cover_world_height}
					   : std::pair<std::size_t, std::size_t>{
								 canopy_world_width, canopy_world_height};
	}
	void set_canopy_data(
			canopy::CanopyData data, std::size_t world_width, std::size_t world_height)
	{
		canopy_data = std::move(data);
		canopy_world_width = world_width;
		canopy_world_height = world_height;
	}
	std::pair<std::size_t, std::size_t> canopy_index(const XZPoint &coord) const
	{
		const auto &c = *canopy_data;
		const double xr = std::clamp(double(coord.x) / double(std::max<std::size_t>(1,
															   canopy_world_width - 1)),
				0.0, 1.0);
		const double zr = std::clamp(double(coord.z) / double(std::max<std::size_t>(1,
															   canopy_world_height - 1)),
				0.0, 1.0);
		return {std::min<std::size_t>(
						std::llround(xr * double(c.width - 1)), c.width - 1),
				std::min<std::size_t>(
						std::llround(zr * double(c.height - 1)), c.height - 1)};
	}
	std::optional<std::uint8_t> canopy_height_m(const XZPoint &coord) const
	{
		if (!has_canopy())
			return std::nullopt;
		const auto [x, z] = canopy_index(coord);
		return canopy_data->canopy_height_m(x, z);
	}
	std::optional<double> canopy_fraction(const XZPoint &coord, int spacing) const
	{
		// `spacing` is in world blocks.  Sampling world coordinates first keeps
		// the result identical whether the canopy raster is coarser or finer
		// than terrain, and excludes no-data columns from the denominator.
		if (!has_canopy() || spacing <= 0)
			return std::nullopt;
		std::uint32_t measured = 0, wooded = 0;
		for (int dz = 0; dz < spacing; ++dz)
			for (int dx = 0; dx < spacing; ++dx)
				if (const auto height = canopy_height_m({coord.x + dx, coord.z + dz})) {
					++measured;
					if (*height >= canopy::CANOPY_MIN_M)
						++wooded;
				}
		return measured ? std::optional<double>(double(wooded) / double(measured))
						: std::nullopt;
	}
	bool snow_capped(int y) const { return y >= snow_threshold_y; }
	void set_elevation_metadata(
			double min_height_m, double blocks_per_meter, int snow_y, int ground_level)
	{
		elevation_min_height_m = min_height_m;
		elevation_blocks_per_meter = blocks_per_meter;
		snow_threshold_y = snow_y;
		elevation_ground_level = ground_level;
	}
	void set_rotation_mask(RotationMask mask) { rotation_mask = mask; }
	bool inside_rotation_mask(int x, int z) const
	{
		if (!rotation_mask)
			return true;
		const auto &m = *rotation_mask;
		const double dx = x - m.cx, dz = z - m.cz;
		const double ox = dx * m.cos + dz * m.neg_sin + m.cx,
					 oz = -dx * m.neg_sin + dz * m.cos + m.cz;
		constexpr double eps = 1e-9;
		return ox >= m.orig_min_x - eps && ox <= m.orig_max_x + eps &&
			   oz >= m.orig_min_z - eps && oz <= m.orig_max_z + eps;
	}
	bool is_in_rotated_bounds(int x, int z) const { return inside_rotation_mask(x, z); }
	biome::Climate climate() const { return climate_state; }
	void set_climate(biome::Climate value) { climate_state = value; }
	void set_urban_lookup(UrbanGroundLookup lookup) { urban_lookup = std::move(lookup); }
	bool is_urban(int x, int z) const { return urban_lookup.is_urban(x, z); }

	void set_land_cover_data(land_cover::LandCoverData data, std::size_t world_width,
			std::size_t world_height)
	{
		// Rust parity: src/ground.rs land-cover accessors.
		// Divergence: C++ currently receives an OSM-derived grid; ESA COG fetch is not ported.
		data.width = data.grid.empty() ? 0 : data.grid.front().size();
		data.height = data.grid.size();
		data.water_distance =
				land_cover::compute_water_distance(data.grid, data.width, data.height);
		data.refresh_water_blend_grid();
		land_cover = std::move(data);
		land_cover_world_width = world_width;
		land_cover_world_height = world_height;
	}

	std::pair<std::size_t, std::size_t> land_cover_index(const XZPoint &coord) const
	{
		// Rust parity: src/ground.rs::cover_class / water_distance sampling.
		const auto &lc = *land_cover;
		const double x_ratio = std::clamp(
				static_cast<double>(coord.x) / static_cast<double>(std::max<std::size_t>(
													   1, land_cover_world_width - 1)),
				0.0, 1.0);
		const double z_ratio = std::clamp(
				static_cast<double>(coord.z) / static_cast<double>(std::max<std::size_t>(
													   1, land_cover_world_height - 1)),
				0.0, 1.0);
		const auto x = std::min<std::size_t>(
				static_cast<std::size_t>(
						std::llround(x_ratio * static_cast<double>(lc.width - 1))),
				lc.width - 1);
		const auto z = std::min<std::size_t>(
				static_cast<std::size_t>(
						std::llround(z_ratio * static_cast<double>(lc.height - 1))),
				lc.height - 1);
		return {x, z};
	}

	uint8_t cover_class(const XZPoint &coord) const
	{
		if (!has_land_cover())
			return 0;
		const auto [x, z] = land_cover_index(coord);
		return land_cover->grid[z][x];
	}

	uint8_t water_distance(const XZPoint &coord) const
	{
		if (!has_land_cover())
			return 0;
		const auto [x, z] = land_cover_index(coord);
		if (z >= land_cover->water_distance.size() ||
				x >= land_cover->water_distance[z].size())
			return 0;
		return land_cover->water_distance[z][x];
	}

	double water_blend(const XZPoint &coord) const
	{
		if (!has_land_cover())
			return 0.0;
		const auto &lc = *land_cover;
		if (lc.water_blend_grid.empty())
			return 0.0;

		const double fx = std::clamp(static_cast<double>(coord.x) /
											 static_cast<double>(std::max<std::size_t>(
													 1, land_cover_world_width - 1)),
								  0.0, 1.0) *
						  static_cast<double>(lc.width - 1);
		const double fz = std::clamp(static_cast<double>(coord.z) /
											 static_cast<double>(std::max<std::size_t>(
													 1, land_cover_world_height - 1)),
								  0.0, 1.0) *
						  static_cast<double>(lc.height - 1);
		const auto x0 = std::min<std::size_t>(
				static_cast<std::size_t>(std::floor(fx)), lc.width - 1);
		const auto z0 = std::min<std::size_t>(
				static_cast<std::size_t>(std::floor(fz)), lc.height - 1);
		const auto x1 = std::min<std::size_t>(x0 + 1, lc.width - 1);
		const auto z1 = std::min<std::size_t>(z0 + 1, lc.height - 1);
		const double tx = fx - std::floor(fx);
		const double tz = fz - std::floor(fz);
		const double w00 = lc.water_blend_grid[z0][x0];
		const double w10 = lc.water_blend_grid[z0][x1];
		const double w01 = lc.water_blend_grid[z1][x0];
		const double w11 = lc.water_blend_grid[z1][x1];
		const double top = w00 * (1.0 - tx) + w10 * tx;
		const double bottom = w01 * (1.0 - tx) + w11 * tx;
		return top * (1.0 - tz) + bottom * tz;
	}

	std::optional<std::tuple<int, int, int, int>> lc_water_block_bounds() const
	{
		// Rust parity: src/ground.rs::lc_water_block_bounds.
		// Used by water_depth to avoid scanning the full world bbox.
		if (!has_land_cover())
			return std::nullopt;
		const auto &lc = *land_cover;
		std::size_t gx0 = std::numeric_limits<std::size_t>::max();
		std::size_t gz0 = std::numeric_limits<std::size_t>::max();
		std::size_t gx1 = 0;
		std::size_t gz1 = 0;
		bool any = false;
		for (std::size_t z = 0; z < lc.height; ++z) {
			for (std::size_t x = 0; x < lc.width; ++x) {
				if (lc.grid[z][x] != land_cover::LC_WATER)
					continue;
				gx0 = std::min(gx0, x);
				gx1 = std::max(gx1, x);
				gz0 = std::min(gz0, z);
				gz1 = std::max(gz1, z);
				any = true;
			}
		}
		if (!any)
			return std::nullopt;

		auto span = [](std::size_t g_lo, std::size_t g_hi, std::size_t world_dim,
							std::size_t grid_dim) {
			if (grid_dim <= 1 || world_dim <= 1)
				return std::pair<int, int>{0, static_cast<int>(world_dim - 1)};
			const double f = static_cast<double>(world_dim - 1) /
							 static_cast<double>(grid_dim - 1);
			const int lo =
					static_cast<int>(std::floor((static_cast<double>(g_lo) - 0.5) * f)) -
					1;
			const int hi =
					static_cast<int>(std::ceil((static_cast<double>(g_hi) + 0.5) * f)) +
					1;
			return std::pair<int, int>{
					std::max(0, lo), std::min(static_cast<int>(world_dim - 1), hi)};
		};
		const auto [x0, x1] = span(gx0, gx1, land_cover_world_width, lc.width);
		const auto [z0, z1] = span(gz0, gz1, land_cover_world_height, lc.height);
		return std::tuple<int, int, int, int>{x0, z0, x1, z1};
	}

	// On gentle ground water follows the interpolated terrain.  Around small
	// DEM/land-cover alignment errors, Rust snaps a steep shoreline to the
	// local minimum; it deliberately does not cross a real cliff or waterfall.
	int slope(const XZPoint &coord) const
	{
		constexpr int step = 4;
		const int east = level({coord.x + step, coord.z}),
				  west = level({coord.x - step, coord.z}),
				  north = level({coord.x, coord.z - step}),
				  south = level({coord.x, coord.z + step});
		return std::max({east, west, north, south}) -
			   std::min({east, west, north, south});
	}
	int water_level(const XZPoint &coord) const
	{
		const int center = level(coord);
		if (slope(coord) <= 2)
			return center;
		constexpr int radius = 3;
		int lowest = center;
		for (int r = 1; r <= radius; ++r)
			for (const auto &[dx, dz] : std::array<std::pair<int, int>, 8>{{{-r, 0},
						 {r, 0}, {0, -r}, {0, r}, {-r, -r}, {-r, r}, {r, -r}, {r, r}}})
				lowest = std::min(lowest, level({coord.x + dx, coord.z + dz}));
		return center - lowest > radius ? center : lowest;
	}
};

}
