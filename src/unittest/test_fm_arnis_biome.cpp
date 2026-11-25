#include "test.h"

#include "mapgen/earth/arnis-cpp/src/biome.h"
#include "mapgen/earth/arnis-cpp/src/climate.h"
#include "mapgen/earth/arnis-cpp/src/ground_decoration.h"
#include "mapgen/earth/arnis-cpp/src/land_cover/land_cover.h"

class TestFmArnisBiome : public TestBase
{
public:
	TestFmArnisBiome() { TestManager::registerTestModule(this); }
	const char *getName() override { return "TestFmArnisBiome"; }
	void runTests(IGameDef *) override
	{
		TEST(testColdSteppeTropicalGrasslandIsNotSavanna);
		TEST(testMoistTropicalPlantsStayGreenInSavannaClimate);
	}

	void testColdSteppeTropicalGrasslandIsNotSavanna()
	{
		using namespace arnis;
		const auto eco = ecoregion::lookup(34); // Angolan mopane woodlands
		UASSERT(eco && eco->biome == ecoregion::EcoBiome::TropicalGrassland);
		UASSERT(!biome::ecoregion_biome(
				land_cover::LC_SHRUBLAND, biome::Climate::ColdSteppe, eco));
		UASSERT(!biome::ecoregion_biome(
				land_cover::LC_GRASSLAND, biome::Climate::ColdSteppe, eco));
	}

	void testMoistTropicalPlantsStayGreenInSavannaClimate()
	{
		using namespace arnis;
		const auto eco = ecoregion::lookup(1); // Moist tropical forest.
		UASSERT(eco && eco->biome == ecoregion::EcoBiome::MoistTropical);
		UASSERT(climate::for_plants(biome::Climate::TropicalSavanna, eco) ==
				biome::Climate::Temperate);
		UASSERT(climate::for_plants(biome::Climate::HotDesert, eco) ==
				biome::Climate::HotDesert);
		const auto dry_eco = ecoregion::lookup(34);
		UASSERT(climate::for_plants(biome::Climate::TropicalSavanna, dry_eco) ==
				biome::Climate::TropicalSavanna);
		biome::Cell cell;
		cell.land_cover = land_cover::LC_GRASSLAND;
		cell.eco = eco;
		UASSERT(biome::cell_biome(cell, biome::Climate::TropicalSavanna, 10.0) ==
				"minecraft:plains");
		UASSERT(ground_decoration::habitat(land_cover::LC_GRASSLAND,
						biome::Climate::TropicalSavanna, 10.0, false,
						eco) == ground_decoration::Habitat::Meadow);
	}
};

static TestFmArnisBiome g_test_fm_arnis_biome;
