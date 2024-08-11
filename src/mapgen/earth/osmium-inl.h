class MyHandler : public osmium::handler::Handler
{
	MapgenEarth *mg;
	const bool todo{false};

public:
	MyHandler(MapgenEarth *mg) : mg{mg} {}

	void osm_object(const osmium::OSMObject &osm_object) const noexcept
	{

	}

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

				if (
						!pos_ok(pos)) {
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

	void relation(const osmium::Relation &relation)
	{
		if (!relation.tags().has_key("building")) {
			return;
		}

		for (const auto &sn : relation.subitems<osmium::Way>()) {

			build_poly(sn.nodes(), 4, mg->visible_surface_dry);

		}
	}
};

class hdl : public handler_i
{
	using index_t = osmium::index::map::SparseMemArray<osmium::unsigned_object_id_type,
			osmium::Location>;
	using cache_t = osmium::handler::NodeLocationsForWays<index_t>;

	index_t index;
	cache_t cache{index};
	const std::string path_name;
	MapgenEarth *mg;

public:
	hdl(MapgenEarth *mg, const std::string &path_name) :
			path_name{path_name}, mg{mg}
	{
		cache.ignore_errors();
	}

	void apply() override
	{
		osmium::area::Assembler::config_type
				assembler_config; 
		assembler_config.create_empty_areas = false;
		// Initialize the MultipolygonManager. Its job is to collect all
		// relations and member ways needed for each area. It then calls an
		// instance of the osmium::area::Assembler class (with the given config)
		// to actually assemble one area.
		osmium::area::MultipolygonManager<osmium::area::Assembler> mp_manager{
				assembler_config};
		// We read the input file twice. In the first pass, only relations are
		// read and fed into the multipolygon manager.
		MyHandler handler{mg};
		osmium::relations::read_relations(osmium::io::File{path_name}, mp_manager);

		osmium::io::Reader reader{osmium::io::File{path_name, "pbf"}};

		osmium::apply(reader, cache, handler,
				mp_manager.handler([&handler](const osmium::memory::Buffer &area_buffer) {
					osmium::apply(area_buffer, handler);
				}));
	}
};