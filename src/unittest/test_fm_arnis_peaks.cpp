#include "test.h"

#include "mapgen/earth/arnis-cpp/src/elevation/peaks.h"
#include "mapgen/earth/arnis-cpp/src/osm_parser.h"

#include <cmath>
#include <cstddef>
#include <cstdint>
#include <initializer_list>
#include <utility>

namespace
{
arnis::tags_t make_tags(
		std::initializer_list<std::pair<const char *, const char *>> pairs)
{
	arnis::tags_t result;
	for (const auto &[key, value] : pairs)
		result.emplace(key, value);
	return result;
}

void add_ring(arnis::osm_parser::RawOsmDocument &document, std::uint64_t way_id,
		std::uint64_t first_node_id, double min_lat, double min_lon, double max_lat,
		double max_lon, arnis::tags_t tags)
{
	const double coordinates[][2]{{min_lat, min_lon}, {min_lat, max_lon},
			{max_lat, max_lon}, {max_lat, min_lon}, {min_lat, min_lon}};
	arnis::osm_parser::RawWay way;
	way.id = way_id;
	way.tags = std::move(tags);
	for (std::size_t i = 0; i < 5; ++i) {
		const auto node_id = first_node_id + i;
		way.node_refs.push_back(node_id);
		document.nodes.push_back({node_id, coordinates[i][0], coordinates[i][1], {}});
	}
	document.ways.push_back(std::move(way));
}
} // namespace

class TestFmArnisPeaks : public TestBase
{
public:
	TestFmArnisPeaks() { TestManager::registerTestModule(this); }
	const char *getName() override { return "TestFmArnisPeaks"; }
	void runTests(IGameDef *) override
	{
		TEST(testTallBuildingPartInsideRideIsSuppressed);
		TEST(testTallManMadeTowerIsIncluded);
		TEST(testTallManMadeTowerRelationIsIncluded);
		TEST(testOutlineReplacedByItsPartsDoesNotRaiseHeightEstimate);
	}

	void testTallBuildingPartInsideRideIsSuppressed()
	{
		arnis::osm_parser::RawOsmDocument document;
		add_ring(document, 1, 1, -0.002, -0.002, 0.002, 0.002,
				make_tags({{"attraction", "big_wheel"}}));
		add_ring(document, 2, 10, -0.001, -0.001, 0.001, 0.001,
				make_tags({{"building:part", "yes"}, {"height", "300"}}));

		UASSERT(arnis::elevation::tall_structures_in(document, 1.0).empty());
	}

	void testTallManMadeTowerIsIncluded()
	{
		arnis::osm_parser::RawOsmDocument document;
		add_ring(document, 1, 1, -0.002, -0.002, 0.002, 0.002,
				make_tags({{"man_made", "tower"}, {"height", "368"}}));

		const auto tall = arnis::elevation::tall_structures_in(document, 1.0);
		UASSERT(!tall.empty());
		UASSERT(tall.front().height_m >= 368.0);
	}

	void testTallManMadeTowerRelationIsIncluded()
	{
		arnis::osm_parser::RawOsmDocument document;
		add_ring(document, 1, 1, -0.002, -0.002, 0.002, 0.002, {});
		arnis::osm_parser::RawRelation relation;
		relation.id = 2;
		relation.members.push_back({"way", 1, "outer"});
		relation.tags = make_tags(
				{{"type", "multipolygon"}, {"man_made", "tower"}, {"height", "368"}});
		document.relations.push_back(std::move(relation));

		const auto tall = arnis::elevation::tall_structures_in(document, 1.0);
		UASSERT(!tall.empty());
		UASSERT(tall.front().height_m >= 368.0);
	}

	void testOutlineReplacedByItsPartsDoesNotRaiseHeightEstimate()
	{
		arnis::osm_parser::RawOsmDocument document;
		// A 25-storey outline is generated from the coincident 93.5 m part, not
		// from the outline's independently inferred storey height.
		add_ring(document, 10, 1, 40.0, -74.0, 40.001, -73.999,
				make_tags({{"building", "office"}, {"building:levels", "25"}}));
		add_ring(document, 11, 10, 40.0, -74.0, 40.001, -73.999,
				make_tags({{"building:part", "yes"}, {"height", "93.5"},
						{"roof:shape", "flat"}}));

		const auto tall = arnis::elevation::tall_structures_in(document, 1.0);
		UASSERT(!tall.empty());
		bool part_height_counted = false;
		for (const auto &model : tall) {
			UASSERT(model.height_m <= 100.0);
			part_height_counted =
					part_height_counted || std::abs(model.height_m - 93.5) < 0.01;
		}
		UASSERT(part_height_counted);
	}
};

static TestFmArnisPeaks g_test_fm_arnis_peaks;
