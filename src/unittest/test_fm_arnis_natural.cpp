#include "test.h"

#include "mapgen/earth/arnis-cpp/src/block_definitions.h"
#include "mapgen/earth/arnis-cpp/src/element_processing/bridges.h"
#include "mapgen/earth/arnis-cpp/src/floodfill_cache.h"
#include "mapgen/earth/arnis_world_editor.h"

#include <optional>
#include <utility>
#include <vector>

namespace arnis::natural
{
void generate_natural(WorldEditor &, const ProcessedElement &, const Args &,
		const FloodFillCache &, const BuildingFootprintBitmap &,
		const bridges::BridgeSurfaceMap &);
}

namespace
{
bool has_block(arnis::WorldEditor &editor, int x, int y, int z, const arnis::Block &block)
{
	const auto actual = editor.get_block_absolute(x, y, z);
	return actual && actual->id() == block.id();
}

arnis::ProcessedWay natural_tree_area()
{
	arnis::ProcessedWay way{};
	way.id = 991;
	way.tags.emplace("natural", "tree");
	const std::vector<std::pair<int, int>> coordinates{
			{10, 10}, {20, 10}, {20, 20}, {10, 20}, {10, 10}};
	for (const auto &[x, z] : coordinates) {
		arnis::ProcessedNode node{};
		node.id = way.id * 10 + way.nodes.size();
		node.x = x;
		node.z = z;
		way.nodes.push_back(node);
	}
	return way;
}
} // namespace

class TestFmArnisNatural : public TestBase
{
public:
	TestFmArnisNatural() { TestManager::registerTestModule(this); }
	const char *getName() override { return "TestFmArnisNatural"; }
	void runTests(IGameDef *) override { TEST(testNaturalTreeWaySkipsNaturalDecoration); }

	void testNaturalTreeWaySkipsNaturalDecoration()
	{
		using namespace arnis;
		WorldEditor editor;
		const auto way = natural_tree_area();
		const std::vector<ProcessedElement> elements{ProcessedElement(way)};
		const auto fills = FloodFillCache::precompute(elements, std::nullopt);
		const BuildingFootprintBitmap footprints(XZBBox(0, 0, 31, 31));
		const bridges::BridgeSurfaceMap bridge_surface;

		natural::generate_natural(
				editor, elements.front(), Args{}, fills, footprints, bridge_surface);

		// Rust's earlier ground-fill pass owns the base surface for tree-tagged
		// ways; the natural-area renderer itself must consume and skip this way.
		UASSERT(!has_block(editor, 15, 0, 15, GRASS_BLOCK));
	}
};
