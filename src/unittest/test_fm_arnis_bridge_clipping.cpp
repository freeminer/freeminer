#include "test.h"

#include "mapgen/earth/arnis-cpp/src/element_processing/bridges.h"
#include "mapgen/earth/arnis_world_editor.h"

#include <cstdint>
#include <vector>

namespace
{
arnis::ProcessedWay make_way(
		std::uint64_t id, const std::vector<std::pair<int, int>> &points, bool bridge)
{
	arnis::ProcessedWay result{};
	result.id = id;
	result.tags.emplace("highway", "residential");
	if (bridge)
		result.tags.emplace("bridge", "yes");
	for (std::size_t i = 0; i < points.size(); ++i) {
		arnis::ProcessedNode node{};
		node.id = id * 100 + i;
		node.x = points[i].first;
		node.z = points[i].second;
		result.nodes.push_back(node);
	}
	return result;
}
} // namespace

class TestFmArnisBridgeClipping : public TestBase
{
public:
	TestFmArnisBridgeClipping() { TestManager::registerTestModule(this); }
	const char *getName() override { return "TestFmArnisBridgeClipping"; }
	void runTests(IGameDef *) override { TEST(testClippedBridgeEndRemainsCarried); }

	void testClippedBridgeEndRemainsCarried()
	{
		auto bridge = make_way(1, {{10, 40}, {34, 40}}, true);
		// One World clipping creates synthetic endpoint IDs. That end must not be
		// mistaken for a real terrain landing when the bridge profile is anchored.
		bridge.nodes.back().id = std::uint64_t{1} << 62;
		auto crossing = make_way(2, {{22, 10}, {22, 70}}, false);
		std::vector<arnis::ProcessedElement> elements{
				arnis::ProcessedElement(bridge), arnis::ProcessedElement(crossing)};
		arnis::Ground ground = arnis::Ground::new_flat(0);
		arnis::WorldEditor editor;
		editor.ground = &ground;

		const auto structures =
				arnis::bridges::BridgeStructureMap::build(elements, editor);
		const auto *member = structures.lookup_member(bridge.id);
		UASSERT(member != nullptr);
		UASSERT(!member->ys.empty());
		UASSERT(member->ys.front() == 0);
		UASSERT(member->ys.back() >= 6);
	}
};
