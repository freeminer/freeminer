#include "test.h"
#include "mapgen/earth/arnis-cpp/src/terrain_surface.h"

#include <limits>

class TestFmArnisTerrainSurface : public TestBase
{
public:
	TestFmArnisTerrainSurface() { TestManager::registerTestModule(this); }
	const char *getName() override { return "TestFmArnisTerrainSurface"; }
	void runTests(IGameDef *) override
	{
		TEST(testSnowCoverMatchesRustThresholds);
		TEST(testSnowDepthFollowsUnroundedTerrain);
		TEST(testDisabledSnowLineDoesNotSnow);
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

	void testDisabledSnowLineDoesNotSnow()
	{
		using namespace arnis::terrain_surface;
		const SnowLine disabled(std::numeric_limits<int>::max(), 1.0, 47.0, 0.0);
		UASSERT(snow_cover(
				disabled.depth(0, 0, 5000), 0.0, [] { return 2.0; }, 1.0, 0, 0)
						.is_none());
	}
};

static TestFmArnisTerrainSurface g_test_fm_arnis_terrain_surface;
