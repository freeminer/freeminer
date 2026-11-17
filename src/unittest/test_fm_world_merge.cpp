// SPDX-License-Identifier: GPL-3.0-or-later

#include "test.h"

#include <array>

#include "fm_world_merge.h"
#include "server/fm_emerge_smart.h"
// fm: Exercise light thinning and actual light-only block persistence.
#include <sstream>
#include <thread>
#include <vector>
#include "database/database-dummy.h"
#include "emerge.h"
#include "gamedef.h"
#include "light.h"
#include "map.h"
#include "mock_server.h"
#include "serverenvironment.h"
// ===

class TestFmWorldMerge : public TestBase
{
public:
	TestFmWorldMerge() { TestManager::registerTestModule(this); }
	const char *getName() override { return "TestFmWorldMerge"; }

	void runTests(IGameDef *gamedef) override;

	void testEmptyCell();
	void testPlayerThrottle();
	void testEmergePlayerThrottle();
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
	void testIdleDrain(IGameDef *gamedef);
	void testLightingRepairFromDisk();
	void testFailedLightingFallback(IGameDef *gamedef);
	void testSynchronousLightingRefresh(IGameDef *gamedef);
	void testDeferredGroupCoalescing(IGameDef *gamedef);
	// ===
};

static TestFmWorldMerge g_test_instance;

void TestFmWorldMerge::runTests(IGameDef *gamedef)
{
	(void)gamedef;
	TEST(testEmptyCell);
	TEST(testPlayerThrottle);
	TEST(testEmergePlayerThrottle);
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
	TEST(testIdleDrain, gamedef);
	TEST(testLightingRepairFromDisk);
	TEST(testFailedLightingFallback, gamedef);
	TEST(testSynchronousLightingRefresh, gamedef);
	TEST(testDeferredGroupCoalescing, gamedef);
	// ===
}

void TestFmWorldMerge::testPlayerThrottle()
{
	UASSERTEQ(uint32_t, world_merge::playerThrottleMs(0, 1, 20), 0);
	UASSERTEQ(uint32_t, world_merge::playerThrottleMs(1, 1, 20), 0);
	UASSERTEQ(uint32_t, world_merge::playerThrottleMs(4, 1, 20), 60);
	UASSERTEQ(uint32_t, world_merge::playerThrottleMs(100, 0, 20), 1000);
	UASSERTEQ(uint32_t, world_merge::playerThrottleMs(UINT32_MAX, 0, UINT32_MAX), 1000);
	UASSERTEQ(uint32_t, world_merge::playerThrottleMs(100, 0, 0), 0);
}

void TestFmWorldMerge::testEmergePlayerThrottle()
{
	UASSERTEQ(uint32_t, emerge_smart::inFlightLimit(10, 0, 1, true), 10);
	UASSERTEQ(uint32_t, emerge_smart::inFlightLimit(10, 1, 1, true), 10);
	UASSERTEQ(uint32_t, emerge_smart::inFlightLimit(10, 2, 1, true), 5);
	UASSERTEQ(uint32_t, emerge_smart::inFlightLimit(10, 3, 1, true), 3);
	UASSERTEQ(uint32_t, emerge_smart::inFlightLimit(10, UINT32_MAX, 1, true), 1);
	UASSERTEQ(uint32_t, emerge_smart::inFlightLimit(10, UINT32_MAX, 1, false), 10);
	UASSERTEQ(uint32_t, emerge_smart::inFlightLimit(0, 0, 1, true), 1);
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

void TestFmWorldMerge::testIdleDrain(IGameDef *gamedef)
{
	Map map(gamedef);
	ServerMap::far_dbases_t far_dbases;
	for (auto &db : far_dbases)
		db = std::make_shared<Database_Dummy>();
	auto block = map.createBlankBlockNoInsert(v3bpos_t(0, 0, 0));
	block->fill(MapNode(t_CONTENT_STONE));
	block->setGenerated(true);
	block->setLightingComplete(0);
	UASSERT(ServerMap::saveBlock(block.get(), far_dbases[0].get(), 1));
	std::vector<v3bpos_t> lighting_requests;
	WorldMerger merger{
			.request_lighting_func{[&](const v3bpos_t &pos, bool incomplete) {
				if (incomplete)
					lighting_requests.push_back(pos);
				return incomplete;
			}},
			.require_lighting_complete{true},
			.ndef{gamedef->ndef()},
			.smap{&map},
			.far_dbases{far_dbases},
			.dbase{far_dbases[0]},
	};
	auto &queue = map.changed_blocks_for_merge;
	queue.emplace(v3bpos_t(0, 0, 0));
	merger.merge_server_diff(queue, 100);
	UASSERTEQ(size_t, queue.size(), 1);
	merger.merge_server_diff(queue, 1);
	UASSERTEQ(size_t, queue.size(), 1);
	UASSERT(merger.retry_deferred);
	UASSERTEQ(size_t, lighting_requests.size(), 1);
	UASSERT(lighting_requests.front() == v3bpos_t(0, 0, 0));
	std::string far_before;
	far_dbases[1]->loadBlock(v3bpos_t(0, 0, 0), &far_before);
	UASSERT(far_before.empty());
	queue.reserve(1024);
	block->setLightingComplete(0xffff);
	UASSERT(ServerMap::saveBlock(block.get(), far_dbases[0].get(), 1));
	merger.merge_server_diff(queue, 100);
	UASSERT(queue.empty());
	std::string far_block;
	far_dbases[1]->loadBlock(v3bpos_t(0, 0, 0), &far_block);
	UASSERT(!far_block.empty());
	const auto lock = queue.lock_unique_rec();
	const auto &storage =
			static_cast<const concurrent_unordered_set<v3bpos_t>::full_type &>(queue);
	UASSERT(storage.bucket_count() < 1024);
}

void TestFmWorldMerge::testLightingRepairFromDisk()
{
	MockServer server(getTestTempDirectory());
	MetricsBackend metrics;
	EmergeManager emerge(&server, &metrics);
	auto map_ptr = std::make_unique<ServerMap>(
			server.getWorldPath(), &server, &emerge, &metrics);
	ServerEnvironment env(std::move(map_ptr), &server, &metrics);
	auto &map = env.getServerMap();
	const v3bpos_t pos(2, 3, 4);
	auto block = map.createBlock(pos);
	block->fill(MapNode(CONTENT_AIR));
	block->setGenerated(true);
	block->setLightingComplete(0);
	UASSERT(map.saveBlock(block.get()));
	block.reset();
	map.m_blocks.erase(pos);
	map.getBlockCacheFlush();
	UASSERT(!map.getBlock(pos));
	UASSERT(map.lighting_merge_add(pos));
	UASSERT(map.lighting_merge_add(pos)); // Duplicate requests are coalesced.
	int attempts = 0;
	UASSERTEQ(unsigned int, map.updateLightingQueue(1000, attempts), 1);
	UASSERTEQ(int, attempts, 1);
	auto repaired = loadBlockNoStore(&map, map.m_db.dbase, pos);
	UASSERT(repaired);
	UASSERTEQ(u16, repaired->getLightingComplete(), 0xffff);
	UASSERT(map.changed_blocks_for_merge.contains(pos));

	// A different thread holds the block lock, forcing the real repair path to
	// fail without modifying the block. The third failure permits a fallback.
	auto live = map.getBlock(pos);
	UASSERT(live);
	live->setLightingComplete(0);
	std::promise<void> locked;
	std::promise<void> release;
	auto release_signal = release.get_future();
	std::thread holder([&] {
		const auto lock = live->lock_unique_rec();
		locked.set_value();
		release_signal.wait();
	});
	locked.get_future().wait();
	const bool queued = map.lighting_merge_add(pos);
	bool waits[3]{};
	int retry_attempts = 0;
	for (auto &wait : waits) {
		map.updateLightingMergeQueue(1000, retry_attempts);
		wait = map.lighting_merge_add(pos);
	}
	release.set_value();
	holder.join();
	UASSERT(queued);
	UASSERTEQ(int, retry_attempts, 3);
	UASSERT(waits[0] && waits[1] && !waits[2]);
	map.lighting_merge_done(pos);
	UASSERT(map.lighting_merge_add(pos));
	int repaired_attempts = 0;
	UASSERTEQ(unsigned int, map.updateLightingMergeQueue(1000, repaired_attempts), 1);
}

void TestFmWorldMerge::testFailedLightingFallback(IGameDef *gamedef)
{
	Map map(gamedef);
	Database_Dummy source, target;
	ServerMap::far_dbases_t far_dbases;
	auto block = map.createBlankBlockNoInsert(v3bpos_t(0, 0, 0));
	block->fill(MapNode(t_CONTENT_STONE));
	block->setGenerated(true);
	block->setLightingComplete(0);
	UASSERT(ServerMap::saveBlock(block.get(), &source, 1));
	bool fallback = false;
	unsigned finished = 0;
	WorldMerger merger{
			.request_lighting_func{[&](const v3bpos_t &, bool incomplete) {
				return incomplete && !fallback;
			}},
			.finish_lighting_func{[&](const v3bpos_t &pos) {
				if (pos == v3bpos_t(0, 0, 0))
					++finished;
			}},
			.require_lighting_complete{true},
			.ndef{gamedef->ndef()},
			.smap{&map},
			.far_dbases{far_dbases},
	};
	UASSERT(merger.merge_one_block(&source, &target, v3bpos_t(0, 0, 0), 0).deferred);
	fallback = true;
	UASSERT(!merger.merge_one_block(&source, &target, v3bpos_t(0, 0, 0), 0).deferred);
	std::string blob;
	target.loadBlock(v3bpos_t(0, 0, 0), &blob);
	UASSERT(!blob.empty());
	UASSERTEQ(unsigned, finished, 1);
}

void TestFmWorldMerge::testSynchronousLightingRefresh(IGameDef *gamedef)
{
	Map map(gamedef);
	Database_Dummy source, target;
	ServerMap::far_dbases_t far_dbases;
	auto block = map.createBlankBlockNoInsert(v3bpos_t(0, 0, 0));
	block->fill(MapNode(t_CONTENT_STONE));
	block->setGenerated(true);
	block->setLightingComplete(0);
	UASSERT(ServerMap::saveBlock(block.get(), &source, 1));
	WorldMerger merger{
			.request_lighting_func{[&](const v3bpos_t &, bool incomplete) {
				if (incomplete) {
					block->fill(MapNode(t_CONTENT_BRICK));
					block->setLightingComplete(0xffff);
					UASSERT(ServerMap::saveBlock(block.get(), &source, 1));
				}
				return false;
			}},
			.require_lighting_complete{true},
			.ndef{gamedef->ndef()},
			.smap{&map},
			.far_dbases{far_dbases},
	};
	UASSERT(!merger.merge_one_block(&source, &target, v3bpos_t(0, 0, 0), 0).deferred);
	std::string blob;
	target.loadBlock(v3bpos_t(0, 0, 0), &blob);
	UASSERT(!blob.empty());
	std::istringstream stream(blob, std::ios::binary);
	const auto version = static_cast<u8>(stream.get());
	auto merged = map.createBlankBlockNoInsert(v3bpos_t(0, 0, 0));
	UASSERT(merged->deSerialize(stream, version, true));
	UASSERT(merged->getNodeNoLock(v3pos_t(0, 0, 0)).getContent() == t_CONTENT_BRICK);
}

void TestFmWorldMerge::testDeferredGroupCoalescing(IGameDef *gamedef)
{
	Map map(gamedef);
	ServerMap::far_dbases_t far_dbases;
	for (auto &db : far_dbases)
		db = std::make_shared<Database_Dummy>();
	WorldMerger merger{
			.request_lighting_func{
					[](const v3bpos_t &, bool incomplete) { return incomplete; }},
			.require_lighting_complete{true},
			.ndef{gamedef->ndef()},
			.smap{&map},
			.far_dbases{far_dbases},
			.dbase{far_dbases[0]},
	};
	for (bpos_t x = 0; x < 2; ++x)
		for (bpos_t y = 0; y < 2; ++y)
			for (bpos_t z = 0; z < 2; ++z) {
				const v3bpos_t pos(x, y, z);
				auto block = map.createBlankBlockNoInsert(pos);
				block->fill(MapNode(t_CONTENT_STONE));
				block->setGenerated(true);
				block->setLightingComplete(0);
				UASSERT(ServerMap::saveBlock(block.get(), far_dbases[0].get(), 1));
				map.changed_blocks_for_merge.emplace(pos);
			}
	merger.merge_server_diff(map.changed_blocks_for_merge, 1);
	UASSERTEQ(size_t, map.changed_blocks_for_merge.size(), 1);
	UASSERT(map.changed_blocks_for_merge.contains(v3bpos_t(0, 0, 0)));
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
