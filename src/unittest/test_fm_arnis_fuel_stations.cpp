#include "test.h"

#include "mapgen/earth/arnis-cpp/src/block_definitions.h"
#include "mapgen/earth/arnis-cpp/src/element_processing/fuel_stations.h"
#include "mapgen/earth/arnis_world_editor.h"

#include <memory>
#include <vector>

class TestFmArnisFuelStations : public TestBase
{
public:
	TestFmArnisFuelStations() { TestManager::registerTestModule(this); }
	const char *getName() override { return "TestFmArnisFuelStations"; }
	void runTests(IGameDef *) override { TEST(testForecourtKeepsCanopyBesideShop); }

	void testForecourtKeepsCanopyBesideShop()
	{
		using namespace arnis;
		WorldEditor editor;
		const XZBBox bbox(0, 0, 79, 79);
		ProcessedWay way{};
		way.id = 1;
		way.tags.emplace("amenity", "fuel");
		const int coordinates[5][2] = {{10, 20}, {70, 20}, {70, 60}, {10, 60}, {10, 20}};
		for (std::size_t i = 0; i < 5; ++i) {
			ProcessedNode node{};
			node.id = i % 4;
			node.x = coordinates[i][0];
			node.z = coordinates[i][1];
			way.nodes.push_back(std::move(node));
		}
		const ProcessedElement station(way);
		editor.set_fuel_index(std::make_shared<const fuel_stations::FuelIndex>(
				fuel_stations::FuelIndex::build({station}, 1.0)));

		BuildingFootprintBitmap shop(bbox);
		for (int x = 31; x <= 38; ++x)
			for (int z = 33; z <= 39; ++z)
				shop.set(x, z);
		RoadMaskBitmap road(bbox);
		for (int x = 0; x <= 79; ++x)
			for (int z = 0; z <= 2; ++z)
				road.set(x, z);

		fuel_stations::generate_station(
				editor, station, FloodFillCache{}, shop, road, 1.0, std::nullopt);

		const auto canopy_block = editor.get_block_absolute(34, 0, 45);
		UASSERT(canopy_block &&
				canopy_block->id() == block_definitions::LIGHT_GRAY_CONCRETE.id());
	}
};

static TestFmArnisFuelStations g_test_fm_arnis_fuel_stations;
