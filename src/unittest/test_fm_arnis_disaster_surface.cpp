#include "test.h"

#include "mapgen/earth/arnis-cpp/src/fm_datapack_disasters.h"

#include <algorithm>
#include <string>

class TestFmArnisDisasterSurface : public TestBase
{
public:
	TestFmArnisDisasterSurface() { TestManager::registerTestModule(this); }
	const char *getName() override { return "TestFmArnisDisasterSurface"; }
	void runTests(IGameDef *) override
	{
		TEST(testSurfaceClassificationMatchesJavaNames);
		TEST(testWildfireAdviceMatchesRust);
	}

	void testSurfaceClassificationMatchesJavaNames()
	{
		using arnis::datapack::disasters::Surface;
		using arnis::datapack::disasters::SurfaceClass;
		UASSERT(Surface::classify("minecraft:water") == SurfaceClass::Fluid);
		UASSERT(Surface::classify("minecraft:oak_leaves") == SurfaceClass::Pass);
		UASSERT(Surface::classify("minecraft:oak_log") == SurfaceClass::Solid);
		UASSERT(Surface::classify("other:water") == SurfaceClass::Solid);
		UASSERT(Surface::classify("minecraft:water_source") == SurfaceClass::Solid);
	}

	void testWildfireAdviceMatchesRust()
	{
		arnis::datapack::disasters::Plan plan(
				arnis::CelestialBody::Earth, -64, 319, true);
		const auto files = plan.files("test area");
		const auto area = std::find_if(files.begin(), files.end(), [](const auto &file) {
			return file.first.ends_with("/area.mcfunction");
		});
		UASSERT(area != files.end());
		UASSERT(area->second.find("Keep ahead of the fire") != std::string::npos);
	}
};

static TestFmArnisDisasterSurface g_test_fm_arnis_disaster_surface;
