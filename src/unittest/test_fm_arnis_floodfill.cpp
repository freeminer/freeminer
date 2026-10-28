#include "test.h"

#include "mapgen/earth/arnis-cpp/src/floodfill.h"

#include <algorithm>
#include <utility>
#include <vector>

class TestFmArnisFloodFill : public TestBase
{
public:
	TestFmArnisFloodFill() { TestManager::registerTestModule(this); }
	const char *getName() override { return "TestFmArnisFloodFill"; }
	void runTests(IGameDef *) override
	{
		TEST(testClosedSquareFillsOnlyStrictInterior);
		TEST(testReversedRingKeepsSameFill);
		TEST(testOpenWayIsNotClosedAndFilled);
		TEST(testHugeBoundingBoxThinRingUsesScanlineFill);
		TEST(testDegenerateHugeRingIsRejectedWithinScanlineBudget);
	}

	void testClosedSquareFillsOnlyStrictInterior()
	{
		const std::vector<std::pair<int, int>> square{
				{0, 0}, {4, 0}, {4, 4}, {0, 4}, {0, 0}};
		auto filled = arnis::floodfill::flood_fill_area(square, nullptr);
		std::sort(filled.begin(), filled.end());
		std::vector<std::pair<int, int>> expected;
		for (int z = 1; z < 4; ++z)
			for (int x = 1; x < 4; ++x)
				expected.emplace_back(x, z);
		UASSERT(filled == expected);
	}

	void testReversedRingKeepsSameFill()
	{
		const std::vector<std::pair<int, int>> ring{
				{-3, -2}, {8, -2}, {8, 7}, {-3, 7}, {-3, -2}};
		auto forward = arnis::floodfill::flood_fill_area(ring, nullptr);
		auto reversed = ring;
		std::reverse(reversed.begin(), reversed.end());
		auto backward = arnis::floodfill::flood_fill_area(reversed, nullptr);
		std::sort(forward.begin(), forward.end());
		std::sort(backward.begin(), backward.end());
		UASSERT(forward == backward);
	}

	void testOpenWayIsNotClosedAndFilled()
	{
		const std::vector<std::pair<int, int>> open_way{{0, 0}, {12, 0}, {12, 8}, {0, 8}};
		UASSERT(arnis::floodfill::flood_fill_area(open_way, nullptr).empty());
	}

	void testHugeBoundingBoxThinRingUsesScanlineFill()
	{
		// The bbox covers over 36 million cells, past the bitmap cap, but the
		// diagonal strip itself has only about sixty thousand interior cells.
		const std::vector<std::pair<int, int>> strip{
				{0, 0}, {6000, 6000}, {6010, 6000}, {10, 0}, {0, 0}};
		const auto filled = arnis::floodfill::flood_fill_area(strip, nullptr);
		UASSERT(!filled.empty());
		UASSERT(filled.size() < 100000);
		UASSERT(std::all_of(filled.begin(), filled.end(), [](const auto &cell) {
			return cell.first > cell.second && cell.first - cell.second < 10;
		}));
	}

	void testDegenerateHugeRingIsRejectedWithinScanlineBudget()
	{
		const std::vector<std::pair<int, int>> collinear{{-1000000000, -1000000000},
				{1000000000, 1000000000}, {0, 0}, {-1000000000, -1000000000}};
		UASSERT(arnis::floodfill::flood_fill_area(collinear, nullptr).empty());
	}
};

static TestFmArnisFloodFill g_test_fm_arnis_floodfill;
