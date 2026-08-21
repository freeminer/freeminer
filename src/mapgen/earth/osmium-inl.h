#include <cstddef>
#include <cstdlib>
#include <exception>
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
	std::unordered_set<std::int64_t> seen_way_ids;

	void append_way(const osmium::Way &way)
	{
		const auto id = static_cast<std::int64_t>(way.id());
		if (!seen_way_ids.emplace(id).second)
			return;

		arnis::WorldEditor editor;
		editor.mg = mg;
		arnis::ProcessedWay processed_way;
		processed_way.id = id;
		for (const auto &tag : way.tags())
			processed_way.tags.emplace(tag.key(), tag.value());
		for (const auto &node : way.nodes()) {
			arnis::ProcessedNode processed_node;
			processed_node.tags = processed_way.tags;
			const auto [x, z] = editor.node_to_xz(node);
			processed_node.x = x;
			processed_node.z = z;
			processed_node.id = id;
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

	void generate()
	{
		if (elements.empty())
			return;
		arnis::Ground ground;
		ground.mg = mg;
		arnis::WorldEditor editor;
		editor.mg = mg;
		editor.set_tile_hooks(
				[this](int min_x, int min_z, int max_x, int max_z) {
					return this->mg->beginTileOverlay(min_x, min_z, max_x, max_z);
				},
				[this](int, int, int, int) { return this->mg->mergeTileOverlay(); });
		editor.ground = &ground;
		arnis::FloodFillCache flood_fill_cache;
		XZBBox xzbbox(mg->node_min.X, mg->node_min.Z, mg->node_max.X, mg->node_max.Z);
		arnis::BuildingFootprintBitmap building_footprints(xzbbox);
		arnis::Args args;
		arnis::generate_world(
				editor, elements, args, flood_fill_cache, building_footprints);
	}
};
class hdl : public handler_i
{
	using index_t = osmium::index::map::SparseMemArray<osmium::unsigned_object_id_type,
			osmium::Location>;
	using cache_t = osmium::handler::NodeLocationsForWays<index_t>;

	const std::string path_name;

public:
	hdl(MapgenEarth *mg, const std::string &path_name) : path_name{path_name} {}

	virtual ~hdl() = default;

	void apply(MapgenEarth *mg) override
	{
		osmium::area::Assembler::config_type assembler_config;
		assembler_config.create_empty_areas = false;

		osmium::area::MultipolygonManager<osmium::area::Assembler> mp_manager{
				assembler_config};
		index_t index;
		cache_t cache{index};
		cache.ignore_errors();
		{
			// const auto llmin = mg->pos_to_ll(mg->node_min.X, mg->node_min.Z);
			// const auto llmax = mg->pos_to_ll(mg->node_max.X, mg->node_max.Z);
		}
		osmium::io::File file{path_name, "pbf"};
		osmium::relations::read_relations(file, mp_manager);

		osmium::io::Reader reader{file};
		if (!mg->vm) {
			errorstream << "wrong vm\n";
			return;
		}

		arnis::init(mg);

		MyHandler handler;

		handler.mg = mg;
		osmium::apply(reader, cache, handler,
				mp_manager.handler([&handler](const osmium::memory::Buffer &area_buffer) {
					osmium::apply(area_buffer, handler);
				}));
		try {
			handler.generate();
		} catch (const std::exception &ex) {
			errorstream << "Earth exception: " << ex.what() << "\n";
		}
	}
};
