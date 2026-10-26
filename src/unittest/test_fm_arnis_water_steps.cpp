#include "test.h"

#include "mapgen/earth/arnis-cpp/src/block_definitions.h"
#include "mapgen/earth/arnis-cpp/src/water_steps.h"
#include "mapgen/earth/arnis_world_editor.h"

#include <string>

class TestFmArnisWaterSteps : public TestBase
{
public:
	TestFmArnisWaterSteps() { TestManager::registerTestModule(this); }
	const char *getName() override { return "TestFmArnisWaterSteps"; }
	void runTests(IGameDef *) override { TEST(testOneBlockWaterDropGetsVanillaRunUp); }

	void testOneBlockWaterDropGetsVanillaRunUp()
	{
		using namespace arnis;
		WorldEditor editor;
		for (int x = 0; x <= 40; ++x)
			for (int z = 0; z <= 4; ++z) {
				const bool upper = x < 20;
				const int floor = upper ? 27 : 26;
				editor.set_block_absolute(block_definitions::STONE, x, floor, z);
				for (int y = floor + 1; y <= floor + 3; ++y)
					editor.set_block_absolute(block_definitions::WATER, x, y, z);
			}

		water_steps::shape_world(editor, false);
		auto level = [&](int x, int y) {
			const auto block = editor.get_block_with_properties_absolute(x, y, 2);
			UASSERT(block && block->block.id() == block_definitions::WATER.id());
			const auto found = block->property_values().find("level");
			return found == block->property_values().end() ? 0 : std::stoi(found->second);
		};
		UASSERT(level(19, 30) == 7);
		UASSERT(level(13, 30) == 1);
		UASSERT(level(12, 30) == 0);
		UASSERT(level(20, 29) == 0);
	}
};

static TestFmArnisWaterSteps g_test_fm_arnis_water_steps;
