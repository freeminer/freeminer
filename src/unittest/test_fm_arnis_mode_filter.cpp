#include "test.h"

#include "mapgen/earth/arnis-cpp/src/mode_filter.h"
#include "mapgen/earth/arnis-cpp/src/world_editor/common.h"

#include <algorithm>
#include <cstdint>
#include <initializer_list>
#include <string>
#include <utility>
#include <vector>

namespace
{
arnis::ProcessedElement way(std::uint64_t id,
		std::initializer_list<std::pair<const char *, const char *>> tags)
{
	arnis::tags_t values;
	for (const auto &[key, value] : tags)
		values.emplace(key, value);
	return arnis::ProcessedWay{id, {}, std::move(values)};
}

arnis::ProcessedElement node(std::uint64_t id,
		std::initializer_list<std::pair<const char *, const char *>> tags)
{
	arnis::tags_t values;
	for (const auto &[key, value] : tags)
		values.emplace(key, value);
	return arnis::ProcessedNode{id, std::move(values), 0, 0};
}

bool has_id(const std::vector<arnis::ProcessedElement> &elements, std::uint64_t id)
{
	return std::any_of(elements.begin(), elements.end(),
			[&](const auto &element) { return element.id() == id; });
}
} // namespace

class TestFmArnisModeFilter : public TestBase
{
public:
	TestFmArnisModeFilter() { TestManager::registerTestModule(this); }
	const char *getName() override { return "TestFmArnisModeFilter"; }
	void runTests(IGameDef *) override
	{
		TEST(testTerrainOnlyKeepsWaterAndNature);
		TEST(testTerrainOnlyDropsBuiltFeatures);
		TEST(testBlueprintKeepsPlansAndDropsFurniture);
		TEST(testStorageMayContainChecksItsRepresentation);
	}

	void testTerrainOnlyKeepsWaterAndNature()
	{
		std::vector<arnis::ProcessedElement> elements{way(1, {{"natural", "water"}}),
				way(2, {{"waterway", "river"}}), way(3, {{"natural", "wood"}}),
				way(4, {{"landuse", "reservoir"}}), way(5, {{"water", "lake"}})};
		arnis::mode_filter::retain_for_mode(elements, arnis::GenerationMode::TerrainOnly);
		UASSERT(elements.size() == 5);
		for (std::uint64_t id = 1; id <= 5; ++id)
			UASSERT(has_id(elements, id));
	}

	void testTerrainOnlyDropsBuiltFeatures()
	{
		std::vector<arnis::ProcessedElement> elements{way(1, {{"building", "yes"}}),
				way(2, {{"highway", "residential"}}), way(3, {{"waterway", "dam"}}),
				way(4, {{"landuse", "farmland"}}), way(5, {{"natural", "tree_row"}}),
				node(6, {{"natural", "tree"}}),
				way(7, {{"natural", "water"}, {"amenity", "fountain"}}),
				way(8, {{"natural", "water"}, {"leisure", "swimming_pool"}})};
		arnis::mode_filter::retain_for_mode(elements, arnis::GenerationMode::TerrainOnly);
		UASSERT(elements.empty());
	}

	void testBlueprintKeepsPlansAndDropsFurniture()
	{
		std::vector<arnis::ProcessedElement> elements{way(1, {{"building", "house"}}),
				way(2, {{"highway", "primary"}}), way(3, {{"railway", "rail"}}),
				way(4, {{"leisure", "park"}}), way(5, {{"amenity", "parking"}}),
				node(6, {{"natural", "tree"}}), way(7, {{"building:part", "yes"}}),
				way(8, {{"building:part", "no"}, {"building", "yes"}}),
				way(9, {{"building", "yes"}, {"building:part", "yes"}}),
				way(10, {{"amenity", "school"}}), way(11, {{"leisure", "playground"}}),
				way(12, {{"power", "line"}}), node(13, {{"highway", "street_lamp"}}),
				node(14, {{"amenity", "bench"}})};
		arnis::mode_filter::retain_for_mode(elements, arnis::GenerationMode::Blueprint);
		for (std::uint64_t id : {1, 2, 3, 4, 5, 6, 8})
			UASSERT(has_id(elements, id));
		for (std::uint64_t id : {7, 9, 10, 11, 12, 13, 14})
			UASSERT(!has_id(elements, id));
	}

	void testStorageMayContainChecksItsRepresentation()
	{
		const arnis::Block low_a(1), low_b(2), high(300);
		arnis::world_editor::BlockStorage dense(low_a);
		UASSERT(dense.may_contain(low_a));
		UASSERT(!dense.may_contain(low_b));
		dense.set(0, low_b);
		UASSERT(dense.may_contain(low_b));
		dense.set(0, low_a);
		UASSERT(!dense.may_contain(low_b));

		arnis::world_editor::BlockStorage paletted(low_a);
		paletted.set(0, high);
		UASSERT(paletted.may_contain(high));
		paletted.set(0, low_a);
		// A palette may retain an ID after the last corresponding cell changes.
		UASSERT(paletted.may_contain(high));
	}
};

static TestFmArnisModeFilter g_test_fm_arnis_mode_filter;
