#include "config.h"

#if BUILD_UNITTESTS
#include "test.h"

#include "mapgen/earth/arnis-cpp/src/element_processing/highways.h"
#include "mapgen/earth/arnis-cpp/src/element_processing/road_width.h"

#include <cmath>
#include <initializer_list>
#include <string>

namespace
{
class TestFmArnisRoadWidth : public TestBase
{
public:
	TestFmArnisRoadWidth() { TestManager::registerTestModule(this); }
	const char *getName() override { return "TestFmArnisRoadWidth"; }
	void runTests(IGameDef *) override
	{
		TEST(testClassAndLaneWidthsMatchRust);
		TEST(testParkingAndWidthTagsMatchRust);
		TEST(testCycleLanesAndSidewalksMatchRust);
		TEST(testParsingAndScaleMatchRust);
	}

	using Tags = std::initializer_list<std::pair<const char *, const char *>>;

	static arnis::tags_t make_tags(Tags pairs)
	{
		arnis::tags_t result;
		for (const auto &[key, value] : pairs)
			result.emplace(key, value);
		return result;
	}

	void testClassAndLaneWidthsMatchRust()
	{
		using arnis::road_width::road_halves;
		UASSERT(road_halves("secondary", {}, 1.0).kerb == 3);
		UASSERT(road_halves("primary", {}, 1.0).kerb == 4);
		UASSERT(road_halves("tertiary", {}, 1.0).kerb == 3);
		UASSERT(road_halves("residential", {}, 1.0).kerb == 2);
		UASSERT(road_halves("motorway", {}, 1.0).kerb == 5);
		UASSERT(road_halves("residential", {{"lanes", "4"}}, 1.0).kerb == 6);
		UASSERT(road_halves("motorway", {{"lanes", "6"}}, 1.0).kerb == 8);
		UASSERT(road_halves("footway", {{"lanes", "4"}}, 1.0).kerb == 1);
	}

	void testParkingAndWidthTagsMatchRust()
	{
		using arnis::road_width::kerb_sides;
		using arnis::road_width::parks_along;
		using arnis::road_width::road_halves;
		const auto both = make_tags({{"parking:both", "lane"}});
		const auto parked = road_halves("residential", both, 1.0);
		UASSERT(parked.lanes == 2 && parked.kerb == 4);
		UASSERT(parks_along(both));
		const auto one_side =
				make_tags({{"parking:both", "lane"}, {"parking:right", "no"}});
		UASSERT(kerb_sides("residential", one_side, 1.0) == std::pair(4, 2));
		const auto bridge = make_tags({{"bridge", "yes"}, {"oneway", "yes"},
				{"parking:left", "street_side"}, {"parking:left:orientation", "diagonal"},
				{"parking:right", "no"}});
		UASSERT(road_halves("residential", bridge, 1.0) ==
				road_halves("residential", {}, 1.0));
		UASSERT(!parks_along(bridge));
		UASSERT(road_halves("residential", {{"width", "7;8"}}, 1.0).kerb == 3);
		UASSERT(road_halves(
						"residential", {{"width", "5"}, {"parking:both", "lane"}}, 1.0)
						.kerb == 3);
	}

	void testCycleLanesAndSidewalksMatchRust()
	{
		using arnis::road_width::road_halves;
		const auto cycle = road_halves("residential",
				{{"lanes", "2"}, {"cycleway:both", "lane"},
						{"cycleway:both:lane", "exclusive"}},
				1.0);
		UASSERT(cycle.lanes == 3 && cycle.kerb == 4);
		UASSERT(road_halves("residential", {{"sidewalk:right", "separate"}}, 1.0).kerb ==
				3);
		UASSERT(road_halves("residential", {{"sidewalk", "both"}, {"oneway", "yes"}}, 1.0)
						.kerb == 2);
		UASSERT(road_halves("residential",
						{{"sidewalk", "both"}, {"parking:both", "lane"}}, 1.0)
						.kerb == 4);
		UASSERT(arnis::road_width::wall_half("residential", {}, 1.0) == 2);
		UASSERT(arnis::road_width::wall_half("residential",
						{{"width:carriageway", "9"}, {"parking:both", "lane"}},
						1.0) == 4);
	}

	void testParsingAndScaleMatchRust()
	{
		using arnis::road_width::count;
		using arnis::road_width::metres;
		using arnis::road_width::road_halves;
		UASSERT(metres("7;8") == 7.0);
		UASSERT(metres("3,5") == 3.5);
		UASSERT(std::abs(*metres("10'6\"") - 3.2004) < 1e-9);
		UASSERT(std::abs(*metres("12 ft") - 3.6576) < 1e-9);
		UASSERT(!metres("wide"));
		UASSERT(count("2;3") == 2);
		UASSERT(!count("x"));
		UASSERT(road_halves("residential", {}, .4).kerb == 1);
		UASSERT((road_halves("residential", {{"parking:both", "lane"}}, .5) ==
				 arnis::road_width::RoadHalves{1, 2}));
		UASSERT(arnis::highways::highway_block_range(
						"primary", {{"lanes", "1"}, {"oneway", "yes"}}, 1.0) == 1);
	}
};

static TestFmArnisRoadWidth g_test_fm_arnis_road_width;
} // namespace
#endif
