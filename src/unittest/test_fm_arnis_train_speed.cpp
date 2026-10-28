#include "test.h"

#include "mapgen/earth/arnis-cpp/src/args.h"
#include "mapgen/earth/arnis-cpp/src/coordinate_system/cartesian/xzbbox/xzbbox_enum.h"
#include "mapgen/earth/arnis-cpp/src/element_processing/trains.h"

#include <algorithm>
#include <cstdint>
#include <cstdlib>
#include <utility>
#include <vector>

class TestFmArnisTrainSpeed : public TestBase
{
public:
	TestFmArnisTrainSpeed() { TestManager::registerTestModule(this); }
	const char *getName() override { return "TestFmArnisTrainSpeed"; }
	void runTests(IGameDef *) override { TEST(testLargestScaleStepStaysWithinPackRange); }

	void testLargestScaleStepStaysWithinPackRange()
	{
		arnis::ProcessedWay railway{};
		railway.id = 1;
		railway.tags.emplace("railway", "rail");
		for (int i = 0; i <= 40; ++i) {
			arnis::ProcessedNode node{};
			node.id = 100 + static_cast<std::uint64_t>(i);
			node.x = i * 100;
			node.z = 1000;
			railway.nodes.push_back(std::move(node));
		}
		std::vector<arnis::ProcessedElement> elements;
		elements.emplace_back(railway);
		const auto bounds = arnis::cartesian::XZBBox::rect_from_min_max(0, 0, 4000, 2000);
		UASSERT(bounds.min_x() == 0 && bounds.min_z() == 0);
		UASSERT(bounds.max_x() == 4000 && bounds.max_z() == 2000);
		const auto network = arnis::trains::TrainRouteNetwork::build(
				elements, arnis::MAX_SCALE, bounds);
		UASSERT(!network.services().empty());

		for (const auto &service : network.services()) {
			const auto lap = service.period;
			std::int64_t largest = 0;
			for (std::size_t i = 1; i < service.timetable.size(); ++i) {
				std::int64_t distance = static_cast<std::int64_t>(service.timetable[i]) -
										service.timetable[i - 1];
				distance = distance < 0 ? -distance : distance;
				if (lap > 0) {
					distance %= lap;
					distance = std::min<std::int64_t>(distance, lap - distance);
				}
				largest = std::max(largest, distance);
			}
			UASSERT(largest > 7000 && largest < 8000);
		}
	}
};

static TestFmArnisTrainSpeed g_test_fm_arnis_train_speed;
