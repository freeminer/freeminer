#include "test.h"

#include "mapgen/earth/arnis-cpp/src/element_processing/aerialways.h"

#include <vector>

class TestFmArnisAerialways : public TestBase
{
public:
	TestFmArnisAerialways() { TestManager::registerTestModule(this); }
	const char *getName() override { return "TestFmArnisAerialways"; }
	void runTests(IGameDef *) override
	{
		TEST(testLiftKindsMatchRust);
		TEST(testCorridorCoversLineAndTerminals);
		TEST(testNoLiftsReturnEmptyBitmap);
	}

	void testLiftKindsMatchRust()
	{
		for (const char *kind : {"cable_car", "gondola", "mixed_lift", "chair_lift",
					 "drag_lift", "t-bar", "j-bar", "platter", "rope_tow", "magic_carpet",
					 "zip_line", "goods"}) {
			arnis::tags_t tags;
			tags.emplace("aerialway", kind);
			UASSERT(arnis::aerialways::is_lift(tags));
		}
		UASSERT(!arnis::aerialways::is_lift({}));
		arnis::tags_t station_tags;
		station_tags.emplace("aerialway", "station");
		UASSERT(!arnis::aerialways::is_lift(station_tags));
	}

	void testCorridorCoversLineAndTerminals()
	{
		arnis::ProcessedWay way{};
		way.id = 1;
		way.tags.emplace("aerialway", "gondola");
		arnis::ProcessedNode start{};
		start.id = 2;
		start.x = 0;
		start.z = 0;
		arnis::ProcessedNode end{};
		end.id = 3;
		end.x = 250;
		end.z = 0;
		way.nodes = {start, end};
		const std::vector<arnis::ProcessedElement> elements{way};
		const XZBBox bounds(-32, -32, 280, 32);
		const auto corridors = arnis::aerialways::clear_corridors(elements, bounds);
		UASSERT(corridors.contains(125, 8));
		UASSERT(!corridors.contains(125, 20));
		UASSERT(corridors.contains(-12, 0));
	}

	void testNoLiftsReturnEmptyBitmap()
	{
		const XZBBox bounds(-32, -32, 280, 32);
		UASSERT(arnis::aerialways::clear_corridors({}, bounds).is_empty());
	}
};

static TestFmArnisAerialways g_test_fm_arnis_aerialways;
