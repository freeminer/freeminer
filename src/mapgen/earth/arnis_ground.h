#pragma once

#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <limits>
#include <optional>
#include <utility>

#include "arnis_types.h"
#include "arnis-cpp/src/celestial.h"
#include "mapgen/mapgen_earth.h"
#include "arnis-cpp/src/biome.h"
#include "arnis-cpp/src/canopy/canopy.h"
#include "arnis-cpp/src/land_cover/land_cover.h"
#include "arnis-cpp/src/ecoregion.h"
#include "arnis-cpp/src/urban_ground.h"

namespace arnis
{

// A “Ground” class that can return ground level from a set of points
struct Ground
{
	struct ElevationSoftTop
	{
		double knee_m = 0.0;
		double width_blocks = 0.0;
	};
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
	// Converts host block-space slope back to the documented world-scale slope.
	double elevation_slope_correction = 1.0;
	std::optional<ElevationSoftTop> elevation_soft_top;
	bool extended_ceiling = false;
	CelestialBody body = CelestialBody::Earth;
	std::optional<int> elevation_ground_level;
	std::vector<std::vector<float>> elevation_grid;
	std::size_t elevation_world_width = 0, elevation_world_height = 0;
	bool elevation_enabled = false;
	int snow_threshold_y = std::numeric_limits<int>::max();
	std::optional<RotationMask> rotation_mask;
	biome::Climate climate_state = biome::Climate::Temperate;
	UrbanGroundLookup urban_lookup;
	std::optional<ecoregion::EcoMap> ecoregion_map;
	static Ground new_flat(int ground_level)
	{
		Ground ground;
		ground.elevation_ground_level = ground_level;
		ground.snow_threshold_y = std::numeric_limits<int>::max();
		ground.elevation_enabled = false;
		return ground;
	}
	static Ground new_flat_with_land_cover(land_cover::LandCoverData data,
			std::size_t world_width, std::size_t world_height, int ground_level)
	{
		Ground ground = new_flat(ground_level);
		ground.set_land_cover_data(std::move(data), world_width, world_height);
		return ground;
	}
	void set_ground_level(int ground_level) { elevation_ground_level = ground_level; }

	int get_absolute_y(int x_input, int y_offset, int z_input) const
	{
		//if (ground) {
		int relative_x = x_input; //- xzbbox.min_x();
		int relative_z = z_input; //- xzbbox.min_z();
		return level(XZPoint(relative_x, relative_z)) + y_offset;
	}

	// Return the minimum ground level among points
	// Return std::nullopt if no valid data, to match the Rust’s Option
	std::optional<int> min_level(const std::vector<XZPoint> &points) const;

	// Rust's Ground exposes both aggregate queries.  Keep an empty input as
	// None; callers use that distinction when a feature has no geometry.
	std::optional<int> max_level(const std::vector<XZPoint> &points) const;

	// Return ground level for a single XZ point
	int level(const XZPoint &pos) const;
	// Unrounded terrain height and the derived continuous terrain metrics used
	// by Rust's surface/material selection.  Keeping these in Ground avoids
	// reconstructing elevation interpolation in individual processors.
	double level_exact(const XZPoint &pos) const;
	double slope_exact(const XZPoint &pos) const;
	double slope_soft_top_stretch(int y) const;
	std::pair<double, std::pair<double, double>> slope_and_gradient(
			const XZPoint &pos) const;
	double convexity(const XZPoint &pos) const;

	bool has_land_cover() const;
	bool has_canopy() const;
	// Names mirror ground.rs so library consumers do not need to know the
	// mapgen-host field layout.
	int snow_threshold() const { return snow_threshold_y; }
	CelestialBody celestial_body() const { return body; }
	bool is_earth() const { return arnis::is_earth(body); }
	double blocks_per_meter() const;
	void set_celestial_body(CelestialBody value) { body = value; }
	void set_extended_ceiling(bool value) { extended_ceiling = value; }
	int base_level(int fallback = -62) const;
	std::pair<std::size_t, std::size_t> world_dims() const;
	void set_canopy_data(
			canopy::CanopyData data, std::size_t world_width, std::size_t world_height);
	// Rust-compatible convenience overload: replace an existing canopy raster
	// while preserving the host world's sampling dimensions.
	void set_canopy_data(
			const std::vector<std::uint8_t> &grid, std::size_t width, std::size_t height);
	std::pair<std::size_t, std::size_t> canopy_index(const XZPoint &coord) const;
	std::optional<std::uint8_t> canopy_height_m(const XZPoint &coord) const;
	std::optional<double> canopy_fraction(const XZPoint &coord, int spacing) const;
	bool snow_capped(int y) const { return y >= snow_threshold_y; }
	// Rust ground.rs snow-line model.  It is expressed in terrain metres, then
	// inverted through the actual elevation affine so a sunk terrain base or an
	// extended build ceiling cannot shift every alpine surface.
	static double snow_line_meters(double latitude_degrees);
	void set_snow_line_for_latitude(double latitude_degrees);
	void set_elevation_metadata(double min_height_m, double blocks_per_meter, int snow_y,
			int ground_level, double slope_correction,
			std::optional<ElevationSoftTop> soft_top = std::nullopt);

	void set_elevation_data(const std::vector<std::vector<float>> &heights,
			std::size_t width, std::size_t height, std::size_t world_width,
			std::size_t world_height);
	void set_elevation_data(const std::vector<std::vector<double>> &heights,
			std::size_t width, std::size_t height, std::size_t world_width,
			std::size_t world_height);
	void set_elevation_enabled(bool enabled) { elevation_enabled = enabled; }
	void clear_elevation_data();
	void set_world_dims(std::size_t world_width, std::size_t world_height);
	void apply_osm_water_override(
			const std::vector<ProcessedElement> &elements, const XZBBox &bbox);
	void apply_osm_land_override(const std::vector<ProcessedElement> &elements,
			const XZBBox &bbox, double scale);
	void apply_bridge_land_cover_repair(const std::vector<ProcessedElement> &elements,
			const XZBBox &bbox, double scale);
	void set_rotation_mask(RotationMask mask) { rotation_mask = mask; }
	bool inside_rotation_mask(int x, int z) const;
	bool is_in_rotated_bounds(int x, int z) const { return inside_rotation_mask(x, z); }
	biome::Climate climate() const { return climate_state; }
	void set_climate(biome::Climate value) { climate_state = value; }
	void set_urban_lookup(UrbanGroundLookup lookup) { urban_lookup = std::move(lookup); }
	bool is_urban(int x, int z) const { return urban_lookup.is_urban(x, z); }
	void set_ecoregion_map(ecoregion::EcoMap map) { ecoregion_map = std::move(map); }
	std::optional<ecoregion::Ecoregion> ecoregion_at(const XZPoint &coord) const;

	void set_land_cover_data(land_cover::LandCoverData data, std::size_t world_width,
			std::size_t world_height);
	void set_land_cover_data(const std::vector<std::vector<std::uint8_t>> &grid,
			const std::vector<std::vector<std::uint8_t>> &water_distance,
			std::size_t width, std::size_t height);

	std::pair<std::size_t, std::size_t> land_cover_index(const XZPoint &coord) const;

	uint8_t cover_class(const XZPoint &coord) const;

	uint8_t water_distance(const XZPoint &coord) const;
	bool is_interior_water(const XZPoint &coord) const;

	double water_blend(const XZPoint &coord) const;
	void warm_water_blend();

	std::optional<std::tuple<int, int, int, int>> lc_water_block_bounds() const;

	// On gentle ground water follows the interpolated terrain.  Around small
	// DEM/land-cover alignment errors, Rust snaps a steep shoreline to the
	// local minimum; it deliberately does not cross a real cliff or waterfall.
	int slope(const XZPoint &coord) const;
	int water_level(const XZPoint &coord) const;
};

}
