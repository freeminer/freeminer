#include "test.h"

#include "mapgen/earth/arnis-cpp/src/assets_root.h"
#include "mapgen/earth/arnis-cpp/src/block_definitions.h"
#include "mapgen/earth/arnis-cpp/src/element_processing/tree.h"
#include "mapgen/earth/arnis-cpp/src/floodfill_cache.h"
#include "mapgen/earth/arnis-cpp/src/trees/region.h"
#include "mapgen/earth/arnis-cpp/src/trees/mapped.h"
#include "mapgen/earth/arnis-cpp/src/trees/schematic.h"
#include "mapgen/earth/arnis-cpp/src/trees/tree_library.h"
#include "mapgen/earth/arnis-cpp/src/trees/tree_pack.h"
#include "mapgen/earth/arnis-cpp/src/structures/schem_decoder.h"
#include "mapgen/earth/arnis_ground.h"
#include "mapgen/earth/arnis_world_editor.h"

#include <array>
#include <algorithm>
#include <cmath>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <initializer_list>
#include <limits>
#include <nlohmann/json.hpp>
#include <memory>
#include <optional>
#include <set>
#include <string>
#include <unordered_map>
#include <vector>

class TestFmArnisTreePacks : public TestBase
{
public:
	TestFmArnisTreePacks() { TestManager::registerTestModule(this); }
	const char *getName() override { return "TestFmArnisTreePacks"; }
	void runTests(IGameDef *) override
	{
		TEST(testSchematicUsesBlockDefaultProperties);
		TEST(testContributedTreePackAssets);
		TEST(testTreePackRealmMapping);
		TEST(testTreeSizeTiers);
		TEST(testTreeConnectivityDenseAndSparse);
		TEST(testTreePackCrownCapsAndSlotSpacing);
		TEST(testMappedPackTreesKeepPositionAndAreNeverThinned);
		TEST(testMappedPackTreesStandInEveryEcoregionMix);
		TEST(testTreeRowPositionsMatchRust);
		TEST(testMappedTreeTagsMatchRust);
		TEST(testPackTreeHeightFollowsRequestedCanopyHeight);
		TEST(testTreeSlotClaimIsUnique);
		TEST(testScatteredTreesRespectSealedSurface);
		TEST(testMappedTreesMayUseSealedSurface);
		TEST(testMappedTreesMayUsePavingButNeverWater);
		TEST(testMappedTreeTypicalHeightUsesSeenCanopyMedian);
		TEST(testCanopyMeasuredMatchesCanopyCellAvailability);
		TEST(testTreeProbeRoundingMatchesRustSaturatingCast);
	}

	void testTreeProbeRoundingMatchesRustSaturatingCast()
	{
		UASSERTEQ(int, arnis::crown_probe_blocks(0.0), 1);
		UASSERTEQ(int, arnis::crown_probe_blocks(0.75), 2);
		UASSERTEQ(int,
				arnis::crown_probe_blocks(std::numeric_limits<double>::quiet_NaN()), 1);
		UASSERTEQ(int, arnis::crown_probe_blocks(std::numeric_limits<double>::max()),
				std::numeric_limits<int>::max());
	}

	void testSchematicUsesBlockDefaultProperties()
	{
		// Rust maps this structure-only palette alias to smooth sandstone too.
		UASSERT(arnis::structures::resolve_schem_block("minecraft:cut_sandstone") ==
				arnis::SMOOTH_SANDSTONE);

		const auto trapdoor = arnis::structures::resolve_schem_block_with_properties(
				"minecraft:oak_trapdoor");
		const auto expected = arnis::default_block_properties(trapdoor.block);
		UASSERT(!expected.empty());
		UASSERT(trapdoor.property_values() == expected);
		UASSERT(trapdoor.property_values().at("facing") == "north");
		UASSERT(trapdoor.property_values().at("open") == "true");
		UASSERT(trapdoor.property_values().at("half") == "top");

		// Explicit palette states take precedence over block defaults just as in
		// Rust's make_palette_item(stored_props.or_else(Block::properties)).
		const auto explicit_state =
				arnis::structures::resolve_schem_block_with_properties(
						"minecraft:oak_trapdoor[facing=south,open=false,half=bottom]");
		UASSERT(explicit_state.property_values().at("facing") == "south");
		UASSERT(explicit_state.property_values().at("open") == "false");
		UASSERT(explicit_state.property_values().at("half") == "bottom");
	}

	void testCanopyMeasuredMatchesCanopyCellAvailability()
	{
		arnis::Ground ground = arnis::Ground::new_flat(0);
		ground.set_world_dims(4, 4);
		std::vector<std::uint8_t> canopy(16, arnis::canopy::CANOPY_NODATA);
		canopy[1] = 9;
		ground.set_canopy_data(canopy, 4, 4);

		const std::array<std::array<int, 3>, 4> probes{
				{{{0, 0, 2}}, {{2, 0, 2}}, {{0, 3, 1}}, {{0, 0, 0}}}};
		for (const auto &probe : probes) {
			const arnis::XZPoint origin{probe[0], probe[1]};
			UASSERT(ground.canopy_measured(origin, probe[2]) ==
					ground.canopy_cell(origin, probe[2]).has_value());
		}
		UASSERT(!arnis::Ground::new_flat(0).canopy_measured({0, 0}, 2));
	}

	void testMappedTreeTypicalHeightUsesSeenCanopyMedian()
	{
		std::vector<arnis::ProcessedElement> trees;
		for (std::uint64_t i = 0; i < 30; ++i) {
			arnis::ProcessedNode node{};
			node.id = i;
			node.tags.emplace("natural", "tree");
			node.x = static_cast<int>(i) * 10;
			node.z = 0;
			trees.emplace_back(node);
		}

		auto trunks = arnis::trees::mapped::MappedTrunks::collect(trees, 1.0);
		trunks.measure_typical_height([](int x, int) -> std::optional<std::uint8_t> {
			return x % 20 == 0 ? std::optional<std::uint8_t>(
										 static_cast<std::uint8_t>(4 + x / 20))
							   : std::nullopt;
		});
		UASSERT(trunks.typical_height_m() == 11.0);

		auto sparse = arnis::trees::mapped::MappedTrunks::collect(trees, 1.0);
		sparse.measure_typical_height([](int x, int) -> std::optional<std::uint8_t> {
			return x < 50 ? std::optional<std::uint8_t>(15) : std::nullopt;
		});
		UASSERT(!sparse.typical_height_m());
	}

	void testTreeConnectivityDenseAndSparse()
	{
		// Standalone unit tests do not register game nodes; give leaves a
		// distinct ID so they are not mistaken for the default-valued logs.
		struct RestoreLeaves
		{
			arnis::Block original = arnis::block_definitions::OAK_LEAVES;
			~RestoreLeaves() { arnis::block_definitions::OAK_LEAVES = original; }
		} restore;
		arnis::block_definitions::OAK_LEAVES = arnis::Block(123);
		arnis::structures::SchemDocument source;
		source.width = 20;
		source.height = 40;
		source.length = 40;
		source.voxels = {{1, 1, 1, "minecraft:oak_log", {}, {}},
				{3, 3, 3, "minecraft:oak_leaves", {}, {}},
				{5, 5, 5, "minecraft:oak_leaves", {}, {}},
				{15, 15, 15, "minecraft:oak_leaves", {}, {}}};
		const auto dense = arnis::trees::tree_only(source);
		source.width = 2000; // Exceeds the dense-index limit.
		const auto sparse = arnis::trees::tree_only(source);
		UASSERTEQ(std::size_t, dense.voxels.size(), 3);
		UASSERTEQ(std::size_t, sparse.voxels.size(), dense.voxels.size());
		for (std::size_t i = 0; i < dense.voxels.size(); ++i) {
			UASSERTEQ(int, dense.voxels[i].x, sparse.voxels[i].x);
			UASSERTEQ(int, dense.voxels[i].y, sparse.voxels[i].y);
			UASSERTEQ(int, dense.voxels[i].z, sparse.voxels[i].z);
			UASSERT(dense.voxels[i].block == sparse.voxels[i].block);
		}
	}

	void testTreePackRealmMapping()
	{
		using arnis::trees::realm_for_latlon;
		UASSERT(realm_for_latlon(40.71, -74.01) == "ena");
		UASSERT(realm_for_latlon(25.76, -80.19) == "fl");
		UASSERT(realm_for_latlon(34.05, -118.24) == "wna");
		UASSERT(realm_for_latlon(51.51, -0.13) == "eur");
		UASSERT(realm_for_latlon(85.0, 0.0) == "vanilla-plus");
	}

	void testTreeSizeTiers()
	{
		using arnis::trees::size_for_height;
		using arnis::trees::SizeFilter;
		using arnis::trees::TreeSize;
		UASSERT(size_for_height(6) == TreeSize::Small);
		UASSERT(size_for_height(7) == TreeSize::Medium);
		UASSERT(size_for_height(13) == TreeSize::Big);
		UASSERT(size_for_height(21) == TreeSize::Tall);
		UASSERT(size_for_height(35) == TreeSize::Giant);
		const auto defaults = SizeFilter{};
		UASSERT(defaults.allows(TreeSize::Small));
		UASSERT(defaults.allows(TreeSize::Medium));
		UASSERT(defaults.allows(TreeSize::Big));
		UASSERT(defaults.allows(TreeSize::Tall));
		UASSERT(defaults.allows(TreeSize::Giant));
	}

	void testTreePackCrownCapsAndSlotSpacing()
	{
		using namespace arnis::trees;
		const auto source =
				TreePackSource::embedded("eur", arnis::assets::path("tree-packs"));
		const auto capped = RegionSelector::load(
				source, 1.0, -62, SizeFilter::up_to(TreeSize::Small));
		const auto full = RegionSelector::load(source, 1.0, -62);
		UTEST(capped.has_value(), "%s", "Failed to load capped European tree pack");
		UTEST(full.has_value(), "%s", "Failed to load European tree pack");
		if (!capped || !full)
			return;
		UASSERT(capped->crown_columns(25) == capped->crown_columns(6));
		UASSERT(full->crown_columns(25) > full->crown_columns(6));

		const auto scaled = RegionSelector::load(source, 0.5, -62);
		UTEST(scaled.has_value(), "%s", "Failed to load scaled European tree pack");
		if (!scaled)
			return;
		UASSERT(scaled->slot_spacing(true) == 4);
		UASSERT(scaled->slot_spacing(false) == scaled->base_spacing());
	}

	void testMappedPackTreesKeepPositionAndAreNeverThinned()
	{
		using namespace arnis::trees;
		const auto source =
				TreePackSource::embedded("eur", arnis::assets::path("tree-packs"));
		const auto library = RegionSelector::load(source, 1.0, -62);
		UTEST(library.has_value(), "%s", "Failed to load European tree pack");
		if (!library)
			return;

		for (int k = 0; k < 2000; ++k) {
			const int x = k * 37 % 3001 - 1500;
			const int z = k * 91 % 2999 - 1500;
			const auto selected =
					library->pick_mapped(x, z, Habitat::Lowland, 0, MappedRequest{});
			UASSERT(selected.has_value());
			if (!selected)
				continue;
			UASSERT(selected->x == x && selected->z == z);
			UASSERT(library->schematic(selected->schematic_index) != nullptr);
		}
	}

	void testMappedPackTreesStandInEveryEcoregionMix()
	{
		using namespace arnis::trees;
		struct Case
		{
			const char *pack;
			std::uint16_t ecoregion;
		};
		// These exercise cross-pack communities: the local realm pack can have
		// no usable species of the requested leaf type, but a mapped tree must
		// still resolve a model from its ecoregion's complete mix.
		for (const auto &[pack, eco_id] :
				std::array<Case, 3>{{{"eur", 795}, {"ena", 330}, {"sam", 576}}}) {
			const auto source =
					TreePackSource::embedded(pack, arnis::assets::path("tree-packs"));
			const auto library = RegionSelector::load(source, 1.0, -62, SizeFilter{},
					false, 0.0, std::vector<std::uint16_t>{eco_id});
			UTEST(library.has_value(), "Failed to load tree pack %s for ecoregion %u",
					pack, static_cast<unsigned>(eco_id));
			if (!library)
				continue;
			const auto eco = arnis::ecoregion::lookup(eco_id);
			UTEST(eco.has_value(), "Missing ecoregion data for %u",
					static_cast<unsigned>(eco_id));
			if (!eco)
				continue;
			for (const auto conifer : {std::optional<bool>{}, std::optional<bool>{true},
						 std::optional<bool>{false}}) {
				for (int k = 0; k < 500; ++k) {
					const int x = k * 37 % 3001 - 1500;
					const int z = k * 91 % 2999 - 1500;
					MappedRequest request;
					request.eco = eco;
					request.conifer = conifer;
					const auto selected =
							library->pick_mapped(x, z, Habitat::Lowland, 0, request);
					UTEST(selected.has_value(),
							"No mapped tree selected for %s ecoregion %u conifer=%d",
							pack, static_cast<unsigned>(eco_id),
							conifer ? (*conifer ? 1 : 0) : -1);
					if (selected)
						UASSERT(selected->x == x && selected->z == z);
				}
			}
		}
	}

	void testTreeRowPositionsMatchRust()
	{
		using namespace arnis::trees::mapped;
		std::unordered_map<std::string, std::string> row_tags{{"natural", "tree_row"}};
		UASSERT(is_tree_row(row_tags));
		for (const char *key : {"building", "building:part", "highway", "landuse"}) {
			row_tags.emplace(key, "yes");
			UASSERT(!is_tree_row(row_tags));
			row_tags.erase(key);
		}

		const std::vector<std::pair<int, int>> straight{{0, 0}, {40, 0}};
		UASSERT((tree_row_positions(straight, 1.0) ==
				 std::vector<std::pair<int, int>>{
						 {0, 0}, {8, 0}, {16, 0}, {24, 0}, {32, 0}, {40, 0}}));

		// Spacing follows the polyline around a corner, not the endpoint chord.
		const std::vector<std::pair<int, int>> bent{{0, 0}, {12, 0}, {12, 12}};
		const auto bent_positions = tree_row_positions(bent, 1.0);
		UASSERT(bent_positions.size() == 4);
		if (!bent_positions.empty()) {
			UASSERT((bent_positions.front() == std::pair<int, int>{0, 0}));
			UASSERT((bent_positions.back() == std::pair<int, int>{12, 12}));
		}
		UASSERT(tree_row_positions(straight, 0.5).size() == 11);
		UASSERT((tree_row_positions({{3, 4}}, 1.0) ==
				 std::vector<std::pair<int, int>>{{3, 4}}));
	}

	void testMappedTreeTagsMatchRust()
	{
		using arnis::TreeType;
		using arnis::trees::mapped::MappedTree;
		auto from = [](std::initializer_list<std::pair<const std::string, std::string>>
									pairs) {
			return MappedTree::from_tags(
					std::unordered_map<std::string, std::string>(pairs), 1);
		};

		auto robinia = from({{"species", "Robinia pseudoacacia"}});
		UASSERT(robinia.kind == TreeType::Oak || robinia.kind == TreeType::TallOak);
		UASSERT(robinia.genus == std::optional<std::string>("Robinia"));
		UASSERT(robinia.conifer == std::optional<bool>(false));

		UASSERT(from({{"genus", "Tilia"}}).genus == std::optional<std::string>("Tilia"));
		UASSERT(from({{"species", "tilia cordata 'Greenspire'"}}).genus ==
				std::optional<std::string>("Tilia"));
		UASSERT(from({{"genus:wikidata", "Q12004"}}).genus ==
				std::optional<std::string>("Betula"));
		UASSERT(!from({{"leaf_type", "broadleaved"}}).genus);

		const auto yew = from({{"species", "Taxus baccata"}});
		UASSERT(yew.kind == TreeType::Spruce);
		UASSERT(yew.conifer == std::optional<bool>(true));
		const auto pine = from({{"species", "Pinus nigra"}});
		UASSERT(pine.kind == TreeType::Pine);
		const auto tagged_conifer =
				from({{"species", "Foo bar"}, {"leaf_type", "needleleaved"}});
		UASSERT(tagged_conifer.kind == TreeType::Spruce ||
				tagged_conifer.kind == TreeType::Pine);
		UASSERT(tagged_conifer.conifer == std::optional<bool>(true));
		UASSERT(!from({}).conifer);

		UASSERT(from({{"height", "12"}}).height_m == 12.0);
		UASSERT(from({{"height", "12.5 m"}}).height_m == 12.5);
		UASSERT(!from({{"height", "0"}}).height_m);
		UASSERT(!from({{"height", "tall"}}).height_m);
	}

	void testPackTreeHeightFollowsRequestedCanopyHeight()
	{
		using namespace arnis::trees;
		const auto source =
				TreePackSource::embedded("eur", arnis::assets::path("tree-packs"));
		const auto library = RegionSelector::load(source, 1.0, -62);
		UTEST(library.has_value(), "%s", "Failed to load European tree pack");
		if (!library)
			return;

		int previous_median = 0;
		std::size_t expected_count = 0;
		for (const int wanted : {5, 10, 15, 20, 25}) {
			SlotRequest request;
			request.want_height = wanted;
			request.density_decided = true;
			std::vector<int> heights;
			for (int k = 0; k < 2000; ++k) {
				const auto selected =
						library->pick_slot(k * 3, k * 7, Habitat::Lowland, 0, request);
				if (!selected)
					continue;
				const auto *schematic = library->schematic(selected->schematic_index);
				UASSERT(schematic != nullptr);
				if (schematic)
					heights.push_back(schematic->height);
			}
			UASSERT(heights.size() > 1000);
			if (heights.empty())
				continue;
			if (!expected_count)
				expected_count = heights.size();
			else
				UASSERT(heights.size() == expected_count);

			const int tolerance =
					std::max(2, static_cast<int>(std::lround(wanted * .15)));
			const auto close = std::count_if(heights.begin(), heights.end(),
					[&](int h) { return std::abs(h - wanted) <= tolerance; });
			UASSERT(close * 10 > heights.size() * 8);
			std::sort(heights.begin(), heights.end());
			const int median = heights[heights.size() / 2];
			UASSERT(median > previous_median);
			previous_median = median;
		}
	}

	void testTreeSlotClaimIsUnique()
	{
		// Rust's regional tree placement claims the snapped trunk slot before
		// stamping a schematic. Multiple cells in that slot must not graft a
		// second tree onto the first one.
		arnis::WorldEditor editor;
		UASSERT(editor.claim_tree_slot(12, -7));
		UASSERT(!editor.claim_tree_slot(12, -7));
		UASSERT(editor.claim_tree_slot(13, -7));
	}

	void testScatteredTreesRespectSealedSurface()
	{
		using namespace arnis;
		WorldEditor editor;
		const XZBBox bounds(0, 0, 31, 31);
		auto sealed = std::make_shared<CoordinateBitmap>(bounds);
		sealed->set(16, 16);
		editor.set_sealed_surface(sealed);

		Tree::create(editor, Coord{16, 0, 16});
		for (int y = 0; y < 40; ++y)
			UASSERT(!editor.block_exists_absolute(16, y, 16));

		Tree::create(editor, Coord{20, 0, 20});
		bool planted = false;
		for (int y = 0; y < 40; ++y)
			planted = planted || editor.block_exists_absolute(20, y, 20);
		UASSERT(planted);
	}

	void testMappedTreesMayUseSealedSurface()
	{
		using namespace arnis;
		WorldEditor editor;
		const XZBBox bounds(0, 0, 31, 31);
		auto sealed = std::make_shared<CoordinateBitmap>(bounds);
		sealed->set(16, 16);
		editor.set_sealed_surface(sealed);

		Tree::create_of_type(editor, Coord{16, 0, 16}, TreeType::Oak, nullptr, nullptr,
				true, std::nullopt, false, false, false);
		bool planted = false;
		for (int y = 0; y < 40; ++y)
			planted = planted || editor.block_exists_absolute(16, y, 16);
		UASSERT(planted);
	}

	void testMappedTreesMayUsePavingButNeverWater()
	{
		using namespace arnis;
		auto place = [](WorldEditor &editor, bool allow_on_paved) {
			Tree::create_of_type(editor, Coord{30, 1, 30}, TreeType::Oak, nullptr,
					nullptr, allow_on_paved, std::nullopt, false, false, false);
		};
		auto has_trunk = [](WorldEditor &editor) {
			return editor.check_for_block(
					30, 2, 30, std::optional<std::vector<Block>>({OAK_LOG}));
		};

		WorldEditor scattered;
		scattered.set_block_absolute(SMOOTH_STONE, 30, 0, 30);
		place(scattered, false);
		UASSERT(!has_trunk(scattered));

		WorldEditor mapped;
		mapped.set_block_absolute(SMOOTH_STONE, 30, 0, 30);
		place(mapped, true);
		UASSERT(has_trunk(mapped));

		WorldEditor underwater;
		underwater.set_block_absolute(WATER, 30, 0, 30);
		place(underwater, true);
		UASSERT(!has_trunk(underwater));
	}

	void testContributedTreePackAssets()
	{
		const auto root = arnis::assets::path("tree-packs");
		UASSERT(std::filesystem::is_directory(root));

		std::set<std::string> listed;
		for (const auto &entry : std::filesystem::recursive_directory_iterator(root)) {
			if (!entry.is_regular_file() || entry.path().filename() != "region.json")
				continue;
			std::ifstream in(entry.path());
			UASSERT(in.good());
			nlohmann::json manifest;
			try {
				in >> manifest;
			} catch (const std::exception &e) {
				UTEST(false, "Invalid tree manifest %s: %s",
						entry.path().string().c_str(), e.what());
			}
			UASSERT(manifest.is_object());
			UASSERT(manifest.contains("communities") &&
					manifest["communities"].is_array());
			for (const auto &community : manifest["communities"]) {
				UASSERT(community.is_object());
				UASSERT(community.contains("species") && community["species"].is_array());
				for (const auto &species : community["species"]) {
					UASSERT(species.is_object());
					for (const auto *field : {"w1", "w2", "w3"}) {
						if (!species.contains(field))
							continue;
						UASSERT(species[field].is_array());
						for (const auto &relative : species[field]) {
							UASSERT(relative.is_string());
							const auto file = entry.path().parent_path() /
											  relative.get<std::string>();
							const auto relative_to_root =
									file.lexically_relative(root).generic_string();
							if (!listed.insert(relative_to_root).second)
								continue;
							UTEST(std::filesystem::is_regular_file(file),
									"Tree pack lists missing schematic %s",
									file.string().c_str());
							arnis::trees::Schematic schematic;
							try {
								schematic = arnis::trees::load_schem(file);
							} catch (const std::exception &e) {
								UTEST(false, "Invalid tree schematic %s: %s",
										file.string().c_str(), e.what());
							}
							if (relative_to_root.find("/contrib/") != std::string::npos) {
								UTEST(arnis::trees::has_leaves(schematic),
										"Contributed schematic has no leaves: %s",
										file.string().c_str());
								UTEST(schematic.height >= 2,
										"Contributed schematic is too short: %s",
										file.string().c_str());
							}
						}
					}
				}
			}
		}

		std::size_t contributed_files = 0;
		for (const auto &entry : std::filesystem::recursive_directory_iterator(root)) {
			if (!entry.is_regular_file())
				continue;
			const auto relative = entry.path().lexically_relative(root).generic_string();
			if (relative.find("/contrib/") == std::string::npos)
				continue;
			++contributed_files;
			UTEST(listed.contains(relative), "Unlisted contributed tree asset: %s",
					entry.path().string().c_str());
		}
		UASSERT(listed.size() > 2400);
		UASSERT(contributed_files > 0);
	}
};

static TestFmArnisTreePacks g_test_fm_arnis_tree_packs;
