#include "test.h"

#include "mapgen/earth/arnis-cpp/src/data_processing.h"
#include "mapgen/earth/arnis-cpp/src/element_processing/relation_area.h"
#include "mapgen/earth/arnis-cpp/src/element_processing/subprocessor/interior/index.h"
#include "mapgen/earth/arnis-cpp/src/floodfill_cache.h"
#include "mapgen/earth/arnis-cpp/src/land_cover/osm_water_override.h"
#include "mapgen/earth/arnis-cpp/src/osm_parser.h"

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <initializer_list>
#include <optional>
#include <sstream>
#include <string>
#include <utility>
#include <vector>

namespace
{
std::vector<arnis::ProcessedNode> square(int min, int max)
{
	std::vector<arnis::ProcessedNode> ring(5);
	const int coords[5][2]{{min, min}, {max, min}, {max, max}, {min, max}, {min, min}};
	for (std::size_t i = 0; i < ring.size(); ++i) {
		ring[i].x = coords[i][0];
		ring[i].z = coords[i][1];
	}
	return ring;
}
} // namespace

class TestFmArnisRelationHoles : public TestBase
{
public:
	TestFmArnisRelationHoles() { TestManager::registerTestModule(this); }
	const char *getName() override { return "TestFmArnisRelationHoles"; }
	void runTests(IGameDef *) override
	{
		TEST(testNestedOuterGetsItsOwnHoleGroup);
		TEST(testHoleGroupingIgnoresBoundaryVertices);
		TEST(testInnerRingsCutHolesButIslandsStayWhole);
		TEST(testWaterOverrideKeepsPondOnIslandInHole);
		TEST(testBuildingInAreaHoleDoesNotInheritAreaUse);
		TEST(testMissingLanduseTagRetainsRustPredicateSemantics);
		TEST(testOpenRideOutlineSuppressesContainedBuildingPart);
		TEST(testStreamingXmlParserHandlesQuotedMarkupAndEntityOrder);
	}

	void testNestedOuterGetsItsOwnHoleGroup()
	{
		using namespace arnis::element_processing;
		const auto groups =
				hole_groups(std::vector<std::vector<arnis::ProcessedNode>>{square(0, 60),
									square(25, 35)},
						std::vector<std::vector<arnis::ProcessedNode>>{
								square(15, 45), square(28, 32)});
		UASSERT(groups.size() == 2);
		UASSERT(groups[0].first == std::vector<std::size_t>{0});
		UASSERT(groups[0].second == (std::vector<std::size_t>{0, 1}));
		UASSERT(groups[1].first == std::vector<std::size_t>{1});
		UASSERT(groups[1].second == std::vector<std::size_t>{1});
	}

	void testHoleGroupingIgnoresBoundaryVertices()
	{
		auto ring = [](std::initializer_list<std::pair<int, int>> coordinates) {
			std::vector<arnis::ProcessedNode> result;
			for (const auto &[x, z] : coordinates) {
				arnis::ProcessedNode node{};
				node.x = x;
				node.z = z;
				result.push_back(node);
			}
			return result;
		};
		// Boundary vertices count as neither inside nor outside in Rust's
		// majority test, so the outside vertices keep this ring from being an island.
		const auto island =
				ring({{-1, 1}, {0, 1}, {5, 1}, {5, 5}, {0, 5}, {-1, 5}, {-1, 1}});
		const auto hole = ring({{0, 0}, {10, 0}, {10, 10}, {0, 10}, {0, 0}});
		UASSERT(!arnis::element_processing::mostly_inside(
				island, hole, [](const arnis::ProcessedNode &point) {
					return std::pair<std::int32_t, std::int32_t>{point.x, point.z};
				}));
		auto xz = [](const auto &points) {
			std::vector<std::pair<std::int32_t, std::int32_t>> result;
			for (const auto &point : points)
				result.emplace_back(point.x, point.z);
			return result;
		};
		const auto island_points = xz(island);
		const auto hole_points = xz(hole);
		UASSERT(!arnis::element_processing::mostly_inside(
				island_points, hole_points, [](const auto &point) { return point; }));
		const auto groups = arnis::element_processing::hole_groups(
				std::vector<std::vector<arnis::ProcessedNode>>{island},
				std::vector<std::vector<arnis::ProcessedNode>>{hole});
		UASSERT(groups.size() == 1);
		UASSERT(groups[0].first == std::vector<std::size_t>{0});
		UASSERT(groups[0].second == std::vector<std::size_t>{0});
	}

	void testMissingLanduseTagRetainsRustPredicateSemantics()
	{
		arnis::tags_t residential;
		residential.emplace("landuse", "residential");
		arnis::tags_t forest;
		forest.emplace("landuse", "forest");
		UASSERT(arnis::element_processing::landuse_paints_ground({}));
		UASSERT(!arnis::element_processing::landuse_paints_ground(residential));
		UASSERT(arnis::element_processing::landuse_paints_ground(forest));
	}

	void testWaterOverrideKeepsPondOnIslandInHole()
	{
		auto make_way = [](std::uint64_t id, int min, int max) {
			arnis::ProcessedWay way;
			way.id = id;
			way.nodes = square(min, max);
			for (std::size_t i = 0; i + 1 < way.nodes.size(); ++i)
				way.nodes[i].id = id * 10 + i;
			way.nodes.back().id = way.nodes.front().id;
			return way;
		};
		auto outer = make_way(1, 0, 60);
		auto hole = make_way(2, 15, 45);
		auto pond = make_way(3, 25, 35);
		arnis::ProcessedRelation relation{};
		relation.id = 9;
		relation.tags.emplace("natural", "water");
		relation.members.emplace_back(outer, arnis::ProcessedMemberRole::Outer);
		relation.members.emplace_back(hole, arnis::ProcessedMemberRole::Inner);
		relation.members.emplace_back(pond, arnis::ProcessedMemberRole::Outer);

		constexpr std::size_t side = 61;
		arnis::land_cover::LandCoverData data;
		data.width = data.height = side;
		data.grid.assign(side, std::vector<std::uint8_t>(side, 1));
		std::vector<std::vector<float>> heights(side, std::vector<float>(side, 10.0f));
		const auto bbox = XZBBox(0, 0, 60, 60);
		arnis::land_cover::apply_osm_water_override(
				data, heights, side, side, {arnis::ProcessedElement(relation)}, bbox);

		UASSERT(data.grid[5][5] == arnis::land_cover::LC_WATER);
		UASSERT(data.grid[20][20] != arnis::land_cover::LC_WATER);
		UASSERT(data.grid[30][30] == arnis::land_cover::LC_WATER);
	}

	void testBuildingInAreaHoleDoesNotInheritAreaUse()
	{
		auto way = [](std::uint64_t id, int min, int max, arnis::tags_t tags = {}) {
			arnis::ProcessedWay result;
			result.id = id;
			result.tags = std::move(tags);
			const int coords[5][2]{
					{min, min}, {max, min}, {max, max}, {min, max}, {min, min}};
			for (int i = 0; i < 5; ++i) {
				arnis::ProcessedNode node{};
				node.id = id * 10 + static_cast<std::uint64_t>(i);
				node.x = coords[i][0];
				node.z = coords[i][1];
				result.nodes.push_back(node);
			}
			return result;
		};
		auto building_tags = [] {
			arnis::tags_t tags;
			tags.emplace("building", "yes");
			return tags;
		};
		arnis::tags_t area_tags;
		area_tags.emplace("landuse", "industrial");
		area_tags.emplace("type", "multipolygon");
		arnis::ProcessedRelation area{};
		area.id = 9;
		area.tags = std::move(area_tags);
		area.members.emplace_back(way(50, 0, 150), arnis::ProcessedMemberRole::Outer);
		area.members.emplace_back(way(51, 40, 120), arnis::ProcessedMemberRole::Inner);
		area.members.emplace_back(way(52, 90, 110), arnis::ProcessedMemberRole::Outer);
		std::vector<arnis::ProcessedElement> elements;
		elements.emplace_back(way(1, 10, 20, building_tags()));
		elements.emplace_back(way(2, 60, 70, building_tags()));
		elements.emplace_back(way(3, 95, 105, building_tags()));
		elements.emplace_back(std::move(area));

		const auto bbox = XZBBox(0, 0, 199, 199);
		const auto index = arnis::interior_uses::InteriorUseIndex::build(elements, bbox);
		UASSERT(index.area(1)->kind == arnis::interior_uses::UseKind::Factory);
		UASSERT(!index.area(2).has_value());
		UASSERT(index.area(3)->kind == arnis::interior_uses::UseKind::Factory);
	}

	void testInnerRingsCutHolesButIslandsStayWhole()
	{
		auto ring = [](std::uint64_t id, int min, int max) {
			arnis::ProcessedWay way;
			way.id = id;
			const int coords[5][2]{
					{min, min}, {max, min}, {max, max}, {min, max}, {min, min}};
			for (int i = 0; i < 5; ++i) {
				arnis::ProcessedNode node{};
				node.id = id * 10 + static_cast<std::uint64_t>(i);
				node.x = coords[i][0];
				node.z = coords[i][1];
				way.nodes.push_back(node);
			}
			return way;
		};
		auto outer = ring(1, 0, 60);
		auto lake = ring(2, 15, 45);
		auto island = ring(3, 25, 35);
		arnis::ProcessedRelation relation{};
		relation.id = 9;
		relation.tags.emplace("natural", "wood");
		relation.members.emplace_back(outer, arnis::ProcessedMemberRole::Outer);
		relation.members.emplace_back(lake, arnis::ProcessedMemberRole::Inner);
		relation.members.emplace_back(island, arnis::ProcessedMemberRole::Outer);
		const std::vector<arnis::ProcessedElement> elements{relation};
		const auto fills = arnis::FloodFillCache::precompute(elements, std::nullopt);
		UASSERT(fills.contains(lake.id));
		const arnis::InnerRings inners(relation);
		const auto holes = fills.holes_in(outer, inners, std::nullopt);
		UASSERT(holes.has_value());
		UASSERT(holes->contains(20, 20));
		UASSERT(holes->contains(30, 30));
		UASSERT(!holes->contains(5, 5));
		UASSERT(!holes->contains(15, 30));
		UASSERT(holes->covers(15, 30));
		const auto outer_fill = fills.get_or_compute(outer, std::nullopt);
		UASSERT(outer_fill != nullptr);
		const auto kept = arnis::without_holes(*outer_fill, &*holes);
		const std::pair<int, int> land_cell{5, 5};
		const std::pair<int, int> hole_cell{20, 20};
		UASSERT(std::find(kept.begin(), kept.end(), land_cell) != kept.end());
		UASSERT(std::find(kept.begin(), kept.end(), hole_cell) == kept.end());
		UASSERT(!fills.holes_in(island, inners, std::nullopt).has_value());
	}

	void testOpenRideOutlineSuppressesContainedBuildingPart()
	{
		auto way = [](std::uint64_t id,
						   const std::vector<std::pair<double, double>> &points,
						   arnis::tags_t tags) {
			arnis::ProcessedWay result;
			result.id = id;
			result.tags = std::move(tags);
			for (const auto &[latitude, longitude] : points) {
				arnis::ProcessedNode node{};
				node.id = id * 100 + result.nodes.size();
				node.latitude = latitude;
				node.longitude = longitude;
				result.nodes.push_back(node);
			}
			return result;
		};
		arnis::tags_t ride_tags;
		ride_tags.emplace("attraction", "big_wheel");
		arnis::tags_t part_tags;
		part_tags.emplace("building:part", "yes");
		std::vector<arnis::ProcessedElement> elements;
		elements.emplace_back(
				way(1, {{0.0, 0.0}, {0.0, 1.0}, {1.0, 1.0}, {1.0, 0.0}}, ride_tags));
		elements.emplace_back(way(2,
				{{0.2, 0.2}, {0.2, 0.3}, {0.3, 0.3}, {0.3, 0.2}, {0.2, 0.2}}, part_tags));
		const auto prepared = arnis::prepare_building_data(elements);
		UASSERT(prepared.outline_suppression.contains({"way", 2}));
	}

	void testStreamingXmlParserHandlesQuotedMarkupAndEntityOrder()
	{
		std::istringstream xml(
				"<?xml version='1.0'?><osm><!-- <node id='999'/> -->"
				"<bounds minlat='1' minlon='2' maxlat='3' maxlon='4'/>"
				"<node id='1' lat='1.5' lon='2.5' note='a > b'>"
				"<tag k='building' v='yes'/><tag k='start_date' v='1900'/><tag "
				"k='name' v='A &amp;gt; B &amp; C'/><tag k='name:en' v='filtered'/>"
				"</node><node id='4'/><node id='5' lat='2.5' lon='3.5'/><way id='2'><nd "
				"ref='1'/><tag k='highway' v='road'/></way>"
				"<relation id='3'><member type='way' ref='2' role='outer'/></relation>"
				"</osm>");
		const auto document = arnis::osm_parser::parse_osm_xml(xml);
		UASSERT(document.nodes.size() == 3);
		UASSERT(document.nodes[0].id == 1);
		UASSERT(document.nodes[1].id == 4);
		UASSERT(!document.nodes[1].lat && !document.nodes[1].lon);
		UASSERT(document.nodes[0].tags.get("name") == "A &gt; B & C");
		UASSERT(document.nodes[0].tags.get("start_date") == "1900");
		UASSERT(!document.nodes[0].tags.contains("name:en"));
		UASSERT(document.ways.size() == 1);
		UASSERT(document.ways[0].node_refs == std::vector<std::uint64_t>{1});
		UASSERT(document.relations.size() == 1);
		UASSERT(document.relations[0].members.size() == 1);
		UASSERT(document.bounds.has_value());
		UASSERT((*document.bounds)[0] == 1.0 && (*document.bounds)[1] == 2.0 &&
				(*document.bounds)[2] == 3.0 && (*document.bounds)[3] == 4.0);
		auto without_source_bounds = document;
		without_source_bounds.bounds.reset();
		const auto derived = arnis::osm_parser::resolve_bbox(without_source_bounds);
		UASSERT(derived.has_value());
		UASSERT((*derived)[0] == 1.5 && (*derived)[1] == 2.5 && (*derived)[2] == 2.5 &&
				(*derived)[3] == 3.5);
	}
};

static TestFmArnisRelationHoles g_test_fm_arnis_relation_holes;
