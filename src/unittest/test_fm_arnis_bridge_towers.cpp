#include "test.h"

#include "mapgen/earth/arnis-cpp/src/block_definitions.h"
#include "mapgen/earth/arnis-cpp/src/element_processing/bridge_towers.h"
#include "mapgen/earth/arnis-cpp/src/element_processing/bridges.h"
#include "mapgen/earth/arnis-cpp/src/element_processing/highways.h"
#include "mapgen/earth/arnis_world_editor.h"

#include <cmath>
#include <cstdint>
#include <string>
#include <vector>

namespace
{
arnis::bridge_towers::CableSpan goldenGateSpan()
{
	// Golden Gate proportions in blocks: 1280-block main span, saddles at 150,
	// and a level deck at zero. The complete anchor-to-anchor span is 1966.
	auto span = arnis::bridge_towers::make_cable_span(
			0.0f, 0.0f, 1.0f, 0.0f, {{343.0f, 150}, {1623.0f, 150}}, {0.0f, 1966.0f},
			[](float) { return 0; }, arnis::block_definitions::ORANGE_TERRACOTTA, 1.0);
	UASSERT(span.has_value());
	return std::move(*span);
}

arnis::ProcessedWay towerWithHeightAt(int x, const std::string &height, std::uint64_t id)
{
	arnis::ProcessedWay tower{};
	tower.id = id;
	tower.tags.emplace("building:part", "yes");
	tower.tags.emplace("bridge:support", "pylon");
	tower.tags.emplace("height", height);
	for (const auto &[dx, z] :
			{std::pair<int, int>{0, 35}, {2, 35}, {2, 45}, {0, 45}, {0, 35}}) {
		arnis::ProcessedNode node{};
		node.id = id * 100 + tower.nodes.size();
		node.x = x + dx;
		node.z = z;
		tower.nodes.push_back(node);
	}
	return tower;
}

arnis::bridge_towers::MappedSupports collectTower(const std::string &height)
{
	const std::vector<arnis::ProcessedElement> elements{
			arnis::ProcessedElement(towerWithHeightAt(10, height, 1))};
	arnis::Ground ground = arnis::Ground::new_flat(0);
	arnis::WorldEditor editor;
	editor.ground = &ground;
	return arnis::bridge_towers::collect(elements, editor, 1.0);
}

} // namespace

class TestFmArnisBridgeTowers : public TestBase
{
public:
	TestFmArnisBridgeTowers() { TestManager::registerTestModule(this); }
	const char *getName() override { return "TestFmArnisBridgeTowers"; }
	void runTests(IGameDef *) override
	{
		TEST(testMainCableHangsToDeckAtMidspan);
		TEST(testSideSpansSagFromSaddlesToAnchors);
		TEST(testOnlySuspendedStretchHangs);
		TEST(testCoincidentTowersMakeNoSpan);
		TEST(testInvalidBuildingHeightsAreDropped);
		TEST(testGirdersAndHangersUseRealProportions);
		TEST(testMappedTowersCreateGoldenGateCableSpan);
	}

	void testMainCableHangsToDeckAtMidspan()
	{
		const auto span = goldenGateSpan();
		UASSERT(span.cable_y(983.0f).has_value());
		UASSERT(std::abs(*span.cable_y(983.0f) - 3.0f) < 0.5f);
		UASSERT(std::abs(*span.cable_y(343.0f) - 150.0f) < 0.001f);
		UASSERT(std::abs(*span.cable_y(1623.0f) - 150.0f) < 0.001f);
	}

	void testSideSpansSagFromSaddlesToAnchors()
	{
		const auto span = goldenGateSpan();
		UASSERT(std::abs(*span.cable_y(0.0f) - 1.0f) < 0.001f);
		const float chord = 1.0f + (150.0f - 1.0f) * 0.5f;
		const float side = *span.cable_y(171.5f);
		UASSERT(side < chord);
		UASSERT(side > chord - 15.0f);
		UASSERT(!span.cable_y(-1.0f).has_value());
		UASSERT(!span.cable_y(1967.0f).has_value());
	}

	void testOnlySuspendedStretchHangs()
	{
		const auto span = goldenGateSpan();
		UASSERT(span.suspends(10, 0));
		UASSERT(!span.suspends(-10, 0));
		UASSERT(!span.suspends(2000, 0));
	}

	void testCoincidentTowersMakeNoSpan()
	{
		const auto span = arnis::bridge_towers::make_cable_span(
				0.0f, 0.0f, 1.0f, 0.0f, {{343.0f, 150}, {343.2f, 150}}, {0.0f, 1966.0f},
				[](float) { return 0; }, arnis::block_definitions::ORANGE_TERRACOTTA,
				1.0);
		UASSERT(!span.has_value());
	}

	void testInvalidBuildingHeightsAreDropped()
	{
		const auto tall = collectTower("227");
		UASSERT(tall.towers.size() == 1);
		UASSERT(tall.towers.front().top_y == 227);
		const auto fractional = collectTower("67.5 m");
		UASSERT(fractional.towers.size() == 1);
		UASSERT(fractional.towers.front().top_y == 68);
		for (const char *invalid : {"inf", "1e10", "NaN", "-5", "1001"}) {
			const auto supports = collectTower(invalid);
			UASSERT(supports.towers.empty());
			UASSERT(supports.minor.size() == 1);
		}
	}

	void testGirdersAndHangersUseRealProportions()
	{
		const auto span = goldenGateSpan();
		UASSERT(span.girder_depth == 8);
		UASSERT(span.hanger_bay(15.0f) == 0);
		UASSERT(span.hanger_bay(16.0f) == 1);
	}

	void testMappedTowersCreateGoldenGateCableSpan()
	{
		arnis::ProcessedWay bridge{};
		bridge.id = 1;
		bridge.tags.emplace("highway", "primary");
		bridge.tags.emplace("bridge", "suspension");
		bridge.tags.emplace("bridge:name", "Golden Gate Bridge");
		bridge.tags.emplace("layer", "1");
		for (const auto &[id, x] : {std::pair<std::uint64_t, int>{101, 0}, {102, 200}}) {
			arnis::ProcessedNode node{};
			node.id = id;
			node.x = x;
			node.z = 40;
			bridge.nodes.push_back(node);
		}
		const std::vector<arnis::ProcessedElement> elements{
				arnis::ProcessedElement(bridge),
				arnis::ProcessedElement(towerWithHeightAt(40, "65", 2)),
				arnis::ProcessedElement(towerWithHeightAt(160, "65", 3))};
		arnis::Ground ground = arnis::Ground::new_flat(0);
		arnis::WorldEditor editor;
		editor.ground = &ground;
		const auto connectivity =
				arnis::highways::build_highway_connectivity_map(elements);
		const auto outlines = arnis::bridge_styles::BridgeOutlineIndex::build(elements);
		const auto structures = arnis::bridges::BridgeStructureMap::build(
				elements, editor, connectivity, outlines, 1.0);
		const auto *member = structures.lookup_member(1);
		UASSERT(member != nullptr);
		UASSERT(member->style == arnis::bridge_styles::BridgeStyle::Suspension);
		UASSERT(member->cable_span != nullptr);
		UASSERT(member->cable_pylons.has_value());
		UASSERT(member->cable_pylons->size() == 2);
		UASSERT(member->cable_span->cable_y(100.0f).has_value());
		UASSERT(*member->cable_span->cable_y(100.0f) > 6.0f);
	}
};

static TestFmArnisBridgeTowers g_test_fm_arnis_bridge_towers;
