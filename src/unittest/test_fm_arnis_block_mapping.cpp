#include "test.h"

#include "mapgen/earth/arnis_block.h"
#include "mapgen/earth/arnis_world_editor.h"
#include "mapgen/earth/arnis-cpp/src/bedrock_block_map.h"
#include "mapgen/earth/arnis-cpp/src/luanti_block_map.h"
#include "mapgen/earth/arnis-cpp/src/world_editor/common.h"

#include <string>
#include <array>
#include <cstdint>
#include <utility>

class TestFmArnisBlockMapping : public TestBase
{
public:
	TestFmArnisBlockMapping() { TestManager::registerTestModule(this); }
	const char *getName() override { return "TestFmArnisBlockMapping"; }
	void runTests(IGameDef *) override
	{
		using namespace arnis;
		// The Rust palette has stable, distinct IDs. The host palette is normally
		// populated from the loaded game, which is absent in these unit tests.
		struct PaletteFixture
		{
			std::array<std::pair<Block *, content_t>, 12> entries{
					{{&STONE, 84}, {&KELP, 66}, {&OAK_STAIRS, 144}, {&FLOWER_POT, 235},
							{&POTTED_RED_TULIP, 246}, {&POTTED_DANDELION, 247},
							{&POTTED_BLUE_ORCHID, 248}, {&HOPPER, 284},
							{&CHISELED_SANDSTONE, 516}, {&CUT_SANDSTONE, 517},
							{&RAW_GOLD_BLOCK, 518}, {&BARRIER, 527}}};
			std::array<Block, 12> saved;
			Block lapis = LAPIS_BLOCK;
			PaletteFixture()
			{
				for (std::size_t i = 0; i < entries.size(); ++i) {
					saved[i] = *entries[i].first;
					*entries[i].first = Block(entries[i].second);
				}
				LAPIS_BLOCK = Block(519);
			}
			~PaletteFixture()
			{
				for (std::size_t i = 0; i < entries.size(); ++i)
					*entries[i].first = saved[i];
				LAPIS_BLOCK = lapis;
			}
		} palette;
		TEST(testNewRustBlocksHaveLuantiMappings);
		TEST(testLuantiDirectionalBlockStates);
		TEST(testPottedPlantBedrockMapping);
		TEST(testWaterloggedBedrockState);
		TEST(testGenericBedrockNameMapping);
		TEST(testGenericOakSignBedrockMapping);
		TEST(testGenericOakFenceGateBedrockMapping);
		TEST(testGenericStairAliasBedrockMapping);
		TEST(testGenericChainBedrockMapping);
		TEST(testGenericSnowBlockDiffersFromSnowLayer);
		TEST(testBedrockHoppersStartUnlocked);
		TEST(testNestedBlockPropertyValuesSurviveSectionsAndPalettePacking);
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

	void testLuantiDirectionalBlockStates()
	{
		using namespace arnis;
		const auto state = [](std::uint16_t id,
								   BlockWithProperties::Properties properties) {
			return to_luanti_node(BlockWithProperties{Block(id), std::move(properties)});
		};
		const auto ladder = state(260, {{"facing", "north"}});
		UASSERT(std::string(ladder.name) == "mcl_core:ladder");
		UASSERT(ladder.param2 == 5);
		const auto east_ladder = state(260, {{"facing", "east"}});
		UASSERT(east_ladder.param2 == 3);
		const auto along_x = state(36, {{"east", "true"}, {"west", "true"}});
		UASSERT(std::string(along_x.name) == "mcl_panes:bar_flat");
		UASSERT(along_x.param2 == 0);
		const auto along_z = state(36, {{"north", "true"}, {"south", "true"}});
		UASSERT(along_z.param2 == 1);
		const auto corner = state(192, {{"north", "true"}, {"east", "true"}});
		UASSERT(std::string(corner.name) == "mcl_panes:pane_grey_flat");
		UASSERT(corner.param2 == 0);
		UASSERT(std::string(to_luanti_node(BARRIER).name) == "air");
	}

	void testPottedPlantBedrockMapping()
	{
		using namespace arnis;
		UASSERT(BEDROCK_BLOCK_STATE_VERSION == ((1 << 24) | (21 << 16) | (60 << 8) | 33));
		UASSERT(!potted_plant(FLOWER_POT));
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

	void testGenericBedrockNameMapping()
	{
		using namespace arnis;
		const Block unlisted{CONTENT_UNKNOWN};
		const auto cobweb = to_bedrock_block(unlisted, "minecraft:cobweb");
		UASSERT(cobweb.name == "minecraft:web");
		UASSERT(to_bedrock_block(RAW_GOLD_BLOCK).name == "minecraft:raw_gold_block");
		UASSERT(to_bedrock_block(LAPIS_BLOCK).name == "minecraft:lapis_block");
		const auto potted_poppy = potted_plant(unlisted, "minecraft:potted_poppy");
		UASSERT(potted_poppy && potted_poppy->name == "minecraft:poppy");
		UASSERT(!potted_plant(unlisted, "minecraft:flower_pot"));
		UASSERT(is_waterlogged(
				BlockWithProperties{unlisted}, "minecraft:tube_coral_fan"));
		UASSERT(!is_waterlogged(
				BlockWithProperties{unlisted,
						BlockWithProperties::Properties{{"waterlogged", "false"}}},
				"minecraft:tube_coral_fan"));
		const auto stairs = to_bedrock_block_with_properties(
				BlockWithProperties{
						unlisted, BlockWithProperties::Properties{{"facing", "east"},
										  {"half", "top"}}},
				"oak_stairs");
		UASSERT(stairs.name == "minecraft:oak_stairs");
		UASSERT(std::get<int>(stairs.states.at("weirdo_direction")) == 0);
		UASSERT(std::get<bool>(stairs.states.at("upside_down_bit")));
	}

	void testBedrockHoppersStartUnlocked()
	{
		using namespace arnis;
		const auto hopper = to_bedrock_block_with_properties(BlockWithProperties{HOPPER});
		UASSERT(hopper.name == "minecraft:hopper");
		UASSERT(!std::get<bool>(hopper.states.at("toggle_bit")));

		const auto locked = to_bedrock_block_with_properties(BlockWithProperties{
				HOPPER, BlockWithProperties::Properties{{"enabled", "false"}}});
		UASSERT(std::get<bool>(locked.states.at("toggle_bit")));

		const auto enabled = to_bedrock_block_with_properties(BlockWithProperties{
				HOPPER, BlockWithProperties::Properties{{"enabled", "true"}}});
		UASSERT(!std::get<bool>(enabled.states.at("toggle_bit")));
	}

	void testGenericOakSignBedrockMapping()
	{
		using namespace arnis;
		const Block unlisted{CONTENT_UNKNOWN};
		UASSERT(to_bedrock_block(unlisted, "minecraft:oak_sign").name ==
				"minecraft:standing_sign");
		const auto sign = to_bedrock_block_with_properties(
				BlockWithProperties{
						unlisted, BlockWithProperties::Properties{{"rotation", "15"}}},
				"minecraft:oak_sign");
		UASSERT(sign.name == "minecraft:standing_sign");
		UASSERT(std::get<int>(sign.states.at("ground_sign_direction")) == 15);
	}

	void testGenericOakFenceGateBedrockMapping()
	{
		using namespace arnis;
		const Block unlisted{CONTENT_UNKNOWN};
		const auto gate = to_bedrock_block_with_properties(
				BlockWithProperties{
						unlisted, BlockWithProperties::Properties{{"facing", "east"},
										  {"open", "true"}, {"in_wall", "true"}}},
				"minecraft:oak_fence_gate");
		UASSERT(gate.name == "minecraft:fence_gate");
		UASSERT(std::get<std::string>(gate.states.at("minecraft:cardinal_direction")) ==
				"east");
		UASSERT(std::get<bool>(gate.states.at("open_bit")));
		UASSERT(std::get<bool>(gate.states.at("in_wall_bit")));
	}

	void testGenericStairAliasBedrockMapping()
	{
		using namespace arnis;
		const Block unlisted{CONTENT_UNKNOWN};
		const BlockWithProperties variant{unlisted,
				BlockWithProperties::Properties{{"facing", "east"}, {"half", "top"}}};
		const auto cobblestone =
				to_bedrock_block_with_properties(variant, "minecraft:cobblestone_stairs");
		UASSERT(cobblestone.name == "minecraft:stone_stairs");
		UASSERT(std::get<int>(cobblestone.states.at("weirdo_direction")) == 0);
		UASSERT(std::get<bool>(cobblestone.states.at("upside_down_bit")));
		const auto smooth_stone =
				to_bedrock_block_with_properties(variant, "minecraft:stone_stairs");
		UASSERT(smooth_stone.name == "minecraft:normal_stone_stairs");
		UASSERT(std::get<int>(smooth_stone.states.at("weirdo_direction")) == 0);
	}

	void testGenericChainBedrockMapping()
	{
		using namespace arnis;
		const Block unlisted{CONTENT_UNKNOWN};
		const auto chain = to_bedrock_block_with_properties(
				BlockWithProperties{
						unlisted, BlockWithProperties::Properties{{"axis", "x"}}},
				"minecraft:chain");
		UASSERT(chain.name == "minecraft:iron_chain");
		UASSERT(std::get<std::string>(chain.states.at("pillar_axis")) == "x");
	}

	void testGenericSnowBlockDiffersFromSnowLayer()
	{
		using namespace arnis;
		struct SnowPaletteFixture
		{
			Block full = SNOW_BLOCK;
			Block layer = SNOW_LAYER;
			SnowPaletteFixture()
			{
				// The Rust palette has distinct IDs. Give the host-resolved blocks
				// distinct IDs here too, independent of whether a game is loaded.
				SNOW_BLOCK = Block(600);
				SNOW_LAYER = Block(601);
			}
			~SnowPaletteFixture()
			{
				SNOW_BLOCK = full;
				SNOW_LAYER = layer;
			}
		} snow_fixture;
		const Block unlisted{CONTENT_UNKNOWN};
		const auto full_block = to_bedrock_block_with_properties(
				BlockWithProperties{unlisted}, "minecraft:snow_block");
		UASSERT(full_block.name == "minecraft:snow");
		UASSERT(full_block.states.empty());
		const auto built_in_full_block =
				to_bedrock_block_with_properties(BlockWithProperties{SNOW_BLOCK});
		UASSERT(built_in_full_block.name == "minecraft:snow");
		UASSERT(built_in_full_block.states.empty());

		const auto layer = to_bedrock_block_with_properties(
				BlockWithProperties{
						unlisted, BlockWithProperties::Properties{{"layers", "4"}}},
				"minecraft:snow");
		UASSERT(layer.name == "minecraft:snow_layer");
		UASSERT(std::get<int>(layer.states.at("height")) == 3);
	}

	void testNestedBlockPropertyValuesSurviveSectionsAndPalettePacking()
	{
		using namespace arnis;
		using namespace arnis::world_editor;
		auto first = nlohmann::json{{"nested", {{"z", 7}, {"a", true}}},
				{"list", nlohmann::json::array({1, "two", false})}};
		auto same = nlohmann::json::object();
		same["list"] = nlohmann::json::array({1, "two", false});
		same["nested"] = nlohmann::json{{"a", true}, {"z", 7}};
		SectionToModify source;
		source.set_block_with_properties(
				0, 0, 0, BlockWithProperties::from_value(STONE, first));
		source.set_block_with_properties(
				1, 0, 0, BlockWithProperties::from_value(STONE, same));

		const auto stored = source.get_block_with_properties(1, 0, 0);
		UASSERT(stored.nbt_properties && *stored.nbt_properties == first);
		UASSERT(stored.property_values().empty());
		UASSERT(source.get_block_with_properties(0, 0, 0).nbt_properties ==
				stored.nbt_properties);
		const auto int_number = BlockWithProperties::intern_value(NbtValue::int_(7));
		const auto long_number = BlockWithProperties::intern_value(NbtValue::long_(7));
		const auto byte_array = BlockWithProperties::intern_value(
				NbtValue::byte_array({std::int8_t{1}, std::int8_t{2}}));
		const auto list = BlockWithProperties::intern_value(
				NbtValue::list_value({NbtValue::byte(1), NbtValue::byte(2)}));
		UASSERT(int_number != long_number);
		UASSERT(byte_array != list);
		UASSERT(canonical_property_value(NbtValue::compound_value(
						{{"z", NbtValue::int_(7)}, {"a", NbtValue::byte(1)}})) ==
				canonical_property_value(NbtValue::compound_value(
						{{"a", NbtValue::byte(1)}, {"z", NbtValue::int_(7)}})));

		WorldEditor editor;
		editor.enable_standalone_storage();
		editor.set_block_with_properties_absolute(
				BlockWithProperties::from_value(STONE, first), 4, 12, 9);
		const auto editor_state = editor.get_block_with_properties_absolute(4, 12, 9);
		UASSERT(editor_state && editor_state->nbt_properties &&
				*editor_state->nbt_properties == first);
		const std::vector<Block> replace_stone{STONE};
		editor.set_block_absolute(DIRT, 4, 12, 9, &replace_stone, nullptr);
		const auto replaced_state = editor.get_block_with_properties_absolute(4, 12, 9);
		UASSERT(replaced_state && !replaced_state->nbt_properties);

		SectionToModify merged;
		merged.merge_non_air(source, true);
		const auto merged_state = merged.get_block_with_properties(0, 0, 0);
		UASSERT(merged_state.nbt_properties && *merged_state.nbt_properties == first);
		merged.set_block(0, 0, 0, DIRT);
		UASSERT(!merged.get_block_with_properties(0, 0, 0).nbt_properties);

		const auto packed = pack_palette(source);
		UASSERT(packed.palette.size() == 2);
		UASSERT(packed.palette.front().property_value == first);
		UASSERT(packed.palette.front().property_value == same);
	}
};

static TestFmArnisBlockMapping g_test_fm_arnis_block_mapping;
