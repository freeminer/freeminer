#include "test.h"

#include "mapgen/earth/arnis-cpp/src/land_cover/osm_land_override.h"
#include "mapgen/earth/arnis-cpp/src/floodfill_cache.h"
#include "mapgen/earth/arnis-cpp/src/osm_parser.h"

#include <algorithm>
#include <cstdint>
#include <initializer_list>
#include <utility>
#include <vector>

namespace
{
using namespace arnis;
using namespace arnis::land_cover;

ProcessedNode node(std::uint64_t id, int x, int z)
{
	ProcessedNode result{};
	result.id = id;
	result.x = x;
	result.z = z;
	return result;
}

ProcessedWay way(std::uint64_t id, std::vector<ProcessedNode> nodes,
		std::initializer_list<std::pair<const char *, const char *>> tags)
{
	ProcessedWay result{};
	result.id = id;
	result.nodes = std::move(nodes);
	for (const auto &[key, value] : tags)
		result.tags.emplace(key, value);
	return result;
}

ProcessedWay rectangle(std::uint64_t id, int x0, int z0, int x1, int z1,
		std::initializer_list<std::pair<const char *, const char *>> tags)
{
	return way(id,
			{node(id * 10, x0, z0), node(id * 10 + 1, x1, z0), node(id * 10 + 2, x1, z1),
					node(id * 10 + 3, x0, z1), node(id * 10, x0, z0)},
			tags);
}

LandCoverData grid_from(bool coast)
{
	LandCoverData data;
	data.width = data.height = 60;
	data.cells_per_meter = 1.0;
	data.grid.assign(60, std::vector<std::uint8_t>(60, LC_GRASSLAND));
	if (coast)
		for (auto &row : data.grid)
			std::fill(row.begin() + 30, row.end(), LC_WATER);
	data.water_distance = compute_water_distance(data.grid, data.width, data.height);
	return data;
}

void run(LandCoverData &data, const std::vector<ProcessedElement> &elements)
{
	const XZBBox bbox(0, 0, 59, 59);
	apply_osm_land_override(data, 60, 60, elements, bbox, 1.0);
}
} // namespace

class TestFmArnisOsmLandOverride : public TestBase
{
public:
	TestFmArnisOsmLandOverride() { TestManager::registerTestModule(this); }
	const char *getName() override { return "TestFmArnisOsmLandOverride"; }
	void runTests(IGameDef *) override
	{
		TEST(testShoreRoadReclaimsOnlyItsWaterCells);
		TEST(testMappedWaterBoundaryTrimsTheMixedPixelRim);
		TEST(testWaterwayCentrelineDoesNotCloseTheRiver);
		TEST(testMappedPondDoesNotTrimWaterAcrossLand);
		TEST(testPoolReclaimsESAWaterAndLeavesMappedLake);
		TEST(testLargePoolReclaimsESAWaterFarFromShore);
		TEST(testPiersAndBridgesAreNotLandEvidence);
		TEST(testOpenWaterFarFromShoreIsNotTrimmed);
		TEST(testAbsurdWaterwayWidthIsClamped);
	}

	void testShoreRoadReclaimsOnlyItsWaterCells()
	{
		auto data = grid_from(true);
		const auto road =
				way(1, {node(1, 32, 0), node(2, 32, 59)}, {{"highway", "footway"}});
		run(data, {ProcessedElement(road)});
		UASSERT(data.grid[30][32] == LC_GRASSLAND);
		UASSERT(data.grid[30][50] == LC_WATER);
	}

	void testMappedWaterBoundaryTrimsTheMixedPixelRim()
	{
		auto data = grid_from(true);
		const auto lake = rectangle(2, 34, -5, 80, 65, {{"natural", "water"}});
		run(data, {ProcessedElement(lake)});
		for (int x = 30; x < 34; ++x)
			UASSERT(data.grid[30][x] != LC_WATER);
		for (int x = 34; x < 60; ++x)
			UASSERT(data.grid[30][x] == LC_WATER);
	}

	void testWaterwayCentrelineDoesNotCloseTheRiver()
	{
		auto data = grid_from(false);
		for (auto &row : data.grid)
			std::fill(row.begin() + 20, row.begin() + 40, LC_WATER);
		data.water_distance = compute_water_distance(data.grid, data.width, data.height);
		const auto river =
				way(3, {node(1, 30, 0), node(2, 30, 59)}, {{"waterway", "stream"}});
		run(data, {ProcessedElement(river)});
		for (int x = 20; x < 40; ++x)
			UASSERT(data.grid[30][x] == LC_WATER);
	}

	void testMappedPondDoesNotTrimWaterAcrossLand()
	{
		auto data = grid_from(false);
		for (auto &row : data.grid) {
			std::fill(row.begin() + 5, row.begin() + 15, LC_WATER);
			std::fill(row.begin() + 40, row.begin() + 55, LC_WATER);
		}
		data.water_distance = compute_water_distance(data.grid, data.width, data.height);
		const auto pond = rectangle(4, 7, 5, 13, 55, {{"natural", "water"}});
		run(data, {ProcessedElement(pond)});
		for (int x = 40; x < 55; ++x)
			UASSERT(data.grid[30][x] == LC_WATER);
	}

	void testPoolReclaimsESAWaterAndLeavesMappedLake()
	{
		auto data = grid_from(false);
		for (int z = 18; z < 28; ++z)
			std::fill(data.grid[z].begin() + 18, data.grid[z].begin() + 32, LC_WATER);
		data.water_distance = compute_water_distance(data.grid, data.width, data.height);
		const auto pool = rectangle(4, 20, 20, 29, 25, {{"leisure", "swimming_pool"}});
		run(data, {ProcessedElement(pool)});
		for (int x = 18; x < 32; ++x)
			UASSERT(data.grid[22][x] == LC_GRASSLAND);

		auto coast = grid_from(true);
		const auto lake = rectangle(5, 30, -5, 80, 65, {{"natural", "water"}});
		const auto pool_in_lake =
				rectangle(6, 40, 20, 50, 30, {{"leisure", "swimming_pool"}});
		run(coast, {ProcessedElement(lake), ProcessedElement(pool_in_lake)});
		UASSERT(coast.grid[25][45] == LC_WATER);
	}

	void testLargePoolReclaimsESAWaterFarFromShore()
	{
		auto data = grid_from(false);
		for (int z = 5; z < 55; ++z)
			std::fill(data.grid[z].begin() + 5, data.grid[z].begin() + 55, LC_WATER);
		data.water_distance = compute_water_distance(data.grid, data.width, data.height);

		// The pool lies well beyond the shoreline-repair band, but its basin and
		// coping still must be classified as land so water carving leaves it dry.
		const auto pool = rectangle(6, 8, 8, 51, 51, {{"leisure", "swimming_pool"}});
		run(data, {ProcessedElement(pool)});
		UASSERT(data.grid[30][30] != LC_WATER);
		UASSERT(data.grid[30][7] != LC_WATER);
	}

	void testPiersAndBridgesAreNotLandEvidence()
	{
		auto data = grid_from(true);
		const auto bridge = way(7, {node(1, 32, 10), node(2, 32, 20)},
				{{"highway", "footway"}, {"bridge", "yes"}});
		const auto pier =
				way(8, {node(3, 34, 30), node(4, 34, 40)}, {{"man_made", "pier"}});
		const auto path_on_pier =
				way(9, {node(5, 34, 30), node(6, 34, 40)}, {{"highway", "footway"}});
		run(data, {ProcessedElement(bridge), ProcessedElement(pier),
						  ProcessedElement(path_on_pier)});
		UASSERT(data.grid[15][32] == LC_WATER);
		UASSERT(data.grid[35][34] == LC_WATER);
	}

	void testOpenWaterFarFromShoreIsNotTrimmed()
	{
		auto data = grid_from(true);
		const auto building = rectangle(10, 0, 0, 59, 59, {{"building", "yes"}});
		run(data, {ProcessedElement(building)});
		UASSERT(data.grid[30][31] != LC_WATER);
		UASSERT(data.grid[30][50] == LC_WATER);
	}

	void testAbsurdWaterwayWidthIsClamped()
	{
		auto data = grid_from(true);
		const auto river = way(11, {node(1, 32, 0), node(2, 32, 59)},
				{{"waterway", "river"}, {"width", "999999999"}});
		run(data, {ProcessedElement(river)});
		UASSERT(data.grid[30][50] == LC_WATER);
	}
};

static TestFmArnisOsmLandOverride g_test_fm_arnis_osm_land_override;
