#include "arnis_ground.h"
#include "arnis-cpp/src/coordinate_system/cartesian/xzpoint.h"

#include "arnis-cpp/src/cache_root.h"
#include "arnis-cpp/src/block_definitions.h"
#include "arnis-cpp/src/climate.h"
#include "arnis-cpp/src/elevation/planetary.h"
#include "arnis-cpp/src/elevation/cache.h"
#include "arnis-cpp/src/elevation/selector.h"
#include "arnis-cpp/src/coordinate_system/transformation.h"
#include "arnis-cpp/src/projection/projection_spec.h"
#include "arnis-cpp/src/fm_ecoregion_cache.h"
#include "arnis-cpp/src/grid_ops.h"
#include "arnis-cpp/src/land_cover/osm_water_override.h"
#include "arnis-cpp/src/land_cover/naturalize.h"
#include "arnis-cpp/src/osm_parser.h"
#include "arnis-cpp/src/elevation/peaks.h"
#include "arnis-cpp/src/water_depth.h"
#include "arnis-cpp/src/world_editor/floor_state.h"

#include <future>
#include <iostream>
#include <png.h>
#include <stdexcept>

namespace arnis
{
namespace
{
// Rust's `f64::round() as i32` rounds ties away from zero, maps NaN to zero,
// and saturates infinities/out-of-range values. Avoid `llround` followed by a
// narrowing cast, whose result is not defined for those boundary inputs.
int rust_round_i32(double value)
{
	if (std::isnan(value))
		return 0;
	if (value >= static_cast<double>(std::numeric_limits<int>::max()))
		return std::numeric_limits<int>::max();
	if (value <= static_cast<double>(std::numeric_limits<int>::min()))
		return std::numeric_limits<int>::min();
	return static_cast<int>(std::round(value));
}

std::string png_filename(const std::string &filename)
{
	return filename.ends_with(".png") ? filename : filename + ".png";
}

bool write_rgb_png(const std::string &filename, std::size_t width, std::size_t height,
		const std::vector<std::uint8_t> &pixels)
{
	if (width == 0 || height == 0 || width > std::numeric_limits<png_uint_32>::max() ||
			height > std::numeric_limits<png_uint_32>::max() ||
			width > std::numeric_limits<std::size_t>::max() / height / 3 ||
			pixels.size() != width * height * 3)
		return false;
	png_image image{};
	image.version = PNG_IMAGE_VERSION;
	image.width = static_cast<png_uint_32>(width);
	image.height = static_cast<png_uint_32>(height);
	image.format = PNG_FORMAT_RGB;
	const bool ok = png_image_write_to_file(&image, png_filename(filename).c_str(), 0,
							pixels.data(), 0, nullptr) != 0;
	if (!ok)
		std::cerr << image.message << '\n';
	png_image_free(&image);
	return ok;
}

elevation::TerrainHints terrain_hints_for(
		const Args &args, const osm_parser::RawOsmDocument &osm)
{
	elevation::TerrainHints hints;
	hints.peaks = elevation::peaks_in(osm);
	if (args.use_3d)
		hints.tall_models = elevation::tall_models_in(osm);
	return hints;
}
} // namespace

int extended_min_y_for(const Args &args)
{
	if (args.disable_height_limit && !args.bedrock && !args.luanti)
		return -2032;
	return world_editor::DEFAULT_MIN_Y;
}

int extended_max_y_for(const Args &args)
{
	if (args.bedrock)
		return 512;
	if (args.luanti)
		return world_editor::DEFAULT_MAX_Y;
	return 2031;
}

int world_top_y_for(const Args &args)
{
	if (!args.disable_height_limit || args.luanti)
		return world_editor::DEFAULT_MAX_Y;
	if (args.bedrock)
		return 511;
	return 2031;
}

int min_ground_level_for(const Args &args)
{
	const int floor = extended_min_y_for(args);
	return floor >= world_editor::DEFAULT_MIN_Y ? args.ground_level : floor + 2;
}

Ground generate_ground_data(const Args &args, const geographic::LLBBox &bbox,
		const std::filesystem::path &cache_base,
		const elevation::TerrainHints &terrain_hints)
{
	return generate_ground_data(
			args, bbox, GroundFrame::from_args(args, bbox), cache_base, terrain_hints);
}

Ground generate_ground_data_from_osm(const Args &args, const geographic::LLBBox &bbox,
		const osm_parser::RawOsmDocument &osm, const std::filesystem::path &cache_base)
{
	auto terrain_hints = terrain_hints_for(args, osm);
	return generate_ground_data(args, bbox, cache_base, terrain_hints);
}

Ground generate_ground_data(const Args &args, const geographic::LLBBox &bbox,
		const std::filesystem::path &cache_base, const elevation::PeakSource &peaks)
{
	const auto terrain_hints = peaks.take();
	return generate_ground_data(args, bbox, cache_base, terrain_hints);
}

GroundFrame GroundFrame::local()
{
	return {};
}

GroundFrame GroundFrame::from_args(const Args &args, const geographic::LLBBox &bbox)
{
	const auto spec = projection::ProjectionSpec::from_args(args);
	// Match Rust GroundFrame::from_args: an explicitly local projection stays
	// local even when One World state is present. The One World affine/padding
	// policy only applies after a projected transformer has been selected.
	if (spec.kind == projection::ProjectionKind::Local)
		return local();
	if (args.one_world_run) {
		elevation::AffinePolicy policy = elevation::AffinePolicy::fit_with_headroom();
		if (args.one_world_run->elevation) {
			const auto &stored = *args.one_world_run->elevation;
			std::optional<elevation::SoftTop> soft_top;
			if (stored.soft_top)
				soft_top = elevation::SoftTop{
						stored.soft_top->knee_m, stored.soft_top->width_blocks};
			policy = elevation::AffinePolicy::fixed_mapping({stored.min_height_m,
					stored.blocks_per_meter, stored.ground_level, soft_top});
		}
		try {
			return projected(bbox, spec.scale,
					static_cast<std::size_t>(one_world::ground_pad_blocks(args.scale)),
					policy, spec.origin);
		} catch (const std::exception &) {
			// ProjectionSpec::transformer in Rust returns an error for a bbox
			// outside the supported projected domain, and GroundFrame deliberately
			// falls back to local coordinates in that case.
			return local();
		}
	}
	try {
		return projected(bbox, spec.scale);
	} catch (const std::exception &) {
		return local();
	}
}

GroundFrame GroundFrame::projected(const geographic::LLBBox &bbox, double scale,
		std::size_t pad, const elevation::AffinePolicy &policy,
		std::optional<std::pair<double, double>> projection_origin)
{
	const projection::ProjectionSpec spec{
			projection::ProjectionKind::WebMercator, projection_origin, scale, 0};
	const auto &[transformer, rect] = spec.transformer(bbox);
	(void)transformer;
	GroundFrame frame;
	frame.world_dims = std::pair<std::size_t, std::size_t>{
			static_cast<std::size_t>(rect.total_blocks_x()),
			static_cast<std::size_t>(rect.total_blocks_z())};
	frame.mercator = spec.mercator(bbox);
	frame.pad_blocks = pad;
	frame.affine = policy;
	return frame;
}

double GroundFrame::anchor_lat(const geographic::LLBBox &bbox) const
{
	return (bbox.min().lat() + bbox.max().lat()) * 0.5;
}

biome::Climate GroundFrame::climate(const geographic::LLBBox &bbox) const
{
	return climate::classify_bbox(
			bbox.min().lat(), bbox.min().lng(), bbox.max().lat(), bbox.max().lng());
}

std::shared_ptr<const ecoregion::EcoMap> GroundFrame::ecoregions(
		const geographic::LLBBox &bbox, std::size_t world_width,
		std::size_t world_height) const
{
	if (!ecoregion::generation_map().map || !world_width || !world_height)
		return {};
	if (mercator) {
		const int x0 =
				projection::snap_edge(mercator->x_for_lon(bbox.min().lng()), false);
		const int z0 =
				projection::snap_edge(mercator->z_for_lat(bbox.max().lat()), false);
		auto map = ecoregion::EcoMap::build(world_width, world_height, {x0, z0},
				[projection = *mercator, x0, z0](double gx, double gz) {
					return std::pair<double, double>{
							projection.lat_for_z(z0 + gz), projection.lon_for_x(x0 + gx)};
				});
		return map ? std::make_shared<const ecoregion::EcoMap>(std::move(*map))
				   : std::shared_ptr<const ecoregion::EcoMap>{};
	}
	const double top = bbox.max().lat(), left = bbox.min().lng();
	const double dlat = top - bbox.min().lat();
	const double dlon = bbox.max().lng() - left;
	auto map = ecoregion::EcoMap::build(world_width, world_height, {0, 0},
			[top, left, dlat, dlon, world_width, world_height](double gx, double gz) {
				return std::pair<double, double>{top - gz / double(world_height) * dlat,
						left + gx / double(world_width) * dlon};
			});
	return map ? std::make_shared<const ecoregion::EcoMap>(std::move(*map))
			   : std::shared_ptr<const ecoregion::EcoMap>{};
}

GroundFetchPlan GroundFrame::fetch_plan(
		const geographic::LLBBox &bbox, double scale) const
{
	if (!world_dims) {
		const auto dims = elevation::compute_grid_dims(bbox, scale);
		return {bbox, dims, 0, {std::get<0>(dims), std::get<1>(dims)}};
	}
	const auto &[world_width, world_height] = *world_dims;
	if (pad_blocks <= (std::numeric_limits<std::size_t>::max() - world_width) / 2 &&
			pad_blocks <= (std::numeric_limits<std::size_t>::max() - world_height) / 2) {
		const auto padded_width = world_width + 2 * pad_blocks;
		const auto padded_height = world_height + 2 * pad_blocks;
		const auto padded_dims =
				elevation::compute_grid_dims_for_world(padded_width, padded_height);
		if (mercator && std::get<2>(padded_dims) == padded_width &&
				std::get<3>(padded_dims) == padded_height) {
			const double x0 =
					projection::snap_edge(mercator->x_for_lon(bbox.min().lng()), false) -
					double(pad_blocks) + 0.5;
			const double z0 =
					projection::snap_edge(mercator->z_for_lat(bbox.max().lat()), false) -
					double(pad_blocks) + 0.5;
			const double x1 = x0 + double(padded_width - 1);
			const double z1 = z0 + double(padded_height - 1);
			// Rust's LLBBox::new returns Err when padding pushes sample centres
			// outside geographic bounds; in that case it drops padding and retries
			// with the original bbox. Keep the same fallback here instead of
			// letting the checked C++ constructor abort world generation.
			try {
				const geographic::LLBBox centers(mercator->lat_for_z(z1),
						mercator->lon_for_x(x0), mercator->lat_for_z(z0),
						mercator->lon_for_x(x1));
				return {centers, padded_dims, pad_blocks, {world_width, world_height}};
			} catch (const std::invalid_argument &) {
				// Fall through to Rust's unpadded fetch plan below.
			}
		}
	}
	const auto dims = elevation::compute_grid_dims_for_world(world_width, world_height);
	return {bbox, dims, 0, {world_width, world_height}};
}

Ground generate_ground_data(const Args &args, const geographic::LLBBox &bbox,
		const GroundFrame &frame, const std::filesystem::path &cache_base,
		const elevation::TerrainHints &terrain_hints)
{
	if (!args.valid())
		throw std::invalid_argument("invalid Arnis ground-generation options");
	// Rust only collects tall-model hints when 3D placement is enabled. Enforce
	// that policy here too, including callers that supply TerrainHints directly
	// instead of going through generate_ground_data_from_osm(). Natural peaks
	// remain active independently of the model-placement toggle.
	elevation::TerrainHints effective_terrain_hints;
	const elevation::TerrainHints *hints = &terrain_hints;
	if (!args.use_3d && !terrain_hints.tall_models.empty()) {
		effective_terrain_hints.peaks = terrain_hints.peaks;
		hints = &effective_terrain_hints;
	}
	// Match Rust's once-daily background sweep of stale elevation/land-cover
	// tiles. Keep the host-supplied cache root authoritative when present.
	elevation::spawn_throttled_cleanup(
			cache_base.empty() ? cache::tile_cache_root() : cache_base);
	world_editor::set_terrain_top_y(args.ground_level);

	const auto plan = frame.fetch_plan(bbox, args.scale);
	const auto &fetch_bbox = plan.bbox;
	const auto &[world_width, world_height, grid_width, grid_height] = plan.dims;
	const auto &[final_width, final_height] = plan.final_dims;
	const auto &pad = plan.pad_blocks;
	const auto bounds = land_cover::GeographicBounds{fetch_bbox.min().lat(),
			fetch_bbox.min().lng(), fetch_bbox.max().lat(), fetch_bbox.max().lng()};
	const auto canopy_root = cache_base.empty() ? cache::provider_cache_root("canopy")
												: cache_base / "canopy";
	const auto elevation_root = cache_base.empty()
										? cache::provider_cache_root("elevation")
										: cache_base / "elevation";

	Ground ground = Ground::new_flat(args.ground_level);
	ground.set_celestial_body(args.body);
	ground.set_extended_ceiling(args.disable_height_limit &&
								extended_max_y_for(args) > world_editor::DEFAULT_MAX_Y);
	ground.set_climate(frame.climate(bbox));

	const bool earth = is_earth(args.body);
	std::future<std::optional<canopy::CanopyData>> canopy_job;
	if (earth && args.canopy_height) {
		canopy_job = std::async(std::launch::async, [&, grid_width = grid_width,
															grid_height = grid_height]() {
			return canopy::fetch_canopy_data(canopy_root, bounds.min_lat, bounds.min_lng,
					bounds.max_lat, bounds.max_lng, grid_width, grid_height);
		});
	}

	std::optional<land_cover::LandCoverData> cover;
	if (earth) {
		auto fetched = land_cover::fetch_land_cover_data(bounds, grid_width, grid_height);
		if (fetched.width && fetched.height)
			cover = std::move(fetched);
	}

	bool elevation_ready = !args.terrain_enabled();
	std::optional<int> elevation_lowest_y;
	if (args.terrain_enabled()) {
		try {
			const int max_carve_depth =
					cover ? water_depth::estimate_max_carve_depth(
									cover->grid, world_width, world_height)
						  : 0;
			const int carve_floor = world_editor::min_y() + max_carve_depth + 2;
			const int water_floor = std::max(args.ground_level, carve_floor);
			const int sink_floor = std::min(
					std::max(min_ground_level_for(args), carve_floor), water_floor);
			const int max_y = extended_max_y_for(args);
			elevation::ProcessedElevationData processed;
			if (earth) {
				elevation::Selector selector(elevation_root,
						args.aws_only_elevation
								? elevation::providers::SourceMode::AwsOnly
								: elevation::providers::SourceMode::Auto);
				processed = elevation::fetch_elevation_data(selector, fetch_bbox,
						world_width, world_height, grid_width, grid_height, args.scale,
						args.height_multiplier, water_floor, sink_floor,
						args.disable_height_limit, max_y, true, cover ? &*cover : nullptr,
						frame.affine, {}, *hints);
			} else {
				auto raw = elevation::fetch_planetary_elevation(args.body, fetch_bbox,
						grid_width, grid_height, elevation::http_planetary_range_reader(),
						elevation_root);
				if (!raw)
					throw std::runtime_error(
							"planetary elevation provider returned no data");
				raw->world_width = world_width;
				raw->world_height = world_height;
				processed = elevation::process_elevation_data(fetch_bbox, std::move(*raw),
						args.scale, args.height_multiplier, water_floor, sink_floor,
						args.disable_height_limit, max_y, false, nullptr, frame.affine,
						{}, *hints);
			}
			if (frame.mercator) {
				const auto source = [&](std::size_t z) {
					return grid_ops::mercator_source_row(fetch_bbox.max().lat(),
							fetch_bbox.min().lat(), processed.heights.size(), z);
				};
				const auto lerp = [](float a, float b, double t) {
					return static_cast<float>(double(a) * (1.0 - t) + double(b) * t);
				};
				grid_ops::remap_rows_in_place(processed.heights, source, lerp);
				if (cover)
					cover->remap_rows_to_mercator(
							fetch_bbox.max().lat(), fetch_bbox.min().lat());
			}
			if (pad) {
				grid_ops::crop_rows(
						processed.heights, pad, pad, final_width, final_height);
				processed.width =
						processed.heights.empty() ? 0 : processed.heights.front().size();
				processed.height = processed.heights.size();
				processed.world_width = final_width;
				processed.world_height = final_height;
				if (cover)
					cover->crop(pad, pad, final_width, final_height);
			}
			elevation_lowest_y = processed.lowest_y();
			ground.set_elevation_data(processed, frame.anchor_lat(bbox));
			elevation_ready = true;
		} catch (const std::exception &e) {
			std::cerr << "Failed to fetch elevation data: " << e.what()
					  << "; using flat ground.\n";
			ground = Ground::new_flat(args.ground_level);
			ground.set_celestial_body(args.body);
			ground.set_climate(frame.climate(bbox));
			cover.reset();
		}
	} else {
		ground.set_ground_level(args.ground_level);
		ground.set_world_dims(final_width, final_height);
		if (cover && frame.mercator)
			cover->remap_rows_to_mercator(fetch_bbox.max().lat(), fetch_bbox.min().lat());
		if (cover && pad)
			cover->crop(pad, pad, final_width, final_height);
	}

	if (cover && elevation_ready)
		ground.set_land_cover_data(std::move(*cover), final_width, final_height);
	if (canopy_job.valid()) {
		try {
			auto canopy_result = canopy_job.get();
			if (canopy_result && frame.mercator)
				canopy_result->remap_rows_to_mercator(
						fetch_bbox.max().lat(), fetch_bbox.min().lat());
			if (canopy_result && pad)
				canopy_result->crop(pad, pad, final_width, final_height);
			if (elevation_ready && canopy_result)
				ground.set_canopy_data(
						std::move(*canopy_result), final_width, final_height);
		} catch (const std::exception &e) {
			std::clog << "Canopy data unavailable: " << e.what() << '\n';
		}
	}
	if (earth)
		if (auto map = frame.ecoregions(bbox, final_width, final_height))
			ground.set_ecoregion_map(std::move(map));
	int base = ground.base_level(args.ground_level);
	if (args.one_world_run && args.disable_height_limit && ground.elevation_enabled) {
		// Rust's area_floor_for follows the actual lowest block in this
		// generated area rather than the affine reference plane.
		if (elevation_lowest_y)
			base = *elevation_lowest_y;
	}
	world_editor::set_base_chunk_y(base);
	world_editor::set_terrain_floor_y(base);
	using namespace block_definitions;
	const Block filler = args.body == CelestialBody::Moon	? ANDESITE
						 : args.body == CelestialBody::Mars ? RED_TERRACOTTA
															: GRASS_BLOCK;
	world_editor::set_base_chunk_block_id(static_cast<std::uint16_t>(filler.id()));
	// Rust returns Ground with the requested seasonal state already installed;
	// callers that use this generator directly must not depend on the later
	// data-processing pass to enable winter styling.
	ground.set_winter(args.winter);
	if (args.debug && args.terrain_enabled()) {
		ground.save_debug_image("elevation_debug");
		ground.save_land_cover_debug_image("landcover_debug");
		ground.save_canopy_debug_image("canopy_debug");
	}
	return ground;
}

Ground generate_ground_data_from_osm(const Args &args, const geographic::LLBBox &bbox,
		const GroundFrame &frame, const osm_parser::RawOsmDocument &osm,
		const std::filesystem::path &cache_base)
{
	auto terrain_hints = terrain_hints_for(args, osm);
	return generate_ground_data(args, bbox, frame, cache_base, terrain_hints);
}

Ground generate_ground_data(const Args &args, const geographic::LLBBox &bbox,
		const GroundFrame &frame, const std::filesystem::path &cache_base,
		const elevation::PeakSource &peaks)
{
	const auto terrain_hints = peaks.take();
	return generate_ground_data(args, bbox, frame, cache_base, terrain_hints);
}

std::optional<int> Ground::min_level(const std::vector<XZPoint> &points) const
{
	return min_level(points.begin(), points.end());
}
std::optional<int> Ground::max_level(const std::vector<XZPoint> &points) const
{
	return max_level(points.begin(), points.end());
}
int Ground::level(const XZPoint &pos) const
{
	// Rust falls back to its configured base when no elevation raster is
	// available. Freeminer may explicitly provide its own host-height callback;
	// keep that adapter-specific fallback separate from the standalone API.
	if (elevation_enabled && (elevation_grid.empty() || elevation_world_width == 0 ||
									 elevation_world_height == 0))
		return host_elevation ? host_elevation(pos) : elevation_ground_level.value_or(0);
	if (elevation_enabled && !elevation_grid.empty() && elevation_world_width > 0 &&
			elevation_world_height > 0) {
		const auto height = elevation_grid.size();
		const auto width = elevation_grid.front().size();
		if (width > 0 && height > 0) {
			const double xr =
					std::clamp(double(pos.x) / double(std::max<std::size_t>(
													   1, elevation_world_width - 1)),
							0.0, 1.0) *
					double(width - 1);
			const double zr =
					std::clamp(double(pos.z) / double(std::max<std::size_t>(
													   1, elevation_world_height - 1)),
							0.0, 1.0) *
					double(height - 1);
			const auto x0 = std::min<std::size_t>(std::size_t(std::floor(xr)), width - 1);
			const auto z0 =
					std::min<std::size_t>(std::size_t(std::floor(zr)), height - 1);
			const auto x1 = std::min(width - 1, x0 + 1),
					   z1 = std::min(height - 1, z0 + 1);
			const double tx = xr - std::floor(xr), tz = zr - std::floor(zr);
			const double v00 = elevation_grid[z0][x0];
			const double v10 = elevation_grid[z0][x1];
			const double v01 = elevation_grid[z1][x0];
			const double v11 = elevation_grid[z1][x1];
			const double lerp_top = v00 + (v10 - v00) * tx;
			const double lerp_bottom = v01 + (v11 - v01) * tx;
			return rust_round_i32(lerp_top + (lerp_bottom - lerp_top) * tz);
		}
	}
	// Match Rust Ground::level: a flat/land-cover-only ground is anchored at
	// the configured ground level. Freeminer's host terrain is not an implicit
	// substitute for Arnis elevation data; using it here made GeoOnly vary from
	// the Rust generator and caused later object/terrain passes to disagree.
	return elevation_ground_level.value_or(0);
}

double Ground::level_exact(const XZPoint &pos) const
{
	// Rust returns its configured flat base whenever elevation is disabled.
	// Keep the continuous query consistent with level(); a Freeminer host-height
	// fallback is used only when terrain mode is enabled and no raster exists.
	if (!elevation_enabled)
		return static_cast<double>(elevation_ground_level.value_or(0));
	if (elevation_grid.empty() || elevation_world_width == 0 ||
			elevation_world_height == 0)
		return static_cast<double>(host_elevation ? host_elevation(pos)
												  : elevation_ground_level.value_or(0));
	const auto height = elevation_grid.size();
	const auto width = elevation_grid.front().size();
	if (width == 0 || height == 0)
		return static_cast<double>(elevation_ground_level.value_or(0));
	const double xr = std::clamp(double(pos.x) / double(std::max<std::size_t>(
														 1, elevation_world_width - 1)),
							  0.0, 1.0) *
					  double(width - 1);
	const double zr = std::clamp(double(pos.z) / double(std::max<std::size_t>(
														 1, elevation_world_height - 1)),
							  0.0, 1.0) *
					  double(height - 1);
	const auto x0 =
			std::min<std::size_t>(static_cast<std::size_t>(std::floor(xr)), width - 1);
	const auto z0 =
			std::min<std::size_t>(static_cast<std::size_t>(std::floor(zr)), height - 1);
	const auto x1 = std::min(width - 1, x0 + 1);
	const auto z1 = std::min(height - 1, z0 + 1);
	const double tx = xr - std::floor(xr), tz = zr - std::floor(zr);
	const double v00 = elevation_grid[z0][x0];
	const double v10 = elevation_grid[z0][x1];
	const double v01 = elevation_grid[z1][x0];
	const double v11 = elevation_grid[z1][x1];
	const double lerp_top = v00 + (v10 - v00) * tx;
	const double lerp_bottom = v01 + (v11 - v01) * tx;
	return lerp_top + (lerp_bottom - lerp_top) * tz;
}

double Ground::slope_exact(const XZPoint &pos) const
{
	return slope_and_gradient(pos).first;
}

double Ground::slope_soft_top_stretch(int y) const
{
	if (!elevation_soft_top || elevation_blocks_per_meter <= 0.0 ||
			elevation_soft_top->width_blocks <= 0.0)
		return 1.0;
	const auto &top = *elevation_soft_top;
	const double knee_y = base_level() + (top.knee_m - elevation_min_height_m) *
												 elevation_blocks_per_meter;
	const double above = static_cast<double>(y) - knee_y;
	return above <= 0.0 ? 1.0 : std::cosh(above / top.width_blocks);
}

std::pair<double, std::pair<double, double>> Ground::slope_and_gradient(
		const XZPoint &pos) const
{
	if (!elevation_enabled)
		return {0.0, {0.0, 0.0}};
	constexpr int step = 4;
	const std::array<double, 4> samples{{level_exact({pos.x + step, pos.z}),
			level_exact({pos.x - step, pos.z}), level_exact({pos.x, pos.z - step}),
			level_exact({pos.x, pos.z + step})}};
	const auto &[min_it, max_it] = std::minmax_element(samples.begin(), samples.end());
	const double raw = *max_it - *min_it;
	const int mid_y = rust_round_i32((*min_it + *max_it) * 0.5);
	const double slope = std::max(
			0.0, raw * elevation_slope_correction * slope_soft_top_stretch(mid_y));
	return {slope, {samples[0] - samples[1], samples[3] - samples[2]}};
}

double Ground::convexity(const XZPoint &pos) const
{
	if (!elevation_enabled)
		return 0.0;
	constexpr int radius = 8;
	constexpr int diagonal = 6;
	const std::array<std::pair<int, int>, 8> ring{{{radius, 0}, {-radius, 0}, {0, radius},
			{0, -radius}, {diagonal, diagonal}, {diagonal, -diagonal},
			{-diagonal, diagonal}, {-diagonal, -diagonal}}};
	double mean = 0.0;
	for (const auto &[dx, dz] : ring)
		mean += level_exact({pos.x + dx, pos.z + dz});
	mean /= static_cast<double>(ring.size());
	// The sample ring is twice the four-block slope baseline used by Rust.
	const int center_y = rust_round_i32(level_exact(pos));
	return (mean - level_exact(pos)) * 0.5 * elevation_slope_correction *
		   slope_soft_top_stretch(center_y);
}
bool Ground::has_land_cover() const
{
	// Match Rust Ground::has_land_cover: availability means the optional raster
	// was supplied; dimensions are handled by the accessors that consume it.
	return land_cover.has_value();
}
bool Ground::has_canopy() const
{
	// Rust's has_canopy() only tests whether the raster exists. Its coordinate
	// mapping uses Ground::world_dims(), not canopy-specific dimensions.
	return canopy_data.has_value();
}

std::optional<ecoregion::Ecoregion> Ground::ecoregion_at(const XZPoint &coord) const
{
	if (!ecoregions)
		return std::nullopt;
	// EcoMap::build uses the projected alignment only to phase its sampling
	// lattice; the stored cells and Ground queries remain area-local. Match
	// Rust Ground::ecoregion, which passes the ground coordinate straight to
	// EcoMap::at rather than adding that alignment a second time.
	if (ecoregions->is_local())
		return ecoregions->at(cartesian::XZPoint{coord.x, coord.z});
	if (!mg)
		return std::nullopt;
	// WorldEditor passes ground-local coordinates here. The global ecoregion
	// raster is geographic, so project the corresponding world block position;
	// omitting node_min made every streamed chunk query near the map origin.
	const auto [lat, lon] = mg->pos_to_ll(static_cast<pos_t>(coord.X + mg->node_min.X),
			static_cast<pos_t>(coord.Y + mg->node_min.Z));
	const auto id = ecoregions->id_at(lat, lon);
	return id ? ecoregion::lookup(*id) : std::nullopt;
}
double Ground::blocks_per_meter() const
{
	return elevation_enabled && elevation_blocks_per_meter > 0.0
				   ? elevation_blocks_per_meter
				   : 1.0;
}
int Ground::base_level(int fallback) const
{
	return elevation_ground_level.value_or(fallback);
}
std::pair<std::size_t, std::size_t> Ground::world_dims() const
{
	// Match Rust Ground::world_dims: elevation dimensions win when a sampled
	// elevation raster is installed; otherwise use the independent flat-mode
	// frame. Companion grids are only a fallback for legacy callers that have
	// not supplied either coordinate frame explicitly.
	if (elevation_world_width > 0 && elevation_world_height > 0)
		return {elevation_world_width, elevation_world_height};
	if (world_width > 0 && world_height > 0)
		return {world_width, world_height};
	if (has_land_cover())
		return {land_cover_world_width, land_cover_world_height};
	return {canopy_world_width, canopy_world_height};
}
void Ground::set_canopy_data(
		canopy::CanopyData data, std::size_t world_width, std::size_t world_height)
{
	canopy_data = std::move(data);
	canopy_world_width = world_width;
	canopy_world_height = world_height;
}
void Ground::set_canopy_data(
		const std::vector<std::uint8_t> &grid, std::size_t width, std::size_t height)
{
	// Rust's rotation-only overload replaces canopy pixels only when the Ground
	// already has canopy data. Do not turn a previously unavailable/disabled
	// canopy provider into an apparently valid all-zero raster during rotation.
	if (!canopy_data || width == 0 || height == 0)
		return;
	set_canopy_data(canopy::CanopyData(grid, width, height),
			canopy_world_width ? canopy_world_width : width,
			canopy_world_height ? canopy_world_height : height);
}

void Ground::save_debug_image(const std::string &filename) const
{
	if (elevation_grid.empty() || elevation_grid.front().empty())
		return;
	const std::size_t width = elevation_grid.front().size();
	const std::size_t height = elevation_grid.size();
	float min_height = std::numeric_limits<float>::max();
	float max_height = std::numeric_limits<float>::lowest();
	for (const auto &row : elevation_grid)
		for (float value : row)
			if (std::isfinite(value)) {
				min_height = std::min(min_height, value);
				max_height = std::max(max_height, value);
			}
	const float range = max_height - min_height;
	std::vector<std::uint8_t> pixels(width * height * 3);
	for (std::size_t z = 0; z < height; ++z)
		for (std::size_t x = 0; x < width; ++x) {
			const float value = elevation_grid[z][x];
			const auto gray = range > 0.0f && std::isfinite(value)
									  ? static_cast<std::uint8_t>(std::clamp(
												(value - min_height) / range * 255.0f,
												0.0f, 255.0f))
									  : static_cast<std::uint8_t>(128);
			const auto i = (z * width + x) * 3;
			pixels[i] = pixels[i + 1] = pixels[i + 2] = gray;
		}
	if (!write_rgb_png(filename, width, height, pixels))
		std::cerr << "Failed to save debug image: " << png_filename(filename) << '\n';
}

void Ground::save_land_cover_debug_image(const std::string &filename) const
{
	if (!land_cover || land_cover->width == 0 || land_cover->height == 0)
		return;
	using Color = std::array<std::uint8_t, 3>;
	const auto color_for = [](std::uint8_t klass) -> Color {
		switch (klass) {
		case land_cover::LC_TREE_COVER:
			return {0x00, 0x6e, 0x00};
		case land_cover::LC_SHRUBLAND:
			return {0xff, 0xbb, 0x22};
		case land_cover::LC_GRASSLAND:
			return {0xff, 0xff, 0x4c};
		case land_cover::LC_CROPLAND:
			return {0xf0, 0x96, 0xff};
		case land_cover::LC_BUILT_UP:
			return {0xfa, 0x00, 0x00};
		case land_cover::LC_BARE:
			return {0xb4, 0xb4, 0xb4};
		case land_cover::LC_SNOW_ICE:
			return {0xf0, 0xf0, 0xf0};
		case land_cover::LC_WATER:
			return {0x00, 0x64, 0xc8};
		case land_cover::LC_WETLAND:
			return {0x00, 0x96, 0xa0};
		case land_cover::LC_MANGROVES:
			return {0x00, 0xcf, 0x75};
		case land_cover::LC_MOSS:
			return {0xfa, 0xe6, 0xa0};
		case land_cover::LC_BEACH:
			return {0xe8, 0xd8, 0x9c};
		default:
			return {0x00, 0x00, 0x00};
		}
	};
	std::vector<std::uint8_t> pixels(land_cover->width * land_cover->height * 3);
	for (std::size_t z = 0; z < land_cover->height; ++z)
		for (std::size_t x = 0; x < land_cover->width; ++x) {
			const auto color = color_for(land_cover->grid[z][x]);
			const auto i = (z * land_cover->width + x) * 3;
			std::copy(color.begin(), color.end(), pixels.begin() + i);
		}
	if (!write_rgb_png(filename, land_cover->width, land_cover->height, pixels))
		std::cerr << "Failed to save land cover debug image: " << png_filename(filename)
				  << '\n';
}

void Ground::save_canopy_debug_image(const std::string &filename) const
{
	if (!canopy_data || canopy_data->width == 0 || canopy_data->height == 0)
		return;
	std::vector<std::uint8_t> pixels(canopy_data->width * canopy_data->height * 3);
	for (std::size_t z = 0; z < canopy_data->height; ++z)
		for (std::size_t x = 0; x < canopy_data->width; ++x) {
			const auto height = canopy_data->at(x, z);
			const auto i = (z * canopy_data->width + x) * 3;
			if (height == canopy::CANOPY_NODATA) {
				pixels[i] = pixels[i + 1] = pixels[i + 2] = 0;
			} else if (height < canopy::CANOPY_MIN_M) {
				pixels[i] = pixels[i + 1] = pixels[i + 2] = 0x30;
			} else {
				const float t = std::min(float(height) / 40.0f, 1.0f);
				pixels[i] = static_cast<std::uint8_t>(64.0f * (1.0f - t));
				pixels[i + 1] = static_cast<std::uint8_t>(64.0f + 191.0f * t);
				pixels[i + 2] = 48;
			}
		}
	if (!write_rgb_png(filename, canopy_data->width, canopy_data->height, pixels))
		std::cerr << "Failed to save canopy debug image: " << png_filename(filename)
				  << '\n';
}

std::pair<std::size_t, std::size_t> Ground::canopy_index(const XZPoint &coord) const
{
	const auto &c = *canopy_data;
	const auto [world_width, world_height] = world_dims();
	const double xr = std::clamp(
			double(coord.x) / double(std::max<std::size_t>(1, world_width - 1)), 0.0,
			1.0);
	const double zr = std::clamp(
			double(coord.z) / double(std::max<std::size_t>(1, world_height - 1)), 0.0,
			1.0);
	return {std::min<std::size_t>(std::llround(xr * double(c.width - 1)), c.width - 1),
			std::min<std::size_t>(std::llround(zr * double(c.height - 1)), c.height - 1)};
}
std::optional<std::uint8_t> Ground::canopy_height_m(const XZPoint &coord) const
{
	if (!has_canopy())
		return std::nullopt;
	const auto &[x, z] = canopy_index(coord);
	return canopy_data->canopy_height_m(x, z);
}
std::optional<std::pair<double, std::uint8_t>> Ground::canopy_cell(
		const XZPoint &origin, int span) const
{
	if (!has_canopy() || span <= 0)
		return std::nullopt;
	std::uint32_t measured = 0, wooded = 0;
	std::uint8_t top = 0;
	for (int dz = 0; dz < span; ++dz)
		for (int dx = 0; dx < span; ++dx)
			if (const auto height = canopy_height_m({origin.x + dx, origin.z + dz})) {
				++measured;
				if (*height >= canopy::CANOPY_MIN_M) {
					++wooded;
					top = std::max(top, *height);
				}
			}
	if (!measured)
		return std::nullopt;
	return std::pair{double(wooded) / measured, top};
}
std::optional<double> Ground::canopy_fraction(const XZPoint &coord, int spacing) const
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
double Ground::snow_line_meters(double latitude_degrees)
{
	const auto latitude = std::min(90.0, std::abs(latitude_degrees));
	if (latitude <= 25.0)
		return 4500.0 + (5700.0 - 4500.0) * latitude / 25.0;
	if (latitude <= 46.0)
		return 5700.0 + (3000.0 - 5700.0) * (latitude - 25.0) / (46.0 - 25.0);
	return std::max(0.0, 3000.0 * (1.0 - (latitude - 46.0) / (90.0 - 46.0)));
}

void Ground::set_snow_line_for_latitude(double latitude_degrees)
{
	if (!is_earth()) {
		snow_threshold_y_ = std::numeric_limits<int>::max();
		return;
	}
	const auto snowline = snow_line_meters(latitude_degrees);
	if (elevation_blocks_per_meter <= 0.0) {
		snow_threshold_y_ = elevation_min_height_m >= snowline
									? std::numeric_limits<int>::min()
									: std::numeric_limits<int>::max();
		return;
	}
	double y = base_level() +
			   (snowline - elevation_min_height_m) * elevation_blocks_per_meter;
	if (elevation_soft_top && snowline > elevation_soft_top->knee_m &&
			elevation_soft_top->width_blocks > 0.0) {
		const auto &top = *elevation_soft_top;
		const double knee_y = base_level() + (top.knee_m - elevation_min_height_m) *
													 elevation_blocks_per_meter;
		const double rise = (snowline - top.knee_m) * elevation_blocks_per_meter;
		y = knee_y + top.width_blocks * std::asinh(rise / top.width_blocks);
	}
	snow_threshold_y_ =
			y <= std::numeric_limits<int>::min()   ? std::numeric_limits<int>::min()
			: y >= std::numeric_limits<int>::max() ? std::numeric_limits<int>::max()
												   : static_cast<int>(std::llround(y));
}
void Ground::set_elevation_metadata(double min_height_m, double blocks_per_meter,
		int snow_y, int ground_level, double slope_correction,
		std::optional<ElevationSoftTop> soft_top)
{
	elevation_min_height_m = min_height_m;
	elevation_blocks_per_meter = blocks_per_meter;
	elevation_slope_correction = slope_correction > 0.0 ? slope_correction : 1.0;
	elevation_soft_top = soft_top;
	snow_threshold_y_ = snow_y;
	elevation_ground_level = ground_level;
}
void Ground::set_elevation_data(const std::vector<std::vector<float>> &heights,
		std::size_t, std::size_t, std::size_t world_width, std::size_t world_height)
{
	// This overload mirrors Rust's rotation-only replacement API: a transform
	// must not create elevation state when the source ground had none.
	if (elevation_grid.empty())
		return;
	elevation_grid = heights;
	elevation_world_width = world_width;
	elevation_world_height = world_height;
	elevation_enabled = !elevation_grid.empty();
}
void Ground::set_elevation_data(const std::vector<std::vector<double>> &heights,
		std::size_t, std::size_t, std::size_t world_width, std::size_t world_height)
{
	// As above, this is the Rust-parity rotation replacement overload, not the
	// initial provider-data installation path.
	if (elevation_grid.empty())
		return;
	elevation_grid.clear();
	elevation_grid.reserve(heights.size());
	for (const auto &row : heights) {
		elevation_grid.emplace_back();
		elevation_grid.back().reserve(row.size());
		for (double value : row)
			elevation_grid.back().push_back(static_cast<float>(value));
	}
	elevation_world_width = world_width;
	elevation_world_height = world_height;
	elevation_enabled = !elevation_grid.empty();
}

void Ground::set_elevation_data(
		const elevation::ProcessedElevationData &data, double latitude_for_snow_line)
{
	// Initial provider installation is separate from the Rust rotation setter:
	// this path is allowed to create elevation state on a fresh Ground.
	elevation_grid = data.heights;
	elevation_world_width = data.world_width;
	elevation_world_height = data.world_height;
	elevation_enabled = !elevation_grid.empty();
	std::optional<ElevationSoftTop> top;
	if (data.soft_top)
		top = ElevationSoftTop{data.soft_top->knee_m, data.soft_top->width_blocks};
	set_elevation_metadata(data.min_height_m, data.blocks_per_meter, snow_threshold_y_,
			data.ground_level, data.slope_correction, top);
	set_snow_line_for_latitude(latitude_for_snow_line);
}

void Ground::clear_elevation_data()
{
	elevation_grid.clear();
	elevation_world_width = elevation_world_height = 0;
	elevation_enabled = false;
}
void Ground::set_world_dims(std::size_t world_width, std::size_t world_height)
{
	this->world_width = world_width;
	this->world_height = world_height;
}
void Ground::apply_osm_water_override(
		const std::vector<ProcessedElement> &elements, const XZBBox &bbox)
{
	if (!land_cover || elevation_grid.empty())
		return;
	land_cover::apply_osm_water_override(*land_cover, elevation_grid,
			elevation_world_width, elevation_world_height, elements, bbox);
}
void Ground::apply_osm_land_override(
		const std::vector<ProcessedElement> &elements, const XZBBox &bbox, double scale)
{
	if (!land_cover)
		return;
	const auto [world_width, world_height] = world_dims();
	land_cover::apply_osm_land_override(
			*land_cover, world_width, world_height, elements, bbox, scale);
}
void Ground::mark_beaches()
{
	if (land_cover)
		land_cover::mark_beaches(*land_cover);
}
void Ground::finish_land_cover(GenerationMode mode)
{
	if (!land_cover)
		return;
	if (generation_mode_natural_land_cover(mode))
		land_cover::naturalize(*land_cover, &elevation_grid, climate_state);
	mark_beaches();
}
void Ground::apply_bridge_land_cover_repair(
		const std::vector<ProcessedElement> &elements, const XZBBox &bbox, double scale)
{
	if (!land_cover || elevation_grid.empty())
		return;
	land_cover::apply_bridge_land_cover_repair(*land_cover, elevation_grid,
			elevation_world_width, elevation_world_height, elements, bbox, scale);
}
bool Ground::inside_rotation_mask(int x, int z) const
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
void Ground::set_land_cover_data(
		land_cover::LandCoverData data, std::size_t world_width, std::size_t world_height)
{
	// Rust parity: src/ground.rs land-cover accessors. The active mapgen
	// populates this from ESA WorldCover, then applies OSM overrides.
	data.width = data.grid.empty() ? 0 : data.grid.front().size();
	data.height = data.grid.size();
	data.water_distance =
			land_cover::compute_water_distance(data.grid, data.width, data.height);
	data.invalidate_water_blend_grid();
	land_cover = std::move(data);
	land_cover_world_width = world_width;
	land_cover_world_height = world_height;
	if (world_width > 0 && world_height > 0) {
		this->world_width = world_width;
		this->world_height = world_height;
	}
}
void Ground::set_land_cover_data(const std::vector<std::vector<std::uint8_t>> &grid,
		const std::vector<std::vector<std::uint8_t>> &water_distance, std::size_t width,
		std::size_t height)
{
	if (!land_cover)
		return;
	land_cover->grid = grid;
	land_cover->water_distance = water_distance;
	land_cover->width = width;
	land_cover->height = height;
	land_cover->invalidate_water_blend_grid();
}
std::pair<std::size_t, std::size_t> Ground::land_cover_index(const XZPoint &coord) const
{
	// Rust parity: src/ground.rs::cover_class / water_distance sampling.
	const auto &lc = *land_cover;
	const auto &[world_width, world_height] = world_dims();
	const double x_ratio = std::clamp(
			static_cast<double>(coord.x) /
					static_cast<double>(std::max<std::size_t>(1, world_width - 1)),
			0.0, 1.0);
	const double z_ratio = std::clamp(
			static_cast<double>(coord.z) /
					static_cast<double>(std::max<std::size_t>(1, world_height - 1)),
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
uint8_t Ground::cover_class(const XZPoint &coord) const
{
	if (!has_land_cover())
		return 0;
	const auto &[x, z] = land_cover_index(coord);
	return land_cover->grid[z][x];
}

bool Ground::has_cover_neighbour(const XZPoint &coord, std::uint8_t class_id) const
{
	if (!land_cover || land_cover->grid.empty() || land_cover->width == 0 ||
			land_cover->height == 0)
		return false;
	const auto [gx, gz] = land_cover_index(coord);
	const auto [world_width, world_height] = world_dims();
	constexpr int offsets[4][2] = {{-1, 0}, {1, 0}, {0, -1}, {0, 1}};
	for (const auto &offset : offsets) {
		const int dx = offset[0];
		const int dz = offset[1];
		const auto nx = static_cast<std::int64_t>(gx) + dx;
		const auto nz = static_cast<std::int64_t>(gz) + dz;
		if (nx < 0 || nz < 0 || nx >= static_cast<std::int64_t>(land_cover->width) ||
				nz >= static_cast<std::int64_t>(land_cover->height))
			continue;
		// Rotation padding duplicates the selected raster edge. Do not treat a
		// clamped duplicate as a second independent cover cell.
		const int world_x = static_cast<int>(std::llround(
				double(nx) / double(std::max<std::size_t>(1, land_cover->width - 1)) *
				double(std::max<std::size_t>(1, world_width - 1))));
		const int world_z = static_cast<int>(std::llround(
				double(nz) / double(std::max<std::size_t>(1, land_cover->height - 1)) *
				double(std::max<std::size_t>(1, world_height - 1))));
		if (inside_rotation_mask(world_x, world_z) &&
				land_cover->grid[static_cast<std::size_t>(nz)]
								[static_cast<std::size_t>(nx)] == class_id)
			return true;
	}
	return false;
}
uint8_t Ground::water_distance(const XZPoint &coord) const
{
	if (!has_land_cover())
		return 0;
	const auto [x, z] = land_cover_index(coord);
	if (z >= land_cover->water_distance.size() ||
			x >= land_cover->water_distance[z].size())
		return 0;
	return land_cover->water_distance[z][x];
}
bool Ground::is_interior_water(const XZPoint &coord) const
{
	if (cover_class(coord) != land_cover::LC_WATER)
		return false;
	const auto distance = water_distance(coord);
	return distance == 0 || distance >= 4;
}
bool Ground::water_blend_is_water(const XZPoint &coord) const
{
	if (!has_land_cover())
		return false;
	const auto [world_width, world_height] = world_dims();
	return land_cover->water_blend_mask(world_width, world_height)
			.contains(coord.x, coord.z);
}
void Ground::warm_water_blend()
{
	if (land_cover) {
		const auto [world_width, world_height] = world_dims();
		(void)land_cover->water_blend_mask(world_width, world_height);
	}
}
std::optional<std::tuple<int, int, int, int>> Ground::lc_water_block_bounds() const
{
	// Rust parity: src/ground.rs::lc_water_block_bounds.
	// Used by water_depth to avoid scanning the full world bbox.
	if (!has_land_cover() || elevation_grid.empty() || elevation_world_width == 0 ||
			elevation_world_height == 0)
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

	// Rust derives the block bounds from ElevationData's world dimensions.
	// Reuse the same span mapper as water-depth preprocessing so the two paths
	// cannot drift on grid edges or degenerate dimensions.
	const auto [x0, x1] = water_depth::grid_span_to_block_span(
			gx0, gx1, elevation_world_width, lc.width);
	const auto [z0, z1] = water_depth::grid_span_to_block_span(
			gz0, gz1, elevation_world_height, lc.height);
	return std::tuple<int, int, int, int>{x0, z0, x1, z1};
}
int Ground::slope(const XZPoint &coord) const
{
	if (!elevation_enabled)
		return 0;
	constexpr int step = 4;
	const int east = level({coord.x + step, coord.z}),
			  west = level({coord.x - step, coord.z}),
			  north = level({coord.x, coord.z - step}),
			  south = level({coord.x, coord.z + step});
	// Rust uses saturating_sub here; avoid signed overflow when malformed
	// elevation metadata spans the complete integer range.
	const auto hi = std::max({east, west, north, south});
	const auto lo = std::min({east, west, north, south});
	const auto difference = static_cast<std::int64_t>(hi) - lo;
	const int raw = static_cast<int>(
			std::min<std::int64_t>(difference, std::numeric_limits<int>::max()));
	const auto midpoint = static_cast<std::int64_t>(lo) + raw / 2;
	const int mid_y = static_cast<int>(std::clamp<std::int64_t>(
			midpoint, std::numeric_limits<int>::min(), std::numeric_limits<int>::max()));
	const double scaled = static_cast<double>(raw) * elevation_slope_correction *
						  slope_soft_top_stretch(mid_y);
	return rust_round_i32(scaled);
}
int Ground::water_level(const XZPoint &coord) const
{
	const int center = level(coord);
	// Rust returns the configured flat level without DEM shoreline snapping,
	// regardless of whether the host mapgen happens to expose legacy terrain.
	if (!elevation_enabled)
		return center;
	if (slope(coord) <= 2)
		return center;
	constexpr int radius = 3;
	int lowest = center;
	for (int r = 1; r <= radius; ++r)
		for (const auto &[dx, dz] : std::array<std::pair<int, int>, 8>{{{-r, 0}, {r, 0},
					 {0, -r}, {0, r}, {-r, -r}, {-r, r}, {r, -r}, {r, r}}})
			lowest = std::min(lowest, level({coord.x + dx, coord.z + dz}));
	const int cliff_drop =
			extended_ceiling ? std::max(radius, rust_round_i32(25.0 * blocks_per_meter()))
							 : radius;
	// Match Rust's saturating_sub: malformed/overflowing DEM values must not
	// wrap into a negative drop and accidentally bypass the cliff guard.
	const auto difference = static_cast<std::int64_t>(center) - lowest;
	const int drop = static_cast<int>(
			std::min<std::int64_t>(difference, std::numeric_limits<int>::max()));
	return drop > cliff_drop ? center : lowest;
}

} // namespace arnis
