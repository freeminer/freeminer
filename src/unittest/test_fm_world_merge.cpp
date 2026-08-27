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
