#include "test.h"

#include "mapgen/earth/arnis-cpp/src/decals/pictograms.h"
#include "mapgen/earth/arnis-cpp/src/element_processing/attractions.h"
#include "mapgen/earth/arnis-cpp/src/floodfill_cache.h"
#include "mapgen/earth/arnis_world_editor.h"

#include <initializer_list>
#include <optional>
#include <string>
#include <utility>
#include <vector>

namespace
{
arnis::tags_t tags(std::initializer_list<std::pair<const char *, const char *>> pairs)
{
	arnis::tags_t result;
	for (const auto &[key, value] : pairs)
		result.emplace(key, value);
	return result;
}
} // namespace

class TestFmArnisAttractions : public TestBase
{
public:
	TestFmArnisAttractions() { TestManager::registerTestModule(this); }
	const char *getName() override { return "TestFmArnisAttractions"; }
	void runTests(IGameDef *) override
	{
		TEST(testRideBuildingsAndHalls);
		TEST(testGenericRideFeatureOwnership);
		TEST(testSpinnerPartReplacement);
		TEST(testBusinessKindMatchesRustTagCoverage);
		TEST(testDropTowerNameHintsRespectWordBoundaries);
		TEST(testRidesInsideAnotherBuildingAreLeftToIt);
	}

	void testRideBuildingsAndHalls()
	{
		UASSERT(arnis::attractions::claims(
				tags({{"attraction", "carousel"}, {"building", "yes"}})));
		UASSERT(arnis::attractions::claims(tags({{"attraction", "big_wheel"}})));
		UASSERT(!arnis::attractions::claims(
				tags({{"attraction", "dark_ride"}, {"building", "yes"}})));
		UASSERT(!arnis::attractions::claims(
				tags({{"attraction", "bumper_car"}, {"building", "yes"}})));
		UASSERT(arnis::attractions::claims(tags({{"attraction", "bumper_car"}})));
		UASSERT(!arnis::attractions::claims(tags({{"attraction", "animal"}})));
	}

	void testGenericRideFeatureOwnership()
	{
		for (const auto &[key, value] : {std::pair{"barrier", "fence"},
					 std::pair{"railway", "rail"}, std::pair{"tourism", "attraction"}}) {
			auto ride = tags({{"attraction", "amusement_ride"}});
			ride.emplace(key, value);
			UASSERT(!arnis::attractions::claims(ride));
		}
		UASSERT(arnis::attractions::replaces_parts(tags({{"attraction", "big_wheel"}})));
		UASSERT(!arnis::attractions::replaces_parts(
				tags({{"attraction", "amusement_ride"}})));
	}

	void testSpinnerPartReplacement()
	{
		UASSERT(arnis::attractions::replaces_parts(
				tags({{"attraction", "amusement_ride"}, {"name", "Riesenrad Wien"}})));
		UASSERT(arnis::attractions::replaces_parts(
				tags({{"attraction", "amusement_ride"}, {"name", "Autoscooter"}})));
		UASSERT(!arnis::attractions::replaces_parts(
				tags({{"attraction", "amusement_ride"}, {"name", "Breakdance Meyer"}})));
		UASSERT(!arnis::attractions::claims(
				tags({{"attraction", "amusement_ride"}, {"building", "yes"}})));
		UASSERT(!arnis::attractions::claims(
				tags({{"attraction", "amusement_ride"}, {"railway", "rail"}})));
		UASSERT(arnis::attractions::claims(
				tags({{"attraction", "amusement_ride"}, {"name", "Tagada"}})));
	}

	void testBusinessKindMatchesRustTagCoverage()
	{
		using arnis::decals::pictograms::business_kind;
		UASSERT(business_kind(tags({{"shop", "department_store"}})) == "supermarket");
		UASSERT(business_kind(tags({{"shop", "bakery"}})) == "bakery");
		UASSERT(business_kind(tags({{"shop", "unknown_thing"}})) == "shop");
		UASSERT(business_kind(tags({{"amenity", "conference_centre"}})) == "office");
		UASSERT(business_kind(tags({{"tourism", "gallery"}})) == "museum");
		UASSERT(business_kind(tags({{"leisure", "water_park"}})) == "swimming");
		UASSERT(business_kind(tags({{"healthcare", "physiotherapist"}})) == "doctors");
		UASSERT(!business_kind(tags({{"amenity", "bench"}})));
	}

	void testDropTowerNameHintsRespectWordBoundaries()
	{
		for (const char *name : {"Wirbelsturm", "El Torrente", "Le Tour des Jardins",
					 "Big Bird's Tour Bus"})
			UASSERT(!arnis::attractions::replaces_parts(
					tags({{"attraction", "amusement_ride"}, {"name", name}})));
		for (const char *name :
				{"Freifallturm", "Kristallturm", "La Tour des Moussaillons", "Tårnet"})
			UASSERT(arnis::attractions::replaces_parts(
					tags({{"attraction", "amusement_ride"}, {"name", name}})));
	}

	void testRidesInsideAnotherBuildingAreLeftToIt()
	{
		using namespace arnis;
		const XZBBox bbox(0, 0, 60, 60);
		BuildingFootprintBitmap hall(bbox);
		for (int x = 10; x <= 50; ++x)
			for (int z = 10; z <= 50; ++z)
				hall.set(x, z);
		const RoadMaskBitmap roads(bbox);

		ProcessedNode node{};
		node.id = 1;
		node.tags = tags({{"attraction", "carousel"}});
		node.x = 30;
		node.z = 30;
		const ProcessedElement ride(node);
		const std::vector<ProcessedElement> node_elements{ride};
		const auto node_fills = FloodFillCache::precompute(node_elements, std::nullopt);
		auto draw_node = [&](const BuildingFootprintBitmap &footprints) {
			WorldEditor editor;
			const bool drawn = attractions::generate_ride(
					editor, ride, node_fills, footprints, roads, 1.0, std::nullopt);
			return std::pair{drawn, editor.block_exists_absolute(30, 1, 30)};
		};
		const auto inside_another_building = draw_node(hall);
		UASSERT(!inside_another_building.first && !inside_another_building.second);
		const BuildingFootprintBitmap no_footprints(bbox);
		const auto in_open_ground = draw_node(no_footprints);
		UASSERT(in_open_ground.first && in_open_ground.second);

		ProcessedWay own_way{};
		own_way.id = 2;
		own_way.tags = tags({{"attraction", "carousel"}, {"building", "yes"}});
		const int coordinates[5][2] = {{10, 10}, {50, 10}, {50, 50}, {10, 50}, {10, 10}};
		for (int i = 0; i < 5; ++i) {
			ProcessedNode corner{};
			corner.id = 20 + i;
			corner.x = coordinates[i][0];
			corner.z = coordinates[i][1];
			own_way.nodes.push_back(corner);
		}
		own_way.nodes.back().id = own_way.nodes.front().id;
		const ProcessedElement mapped_building_ride(std::move(own_way));
		const std::vector<ProcessedElement> way_elements{mapped_building_ride};
		const auto way_fills = FloodFillCache::precompute(way_elements, std::nullopt);
		WorldEditor editor;
		UASSERT(attractions::generate_ride(
				editor, mapped_building_ride, way_fills, hall, roads, 1.0, std::nullopt));
		UASSERT(editor.block_exists_absolute(30, 1, 30));
	}
};

static TestFmArnisAttractions g_test_fm_arnis_attractions;
