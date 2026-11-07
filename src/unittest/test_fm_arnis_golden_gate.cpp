#include "test.h"

#include "mapgen/earth/arnis-cpp/src/coordinate_system/transformation.h"
#include "mapgen/earth/arnis-cpp/src/element_processing/bridges.h"
#include "mapgen/earth/arnis-cpp/src/element_processing/highways.h"
#include "mapgen/earth/arnis-cpp/src/osm_parser.h"
#include "mapgen/earth/arnis_world_editor.h"

#include <algorithm>
#include <cstdint>
#include <cmath>
#include <filesystem>
#include <fstream>
#include <limits>
#include <optional>
#include <unordered_map>
#include <utility>
#include <vector>

namespace
{
struct GoldenGateResult
{
	arnis::bridge_towers::MappedSupports supports;
	arnis::bridges::BridgeStructureMap structures;
	arnis::bridges::BridgeSurfaceMap surface;
	std::vector<arnis::ProcessedElement> elements;
};

GoldenGateResult buildGoldenGate()
{
	const auto fixture = std::filesystem::path(__FILE__).parent_path() / "data" /
						 "test_fm_golden_gate.osm";
	std::ifstream input(fixture, std::ios::binary);
	UASSERT(input.good());
	const auto raw = arnis::osm_parser::parse_osm_xml(input);
	UASSERT(raw.ways.size() == 81);
	UASSERT(raw.completeness.unresolved_node_refs == 0);
	UASSERT(raw.completeness.ways_missing_nodes == 0);
	const auto bounds = arnis::osm_parser::resolve_bbox(raw);
	UASSERT(bounds.has_value());
	const auto [minLat, minLon, maxLat, maxLon] = *bounds;
	const auto [transform, rectangle] =
			arnis::coordinate_system::CoordTransformer::llbbox_to_xzbbox(
					arnis::geographic::LLBBox(minLat, minLon, maxLat, maxLon), 1.0);
	(void)rectangle;

	std::unordered_map<std::uint64_t, arnis::ProcessedNode> nodes;
	for (const auto &rawNode : raw.nodes) {
		UASSERT(rawNode.lat && rawNode.lon);
		const auto point = transform.transform_point(
				arnis::geographic::LLPoint(*rawNode.lat, *rawNode.lon));
		arnis::ProcessedNode node{};
		node.id = rawNode.id;
		node.tags = rawNode.tags;
		node.x = point.x;
		node.z = point.z;
		node.latitude = *rawNode.lat;
		node.longitude = *rawNode.lon;
		nodes.emplace(node.id, node);
	}
	std::vector<arnis::ProcessedElement> elements;
	for (const auto &rawWay : raw.ways) {
		arnis::ProcessedWay way{};
		way.id = rawWay.id;
		way.tags = rawWay.tags;
		arnis::osm_parser::filter_tags(way.tags);
		way.source_node_refs = rawWay.node_refs;
		for (const std::uint64_t ref : rawWay.node_refs) {
			const auto node = nodes.find(ref);
			UASSERT(node != nodes.end());
			way.nodes.push_back(node->second);
		}
		elements.emplace_back(std::move(way));
	}
	arnis::Ground ground = arnis::Ground::new_flat(0);
	arnis::WorldEditor editor;
	editor.ground = &ground;
	auto supports = arnis::bridge_towers::collect(elements, editor, 1.0);
	const auto connectivity = arnis::highways::build_highway_connectivity_map(elements);
	const auto outlines = arnis::bridge_styles::BridgeOutlineIndex::build(elements);
	auto structures = arnis::bridges::BridgeStructureMap::build(
			elements, editor, connectivity, outlines, 1.0);
	auto surface = arnis::bridges::BridgeSurfaceMap::build(elements, structures, 1.0);
	return {std::move(supports), std::move(structures), std::move(surface),
			std::move(elements)};
}

} // namespace

class TestFmArnisGoldenGate : public TestBase
{
public:
	TestFmArnisGoldenGate() { TestManager::registerTestModule(this); }
	const char *getName() override { return "TestFmArnisGoldenGate"; }
	void runTests(IGameDef *) override
	{
		TEST(testRealTowerPartsClusterAtDeckLevel);
		TEST(testRealBridgeWaysProduceRenderReadyCableSpan);
	}

	void testRealTowerPartsClusterAtDeckLevel()
	{
		const auto built = buildGoldenGate();
		UTEST(built.supports.towers.size() == 2, "mapped tower count: %zu",
				built.supports.towers.size());
		for (const auto &tower : built.supports.towers) {
			UTEST(tower.top_y == 225, "tower at %d,%d: top=%d", tower.x, tower.z,
					tower.top_y);
			UTEST(tower.deck_y == 74, "tower at %d,%d: deck=%d", tower.x, tower.z,
					tower.deck_y.value_or(-1000));
		}
		const auto [north, south] = std::minmax_element(built.supports.towers.begin(),
				built.supports.towers.end(),
				[](const auto &a, const auto &b) { return a.z < b.z; });
		UTEST(south->z - north->z > 1200 && south->z - north->z < 1400,
				"projected tower separation=%d", south->z - north->z);
	}

	void testRealBridgeWaysProduceRenderReadyCableSpan()
	{
		auto built = buildGoldenGate();
		UASSERT(built.supports.towers.size() == 2);
		const arnis::bridges::BridgeMemberInfo *carrier = nullptr;
		std::uint64_t carrierId = 0;
		for (const std::uint64_t id :
				{std::uint64_t{1560010146}, std::uint64_t{1560010147},
						std::uint64_t{1557712986}, std::uint64_t{1557712987}}) {
			const auto *member = built.structures.lookup_member(id);
			UTEST(member != nullptr, "bridge way %llu has no member",
					static_cast<unsigned long long>(id));
			if (member->cable_span &&
					(!carrier || member->ys.size() > carrier->ys.size())) {
				carrier = member;
				carrierId = id;
			}
		}
		UTEST(carrier != nullptr, "no real Golden Gate carriage received a cable span");
		const auto &span = *carrier->cable_span;
		UTEST(span.points.size() == 4, "cable span has %zu points", span.points.size());
		UTEST(span.points[1].second == 222.0f && span.points[2].second == 222.0f,
				"saddles: %.2f, %.2f", span.points[1].second, span.points[2].second);
		UASSERT(span.girder_depth == 8);
		const float midpoint = (span.points[1].first + span.points[2].first) * 0.5f;
		const auto cable = span.cable_y(midpoint);
		UASSERT(cable.has_value());
		UTEST(std::abs(*cable - 77.0f) < 1.0f, "midspan cable height %.3f", *cable);

		const arnis::ProcessedElement *carrierElement = nullptr;
		for (const auto &element : built.elements)
			if (element.is_way() && element.as_way().id == carrierId)
				carrierElement = &element;
		UASSERT(carrierElement != nullptr);
		const auto &way = carrierElement->as_way();
		const auto member = carrier;
		const auto cells = arnis::bridges::way_cells(way);
		std::vector<arnis::bridge_styles::BridgePathSample> path;
		path.reserve(cells.size());
		for (std::size_t i = 0; i < cells.size(); ++i) {
			const auto &previous = cells[i ? i - 1 : i];
			const auto &next = cells[std::min(i + 1, cells.size() - 1)];
			const float dx = static_cast<float>(next.first - previous.first);
			const float dz = static_cast<float>(next.second - previous.second);
			const float length = std::hypot(dx, dz);
			path.push_back({cells[i].first, member->drive_y_at(i), cells[i].second,
					length > 0.0f ? dz / length : 0.0f,
					length > 0.0f ? -dx / length : 1.0f});
		}
		UASSERT(path.size() > 4);
		std::size_t suspendedSamples = 0;
		int maxCableY = std::numeric_limits<int>::min();
		for (const auto &sample : path) {
			const auto height = span.cable_y(span.u(sample.x, sample.z));
			if (!height)
				continue;
			++suspendedSamples;
			maxCableY = std::max(maxCableY,
					std::max(static_cast<int>(std::lround(*height)), sample.y + 2));
		}
		UTEST(suspendedSamples > 0, "no real path samples intersect the cable span");
		UTEST(maxCableY == 222, "renderer-ready cable top=%d", maxCableY);
	}
};

static TestFmArnisGoldenGate g_test_fm_arnis_golden_gate;
