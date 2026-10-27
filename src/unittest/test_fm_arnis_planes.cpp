#include "test.h"

#include "mapgen/earth/arnis-cpp/src/structures/plane.h"

namespace
{
arnis::ProcessedWay runway(const char *surface = nullptr)
{
	arnis::ProcessedWay result{};
	result.id = 1;
	result.tags.emplace("aeroway", "runway");
	if (surface)
		result.tags.emplace("surface", surface);
	arnis::ProcessedNode first{};
	first.id = 2;
	first.x = 0;
	first.z = 0;
	arnis::ProcessedNode last{};
	last.id = 3;
	last.x = 500;
	last.z = 0;
	result.nodes = {first, last};
	return result;
}
} // namespace

class TestFmArnisPlanes : public TestBase
{
public:
	TestFmArnisPlanes() { TestManager::registerTestModule(this); }
	const char *getName() override { return "TestFmArnisPlanes"; }
	void runTests(IGameDef *) override
	{
		TEST(testSoftRunwaysDoNotGetAirliners);
		TEST(testGravelRunwaysCanGetAirliners);
		TEST(testPavedRunwaysCanGetAirliners);
	}

	void testSoftRunwaysDoNotGetAirliners()
	{
		for (const char *surface : {"grass", "dirt", "ground", "sand", "mud", "clay"}) {
			arnis::structures::plane::Segment segment;
			UASSERT(!arnis::structures::plane::extract_aeroway_segment(
					runway(surface), segment));
		}
	}

	void testGravelRunwaysCanGetAirliners()
	{
		arnis::structures::plane::Segment segment;
		UASSERT(arnis::structures::plane::extract_aeroway_segment(
				runway("gravel"), segment));
	}

	void testPavedRunwaysCanGetAirliners()
	{
		arnis::structures::plane::Segment segment;
		UASSERT(arnis::structures::plane::extract_aeroway_segment(runway(), segment));
	}
};

static TestFmArnisPlanes g_test_fm_arnis_planes;
