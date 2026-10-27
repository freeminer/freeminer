#include "test.h"

#include "mapgen/earth/arnis-cpp/src/element_processing/buildings.h"

#include <cstddef>
#include <initializer_list>
#include <optional>
#include <utility>

namespace
{
arnis::ProcessedWay roof_way(
		std::initializer_list<std::pair<const char *, const char *>> tags)
{
	arnis::ProcessedWay result{};
	result.id = 78;
	for (const auto &[key, value] : tags)
		result.tags.emplace(key, value);
	const int coordinates[5][2]{{10, 10}, {16, 10}, {16, 14}, {10, 14}, {10, 10}};
	for (std::size_t i = 0; i < 5; ++i) {
		arnis::ProcessedNode node{};
		node.id = i + 1;
		node.x = coordinates[i][0];
		node.z = coordinates[i][1];
		result.nodes.push_back(node);
	}
	return result;
}

std::optional<int> roof_top(const arnis::ProcessedWay &way)
{
	arnis::WorldEditor editor;
	arnis::Args args;
	args.mode = arnis::GenerationMode::GeoOnly;
	args.ground_level = 0;
	arnis::buildings::generate_buildings(&editor, way, args, std::nullopt);
	return editor.highest_block_between(13, 12, 0, 200);
}
} // namespace

class TestFmArnisRoofLayers : public TestBase
{
public:
	TestFmArnisRoofLayers() { TestManager::registerTestModule(this); }
	const char *getName() override { return "TestFmArnisRoofLayers"; }
	void runTests(IGameDef *) override
	{
		TEST(testLayerDoesNotLiftOrdinaryRoof);
		TEST(testLayerStacksBuildingPartRoof);
	}

	void testLayerDoesNotLiftOrdinaryRoof()
	{
		const auto plain = roof_top(roof_way({{"building", "roof"}}));
		const auto layered = roof_top(roof_way({{"building", "roof"}, {"layer", "1"}}));
		UASSERT(plain.has_value());
		UASSERT(layered == plain);
	}

	void testLayerStacksBuildingPartRoof()
	{
		const auto plain = roof_top(roof_way({{"building:part", "roof"}}));
		const auto layered =
				roof_top(roof_way({{"building:part", "roof"}, {"layer", "1"}}));
		UASSERT(plain.has_value());
		UASSERT(layered.has_value());
		UASSERT(*layered > *plain);
	}
};

static TestFmArnisRoofLayers g_test_fm_arnis_roof_layers;
