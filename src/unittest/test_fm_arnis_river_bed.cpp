#include "test.h"

#include "mapgen/earth/arnis-cpp/src/osm_parser.h"
#include "mapgen/earth/arnis-cpp/src/river_bed.h"
#include "mapgen/earth/arnis-cpp/src/water_depth.h"
#include "mapgen/earth/arnis_ground.h"

#include <cstdint>
#include <initializer_list>
#include <optional>
#include <utility>
#include <vector>

namespace
{
using namespace arnis;

tags_t tags(std::initializer_list<std::pair<const char *, const char *>> values)
{
	tags_t result;
	for (const auto &[key, value] : values)
		result.emplace(key, value);
	return result;
}

ProcessedWay way(std::uint64_t id, std::initializer_list<std::pair<int, int>> points,
		tags_t values)
{
	ProcessedWay result{};
	result.id = id;
	result.tags = std::move(values);
	std::uint64_t node_id = id * 100;
	for (const auto &[x, z] : points) {
		ProcessedNode node{};
		node.id = node_id++;
		node.x = x;
		node.z = z;
		result.nodes.push_back(std::move(node));
	}
	return result;
}

ProcessedWay rectangle(std::uint64_t id, int x0, int x1, int z0, int z1, tags_t values)
{
	return way(id, {{x0, z0}, {x1, z0}, {x1, z1}, {x0, z1}, {x0, z0}}, std::move(values));
}

river_bed::RiverBedField build(
		const std::vector<ProcessedElement> &elements, const Ground &ground)
{
	const water_depth::BigWaterField legacy;
	return river_bed::compute_river_bed_field(river_bed::RiverBed::V1, elements, ground,
			legacy, 0, 0, 0, 1299, 0, 699, 1.0);
}
} // namespace

class TestFmArnisRiverBed : public TestBase
{
public:
	TestFmArnisRiverBed() { TestManager::registerTestModule(this); }
	const char *getName() override { return "TestFmArnisRiverBed"; }
	void runTests(IGameDef *) override
	{
		TEST(testStreamLineHasShallowBanks);
		TEST(testRiverFieldCrossesSparseTileBoundaries);
		TEST(testIslandInsideHoleKeepsItsRiverBed);
		TEST(testNonRiverWaterDoesNotGetRiverBed);
	}

	void testStreamLineHasShallowBanks()
	{
		const auto stream =
				way(1, {{100, 20}, {100, 180}}, tags({{"waterway", "stream"}}));
		const auto field = build({ProcessedElement(stream)}, Ground::new_flat(0));
		const std::vector<std::optional<int>> expected{
				std::nullopt, 0, 1, 1, 1, 0, std::nullopt};
		std::vector<std::optional<int>> actual;
		for (int x = 97; x <= 103; ++x)
			actual.push_back(field.depth_override(x, 100));
		UASSERT(actual == expected);
	}

	void testRiverFieldCrossesSparseTileBoundaries()
	{
		const auto river = rectangle(
				2, 40, 1250, 300, 360, tags({{"natural", "water"}, {"water", "river"}}));
		const auto field = build({ProcessedElement(river)}, Ground::new_flat(0));
		for (const int x : {511, 512, 1023, 1024})
			UASSERT(field.depth_override(x, 330).has_value());
		UASSERT(!field.depth_override(39, 330).has_value());
		UASSERT(!field.depth_override(1251, 330).has_value());
	}

	void testNonRiverWaterDoesNotGetRiverBed()
	{
		const auto lake = rectangle(3, 40, 160, 40, 160, tags({{"natural", "water"}}));
		const auto field = build({ProcessedElement(lake)}, Ground::new_flat(0));
		UASSERT(field.empty());
	}

	void testIslandInsideHoleKeepsItsRiverBed()
	{
		ProcessedRelation relation{};
		relation.id = 4;
		relation.tags = tags({{"natural", "water"}, {"water", "river"}});
		relation.members.emplace_back(
				rectangle(40, 10, 190, 10, 190, {}), ProcessedMemberRole::Outer);
		relation.members.emplace_back(
				rectangle(41, 80, 120, 80, 120, {}), ProcessedMemberRole::Outer);
		relation.members.emplace_back(
				rectangle(42, 60, 140, 60, 140, {}), ProcessedMemberRole::Inner);
		const auto field = build({ProcessedElement(relation)}, Ground::new_flat(0));
		UASSERT(field.depth_override(30, 100).has_value());
		UASSERT(!field.depth_override(70, 100).has_value());
		UASSERT(field.depth_override(100, 100).has_value());
	}
};

static TestFmArnisRiverBed g_test_fm_arnis_river_bed;
