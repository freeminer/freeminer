#include "test.h"

#include "mapgen/earth/arnis_ground.h"
#include "mapgen/earth/arnis-cpp/src/block_definitions.h"
#include "mapgen/earth/arnis-cpp/src/land_cover/land_cover.h"
#include "mapgen/earth/arnis-cpp/src/land_cover/shoreline.h"
#include "mapgen/earth/arnis-cpp/src/water_depth.h"
#include "mapgen/earth/arnis_world_editor.h"

#include <cstddef>
#include <cstdint>
#include <utility>
#include <unordered_set>
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
	void runTests(IGameDef *) override
	{
		TEST(testSinglePixelPondAndIslandSurvive);
		TEST(testCoarseGridKeepsThePixelSample);
		TEST(testBeachProximityIndexAndInvalidation);
		TEST(testBeachProximityAcrossTileBoundary);
		TEST(testWorldEditorBeachProximityUsesGroundCoordinates);
		TEST(testShallowBedsUseBeachSandAndInlandMaterials);
	}

	void testShallowBedsUseBeachSandAndInlandMaterials()
	{
		using namespace arnis;
		land_cover::LandCoverData data;
		data.width = 64;
		data.height = 64;
		data.grid.assign(data.height,
				std::vector<std::uint8_t>(data.width, land_cover::LC_GRASSLAND));
		for (std::size_t z = 0; z < data.height; ++z)
			for (std::size_t x = 0; x < 4; ++x)
				data.grid[z][x] = land_cover::LC_BEACH;
		auto ground = Ground::new_flat_with_land_cover(std::move(data), 64, 64, 0);
		WorldEditor editor;
		editor.ground = &ground;

		const auto [beach_bed, beach_under] = water_depth::shallow_bed(editor, 5, 30);
		UASSERT(beach_bed == SAND);
		UASSERT(beach_under == SANDSTONE);

		std::unordered_set<content_t> inland_beds;
		std::size_t inland_sand = 0;
		for (int x = 16; x < 64; ++x) {
			for (int z = 0; z < 64; ++z) {
				const auto [bed, under] = water_depth::shallow_bed(editor, x, z);
				UASSERT(under == DIRT);
				inland_beds.insert(bed.id());
				inland_sand += bed == SAND;
			}
		}
		UASSERT(inland_beds.size() >= 3);
		UASSERT(inland_sand * 4 < 48U * 64U);
	}

	void testBeachProximityIndexAndInvalidation()
	{
		using namespace arnis::land_cover;
		LandCoverData data;
		data.width = 64;
		data.height = 64;
		data.grid.assign(
				data.height, std::vector<std::uint8_t>(data.width, LC_GRASSLAND));
		data.grid[30][3] = LC_BEACH;
		UASSERT(data.beach_within(7, 30, 4));
		UASSERT(!data.beach_within(8, 30, 4));
		data.grid[30][12] = LC_BEACH;
		data.invalidate_water_blend_grid();
		UASSERT(data.beach_within(8, 30, 4));
	}

	void testBeachProximityAcrossTileBoundary()
	{
		using namespace arnis::land_cover;
		LandCoverData data;
		data.width = 64;
		data.height = 64;
		data.grid.assign(
				data.height, std::vector<std::uint8_t>(data.width, LC_GRASSLAND));
		// The query spans several 16x16 cache tiles; only the far tile has beach.
		data.grid[30][33] = LC_BEACH;
		UASSERT(data.beach_within(15, 30, 20));
		UASSERT(!data.beach_within(15, 30, 17));
	}

	void testWorldEditorBeachProximityUsesGroundCoordinates()
	{
		using namespace arnis;
		land_cover::LandCoverData data;
		data.width = 64;
		data.height = 64;
		data.grid.assign(data.height,
				std::vector<std::uint8_t>(data.width, land_cover::LC_GRASSLAND));
		data.grid[30][33] = land_cover::LC_BEACH;
		auto ground = Ground::new_flat_with_land_cover(std::move(data), 64, 64, 0);
		WorldEditor editor;
		editor.ground = &ground;

		UASSERT(editor.near_beach(15, 30, 20.0));
		UASSERT(!editor.near_beach(15, 30, 17.0));
	}

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

	void testCoarseGridKeepsThePixelSample()
	{
		using namespace arnis::land_cover;
		auto tile = make_tile();
		// At 40 pixels per degree, a 60-cell grid over one degree has fewer
		// than two grid cells per source pixel. Rust skips polygon smoothing at
		// that resolution but must retain its already sampled water pixels.
		for (std::size_t z = 0; z < tile.height; ++z)
			for (std::size_t x = 60; x <= 64; ++x)
				tile.pixels[z * tile.width + x] = LC_WATER;

		LandCoverData data;
		data.source_bounds = {0.0, 0.0, 1.0, 1.0};
		data.width = 60;
		data.height = 60;
		data.grid.assign(data.height, std::vector<std::uint8_t>(data.width));
		UASSERT(!reconstruct_water_shoreline(data, {tile}));
		UASSERT(data.grid[30][32] == LC_WATER);
		UASSERT(data.grid[30][10] == LC_GRASSLAND);
	}
};

static TestFmArnisShoreline g_test_fm_arnis_shoreline;
