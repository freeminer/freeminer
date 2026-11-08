#include "test.h"

#include "mapgen/earth/arnis-cpp/src/building_suppression.h"
#include "mapgen/earth/arnis-cpp/src/data_processing.h"
#include "mapgen/earth/arnis-cpp/src/elevation/peaks.h"
#include "mapgen/earth/arnis-cpp/src/osm_parser.h"

#include <cmath>
#include <algorithm>
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
		const auto node_id = i == 4 ? first_node_id : first_node_id + i;
		way.node_refs.push_back(node_id);
		if (i < 4)
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
		TEST(testRoundedSummitIsSteepenedWithoutASeam);
		TEST(testPeakLiftRespectsOneWorldSeams);
		TEST(testOutlineSuppressionIsUsedForTerrainHeightHints);
	}

	static arnis::geographic::LLBBox peak_bbox()
	{
		return {-22.9545, -43.2140, -22.9495, -43.2070};
	}

	static std::pair<std::vector<std::vector<double>>, arnis::elevation::PeakHint> dome(
			double top, double slope)
	{
		std::vector<std::vector<double>> grid(241, std::vector<double>(241));
		for (std::size_t y = 0; y < grid.size(); ++y) {
			for (std::size_t x = 0; x < grid[y].size(); ++x) {
				const double d = std::hypot(static_cast<double>(x) - 120.0,
										 static_cast<double>(y) - 120.0) *
								 5.0;
				grid[y][x] = top - slope * d * d / (d + 100.0);
			}
		}
		const auto bbox = peak_bbox();
		return {std::move(grid),
				{(bbox.min().lat() + bbox.max().lat()) * 0.5,
						(bbox.min().lng() + bbox.max().lng()) * 0.5, 0.0}};
	}

	void testRoundedSummitIsSteepenedWithoutASeam()
	{
		auto [grid, peak] = dome(8564.0, 0.8);
		peak.ele_m = 8611.0;
		const auto before = grid;
		arnis::elevation::raise_peaks(grid, peak_bbox(), {peak}, 5.0, std::nullopt);

		UASSERT(std::abs(grid[120][120] - 8611.0) < 0.5);
		UASSERT(grid[120][120] - grid[120][122] < 5.0);
		const double centre_lift = grid[120][120] - before[120][120];
		const double lift_at_50m = grid[120][130] - before[120][130];
		UASSERT(lift_at_50m > centre_lift * 0.5);
		for (std::size_t x = 120; x < 200; ++x) {
			const double raised_drop = grid[120][x] - grid[120][x + 1];
			const double original_drop = before[120][x] - before[120][x + 1];
			UASSERT(raised_drop <= 1.5 * original_drop + 0.5);
		}
	}

	void testPeakLiftRespectsOneWorldSeams()
	{
		const auto bbox = peak_bbox();
		std::vector<std::vector<double>> cone(201, std::vector<double>(201));
		for (std::size_t y = 0; y < cone.size(); ++y)
			for (std::size_t x = 0; x < cone[y].size(); ++x)
				cone[y][x] = 530.0 - std::hypot(static_cast<double>(x) - 100.0,
											 static_cast<double>(y) - 100.0) *
											 4.0;
		const arnis::elevation::PeakHint peak{(bbox.min().lat() + bbox.max().lat()) * 0.5,
				(bbox.min().lng() + bbox.max().lng()) * 0.5, 710.0};
		const auto summit = [&](std::optional<std::size_t> seam_pad) {
			auto grid = cone;
			arnis::elevation::raise_peaks(grid, bbox, {peak}, 5.0, seam_pad);
			return grid[100][100];
		};
		UASSERT(summit(std::nullopt) > 700.0);
		UASSERT(summit(64) > 700.0);
		UASSERT(summit(65) == 530.0);
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
			UTEST(model.height_m <= 100.0, "unexpected mapped height %.3f",
					model.height_m);
			part_height_counted =
					part_height_counted || std::abs(model.height_m - 93.5) < 0.01;
		}
		UASSERT(part_height_counted);
	}

	void testOutlineSuppressionIsUsedForTerrainHeightHints()
	{
		arnis::osm_parser::RawOsmDocument document;
		add_ring(document, 10, 1, 40.0, -74.0, 40.001, -73.999,
				make_tags({{"building", "office"}, {"building:levels", "25"}}));
		add_ring(document, 11, 10, 40.0, -74.0, 40.001, -73.999,
				make_tags({{"building:part", "yes"}, {"height", "93.5"}}));
		std::vector<arnis::ProcessedElement> elements;
		for (const auto &way : document.ways) {
			arnis::ProcessedWay processed;
			processed.id = way.id;
			processed.tags = way.tags;
			processed.source_node_refs = way.node_refs;
			for (const auto &node : document.nodes) {
				if (std::find(way.node_refs.begin(), way.node_refs.end(), node.id) ==
						way.node_refs.end())
					continue;
				arnis::ProcessedNode p{};
				p.id = node.id;
				p.latitude = *node.lat;
				p.longitude = *node.lon;
				processed.nodes.push_back(p);
			}
			elements.emplace_back(std::move(processed));
		}
		const auto prepared = arnis::prepare_building_data(elements);
		UTEST(prepared.outline_suppression.contains({"way", 10}),
				"outline suppression missing; part-group=%d, groups=%zu",
				prepared.part_groups.contains(11), prepared.part_groups.size());
	}
};

static TestFmArnisPeaks g_test_fm_arnis_peaks;
