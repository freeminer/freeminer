#include "test.h"

#include "mapgen/earth/arnis-cpp/src/water_probe.h"

#include <vector>

class TestFmArnisWaterProbe : public TestBase
{
public:
	TestFmArnisWaterProbe() { TestManager::registerTestModule(this); }
	const char *getName() override { return "TestFmArnisWaterProbe"; }
	void runTests(IGameDef *) override
	{
		TEST(testWaterColumnFindsWaterAndOverhead);
		TEST(testFootingFindsGroundAndHalfSlabs);
		TEST(testProbeReadsRegionOnceAndRoundsPosts);
		TEST(testWalkSpotClassifiesBlocks);
	}

	void testWaterColumnFindsWaterAndOverhead()
	{
		using namespace arnis::water_probe;
		const auto sample = water_column(
				[](int y) {
					if (y == 70)
						return Spot::Solid;
					if (y <= 60)
						return Spot::Water;
					return Spot::Open;
				},
				60, 80);
		UASSERT(sample.water() == 60);
		UASSERT(sample.clearance() == 9);
		UASSERT(!water_column([](int) { return Spot::Solid; }, 60, 80).water());
	}

	void testFootingFindsGroundAndHalfSlabs()
	{
		using namespace arnis::water_probe;
		const auto f = footing(
				[](int y) {
					if (y == 63)
						return Spot::Solid;
					if (y == 66)
						return Spot::Leaves;
					return Spot::Open;
				},
				64);
		UASSERT(f.nearest(64.0) == 64.0);
		const auto slab = footing(
				[](int y) {
					if (y == 63)
						return Spot::Half;
					return Spot::Open;
				},
				64);
		UASSERT(slab.nearest(63.5) == 63.5);
	}

	void testProbeReadsRegionOnceAndRoundsPosts()
	{
		using namespace arnis::water_probe;
		const std::vector<Column> cols{{5, 5, 60}, {600, 5, 60}};
		WalkProbe probe(cols);
		int reads = 0;
		probe.read_region_round(
				0, 0,
				[&](int, int, int) {
					++reads;
					return Footing{60, 0, 0, true};
				},
				[](const Footing &f) { return f.post(); });
		const Footing post{60, 0, 0, true};
		UASSERT(probe.get(5, 5, 60) == post);
		UASSERT(probe.get(7, 7, 60) == post);
		UASSERT(probe.get(6, 6, 60) == post);
		UASSERT(!probe.get(8, 8, 60));
		const int reads_after_first = reads;
		UASSERT(reads_after_first == 25);
		probe.read_region(0, 0, [&](int, int, int) {
			++reads;
			return Footing{};
		});
		UASSERT(reads == reads_after_first);
	}

	void testWalkSpotClassifiesBlocks()
	{
		using namespace arnis::water_probe;
		UASSERT(walk_spot("minecraft:short_grass", false) == Spot::Open);
		UASSERT(walk_spot("minecraft:stone_slab", true) == Spot::Half);
		UASSERT(walk_spot("minecraft:stone_slab", false) == Spot::Solid);
		UASSERT(walk_spot("minecraft:oak_leaves", false) == Spot::Leaves);
		UASSERT(walk_spot("minecraft:oak_fence", false) == Spot::Tall);
		UASSERT(walk_spot("minecraft:lily_pad", false) == Spot::Open);
	}
};

static TestFmArnisWaterProbe g_test_fm_arnis_water_probe;
