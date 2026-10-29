#include "test.h"

#include "mapgen/earth/arnis-cpp/src/land_cover/naturalize.h"

#include <algorithm>
#include <string>
#include <utility>
#include <vector>

namespace
{
arnis::land_cover::LandCoverData make_land_cover(
		const std::vector<std::string> &rows, double cells_per_meter)
{
	using namespace arnis::land_cover;
	LandCoverData data;
	data.height = rows.size();
	data.width = rows.empty() ? 0 : rows.front().size();
	data.cells_per_meter = cells_per_meter;
	data.grid.reserve(data.height);
	for (const auto &row : rows) {
		std::vector<std::uint8_t> cells;
		cells.reserve(row.size());
		for (const char cell : row) {
			switch (cell) {
			case 'w':
				cells.push_back(LC_WATER);
				break;
			case 'b':
				cells.push_back(LC_BUILT_UP);
				break;
			case 'c':
				cells.push_back(LC_CROPLAND);
				break;
			case 't':
				cells.push_back(LC_TREE_COVER);
				break;
			case 'r':
				cells.push_back(LC_BARE);
				break;
			default:
				cells.push_back(LC_GRASSLAND);
				break;
			}
		}
		data.grid.push_back(std::move(cells));
	}
	data.water_distance = compute_water_distance(data.grid, data.width, data.height);
	return data;
}

std::size_t count_class(
		const arnis::land_cover::LandCoverData &data, std::uint8_t class_id)
{
	std::size_t count = 0;
	for (const auto &row : data.grid)
		for (const auto cell : row)
			count += cell == class_id;
	return count;
}
} // namespace

class TestFmArnisNaturalize : public TestBase
{
public:
	TestFmArnisNaturalize() { TestManager::registerTestModule(this); }
	const char *getName() override { return "TestFmArnisNaturalize"; }
	void runTests(IGameDef *) override
	{
		TEST(testFieldsBecomeGrassland);
		TEST(testTownTakesSurroundingForest);
		TEST(testBuiltUpFallbackUsesClimate);
		TEST(testBareGroundSpreadsOnlyInDryOrColdClimates);
		TEST(testBridgeReturnsToRiverAndLowersDeck);
		TEST(testRiverReadAsTownIsClosed);
		TEST(testHighDamStaysLand);
		TEST(testBlueprintCausewayIsNotReclaimed);
		TEST(testWideWaterfrontStaysLand);
		TEST(testOverlappingAreasShareNaturalizedBorders);
	}

	void testFieldsBecomeGrassland()
	{
		using namespace arnis::land_cover;
		auto data = make_land_cover({"ccgg", "cctt"}, 1.0);
		naturalize(data, nullptr, arnis::biome::Climate::Temperate);
		UASSERT(count_class(data, LC_CROPLAND) == 0);
		UASSERT(count_class(data, LC_GRASSLAND) == 6);
		UASSERT(count_class(data, LC_TREE_COVER) == 2);
	}

	void testTownTakesSurroundingForest()
	{
		using namespace arnis::land_cover;
		std::vector<std::string> rows(60, std::string(60, 't'));
		for (int z = 20; z < 40; ++z)
			for (int x = 20; x < 40; ++x)
				rows[z][x] = 'b';
		auto data = make_land_cover(rows, 1.0);
		naturalize(data, nullptr, arnis::biome::Climate::Temperate);
		UASSERT(count_class(data, LC_BUILT_UP) == 0);
		UASSERT(count_class(data, LC_TREE_COVER) == 60 * 60);
	}

	void testBuiltUpFallbackUsesClimate()
	{
		using namespace arnis::land_cover;
		auto temperate = make_land_cover({"bbbb", "bbbb"}, 1.0);
		naturalize(temperate, nullptr, arnis::biome::Climate::Temperate);
		UASSERT(count_class(temperate, LC_GRASSLAND) == 8);

		auto desert = make_land_cover({"bbbb", "bbbb"}, 1.0);
		naturalize(desert, nullptr, arnis::biome::Climate::HotDesert);
		UASSERT(count_class(desert, LC_BARE) == 8);
	}

	void testBareGroundSpreadsOnlyInDryOrColdClimates()
	{
		using namespace arnis::land_cover;
		std::vector<std::string> rows(50, std::string(400, 'b'));
		for (auto &row : rows) {
			std::fill(row.begin(), row.begin() + 100, 'r');
			std::fill(row.begin() + 300, row.end(), 't');
		}
		auto temperate = make_land_cover(rows, 1.0);
		naturalize(temperate, nullptr, arnis::biome::Climate::Temperate);
		UASSERT(count_class(temperate, LC_BARE) == 100 * 50);

		auto desert = make_land_cover(rows, 1.0);
		naturalize(desert, nullptr, arnis::biome::Climate::HotDesert);
		UASSERT(count_class(desert, LC_BARE) > 150 * 50);
		UASSERT(count_class(desert, LC_TREE_COVER) > 150 * 50);
		UASSERT(count_class(desert, LC_BUILT_UP) == 0);
	}

	void testBridgeReturnsToRiverAndLowersDeck()
	{
		using namespace arnis::land_cover;
		std::vector<std::string> rows(30, std::string(30, 'w'));
		for (int z = 0; z < 10; ++z)
			std::fill(rows[z].begin(), rows[z].end(), 'b');
		for (int z = 20; z < 30; ++z)
			std::fill(rows[z].begin(), rows[z].end(), 'b');
		for (int z = 10; z < 20; ++z)
			for (int x = 13; x < 17; ++x)
				rows[z][x] = 'b';
		auto data = make_land_cover(rows, 0.2);
		std::vector<std::vector<float>> heights(30, std::vector<float>(30, 10.0f));
		for (int z = 10; z < 20; ++z)
			for (int x = 13; x < 17; ++x)
				heights[z][x] = 12.0f;
		naturalize(data, &heights, arnis::biome::Climate::Temperate);
		for (int z = 10; z < 20; ++z)
			for (int x = 13; x < 17; ++x) {
				UASSERT(data.grid[z][x] == LC_WATER);
				UASSERT(heights[z][x] == 10.0f);
			}
		for (int z : {0, 9, 20, 29})
			UASSERT(data.grid[z][15] == LC_GRASSLAND);
		UASSERT(data.water_distance[15][15] > 0);
	}

	void testRiverReadAsTownIsClosed()
	{
		using namespace arnis::land_cover;
		std::vector<std::string> rows(30, std::string(40, 'w'));
		for (int z = 0; z < 30; ++z) {
			if (z < 10 || z >= 20)
				std::fill(rows[z].begin(), rows[z].end(), 'b');
			else
				std::fill(rows[z].begin() + 15, rows[z].begin() + 23, 'b');
		}
		auto data = make_land_cover(rows, 0.2);
		naturalize(data, nullptr, arnis::biome::Climate::Temperate);
		for (int z = 10; z < 20; ++z)
			for (int x = 15; x < 23; ++x)
				UASSERT(data.grid[z][x] == LC_WATER);
		UASSERT(data.grid[5][19] == LC_GRASSLAND);
		UASSERT(data.grid[25][19] == LC_GRASSLAND);
	}

	void testHighDamStaysLand()
	{
		using namespace arnis::land_cover;
		std::vector<std::string> rows(30, std::string(30, 'w'));
		for (int z = 0; z < 30; ++z)
			for (int x = 13; x < 17; ++x)
				rows[z][x] = 'b';
		auto data = make_land_cover(rows, 0.2);
		std::vector<std::vector<float>> heights(30, std::vector<float>(30, 10.0f));
		for (auto &row : heights)
			for (int x = 13; x < 17; ++x)
				row[x] = 40.0f;
		naturalize(data, &heights, arnis::biome::Climate::Temperate);
		UASSERT(data.grid[15][14] == LC_GRASSLAND);
		UASSERT(heights[15][14] == 40.0f);
	}

	void testBlueprintCausewayIsNotReclaimed()
	{
		using namespace arnis::land_cover;
		std::vector<std::string> rows(30, std::string(30, 'w'));
		for (auto &row : rows)
			std::fill(row.begin() + 13, row.begin() + 17, 'b');
		auto data = make_land_cover(rows, 0.2);
		std::vector<std::vector<float>> heights(30, std::vector<float>(30, 10.0f));
		naturalize(data, &heights, arnis::biome::Climate::Temperate, {0, 0}, false);
		UASSERT(data.grid[15][14] == LC_GRASSLAND);
		UASSERT(heights[15][14] == 10.0f);
	}

	void testWideWaterfrontStaysLand()
	{
		using namespace arnis::land_cover;
		std::vector<std::string> rows(30, std::string(30, 'b'));
		for (auto &row : rows) {
			std::fill(row.begin(), row.begin() + 5, 'w');
			std::fill(row.end() - 5, row.end(), 'w');
		}
		auto data = make_land_cover(rows, 0.2);
		naturalize(data, nullptr, arnis::biome::Climate::Temperate);
		UASSERT(count_class(data, LC_WATER) == 10 * 30);
		UASSERT(count_class(data, LC_GRASSLAND) == 20 * 30);
	}

	void testOverlappingAreasShareNaturalizedBorders()
	{
		using namespace arnis::land_cover;
		const auto source_class = [](int x) {
			return x < 100 ? 't' : x < 200 ? 'b' : 'g';
		};
		const auto make_area = [&](int x0, int z0, int width, int height) {
			std::vector<std::string> rows(height, std::string(width, 'g'));
			for (int z = 0; z < height; ++z)
				for (int x = 0; x < width; ++x)
					rows[z][x] = source_class(x0 + x);
			auto data = make_land_cover(rows, 1.0);
			naturalize(data, nullptr, arnis::biome::Climate::Temperate, {x0, z0});
			return data;
		};
		const auto whole = make_area(0, 0, 300, 260);
		const auto part = make_area(37, 23, 226, 200);
		for (int z = 0; z < 200; ++z)
			for (int x = 0; x < 226; ++x)
				UASSERT(part.grid[z][x] == whole.grid[z + 23][x + 37]);
		UASSERT(count_class(whole, LC_TREE_COVER) > 100 * 260);
		UASSERT(count_class(whole, LC_GRASSLAND) > 100 * 260);
	}
};

static TestFmArnisNaturalize g_test_fm_arnis_naturalize;
