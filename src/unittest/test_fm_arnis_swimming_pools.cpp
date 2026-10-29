#include "test.h"

#include "mapgen/earth/arnis_ground.h"
#include "mapgen/earth/arnis-cpp/src/element_processing/swimming_pools.h"
#include "mapgen/earth/arnis-cpp/src/floodfill_cache.h"
#include "mapgen/earth/arnis_world_editor.h"

#include <optional>
#include <string>
#include <utility>
#include <vector>

namespace
{
arnis::ProcessedWay rectangular_pool()
{
	arnis::ProcessedWay way{};
	way.id = 701;
	way.tags.emplace("leisure", "swimming_pool");
	const int coordinates[5][2] = {{10, 10}, {19, 10}, {19, 15}, {10, 15}, {10, 10}};
	for (const auto &coordinate : coordinates) {
		arnis::ProcessedNode node{};
		node.x = coordinate[0];
		node.z = coordinate[1];
		way.nodes.push_back(std::move(node));
	}
	return way;
}

arnis::ProcessedWay rectangular_ring(std::uint64_t id, int x0, int z0, int x1, int z1)
{
	arnis::ProcessedWay way{};
	way.id = id;
	const int coordinates[5][2] = {{x0, z0}, {x1, z0}, {x1, z1}, {x0, z1}, {x0, z0}};
	for (const auto &coordinate : coordinates) {
		arnis::ProcessedNode node{};
		node.id = id * 10 + way.nodes.size();
		node.x = coordinate[0];
		node.z = coordinate[1];
		way.nodes.push_back(std::move(node));
	}
	return way;
}

bool contains(arnis::WorldEditor &editor, int x, int y, int z, const arnis::Block &block)
{
	const auto actual = editor.get_block_absolute(x, y, z);
	return actual && actual->id() == block.id();
}
} // namespace

class TestFmArnisSwimmingPools : public TestBase
{
public:
	TestFmArnisSwimmingPools() { TestManager::registerTestModule(this); }
	const char *getName() override { return "TestFmArnisSwimmingPools"; }
	void runTests(IGameDef *) override
	{
		TEST(testPoolIsLinedAndRimmed);
		TEST(testPoolOnSlopeStaysLevelAndWallsItsLowEnd);
		TEST(testPoolDepthSaturatesBeforeIntegerConversion);
		TEST(testPoolKeepsDeckAndRoadsidePath);
		TEST(testIndoorPoolNeedsAFloor);
		TEST(testDivingPoolDoesNotGetLapLaneLines);
		TEST(testMultipolygonPoolLeavesItsIslandDry);
		TEST(testLapPoolLaneLinesMatchMappedDimensions);
		TEST(testLapPoolCrossbarsAtLargeScale);
		TEST(testPoolDepthUsesMetresAndWorldScale);
	}

	void testPoolIsLinedAndRimmed()
	{
		using namespace arnis;
		WorldEditor editor;
		const XZBBox bbox(0, 0, 63, 63);
		const auto way = rectangular_pool();
		const std::vector<ProcessedElement> elements{ProcessedElement(way)};
		const auto cache = FloodFillCache::precompute(elements, std::nullopt);
		const BuildingFootprintBitmap footprints(bbox);
		const RoadMaskBitmap roads(bbox);

		swimming_pools::generate(editor, way, Args{}, cache, footprints, roads);

		for (const auto &[x, z] :
				std::vector<std::pair<int, int>>{{10, 10}, {14, 12}, {19, 15}}) {
			UASSERT(contains(editor, x, 0, z, WATER));
			UASSERT(contains(editor, x, -1, z, WATER));
			UASSERT(contains(editor, x, -2, z, swimming_pools::LINING));
		}
		for (const auto &[x, z] :
				std::vector<std::pair<int, int>>{{9, 9}, {9, 12}, {20, 16}, {14, 16}}) {
			UASSERT(contains(editor, x, 0, z, swimming_pools::COPING));
			UASSERT(contains(editor, x, -1, z, swimming_pools::LINING));
		}
		UASSERT(!editor.block_exists_absolute(8, 0, 12));
		for (int x = 10; x <= 19; ++x)
			for (int z = 10; z <= 15; ++z)
				for (int y = -1; y <= 0; ++y) {
					if (!contains(editor, x, y, z, WATER))
						continue;
					UASSERT(editor.block_exists_absolute(x + 1, y, z));
					UASSERT(editor.block_exists_absolute(x - 1, y, z));
					UASSERT(editor.block_exists_absolute(x, y, z + 1));
					UASSERT(editor.block_exists_absolute(x, y, z - 1));
					UASSERT(editor.block_exists_absolute(x, y - 1, z));
				}
	}

	void testPoolOnSlopeStaysLevelAndWallsItsLowEnd()
	{
		using namespace arnis;
		Ground ground = Ground::new_flat(0);
		ground.elevation_grid.assign(64, std::vector<float>(64));
		for (auto &row : ground.elevation_grid)
			for (std::size_t x = 0; x < row.size(); ++x)
				row[x] = 4.0f + 0.25f * static_cast<float>(x);
		ground.elevation_world_width = 64;
		ground.elevation_world_height = 64;
		ground.elevation_enabled = true;

		WorldEditor editor;
		editor.ground = &ground;
		const XZBBox bbox(0, 0, 63, 63);
		auto way = rectangular_ring(704, 10, 10, 21, 15);
		way.tags.emplace("leisure", "swimming_pool");
		const std::vector<ProcessedElement> elements{ProcessedElement(way)};
		const auto cache = FloodFillCache::precompute(elements, std::nullopt);
		const BuildingFootprintBitmap footprints(bbox);
		const RoadMaskBitmap roads(bbox);
		const int low = editor.get_ground_level(10, 12);
		const int high = editor.get_ground_level(21, 12);
		UASSERT(high >= low + 2);

		swimming_pools::generate(editor, way, Args{}, cache, footprints, roads);

		for (int x = 10; x <= 21; ++x) {
			UASSERT(contains(editor, x, high, 12, WATER));
			UASSERT(!editor.block_exists_absolute(x, high + 1, 12));
		}
		const int rim_ground = editor.get_ground_level(9, 12);
		UASSERT(contains(editor, 9, high, 12, swimming_pools::COPING));
		for (int y = rim_ground + 1; y < high; ++y)
			UASSERT(contains(editor, 9, y, 12, swimming_pools::COPING));
		for (int y = low; y < high - 2; ++y)
			UASSERT(contains(editor, 10, y, 12, swimming_pools::LINING));
	}

	void testPoolKeepsDeckAndRoadsidePath()
	{
		using namespace arnis;
		WorldEditor editor;
		const XZBBox bbox(0, 0, 63, 63);
		const auto way = rectangular_pool();
		const std::vector<ProcessedElement> elements{ProcessedElement(way)};
		const auto cache = FloodFillCache::precompute(elements, std::nullopt);
		const BuildingFootprintBitmap footprints(bbox);
		RoadMaskBitmap roads(bbox);
		roads.set(9, 12);
		editor.set_block_absolute(OAK_PLANKS, 10, 1, 12);
		editor.set_block_absolute(GRASS, 11, 1, 13);
		editor.set_block_absolute(GRAVEL, 9, 0, 12);

		swimming_pools::generate(editor, way, Args{}, cache, footprints, roads);

		UASSERT(contains(editor, 10, 1, 12, OAK_PLANKS));
		UASSERT(contains(editor, 11, 1, 13, WATER));
		UASSERT(contains(editor, 9, 0, 12, GRAVEL));
		UASSERT(contains(editor, 9, -1, 12, swimming_pools::LINING));
		UASSERT(contains(editor, 9, 0, 13, swimming_pools::COPING));
	}

	void testIndoorPoolNeedsAFloor()
	{
		using namespace arnis;
		const XZBBox bbox(0, 0, 63, 63);
		const auto way = rectangular_pool();
		const std::vector<ProcessedElement> elements{ProcessedElement(way)};
		const auto cache = FloodFillCache::precompute(elements, std::nullopt);
		BuildingFootprintBitmap footprints(bbox);
		for (int x = 8; x <= 21; ++x)
			for (int z = 8; z <= 17; ++z)
				footprints.set(x, z);
		const RoadMaskBitmap roads(bbox);

		WorldEditor hall;
		for (int x = 8; x <= 21; ++x)
			for (int z = 8; z <= 17; ++z)
				hall.set_block_absolute(STONE_BRICKS, x, 0, z);
		swimming_pools::generate(hall, way, Args{}, cache, footprints, roads);
		for (int x = 8; x <= 21; ++x)
			for (int z = 8; z <= 17; ++z) {
				UASSERT(!contains(hall, x, 0, z, WATER));
				UASSERT(!contains(hall, x, 0, z, swimming_pools::COPING));
			}

		// The same footprint without a floor represents a canopy on posts, not a hall.
		WorldEditor canopy;
		swimming_pools::generate(canopy, way, Args{}, cache, footprints, roads);
		UASSERT(contains(canopy, 14, 0, 12, WATER));
		UASSERT(contains(canopy, 9, 0, 12, swimming_pools::COPING));
	}

	void testDivingPoolDoesNotGetLapLaneLines()
	{
		using namespace arnis;
		WorldEditor editor;
		const XZBBox bbox(0, 0, 63, 63);
		auto way = rectangular_ring(702, 5, 5, 29, 16);
		way.tags.emplace("leisure", "swimming_pool");
		way.tags.emplace("swimming_pool", "diving");
		const std::vector<ProcessedElement> elements{ProcessedElement(way)};
		const auto cache = FloodFillCache::precompute(elements, std::nullopt);
		const BuildingFootprintBitmap footprints(bbox);
		const RoadMaskBitmap roads(bbox);

		swimming_pools::generate(editor, way, Args{}, cache, footprints, roads);
		for (int x = 5; x <= 29; ++x)
			for (int z = 5; z <= 16; ++z)
				UASSERT(!contains(editor, x, -2, z, BLUE_CONCRETE));
	}

	void testPoolDepthSaturatesBeforeIntegerConversion()
	{
		using namespace arnis;
		WorldEditor editor;
		const XZBBox bbox(0, 0, 63, 63);
		auto way = rectangular_pool();
		// Finite, parseable depth above the range of lround's return type. Rust's
		// float-to-i32 cast saturates, then the pool's five-block cap is applied.
		way.tags.insert_or_assign("depth", "100000000000000000000");
		const std::vector<ProcessedElement> elements{ProcessedElement(way)};
		const auto cache = FloodFillCache::precompute(elements, std::nullopt);
		const BuildingFootprintBitmap footprints(bbox);
		const RoadMaskBitmap roads(bbox);

		swimming_pools::generate(editor, way, Args{}, cache, footprints, roads);

		UASSERT(contains(editor, 14, -4, 12, WATER));
		UASSERT(contains(editor, 14, -5, 12, swimming_pools::LINING));
	}

	void testMultipolygonPoolLeavesItsIslandDry()
	{
		using namespace arnis;
		WorldEditor editor;
		const XZBBox bbox(0, 0, 63, 63);
		ProcessedRelation relation{};
		relation.id = 901;
		relation.tags.emplace("leisure", "swimming_pool");
		relation.tags.emplace("type", "multipolygon");
		relation.members.emplace_back(
				rectangular_ring(902, 10, 10, 30, 30), ProcessedMemberRole::Outer);
		relation.members.emplace_back(
				rectangular_ring(903, 18, 18, 22, 22), ProcessedMemberRole::Inner);
		const std::vector<ProcessedElement> elements{ProcessedElement(relation)};
		const auto cache = FloodFillCache::precompute(elements, std::nullopt);
		const BuildingFootprintBitmap footprints(bbox);
		const RoadMaskBitmap roads(bbox);

		swimming_pools::generate_relation(
				editor, relation, Args{}, cache, footprints, roads);

		UASSERT(contains(editor, 12, 0, 12, WATER));
		UASSERT(!contains(editor, 20, 0, 20, WATER));
	}

	void testLapPoolLaneLinesMatchMappedDimensions()
	{
		using namespace arnis;
		WorldEditor editor;
		const XZBBox bbox(0, 0, 63, 63);
		auto way = rectangular_ring(702, 5, 5, 29, 16);
		way.tags.emplace("leisure", "swimming_pool");
		way.tags.emplace("sport", "swimming");
		const std::vector<ProcessedElement> elements{ProcessedElement(way)};
		const auto cache = FloodFillCache::precompute(elements, std::nullopt);
		const BuildingFootprintBitmap footprints(bbox);
		const RoadMaskBitmap roads(bbox);

		swimming_pools::generate(editor, way, Args{}, cache, footprints, roads);

		int lane_count = 0;
		for (int z = 5; z <= 16; ++z)
			lane_count += contains(editor, 17, -2, z, BLUE_CONCRETE);
		UASSERT(lane_count == 4);
		UASSERT(!contains(editor, 5, -2, 7, BLUE_CONCRETE));
	}

	void testPoolDepthUsesMetresAndWorldScale()
	{
		using namespace arnis;
		const XZBBox bbox(0, 0, 63, 63);
		const auto way = rectangular_pool();
		const std::vector<ProcessedElement> elements{ProcessedElement(way)};
		const auto cache = FloodFillCache::precompute(elements, std::nullopt);
		const BuildingFootprintBitmap footprints(bbox);
		const RoadMaskBitmap roads(bbox);

		WorldEditor tagged_editor;
		tagged_editor.set_projection_info("test", 1.0);
		auto tagged = way;
		tagged.tags.emplace("depth", "3.5 m");
		const std::vector<ProcessedElement> tagged_elements{ProcessedElement(tagged)};
		const auto tagged_cache =
				FloodFillCache::precompute(tagged_elements, std::nullopt);
		swimming_pools::generate(
				tagged_editor, tagged, Args{}, tagged_cache, footprints, roads);
		UASSERT(contains(tagged_editor, 14, -3, 12, WATER));
		UASSERT(contains(tagged_editor, 14, -4, 12, swimming_pools::LINING));

		WorldEditor scaled_editor;
		scaled_editor.set_projection_info("test", 2.0);
		swimming_pools::generate(scaled_editor, way, Args{}, cache, footprints, roads);
		// The default two-metre pool depth scales to four blocks in a 2x world.
		UASSERT(contains(scaled_editor, 14, -3, 12, WATER));
		UASSERT(contains(scaled_editor, 14, -4, 12, swimming_pools::LINING));
	}

	void testLapPoolCrossbarsAtLargeScale()
	{
		using namespace arnis;
		WorldEditor editor;
		editor.set_projection_info("test", 2.0);
		const XZBBox bbox(0, 0, 63, 63);
		auto way = rectangular_ring(703, 5, 5, 54, 28);
		way.tags.emplace("leisure", "swimming_pool");
		way.tags.emplace("sport", "swimming");
		const std::vector<ProcessedElement> elements{ProcessedElement(way)};
		const auto cache = FloodFillCache::precompute(elements, std::nullopt);
		const BuildingFootprintBitmap footprints(bbox);
		const RoadMaskBitmap roads(bbox);

		swimming_pools::generate(editor, way, Args{}, cache, footprints, roads);

		int lane_cells = 0;
		int crossbar_cells = 0;
		for (int z = 5; z <= 28; ++z) {
			lane_cells += contains(editor, 30, -4, z, BLUE_CONCRETE);
			crossbar_cells += contains(editor, 8, -4, z, BLUE_CONCRETE);
		}
		UASSERT(crossbar_cells > lane_cells);
	}
};

static TestFmArnisSwimmingPools g_test_fm_arnis_swimming_pools;
