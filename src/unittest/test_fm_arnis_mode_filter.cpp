#include "test.h"

#include "mapgen/earth/arnis-cpp/src/mode_filter.h"
#include "mapgen/earth/arnis-cpp/src/osm_parser.h"
#include "mapgen/earth/arnis-cpp/src/data_processing.h"
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
		TEST(testRustParityOsmTagFilters);
		TEST(testRustPrioritySortLeavesBroadLanduseLast);
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

	void testRustParityOsmTagFilters()
	{
		arnis::tags_t proposed;
		proposed.emplace("highway", "proposed");
		proposed.emplace("bridge", "yes");
		proposed.emplace("tunnel", "yes");
		proposed.emplace("layer", "1");
		arnis::osm_parser::filter_tags(proposed);
		UASSERT(proposed.empty());

		arnis::tags_t railway_bridge;
		railway_bridge.emplace("highway", "razed");
		railway_bridge.emplace("railway", "rail");
		railway_bridge.emplace("bridge", "yes");
		railway_bridge.emplace("natural", "peninsula");
		arnis::osm_parser::filter_tags(railway_bridge);
		UASSERT(!railway_bridge.contains("highway"));
		UASSERT(!railway_bridge.contains("natural"));
		UASSERT(railway_bridge.get("railway") == "rail");
		UASSERT(railway_bridge.get("bridge") == "yes");

		arnis::tags_t construction;
		construction.emplace("highway", "construction");
		arnis::osm_parser::filter_tags(construction);
		UASSERT(construction.get("highway") == "construction");
		arnis::tags_t woodland;
		woodland.emplace("natural", "wood");
		arnis::osm_parser::filter_tags(woodland);
		UASSERT(woodland.get("natural") == "wood");
	}

	void testRustPrioritySortLeavesBroadLanduseLast()
	{
		std::vector<arnis::ProcessedElement> elements{way(1, {{"landuse", "meadow"}}),
				way(2, {{"man_made", "mast"}}), way(3, {{"amenity", "parking"}}),
				way(4, {{"building:part", "yes"}}), way(5, {{"highway", "service"}}),
				way(6, {{"building", "yes"}})};
		arnis::sort_elements_by_priority(elements);
		const std::vector<std::uint64_t> expected{6, 5, 3, 4, 2, 1};
		for (std::size_t i = 0; i < expected.size(); ++i)
			UASSERT(elements[i].id() == expected[i]);

		// A building carrying landuse is still sorted by its building priority.
		elements = {way(10, {{"landuse", "meadow"}}),
				way(11, {{"building", "retail"}, {"landuse", "retail"}}),
				way(12, {{"building:part", "yes"}})};
		arnis::sort_elements_by_priority(elements);
		UASSERT(elements[0].id() == 11);
		UASSERT(elements[1].id() == 12);
		UASSERT(elements[2].id() == 10);
	}
};

static TestFmArnisModeFilter g_test_fm_arnis_mode_filter;
