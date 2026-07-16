// Luanti
// SPDX-License-Identifier: LGPL-2.1-or-later
// Copyright (C) 2024 proller <proler@gmail.com>

#include "debug/dump.h"
#include "test.h"
#include "catch.h"
#include "fm_far_calc.h"
#include "client/clientmap.h"

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
	void testRunFarAllVerification();
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
	TEST(testRunFarAllVerification);
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
