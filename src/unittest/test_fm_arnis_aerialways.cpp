#include "test.h"

#include "mapgen/earth/arnis-cpp/src/element_processing/aerialways.h"
#include "mapgen/earth/arnis-cpp/src/clipping.h"

#include <cstdint>
#include <utility>
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
		TEST(testCorridorsStopAtWorldEdge);
		TEST(testClippedLiftEndIsNotAnOsmTerminal);
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

	void testCorridorsStopAtWorldEdge()
	{
		const XZBBox bounds(0, 0, 400, 400);
		auto lift = [](std::uint64_t id, int ax, int az, int bx, int bz) {
			arnis::ProcessedWay way{};
			way.id = id;
			way.tags.emplace("aerialway", "chair_lift");
			arnis::ProcessedNode start{};
			start.id = id;
			start.x = ax;
			start.z = az;
			arnis::ProcessedNode end{};
			end.id = id + 1;
			end.x = bx;
			end.z = bz;
			way.nodes = {start, end};
			return arnis::ProcessedElement{std::move(way)};
		};
		const auto inside = lift(1, -100, 200, 100, 200);
		const auto outside = lift(3, -500, -500, -450, -500);
		const auto corridors =
				arnis::aerialways::clear_corridors({inside, outside}, bounds);
		UASSERT(corridors.contains(0, 200));
		UASSERT(corridors.contains(100, 208));
		UASSERT(corridors.contains(112, 200));
		UASSERT(!corridors.contains(300, 200));
		UASSERT(!corridors.contains(0, 0));
		UASSERT(arnis::aerialways::clear_corridors({outside}, bounds).is_empty());
	}

	void testNoLiftsReturnEmptyBitmap()
	{
		const XZBBox bounds(-32, -32, 280, 32);
		UASSERT(arnis::aerialways::clear_corridors({}, bounds).is_empty());
	}

	void testClippedLiftEndIsNotAnOsmTerminal()
	{
		arnis::ProcessedNode start{};
		start.id = 2;
		start.x = 0;
		start.z = 0;
		arnis::ProcessedNode end{};
		end.id = 3;
		end.x = 100;
		end.z = 0;
		const XZBBox bounds(0, -5, 90, 5);
		auto clipped = arnis::clipping::clip_way_to_bbox({start, end}, bounds);
		UASSERT(clipped.size() == 2);
		UASSERT(clipped.front().id == start.id);
		UASSERT(clipped.back().id == end.id);
		arnis::clipping::mark_cut_ends(
				clipped, {{{start.x, start.z}, {end.x, end.z}}}, 7);
		UASSERT(clipped.front().id == start.id);
		UASSERT(arnis::clipping::is_invented_node_id(clipped.back().id));
	}
};

static TestFmArnisAerialways g_test_fm_arnis_aerialways;
