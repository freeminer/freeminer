#include "test.h"

#include "mapgen/earth/arnis_block.h"
#include "mapgen/earth/arnis-cpp/src/bedrock_block_map.h"
#include "mapgen/earth/arnis-cpp/src/luanti_block_map.h"

#include <string>

class TestFmArnisBlockMapping : public TestBase
{
public:
	TestFmArnisBlockMapping() { TestManager::registerTestModule(this); }
	const char *getName() override { return "TestFmArnisBlockMapping"; }
	void runTests(IGameDef *) override
	{
		TEST(testNewRustBlocksHaveLuantiMappings);
		TEST(testPottedPlantBedrockMapping);
		TEST(testWaterloggedBedrockState);
	}

	void testNewRustBlocksHaveLuantiMappings()
	{
		using namespace arnis;
		UASSERT(std::string(to_luanti_node(CHISELED_SANDSTONE).name) ==
				"mcl_core:sandstonecarved");
		UASSERT(std::string(to_luanti_node(CUT_SANDSTONE).name) ==
				"mcl_core:sandstonesmooth");
		UASSERT(std::string(to_luanti_node(RAW_GOLD_BLOCK).name) ==
				"mcl_raw_ores:raw_gold_block");
		UASSERT(std::string(to_luanti_node(LAPIS_BLOCK).name) == "mcl_core:lapisblock");
	}

	void testPottedPlantBedrockMapping()
	{
		using namespace arnis;
		UASSERT(BEDROCK_BLOCK_STATE_VERSION == ((1 << 24) | (21 << 16) | (60 << 8) | 33));
		const auto plant = potted_plant(FLOWER_POT);
		UASSERT(plant && plant->name == "minecraft:poppy");
		const auto tulip = potted_plant(POTTED_RED_TULIP);
		UASSERT(tulip && tulip->name == "minecraft:red_tulip");
		const auto dandelion = potted_plant(POTTED_DANDELION);
		UASSERT(dandelion && dandelion->name == "minecraft:dandelion");
		const auto orchid = potted_plant(POTTED_BLUE_ORCHID);
		UASSERT(orchid && orchid->name == "minecraft:blue_orchid");
		UASSERT(!potted_plant(STONE));
	}

	void testWaterloggedBedrockState()
	{
		using namespace arnis;
		UASSERT(is_waterlogged(BlockWithProperties{KELP}));
		UASSERT(is_waterlogged(BlockWithProperties{
				OAK_STAIRS, BlockWithProperties::Properties{{"waterlogged", "true"}}}));
		UASSERT(!is_waterlogged(BlockWithProperties{
				OAK_STAIRS, BlockWithProperties::Properties{{"waterlogged", "false"}}}));
	}
};

static TestFmArnisBlockMapping g_test_fm_arnis_block_mapping;
