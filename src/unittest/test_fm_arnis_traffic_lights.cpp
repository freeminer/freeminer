#include "test.h"

#include "mapgen/earth/arnis-cpp/src/block_definitions.h"
#include "mapgen/earth/arnis-cpp/src/element_processing/traffic_lights.h"
#include "mapgen/earth/arnis_world_editor.h"

#include <cstdint>
#include <utility>
#include <vector>

namespace
{
arnis::ProcessedNode node(std::uint64_t id, int x, int z, bool signal = false)
{
	arnis::ProcessedNode result{};
	result.id = id;
	result.x = x;
	result.z = z;
	if (signal)
		result.tags.emplace("highway", "traffic_signals");
	return result;
}

arnis::ProcessedElement road(std::uint64_t id, std::vector<arnis::ProcessedNode> nodes)
{
	arnis::ProcessedWay way{};
	way.id = id;
	way.nodes = std::move(nodes);
	way.tags.emplace("highway", "residential");
	return arnis::ProcessedElement(std::move(way));
}
} // namespace

class TestFmArnisTrafficLights : public TestBase
{
public:
	TestFmArnisTrafficLights() { TestManager::registerTestModule(this); }
	const char *getName() override { return "TestFmArnisTrafficLights"; }
	void runTests(IGameDef *) override
	{
		TEST(testSignalizedIntersectionPlacesRoadsidePoles);
	}

	void testSignalizedIntersectionPlacesRoadsidePoles()
	{
		using namespace arnis;
		const XZBBox bounds(-48, -48, 48, 48);
		RoadMaskBitmap roads(bounds);
		for (int t = -40; t <= 40; ++t)
			for (int w = -2; w <= 2; ++w) {
				roads.set(t, w);
				roads.set(w, t);
			}

		const std::vector<ProcessedElement> elements{
				road(1, {node(1, -40, 0), node(9, 0, 0, true), node(2, 40, 0)}),
				road(2, {node(3, 0, -40), node(9, 0, 0, true), node(4, 0, 40)})};
		const auto signals = highways::SignalIndex::build(elements, 1.0, false, roads);
		WorldEditor editor;
		UASSERT(signals->place_node(
				editor, elements.front().as_way().nodes[1], [](int, int) { return 0; }));

		std::size_t poles = 0;
		for (int x = bounds.min_x(); x <= bounds.max_x(); ++x)
			for (int z = bounds.min_z(); z <= bounds.max_z(); ++z) {
				const auto block = editor.get_block_absolute(x, 1, z);
				poles += block && block->id() == block_definitions::COBBLESTONE_WALL.id();
			}
		UASSERT(poles >= 2);
	}
};

static TestFmArnisTrafficLights g_test_fm_arnis_traffic_lights;
