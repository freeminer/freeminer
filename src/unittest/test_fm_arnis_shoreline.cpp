#include "test.h"

#include "mapgen/earth/arnis-cpp/src/land_cover/land_cover.h"
#include "mapgen/earth/arnis-cpp/src/land_cover/shoreline.h"

#include <cstddef>
#include <cstdint>
#include <vector>

namespace
{
arnis::land_cover::EsaRasterTile make_tile()
{
	using namespace arnis::land_cover;
	EsaRasterTile tile;
	tile.min_lat = -1.0;
	tile.max_lat = 2.0;
	tile.min_lng = -1.0;
	tile.max_lng = 2.0;
	tile.width = 120;
	tile.height = 120;
	tile.pixels_per_degree = 40.0;
	tile.pixels.assign(tile.width * tile.height, LC_GRASSLAND);
	return tile;
}

std::size_t count_water(const arnis::land_cover::LandCoverData &data)
{
	std::size_t count = 0;
	for (const auto &row : data.grid)
		for (const auto cell : row)
			count += cell == arnis::land_cover::LC_WATER;
	return count;
}
} // namespace

class TestFmArnisShoreline : public TestBase
{
public:
	TestFmArnisShoreline() { TestManager::registerTestModule(this); }
	const char *getName() override { return "TestFmArnisShoreline"; }
	void runTests(IGameDef *) override { TEST(testSinglePixelPondAndIslandSurvive); }

	void testSinglePixelPondAndIslandSurvive()
	{
		using namespace arnis::land_cover;
		auto tile = make_tile();
		const auto set_pixel = [&](std::size_t x, std::size_t z, std::uint8_t value) {
			tile.pixels[z * tile.width + x] = value;
		};

		// A one-pixel pond beside a larger lake with a one-pixel land island.
		set_pixel(60, 60, LC_WATER);
		for (std::size_t z = 62; z <= 68; ++z)
			for (std::size_t x = 62; x <= 68; ++x)
				set_pixel(x, z, LC_WATER);
		set_pixel(65, 65, LC_TREE_COVER);

		LandCoverData data;
		data.source_bounds = {0.0, 0.0, 1.0, 1.0};
		data.width = 400;
		data.height = 400;
		data.grid.assign(data.height, std::vector<std::uint8_t>(data.width));
		UASSERT(reconstruct_water_shoreline(data, {tile}));

		// The source pixels each cover about ten output cells at this resolution.
		UASSERT(data.grid[205][205] == LC_WATER);
		UASSERT(data.grid[235][235] == LC_WATER);
		UASSERT(data.grid[255][255] == LC_TREE_COVER);
		UASSERT(count_water(data) > 400);
	}
};

static TestFmArnisShoreline g_test_fm_arnis_shoreline;
