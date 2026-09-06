// SPDX-License-Identifier: GPL-3.0-or-later

#include "test.h"

#include <array>

#include "fm_world_merge.h"
// fm: Exercise light thinning and actual light-only block persistence.
#include <sstream>
#include <vector>
#include "database/database-dummy.h"
#include "gamedef.h"
#include "light.h"
#include "map.h"
// ===

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
	void testSparseLightsAcrossLevels();
	void testDenseLightRegions();
	void testSparseRoadBesideDenseLights();
	void testLightSelectionOrder();
	void testBrighterLightSurvives();
	void testLightOnlyBlockPersistence(IGameDef *gamedef);
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
	TEST(testSparseLightsAcrossLevels);
	TEST(testDenseLightRegions);
	TEST(testSparseRoadBesideDenseLights);
	TEST(testLightSelectionOrder);
	TEST(testBrighterLightSurvives);
	TEST(testLightOnlyBlockPersistence, gamedef);
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

// fm: Keep sparse lights at every LOD, but cap dense spatial regions.
void TestFmWorldMerge::testSparseLightsAcrossLevels()
{
	MapBlock::light_points_t lights;
	for (pos_t i = 0; i < 8; ++i)
		lights.emplace(v3pos_t(-113 + i * 16, -5, -7),
				MapBlock::makeLightPoint(i + 1, video::SColor(255, 255, i * 30, 42)));
	const auto original = lights;
	for (block_step_t step = 1; step < FARMESH_STEP_MAX; ++step) {
		lights = world_merge::reduceFarLightPoints(lights, step);
		UASSERT(lights == original);
	}
	lights.clear();
	UASSERT(world_merge::reduceFarLightPoints(lights, 1).empty());
	lights.emplace(v3pos_t(0, 0, 0),
			MapBlock::makeLightPoint(0, video::SColor(255, 255, 255, 255)));
	UASSERT(world_merge::reduceFarLightPoints(lights, 1).empty());
}

void TestFmWorldMerge::testDenseLightRegions()
{
	for (const block_step_t step : {1, 2, 3}) {
		const pos_t width = MAP_BLOCKSIZE * (1 << step);
		const pos_t region_width = width / 4;
		MapBlock::light_points_t lights;
		for (pos_t x = -width; x < 0; ++x)
			for (pos_t z = -width; z < 0; ++z)
				lights.emplace(
						v3pos_t(x, -1, z), MapBlock::makeLightPoint(LIGHT_MAX,
												   video::SColor(255, 20, 180, 255)));
		const auto reduced = world_merge::reduceFarLightPoints(lights, step);
		UASSERT(reduced.size() <= lights.size() / 4);
		std::array<size_t, 16> per_region{};
		for (const auto &[pos, light] : reduced) {
			UASSERT(lights.at(pos) == light); // No amplified or recolored proxy lights.
			++per_region[(pos.X + width) / region_width * 4 +
						 (pos.Z + width) / region_width];
		}
		for (const auto count : per_region)
			UASSERTEQ(size_t, count, 8);
		UASSERT(world_merge::reduceFarLightPoints(reduced, step) == reduced);
	}
}

void TestFmWorldMerge::testSparseRoadBesideDenseLights()
{
	MapBlock::light_points_t road;
	for (pos_t x = 0; x < 128; x += 8)
		road.emplace(v3pos_t(x, 0, 96),
				MapBlock::makeLightPoint(8, video::SColor(255, 255, 160, 40)));
	const auto original = world_merge::reduceFarLightPoints(road, 3);
	UASSERT(original == road);
	auto mixed = road;
	for (pos_t x = 0; x < 128; ++x)
		for (pos_t z = 0; z < 32; ++z)
			mixed.emplace(v3pos_t(x, 0, z), MapBlock::makeLightPoint(LIGHT_MAX,
													video::SColor(255, 255, 255, 255)));
	const auto reduced = world_merge::reduceFarLightPoints(mixed, 3);
	for (const auto &[pos, light] : road)
		UASSERT(reduced.at(pos) == light);
	UASSERT(reduced.size() < mixed.size() / 4);
}

void TestFmWorldMerge::testLightSelectionOrder()
{
	MapBlock::light_points_t forward;
	std::vector<std::pair<v3pos_t, MapBlock::light_t>> entries;
	for (pos_t x = -8; x < 8; ++x)
		for (pos_t z = -8; z < 8; ++z) {
			const auto light = MapBlock::makeLightPoint(
					10 + (x + z + 16) % 5, video::SColor(255, 100, 255, 42));
			entries.emplace_back(v3pos_t(x, -1, z), light);
			forward.emplace(entries.back());
		}
	MapBlock::light_points_t reverse;
	reverse.reserve(4096);
	for (auto it = entries.rbegin(); it != entries.rend(); ++it)
		reverse.emplace(*it);
	for (const block_step_t step : {1, 2, 3, 4})
		UASSERT(world_merge::reduceFarLightPoints(forward, step) ==
				world_merge::reduceFarLightPoints(reverse, step));
}

void TestFmWorldMerge::testBrighterLightSurvives()
{
	MapBlock::light_points_t lights;
	for (pos_t x = 0; x < 8; ++x)
		for (pos_t z = 0; z < 8; ++z)
			lights.emplace(v3pos_t(x, 0, z),
					MapBlock::makeLightPoint(13, video::SColor(255, 255, 255, 255)));
	const auto original = world_merge::reduceFarLightPoints(lights, 1);
	UASSERTEQ(size_t, original.size(), 8);
	for (const auto &[pos, light] : lights) {
		auto brighter = lights;
		brighter.at(pos) =
				MapBlock::makeLightPoint(14, video::SColor(255, 255, 255, 255));
		const auto reduced = world_merge::reduceFarLightPoints(brighter, 1);
		// A retained light stays retained; a previously discarded light now has
		// higher priority than all the others and must also be selected.
		UASSERT(reduced.contains(pos));
		UASSERT(reduced.at(pos) == brighter.at(pos));
	}
}

void TestFmWorldMerge::testLightOnlyBlockPersistence(IGameDef *gamedef)
{
	Map map(gamedef);
	std::array<Database_Dummy, 5> db;
	ServerMap::far_dbases_t far_dbases;
	WorldMerger merger{
			.get_time_func{[]() { return 10; }},
			.farlights{1},
			.ndef{gamedef->ndef()},
			.smap{&map},
			.far_dbases{far_dbases},
	};
	// A single exposed torch among air loses the 2x2x2 terrain occupancy vote.
	// It must nevertheless become a stored far light and survive further merges.
	for (pos_t x = 0; x < 2; ++x)
		for (pos_t y = 0; y < 2; ++y)
			for (pos_t z = 0; z < 2; ++z) {
				auto block = map.createBlankBlockNoInsert(v3bpos_t(x, y, z));
				block->fill(MapNode(CONTENT_AIR));
				block->setGenerated(true);
				block->setTimestampNoChangedFlag(10);
				if (!x && !y && !z)
					block->setNodeNoLock(
							v3pos_t(2, 2, 2), MapNode(t_CONTENT_TORCH), true);
				UASSERT(ServerMap::saveBlock(block.get(), &db[0], 1));
			}
	MapBlock::light_points_t original;
	for (block_step_t step = 0; step + 1 < db.size(); ++step) {
		const auto result =
				merger.merge_one_block(&db[step], &db[step + 1], v3bpos_t(0, 0, 0), step);
		UASSERTEQ(size_t, result.lights_used, 1);
		std::string blob;
		db[step + 1].loadBlock(v3bpos_t(0, 0, 0), &blob);
		UASSERT(!blob.empty());
		std::istringstream serialized(blob, std::ios::binary);
		const auto version = static_cast<u8>(serialized.get());
		auto restored = map.createBlankBlockNoInsert(v3bpos_t(0, 0, 0));
		UASSERT(restored->deSerialize(serialized, version, true));
		UASSERT(restored->isGenerated());
		UASSERTEQ(size_t, restored->m_light_points->size(), 1);
		UASSERT(restored->m_light_points->contains(v3pos_t(2, 2, 2)));
		if (!step)
			original = *restored->m_light_points;
		else
			UASSERT(*restored->m_light_points == original);
		for (pos_t x = 0; x < MAP_BLOCKSIZE; ++x)
			for (pos_t y = 0; y < MAP_BLOCKSIZE; ++y)
				for (pos_t z = 0; z < MAP_BLOCKSIZE; ++z) {
					const auto content =
							restored->getNodeNoLock(v3pos_t(x, y, z)).getContent();
					UASSERT(content == CONTENT_AIR || content == CONTENT_IGNORE);
				}
	}
	// Removing the final lamp must still delete the obsolete light-only block.
	auto empty = map.createBlankBlockNoInsert(v3bpos_t(0, 0, 0));
	empty->fill(MapNode(CONTENT_AIR));
	empty->setGenerated(true);
	UASSERT(ServerMap::saveBlock(empty.get(), &db[0], 1));
	merger.merge_one_block(&db[0], &db[1], v3bpos_t(0, 0, 0), 0);
	std::string blob;
	db[1].loadBlock(v3bpos_t(0, 0, 0), &blob);
	UASSERT(blob.empty());
}
// ===
