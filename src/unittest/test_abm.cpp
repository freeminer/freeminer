// Luanti
// SPDX-License-Identifier: LGPL-2.1-or-later

#include "test.h"

#include "emerge.h"
#include "itemdef.h"
#include "mock_server.h"
#include "nodedef.h"
#include "server/blockmodifier.h"
#include "serverenvironment.h"
#include "servermap.h"

namespace
{

class TestABMDefinition final : public ActiveBlockModifier
{
public:
	unsigned int triggers = 0;
	uint8_t last_activate = 0;

	const std::vector<std::string> &getTriggerContents() const override
	{
		static const std::vector<std::string> contents{"test:abm_trigger"};
		return contents;
	}

	const std::vector<std::string> &getRequiredNeighbors(uint8_t) const override
	{
		static const std::vector<std::string> none;
		return none;
	}

	const std::vector<std::string> &getWithoutNeighbors() const override
	{
		static const std::vector<std::string> excluded{"test:abm_excluded"};
		return excluded;
	}

	float getTriggerInterval() override { return 2.5f; }
	u32 getTriggerChance() override { return 1; }
	bool getSimpleCatchUp() override { return false; }
	pos_t getMinY() override { return -MAX_MAP_GENERATION_LIMIT; }
	pos_t getMaxY() override { return MAX_MAP_GENERATION_LIMIT; }

	void trigger(ServerEnvironment *, v3pos_t, MapNode, u32, u32, v3pos_t,
			uint8_t activate) override
	{
		++triggers;
		last_activate = activate;
	}
};

class TestABM : public TestBase
{
public:
	TestABM() { TestManager::registerTestModule(this); }
	const char *getName() override { return "TestABM"; }

	void runTests(IGameDef *gamedef) override;
	void testCachedABM(ServerEnvironment *env, TestABMDefinition *definition,
			content_t trigger_content, content_t excluded_content);
};

static TestABM g_test_instance;

content_t register_node(MockServer &server, const std::string &name)
{
	auto *itemdef = static_cast<IWritableItemDefManager *>(server.getItemDefManager());
	auto *nodedef = const_cast<NodeDefManager *>(server.getNodeDefManager());

	ItemDefinition item;
	item.type = ITEM_NODE;
	item.name = name;
	itemdef->registerItem(item);

	ContentFeatures features;
	features.name = name;
	return nodedef->set(name, features);
}

void TestABM::runTests(IGameDef *gamedef)
{
	MockServer server(getTestTempDirectory());
	const content_t trigger_content = register_node(server, "test:abm_trigger");
	const content_t excluded_content = register_node(server, "test:abm_excluded");
	auto *ndef = const_cast<NodeDefManager *>(server.getNodeDefManager());
	ndef->updateAliases(server.getItemDefManager());
	ndef->resolveCrossrefs();

	MetricsBackend metrics;
	EmergeManager emerge(&server, &metrics);
	auto map = std::make_unique<ServerMap>(
			server.getWorldPath(), &server, &emerge, &metrics);
	ServerEnvironment env(std::move(map), &server, &metrics);

	auto *definition = new TestABMDefinition();
	env.addActiveBlockModifier(definition);
	env.m_abmhandler.init(env.m_abms);

	TEST(testCachedABM, &env, definition, trigger_content, excluded_content);

	env.deactivateBlocksAndObjects();
}

void TestABM::testCachedABM(ServerEnvironment *env, TestABMDefinition *definition,
		content_t trigger_content, content_t excluded_content)
{
	const ABMWithState &state = env->m_abms.back();
	UASSERT(state.interval == 2.5f);
	UASSERT(state.without_neighbors.get(excluded_content));

	auto block = env->getServerMap().createBlock({0, 0, 0});
	UASSERT(block);
	for (pos_t x = 0; x < MAP_BLOCKSIZE; ++x)
		for (pos_t y = 0; y < MAP_BLOCKSIZE; ++y)
			for (pos_t z = 0; z < MAP_BLOCKSIZE; ++z)
				block->setNodeNoCheck({x, y, z}, MapNode(CONTENT_AIR));

	block->m_is_mono_block = true;
	block->heat_add = 10;
	block->humidity_add = 10;
	env->m_abmhandler.apply(block.get());
	UASSERTEQ(short, block->heat_add.load(), 0);
	UASSERTEQ(short, block->humidity_add.load(), 0);
	UASSERT(!block->hasAbmTriggers());

	block->m_is_mono_block = false;
	block->setNodeNoCheck({1, 1, 1}, MapNode(trigger_content));
	block->setNodeNoCheck({2, 1, 1}, MapNode(excluded_content));
	env->m_abmhandler.apply(block.get(), ABM_ACTIVATE_CATCH_UP);
	UASSERT(block->hasAbmTriggers());

	{
		std::lock_guard<std::mutex> lock(block->abm_triggers_mutex);
		block->abm_triggers->emplace_back(abm_trigger_one{});
	}
	UASSERTEQ(size_t, block->abmTriggersRun(env, 1), 0);
	UASSERTEQ(unsigned int, definition->triggers, 0);
	UASSERT(block->hasAbmTriggers());

	block->setNodeNoCheck({2, 1, 1}, MapNode(CONTENT_AIR));
	UASSERTEQ(size_t, block->abmTriggersRun(env, 2), 1);
	UASSERTEQ(unsigned int, definition->triggers, 1);
	UASSERTEQ(uint8_t, definition->last_activate, ABM_ACTIVATE_NORMAL);
}

} // namespace
