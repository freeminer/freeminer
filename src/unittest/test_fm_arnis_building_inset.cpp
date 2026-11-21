#include "test.h"

#include "mapgen/earth/arnis-cpp/src/element_processing/building_inset.h"
#include "mapgen/earth/arnis-cpp/src/floodfill.h"

#include <utility>
#include <vector>

namespace
{
std::vector<arnis::ProcessedNode> rectangle(int x0, int z0, int x1, int z1)
{
	const std::pair<int, int> points[]{{x0, z0}, {x1, z0}, {x1, z1}, {x0, z1}, {x0, z0}};
	std::vector<arnis::ProcessedNode> nodes;
	for (const auto &[x, z] : points) {
		arnis::ProcessedNode n{};
		n.x = x;
		n.z = z;
		nodes.push_back(std::move(n));
	}
	return nodes;
}

std::vector<std::pair<int, int>> fill(const std::vector<arnis::ProcessedNode> &nodes)
{
	std::vector<std::pair<int, int>> ring;
	for (const auto &node : nodes)
		ring.emplace_back(node.x, node.z);
	return arnis::floodfill::flood_fill_area(ring, nullptr);
}
} // namespace

class TestFmArnisBuildingInset : public TestBase
{
public:
	TestFmArnisBuildingInset() { TestManager::registerTestModule(this); }
	const char *getName() override { return "TestFmArnisBuildingInset"; }
	void runTests(IGameDef *) override
	{
		TEST(testFreeStandingWallsStepInUnderEaves);
		TEST(testPartyWallStaysOnTheSharedLine);
	}

	void testFreeStandingWallsStepInUnderEaves()
	{
		const auto nodes = rectangle(10, 10, 24, 22);
		const auto cells = fill(nodes);
		XZBBox bounds(0, 0, 40, 40);
		arnis::CoordinateBitmap footprints(bounds);
		for (const auto &[x, z] : cells)
			footprints.set(x, z);
		const auto inset = arnis::building_inset::inset_outline(nodes, cells, footprints);
		UASSERT(inset.has_value());
		UASSERT(inset->nodes.size() == 5);
		UASSERT(inset->nodes.front().x == 11 && inset->nodes.front().z == 11);
		UASSERT(inset->nodes[1].x == 23 && inset->nodes[1].z == 11);
		UASSERT(inset->nodes[2].x == 23 && inset->nodes[2].z == 21);
		UASSERT(inset->fill.size() == 99);
		UASSERT(!inset->strip.empty());
		UASSERT(inset->strip.contains({12, 10}));
	}

	void testPartyWallStaysOnTheSharedLine()
	{
		const auto nodes = rectangle(10, 10, 22, 22);
		const auto cells = fill(nodes);
		XZBBox bounds(0, 0, 50, 40);
		arnis::CoordinateBitmap footprints(bounds);
		// Match Rust's bitmap_with_rect fixtures: footprints include outline
		// cells as well as the flood-filled interior.
		for (int x = 10; x <= 22; ++x)
			for (int z = 10; z <= 22; ++z)
				footprints.set(x, z);
		for (int x = 22; x <= 32; ++x)
			for (int z = 10; z <= 22; ++z)
				footprints.set(x, z);
		const auto inset = arnis::building_inset::inset_outline(nodes, cells, footprints);
		UTEST(inset.has_value(), "shared party wall produced no inset");
		UTEST(inset->nodes.size() == 5, "expected 5 inset vertices, got %zu",
				inset->nodes.size());
		const std::pair<int, int> expected[]{
				{11, 11}, {22, 11}, {22, 21}, {11, 21}, {11, 11}};
		for (std::size_t i = 0; i < std::size(expected); ++i)
			UTEST(inset->nodes[i].x == expected[i].first &&
							inset->nodes[i].z == expected[i].second,
					"party wall inset vertex %zu is (%d,%d), expected (%d,%d)", i,
					inset->nodes[i].x, inset->nodes[i].z, expected[i].first,
					expected[i].second);
	}
};

static TestFmArnisBuildingInset g_test_fm_arnis_building_inset;
