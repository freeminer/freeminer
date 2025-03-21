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

class MyHandler : public osmium::handler::Handler
{
	MapgenEarth *mg;
	const bool todo{false};

public:
	MyHandler(MapgenEarth *mg) : mg{mg} {}

	void osm_object(const osmium::OSMObject &osm_object) const noexcept {}

	void build_poly(const osmium::NodeRefList &a, short h, MapNode n)
	{
		const auto pos_ok = [&](const v2pos_t &pos) {
			return (pos.X >= mg->node_min.X && pos.X < mg->node_max.X &&
					pos.Y >= mg->node_min.Z && pos.Y < mg->node_max.Z);
		};

		v2pos_t prev_pos;
		size_t prev_ok = 0;
		for (const auto &node_ref : a) {
			if (!node_ref.location())
				continue;

			{
				v2pos_t pos = mg->ll_to_pos(
						ll(node_ref.location().lat(), node_ref.location().lon()));

				if (!pos_ok(pos)) {
				}
				if (prev_ok && (pos_ok(pos) || pos_ok(prev_pos))) {
					mg->bresenham(pos.X, pos.Y, prev_pos.X, prev_pos.Y,
							mg->get_height(pos.X, pos.Y), h, n);
				}
				prev_pos = pos;
				++prev_ok;
			}
		}
	}

	void way(const osmium::Way &way)
	{
		MapNode n;
		pos_t h = 0;
		if (way.tags().has_key("building")) {
			if (const auto levels = way.tags().get_value_by_key("building:levels")) {
				h = 4 * stoi(levels);
			} else {
				h = 8;
			}
			n = mg->c_cobble;
		} else if (way.tags().has_key("highway") || way.tags().has_key("aeroway")) {
			h = 1;
			n = mg->c_cobble;
		} else if (way.tags().has_key("barrier")) {
			h = 2;
			n = mg->c_cobble;
		} else if (way.tags().has_key("natural") &&
				   way.tags().get_value_by_key("natural") == std::string{"coastline"}) {
			h = 1;
			n = mg->visible_surface_hot;
		} else if (way.tags().has_key("waterway")) {
			h = 1;
			n = mg->n_water;

		} else {

			if (todo)
				DUMP("skip", way.id(), way.tags());
			return;
		}

		build_poly(way.nodes(), h, n);
	}

	void relation(const osmium::Relation &relation)
	{
		if (!relation.tags().has_key("building")) {
			return;
		}

		for (const auto &sn : relation.subitems<osmium::Way>()) {
			way(sn);
		}
	}
};

class hdl : public handler_i
{
	using index_t = osmium::index::map::SparseMemArray<osmium::unsigned_object_id_type,
			osmium::Location>;
	using cache_t = osmium::handler::NodeLocationsForWays<index_t>;

	MapgenEarth *mg{};
	const std::string path_name;

	MyHandler handler;

public:
	hdl(MapgenEarth *mg, const std::string &path_name) :
			mg{mg}, path_name{path_name}, handler{mg}
	{
	}

	void apply() override
	{
		osmium::area::Assembler::config_type assembler_config;
		assembler_config.create_empty_areas = false;

		osmium::area::MultipolygonManager<osmium::area::Assembler> mp_manager{
				assembler_config};
		index_t index;
		cache_t cache{index};
		cache.ignore_errors();
		{
			const auto llmin = mg->pos_to_ll(mg->node_min.X, mg->node_min.Z);
			const auto llmax = mg->pos_to_ll(mg->node_max.X, mg->node_max.Z);

		}
		osmium::io::File file{path_name, "pbf"};
		osmium::relations::read_relations(file, mp_manager);

		osmium::io::Reader reader{file};
		osmium::apply(reader, cache, handler,
				mp_manager.handler([&handler = this->handler](
										   const osmium::memory::Buffer &area_buffer) {
					osmium::apply(area_buffer, handler);
				}));
	}
};
