#include "test.h"

#include "mapgen/earth/arnis-cpp/src/element_processing/bridges.h"
#include "mapgen/earth/arnis-cpp/src/element_processing/highways.h"
#include "mapgen/earth/arnis_world_editor.h"

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <functional>
#include <initializer_list>
#include <numbers>
#include <string>
#include <utility>
#include <unordered_map>
#include <vector>

namespace
{
using arnis::ProcessedWay;
using Tags = std::initializer_list<std::pair<const char *, const char *>>;
using Points = std::initializer_list<std::pair<int, int>>;

ProcessedWay way(std::uint64_t id, Tags tags, Points points)
{
	ProcessedWay result{};
	result.id = id;
	for (const auto &[key, value] : tags)
		result.tags.emplace(key, value);
	for (const auto &[x, z] : points) {
		arnis::ProcessedNode node{};
		node.id = id * 100 + result.nodes.size();
		node.x = x;
		node.z = z;
		result.nodes.push_back(node);
	}
	return result;
}

ProcessedWay roadBridge(std::uint64_t id, Points points)
{
	return way(id, {{"highway", "residential"}, {"bridge", "yes"}}, points);
}

struct BuiltBridges
{
	arnis::bridges::BridgeStructureMap structures;
	arnis::bridges::BridgeSurfaceMap surface;
	std::unordered_map<std::uint64_t, std::vector<int>> groundProfiles;
};

BuiltBridges structuresOver(
		const std::vector<ProcessedWay> &ways,
		const std::function<float(std::size_t)> &height = [](std::size_t) {
			return 0.0f;
		})
{
	constexpr std::size_t side = 220;
	arnis::Ground ground = arnis::Ground::new_flat(0);
	std::vector<float> row(side);
	for (std::size_t x = 0; x < side; ++x)
		row[x] = height(x);
	ground.elevation_grid.assign(side, row);
	ground.elevation_world_width = ground.elevation_world_height = side;
	ground.world_width = ground.world_height = side;
	ground.elevation_enabled = true;
	ground.elevation_blocks_per_meter = 1.0;

	arnis::WorldEditor editor;
	editor.ground = &ground;
	std::vector<arnis::ProcessedElement> elements;
	elements.reserve(ways.size());
	for (const auto &way : ways)
		elements.emplace_back(way);
	const auto connectivity = arnis::highways::build_highway_connectivity_map(elements);
	const auto outlines = arnis::bridge_styles::BridgeOutlineIndex::build(elements);
	std::unordered_map<std::uint64_t, std::vector<int>> groundProfiles;
	for (const auto &way : ways) {
		if (!way.tags.contains("highway") || arnis::bridges::is_bridge_way(way))
			continue;
		groundProfiles.emplace(way.id,
				arnis::highways::road_profile(editor, way,
						arnis::highways::highway_block_range(
								way.tags.get("highway"), way.tags, 1.0),
						connectivity, [&](int x, int z) -> std::optional<int> {
							return connectivity.is_shared(x, z)
										   ? std::optional<int>(
													 arnis::highways::junction_level(
															 editor, x, z))
										   : std::nullopt;
						}));
	}
	auto structures = arnis::bridges::BridgeStructureMap::build(
			elements, editor, connectivity, outlines, 1.0);
	auto surface = arnis::bridges::BridgeSurfaceMap::build(elements, structures, 1.0);
	return {std::move(structures), std::move(surface), std::move(groundProfiles)};
}

const arnis::bridges::BridgeMemberInfo &member(
		const arnis::bridges::BridgeStructureMap &structures, std::uint64_t id)
{
	const auto *result = structures.lookup_member(id);
	UASSERT(result != nullptr);
	return *result;
}

const std::vector<int> &ys(
		const arnis::bridges::BridgeStructureMap &structures, std::uint64_t id)
{
	return member(structures, id).ys;
}

const std::vector<int> &railYs(
		const arnis::bridges::BridgeStructureMap &structures, std::uint64_t id)
{
	const auto *deck = structures.rail_deck(id);
	UASSERT(deck != nullptr);
	return deck->ys;
}

int railLevel(const arnis::bridges::BridgeStructureMap &structures, std::uint64_t id)
{
	const auto &profile = railYs(structures, id);
	UASSERT(!profile.empty());
	return *std::max_element(profile.begin(), profile.end());
}

void assertNoJumps(const std::vector<int> &profile)
{
	for (std::size_t i = 1; i < profile.size(); ++i)
		UASSERT(std::abs(profile[i] - profile[i - 1]) <= 1);
}

} // namespace

class TestFmArnisBridges : public TestBase
{
public:
	TestFmArnisBridges() { TestManager::registerTestModule(this); }
	const char *getName() override { return "TestFmArnisBridges"; }
	void runTests(IGameDef *) override;

	void testPedestrianOnlyBridgeDoesNotUseModuleDeck();
	void testFootwayBesideRoadModuleKeepsOwnDeck();
	void testVehicularBridgeUsesModuleDeck();
	void testSideBySideCarriagewaysAreOneStructure();
	void testSidewalkOnAnotherLayerSharesRoadDeck();
	void testDeckClearsRoadUnderneath();
	void testShortRoadBridgeWithFootbridgesStaysLevel();
	void testCurvedDeckUsesWalkedLength();
	void testShortArchLiftsButBeamStaysLevel();
	void testShortBridgeSlopeLimit();
	void testApproachRoadDoesNotLiftDeck();
	void testDeckSpansValley();
	void testDeckRunsBetweenDifferentBanks();
	void testUpperDeckClearsLowerDeck();
	void testJointBetweenLayersStaysInAir();
	void testApproachRampMeetsDeckAndRoad();
	void testRampRidesOverGroundAboveGrade();
	void testLongEmbankmentLandsAndFollowsGround();
	void testRoadReachesAnchoredBridgeAtJunctionLevel();
	void testDecksForkingOffRoadLand();
	void testSideRoadDoesNotLandContinuingDeck();
	void testWayContinuingPastDeckIsNotBesideIt();
	void testOverhangingCarriagewayStaysCovered();
	void testWideDeckTailLands();
	void testShortRampSlopeLimit();
	void testCoveredCarriagewayLandingBringsDeckDown();
	void testCoveredCarriagewayOnRaisedKeepsDeckUp();
	void testRampAtForkStartsAtDrawnDeck();
	void testPathOffRoadForkDoesNotLandIt();
	void testCloseLandingsEaseDownTogether();
	void testDecksFanOutOnlyAwayFromRoad();
	void testRingOfBridgeWaysStaysGrounded();
	void testRailViaductBlocksRoadPiersAboveIt();
	void testTramRidesRoadBridgeDeck();
	void testSplitRailViaductKeepsOneLevel();
	void testDeckedRailGapBlocksSupportsAbove();
	void testSideBySideRailBridgesShareLevel();
};

void TestFmArnisBridges::runTests(IGameDef *)
{
	TEST(testPedestrianOnlyBridgeDoesNotUseModuleDeck);
	TEST(testFootwayBesideRoadModuleKeepsOwnDeck);
	TEST(testVehicularBridgeUsesModuleDeck);
	TEST(testSideBySideCarriagewaysAreOneStructure);
	TEST(testSidewalkOnAnotherLayerSharesRoadDeck);
	TEST(testDeckClearsRoadUnderneath);
	TEST(testShortRoadBridgeWithFootbridgesStaysLevel);
	TEST(testCurvedDeckUsesWalkedLength);
	TEST(testShortArchLiftsButBeamStaysLevel);
	TEST(testShortBridgeSlopeLimit);
	TEST(testApproachRoadDoesNotLiftDeck);
	TEST(testDeckSpansValley);
	TEST(testDeckRunsBetweenDifferentBanks);
	TEST(testUpperDeckClearsLowerDeck);
	TEST(testJointBetweenLayersStaysInAir);
	TEST(testApproachRampMeetsDeckAndRoad);
	TEST(testRampRidesOverGroundAboveGrade);
	TEST(testLongEmbankmentLandsAndFollowsGround);
	TEST(testRoadReachesAnchoredBridgeAtJunctionLevel);
	TEST(testDecksForkingOffRoadLand);
	TEST(testSideRoadDoesNotLandContinuingDeck);
	TEST(testWayContinuingPastDeckIsNotBesideIt);
	TEST(testOverhangingCarriagewayStaysCovered);
	TEST(testWideDeckTailLands);
	TEST(testShortRampSlopeLimit);
	TEST(testCoveredCarriagewayLandingBringsDeckDown);
	TEST(testCoveredCarriagewayOnRaisedKeepsDeckUp);
	TEST(testRampAtForkStartsAtDrawnDeck);
	TEST(testPathOffRoadForkDoesNotLandIt);
	TEST(testCloseLandingsEaseDownTogether);
	TEST(testDecksFanOutOnlyAwayFromRoad);
	TEST(testRingOfBridgeWaysStaysGrounded);
	TEST(testRailViaductBlocksRoadPiersAboveIt);
	TEST(testTramRidesRoadBridgeDeck);
	TEST(testSplitRailViaductKeepsOneLevel);
	TEST(testDeckedRailGapBlocksSupportsAbove);
	TEST(testSideBySideRailBridgesShareLevel);
}

void TestFmArnisBridges::testPedestrianOnlyBridgeDoesNotUseModuleDeck()
{
	const auto bridge =
			way(1, {{"highway", "footway"}, {"bridge", "yes"}}, {{10, 40}, {60, 40}});
	const auto built = structuresOver({bridge});
	UASSERT(!member(built.structures, 1).module_idx.has_value());
}

void TestFmArnisBridges::testFootwayBesideRoadModuleKeepsOwnDeck()
{
	const auto road =
			way(1, {{"highway", "primary"}, {"bridge", "yes"}}, {{10, 40}, {60, 40}});
	// Deliberately longer than the road module can cover.
	const auto footway =
			way(2, {{"highway", "footway"}, {"bridge", "yes"}}, {{0, 52}, {95, 52}});
	const auto built = structuresOver({road, footway});
	UASSERT(member(built.structures, 1).module_idx.has_value());
	UASSERT(!member(built.structures, 2).covered_by_wider);
	UASSERT(!member(built.structures, 2).module_idx.has_value());
}

void TestFmArnisBridges::testVehicularBridgeUsesModuleDeck()
{
	const auto bridge =
			way(2, {{"highway", "primary"}, {"bridge", "yes"}}, {{10, 40}, {60, 40}});
	const auto built = structuresOver({bridge});
	UASSERT(member(built.structures, 2).module_idx.has_value());
}

void TestFmArnisBridges::testSideBySideCarriagewaysAreOneStructure()
{
	const auto a =
			way(1, {{"highway", "secondary"}, {"bridge", "yes"}, {"oneway", "yes"}},
					{{10, 40}, {70, 40}});
	const auto b =
			way(2, {{"highway", "secondary"}, {"bridge", "yes"}, {"oneway", "yes"}},
					{{70, 48}, {10, 48}});
	const auto far =
			way(3, {{"highway", "secondary"}, {"bridge", "yes"}, {"oneway", "yes"}},
					{{70, 90}, {10, 90}});
	const auto under = way(4, {{"highway", "residential"}}, {{30, 20}, {30, 80}});
	const auto built = structuresOver({a, b, far, under});
	const auto &nearA = ys(built.structures, 1);
	const auto &farProfile = ys(built.structures, 3);
	UASSERT(nearA.size() == farProfile.size());
	UASSERT(nearA.at(20) >= 6);
	const auto &bMember = member(built.structures, 2);
	const std::size_t matchingAIndex = nearA.size() - 1 - 20;
	UTEST(bMember.drive_y_at(20) == nearA.at(matchingAIndex), "a[%zu]=%d, b[20]=%d",
			matchingAIndex, nearA.at(matchingAIndex), bMember.drive_y_at(20));
	UASSERT(farProfile.at(20) == 0);
}

void TestFmArnisBridges::testSidewalkOnAnotherLayerSharesRoadDeck()
{
	const auto road = way(1, {{"highway", "primary"}, {"bridge", "yes"}, {"layer", "2"}},
			{{10, 40}, {80, 40}});
	const auto walk = way(2, {{"highway", "footway"}, {"bridge", "yes"}, {"layer", "1"}},
			{{10, 48}, {80, 48}});
	const auto built = structuresOver({road, walk});
	UASSERT(ys(built.structures, 1).at(35) == 12);
	UASSERT(member(built.structures, 2).drive_y_at(35) == 12);
}

void TestFmArnisBridges::testDeckClearsRoadUnderneath()
{
	const auto bridge = roadBridge(1, {{10, 40}, {34, 40}});
	const auto road = way(2, {{"highway", "residential"}}, {{22, 10}, {22, 70}});
	const auto built = structuresOver({bridge, road});
	const auto &profile = ys(built.structures, 1);
	UASSERT(profile.front() == 0);
	UASSERT(profile.back() == 0);
	UASSERT(profile.at(12) >= 6);
	UASSERT(built.surface.support_blocked(22, 40, profile.at(12)));
	UASSERT(!built.surface.support_blocked(15, 40, profile.at(5)));
}

void TestFmArnisBridges::testShortRoadBridgeWithFootbridgesStaysLevel()
{
	const std::vector<ProcessedWay> ways{
			way(1, {{"highway", "tertiary"}, {"bridge", "yes"}, {"layer", "1"}},
					{{60, 46}, {39, 58}, {38, 59}}),
			way(2, {{"highway", "footway"}, {"bridge", "yes"}, {"layer", "1"}},
					{{38, 53}, {60, 42}}),
			way(3, {{"highway", "footway"}, {"bridge", "yes"}, {"layer", "1"}},
					{{37, 65}, {43, 60}, {52, 54}, {61, 49}}),
			way(4, {{"waterway", "river"}}, {{55, 82}, {49, 4}}),
			way(5, {{"highway", "tertiary"}}, {{69, 42}, {60, 46}}),
			way(6, {{"highway", "tertiary"}}, {{38, 59}, {37, 60}, {30, 65}}),
			way(7, {{"highway", "footway"}}, {{60, 42}, {68, 37}}),
			way(8, {{"highway", "footway"}}, {{38, 53}, {36, 50}, {30, 45}}),
			way(9, {{"highway", "footway"}}, {{61, 49}, {69, 46}}),
			way(10, {{"highway", "footway"}}, {{28, 76}, {37, 65}}),
	};
	const auto built = structuresOver(ways);
	for (std::uint64_t id = 1; id <= 3; ++id)
		UASSERT(std::all_of(ys(built.structures, id).begin(),
				ys(built.structures, id).end(), [](int y) { return y == 0; }));
}

void TestFmArnisBridges::testCurvedDeckUsesWalkedLength()
{
	std::vector<std::pair<int, int>> curve;
	for (int i = 0; i <= 10; ++i) {
		const double angle = i * std::numbers::pi / 10.0;
		curve.emplace_back(20 + std::lround(14.0 * (1.0 - std::cos(angle))),
				40 + std::lround(14.0 * std::sin(angle)));
	}
	ProcessedWay bridge =
			way(1, {{"highway", "residential"}, {"bridge", "yes"}, {"layer", "1"}}, {});
	for (const auto &[x, z] : curve) {
		arnis::ProcessedNode node{};
		node.id = 100 + bridge.nodes.size();
		node.x = x;
		node.z = z;
		bridge.nodes.push_back(node);
	}
	const auto built = structuresOver({bridge});
	UASSERT(*std::max_element(
					ys(built.structures, 1).begin(), ys(built.structures, 1).end()) > 0);
}

void TestFmArnisBridges::testShortArchLiftsButBeamStaysLevel()
{
	const auto arch = way(1,
			{{"highway", "residential"}, {"bridge", "yes"}, {"bridge:structure", "arch"}},
			{{10, 40}, {44, 40}});
	const auto arched = structuresOver({arch});
	UASSERT(*std::max_element(ys(arched.structures, 1).begin(),
					ys(arched.structures, 1).end()) > 0);
	const auto beamed = structuresOver({roadBridge(1, {{10, 40}, {44, 40}})});
	UASSERT(std::all_of(ys(beamed.structures, 1).begin(), ys(beamed.structures, 1).end(),
			[](int y) { return y == 0; }));
}

void TestFmArnisBridges::testShortBridgeSlopeLimit()
{
	const auto bridge = roadBridge(1, {{20, 40}, {26, 40}});
	const auto road = way(2, {{"highway", "residential"}}, {{23, 10}, {23, 70}});
	const auto built = structuresOver({bridge, road});
	const auto &profile = ys(built.structures, 1);
	UASSERT(*std::max_element(profile.begin(), profile.end()) <= 3);
	assertNoJumps(profile);
}

void TestFmArnisBridges::testApproachRoadDoesNotLiftDeck()
{
	const auto bridge = roadBridge(1, {{10, 40}, {34, 40}});
	const auto road =
			way(2, {{"highway", "residential"}}, {{34, 10}, {34, 40}, {34, 70}});
	const auto built = structuresOver({bridge, road});
	UASSERT(std::all_of(ys(built.structures, 1).begin(), ys(built.structures, 1).end(),
			[](int y) { return y == 0; }));
}

void TestFmArnisBridges::testDeckSpansValley()
{
	const auto built = structuresOver({roadBridge(1, {{10, 40}, {70, 40}})},
			[](std::size_t x) { return x >= 25 && x <= 55 ? 0.0f : 20.0f; });
	UASSERT(std::all_of(ys(built.structures, 1).begin(), ys(built.structures, 1).end(),
			[](int y) { return y == 20; }));
}

void TestFmArnisBridges::testDeckRunsBetweenDifferentBanks()
{
	const auto built =
			structuresOver({roadBridge(1, {{10, 40}, {70, 40}})}, [](std::size_t x) {
				return x <= 15 ? 10.0f : x >= 65 ? 30.0f : 0.0f;
			});
	const auto &profile = ys(built.structures, 1);
	UASSERT(profile.front() == 10);
	UASSERT(profile.back() == 30);
	UASSERT(profile.at(30) >= 18 && profile.at(30) <= 22);
}

void TestFmArnisBridges::testUpperDeckClearsLowerDeck()
{
	const auto lower =
			way(1, {{"highway", "residential"}, {"bridge", "yes"}, {"layer", "1"}},
					{{20, 40}, {44, 40}});
	const auto upper =
			way(2, {{"highway", "residential"}, {"bridge", "yes"}, {"layer", "2"}},
					{{32, 20}, {32, 60}});
	const auto built = structuresOver({lower, upper});
	const int lowerY = ys(built.structures, 1).at(12);
	const int upperY = ys(built.structures, 2).at(20);
	UASSERT(upperY >= lowerY + 6);
	UASSERT(built.surface.support_blocked(32, 40, upperY));
}

void TestFmArnisBridges::testJointBetweenLayersStaysInAir()
{
	const auto approach =
			way(1, {{"highway", "motorway"}, {"bridge", "yes"}, {"layer", "1"}},
					{{10, 40}, {40, 40}});
	const auto span = way(2, {{"highway", "motorway"}, {"bridge", "yes"}, {"layer", "2"}},
			{{40, 40}, {70, 40}});
	const auto road = way(3, {{"highway", "residential"}}, {{36, 10}, {36, 70}});
	const auto built = structuresOver({approach, span, road});
	const int approachEnd = ys(built.structures, 1).back();
	UASSERT(approachEnd > 0);
	UASSERT(ys(built.structures, 2).front() == approachEnd);
}

void TestFmArnisBridges::testApproachRampMeetsDeckAndRoad()
{
	const auto bridge =
			way(1, {{"highway", "residential"}, {"bridge", "yes"}, {"layer", "1"}},
					{{40, 40}, {80, 40}});
	const auto ramp = way(
			2, {{"highway", "residential"}, {"embankment", "yes"}}, {{10, 40}, {40, 40}});
	const auto road = way(3, {{"highway", "residential"}}, {{0, 40}, {10, 40}});
	const auto built = structuresOver({bridge, ramp, road});
	const int deck = ys(built.structures, 1).front();
	const auto *rampInfo = built.structures.lookup_ramp(2);
	UASSERT(rampInfo != nullptr);
	UASSERT(deck > 2);
	UASSERT(rampInfo->ys.back() == deck);
	UASSERT(rampInfo->ys.front() == 0);
	for (std::size_t i = 1; i < rampInfo->ys.size(); ++i) {
		const int change = rampInfo->ys[i] - rampInfo->ys[i - 1];
		UASSERT(change >= 0 && change <= 1);
	}
}

void TestFmArnisBridges::testRampRidesOverGroundAboveGrade()
{
	const auto bridge =
			way(1, {{"highway", "residential"}, {"bridge", "yes"}, {"layer", "1"}},
					{{30, 40}, {70, 40}});
	const auto ramp =
			way(2, {{"highway", "residential"}, {"layer", "1"}}, {{0, 40}, {30, 40}});
	const auto built = structuresOver({bridge, ramp},
			[](std::size_t x) { return x >= 10 && x < 14 ? 5.0f : 0.0f; });
	const auto *rampInfo = built.structures.lookup_ramp(2);
	UASSERT(rampInfo != nullptr);
	UASSERT(rampInfo->ys.front() == 0);
	UASSERT(rampInfo->ys.back() == 6);
	UASSERT(rampInfo->ys.at(20) == 4);
	for (std::size_t x = 0; x < rampInfo->ys.size(); ++x)
		UASSERT(rampInfo->ys[x] >= built.groundProfiles.at(2).at(x));
}

void TestFmArnisBridges::testLongEmbankmentLandsAndFollowsGround()
{
	const auto bridge =
			way(1, {{"highway", "residential"}, {"bridge", "yes"}, {"layer", "1"}},
					{{0, 40}, {40, 40}});
	const auto ramp =
			way(2, {{"highway", "residential"}, {"layer", "1"}}, {{0, 40}, {199, 40}});
	const auto road = way(3, {{"highway", "residential"}}, {{199, 40}, {210, 40}});
	const auto built = structuresOver({bridge, ramp, road});
	const auto *rampInfo = built.structures.lookup_ramp(2);
	UASSERT(rampInfo != nullptr);
	UASSERT(ys(built.structures, 1).front() > 0);
	const int deck = rampInfo->ys.front();
	UASSERT(deck > 0);
	const auto &ground = built.groundProfiles.at(2);
	const float fall = std::max(
			(deck - ground.back()) / static_cast<float>(ground.size() - 1), 1.0f / 8.0f);
	UASSERT(rampInfo->ys.size() == ground.size());
	for (std::size_t i = 0; i < ground.size(); ++i) {
		const int grade = std::lround(deck - fall * static_cast<float>(i));
		UTEST(rampInfo->ys[i] == std::max(grade, ground[i]),
				"cell %zu: ramp %d, ground %d, grade %d, deck %d", i, rampInfo->ys[i],
				ground[i], grade, deck);
	}
}

void TestFmArnisBridges::testRoadReachesAnchoredBridgeAtJunctionLevel()
{
	constexpr std::size_t side = 100;
	const auto bridge = roadBridge(1, {{40, 40}, {70, 40}});
	const auto road = way(2, {{"highway", "residential"}}, {{10, 40}, {40, 40}});
	std::vector<float> row(side);
	for (std::size_t x = 0; x < side; ++x)
		row[x] = x < 40 ? 20.0f : 10.0f;
	arnis::Ground ground = arnis::Ground::new_flat(0);
	ground.elevation_grid.assign(side, row);
	ground.elevation_world_width = ground.elevation_world_height = side;
	ground.world_width = ground.world_height = side;
	ground.elevation_enabled = true;
	ground.elevation_blocks_per_meter = 1.0;
	arnis::WorldEditor editor;
	editor.ground = &ground;
	const std::vector<arnis::ProcessedElement> elements{
			arnis::ProcessedElement(bridge), arnis::ProcessedElement(road)};
	const auto connectivity = arnis::highways::build_highway_connectivity_map(elements);
	const auto outlines = arnis::bridge_styles::BridgeOutlineIndex::build(elements);
	const auto structures = arnis::bridges::BridgeStructureMap::build(
			elements, editor, connectivity, outlines, 1.0);
	const int blockRange =
			arnis::highways::highway_block_range("residential", road.tags, 1.0);
	const auto roadYs = arnis::highways::road_profile(editor, road, blockRange,
			connectivity, [&](int x, int z) -> std::optional<int> {
				return connectivity.is_shared(x, z)
							   ? std::optional<int>(
										 arnis::highways::junction_level(editor, x, z))
							   : std::nullopt;
			});
	UASSERT(!roadYs.empty());
	UASSERT(structures.lookup_member(1) != nullptr);
	UASSERT(structures.lookup_member(1)->ys.front() == roadYs.back());
}

void TestFmArnisBridges::testDecksForkingOffRoadLand()
{
	const Tags tags{{"highway", "primary"}, {"bridge", "yes"}, {"layer", "1"}};
	const auto south = way(1, tags, {{50, 20}, {50, 80}});
	const auto east = way(2, tags, {{50, 20}, {80, 70}});
	const auto road = way(3, {{"highway", "primary"}}, {{50, 0}, {50, 20}});
	const auto under = way(4, {{"highway", "residential"}}, {{0, 50}, {99, 50}});
	const auto built = structuresOver({south, east, road, under});
	for (std::uint64_t id : {std::uint64_t{1}, std::uint64_t{2}}) {
		const auto &profile = ys(built.structures, id);
		UASSERT(profile.front() == 0);
		UASSERT(*std::max_element(profile.begin(), profile.end()) >= 5);
		assertNoJumps(profile);
	}
}

void TestFmArnisBridges::testSideRoadDoesNotLandContinuingDeck()
{
	const Tags tags{{"highway", "primary"}, {"bridge", "yes"}, {"layer", "1"}};
	const auto west = way(1, tags, {{10, 40}, {50, 40}});
	const auto east = way(2, tags, {{50, 40}, {90, 40}});
	const auto side = way(3, {{"highway", "service"}}, {{50, 40}, {50, 90}});
	const auto underW = way(4, {{"highway", "residential"}}, {{35, 0}, {35, 99}});
	const auto underE = way(5, {{"highway", "residential"}}, {{65, 0}, {65, 99}});
	const auto built = structuresOver({west, east, side, underW, underE});
	UASSERT(ys(built.structures, 1).back() > 0);
}

void TestFmArnisBridges::testWayContinuingPastDeckIsNotBesideIt()
{
	const Tags wideTags{{"highway", "primary"}, {"bridge", "yes"}, {"lanes", "6"}};
	const auto deck = way(1, wideTags, {{10, 40}, {50, 40}});
	const auto tail = way(2, {{"highway", "primary"}, {"bridge", "yes"}, {"lanes", "1"}},
			{{50, 40}, {58, 40}});
	const auto beside =
			way(3, {{"highway", "primary"}, {"bridge", "yes"}, {"lanes", "1"}},
					{{12, 43}, {48, 43}});
	auto doubled = way(4, wideTags, {});
	for (const auto &[x, z] :
			{std::pair<int, int>{10, 40}, {10, 40}, {50, 40}, {50, 40}}) {
		arnis::ProcessedNode node{};
		node.id = 400 + doubled.nodes.size();
		node.x = x;
		node.z = z;
		doubled.nodes.push_back(node);
	}
	const auto built = structuresOver({deck, tail, beside});
	UASSERT(!member(built.structures, 2).covered_by_wider);
	UASSERT(member(built.structures, 3).covered_by_wider);
	const auto repeated = structuresOver({doubled, tail});
	UASSERT(!member(repeated.structures, 2).covered_by_wider);
}

void TestFmArnisBridges::testOverhangingCarriagewayStaysCovered()
{
	const auto wide = way(1, {{"highway", "primary"}, {"bridge", "yes"}, {"lanes", "6"}},
			{{10, 40}, {60, 40}});
	const auto narrow =
			way(2, {{"highway", "primary"}, {"bridge", "yes"}, {"lanes", "1"}},
					{{8, 42}, {62, 42}});
	const auto built = structuresOver({wide, narrow});
	UASSERT(member(built.structures, 2).covered_by_wider);
}

void TestFmArnisBridges::testWideDeckTailLands()
{
	const Tags tags{
			{"highway", "primary"}, {"bridge", "yes"}, {"layer", "1"}, {"lanes", "6"}};
	const auto deck = way(1, tags, {{10, 40}, {60, 40}});
	const auto tail = way(2, tags, {{60, 40}, {66, 40}});
	const auto road = way(3, {{"highway", "primary"}}, {{66, 40}, {99, 40}});
	const auto under = way(4, {{"highway", "residential"}}, {{50, 0}, {50, 99}});
	const auto built = structuresOver({deck, tail, road, under});
	UASSERT(!member(built.structures, 2).covered_by_wider);
	UASSERT(ys(built.structures, 2).back() == 0);
	auto profile = ys(built.structures, 1);
	profile.insert(profile.end(), ys(built.structures, 2).begin(),
			ys(built.structures, 2).end());
	assertNoJumps(profile);
}

void TestFmArnisBridges::testShortRampSlopeLimit()
{
	const auto bridge =
			way(1, {{"highway", "footway"}, {"bridge", "yes"}, {"layer", "2"}},
					{{40, 40}, {80, 40}});
	const auto ramp =
			way(2, {{"highway", "footway"}, {"layer", "1"}}, {{36, 40}, {40, 40}});
	const auto path = way(3, {{"highway", "footway"}}, {{0, 40}, {36, 40}});
	const auto built = structuresOver({bridge, ramp, path});
	const auto *rampInfo = built.structures.lookup_ramp(2);
	UASSERT(rampInfo != nullptr);
	UASSERT(rampInfo->ys.front() == 0);
	assertNoJumps(rampInfo->ys);
	const auto &deck = ys(built.structures, 1);
	UASSERT(rampInfo->ys.back() == deck.front());
	UASSERT(*std::max_element(deck.begin(), deck.end()) > rampInfo->ys.back());
	assertNoJumps(deck);
}

void TestFmArnisBridges::testCoveredCarriagewayLandingBringsDeckDown()
{
	const Tags wideTags{
			{"highway", "primary"}, {"bridge", "yes"}, {"layer", "1"}, {"lanes", "6"}};
	const auto wide = way(1, wideTags, {{10, 40}, {90, 40}});
	const auto narrow = way(2,
			{{"highway", "primary"}, {"bridge", "yes"}, {"layer", "1"}, {"lanes", "1"}},
			{{30, 42}, {60, 42}});
	const auto road = way(3, {{"highway", "primary"}}, {{0, 42}, {30, 42}});
	const auto under = way(4, {{"highway", "residential"}}, {{50, 0}, {50, 30}});
	const auto built = structuresOver({wide, narrow, road, under});
	const auto &narrowMember = member(built.structures, 2);
	UASSERT(narrowMember.covered_by_wider);
	const auto &profile = ys(built.structures, 1);
	UASSERT(std::all_of(
			profile.begin(), profile.begin() + 21, [](int y) { return y == 0; }));
	assertNoJumps(profile);
	for (std::size_t t = 0; t <= 30; ++t)
		UASSERT(narrowMember.drive_y_at(t) == profile.at(20 + t));
}

void TestFmArnisBridges::testCoveredCarriagewayOnRaisedKeepsDeckUp()
{
	const Tags wideTags{
			{"highway", "primary"}, {"bridge", "yes"}, {"layer", "1"}, {"lanes", "6"}};
	const auto wide = way(1, wideTags, {{10, 40}, {90, 40}});
	const auto narrow = way(2,
			{{"highway", "primary"}, {"bridge", "yes"}, {"layer", "1"}, {"lanes", "1"}},
			{{30, 42}, {60, 42}});
	const auto raised =
			way(3, {{"highway", "primary"}, {"layer", "1"}}, {{0, 42}, {30, 42}});
	const auto under = way(4, {{"highway", "residential"}}, {{25, 0}, {25, 30}});
	const auto built = structuresOver({wide, narrow, raised, under});
	UASSERT(member(built.structures, 2).covered_by_wider);
	const auto &profile = ys(built.structures, 1);
	UASSERT(profile.at(15) > 0);
	for (std::size_t i = 1; i < 40; ++i)
		UASSERT(profile[i] >= profile[i - 1]);
}

void TestFmArnisBridges::testRampAtForkStartsAtDrawnDeck()
{
	const auto narrow = way(1,
			{{"highway", "primary"}, {"bridge", "yes"}, {"layer", "1"}, {"lanes", "1"}},
			{{20, 42}, {55, 42}, {60, 40}});
	const auto wide = way(2,
			{{"highway", "primary"}, {"bridge", "yes"}, {"layer", "1"}, {"lanes", "6"}},
			{{10, 40}, {60, 40}});
	const auto ramp =
			way(3, {{"highway", "primary"}, {"layer", "1"}}, {{60, 40}, {63, 40}});
	const auto road = way(4, {{"highway", "primary"}}, {{63, 40}, {99, 40}});
	const auto under = way(5, {{"highway", "residential"}}, {{40, 0}, {40, 99}});
	const auto built = structuresOver({narrow, wide, ramp, road, under});
	UASSERT(member(built.structures, 1).covered_by_wider);
	if (const auto *rampInfo = built.structures.lookup_ramp(3)) {
		UASSERT(rampInfo->ys.front() == ys(built.structures, 2).back());
		assertNoJumps(rampInfo->ys);
	}
}

void TestFmArnisBridges::testPathOffRoadForkDoesNotLandIt()
{
	const Tags tags{{"highway", "primary"}, {"bridge", "yes"}, {"layer", "1"}};
	for (const char *path : {"footway", "construction"}) {
		const auto south = way(1, tags, {{50, 20}, {50, 80}});
		const auto east = way(2, tags, {{50, 20}, {80, 70}});
		const auto walk = way(3, {{"highway", path}}, {{50, 0}, {50, 20}});
		const auto under = way(4, {{"highway", "residential"}}, {{0, 30}, {99, 30}});
		const auto built = structuresOver({south, east, walk, under});
		UASSERT(ys(built.structures, 1).front() > 0);
	}
}

void TestFmArnisBridges::testCloseLandingsEaseDownTogether()
{
	const auto wide = way(1,
			{{"highway", "primary"}, {"bridge", "yes"}, {"layer", "1"}, {"lanes", "6"}},
			{{10, 40}, {90, 40}});
	const auto narrow = way(2,
			{{"highway", "primary"}, {"bridge", "yes"}, {"layer", "1"}, {"lanes", "1"}},
			{{30, 42}, {60, 42}});
	const auto road = way(3, {{"highway", "primary"}}, {{0, 42}, {30, 42}});
	const auto under = way(4, {{"highway", "residential"}}, {{50, 0}, {50, 30}});
	const auto built = structuresOver({wide, narrow, road, under});
	const auto &profile = ys(built.structures, 1);
	UASSERT(!profile.empty());
	assertNoJumps(profile);
	UASSERT(*std::max_element(profile.begin(), profile.begin() + 21) <= 3);
}

void TestFmArnisBridges::testDecksFanOutOnlyAwayFromRoad()
{
	const Tags tags{{"highway", "primary"}, {"bridge", "yes"}, {"layer", "1"}};
	for (int scenario = 0; scenario < 4; ++scenario) {
		ProcessedWay a;
		ProcessedWay b;
		ProcessedWay road;
		switch (scenario) {
		case 0: // South and southeast decks, road north: both land.
			a = way(1, tags, {{50, 50}, {50, 100}});
			b = way(2, tags, {{50, 50}, {100, 90}});
			road = way(3, {{"highway", "primary"}}, {{50, 0}, {50, 50}});
			break;
		case 1: // An L corner with the road outside the corner lands.
			a = way(1, tags, {{50, 50}, {50, 100}});
			b = way(2, tags, {{50, 50}, {0, 50}});
			road = way(3, {{"highway", "primary"}}, {{100, 50}, {50, 50}});
			break;
		case 2: // A side road does not land a deck running through the joint.
			a = way(1, tags, {{10, 50}, {50, 50}});
			b = way(2, tags, {{50, 50}, {90, 50}});
			road = way(3, {{"highway", "primary"}}, {{50, 0}, {50, 50}});
			break;
		default: // A road leaving between two decks passes underneath them.
			a = way(1, tags, {{50, 50}, {90, 50}});
			b = way(2, tags, {{50, 50}, {43, 90}});
			road = way(3, {{"highway", "primary"}}, {{50, 50}, {75, 85}});
			break;
		}
		const auto built = structuresOver({a, b, road});
		const int atJoint = scenario == 2 ? ys(built.structures, 1).back()
										  : ys(built.structures, 1).front();
		if (scenario < 2) {
			UASSERT(atJoint == 0);
		} else {
			UASSERT(atJoint > 0);
		}
	}
}

void TestFmArnisBridges::testRingOfBridgeWaysStaysGrounded()
{
	const Tags tags{{"highway", "footway"}, {"bridge", "yes"}, {"layer", "1"}};
	const auto a = way(1, tags, {{20, 20}, {60, 20}, {60, 60}});
	const auto b = way(2, tags, {{60, 60}, {20, 60}, {20, 20}});
	const auto built = structuresOver({a, b});
	UASSERT(std::all_of(ys(built.structures, 1).begin(), ys(built.structures, 1).end(),
			[](int y) { return y == 0; }));
}

void TestFmArnisBridges::testRailViaductBlocksRoadPiersAboveIt()
{
	const auto rail = way(1, {{"railway", "rail"}, {"bridge", "yes"}, {"layer", "1"}},
			{{10, 40}, {70, 40}});
	const auto road =
			way(2, {{"highway", "residential"}, {"bridge", "yes"}, {"layer", "2"}},
					{{40, 10}, {40, 70}});
	const auto built = structuresOver({rail, road});
	const auto *deck = built.structures.rail_deck(1);
	UASSERT(deck != nullptr);
	UASSERT(deck->kind == arnis::bridges::RailDeckKind::Level);
	const int railY = deck->ys.at(30);
	UASSERT(built.surface.support_blocked(40, 40, railY + 6));
	UASSERT(!built.surface.support_blocked(40, 40, railY));
}

void TestFmArnisBridges::testTramRidesRoadBridgeDeck()
{
	const auto road = way(1, {{"highway", "primary"}, {"bridge", "yes"}, {"layer", "1"}},
			{{10, 40}, {70, 40}});
	const auto tram = way(2, {{"railway", "tram"}, {"bridge", "yes"}, {"layer", "1"}},
			{{10, 41}, {70, 41}});
	const auto built = structuresOver({road, tram});
	const auto *deck = built.structures.rail_deck(2);
	UASSERT(deck != nullptr);
	UASSERT(deck->kind == arnis::bridges::RailDeckKind::Carried);
	UASSERT(deck->ys.at(30) == ys(built.structures, 1).at(30));
}

void TestFmArnisBridges::testSplitRailViaductKeepsOneLevel()
{
	const Tags tags{{"railway", "rail"}, {"bridge", "viaduct"}};
	const auto a = way(1, tags, {{10, 40}, {40, 40}});
	const auto b = way(2, tags, {{40, 40}, {70, 40}});
	const auto built =
			structuresOver({a, b}, [](std::size_t x) { return x > 50 ? 12.0f : 0.0f; });
	UASSERT(railLevel(built.structures, 1) == railLevel(built.structures, 2));
	UASSERT(railLevel(built.structures, 1) >= 12);
}

void TestFmArnisBridges::testDeckedRailGapBlocksSupportsAbove()
{
	const Tags tags{{"railway", "rail"}, {"bridge", "yes"}};
	const auto a = way(1, tags, {{10, 40}, {70, 40}});
	const auto b = way(2, tags, {{10, 47}, {70, 47}});
	const auto built = structuresOver({a, b});
	const auto *deck = built.structures.rail_deck(1);
	UASSERT(deck != nullptr);
	UASSERT(deck->kind == arnis::bridges::RailDeckKind::Level);
	const int upper = deck->ys.at(30) + 6;
	for (int z : {43, 44})
		UASSERT(built.surface.support_blocked(40, z, upper));
}

void TestFmArnisBridges::testSideBySideRailBridgesShareLevel()
{
	const Tags tags{{"railway", "rail"}, {"bridge", "yes"}};
	const auto a = way(1, tags, {{10, 40}, {70, 40}});
	const auto b = way(2, tags, {{10, 46}, {45, 46}});
	const auto far = way(3, tags, {{10, 70}, {45, 70}});
	const auto built = structuresOver(
			{a, b, far}, [](std::size_t x) { return x > 50 ? 12.0f : 0.0f; });
	UASSERT(railLevel(built.structures, 1) == railLevel(built.structures, 2));
	UASSERT(railLevel(built.structures, 3) < railLevel(built.structures, 1));
}

static TestFmArnisBridges g_test_fm_arnis_bridges;
