#include "test.h"

#include "mapgen/earth/arnis-cpp/src/element_processing/advtrains.h"
#include "mapgen/earth/arnis-cpp/src/element_processing/rail_track.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <functional>
#include <initializer_list>
#include <limits>
#include <optional>
#include <set>
#include <vector>

namespace arnis::railways
{
void generate_railways(WorldEditor &editor, const ProcessedWay &element);
std::vector<std::pair<int, int>> build_smoothed_centerline(const ProcessedWay &way);
} // namespace arnis::railways

namespace
{
struct NativeTracks
{
	bool available = arnis::block_definitions::ADVTRAINS_AVAILABLE;
	NativeTracks() { arnis::block_definitions::ADVTRAINS_AVAILABLE = true; }
	~NativeTracks()
	{
		arnis::WorldEditor editor;
		arnis::railways::advtrains::prepare_network({}, editor);
		arnis::block_definitions::ADVTRAINS_AVAILABLE = available;
	}
};

arnis::ProcessedWay way(std::initializer_list<std::pair<pos_t, pos_t>> coordinates)
{
	arnis::ProcessedWay result{};
	result.id = 71;
	result.tags.emplace("railway", "rail");
	for (const auto &[x, z] : coordinates) {
		arnis::ProcessedNode node{};
		node.id = 100 + result.nodes.size();
		node.x = x;
		node.z = z;
		result.nodes.push_back(node);
	}
	return result;
}

std::vector<int> full_scan_rail_profile(const std::vector<bool> &straight,
		const std::function<std::pair<int, int>(std::size_t)> &range,
		const std::function<double(std::size_t, int)> &cost, double step_cost)
{
	constexpr double broken = 1000.0;
	const auto n = straight.size();
	std::vector<int> lows;
	std::vector<std::vector<std::uint32_t>> back;
	std::vector<std::vector<double>> rows;
	for (std::size_t i = 0; i < n; ++i) {
		auto [lo, hi] = range(i);
		if (lo > hi)
			std::swap(lo, hi);
		const auto width = static_cast<std::size_t>(hi - lo + 1);
		std::vector<double> cur(width * 2, std::numeric_limits<double>::infinity());
		std::vector<std::uint32_t> from(
				width * 2, std::numeric_limits<std::uint32_t>::max());
		for (std::size_t bi = 0; bi < width; ++bi) {
			const int b = lo + static_cast<int>(bi);
			const double own = cost(i, b);
			if (i == 0) {
				cur[bi * 2] = own;
				continue;
			}
			for (std::size_t s = 0; s < rows.back().size(); ++s) {
				const double was = rows.back()[s];
				if (!std::isfinite(was))
					continue;
				const int d = b - (lows.back() + static_cast<int>(s / 2));
				double extra = 0.0;
				bool low = false;
				if (d == 1 && straight[i - 1] && s % 2 == 0)
					extra = step_cost;
				else if (d == -1 && straight[i]) {
					extra = step_cost;
					low = true;
				} else if (d != 0) {
					extra = std::abs(d) == 1 ? broken : broken * std::abs(d);
					low = d == -1;
				}
				const auto slot = bi * 2 + static_cast<std::size_t>(low);
				if (was + extra + own < cur[slot]) {
					cur[slot] = was + extra + own;
					from[slot] = static_cast<std::uint32_t>(s);
				}
			}
		}
		lows.push_back(lo);
		rows.push_back(std::move(cur));
		back.push_back(std::move(from));
	}
	if (n == 0)
		return {};
	const auto &last = rows.back();
	auto state = static_cast<std::size_t>(
			std::min_element(last.begin(), last.end()) - last.begin());
	std::vector<int> heights(n);
	for (std::size_t i = n; i-- > 0;) {
		heights[i] = lows[i] + static_cast<int>(state / 2);
		if (i)
			state = back[i][state];
	}
	return heights;
}
} // namespace

class TestFmArnisRailways : public TestBase
{
public:
	TestFmArnisRailways() { TestManager::registerTestModule(this); }
	const char *getName() override { return "TestFmArnisRailways"; }
	void runTests(IGameDef *) override
	{
		TEST(testShallowDiagonalUsesOnlyNativeCells);
		TEST(testReversedWayUsesOnlyNativeCells);
		TEST(testCurveUsesOnlyNativeCells);
		TEST(testEmptyNativePlanDoesNotFallBackToCartRail);
		TEST(testCartBackendStillRegistersItsRaster);
		TEST(testOneWorldAreaEdgeCrossings);
		TEST(testNearbyRailProfileMatchesFullScan);
	}

	void check_native(const arnis::ProcessedWay &railway)
	{
		NativeTracks tracks;
		arnis::WorldEditor editor;
		editor.enable_rail_height_collection();
		// Exercise the real railway dispatcher, initial pass, and finalization.
		// The height registry records every rail placement even without a VM.
		arnis::railways::generate_railways(editor, railway);
		const auto *plan = arnis::railways::advtrains::get_plan(railway);
		UASSERT(plan && !plan->line.empty());
		UASSERT(plan->line.size() == plan->rails.size());
		arnis::world_editor::RailHeightMap expected;
		for (std::size_t i = 0; i < plan->line.size(); ++i) {
			const auto &[x, z] = plan->line[i];
			expected.emplace(arnis::world_editor::RailHeightKey{railway.id, x, z},
					plan->heights[i] + 1);
		}
		const auto raster = arnis::railways::build_smoothed_centerline(railway);
		UASSERT(std::any_of(raster.begin(), raster.end(), [&](const auto &p) {
			return !expected.contains({railway.id, p.first, p.second});
		}));
		// Regression: the previous initial pass registered the cart raster and
		// placed fallback rails between and beside the final native track.
		UASSERT(editor.rail_height_snapshot() == expected);
	}

	void testShallowDiagonalUsesOnlyNativeCells()
	{
		check_native(way({{0, 0}, {24, 64}}));
	}

	void testReversedWayUsesOnlyNativeCells() { check_native(way({{24, 64}, {0, 0}})); }

	void testCurveUsesOnlyNativeCells()
	{
		check_native(way({{0, 0}, {0, 40}, {24, 88}}));
	}

	void testEmptyNativePlanDoesNotFallBackToCartRail()
	{
		NativeTracks tracks;
		arnis::WorldEditor editor;
		editor.enable_rail_height_collection();
		arnis::railways::generate_railways(editor, way({{4, 4}, {4, 4}}));
		UASSERT(editor.rail_height_snapshot().empty());
	}

	void testCartBackendStillRegistersItsRaster()
	{
		NativeTracks tracks;
		arnis::block_definitions::ADVTRAINS_AVAILABLE = false;
		arnis::WorldEditor editor;
		editor.enable_rail_height_collection();
		const auto railway = way({{0, 0}, {24, 64}});
		arnis::railways::generate_railways(editor, railway);
		std::set<std::pair<int, int>> recorded;
		for (const auto &[key, height] : editor.rail_height_snapshot()) {
			const auto &[id, x, z] = key;
			UASSERT(id == railway.id);
			recorded.emplace(x, z);
		}
		const auto raster = arnis::railways::build_smoothed_centerline(railway);
		UASSERT((
				recorded == std::set<std::pair<int, int>>(raster.begin(), raster.end())));
	}

	void testOneWorldAreaEdgeCrossings()
	{
		const std::vector<arnis::ProcessedElement> empty;
		auto joins =
				arnis::railways::RailJoins::build(empty, arnis::railways::track_level);
		joins.meet_at_edge_of({0, 0, 9, 9});
		const std::vector<std::pair<int, int>> cells{{-1, 5}, {0, 5}, {10, 5}, {11, 5}};
		const auto crossings = joins.edge_crossings(cells);
		UASSERT(crossings.size() == 2);
		UASSERT((crossings[0] == std::array<std::size_t, 2>{0, 1}));
		UASSERT((crossings[1] == std::array<std::size_t, 2>{1, 2}));
	}

	void testNearbyRailProfileMatchesFullScan()
	{
		std::uint64_t seed = 0x9e3779b97f4a7c15ULL;
		const auto random = [&seed](std::uint64_t limit) {
			seed ^= seed << 13;
			seed ^= seed >> 7;
			seed ^= seed << 17;
			return seed % limit;
		};
		for (int trial = 0; trial < 3000; ++trial) {
			const auto n = static_cast<std::size_t>(1 + random(40));
			std::vector<bool> straight(n);
			std::vector<int> ground(n);
			std::vector<std::optional<int>> pins(n);
			for (std::size_t i = 0; i < n; ++i) {
				straight[i] = random(3) > 0;
				ground[i] = static_cast<int>(random(9)) - 4;
				if (random(6) == 0)
					pins[i] = static_cast<int>(random(13)) - 6;
			}
			const int reach = static_cast<int>(random(4));
			const double step = std::array<double, 3>{0.0, 0.7, 2.5}[random(3)];
			const auto range = [&](std::size_t i) {
				return pins[i] ? std::pair{*pins[i], *pins[i]}
							   : std::pair{ground[i] - reach, ground[i] + reach};
			};
			const auto cost = [&](std::size_t i, int y) {
				return 0.25 * std::abs(y - ground[i]) + static_cast<double>(i % 3) * 0.1;
			};
			UASSERT(arnis::railways::rail_profile(straight, range, cost, step) ==
					full_scan_rail_profile(straight, range, cost, step));
		}
	}
};

static TestFmArnisRailways g_test_fm_arnis_railways;
