// SPDX-License-Identifier: LGPL-2.1-or-later
#include "test.h"
#include "content/fm_mod_dependencies.h"
#include "filesys.h"
#include <fstream>

class TestFmModDependencies : public TestBase
{
public:
	TestFmModDependencies() { TestManager::registerTestModule(this); }
	const char *getName() override { return "TestFmModDependencies"; }
	void runTests(IGameDef *) override
	{
		TEST(testSelection);
		TEST(testOptionalSelection);
	}

	void testSelection()
	{
		const std::string root = getTestTempDirectory() + DIR_DELIM "fm_mod_deps";
		const std::string mods = root + DIR_DELIM "mods";
		const std::string world = root + DIR_DELIM "world.mt";
		fs::RecursiveDelete(root);
		fs::CreateAllDirs(mods + DIR_DELIM "sponge");
		fs::CreateAllDirs(mods + DIR_DELIM "other");
		std::ofstream(mods + DIR_DELIM "sponge" DIR_DELIM "init.lua");
		std::ofstream(mods + DIR_DELIM "other" DIR_DELIM "init.lua");
		{
			std::ofstream(mods + DIR_DELIM "sponge" DIR_DELIM "mod.conf")
					<< "name = sponge\ndepends = other\n";
		}
		{
			std::ofstream(mods + DIR_DELIM "other" DIR_DELIM "mod.conf")
					<< "name = other\ndepends = sponge\n";
		}
		{
			std::ofstream(world) << "gameid = test\n";
		}
		std::unordered_map<std::string, std::string> paths = {{"mods", mods}};
		ModSpec game;
		game.name = "game";
		game.depends.insert("sponge");
		game.optdepends.insert("optional");
		const std::vector<ModSpec> selected = {game};
		auto found = fm_select_dependency_mods(selected, paths, world);
		UASSERTEQ(size_t, found.size(), 2);
		UASSERTEQ(std::string, found[0].name, "sponge");
		UASSERTEQ(std::string, found[1].name, "other");
		{
			std::ofstream(world) << "load_mod_sponge = false\n";
		}
		UASSERT(fm_select_dependency_mods(selected, paths, world).empty());
		fs::CreateAllDirs(mods + DIR_DELIM "duplicate");
		std::ofstream(mods + DIR_DELIM "duplicate" DIR_DELIM "init.lua");
		{
			std::ofstream(mods + DIR_DELIM "duplicate" DIR_DELIM "mod.conf")
					<< "name = sponge\n";
		}
		{
			std::ofstream(world) << "gameid = test\n";
		}
		UASSERT(fm_select_dependency_mods(selected, paths, world).empty());
		{
			std::ofstream(world) << "load_mod_sponge = mods/sponge\n";
		}
		UASSERTEQ(size_t, fm_select_dependency_mods(selected, paths, world).size(), 2);
	}

	void testOptionalSelection()
	{
		const std::string root = getTestTempDirectory() + DIR_DELIM "fm_optional_deps";
		const std::string mods = root + DIR_DELIM "mods";
		const std::string world = root + DIR_DELIM "world.mt";
		fs::RecursiveDelete(root);
		fs::CreateAllDirs(mods + DIR_DELIM "optional");
		fs::CreateAllDirs(mods + DIR_DELIM "child");
		std::ofstream(mods + DIR_DELIM "optional" DIR_DELIM "init.lua");
		std::ofstream(mods + DIR_DELIM "child" DIR_DELIM "init.lua");
		{
			std::ofstream(mods + DIR_DELIM "optional" DIR_DELIM "mod.conf")
					<< "name = optional\ndepends = child\noptional_depends = missing\n";
		}
		{
			std::ofstream(mods + DIR_DELIM "child" DIR_DELIM "mod.conf")
					<< "name = child\n";
		}
		{
			std::ofstream(world) << "gameid = test\n";
		}
		std::unordered_map<std::string, std::string> paths = {{"mods", mods}};
		ModSpec game;
		game.name = "game";
		game.optdepends.insert("optional");
		const std::vector<ModSpec> selected = {game};
		auto found = fm_select_dependency_mods(selected, paths, world);
		UASSERTEQ(size_t, found.size(), 2);
		UASSERTEQ(std::string, found[0].name, "optional");
		UASSERTEQ(std::string, found[1].name, "child");
		{
			std::ofstream(world) << "load_mod_optional = false\n";
		}
		UASSERT(fm_select_dependency_mods(selected, paths, world).empty());
		fs::CreateAllDirs(mods + DIR_DELIM "duplicate");
		std::ofstream(mods + DIR_DELIM "duplicate" DIR_DELIM "init.lua");
		{
			std::ofstream(mods + DIR_DELIM "duplicate" DIR_DELIM "mod.conf")
					<< "name = optional\n";
		}
		{
			std::ofstream(world) << "gameid = test\n";
		}
		UASSERT(fm_select_dependency_mods(selected, paths, world).empty());
	}
};

static TestFmModDependencies g_test_instance;
