#include "test.h"

#include "mapgen/earth/arnis-cpp/src/data_processing.h"
#include "mapgen/earth/arnis-cpp/src/building_suppression.h"
#include "mapgen/earth/arnis-cpp/src/element_processing/buildings.h"
#include "mapgen/earth/arnis-cpp/src/element_processing/relation_area.h"
#include "mapgen/earth/arnis-cpp/src/element_processing/subprocessor/interior/index.h"
#include "mapgen/earth/arnis-cpp/src/element_processing/waterfront.h"
#include "mapgen/earth/arnis-cpp/src/floodfill_cache.h"
#include "mapgen/earth/arnis-cpp/src/land_cover/osm_water_override.h"
#include "mapgen/earth/arnis-cpp/src/osm_parser.h"
#include "mapgen/earth/fm_osm_relation_tags.h"

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <initializer_list>
#include <optional>
#include <sstream>
#include <string>
#include <unordered_set>
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
		TEST(testPierKeepsDeckOnIslandInsideHole);
		TEST(testBuildingCourtyardKeepsNestedKeepFootprint);
		TEST(testWaterOverrideKeepsPondOnIslandInHole);
		TEST(testWaterOverrideClipsStraddlingPolygon);
		TEST(testWaterGuardUsesStableNearestSeedTieBreak);
		TEST(testBuildingInAreaHoleDoesNotInheritAreaUse);
		TEST(testMissingLanduseTagRetainsRustPredicateSemantics);
		TEST(testOpenRideOutlineSuppressesContainedBuildingPart);
		TEST(testListedNonResidentialPartRelationDoesNotInheritHousing);
		TEST(testHousingPartRelationUsesHousingStoreyHeight);
		TEST(testStreamingXmlParserHandlesQuotedMarkupAndEntityOrder);
		TEST(testRepeatedRelationAreaOuterSuppressionMatchesRust);
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
		for (const auto *value :
				{"residential", "commercial", "education", "religious"}) {
			arnis::tags_t broad_area;
			broad_area.emplace("landuse", value);
			UASSERT(!arnis::element_processing::landuse_paints_ground(broad_area));
		}
		arnis::tags_t forest;
		forest.emplace("landuse", "forest");
		UASSERT(arnis::element_processing::landuse_paints_ground({}));
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

	void testWaterOverrideClipsStraddlingPolygon()
	{
		arnis::ProcessedWay way;
		way.id = 41;
		way.tags.emplace("natural", "water");
		const int coordinates[5][2]{{8, 4}, {30, 4}, {30, 11}, {8, 11}, {8, 4}};
		for (std::size_t i = 0; i < 5; ++i) {
			arnis::ProcessedNode node{};
			node.id = i == 4 ? 1 : i + 1;
			node.x = coordinates[i][0];
			node.z = coordinates[i][1];
			way.nodes.push_back(std::move(node));
		}

		constexpr std::size_t side = 16;
		arnis::land_cover::LandCoverData data;
		data.width = data.height = side;
		data.grid.assign(side, std::vector<std::uint8_t>(side, 0));
		std::vector<std::vector<float>> heights(side, std::vector<float>(side, 0.0f));
		arnis::land_cover::apply_osm_water_override(data, heights, side, side,
				{arnis::ProcessedElement(way)}, XZBBox(0, 0, 15, 15));

		UASSERT(data.grid[7][10] == arnis::land_cover::LC_WATER);
		for (std::size_t z = 0; z < side; ++z)
			for (std::size_t x = 0; x < side; ++x)
				if (data.grid[z][x] == arnis::land_cover::LC_WATER)
					UASSERT(x >= 8 && x <= 15 && z >= 4 && z <= 11);
	}

	void testWaterGuardUsesStableNearestSeedTieBreak()
	{
		using namespace arnis::land_cover;
		constexpr std::size_t side = 3;
		std::vector<std::vector<std::uint8_t>> grid(
				side, std::vector<std::uint8_t>(side, LC_BARE));
		std::vector<std::vector<float>> heights(side, std::vector<float>(side, 0.0f));
		grid[0][0] = grid[0][2] = LC_WATER;
		heights[0][0] = 10.0f;
		heights[0][2] = 20.0f;

		const auto guard = build_water_override_guard(grid, heights, 0.0);
		UASSERT(guard.has_elevation);
		UASSERT(guard.nearest_water_y.size() == side * side);
		// Multi-source BFS seeds are enqueued in row-major order. The equidistant
		// middle cell therefore retains the first water source's elevation.
		UASSERT(guard.nearest_water_y[1] == 10.0f);
		UASSERT(guard.nearest_water_y[2] == 20.0f);
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

	void testPierKeepsDeckOnIslandInsideHole()
	{
		auto ring = [](std::uint64_t id, int min, int max) {
			arnis::ProcessedWay way;
			way.id = id;
			way.nodes = square(min, max);
			for (std::size_t i = 0; i + 1 < way.nodes.size(); ++i)
				way.nodes[i].id = id * 10 + i;
			way.nodes.back().id = way.nodes.front().id;
			return way;
		};

		arnis::ProcessedRelation pier{};
		pier.id = 9;
		pier.tags.emplace("man_made", "pier");
		pier.members.emplace_back(ring(1, 10, 60), arnis::ProcessedMemberRole::Outer);
		pier.members.emplace_back(ring(2, 20, 50), arnis::ProcessedMemberRole::Inner);
		pier.members.emplace_back(ring(3, 30, 40), arnis::ProcessedMemberRole::Outer);

		auto footway = [](std::uint64_t id, int x0, int x1, int z) {
			arnis::ProcessedWay way;
			way.id = id;
			way.tags.emplace("highway", "footway");
			for (const auto &[x, node_id] : std::vector<std::pair<int, std::uint64_t>>{
						 {x0, id * 10}, {x1, id * 10 + 1}}) {
				arnis::ProcessedNode node{};
				node.id = node_id;
				node.x = x;
				node.z = z;
				way.nodes.push_back(node);
			}
			return way;
		};
		std::vector<arnis::ProcessedElement> elements{
				arnis::ProcessedElement(std::move(pier)),
				arnis::ProcessedElement(footway(4, 32, 38, 35)),
				arnis::ProcessedElement(footway(5, 22, 28, 25))};
		const auto cache = arnis::FloodFillCache::precompute(elements, std::nullopt);

		UASSERT(arnis::waterfront::drop_paths_on_piers(
						elements, 1.0, cache, std::nullopt) == 1);
		UASSERT(elements.size() == 2);
		UASSERT(elements[1].is_way());
		UASSERT(elements[1].as_way().id == 5);
	}

	void testBuildingCourtyardKeepsNestedKeepFootprint()
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
		auto outer = make_way(1, 10, 90);
		auto courtyard = make_way(2, 20, 80);
		auto keep = make_way(3, 40, 60);
		arnis::ProcessedRelation castle{};
		castle.id = 9;
		castle.tags.emplace("building", "castle");
		castle.members.emplace_back(outer, arnis::ProcessedMemberRole::Outer);
		castle.members.emplace_back(courtyard, arnis::ProcessedMemberRole::Inner);
		castle.members.emplace_back(keep, arnis::ProcessedMemberRole::Outer);
		const std::vector<arnis::ProcessedElement> elements{castle};
		const XZBBox bbox(0, 0, 99, 99);
		const auto footprints =
				arnis::FloodFillCache{}.collect_building_footprints(elements, bbox);
		UASSERT(footprints.contains(15, 15));
		UASSERT(!footprints.contains(30, 30));
		UASSERT(footprints.contains(50, 50));
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

	void testListedNonResidentialPartRelationDoesNotInheritHousing()
	{
		auto way = [](std::uint64_t id, double min, double max) {
			arnis::ProcessedWay result;
			result.id = id;
			const double coords[5][2]{
					{min, min}, {max, min}, {max, max}, {min, max}, {min, min}};
			for (int i = 0; i < 5; ++i) {
				arnis::ProcessedNode node{};
				node.id = id * 10 + static_cast<std::uint64_t>(i);
				node.latitude = coords[i][0];
				node.longitude = coords[i][1];
				result.nodes.push_back(node);
				result.source_node_refs.push_back(node.id);
			}
			return result;
		};
		auto outer = way(10, 0.0, 1.0);
		auto part = way(20, 0.2, 0.4);
		arnis::ProcessedRelation housing_outline{};
		housing_outline.id = 1;
		housing_outline.tags.emplace("type", "multipolygon");
		housing_outline.tags.emplace("building", "apartments");
		housing_outline.source_members.push_back({"way", outer.id, "outer"});
		arnis::ProcessedRelation part_relation{};
		part_relation.id = 2;
		part_relation.tags.emplace("type", "multipolygon");
		part_relation.tags.emplace("building:part", "yes");
		part_relation.source_members.push_back({"way", part.id, "outer"});

		const auto prepare = [&](bool listed_by_office) {
			std::vector<arnis::ProcessedElement> elements;
			elements.emplace_back(outer);
			elements.emplace_back(part);
			elements.emplace_back(housing_outline);
			elements.emplace_back(part_relation);
			if (listed_by_office) {
				arnis::ProcessedRelation office{};
				office.id = 3;
				office.tags.emplace("type", "building");
				office.tags.emplace("building", "office");
				office.source_members.push_back(
						{"relation", housing_outline.id, "outline"});
				office.source_members.push_back({"relation", part_relation.id, "part"});
				elements.emplace_back(std::move(office));
			}
			return arnis::prepare_building_data(elements);
		};
		UASSERT(prepare(false).housing_relation_parts.contains(part_relation.id));
		UASSERT(!prepare(true).housing_relation_parts.contains(part_relation.id));
	}

	void testHousingPartRelationUsesHousingStoreyHeight()
	{
		using namespace arnis;
		ProcessedWay outer;
		outer.id = 7;
		const int coordinates[5][2]{{20, 20}, {40, 20}, {40, 36}, {20, 36}, {20, 20}};
		for (std::size_t i = 0; i < 5; ++i) {
			ProcessedNode node{};
			node.id = 70 + i;
			node.x = coordinates[i][0];
			node.z = coordinates[i][1];
			outer.nodes.push_back(node);
		}

		ProcessedRelation relation{};
		relation.id = 9;
		relation.tags.emplace("type", "multipolygon");
		relation.tags.emplace("building:part", "yes");
		relation.tags.emplace("building:levels", "5");
		relation.tags.emplace("roof:shape", "flat");
		relation.members.emplace_back(outer, ProcessedMemberRole::Outer);

		const XZBBox bbox(0, 0, 79, 79);
		const auto top = [&](const std::unordered_set<std::uint64_t> &housing_parts) {
			WorldEditor editor;
			Args args;
			args.mode = GenerationMode::GeoOnly;
			args.ground_level = 0;
			FloodFillCache cache;
			const auto passages = CoordinateBitmap::new_empty();
			buildings::generate_building_from_relation(editor, relation, args, cache,
					bbox, passages, nullptr, nullptr, nullptr, nullptr, &housing_parts);
			return editor.highest_block_between(30, 20, -64, 300);
		};

		const std::unordered_set<std::uint64_t> ordinary_parts;
		const std::unordered_set<std::uint64_t> housing_parts{relation.id};
		UASSERT(top(ordinary_parts) == std::optional<int>(23));
		UASSERT(top(housing_parts) == std::optional<int>(18));
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

	void testRepeatedRelationAreaOuterSuppressionMatchesRust()
	{
		arnis::tags_t repeated_outer;
		repeated_outer.emplace("natural", "water");
		repeated_outer.emplace("name", "Lake");
		repeated_outer.emplace("wikidata", "Q123");
		arnis::tags_t water_relation;
		water_relation.emplace("natural", "water");
		UASSERT(arnis::osm_parser::repeats_relation_area_tags(
				repeated_outer, water_relation));

		// An outer with a separate highway feature remains a standalone way.
		arnis::tags_t independent_way = repeated_outer;
		independent_way.emplace("highway", "footway");
		UASSERT(!arnis::osm_parser::repeats_relation_area_tags(
				independent_way, water_relation));

		// A differently-valued classification is not repeated relation data.
		arnis::tags_t different_water;
		different_water.emplace("water", "lake");
		arnis::tags_t land_relation;
		land_relation.emplace("water", "river");
		UASSERT(!arnis::osm_parser::repeats_relation_area_tags(
				different_water, land_relation));

		arnis::tags_t metadata_only;
		metadata_only.emplace("name", "Unclassified ring");
		UASSERT(!arnis::osm_parser::repeats_relation_area_tags(
				metadata_only, water_relation));
	}
};

static TestFmArnisRelationHoles g_test_fm_arnis_relation_holes;
