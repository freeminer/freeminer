#include "test.h"

#include "mapgen/earth/arnis-cpp/src/block_definitions.h"
#include "mapgen/earth/arnis-cpp/src/water_depth.h"
#include "mapgen/earth/arnis-cpp/src/water_steps.h"
#include "mapgen/earth/arnis_world_editor.h"

#include <string>

class TestFmArnisWaterSteps : public TestBase
{
public:
	TestFmArnisWaterSteps() { TestManager::registerTestModule(this); }
	const char *getName() override { return "TestFmArnisWaterSteps"; }
	void runTests(IGameDef *) override
	{
		TEST(testOneBlockWaterDropGetsVanillaRunUp);
		TEST(testBoundedTreeClearStopsAtItsColumns);
	}

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

	void testBoundedTreeClearStopsAtItsColumns()
	{
		using namespace arnis;
		WorldEditor editor;
		auto is_block = [&](int x, int y, const Block &expected) {
			const auto block = editor.get_block_absolute(x, y, 5);
			return block && block->id() == expected.id();
		};
		auto is_empty = [&](int x, int y) {
			const auto block = editor.get_block_absolute(x, y, 5);
			return !block || block->id() == block_definitions::AIR.id();
		};

		editor.set_block_absolute(block_definitions::OAK_LOG, 0, 1, 5);
		for (int x = 0; x < 64; ++x)
			editor.set_block_absolute(block_definitions::OAK_LEAVES, x, 2, 5);

		water_depth::clear_tree_within(
				editor, 0, 1, 5, [](int x, int) { return x <= 20; });
		UASSERT(is_empty(0, 1));
		UASSERT(is_empty(20, 2));
		UASSERT(is_block(21, 2, block_definitions::OAK_LEAVES));

		water_depth::clear_tree_from(editor, 21, 2, 5);
		UASSERT(is_empty(63, 2));
	}
};

static TestFmArnisWaterSteps g_test_fm_arnis_water_steps;
