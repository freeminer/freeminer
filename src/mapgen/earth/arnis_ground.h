#pragma once

#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <functional>
#include <limits>
#include <memory>
#include <optional>
#include <string>
#include <utility>
#include <tuple>

#include "arnis_types.h"
#include "arnis-cpp/src/celestial.h"
#include "arnis-cpp/src/elevation/postprocess.h"
#include "arnis-cpp/src/elevation/pipeline.h"
#include "arnis-cpp/src/args.h"
#include "mapgen/mapgen_earth.h"
#include "arnis-cpp/src/biome.h"
#include "arnis-cpp/src/canopy/canopy.h"
#include "arnis-cpp/src/land_cover/land_cover.h"
#include "arnis-cpp/src/ecoregion.h"
#include "arnis-cpp/src/coordinate_system/geographic/llbbox.h"
#include "arnis-cpp/src/projection/web_mercator.h"

namespace arnis
{

namespace osm_parser
{
struct RawOsmDocument;
}

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
	// Freeminer's mapgen adapter can delegate terrain queries to its native
	// height sampler when no standalone Arnis elevation raster is installed.
	// Coordinates are Ground-local, like the Rust-compatible elevation grid.
	std::function<int(const XZPoint &)> host_elevation;
	std::optional<land_cover::LandCoverData> land_cover;
	std::size_t land_cover_world_width = 0;
	std::size_t land_cover_world_height = 0;
	// Rust Ground keeps the flat-mode coordinate frame independently from the
	// sampled elevation raster. In particular, set_world_dims must not mutate
	// ElevationData after a rotation has already installed its own dimensions.
	std::size_t world_width = 0, world_height = 0;
	std::optional<canopy::CanopyData> canopy_data;
	std::size_t canopy_world_width = 0, canopy_world_height = 0;
	double elevation_min_height_m = 0.0, elevation_blocks_per_meter = 0.0;
	// Converts host block-space slope back to the documented world-scale slope.
	double elevation_slope_correction = 1.0;
	std::optional<ElevationSoftTop> elevation_soft_top;
	bool extended_ceiling = false;
	CelestialBody body_ = CelestialBody::Earth;
	std::optional<int> elevation_ground_level;
	std::vector<std::vector<float>> elevation_grid;
	std::size_t elevation_world_width = 0, elevation_world_height = 0;
	bool elevation_enabled = false;
	int snow_threshold_y_ = std::numeric_limits<int>::max();
	std::optional<RotationMask> rotation_mask;
	biome::Climate climate_state = biome::Climate::Temperate;
	bool winter_state = false;
	// Shared immutable grid, matching Rust's Arc<EcoMap> and avoiding a full
	// raster copy when Ground is copied for generation/transformation stages.
	std::shared_ptr<const ecoregion::EcoMap> ecoregions;
	static Ground new_flat(int ground_level)
	{
		Ground ground;
		ground.elevation_ground_level = ground_level;
		ground.snow_threshold_y_ = std::numeric_limits<int>::max();
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
	template <typename InputIt>
	std::optional<int> min_level(InputIt first, InputIt last) const
	{
		if (!elevation_enabled)
			return elevation_ground_level.value_or(0);
		if (first == last)
			return std::nullopt;
		int result = level(*first++);
		for (; first != last; ++first)
			result = std::min(result, level(*first));
		return result;
	}

	// Rust's Ground exposes both aggregate queries.  Keep an empty input as
	// None; callers use that distinction when a feature has no geometry.
	std::optional<int> max_level(const std::vector<XZPoint> &points) const;
	template <typename InputIt>
	std::optional<int> max_level(InputIt first, InputIt last) const
	{
		if (!elevation_enabled)
			return elevation_ground_level.value_or(0);
		if (first == last)
			return std::nullopt;
		int result = level(*first++);
		for (; first != last; ++first)
			result = std::max(result, level(*first));
		return result;
	}

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
	int snow_threshold_y() const { return snow_threshold_y_; }
	// Backward-compatible spelling used by older Freeminer call sites.
	int snow_threshold() const { return snow_threshold_y(); }
	CelestialBody body() const { return body_; }
	CelestialBody celestial_body() const { return body(); }
	bool is_earth() const { return arnis::is_earth(body_); }
	// Rust Ground::slope_correction exposes the vertical scale used to turn
	// sampled block heights back into the documented world-scale incline.
	double slope_correction() const { return elevation_slope_correction; }
	double blocks_per_meter() const;
	void set_celestial_body(CelestialBody value) { body_ = value; }
	void set_extended_ceiling(bool value) { extended_ceiling = value; }
	int base_level(int fallback = -62) const;
	std::pair<std::size_t, std::size_t> world_dims() const;
	void set_canopy_data(
			canopy::CanopyData data, std::size_t world_width, std::size_t world_height);
	// Rust-compatible convenience overload: replace an existing canopy raster
	// while preserving the host world's sampling dimensions.
	void set_canopy_data(
			const std::vector<std::uint8_t> &grid, std::size_t width, std::size_t height);
	void save_debug_image(const std::string &filename) const;
	void save_land_cover_debug_image(const std::string &filename) const;
	void save_canopy_debug_image(const std::string &filename) const;
	std::pair<std::size_t, std::size_t> canopy_index(const XZPoint &coord) const;
	std::optional<std::uint8_t> canopy_height_m(const XZPoint &coord) const;
	std::optional<std::pair<double, std::uint8_t>> canopy_cell(
			const XZPoint &origin, int span) const;
	std::optional<double> canopy_fraction(const XZPoint &coord, int spacing) const;
	bool snow_capped(int y) const { return y >= snow_threshold_y_; }
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
	void set_elevation_data(
			const elevation::ProcessedElevationData &data, double latitude_for_snow_line);
	std::optional<elevation::ElevationAffine> elevation_affine() const
	{
		// Rust returns None unless sampled ElevationData exists as well as
		// elevation being enabled; metadata alone is not a usable terrain grid.
		if (!elevation_enabled || elevation_grid.empty() || elevation_world_width == 0 ||
				elevation_world_height == 0 || elevation_grid.front().empty())
			return std::nullopt;
		return elevation::ElevationAffine{elevation_min_height_m,
				elevation_blocks_per_meter, elevation_ground_level.value_or(0),
				elevation_soft_top ? std::optional<elevation::SoftTop>(elevation::SoftTop{
											 elevation_soft_top->knee_m,
											 elevation_soft_top->width_blocks})
								   : std::nullopt};
	}
	void set_elevation_enabled(bool enabled) { elevation_enabled = enabled; }
	void clear_elevation_data();
	void set_world_dims(std::size_t world_width, std::size_t world_height);
	void apply_osm_water_override(
			const std::vector<ProcessedElement> &elements, const XZBBox &bbox);
	void apply_osm_land_override(const std::vector<ProcessedElement> &elements,
			const XZBBox &bbox, double scale);
	void finish_land_cover(GenerationMode mode);
	void mark_beaches();
	void apply_bridge_land_cover_repair(const std::vector<ProcessedElement> &elements,
			const XZBBox &bbox, double scale);
	void set_rotation_mask(RotationMask mask) { rotation_mask = mask; }
	bool inside_rotation_mask(int x, int z) const;
	bool is_in_rotated_bounds(int x, int z) const { return inside_rotation_mask(x, z); }
	biome::Climate climate() const { return climate_state; }
	void set_climate(biome::Climate value) { climate_state = value; }
	bool winter() const { return winter_state; }
	void set_winter(bool value) { winter_state = value; }
	void set_ecoregion_map(std::shared_ptr<const ecoregion::EcoMap> map)
	{
		ecoregions = std::move(map);
	}
	void set_ecoregion_map(ecoregion::EcoMap map)
	{
		ecoregions = std::make_shared<const ecoregion::EcoMap>(std::move(map));
	}
	std::optional<ecoregion::Ecoregion> ecoregion_at(const XZPoint &coord) const;
	// Rust-compatible query name; retain ecoregion_at for existing Freeminer
	// callers while exposing Ground::ecoregion to library consumers.
	std::optional<ecoregion::Ecoregion> ecoregion(const XZPoint &coord) const
	{
		return ecoregion_at(coord);
	}
	// Rust Ground::ecoregion_map exposes a non-owning view of the full map.
	const ecoregion::EcoMap *ecoregion_map() const { return ecoregions.get(); }

	void set_land_cover_data(land_cover::LandCoverData data, std::size_t world_width,
			std::size_t world_height);
	void set_land_cover_data(const std::vector<std::vector<std::uint8_t>> &grid,
			const std::vector<std::vector<std::uint8_t>> &water_distance,
			std::size_t width, std::size_t height);

	std::pair<std::size_t, std::size_t> land_cover_index(const XZPoint &coord) const;

	uint8_t cover_class(const XZPoint &coord) const;
	bool has_cover_neighbour(const XZPoint &coord, std::uint8_t class_id) const;

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

struct GroundFetchPlan
{
	geographic::LLBBox bbox;
	std::tuple<std::size_t, std::size_t, std::size_t, std::size_t> dims;
	std::size_t pad_blocks = 0;
	std::pair<std::size_t, std::size_t> final_dims;
};

// The Rust GroundFrame contract, split from the Freeminer host so library
// callers can prepare local or projected/padded grids before generation.
struct GroundFrame
{
	std::optional<std::pair<std::size_t, std::size_t>> world_dims;
	std::optional<projection::WebMercatorProjection> mercator;
	std::size_t pad_blocks = 0;
	elevation::AffinePolicy affine{};

	static GroundFrame local();
	static GroundFrame from_args(const Args &, const geographic::LLBBox &);
	static GroundFrame projected(const geographic::LLBBox &, double scale,
			std::size_t pad_blocks = 0, const elevation::AffinePolicy &affine = {},
			std::optional<std::pair<double, double>> projection_origin = std::nullopt);
	GroundFetchPlan fetch_plan(const geographic::LLBBox &, double scale) const;
	biome::Climate climate(const geographic::LLBBox &) const;
	double anchor_lat(const geographic::LLBBox &) const;
	std::shared_ptr<const ecoregion::EcoMap> ecoregions(const geographic::LLBBox &,
			std::size_t world_width, std::size_t world_height) const;
};

int min_ground_level_for(const Args &args);
int extended_min_y_for(const Args &args);
int extended_max_y_for(const Args &args);
int world_top_y_for(const Args &args);
Ground generate_ground_data(const Args &args, const geographic::LLBBox &bbox,
		const std::filesystem::path &cache_base = {},
		const elevation::TerrainHints &terrain_hints = {});
// Convenience entry points matching Rust's OSM-to-terrain hint handoff.
// Natural peaks always inform terrain; tall-model headroom is used only when
// 3D model placement is enabled.
Ground generate_ground_data_from_osm(const Args &args, const geographic::LLBBox &bbox,
		const osm_parser::RawOsmDocument &osm,
		const std::filesystem::path &cache_base = {});
Ground generate_ground_data(const Args &args, const geographic::LLBBox &bbox,
		const std::filesystem::path &cache_base, const elevation::PeakSource &peaks);
Ground generate_ground_data(const Args &args, const geographic::LLBBox &bbox,
		const GroundFrame &frame, const std::filesystem::path &cache_base = {},
		const elevation::TerrainHints &terrain_hints = {});
Ground generate_ground_data_from_osm(const Args &args, const geographic::LLBBox &bbox,
		const GroundFrame &frame, const osm_parser::RawOsmDocument &osm,
		const std::filesystem::path &cache_base = {});
Ground generate_ground_data(const Args &args, const geographic::LLBBox &bbox,
		const GroundFrame &frame, const std::filesystem::path &cache_base,
		const elevation::PeakSource &peaks);

}
