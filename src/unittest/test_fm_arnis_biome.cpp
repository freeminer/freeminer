#include "test.h"

#include "mapgen/earth/arnis-cpp/src/biome.h"
#include "mapgen/earth/arnis-cpp/src/land_cover/land_cover.h"

class TestFmArnisBiome : public TestBase
{
public:
	TestFmArnisBiome() { TestManager::registerTestModule(this); }
	const char *getName() override { return "TestFmArnisBiome"; }
	void runTests(IGameDef *) override
	{
		TEST(testColdSteppeTropicalGrasslandIsNotSavanna);
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
};

static TestFmArnisBiome g_test_fm_arnis_biome;
