#include "test.h"
#include "mapgen/earth/arnis_ground.h"
#include "mapgen/earth/arnis-cpp/src/elevation/peaks.h"
#include "mapgen/earth/arnis-cpp/src/elevation/tile_math.h"
#include "mapgen/earth/arnis-cpp/src/coordinate_system/geographic/llbbox.h"
#include "mapgen/earth/arnis-cpp/src/terrain_surface.h"

#include <cmath>
#include <limits>
#include <utility>

class TestFmArnisTerrainSurface : public TestBase
{
public:
	TestFmArnisTerrainSurface() { TestManager::registerTestModule(this); }
	const char *getName() override { return "TestFmArnisTerrainSurface"; }
	void runTests(IGameDef *) override
	{
		TEST(testSnowCoverMatchesRustThresholds);
		TEST(testSnowDepthFollowsUnroundedTerrain);
		TEST(testSnowShadeMatchesRustPoleAndRotationRules);
		TEST(testDisabledSnowLineDoesNotSnow);
		TEST(testNonFinitePeakHintDoesNotPoisonElevationGrid);
		TEST(testNonFinitePeakResolutionDoesNotPoisonElevationGrid);
		TEST(testFlatWorldDimensionsDoNotOverwriteElevationDimensions);
		TEST(testTerrainTileSamplingIncludesSplineNeighbourTiles);
	}

	void testSnowCoverMatchesRustThresholds()
	{
		using namespace arnis::terrain_surface;
		UASSERT(snow_cover(1.5, 1.0, [] { return 0.0; }, 0.0, 3, 4).is_block());
		for (int x = 0; x < 64; ++x)
			for (int z = 0; z < 64; ++z)
				UASSERT(!snow_cover(1.5, 12.0, [] { return 0.0; }, 1.0, x, z).is_block());
		for (int x = 0; x < 96; ++x)
			for (int z = 0; z < 96; ++z)
				UASSERT(snow_cover(
						SNOW_MIN_DEPTH, 0.0, [] { return 2.0; }, 1.0, x, z)
								.is_none());
	}

	void testSnowDepthFollowsUnroundedTerrain()
	{
		using namespace arnis::terrain_surface;
		for (std::uint8_t rise = 0; rise < 8; ++rise) {
			const auto eighths = snow_eighths(
					Snow::block(), std::optional<double>(double(rise) / 8.0));
			UASSERT(eighths == rise);
		}
		UASSERT(snow_eighths(Snow::layer(7), 0.0) == 1);
		UASSERT(snow_eighths(Snow::layer(2), 0.9) == 2);
		UASSERT(snow_eighths(Snow::layer(7)) == 1);
		UASSERT(snow_eighths(Snow::block()) == 0);
		UASSERT(snow_eighths(Snow::layer(7), 3.0) == 1);
		UASSERT(snow_eighths(Snow::none(), 0.5) == 0);
	}

	void testSnowShadeMatchesRustPoleAndRotationRules()
	{
		using arnis::terrain_surface::SnowLine;
		const SnowLine north(0, 1.0, 47.0, 0.0);
		const SnowLine south(0, 1.0, -47.0, 0.0);
		const SnowLine tropics(0, 1.0, 3.0, 0.0);
		UASSERT(std::abs(north.shade(0.0, 1.0, 4.0) - 1.0) < 1e-9);
		UASSERT(std::abs(south.shade(0.0, 1.0, 4.0) + 1.0) < 1e-9);
		UASSERT(std::abs(north.shade(0.0, 1.0, 2.0) - 0.5) < 1e-9);
		UASSERT(std::abs(north.shade(0.0, 1.0, 8.0) - 1.0) < 1e-9);
		UASSERT(std::abs(north.shade(0.0, 1.0, 14.0) - 0.25) < 1e-9);
		UASSERT(tropics.shade(0.0, 1.0, 4.0) == 0.0);

		// A quarter turn points the pole direction along +X, perpendicular to this slope.
		const SnowLine rotated(0, 1.0, 47.0, 90.0);
		UASSERT(std::abs(rotated.shade(0.0, 1.0, 4.0)) < 1e-9);
		UASSERT(std::abs(rotated.shade(1.0, 0.0, 4.0) - 1.0) < 1e-9);
	}

	void testDisabledSnowLineDoesNotSnow()
	{
		using namespace arnis::terrain_surface;
		const SnowLine disabled(std::numeric_limits<int>::max(), 1.0, 47.0, 0.0);
		UASSERT(snow_cover(
				disabled.depth(0, 0, 5000), 0.0, [] { return 2.0; }, 1.0, 0, 0)
						.is_none());
	}

	void testNonFinitePeakHintDoesNotPoisonElevationGrid()
	{
		using namespace arnis::elevation;
		const arnis::geographic::LLBBox bbox(-1.0, -1.0, 1.0, 1.0);
		std::vector<std::vector<double>> grid(5, std::vector<double>(5, 100.0));
		const auto before = grid;
		raise_peaks(
				grid, bbox, {{0.0, 0.0, std::numeric_limits<double>::quiet_NaN()}}, 10.0);
		UASSERT(grid == before);
	}

	void testNonFinitePeakResolutionDoesNotPoisonElevationGrid()
	{
		using namespace arnis::elevation;
		const arnis::geographic::LLBBox bbox(-1.0, -1.0, 1.0, 1.0);
		for (const double resolution : {std::numeric_limits<double>::quiet_NaN(),
					 std::numeric_limits<double>::infinity()}) {
			std::vector<std::vector<double>> grid(5, std::vector<double>(5, 100.0));
			const auto before = grid;
			raise_peaks(grid, bbox, {{0.0, 0.0, 200.0}}, resolution);
			UASSERT(grid == before);
		}
	}

	void testFlatWorldDimensionsDoNotOverwriteElevationDimensions()
	{
		arnis::Ground ground;
		const std::vector<std::vector<float>> heights{{1.0f, 2.0f}, {3.0f, 4.0f}};
		ground.set_elevation_data(heights, 2, 2, 100, 80);
		ground.set_world_dims(64, 48);
		const auto elevation_dims = ground.world_dims();
		UASSERT(elevation_dims.first == 100 && elevation_dims.second == 80);

		auto flat = arnis::Ground::new_flat(0);
		flat.set_world_dims(64, 48);
		UASSERT(flat.body() == arnis::CelestialBody::Earth);
		UASSERT(flat.snow_threshold_y() == std::numeric_limits<int>::max());
		const auto flat_dims = flat.world_dims();
		UASSERT(flat_dims.first == 64 && flat_dims.second == 48);
	}

	void testTerrainTileSamplingIncludesSplineNeighbourTiles()
	{
		using arnis::elevation::providers::tile_math::bspline_weights;
		using arnis::elevation::providers::tile_math::sample_pixels;
		using arnis::elevation::providers::tile_math::tap_tile_range;
		constexpr unsigned zoom = 6;
		constexpr double tile_pixels = 512.0;
		const double tile_span = tile_pixels * (1u << zoom);
		const double seam = tile_pixels / tile_span;
		const auto [x0, x1] =
				tap_tile_range(zoom, seam + 1.0 / tile_span, seam + 2.0 / tile_span, 512);
		UASSERT(x0 == 0 && x1 == 1);
		const auto [edge0, edge1] = tap_tile_range(zoom, 0.0, 1.0 / tile_span, 512);
		UASSERT(edge0 == 0 && edge1 == 0);

		const auto ramp = [](int x, int y) { return 3.0 * x - 2.0 * y + 7.0; };
		for (const auto [x, y] :
				{std::pair{3.2, 4.7}, std::pair{100.01, 0.99}, std::pair{-5.5, 2.25}}) {
			UASSERT(std::abs(sample_pixels(x, y, true, ramp) -
							 sample_pixels(x, y, false, ramp)) < 1e-9);
		}
		const auto holey = [&](int x, int y) {
			return x < 0 ? std::numeric_limits<double>::quiet_NaN() : ramp(x, y);
		};
		UASSERT(std::isfinite(sample_pixels(0.75, 0.5, true, holey)));
		const auto wx = bspline_weights(0.25);
		const auto wy = bspline_weights(0.0);
		double expected = 0.0;
		for (std::size_t j = 0; j < wy.size(); ++j)
			for (std::size_t i = 1; i < wx.size(); ++i)
				expected += wx[i] * wy[j] *
							ramp(static_cast<int>(i) - 1, static_cast<int>(j) - 1);
		expected /= wx[1] + wx[2] + wx[3];
		UASSERT(std::abs(sample_pixels(0.75, 0.5, true, holey) - expected) < 1e-9);

		const auto bowl = [](int x, int y) {
			return x > 3 ? std::numeric_limits<double>::quiet_NaN()
						 : static_cast<double>(x * x + x * y + y * y);
		};
		for (const double py : {0.5, 1.3, 2.8}) {
			const double before = sample_pixels(2.5 - 1e-9, py, true, bowl);
			const double after = sample_pixels(2.5, py, true, bowl);
			UASSERT(std::abs(before - after) < 1e-6);
			UASSERT(std::abs(after - sample_pixels(2.5, py, false, bowl)) > 0.1);
		}
		UASSERT(std::isnan(sample_pixels(0.5, 0.5, true,
				[](int, int) { return std::numeric_limits<double>::quiet_NaN(); })));
	}
};

static TestFmArnisTerrainSurface g_test_fm_arnis_terrain_surface;
