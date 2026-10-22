#include "test.h"

#include "mapgen/earth/arnis-cpp/src/deferred_placement.h"

#include <utility>
#include <vector>

class TestFmArnisDeferredPlacement : public TestBase
{
public:
	TestFmArnisDeferredPlacement() { TestManager::registerTestModule(this); }
	const char *getName() override { return "TestFmArnisDeferredPlacement"; }
	void runTests(IGameDef *) override
	{
		TEST(testPlacementWaitsForAllRegions);
		TEST(testOverlappingPlacementsKeepOrder);
		TEST(testDisjointPlacementsReleaseInWaves);
		TEST(testOutOfWorldRegionsAreIgnored);
		TEST(testWorldPredicateFiltersRegions);
		TEST(testDrainReturnsUnfinishedPlacementsOnce);
	}

	void testPlacementWaitsForAllRegions()
	{
		arnis::DeferredPlacements scheduled({{{0, 0}, {1, 0}}}, {{0, 0}, {1, 0}});
		UASSERT(scheduled.complete({0, 0}).empty());
		UASSERT(scheduled.holds({0, 0}));
		UASSERT((scheduled.complete({1, 0}) ==
				 std::vector<std::vector<std::size_t>>{{0}}));
		UASSERT(!scheduled.holds({0, 0}) && !scheduled.holds({1, 0}));
	}

	void testOverlappingPlacementsKeepOrder()
	{
		arnis::DeferredPlacements scheduled(
				{{{0, 0}, {1, 0}}, {{1, 0}, {2, 0}}}, {{0, 0}, {1, 0}, {2, 0}});
		UASSERT(scheduled.complete({1, 0}).empty());
		UASSERT(scheduled.complete({2, 0}).empty());
		UASSERT(scheduled.complete({0, 0}) ==
				(std::vector<std::vector<std::size_t>>{{0}, {1}}));
	}

	void testDisjointPlacementsReleaseInWaves()
	{
		arnis::DeferredPlacements scheduled(
				{{{0, 0}}, {{1, 0}}, {{0, 0}, {1, 0}}}, {{0, 0}, {1, 0}});
		UASSERT((scheduled.complete({0, 0}) ==
				 std::vector<std::vector<std::size_t>>{{0}}));
		UASSERT(scheduled.complete({1, 0}) ==
				(std::vector<std::vector<std::size_t>>{{1}, {2}}));
	}

	void testOutOfWorldRegionsAreIgnored()
	{
		arnis::DeferredPlacements scheduled({{{-1, 0}, {0, 0}}, {{-1, 0}}}, {{0, 0}});
		UASSERT(!scheduled.holds({-1, 0}));
		UASSERT((scheduled.complete({0, 0}) ==
				 std::vector<std::vector<std::size_t>>{{0}}));
		UASSERT(scheduled.drain() == std::vector<std::size_t>{1});
	}

	void testWorldPredicateFiltersRegions()
	{
		arnis::DeferredPlacements scheduled(
				{{{0, 0}, {1, 0}, {2, 0}}}, [](arnis::DeferredPlacements::Region region) {
					return region.first < 2;
				});
		UASSERT(scheduled.holds({0, 0}) && scheduled.holds({1, 0}));
		UASSERT(!scheduled.holds({2, 0}));
		UASSERT(scheduled.complete({0, 0}).empty());
		UASSERT((scheduled.complete({1, 0}) ==
				 std::vector<std::vector<std::size_t>>{{0}}));
		UASSERT(scheduled.regions_of(0) ==
				(std::vector<arnis::DeferredPlacements::Region>{{0, 0}, {1, 0}}));
	}

	void testDrainReturnsUnfinishedPlacementsOnce()
	{
		arnis::DeferredPlacements scheduled(
				{{{0, 0}}, {{0, 0}, {5, 5}}, {{6, 6}}}, {{0, 0}, {5, 5}});
		UASSERT(scheduled.complete({0, 0}) == std::vector<std::vector<std::size_t>>{{0}});
		UASSERT(scheduled.drain() == (std::vector<std::size_t>{1, 2}));
		UASSERT(!scheduled.holds({0, 0}) && !scheduled.holds({5, 5}));
		UASSERT(scheduled.drain().empty());
	}
};

static TestFmArnisDeferredPlacement g_test_fm_arnis_deferred_placement;
