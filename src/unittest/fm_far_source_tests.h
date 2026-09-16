// SPDX-License-Identifier: LGPL-2.1-or-later
#pragma once

#include "test.h"
#include "client/fm_far_source.h"
#include <array>

class TestFmFarSource : public TestBase
{
public:
	TestFmFarSource() { TestManager::registerTestModule(this); }
	const char *getName() { return "TestFmFarSource"; }
	void runTests(IGameDef *)
	{
		TEST(testPartialMerge);
		TEST(testMissingSources);
	}

private:
	void testPartialMerge()
	{
		std::array<MapNode, FARMESH_STEP_MAX> samples;
		samples.fill(MapNode(CONTENT_IGNORE));
		const MapNode road(42, 123, 7);
		samples[0] = road;
		samples[6] = MapNode(43);
		const auto load = [&](block_step_t step) { return samples[step]; };
		// A step-5 block exists, but this particular cell has no merged child.
		for (const content_t missing : {CONTENT_IGNORE, CONTENT_UNKNOWN}) {
			samples[5] = MapNode(missing);
			const auto result = farmesh::findKnownFarSample(5, load);
			UASSERT(result && *result == road);
		}
		// Once the cell arrives, the exact step takes priority, including air.
		for (const MapNode received : {MapNode(44, 98, 4), MapNode(CONTENT_AIR)}) {
			samples[5] = received;
			const auto result = farmesh::findKnownFarSample(5, load);
			UASSERT(result && *result == received);
		}
		// A valid air fallback must also stop the search before a coarse solid.
		samples[5] = MapNode(CONTENT_IGNORE);
		samples[0] = MapNode(CONTENT_AIR);
		const auto result = farmesh::findKnownFarSample(5, load);
		UASSERT(result && result->getContent() == CONTENT_AIR);
	}

	void testMissingSources()
	{
		std::array<MapNode, FARMESH_STEP_MAX> samples;
		samples.fill(MapNode(CONTENT_IGNORE));
		const auto load = [&](block_step_t step) { return samples[step]; };
		UASSERT(!farmesh::findKnownFarSample(5, load));
		// Skip unknown cells in finer blocks and retain a known coarser source.
		samples[4] = MapNode(CONTENT_UNKNOWN);
		samples[6] = MapNode(42);
		const auto result = farmesh::findKnownFarSample(5, load);
		UASSERT(result && result->getContent() == 42);
	}
};

static TestFmFarSource g_test_far_source;
