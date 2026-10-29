#include "test.h"

#include "mapgen/earth/arnis-cpp/src/args.h"
#include "mapgen/earth/arnis-cpp/src/bedrock_addon/models.h"
#include "mapgen/earth/arnis-cpp/src/coordinate_system/cartesian/xzbbox/xzbbox_enum.h"
#include "mapgen/earth/arnis-cpp/src/element_processing/trains.h"

#include <algorithm>
#include <cstdint>
#include <cstdlib>
#include <optional>
#include <sstream>
#include <string>
#include <unordered_set>
#include <utility>
#include <vector>

class TestFmArnisTrainSpeed : public TestBase
{
public:
	TestFmArnisTrainSpeed() { TestManager::registerTestModule(this); }
	const char *getName() override { return "TestFmArnisTrainSpeed"; }
	void runTests(IGameDef *) override
	{
		TEST(testLargestScaleStepStaysWithinPackRange);
		TEST(testBedrockDataMatchesScheduledRoutesAndRailHeights);
		TEST(testBedrockMovingVehiclesPassValidation);
		TEST(testMoonDefaultsPreserveBodyGravity);
	}

	void testBedrockMovingVehiclesPassValidation()
	{
		arnis::Args args;
		args.bedrock = true;
		args.moving_vehicles = true;
		UASSERT(!args.validation_error());

		args.bedrock = false;
		args.luanti = true;
		UASSERT(args.validation_error().has_value());
	}

	void testMoonDefaultsPreserveBodyGravity()
	{
		arnis::Args args;
		args.body = arnis::CelestialBody::Moon;
		args.body_gravity = true;
		args.apply_body_defaults();
		UASSERT(args.body_gravity);
		UASSERT(!args.validation_error());
	}

	void testBedrockDataMatchesScheduledRoutesAndRailHeights()
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
		const auto network = arnis::trains::TrainRouteNetwork::build(
				elements, arnis::MAX_SCALE, bounds);
		const auto [data, bodies] = network.bedrock_data(
				[](std::uint64_t, int, int) { return std::optional<int>{-61}; });
		const auto &services = data["services"];
		UASSERT(services.is_array() && !services.empty());
		UASSERT(services.size() == network.services().size());
		std::size_t train_count = 0;
		std::unordered_set<std::uint64_t> carriage_ids;
		for (std::size_t i = 0; i < services.size(); ++i) {
			const auto &service = services[i];
			const auto route_index = service["route"].get<std::size_t>();
			const auto &route = data["routes"][route_index];
			std::istringstream encoded(route["p"].get<std::string>());
			std::string field;
			for (int component = 0; component < 2; ++component)
				UASSERT(static_cast<bool>(std::getline(encoded, field, ',')));
			// A rail at -61 is 1/16 block deep; Rust serializes its center at -60.9375.
			UASSERT(std::stoll(field) == -60938);
			UASSERT(route["p"].get<std::string>().find(',') != std::string::npos);
			const auto &rust_service = network.services()[i];
			const auto &rust_route = network.routes()[rust_service.route];
			std::size_t point_count = 2;
			while (std::getline(encoded, field, ','))
				++point_count;
			UASSERT(point_count == rust_route.points.size() * 4);
			UASSERT(service["count"].get<std::size_t>() == rust_service.timetable.size());
			for (const auto &train : service["trains"]) {
				++train_count;
				for (const auto &car : train["cars"]) {
					UASSERT(car[1].get<std::size_t>() < bodies.size());
					UASSERT(carriage_ids.insert(car[0].get<std::uint64_t>()).second);
				}
			}
		}
		UASSERT(train_count == network.train_count());
		UASSERT(data["bodies"].size() == bodies.size());
		for (std::size_t i = 0; i < bodies.size(); ++i) {
			UASSERT(data["bodies"][i].get<std::string>() == bodies[i].name);
			UASSERT(bodies[i].name.starts_with("train_"));
			for (const auto &shape : bodies[i].shapes)
				UASSERT(arnis::bedrock_addon::models::knows(shape.block));
		}
		const auto without_rails = network.bedrock_data(
				[](std::uint64_t, int, int) { return std::optional<int>{}; });
		UASSERT(without_rails.first["services"].empty());
	}

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
				largest = std::max(largest, static_cast<std::int64_t>(distance));
			}
			UASSERT(largest > 7000 && largest < 8000);
		}
	}
};

static TestFmArnisTrainSpeed g_test_fm_arnis_train_speed;
