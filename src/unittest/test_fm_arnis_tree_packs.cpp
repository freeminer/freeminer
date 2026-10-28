#include "test.h"

#include "mapgen/earth/arnis-cpp/src/assets_root.h"
#include "mapgen/earth/arnis-cpp/src/block_definitions.h"
#include "mapgen/earth/arnis-cpp/src/element_processing/tree.h"
#include "mapgen/earth/arnis-cpp/src/floodfill_cache.h"
#include "mapgen/earth/arnis-cpp/src/trees/region.h"
#include "mapgen/earth/arnis-cpp/src/trees/schematic.h"
#include "mapgen/earth/arnis-cpp/src/trees/tree_library.h"
#include "mapgen/earth/arnis-cpp/src/trees/tree_pack.h"
#include "mapgen/earth/arnis_world_editor.h"

#include <filesystem>
#include <fstream>
#include <nlohmann/json.hpp>
#include <memory>
#include <set>
#include <string>

class TestFmArnisTreePacks : public TestBase
{
public:
	TestFmArnisTreePacks() { TestManager::registerTestModule(this); }
	const char *getName() override { return "TestFmArnisTreePacks"; }
	void runTests(IGameDef *) override
	{
		TEST(testContributedTreePackAssets);
		TEST(testTreePackRealmMapping);
		TEST(testTreeSizeTiers);
		TEST(testTreePackCrownCapsAndSlotSpacing);
		TEST(testScatteredTreesRespectSealedSurface);
		TEST(testMappedTreesMayUseSealedSurface);
		TEST(testMappedTreesMayUsePavingButNeverWater);
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
