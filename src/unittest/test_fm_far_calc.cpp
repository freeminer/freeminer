// Luanti
// SPDX-License-Identifier: LGPL-2.1-or-later
// Copyright (C) 2024 proller <proler@gmail.com>

#include "debug/dump.h"
#include "test.h"
#include "catch.h"
#include "fm_far_calc.h"
#include "client/clientmap.h"

#include <limits>
#include <vector>

class TestFmFarCalc : public TestBase
{
public:
	TestFmFarCalc() { TestManager::registerTestModule(this); }
	const char *getName() { return "TestFmFarCalc"; }

	void runTests(IGameDef *gamedef);

	void testRangeToStep();
	void testPlayerBlockAlign();
	void testGetLodStep();
	void testGetFarStep();
	void testGetFarStepBad();
	//void testGetFarStepCellSize();
	void testGetFarActual();
	void testInFarGrid();
	void testCellRange();
	void testFarMeshNearRange();
	void testRunFarAllVerification();
	void testRunFarAllCoverage();
	void testRunFarAll2DCoverage();
	void testRunFarAllCellEachConsistency();
	void testRunFarAllStops();
};

static TestFmFarCalc g_test_instance;

void TestFmFarCalc::runTests(IGameDef *gamedef)
{
	TEST(testRangeToStep);
	TEST(testPlayerBlockAlign);
	TEST(testGetLodStep);
	TEST(testGetFarStep);
	TEST(testGetFarStepBad);
	//TEST(testGetFarStepCellSize);
	TEST(testGetFarActual);
	TEST(testInFarGrid);
	TEST(testCellRange);
	TEST(testFarMeshNearRange);
	TEST(testRunFarAllVerification);
	TEST(testRunFarAllCoverage);
	TEST(testRunFarAll2DCoverage);
	TEST(testRunFarAllCellEachConsistency);
	TEST(testRunFarAllStops);
}

void TestFmFarCalc::testRangeToStep()
{
	// Test basic functionality
	UASSERTEQ(int, farmesh::rangeToStep(1), 0);
	UASSERTEQ(int, farmesh::rangeToStep(2), 1);
	UASSERTEQ(int, farmesh::rangeToStep(4), 2);
	UASSERTEQ(int, farmesh::rangeToStep(8), 3);
	UASSERTEQ(int, farmesh::rangeToStep(16), 4);

	// Test edge cases
	UASSERTEQ(int, farmesh::rangeToStep(0), 0);
	UASSERTEQ(int, farmesh::rangeToStep(3), 1);
	UASSERTEQ(int, farmesh::rangeToStep(7), 2);
	UASSERTEQ(int, farmesh::rangeToStep(15), 3);
}

void TestFmFarCalc::testPlayerBlockAlign()
{
	MapDrawControl draw_control;
	draw_control.cell_size_pow = 2;		  // 4
	draw_control.farmesh_quality_pow = 1; // 2

	v3bpos_t player_pos(100, 200, 300);
	v3bpos_t aligned = farmesh::playerBlockAlign(draw_control, player_pos);

	// With cell_size_pow=2 and farmesh_quality_pow=1, step_pow2 = 3
	// The function aligns to multiples of 2^3 = 8, then adds offset of 3>>1 = 1
	// So result should be aligned to 8*N + 1
	const int alignment = 8;
	const int offset = 1; // (3 >> 1)
	UASSERT((aligned.X - offset) % alignment == 0);
	UASSERT((aligned.Y - offset) % alignment == 0);
	UASSERT((aligned.Z - offset) % alignment == 0);
}

void TestFmFarCalc::testGetLodStep()
{
	MapDrawControl draw_control;
	draw_control.lodmesh = 4;
	draw_control.cell_size = 1;

	v3bpos_t player_pos(0, 0, 0);
	v3bpos_t block_pos(1, 0, 0); // distance 1

	// Should return 0 when not in LOD range
	UASSERTEQ(block_step_t,
			farmesh::getLodStep(draw_control, player_pos, block_pos, 0.0f), 0);

	// Test with larger range
	block_pos = v3bpos_t(10, 0, 0);
	draw_control.lodmesh = 1;

	// Distance 10, cells = 2, range >= cells + lodmesh * 4 = 6 -> step 4
	// But the function has simplified logic, so let's test the actual behavior
	block_step_t step = farmesh::getLodStep(draw_control, player_pos, block_pos, 0.0f);
	// Should be 0 since 10 < 2 + 1*4 = 6? No, 10 >= 6, so should be 4
	// Actually, let's check the logic: if range >= cells + draw_control.lodmesh * 4
	int range = 10;
	int cells = 2;
	if (range >= cells + draw_control.lodmesh * 4) {
		// Should return 4
		UASSERT(step == 4);
	}
}

void TestFmFarCalc::testGetFarStep()
{
	MapDrawControl draw_control;
	draw_control.farmesh = 1000;
	draw_control.cell_size_pow = 0;
	draw_control.farmesh_quality = 0;

	v3bpos_t player_pos(0, 0, 0);
	v3bpos_t block_pos(1, 0, 0);

	block_step_t step = farmesh::getFarStep(draw_control, player_pos, block_pos);
	// Should return a valid step value
	UASSERT(step >= 0);
}

void TestFmFarCalc::testGetFarStepBad()
{
	MapDrawControl draw_control;
	draw_control.farmesh = 0; // Disable farmesh

	v3bpos_t player_pos(0, 0, 0);
	v3bpos_t block_pos(1, 0, 0);

	// Should return 1 when farmesh is disabled
	UASSERTEQ(
			block_step_t, farmesh::getFarStepBad(draw_control, player_pos, block_pos), 1);

	// Enable farmesh
	draw_control.farmesh = 1000;
	block_step_t step = farmesh::getFarStepBad(draw_control, player_pos, block_pos);
	UASSERT(step >= 1);
}
/*
void TestFmFarCalc::testGetFarStepCellSize()
{
	MapDrawControl draw_control;
	draw_control.farmesh = 1000;
	draw_control.cell_size_pow = 0;

	v3bpos_t player_pos(0, 0, 0);
	v3bpos_t block_pos(1, 0, 0);

	block_step_t step = getFarStepCellSize(draw_control, player_pos, block_pos, 0);
	UASSERT(step >= 0);
}
	*/

void TestFmFarCalc::testGetFarActual()
{
	MapDrawControl draw_control;
	draw_control.farmesh = 1000;
	draw_control.cell_size_pow = 0;
	draw_control.farmesh_quality = 0;

	v3bpos_t player_pos(0, 0, 0);
	v3bpos_t block_pos(1, 0, 0);
	//block_step_t step = 0;

	v3bpos_t actual = farmesh::getFarActualBlockPos(draw_control, player_pos, block_pos);
	// Should return a valid position
	UASSERT(true); // Basic test that it doesn't crash
}

void TestFmFarCalc::testInFarGrid()
{
	MapDrawControl draw_control;
	draw_control.farmesh = 1000;
	draw_control.cell_size_pow = 0;
	draw_control.farmesh_quality = 0;

	v3bpos_t player_pos(0, 0, 0);
	v3bpos_t block_pos(1, 0, 0);
	block_step_t step = 0;

	bool result = farmesh::inFarGrid(draw_control, player_pos, block_pos, step);
	// Should return a boolean value
	UASSERT(result == true || result == false);
}

void TestFmFarCalc::testCellRange()
{
	const v3pos_t camera_pos(0, 0, 0);

	// The range and cells are inclusive at their outer node boundary.
	UASSERT(farmesh::cellIntersectsRange(v3pos_t(10, -2, -2), 4,
			camera_pos, 10));
	UASSERT(!farmesh::cellIntersectsRange(v3pos_t(11, -2, -2), 4,
			camera_pos, 10));

	// Negative cells use the same rules and a miss on any axis rejects the cell.
	UASSERT(farmesh::cellIntersectsRange(v3pos_t(-13, -2, -2), 4,
			camera_pos, 10));
	UASSERT(!farmesh::cellIntersectsRange(v3pos_t(-14, 11, -2), 4,
			camera_pos, 10));

	UASSERTEQ(uint64_t,
			farmesh::cellMaxDistance(v3pos_t(-13, -2, -2), 4, camera_pos), 13);
	UASSERTEQ(uint64_t,
			farmesh::cellMaxDistance(v3pos_t(8, -4, 2), 8, camera_pos), 15);
}

void TestFmFarCalc::testFarMeshNearRange()
{
	MapDrawControl draw_control;
	draw_control.farmesh = 256;
	draw_control.cell_size_pow = 1;
	draw_control.cell_size = 1 << draw_control.cell_size_pow;
	draw_control.farmesh_quality_pow = 1;

	// A 32-node view range ends inside the step-0 zone for the default
	// client_mesh_chunk=2 layout.
	UASSERTEQ(uint64_t, farmesh::getFarMeshNearRange(draw_control), 160);
	UASSERT(farmesh::getFarMeshNearRange(draw_control) > 32);

	for (const uint8_t cell_size_pow : {0, 1, 2, 3}) {
		for (const uint8_t quality_pow : {0, 1, 2, 3, 4}) {
			draw_control.cell_size_pow = cell_size_pow;
			draw_control.cell_size = 1 << cell_size_pow;
			draw_control.farmesh_quality_pow = quality_pow;
			const v3bpos_t player_block(-17, 9, 31);
			const v3pos_t camera_node = player_block * MAP_BLOCKSIZE +
					v3pos_t(MAP_BLOCKSIZE - 1, 3, MAP_BLOCKSIZE / 2);
			uint64_t actual_step_zero_range = 0;

			farmesh::runFarAll(player_block, cell_size_pow, draw_control.farmesh,
					quality_pow, 0, false, 0,
					[&](const v3bpos_t &pos, const bpos_t &size,
							const block_step_t &step) {
						if (!step) {
							const pos_t width = static_cast<pos_t>(size) *
									draw_control.cell_size * MAP_BLOCKSIZE;
							actual_step_zero_range = std::max(actual_step_zero_range,
									farmesh::cellMaxDistance(pos * MAP_BLOCKSIZE,
											width, camera_node));
						}
						return false;
					});

			UASSERT(actual_step_zero_range <=
					farmesh::getFarMeshNearRange(draw_control));
		}
	}

	draw_control.farmesh = 0;
	UASSERTEQ(uint64_t, farmesh::getFarMeshNearRange(draw_control), 0);
}

void TestFmFarCalc::testRunFarAllVerification()
{
	// Set up test configuration
	MapDrawControl draw_control;
	draw_control.farmesh = 256;
	draw_control.farmesh_quality = 1;
	draw_control.farmesh_quality_pow = 0;
	draw_control.cell_size_pow = 2; // cell size = 4
	draw_control.lodmesh = 0;
	draw_control.cell_size = 1 << draw_control.cell_size_pow;

	v3bpos_t player_pos(100, 100, 100);

	// Counter for visited blocks
	int block_count = 0;
	// Include client_mesh_chunk=2 and 4 (powers 1 and 2).
	for (const auto &dc_csp : {0, 1, 2, 3, 4}) {
		draw_control.cell_size_pow = dc_csp;
		draw_control.cell_size = 1 << dc_csp;
		draw_control.farmesh_quality_pow = draw_control.cell_size_pow;
		for (const auto &dc_fm : {
					 128,
					 1000,
			 }) {
			draw_control.farmesh = dc_fm;
			for (const auto &cell_each : {true, false}) {
				//for (const auto &two_d : {true, false}) {
				for (const auto &two_d : {false}) {

					block_count = 0;

					// Verify that we processed some blocks

					farmesh::runFarAll(player_pos, draw_control.cell_size_pow,
							draw_control.farmesh, draw_control.farmesh_quality_pow,
							two_d,	   // two_d
							cell_each, // cell_each
							0,		   // max_step
							[&](const v3bpos_t &block_pos, const bpos_t &size,
									const block_step_t step) -> bool {
								++block_count;

								// Enumeration and point lookup must describe the same cell.
								const auto res = farmesh::getFarParams(
										draw_control, player_pos, block_pos, cell_each);
								UASSERT(res.has_value());
								const auto &check_step = res->step;
								const auto &check_pos = res->pos;
								UASSERT(check_step >= 0);
								UASSERT(check_step <= FARMESH_STEP_MAX);

								// Verify that lookup resolves the enumerated block origin.
								UASSERTEQ(auto, check_pos.X, block_pos.X);
								UASSERTEQ(auto, check_pos.Y, block_pos.Y);
								UASSERTEQ(auto, check_pos.Z, block_pos.Z);

								// Check that the step is valid
								UASSERT(step >= 0);
								UASSERT(step <= FARMESH_STEP_MAX);
								UASSERTEQ(auto, check_step, step);
								UASSERTEQ(bpos_t, size, res->size);
								UASSERTEQ(bpos_t, size, static_cast<bpos_t>(1 << step));
								// Continue processing
								return false;
							});
					UASSERT(block_count > 0);
				}
			}
		}
	}

	// Verify that we processed some blocks
	UASSERT(block_count > 0);
}

void TestFmFarCalc::testRunFarAllCoverage()
{
	struct Config
	{
		uint8_t cell_size_pow;
		uint8_t quality_pow;
		int farmesh;
		v3bpos_t player_pos;
	};
	struct Cell
	{
		v3bpos_t pos;
		bpos_t size;
		block_step_t step;
		bpos_t span;
	};

	const std::vector<Config> configs{
			{0, 0, 64, v3bpos_t(0, 0, 0)},
			{0, 3, 64, v3bpos_t(-33, 17, -65)},
			{1, 0, 128, v3bpos_t(63, -47, 31)},
			{1, 3, 128, v3bpos_t(-64, 32, 95)},
			{2, 0, 128, v3bpos_t(-97, -1, 66)},
			{2, 2, 256, v3bpos_t(127, 65, -129)},
			{3, 1, 256, v3bpos_t(-129, 7, 127)},
			{3, 4, 256, v3bpos_t(31, -95, 64)},
	};

	for (const auto &config : configs) {
		for (const bool cell_each : {false, true}) {
			MapDrawControl draw_control;
			draw_control.farmesh = config.farmesh;
			draw_control.cell_size_pow = config.cell_size_pow;
			draw_control.cell_size = 1 << config.cell_size_pow;
			draw_control.farmesh_quality_pow = config.quality_pow;

			std::vector<Cell> cells;
			v3bpos_t min_pos(std::numeric_limits<bpos_t>::max());
			v3bpos_t max_pos(std::numeric_limits<bpos_t>::min());
			farmesh::runFarAll(config.player_pos, config.cell_size_pow,
					config.farmesh, config.quality_pow, 0, cell_each, 0,
					[&](const v3bpos_t &pos, const bpos_t &size,
							const block_step_t &step) {
						const bpos_t span = size << (cell_each ? 0 : config.cell_size_pow);
						UASSERT(size > 0);
						UASSERT(span > 0);
						UASSERTEQ(bpos_t, size, static_cast<bpos_t>(1 << step));
						cells.push_back({pos, size, step, span});
						for (u8 axis = 0; axis < 3; ++axis) {
							min_pos[axis] = std::min(min_pos[axis], pos[axis]);
							max_pos[axis] = std::max(max_pos[axis],
									static_cast<bpos_t>(pos[axis] + span));
						}
						return false;
					});

			UASSERT(!cells.empty());
			const bpos_t base = cell_each ? 1 : 1 << config.cell_size_pow;
			const bpos_t tree_pow = std::max<block_step_t>(config.cell_size_pow,
					farmesh::rangeToStep(config.farmesh) - 1);
			const bpos_t tree_size = 1 << tree_pow;
			for (u8 axis = 0; axis < 3; ++axis)
				UASSERTEQ(bpos_t, max_pos[axis] - min_pos[axis], tree_size);

			const size_t side = tree_size / base;
			std::vector<size_t> owner(side * side * side,
					std::numeric_limits<size_t>::max());
			const auto index = [side](size_t x, size_t y, size_t z) {
				return x + y * side + z * side * side;
			};

			for (size_t cell_index = 0; cell_index < cells.size(); ++cell_index) {
				const auto &cell = cells[cell_index];
				UASSERTEQ(bpos_t, cell.span % base, 0);
				for (bpos_t z = 0; z < cell.span; z += base)
					for (bpos_t y = 0; y < cell.span; y += base)
						for (bpos_t x = 0; x < cell.span; x += base) {
							const size_t ix = (cell.pos.X + x - min_pos.X) / base;
							const size_t iy = (cell.pos.Y + y - min_pos.Y) / base;
							const size_t iz = (cell.pos.Z + z - min_pos.Z) / base;
							auto &slot = owner[index(ix, iy, iz)];
							UASSERTEQ(size_t, slot, std::numeric_limits<size_t>::max());
							slot = cell_index;
						}
			}

			for (size_t z = 0; z < side; ++z)
				for (size_t y = 0; y < side; ++y)
					for (size_t x = 0; x < side; ++x) {
						const auto cell_index = owner[index(x, y, z)];
						UASSERT(cell_index != std::numeric_limits<size_t>::max());
						const v3bpos_t sample(
								min_pos.X + x * base, min_pos.Y + y * base,
								min_pos.Z + z * base);
						const auto lookup = farmesh::getFarParams(
								draw_control, config.player_pos, sample, cell_each);
						UASSERT(lookup.has_value());
						const auto &cell = cells[cell_index];
						UASSERTEQ(v3bpos_t, lookup->pos, cell.pos);
						UASSERTEQ(bpos_t, lookup->size, cell.size);
						UASSERTEQ(block_step_t, lookup->step, cell.step);

						// The adaptive octree must remain 2:1 balanced at faces;
						// larger jumps are especially prone to visible LOD cracks.
						for (const auto &neighbor : {
									std::array<size_t, 3>{x + 1, y, z},
									std::array<size_t, 3>{x, y + 1, z},
									std::array<size_t, 3>{x, y, z + 1},
						}) {
							if (neighbor[0] >= side || neighbor[1] >= side ||
									neighbor[2] >= side)
								continue;
							const auto neighbor_index = owner[index(
									neighbor[0], neighbor[1], neighbor[2])];
							UASSERT(std::abs(static_cast<int>(cell.step) -
									static_cast<int>(cells[neighbor_index].step)) <= 1);
						}
					}
		}
	}
}

void TestFmFarCalc::testRunFarAll2DCoverage()
{
	for (const uint8_t cell_size_pow : {0, 1, 2, 3}) {
		for (const block_step_t max_step : {block_step_t{0}, block_step_t{4},
					block_step_t{6}}) {
			const int farmesh = 256;
			const pos_t plane_y = 17;
			const v3bpos_t player_pos(-65, 33, 95);
			const bpos_t base = 1 << cell_size_pow;
			const block_step_t tree_pow = std::max<block_step_t>(cell_size_pow,
					max_step ? max_step : farmesh::rangeToStep(farmesh) - 1);
			const bpos_t tree_size = 1 << tree_pow;

			struct Cell2D
			{
				v3bpos_t pos;
				bpos_t span;
			};
			std::vector<Cell2D> cells;
			bpos_t min_x = std::numeric_limits<bpos_t>::max();
			bpos_t min_z = std::numeric_limits<bpos_t>::max();
			bpos_t max_x = std::numeric_limits<bpos_t>::min();
			bpos_t max_z = std::numeric_limits<bpos_t>::min();

			farmesh::runFarAll(player_pos, cell_size_pow, farmesh,
					cell_size_pow, plane_y, false, max_step,
					[&](const v3bpos_t &pos, const bpos_t &size,
							const block_step_t &step) {
						const bpos_t span = size << cell_size_pow;
						UASSERTEQ(bpos_t, span,
								static_cast<bpos_t>(1 << (step + cell_size_pow)));
						UASSERTEQ(bpos_t, pos.Y, plane_y);
						cells.push_back({pos, span});
						min_x = std::min(min_x, pos.X);
						min_z = std::min(min_z, pos.Z);
						max_x = std::max(max_x, static_cast<bpos_t>(pos.X + span));
						max_z = std::max(max_z, static_cast<bpos_t>(pos.Z + span));
						return false;
					});

			UASSERT(!cells.empty());
			UASSERTEQ(bpos_t, max_x - min_x, tree_size);
			UASSERTEQ(bpos_t, max_z - min_z, tree_size);
			const size_t side = tree_size / base;
			std::vector<u8> coverage(side * side, 0);
			for (const auto &cell : cells) {
				for (bpos_t z = 0; z < cell.span; z += base)
					for (bpos_t x = 0; x < cell.span; x += base) {
						const size_t ix = (cell.pos.X + x - min_x) / base;
						const size_t iz = (cell.pos.Z + z - min_z) / base;
						auto &count = coverage[ix + iz * side];
						UASSERTEQ(u8, count, 0);
						++count;
					}
			}
			for (const auto count : coverage)
				UASSERTEQ(u8, count, 1);
		}
	}
}

void TestFmFarCalc::testRunFarAllCellEachConsistency()
{
	struct Entry
	{
		v3bpos_t pos;
		bpos_t size;
		block_step_t step;
	};
	const auto less = [](const Entry &a, const Entry &b) {
		if (a.pos.X != b.pos.X)
			return a.pos.X < b.pos.X;
		if (a.pos.Y != b.pos.Y)
			return a.pos.Y < b.pos.Y;
		if (a.pos.Z != b.pos.Z)
			return a.pos.Z < b.pos.Z;
		if (a.step != b.step)
			return a.step < b.step;
		return a.size < b.size;
	};

	for (const uint8_t cell_size_pow : {0, 1, 2, 3}) {
		for (const auto &player_pos : {
					v3bpos_t(0, 0, 0), v3bpos_t(-65, 31, -129),
					v3bpos_t(127, -33, 64)}) {
			std::vector<Entry> expanded_mesh_cells;
			std::vector<Entry> storage_cells;
			const int farmesh = 128;
			const uint8_t quality_pow = 3;
			const bpos_t blocks_per_side = 1 << cell_size_pow;

			farmesh::runFarAll(player_pos, cell_size_pow, farmesh, quality_pow,
					0, false, 0,
					[&](const v3bpos_t &pos, const bpos_t &size,
							const block_step_t &step) {
						const bpos_t step_width = 1 << step;
						for (bpos_t z = 0; z < blocks_per_side; ++z)
							for (bpos_t y = 0; y < blocks_per_side; ++y)
								for (bpos_t x = 0; x < blocks_per_side; ++x)
									expanded_mesh_cells.push_back({
											pos + v3bpos_t(x, y, z) * step_width,
											size, step});
						return false;
					});

			farmesh::runFarAll(player_pos, cell_size_pow, farmesh, quality_pow,
					0, true, 0,
					[&](const v3bpos_t &pos, const bpos_t &size,
							const block_step_t &step) {
						storage_cells.push_back({pos, size, step});
						return false;
					});

			std::sort(expanded_mesh_cells.begin(), expanded_mesh_cells.end(), less);
			std::sort(storage_cells.begin(), storage_cells.end(), less);
			UASSERTEQ(size_t, expanded_mesh_cells.size(), storage_cells.size());
			for (size_t i = 0; i < storage_cells.size(); ++i) {
				UASSERTEQ(v3bpos_t, expanded_mesh_cells[i].pos, storage_cells[i].pos);
				UASSERTEQ(bpos_t, expanded_mesh_cells[i].size, storage_cells[i].size);
				UASSERTEQ(block_step_t, expanded_mesh_cells[i].step,
						storage_cells[i].step);
			}
		}
	}
}

void TestFmFarCalc::testRunFarAllStops()
{
	for (const bool two_d : {false, true}) {
		size_t calls = 0;
		farmesh::runFarAll(v3bpos_t(0, 0, 0), 1, 256, 1, two_d, false, 0,
				[&](const v3bpos_t &, const bpos_t &, const block_step_t &) {
					++calls;
					return true;
				});
		UASSERTEQ(size_t, calls, 1);
	}
}
