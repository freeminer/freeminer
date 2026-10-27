#include "test.h"

#include "mapgen/earth/arnis-cpp/src/element_processing/road_markings.h"
#include "mapgen/earth/arnis-cpp/src/element_processing/signage.h"

#include <cstdint>
#include <initializer_list>
#include <utility>
#include <vector>

namespace
{
using namespace arnis;
using namespace arnis::road_markings;

ProcessedNode node(std::uint64_t id, int x, int z,
		std::initializer_list<std::pair<const char *, const char *>> tags = {})
{
	ProcessedNode result{};
	result.id = id;
	result.x = x;
	result.z = z;
	for (const auto &[key, value] : tags)
		result.tags.emplace(key, value);
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

std::vector<ProcessedElement> crossing_network(
		std::initializer_list<std::pair<const char *, const char *>> crossing_way_tags,
		std::initializer_list<std::pair<const char *, const char *>> first_road_node_tags,
		std::initializer_list<std::pair<const char *, const char *>>
				second_road_node_tags)
{
	const auto first = node(2, 40, 50, first_road_node_tags);
	const auto second = node(5, 40, 40, second_road_node_tags);
	return {ProcessedElement(way(1, {node(10, 0, 50), first, node(11, 100, 50)},
					{{"highway", "secondary"}})),
			ProcessedElement(way(4, {node(14, 0, 40), second, node(15, 100, 40)},
					{{"highway", "secondary_link"}})),
			ProcessedElement(way(3, {node(12, 40, 30), second, first, node(13, 40, 70)},
					crossing_way_tags))};
}

} // namespace

class TestFmArnisRoadMarkings : public TestBase
{
public:
	TestFmArnisRoadMarkings() { TestManager::registerTestModule(this); }
	const char *getName() override { return "TestFmArnisRoadMarkings"; }
	void runTests(IGameDef *) override
	{
		TEST(testCrossingWayUsesTagsFromRoadNodes);
		TEST(testOnePaintedRoadNodeWins);
		TEST(testUnmarkedRoadNodesSuppressDefaultZebra);
		TEST(testUnrelatedCrossingNodeDoesNotControlWay);
		TEST(testUntaggedCrossingNodeAloneStaysBare);
		TEST(testStopSignHeadingFollowsNearestJunction);
	}

	void testCrossingWayUsesTagsFromRoadNodes()
	{
		auto elements =
				crossing_network({{"highway", "footway"}, {"footway", "crossing"}},
						{{"highway", "crossing"}, {"crossing:markings", "lines"}},
						{{"highway", "crossing"}, {"crossing", "unmarked"}});
		const auto &crossing = elements[2].as_way();
		const auto index =
				RoadMarkingIndex::build(elements, 1.0, decals::SignRegion::Europe);
		const auto paint = index.crossing_way_paint(crossing);
		UASSERT(paint.has_value());
		UASSERT(paint->kind == CrossingPaint::Kind::Lines);
		UASSERT(!paint->broken);
	}

	void testOnePaintedRoadNodeWins()
	{
		auto elements =
				crossing_network({{"highway", "footway"}, {"footway", "crossing"}},
						{{"highway", "crossing"}, {"crossing", "unmarked"}},
						{{"highway", "crossing"}, {"crossing", "zebra"}});
		const auto paint =
				RoadMarkingIndex::build(elements, 1.0, decals::SignRegion::Europe)
						.crossing_way_paint(elements[2].as_way());
		UASSERT(paint.has_value());
		UASSERT(paint->kind == CrossingPaint::Kind::Zebra);
	}

	void testUnmarkedRoadNodesSuppressDefaultZebra()
	{
		auto elements =
				crossing_network({{"highway", "footway"}, {"footway", "crossing"}},
						{{"highway", "crossing"}, {"crossing", "unmarked"}},
						{{"highway", "crossing"}, {"crossing", "informal"}});
		const auto paint =
				RoadMarkingIndex::build(elements, 1.0, decals::SignRegion::Europe)
						.crossing_way_paint(elements[2].as_way());
		UASSERT(!paint.has_value());
	}

	void testUnrelatedCrossingNodeDoesNotControlWay()
	{
		auto elements =
				crossing_network({{"highway", "footway"}, {"footway", "crossing"}},
						{{"highway", "crossing"}}, {{"highway", "crossing"}});
		elements[2].as_way().nodes[0].tags.emplace("crossing", "unmarked");
		const auto paint =
				RoadMarkingIndex::build(elements, 1.0, decals::SignRegion::Europe)
						.crossing_way_paint(elements[2].as_way());
		UASSERT(paint.has_value());
		UASSERT(paint->kind == CrossingPaint::Kind::Zebra);
	}

	void testUntaggedCrossingNodeAloneStaysBare()
	{
		const std::vector<ProcessedElement> elements{ProcessedElement(way(1,
				{node(10, 0, 50), node(2, 40, 50, {{"highway", "crossing"}}),
						node(11, 100, 50)},
				{{"highway", "secondary"}}))};
		const auto index =
				RoadMarkingIndex::build(elements, 1.0, decals::SignRegion::Europe);
		UASSERT(index.way(1) == nullptr);
	}

	void testStopSignHeadingFollowsNearestJunction()
	{
		const auto stop = node(2, 0, -5, {{"highway", "stop"}});
		std::vector<ProcessedElement> elements{ProcessedElement(stop),
				ProcessedElement(
						way(1, {node(10, 0, -40), node(2, 0, -5), node(3, 0, 40)},
								{{"highway", "residential"}})),
				ProcessedElement(
						way(2, {node(11, -40, 40), node(3, 0, 40), node(12, 40, 40)},
								{{"highway", "primary"}}))};
		const auto headings = signage::build_node_headings(elements, SignageLevel::Full);
		const auto found = headings.find(2);
		UASSERT(found != headings.end());
		UASSERT(found->second.dir.second > 0.99);
		UASSERT(!found->second.both_ways);
	}
};

static TestFmArnisRoadMarkings g_test_fm_arnis_road_markings;
