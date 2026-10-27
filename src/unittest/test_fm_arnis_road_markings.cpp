#include "test.h"

#include "mapgen/earth/arnis-cpp/src/element_processing/highways.h"
#include "mapgen/earth/arnis-cpp/src/element_processing/road_markings.h"
#include "mapgen/earth/arnis-cpp/src/element_processing/signage.h"
#include "mapgen/earth/arnis-cpp/src/decals/draw.h"
#include "mapgen/earth/arnis-cpp/src/decals/templates.h"

#include <algorithm>
#include <cstdint>
#include <initializer_list>
#include <string>
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
		TEST(testStopNamesUseReadableTwoLineFallback);
		TEST(testStopNameTextStaysOffPlateBorder);
		TEST(testPierFootwayIsNotARoadSurface);
	}

	void testPierFootwayIsNotARoadSurface()
	{
		const auto pier = way(1, {node(10, 0, 20), node(11, 40, 20)},
				{{"highway", "footway"}, {"man_made", "pier"}});
		const auto ordinary =
				way(2, {node(20, 0, 40), node(21, 40, 40)}, {{"highway", "footway"}});
		UASSERT(!highways::surface_highway_type(pier).has_value());
		UASSERT(highways::surface_highway_type(ordinary) == "footway");
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

	void testStopNamesUseReadableTwoLineFallback()
	{
		using arnis::decals::Canvas;
		using arnis::decals::TextStyle;
		using arnis::decals::TextStyleKind;
		Canvas plate(128, 128);
		plate.rounded_rect(4, 46, 120, 36, 3, arnis::decals::colors::WHITE);
		plate.stroke_rounded_rect(4, 46, 120, 36, 3, 2, arnis::decals::colors::DARK_GRAY);
		const auto blank_plate = plate.pixels();
		arnis::decals::templates::text_sign(
				plate, TextStyle{TextStyleKind::StopName}, "Bethaniendamm");
		// Text is now fitted within the two-line plate instead of being omitted when
		// the complete stop name cannot fit on one line.
		UASSERT(!std::equal(
				plate.pixels().begin(), plate.pixels().end(), blank_plate.begin()));
	}

	void testStopNameTextStaysOffPlateBorder()
	{
		using namespace arnis::decals;
		Canvas baseline(128, 128);
		baseline.rounded_rect(4, 46, 120, 36, 3, colors::WHITE);
		baseline.stroke_rounded_rect(4, 46, 120, 36, 3, 2, colors::DARK_GRAY);
		std::vector<std::string> names{"Varian-Fry-Straße/Potsdamer Platz",
				"Friedrichstraße/Reinhardtstraße", "Mollstraße/Otto-Braun-Straße"};
		for (int n = 1; n < 40; ++n)
			names.push_back("J" + std::string(static_cast<std::size_t>(n), 'f'));
		for (const auto &name : names) {
			Canvas rendered(128, 128);
			arnis::decals::templates::text_sign(
					rendered, TextStyle{TextStyleKind::StopName}, name);
			for (const int x : {0, 1, 126, 127})
				for (int y = 46; y < 82; ++y)
					UASSERT(rendered.get(x, y) == baseline.get(x, y));
		}
	}
};

static TestFmArnisRoadMarkings g_test_fm_arnis_road_markings;
