#include "test.h"

#include "mapgen/earth/arnis-cpp/src/block_definitions.h"
#include "mapgen/earth/arnis-cpp/src/land_cover/land_cover.h"
#include "mapgen/earth/arnis-cpp/src/water_depth.h"
#include "mapgen/earth/arnis-cpp/src/water_detail.h"
#include "mapgen/earth/arnis-cpp/src/water_steps.h"
#include "mapgen/earth/arnis_world_editor.h"
#include "mapgen/earth/voxelibre_placement.h"

#include <cstdint>
#include <limits>
#include <string>
#include <vector>

namespace
{
struct WaterStepBlockFixture
{
	arnis::Block stone = arnis::block_definitions::STONE;
	arnis::Block water = arnis::block_definitions::WATER;
	arnis::Block oak_log = arnis::block_definitions::OAK_LOG;
	arnis::Block oak_leaves = arnis::block_definitions::OAK_LEAVES;

	WaterStepBlockFixture()
	{
		arnis::block_definitions::STONE = arnis::Block(320);
		arnis::block_definitions::WATER = arnis::Block(321);
		arnis::block_definitions::OAK_LOG = arnis::Block(322);
		arnis::block_definitions::OAK_LEAVES = arnis::Block(323);
	}

	~WaterStepBlockFixture()
	{
		arnis::block_definitions::STONE = stone;
		arnis::block_definitions::WATER = water;
		arnis::block_definitions::OAK_LOG = oak_log;
		arnis::block_definitions::OAK_LEAVES = oak_leaves;
	}
};
} // namespace

class TestFmArnisWaterSteps : public TestBase
{
public:
	TestFmArnisWaterSteps() { TestManager::registerTestModule(this); }
	const char *getName() override { return "TestFmArnisWaterSteps"; }
	void runTests(IGameDef *) override
	{
		TEST(testOneBlockWaterDropGetsVanillaRunUp);
		TEST(testBoundedTreeClearStopsAtItsColumns);
		TEST(testCarveDepthEstimateHandlesNoWater);
		TEST(testCarveDepthEstimateCapsLargeWaterBodies);
		TEST(testBoreCrossingsReserveConfiguredCarveDepth);
		TEST(testScaledWaterCrossingParsesLayerLikeRust);
		TEST(testVoxeLibreRootedSeagrass);
		TEST(testVoxeLibreRootedOceanAndStates);
	}

	void testVoxeLibreRootedOceanAndStates()
	{
		using namespace arnis;
		WorldEditor editor;
		editor.enable_standalone_storage();
		voxelibre::RootedOcean nodes;
		nodes.enabled = nodes.kelp_enabled = true;
		nodes.water = Block{371};
		nodes.substrates[0] = Block{372};
		nodes.kelp[0] = Block{373};
		editor.set_block_absolute(nodes.substrates[0], 0, 0, 0);
		for (int y = 1; y <= 3; ++y)
			editor.set_block_absolute(nodes.water, 0, y, 0);
		voxelibre::place_kelp(editor, nodes, 0, 0, 0, 3);
		UASSERT(editor.get_block_absolute(0, 0, 0)->id() == 373);
		UASSERT(editor.get_block_absolute(0, 0, 0)->getParam2() == 48);
		UASSERT(editor.get_block_absolute(0, 3, 0)->id() == 371);
		editor.set_block_absolute(Block{374}, 1, 0, 0);
		editor.set_block_absolute(nodes.water, 1, 1, 0);
		UASSERT(voxelibre::place_rooted(editor, Block{375}, 4, nodes.water, 1, 0, 0));
		UASSERT(editor.get_block_absolute(1, 0, 0)->getParam2() == 4);
		UASSERT(editor.get_block_absolute(1, 1, 0)->id() == 371);
		auto slab = voxelibre::node_state("mcl_stairs:slab_wood", 0, {{"type", "top"}});
		UASSERT(slab && slab->first == "mcl_stairs:slab_wood_top");
		auto door = voxelibre::node_state("mcl_doors:birch_door_b_1", 0,
				{{"half", "upper"}, {"hinge", "right"}, {"facing", "east"},
						{"open", "true"}});
		UASSERT(door && door->first == "mcl_doors:birch_door_t_4" && door->second == 0);
		auto trap = voxelibre::node_state("mcl_doors:birch_trapdoor", 0,
				{{"half", "top"}, {"facing", "south"}, {"open", "true"}});
		UASSERT(trap && trap->first == "mcl_doors:birch_trapdoor_open" &&
				trap->second == 22);
		auto bed = voxelibre::node_state(
				"mcl_beds:bed_white_bottom", 0, {{"part", "head"}, {"facing", "west"}});
		UASSERT(bed && bed->first == "mcl_beds:bed_white_top" && bed->second == 3);
		auto banner = voxelibre::node_state(
				"mcl_banners:hanging_banner", 0, {{"facing", "north"}});
		UASSERT(banner && banner->second == 5);
	}

	void testVoxeLibreRootedSeagrass()
	{
		using namespace arnis;
		voxelibre::RootedSeagrass nodes;
		nodes.enabled = true;
		nodes.water = Block{321};
		WorldEditor editor;
		editor.enable_standalone_storage();
		for (int i = 0; i < 4; ++i) {
			nodes.substrates[i] = Block{static_cast<content_t>(330 + i)};
			nodes.plants[i] = Block{static_cast<content_t>(340 + i)};
			editor.set_block_absolute(nodes.substrates[i], i, 0, 0);
			editor.set_block_absolute(nodes.water, i, 1, 0);
			voxelibre::place_seagrass(editor, nodes, i, 0, 0);
			const auto bed = editor.get_block_absolute(i, 0, 0);
			const auto water = editor.get_block_absolute(i, 1, 0);
			UASSERT(bed && bed->id() == nodes.plants[i].id() && bed->getParam2() == 3);
			UASSERT(water && water->id() == nodes.water.id());
		}
		editor.set_block_absolute(nodes.substrates[0], 5, 0, 0);
		voxelibre::place_seagrass(editor, nodes, 5, 0, 0);
		UASSERT(editor.get_block_absolute(5, 0, 0)->id() == nodes.substrates[0].id());
		editor.set_block_absolute(Block{350}, 6, 0, 0);
		editor.set_block_absolute(nodes.water, 6, 1, 0);
		voxelibre::place_seagrass(editor, nodes, 6, 0, 0);
		UASSERT(editor.get_block_absolute(6, 0, 0)->id() == 350);
	}

	void testScaledWaterCrossingParsesLayerLikeRust()
	{
		using namespace arnis;
		tags_t tags;
		tags.emplace("layer", " 0 ");
		UASSERT(water_detail::drowns_crossing(WaterDetail::Scaled, 0.5, tags));
		tags["layer"] = "1";
		UASSERT(!water_detail::drowns_crossing(WaterDetail::Scaled, 0.5, tags));
		tags.clear();
		UASSERT(!water_detail::drowns_crossing(
				WaterDetail::Scaled, std::numeric_limits<double>::quiet_NaN(), tags));
	}

	void testOneBlockWaterDropGetsVanillaRunUp()
	{
		using namespace arnis;
		WaterStepBlockFixture block_fixture;
		WorldEditor editor;
		editor.enable_standalone_storage();
		// Keep a dry perimeter around the fixture so these columns are interior
		// to the standalone area's bounds, just like the Rust water-step test.
		for (int x = -1; x <= 41; ++x) {
			editor.set_block_absolute(block_definitions::STONE, x, 0, -1);
			editor.set_block_absolute(block_definitions::STONE, x, 0, 5);
		}
		for (int z = 0; z <= 4; ++z) {
			editor.set_block_absolute(block_definitions::STONE, -1, 0, z);
			editor.set_block_absolute(block_definitions::STONE, 41, 0, z);
		}
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
		UTEST(level(19, 30) == 7, "expected level 7, got %d", level(19, 30));
		UASSERT(level(13, 30) == 1);
		UASSERT(level(12, 30) == 0);
		UASSERT(level(20, 29) == 0);
	}

	void testBoundedTreeClearStopsAtItsColumns()
	{
		using namespace arnis;
		WaterStepBlockFixture block_fixture;
		WorldEditor editor;
		editor.enable_standalone_storage();
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

	void testCarveDepthEstimateHandlesNoWater()
	{
		const std::vector<std::vector<std::uint8_t>> grid(
				16, std::vector<std::uint8_t>(16, arnis::land_cover::LC_GRASSLAND));
		UASSERT(arnis::water_depth::estimate_max_carve_depth(grid, 16, 16) == 0);
	}

	void testCarveDepthEstimateCapsLargeWaterBodies()
	{
		const std::vector<std::vector<std::uint8_t>> grid(
				32, std::vector<std::uint8_t>(32, arnis::land_cover::LC_WATER));
		UASSERT(arnis::water_depth::estimate_max_carve_depth(grid, 32, 32) == 6);
	}

	void testBoreCrossingsReserveConfiguredCarveDepth()
	{
		using namespace arnis;
		struct ResetCrossingCarve
		{
			~ResetCrossingCarve()
			{
				water_depth::set_crossing_carve({0, WaterDetail::Default, 1.0});
			}
		} reset;
		std::vector<std::vector<std::uint8_t>> grid(
				3, std::vector<std::uint8_t>(3, land_cover::LC_GRASSLAND));
		grid[1][1] = land_cover::LC_WATER;
		const std::vector<std::pair<std::size_t, std::size_t>> cells{{1, 1}};
		const auto height = [](std::size_t, std::size_t) { return 0.0; };

		water_depth::set_crossing_carve({0, WaterDetail::Default, 1.0});
		auto crossings = water_depth::bore_crossings(grid, 3, 3, cells, height);
		UASSERT(crossings.size() == 1);
		UASSERT(crossings.front().depth == 0);

		water_depth::set_crossing_carve(
				{water_depth::MAX_WATER_DEPTH, WaterDetail::Default, 1.0});
		crossings = water_depth::bore_crossings(grid, 3, 3, cells, height);
		UASSERT(crossings.size() == 1);
		UASSERT(crossings.front().depth == water_depth::MAX_WATER_DEPTH);
	}
};

static TestFmArnisWaterSteps g_test_fm_arnis_water_steps;
