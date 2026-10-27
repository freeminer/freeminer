#include "test.h"

#include "mapgen/earth/arnis-cpp/src/element_processing/advtrains.h"

#include <algorithm>
#include <initializer_list>
#include <set>

namespace arnis::railways
{
void generate_railways(WorldEditor &editor, const ProcessedWay &element);
std::vector<std::pair<int, int>> build_smoothed_centerline(const ProcessedWay &way);
} // namespace arnis::railways

namespace
{
struct NativeTracks
{
	bool available = arnis::block_definitions::ADVTRAINS_AVAILABLE;
	NativeTracks() { arnis::block_definitions::ADVTRAINS_AVAILABLE = true; }
	~NativeTracks()
	{
		arnis::WorldEditor editor;
		arnis::railways::advtrains::prepare_network({}, editor);
		arnis::block_definitions::ADVTRAINS_AVAILABLE = available;
	}
};

arnis::ProcessedWay way(std::initializer_list<std::pair<pos_t, pos_t>> coordinates)
{
	arnis::ProcessedWay result{};
	result.id = 71;
	result.tags.emplace("railway", "rail");
	for (const auto &[x, z] : coordinates) {
		arnis::ProcessedNode node{};
		node.id = 100 + result.nodes.size();
		node.x = x;
		node.z = z;
		result.nodes.push_back(node);
	}
	return result;
}
} // namespace

class TestFmArnisRailways : public TestBase
{
public:
	TestFmArnisRailways() { TestManager::registerTestModule(this); }
	const char *getName() override { return "TestFmArnisRailways"; }
	void runTests(IGameDef *) override
	{
		TEST(testShallowDiagonalUsesOnlyNativeCells);
		TEST(testReversedWayUsesOnlyNativeCells);
		TEST(testCurveUsesOnlyNativeCells);
		TEST(testEmptyNativePlanDoesNotFallBackToCartRail);
		TEST(testCartBackendStillRegistersItsRaster);
	}

	void check_native(const arnis::ProcessedWay &railway)
	{
		NativeTracks tracks;
		arnis::WorldEditor editor;
		editor.enable_rail_height_collection();
		// Exercise the real railway dispatcher, initial pass, and finalization.
		// The height registry records every rail placement even without a VM.
		arnis::railways::generate_railways(editor, railway);
		const auto *plan = arnis::railways::advtrains::get_plan(railway);
		UASSERT(plan && !plan->line.empty());
		UASSERT(plan->line.size() == plan->rails.size());
		arnis::world_editor::RailHeightMap expected;
		for (std::size_t i = 0; i < plan->line.size(); ++i) {
			const auto &[x, z] = plan->line[i];
			expected.emplace(arnis::world_editor::RailHeightKey{railway.id, x, z},
					plan->heights[i] + 1);
		}
		const auto raster = arnis::railways::build_smoothed_centerline(railway);
		UASSERT(std::any_of(raster.begin(), raster.end(), [&](const auto &p) {
			return !expected.contains({railway.id, p.first, p.second});
		}));
		// Regression: the previous initial pass registered the cart raster and
		// placed fallback rails between and beside the final native track.
		UASSERT(editor.rail_height_snapshot() == expected);
	}

	void testShallowDiagonalUsesOnlyNativeCells()
	{
		check_native(way({{0, 0}, {24, 64}}));
	}

	void testReversedWayUsesOnlyNativeCells() { check_native(way({{24, 64}, {0, 0}})); }

	void testCurveUsesOnlyNativeCells()
	{
		check_native(way({{0, 0}, {0, 40}, {24, 88}}));
	}

	void testEmptyNativePlanDoesNotFallBackToCartRail()
	{
		NativeTracks tracks;
		arnis::WorldEditor editor;
		editor.enable_rail_height_collection();
		arnis::railways::generate_railways(editor, way({{4, 4}, {4, 4}}));
		UASSERT(editor.rail_height_snapshot().empty());
	}

	void testCartBackendStillRegistersItsRaster()
	{
		NativeTracks tracks;
		arnis::block_definitions::ADVTRAINS_AVAILABLE = false;
		arnis::WorldEditor editor;
		editor.enable_rail_height_collection();
		const auto railway = way({{0, 0}, {24, 64}});
		arnis::railways::generate_railways(editor, railway);
		std::set<std::pair<int, int>> recorded;
		for (const auto &[key, height] : editor.rail_height_snapshot()) {
			const auto &[id, x, z] = key;
			UASSERT(id == railway.id);
			recorded.emplace(x, z);
		}
		const auto raster = arnis::railways::build_smoothed_centerline(railway);
		UASSERT((
				recorded == std::set<std::pair<int, int>>(raster.begin(), raster.end())));
	}
};

static TestFmArnisRailways g_test_fm_arnis_railways;
