#include "test.h"
#include "mapgen/earth/arnis-cpp/src/overture/fm_range_fetch.h"

#include <atomic>
#include <chrono>
#include <future>
#include <thread>

class TestFmArnisRangeFetch : public TestBase
{
public:
	TestFmArnisRangeFetch() { TestManager::registerTestModule(this); }
	const char *getName() override { return "TestFmArnisRangeFetch"; }
	void runTests(IGameDef *) override { TEST(testConcurrentRequests); }

	void testConcurrentRequests()
	{
		arnis::overture::RangeFetches ranges;
		std::atomic<int> loads{0};
		std::promise<void> started, release;
		auto gate = release.get_future().share();
		auto load = [&]() -> std::optional<std::vector<std::uint8_t>> {
			if (++loads == 1)
				started.set_value();
			gate.wait();
			return std::vector<std::uint8_t>{1, 2, 3};
		};
		auto first = std::async(
				std::launch::async, [&] { return ranges.fetch("a", 4, 3, load); });
		started.get_future().wait();
		auto other = ranges.fetch("b", 4, 3, [] {
			return std::optional<std::vector<std::uint8_t>>{std::vector<std::uint8_t>{9}};
		});
		UASSERT(other && other->at(0) == 9);
		auto second = std::async(
				std::launch::async, [&] { return ranges.fetch("a", 4, 3, load); });
		// The first request remains in flight while the second joins it.
		UASSERT(second.wait_for(std::chrono::milliseconds(50)) ==
				std::future_status::timeout);
		release.set_value();
		UASSERT(first.get() == second.get());
		UASSERTEQ(int, loads.load(), 1);
		UASSERT(!ranges.fetch(
				"a", 4, 3, [] { return std::optional<std::vector<std::uint8_t>>{}; }));
	}
};

static TestFmArnisRangeFetch g_test_fm_arnis_range_fetch;
