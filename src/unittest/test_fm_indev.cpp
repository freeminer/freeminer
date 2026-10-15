// SPDX-License-Identifier: GPL-3.0-or-later
#include "test.h"
#include "mapgen/mapgen_indev.h"
#include "settings.h"

class TestFmIndev : public TestBase
{
public:
	TestFmIndev() { TestManager::registerTestModule(this); }
	const char *getName() override { return "TestFmIndev"; }
	void runTests(IGameDef *) override
	{
		TEST(testFlags);
		TEST(testLegacyFlags);
		TEST(testRoundTrip);
	}
	void testFlags()
	{
		Settings settings;
		settings.set("mgv6_spflags", "nofloatlands,trees");
		settings.set("mgindev_spflags", "floatlands,notrees");
		MapgenIndevParams params;
		params.readParams(&settings);
		UASSERT(params.spflags & MGV6_FLOATLANDS);
		UASSERT(!(params.spflags & MGV6_TREES));

		settings.set("mgv6_spflags", "floatlands");
		settings.set("mgindev_spflags", "nofloatlands");
		params.readParams(&settings);
		UASSERT(!(params.spflags & MGV6_FLOATLANDS));
	}
	void testLegacyFlags()
	{
		Settings settings;
		settings.set("mgv6_spflags", "floatlands,trees");
		MapgenIndevParams params;
		params.readParams(&settings);
		UASSERT(params.spflags & MGV6_FLOATLANDS);
		UASSERT(params.spflags & MGV6_TREES);
	}
	void testRoundTrip()
	{
		Settings settings;
		MapgenIndevParams original;
		original.spflags = MGV6_FLOATLANDS | MGV6_SNOWBIOMES;
		original.writeParams(&settings);
		UASSERT(settings.exists("mgindev_spflags"));
		MapgenIndevParams restored;
		restored.readParams(&settings);
		UASSERT(restored.spflags == original.spflags);
	}
};
static TestFmIndev g_test_instance;
