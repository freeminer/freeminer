#include "test.h"

#include "mapgen/earth/arnis-cpp/src/data_processing.h"
#include "mapgen/earth/arnis-cpp/src/element_processing/relation_area.h"
#include "mapgen/earth/arnis-cpp/src/floodfill_cache.h"

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <optional>
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
		TEST(testInnerRingsCutHolesButIslandsStayWhole);
		TEST(testMissingLanduseTagRetainsRustPredicateSemantics);
		TEST(testOpenRideOutlineSuppressesContainedBuildingPart);
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
		const auto kept = arnis::without_holes(outer_fill, &*holes);
		const std::pair<int, int> land_cell{5, 5};
		const std::pair<int, int> hole_cell{20, 20};
		UASSERT(std::find(kept->begin(), kept->end(), land_cell) != kept->end());
		UASSERT(std::find(kept->begin(), kept->end(), hole_cell) == kept->end());
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
};

static TestFmArnisRelationHoles g_test_fm_arnis_relation_holes;
