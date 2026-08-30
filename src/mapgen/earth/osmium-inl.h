#include <algorithm>
#include <cctype>
#include <cmath>
#include <cstddef>
#include <cstdlib>
#include <exception>
#include <limits>
#include <memory>
#include <mutex>
#include <optional>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>
#include "irr_v3d.h"
#include "irrlichttypes.h"
#include "log.h"
#include "map.h"
#if !defined(FILE_INCLUDED)
#include "debug/dump.h"
#include <osmium/area/assembler.hpp>
#include <osmium/area/multipolygon_manager.hpp>
#include <osmium/dynamic_handler.hpp>
#include <osmium/handler/node_locations_for_ways.hpp>
#include <osmium/index/map/sparse_mem_array.hpp>
#include <osmium/io/file.hpp>
#include <osmium/io/pbf_input.hpp>
#include <osmium/osm/entity_bits.hpp>
#include <osmium/osm/node.hpp>
#include <osmium/osm/way.hpp>
#include <osmium/tags/tags_filter.hpp>

#include "mapgen/mapgen_earth.h"
#endif

#include "arnis-cpp/src/data_processing.h"

#if 0
static constexpr auto floor_height = 4;
static constexpr auto default_floors = 2;

class MyHandlerManual : public osmium::handler::Handler
{
	MapgenEarth *mg;
	const bool todo{false};

public:
	MyHandlerManual(MapgenEarth *mg) : mg{mg} {}

	void osm_object(const osmium::OSMObject &osm_object) const noexcept {}

	bool pos_ok(const v2pos_t &pos)
	{
		return (pos.X >= mg->node_min.X && pos.X < mg->node_max.X &&
				pos.Y >= mg->node_min.Z && pos.Y < mg->node_max.Z);
	};

	void build_poly(const osmium::NodeRefList &a, pos_t h_min, pos_t h, MapNode n,
			bool use_surface_height = false)
	{

		v2pos_t prev_pos;
		size_t prev_ok{};
		pos_t y{};
		size_t num{};
		for (const auto &node_ref : a) {
			if (!node_ref.location())
				continue;

			{
				v2pos_t pos = mg->ll_to_pos(
						ll(node_ref.location().lat(), node_ref.location().lon()));

				if (!num++) {
					y = mg->get_height(pos.X, pos.Y);
				}
				if (prev_ok && (pos_ok(pos) || pos_ok(prev_pos))) {
					mg->bresenham(pos.X, pos.Y, prev_pos.X, prev_pos.Y,
							y + h_min, h - h_min, n);
				}
				prev_pos = pos;
				++prev_ok;
			}
		}

		for (const auto &h_use :
				{static_cast<pos_t>(h_min), static_cast<pos_t>(h)}) { //try roof

			auto at_y = h_use + y;

			if (at_y < mg->node_min.Y || at_y > mg->node_max.Y) {
				continue;
			}

			std::vector<v2pos_t> list;
			for (const auto &node_ref : a) {
				v2pos_t pos = mg->ll_to_pos(
						ll(node_ref.location().lat(), node_ref.location().lon()));
				list.emplace_back(pos);
			}
			auto area = flood_fill_area(list);
			for (const auto &pos2 : area) {
				if (use_surface_height) {
					y = mg->get_height(pos2.X, pos2.Y);
				}

				const v3pos_t pos = {pos2.X, static_cast<short>(h_use + y), pos2.Y};

				if (mg->vm->exists(pos)) {
					mg->vm->setNode(pos, n);
				}
			}
		}
	}

	void way(const osmium::Way &way)
	{

		if (!(way.tags().has_key("building") || way.tags().has_key("building:part"))) {
			return;
		}
		go_way(mg, way);
		return;

		MapNode n;
		pos_t h = 0;
		pos_t h_min = 0;
		bool use_surface_height = false;

		if (way.tags().has_key("height")) {
			h = stoi(way.tags().get_value_by_key("height"));
		}
		if (way.tags().has_key("min_height")) {
			h_min = stoi(way.tags().get_value_by_key("min_height"));
		}

		if (way.tags().has_key("building") || way.tags().has_key("building:part")) {
			if (!h) {
				if (const auto levels = way.tags().get_value_by_key("building:levels")) {
					h = floor_height * stoi(levels);
				} else {
					h = floor_height * default_floors;
				}
			}
			n = mg->c_cobble;
		} else if (way.tags().has_key("highway") || way.tags().has_key("aeroway")) {
			if (!h)
				h = 1;
			n = mg->c_cobble;
			use_surface_height = true;
		} else if (way.tags().has_key("barrier")) {
			if (!h)
				h = 2;
			n = mg->c_cobble;
			use_surface_height = true;
		} else if (way.tags().has_key("natural") &&
				   way.tags().get_value_by_key("natural") == std::string{"coastline"}) {
			if (!h)
				h = 1;
			n = mg->visible_surface_hot;
			use_surface_height = true;
		} else if (way.tags().has_key("waterway")) {
			if (!h)
				h = 1;
			n = mg->n_water;
			use_surface_height = true;
		} else {
			if (todo)
				DUMP("skip", way.id(), way.tags());
			return;
		}
		if (n) {
			build_poly(way.nodes(), h_min, h, n, use_surface_height);
		}
	}

	void relation(const osmium::Relation &relation)
	{

/*		if (!(relation.tags().has_key("building") ||
					relation.tags().has_key("building:part"))) {
			return;
		}
*/
		go_buildings(mg, relation);
		return;

		for (const auto &sn : relation.subitems<osmium::Way>()) {
			way(sn);
		}
	}
};
#endif

class MyHandler : public osmium::handler::Handler
{
public:
	MapgenEarth *mg{};
	std::vector<arnis::ProcessedElement> elements;
	std::unordered_set<std::uint64_t> seen_way_ids;
	std::unordered_set<std::uint64_t> seen_node_ids;
	std::unordered_map<std::uint64_t, arnis::tags_t> tagged_node_tags;

	void node(const osmium::Node &node)
	{
		const auto id = static_cast<std::uint64_t>(node.id());
		if (!seen_node_ids.emplace(id).second)
			return;
		arnis::tags_t tags;
		for (const auto &tag : node.tags())
			tags.emplace(tag.key(), tag.value());
		if (tags.empty())
			return;
		tagged_node_tags.emplace(id, tags);
		arnis::WorldEditor editor;
		editor.mg = mg;
		editor.set_ground_origin(mg->node_min.X, mg->node_min.Z);
		const auto pos = mg->ll_to_pos({static_cast<ll_t>(node.location().lat()),
				static_cast<ll_t>(node.location().lon())});
		const int x = pos.X, z = pos.Y;
		if (x < mg->node_min.X || x > mg->node_max.X || z < mg->node_min.Z ||
				z > mg->node_max.Z)
			return;
		arnis::ProcessedNode processed_node;
		processed_node.id = id;
		processed_node.tags = std::move(tags);
		processed_node.x = x;
		processed_node.z = z;
		elements.emplace_back(std::move(processed_node));
	}

	void append_way(const osmium::Way &way)
	{
		const auto id = static_cast<std::uint64_t>(way.id());
		if (!seen_way_ids.emplace(id).second)
			return;

		arnis::WorldEditor editor;
		editor.mg = mg;
		editor.set_ground_origin(mg->node_min.X, mg->node_min.Z);
		arnis::ProcessedWay processed_way;
		processed_way.id = id;
		for (const auto &tag : way.tags())
			processed_way.tags.emplace(tag.key(), tag.value());
		for (const auto &node : way.nodes()) {
			arnis::ProcessedNode processed_node;
			const auto node_id = static_cast<std::uint64_t>(node.ref());
			if (const auto found = tagged_node_tags.find(node_id);
					found != tagged_node_tags.end())
				processed_node.tags = found->second;
			const auto [x, z] = editor.node_to_xz(node);
			processed_node.x = x;
			processed_node.z = z;
			processed_node.id = node_id;
			processed_way.nodes.emplace_back(std::move(processed_node));
		}
		elements.emplace_back(processed_way);
	}

	void way(const osmium::Way &way) { append_way(way); }

	void relation(const osmium::Relation &relation)
	{
		try {
			for (const auto &sn : relation.subitems<osmium::Way>()) {
				append_way(sn);
			}
		} catch (const std::exception &ex) {
			DUMP(ex.what());
		}
	}
};

namespace earth_osmium_detail
{

arnis::Args earth_arnis_args()
{
	arnis::Args args;
	args.use_3d = true;
	args.interior = true;
	args.roof = true;
	args.signage = arnis::SignageLevel::Full;
	args.fillground = true;
	args.disable_height_limit = true;
	return args;
}

std::optional<double> earth_dimension_meters(const std::string &text)
{
	const char *begin = text.c_str();
	char *end = nullptr;
	const double value = std::strtod(begin, &end);
	if (end == begin || !std::isfinite(value) || value < 0.0)
		return std::nullopt;
	while (*end && std::isspace(static_cast<unsigned char>(*end)))
		++end;
	if ((*end == 'f' || *end == 'F') && (end[1] == 't' || end[1] == 'T'))
		return value * 0.3048;
	if (*end == '\'')
		return value * 0.3048;
	return value;
}

double earth_tag_number(const arnis::tags_t &tags, const char *key)
{
	const auto found = tags.find(key);
	if (found == tags.end())
		return 0.0;
	return earth_dimension_meters(found->second).value_or(0.0);
}

pos_t earth_authored_height_margin(const std::vector<arnis::ProcessedElement> &elements)
{
	if (elements.empty())
		return 0;
	// Covers inferred buildings, trees, signs, street lights, bridge layers and
	// rooftop details even when OSM has no explicit height tags.
	double max_height = 256.0;
	for (const auto &element : elements) {
		const auto &tags = element.tags();
		const double height = std::max({earth_tag_number(tags, "height"),
				earth_tag_number(tags, "building:height"),
				earth_tag_number(tags, "est_height")});
		const double min_height = earth_tag_number(tags, "min_height");
		const double roof_height = earth_tag_number(tags, "roof:height");
		const double levels = std::max(earth_tag_number(tags, "building:levels"),
				earth_tag_number(tags, "levels"));
		const double min_level = earth_tag_number(tags, "building:min_level");
		const double roof_levels = earth_tag_number(tags, "roof:levels");
		const double layer = earth_tag_number(tags, "layer");

		max_height = std::max(
				max_height, std::max(height + min_height + roof_height,
									(levels + min_level + roof_levels) * 6.0 + 16.0));
		max_height = std::max(max_height, layer * 8.0 + 64.0);

		const auto has_model_tag = [&tags](const char *key) {
			const auto found = tags.find(key);
			return found != tags.end() && !found->second.empty();
		};
		if (has_model_tag("wikidata") || has_model_tag("3dmr") ||
				has_model_tag("ref:3dmr") || has_model_tag("model") ||
				has_model_tag("model:uri"))
			max_height = std::max(max_height, 640.0);

		const std::string man_made = tags.get("man_made");
		if (man_made == "tower" || man_made == "communications_tower" ||
				man_made == "chimney" || man_made == "wind_turbine")
			max_height = std::max(max_height, 320.0);
	}
	// Generation can add roof ornaments and lights above the tagged height.
	const long double margin = std::ceil(max_height) + 64.0L;
	return margin >= static_cast<long double>(std::numeric_limits<pos_t>::max())
				   ? std::numeric_limits<pos_t>::max()
				   : static_cast<pos_t>(margin);
}

pos_t earth_element_terrain_max(MapgenEarth *mg,
		const std::vector<arnis::ProcessedElement> &elements, pos_t maximum)
{
	const auto update = [mg, &maximum](int x, int z) {
		if (x < std::numeric_limits<pos_t>::min() ||
				x > std::numeric_limits<pos_t>::max() ||
				z < std::numeric_limits<pos_t>::min() ||
				z > std::numeric_limits<pos_t>::max())
			return;
		maximum = std::max(
				maximum, mg->get_height(static_cast<pos_t>(x), static_cast<pos_t>(z)));
	};
	for (const auto &element : elements) {
		if (element.is_node()) {
			const auto &node = element.as_node();
			update(node.x, node.z);
		} else if (element.is_way()) {
			for (const auto &node : element.as_way().nodes)
				update(node.x, node.z);
		} else {
			for (const auto &member : element.as_relation().members)
				for (const auto &node : member.way.nodes)
					update(node.x, node.z);
		}
	}
	return maximum;
}

struct CachedArnisExtract
{
	std::once_flag parse_once;
	std::once_flag flood_once;
	std::mutex flood_wave_mutex;
	std::size_t active_generators = 0;
	bool flood_released = false;
	std::vector<arnis::ProcessedElement> elements;
	std::unique_ptr<arnis::FloodFillCache> flood_fill_cache;
	std::unique_ptr<arnis::BuildingFootprintBitmap> building_footprints;
	pos_t authored_max_y = std::numeric_limits<pos_t>::lowest();
};

class FloodWaveGuard
{
	CachedArnisExtract &cached;

public:
	explicit FloodWaveGuard(CachedArnisExtract &cached) : cached(cached)
	{
		std::lock_guard<std::mutex> lock(cached.flood_wave_mutex);
		if (cached.flood_released) {
			auto args = earth_arnis_args();
			auto flood = arnis::FloodFillCache::precompute(cached.elements, args.timeout);
			flood.retain_entries();
			*cached.flood_fill_cache = std::move(flood);
			cached.flood_released = false;
		}
		++cached.active_generators;
	}

	~FloodWaveGuard()
	{
		std::lock_guard<std::mutex> lock(cached.flood_wave_mutex);
		if (--cached.active_generators == 0) {
			cached.flood_fill_cache->clear();
			cached.flood_released = true;
		}
	}

	FloodWaveGuard(const FloodWaveGuard &) = delete;
	FloodWaveGuard &operator=(const FloodWaveGuard &) = delete;
};

void generate_cached_arnis(MapgenEarth *mg, CachedArnisExtract &cached)
{
	if (cached.elements.empty() || !cached.flood_fill_cache ||
			!cached.building_footprints)
		return;
	arnis::Ground ground;
	ground.mg = mg;
	arnis::WorldEditor editor;
	editor.mg = mg;
	editor.set_ground_origin(mg->node_min.X, mg->node_min.Z);
	editor.set_tile_hooks(
			[mg](int min_x, int min_z, int max_x, int max_z) {
				return mg->beginTileOverlay(min_x, min_z, max_x, max_z);
			},
			[mg](int, int, int, int) { return mg->mergeTileOverlay(); });
	editor.ground = &ground;
	auto args = earth_arnis_args();
	FloodWaveGuard flood_wave(cached);
	arnis::generate_world(editor, cached.elements, args, *cached.flood_fill_cache,
			*cached.building_footprints, true);
}

} // namespace earth_osmium_detail

class hdl : public handler_i
{
	using index_t = osmium::index::map::SparseMemArray<osmium::unsigned_object_id_type,
			osmium::Location>;
	using cache_t = osmium::handler::NodeLocationsForWays<index_t>;

	const std::string path_name;
	std::mutex cached_extracts_mutex;
	std::unordered_map<EarthHorizontalKey,
			std::shared_ptr<earth_osmium_detail::CachedArnisExtract>,
			EarthHorizontalKeyHash>
			cached_extracts;

public:
	hdl(MapgenEarth *mg, const std::string &path_name) : path_name{path_name} {}

	virtual ~hdl() = default;

	void apply(MapgenEarth *mg) override
	{
		if (!mg->vm) {
			errorstream << "wrong vm\n";
			return;
		}

		const EarthHorizontalKey key = mg->horizontalKey();
		std::shared_ptr<earth_osmium_detail::CachedArnisExtract> cached;
		{
			std::lock_guard<std::mutex> lock(cached_extracts_mutex);
			if (!cached_extracts.contains(key) && cached_extracts.size() >= 8)
				cached_extracts.erase(cached_extracts.begin());
			auto [it, inserted] = cached_extracts.try_emplace(key);
			if (inserted)
				it->second = std::make_shared<earth_osmium_detail::CachedArnisExtract>();
			cached = it->second;
		}

		try {
			std::call_once(cached->parse_once, [&]() {
				osmium::area::Assembler::config_type assembler_config;
				assembler_config.create_empty_areas = false;
				osmium::area::MultipolygonManager<osmium::area::Assembler> mp_manager{
						assembler_config};
				index_t index;
				cache_t node_cache{index};
				node_cache.ignore_errors();
				osmium::io::File file{path_name, "pbf"};
				osmium::relations::read_relations(file, mp_manager);
				osmium::io::Reader reader{file};
				MyHandler handler;
				handler.mg = mg;
				osmium::apply(reader, node_cache, handler,
						mp_manager.handler(
								[&handler](const osmium::memory::Buffer &area_buffer) {
									osmium::apply(area_buffer, handler);
								}));
				cached->elements = std::move(handler.elements);
				const pos_t terrain_max = earth_osmium_detail::earth_element_terrain_max(
						mg, cached->elements, mg->cachedOrComputeTerrainMaxY());
				const pos_t margin = earth_osmium_detail::earth_authored_height_margin(
						cached->elements);
				const long double maximum =
						static_cast<long double>(terrain_max) + margin;
				cached->authored_max_y =
						maximum >= static_cast<long double>(
										   std::numeric_limits<pos_t>::max())
								? std::numeric_limits<pos_t>::max()
								: static_cast<pos_t>(maximum);
				arnis::prepare_elements_for_generation(cached->elements);
			});

			mg->cacheAuthoredMaxY(cached->authored_max_y);
			if (mg->node_min.Y > cached->authored_max_y)
				return;
			if (cached->elements.empty())
				return;

			std::call_once(cached->flood_once, [&]() {
				auto args = earth_osmium_detail::earth_arnis_args();
				auto flood =
						arnis::FloodFillCache::precompute(cached->elements, args.timeout);
				flood.retain_entries();
				XZBBox xzbbox(
						mg->node_min.X, mg->node_min.Z, mg->node_max.X, mg->node_max.Z);
				auto footprints =
						flood.collect_building_footprints(cached->elements, xzbbox);
				cached->flood_fill_cache =
						std::make_unique<arnis::FloodFillCache>(std::move(flood));
				cached->building_footprints =
						std::make_unique<arnis::BuildingFootprintBitmap>(
								std::move(footprints));
			});

			arnis::init(mg);
			earth_osmium_detail::generate_cached_arnis(mg, *cached);
		} catch (const std::exception &ex) {
			errorstream << "Earth exception: " << ex.what() << "\n";
		}
	}
};
