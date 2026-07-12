// Freeminer

#include "test.h"

#include <cstdlib>

#include "mapgen/mapgen_terraindiffusion.h"
#include "mapgen/mapgen_terraindiffusion_native.h"

class TestTerrainDiffusion : public TestBase
{
public:
	TestTerrainDiffusion() { TestManager::registerTestModule(this); }
	const char *getName() { return "TestTerrainDiffusion"; }

	void runTests(IGameDef *gamedef)
	{
		TEST(testDeterministicHelpers);
		if (const char *model_dir = std::getenv("FREEMINER_TD_MODEL_DIR"))
			TEST(testModelLoad, model_dir);
	}

	void testDeterministicHelpers()
	{
		std::string error;
		UASSERT(TerrainDiffusionNativePipeline::runDeterminismSelfTest(error));
	}

	void testModelLoad(const char *model_dir)
	{
		TerrainDiffusionNativePipeline pipeline(1, 30, 1.0f, 0.0f, 0.7f,
				8, 128, "cpu", 0, 8, "", false);
		UASSERT(pipeline.load(model_dir));
		std::vector<TerrainDiffusionSample> samples;
		UASSERT(!pipeline.sampleGridCached(0, 0, 0, 0, samples));
	}
};

static TestTerrainDiffusion g_test_instance;
