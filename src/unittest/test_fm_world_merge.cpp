// SPDX-License-Identifier: GPL-3.0-or-later

#include "test.h"

#include <array>

#include "fm_world_merge.h"

class TestFmWorldMerge : public TestBase
{
public:
	TestFmWorldMerge() { TestManager::registerTestModule(this); }
	const char *getName() override { return "TestFmWorldMerge"; }

	void runTests(IGameDef *gamedef) override;

	void testEmptyCell();
	void testSparsePreferredSolidBecomesAir();
	void testSurfaceTieStaysSolid();
	void testMaterialVoteAfterOccupancy();
	void testMainSampleBreaksMaterialTie();
	// fm: The preferred surface sample keeps a supported narrow feature.
	void testSupportedMainSurfaceFeatureWins();
	void testUndergroundMaterialVoteStaysUnbiased();
	void testExposedUpperLayerWins();
	// ===
};

static TestFmWorldMerge g_test_instance;

void TestFmWorldMerge::runTests(IGameDef *gamedef)
{
	(void)gamedef;
	TEST(testEmptyCell);
	TEST(testSparsePreferredSolidBecomesAir);
	TEST(testSurfaceTieStaysSolid);
	TEST(testMaterialVoteAfterOccupancy);
	TEST(testMainSampleBreaksMaterialTie);
	// fm: Regression coverage for narrow roads during far-world merging.
	TEST(testSupportedMainSurfaceFeatureWins);
	TEST(testUndergroundMaterialVoteStaysUnbiased);
	TEST(testExposedUpperLayerWins);
	// ===
}

void TestFmWorldMerge::testEmptyCell()
{
	std::array<MapNode, 8> samples;
	samples.fill(MapNode(CONTENT_IGNORE));
	UASSERT(!world_merge::selectFarNodeIndex(samples));
}

void TestFmWorldMerge::testSparsePreferredSolidBecomesAir()
{
	constexpr content_t stone = 42;
	std::array<MapNode, 8> samples;
	samples.fill(MapNode(CONTENT_AIR));
	samples[3] = MapNode(stone); // Grid-aligned preferred sample.
	samples[7] = MapNode(stone);

	const auto selected = world_merge::selectFarNodeIndex(samples);
	UASSERT(selected);
	UASSERTEQ(content_t, samples[*selected].getContent(), CONTENT_AIR);
}

void TestFmWorldMerge::testSurfaceTieStaysSolid()
{
	constexpr content_t stone = 42;
	std::array<MapNode, 8> samples;
	samples.fill(MapNode(CONTENT_AIR));
	for (size_t i = 0; i < 4; ++i)
		samples[i] = MapNode(stone);

	const auto selected = world_merge::selectFarNodeIndex(samples);
	UASSERT(selected);
	UASSERTEQ(content_t, samples[*selected].getContent(), stone);
}

void TestFmWorldMerge::testMaterialVoteAfterOccupancy()
{
	constexpr content_t stone = 42;
	constexpr content_t dirt = 43;
	std::array<MapNode, 8> samples;
	samples.fill(MapNode(CONTENT_AIR));
	samples[0] = MapNode(stone);
	samples[1] = MapNode(stone);
	samples[2] = MapNode(dirt);
	samples[3] = MapNode(dirt);
	samples[4] = MapNode(dirt);

	const auto selected = world_merge::selectFarNodeIndex(samples);
	UASSERT(selected);
	UASSERTEQ(content_t, samples[*selected].getContent(), dirt);
}

void TestFmWorldMerge::testMainSampleBreaksMaterialTie()
{
	constexpr content_t stone = 42;
	constexpr content_t dirt = 43;
	std::array<MapNode, 8> samples;
	samples.fill(MapNode(CONTENT_AIR));
	samples[0] = MapNode(stone);
	samples[1] = MapNode(stone);
	samples[2] = MapNode(dirt);
	samples[3] = MapNode(dirt); // Main sample chooses between equal materials.

	const auto selected = world_merge::selectFarNodeIndex(samples);
	UASSERT(selected);
	UASSERTEQ(content_t, samples[*selected].getContent(), dirt);
}

// fm: A road in the preferred grid sample plus one supporting sample should
// survive three surrounding base-terrain samples after the occupancy vote.
void TestFmWorldMerge::testSupportedMainSurfaceFeatureWins()
{
	constexpr content_t road = 42;
	constexpr content_t grass = 43;
	std::array<MapNode, 8> samples;
	samples.fill(MapNode(CONTENT_AIR));
	samples[1] = MapNode(grass);
	samples[2] = MapNode(grass);
	samples[3] = MapNode(road); // Preferred grid-aligned surface sample.
	samples[4] = MapNode(road); // Supporting uphill road sample.
	samples[6] = MapNode(grass);

	const auto selected = world_merge::selectFarNodeIndex(samples);
	UASSERT(selected);
	UASSERTEQ(content_t, samples[*selected].getContent(), road);
}

void TestFmWorldMerge::testUndergroundMaterialVoteStaysUnbiased()
{
	constexpr content_t road = 42;
	constexpr content_t stone = 43;
	constexpr content_t dirt = 44;
	std::array<MapNode, 8> samples;
	samples[0] = MapNode(stone);
	samples[1] = MapNode(stone);
	samples[2] = MapNode(stone);
	samples[3] = MapNode(road);
	samples[4] = MapNode(road);
	samples[5] = MapNode(stone);
	samples[6] = MapNode(dirt);
	samples[7] = MapNode(dirt);

	const auto selected = world_merge::selectFarNodeIndex(samples);
	UASSERT(selected);
	UASSERTEQ(content_t, samples[*selected].getContent(), stone);
}

void TestFmWorldMerge::testExposedUpperLayerWins()
{
	constexpr content_t grass = 42;
	constexpr content_t dirt = 43;
	std::array<MapNode, 8> samples;
	samples.fill(MapNode(dirt));
	std::array<bool, 8> exposed{};
	for (const size_t index : {size_t{1}, size_t{2}, size_t{3}, size_t{6}})
		exposed[index] = true;
	samples[0] = MapNode(grass);
	exposed[0] = true;

	const auto selected = world_merge::selectFarNodeIndex(samples, &exposed);
	UASSERT(selected);
	UASSERTEQ(content_t, samples[*selected].getContent(), grass);
}
// ===
