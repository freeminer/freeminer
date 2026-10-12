#include <algorithm>
#include <array>
#include <cerrno>
#include <cctype>
#include <cmath>
#include <cstddef>
#include <cstdlib>
#include <exception>
#include <filesystem>
#include <functional>
#include <iterator>
#include <limits>
#include <memory>
#include <mutex>
#include <optional>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>
#include "log.h"
#include "mapgen/earth/osmium-inl.h"
#include "mapgen/earth/arnis-cpp/src/args.h"
#include "settings.h"
#if !defined(FILE_INCLUDED)
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
#include "arnis-cpp/src/clipping.h"
#include "arnis-cpp/src/floodfill_cache.h"
#include "arnis-cpp/src/osm_parser.h"
#include "arnis-cpp/src/overture.h"

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
	std::unordered_set<std::uint64_t> seen_relation_ids;
	std::unordered_set<std::uint64_t> seen_node_ids;
	std::unordered_map<std::uint64_t, arnis::tags_t> tagged_node_tags;
	std::unordered_map<std::uint64_t, std::size_t> way_indices;
	struct PendingRelation
	{
		arnis::ProcessedRelation relation;
		std::vector<std::pair<std::uint64_t, arnis::ProcessedMemberRole>> ways;
	};
	std::vector<PendingRelation> pending_relations;

	void node(const osmium::Node &node)
	{
		const auto id = static_cast<std::uint64_t>(node.id());
		if (!seen_node_ids.emplace(id).second)
			return;
		arnis::tags_t tags;
		for (const auto &tag : node.tags())
			tags.emplace(tag.key(), tag.value());
		arnis::osm_parser::filter_tags(tags);
		if (tags.empty())
			return;
		tagged_node_tags.emplace(id, tags);
		arnis::WorldEditor editor;
		editor.mg = mg;
		editor.set_ground_origin(mg->node_min.X, mg->node_min.Z);
		const auto position = mg->ll_to_pos({static_cast<ll_t>(node.location().lat()),
				static_cast<ll_t>(node.location().lon())});
		const int x = position.X;
		const int z = position.Y;
		arnis::ProcessedNode processed_node;
		processed_node.id = id;
		processed_node.tags = std::move(tags);
		processed_node.x = x;
		processed_node.z = z;
		processed_node.y = editor.node_to_position(node).Y;
		processed_node.latitude = node.location().lat();
		processed_node.longitude = node.location().lon();
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
		arnis::osm_parser::filter_tags(processed_way.tags);
		for (const auto &node : way.nodes()) {
			arnis::ProcessedNode processed_node;
			const auto node_id = static_cast<std::uint64_t>(node.ref());
			if (const auto found = tagged_node_tags.find(node_id);
					found != tagged_node_tags.end())
				processed_node.tags = found->second;
			const auto [x, z] = editor.node_to_xz(node);
			processed_node.x = x;
			processed_node.z = z;
			processed_node.y = editor.node_to_position(node).Y;
			if (node.location()) {
				processed_node.latitude = node.location().lat();
				processed_node.longitude = node.location().lon();
			}
			processed_node.id = node_id;
			processed_way.nodes.emplace_back(std::move(processed_node));
		}
		way_indices.emplace(id, elements.size());
		elements.emplace_back(std::move(processed_way));
	}

	void way(const osmium::Way &way) { append_way(way); }

	void relation(const osmium::Relation &relation)
	{
		const auto id = static_cast<std::uint64_t>(relation.id());
		if (!seen_relation_ids.emplace(id).second)
			return;
		arnis::ProcessedRelation processed;
		processed.id = id;
		for (const auto &tag : relation.tags())
			processed.tags.emplace(tag.key(), tag.value());
		arnis::osm_parser::filter_tags(processed.tags);
		const auto type = processed.tags.get("type");
		if (type != "multipolygon" && type != "building")
			return;
		const bool building_relation = type == "building" ||
									   processed.tags.contains("building") ||
									   processed.tags.contains("building:part");
		PendingRelation pending;
		pending.relation = std::move(processed);
		for (const auto &member : relation.members()) {
			const auto member_id = static_cast<std::uint64_t>(member.ref());
			std::string member_type;
			switch (member.type()) {
			case osmium::item_type::node:
				member_type = "node";
				break;
			case osmium::item_type::way:
				member_type = "way";
				break;
			case osmium::item_type::relation:
				member_type = "relation";
				break;
			default:
				continue;
			}
			pending.relation.source_members.push_back(
					{member_type, member_id, std::string(member.role())});
			if (member.type() != osmium::item_type::way)
				continue;
			const auto way_id = member_id;
			const auto found = way_indices.find(way_id);
			if (found == way_indices.end())
				continue;
			std::string role = member.role();
			const auto first = role.find_first_not_of(" \t\r\n");
			const auto last = role.find_last_not_of(" \t\r\n");
			role = first == std::string::npos ? std::string{}
											  : role.substr(first, last - first + 1);
			std::transform(role.begin(), role.end(), role.begin(),
					[](unsigned char c) { return static_cast<char>(std::tolower(c)); });
			std::optional<arnis::ProcessedMemberRole> processed_role;
			if (role == "outer" || role == "outline")
				processed_role = arnis::ProcessedMemberRole::Outer;
			else if (role == "inner")
				processed_role = arnis::ProcessedMemberRole::Inner;
			else if (role == "part" && type == "building")
				processed_role = arnis::ProcessedMemberRole::Part;
			else if (role != "part" && building_relation)
				processed_role = arnis::ProcessedMemberRole::Outer;
			if (!processed_role)
				continue;
			pending.ways.emplace_back(way_id, *processed_role);
		}
		if (!pending.ways.empty())
			pending_relations.push_back(std::move(pending));
	}

	void finish_relations()
	{
		for (auto &pending : pending_relations) {
			const XZBBox bbox(
					mg->node_min.X, mg->node_min.Z, mg->node_max.X, mg->node_max.Z);
			for (const auto &[way_id, role] : pending.ways) {
				const auto found = way_indices.find(way_id);
				if (found == way_indices.end())
					continue;
				pending.relation.members.push_back(
						{elements[found->second].as_way(), role});
			}
			const auto &tags = pending.relation.tags;
			const bool filled_area = tags.contains("natural") ||
									 tags.contains("landuse") || tags.contains("leisure");
			const bool water_area =
					tags.contains("water") ||
					(tags.get("natural") == "water" || tags.get("natural") == "bay") ||
					tags.get("waterway") == "dock";
			const bool building_area =
					tags.contains("building") || tags.contains("building:part");
			const bool keep_unclipped =
					water_area || (building_area && tags.get("type") == "multipolygon");
			const auto is_open = [](const arnis::ProcessedWay &way) {
				if (way.nodes.size() < 3)
					return true;
				const auto &first = way.nodes.front();
				const auto &last = way.nodes.back();
				return first.id != last.id && (first.x != last.x || first.z != last.z);
			};
			const bool needs_ring_assembly =
					!water_area && !building_area && tags.get("type") == "multipolygon" &&
					filled_area &&
					std::any_of(pending.relation.members.begin(),
							pending.relation.members.end(), [&](const auto &member) {
								return member.role != arnis::ProcessedMemberRole::Part &&
									   is_open(member.way);
							});
			if (needs_ring_assembly) {
				std::vector<std::vector<arnis::ProcessedNode>> outer_rings;
				std::vector<std::vector<arnis::ProcessedNode>> inner_rings;
				for (const auto &member : pending.relation.members) {
					auto *rings = member.role == arnis::ProcessedMemberRole::Outer
										  ? &outer_rings
								  : member.role == arnis::ProcessedMemberRole::Inner
										  ? &inner_rings
										  : nullptr;
					if (rings && member.way.nodes.size() >= 2)
						rings->push_back(member.way.nodes);
				}
				std::function<void(std::vector<std::vector<arnis::ProcessedNode>> &)>
						merge_segments;
				merge_segments = [&](auto &rings) {
					const auto matches = [](const auto &a, const auto &b) {
						return a.id == b.id ||
							   (std::abs(a.x - b.x) <= 1 && std::abs(a.z - b.z) <= 1);
					};
					std::vector<bool> removed(rings.size(), false);
					std::vector<std::vector<arnis::ProcessedNode>> merged;
					for (std::size_t i = 0; i < rings.size(); ++i) {
						for (std::size_t j = 0; j < rings.size(); ++j) {
							if (i == j || removed[i] || removed[j] || rings[i].empty() ||
									rings[j].empty())
								continue;
							const auto &a = rings[i];
							const auto &b = rings[j];
							if (matches(a.front(), a.back()) ||
									matches(b.front(), b.back()))
								continue;
							std::vector<arnis::ProcessedNode> joined;
							if (matches(a.front(), b.front())) {
								joined.assign(a.rbegin(), a.rend());
								joined.insert(
										joined.end(), std::next(b.begin()), b.end());
							} else if (matches(a.back(), b.back())) {
								joined = a;
								joined.insert(
										joined.end(), std::next(b.rbegin()), b.rend());
							} else if (matches(a.front(), b.back())) {
								joined = b;
								joined.insert(
										joined.end(), std::next(a.begin()), a.end());
							} else if (matches(a.back(), b.front())) {
								joined = a;
								joined.insert(
										joined.end(), std::next(b.begin()), b.end());
							} else {
								continue;
							}
							removed[i] = removed[j] = true;
							merged.push_back(std::move(joined));
						}
					}
					for (std::size_t i = removed.size(); i > 0; --i)
						if (removed[i - 1])
							rings.erase(
									rings.begin() + static_cast<std::ptrdiff_t>(i - 1));
					const auto merged_count = merged.size();
					for (auto &ring : merged)
						rings.push_back(std::move(ring));
					if (merged_count > 0)
						merge_segments(rings);
				};
				merge_segments(outer_rings);
				merge_segments(inner_rings);
				std::vector<arnis::ProcessedMember> assembled;
				for (const auto &[role, rings] :
						{std::pair{arnis::ProcessedMemberRole::Outer, &outer_rings},
								std::pair{arnis::ProcessedMemberRole::Inner,
										&inner_rings}}) {
					std::size_t ring_index = 0;
					for (auto ring : *rings) {
						const std::size_t current_index = ring_index++;
						if (ring.size() < 3)
							continue;
						const auto &first = ring.front();
						const auto &last = ring.back();
						if (first.id != last.id) {
							if (std::abs(first.x - last.x) > 1 ||
									std::abs(first.z - last.z) > 1)
								continue;
							ring.push_back(first);
						}
						auto clipped = arnis::clipping::clip_way_to_bbox(ring, bbox);
						if (clipped.size() < 4)
							continue;
						arnis::ProcessedWay way;
						way.id = (std::uint64_t{1} << 61) |
								 ((pending.relation.id & ((std::uint64_t{1} << 45) - 1))
										 << 16) |
								 (role == arnis::ProcessedMemberRole::Inner ? (1U << 15)
																			: 0U) |
								 (current_index & 0x7fff);
						way.nodes = std::move(clipped);
						assembled.push_back({std::move(way), role});
					}
				}
				pending.relation.members = std::move(assembled);
			} else if (!keep_unclipped) {
				std::vector<arnis::ProcessedMember> clipped_members;
				clipped_members.reserve(pending.relation.members.size());
				for (auto &member : pending.relation.members) {
					auto clipped =
							arnis::clipping::clip_way_to_bbox(member.way.nodes, bbox);
					if (clipped.empty())
						continue;
					member.way.nodes = std::move(clipped);
					clipped_members.push_back(std::move(member));
				}
				pending.relation.members = std::move(clipped_members);
			}
			if (!pending.relation.members.empty())
				elements.emplace_back(std::move(pending.relation));
		}
	}
};

namespace earth_osmium_detail
{

arnis::Args earth_arnis_args()
{
	// Freeminer owns the application cache directory; all Arnis providers derive
	// their cache subdirectories from this one configured base.
	::arnis::cache::set_base_directory(
			std::filesystem::path(porting::path_cache) / "earth" / "arnis");
	// Assets are installed independently from the executable and cache tree.
	::arnis::assets::set_base_directory(
			std::filesystem::path(porting::path_share) / "assets" / "arnis");
	::arnis::Args arnis;
	arnis.use_3d = true;
	arnis.interior = true;
	arnis.roof = true;
	arnis.signage = ::arnis::SignageLevel::Full;
	arnis.fillground = true;
	arnis.disable_height_limit = true;
	arnis.building_facades = true;
	arnis.facade_detail = ::arnis::FacadeDetail::High;
	arnis.caves = true;
	arnis.cave_biomes =
			"lush=100,dripstone=100,deepdark=100,mushroom=100,ice=100,amethyst=100,volcanic=100,coral=100";
	// Match Rust's default. The shared canopy cache keeps this single raster
	// available across chunk-generation threads without per-thread downloads.
	arnis.canopy_height = true;
	arnis.building_facades_dir = ::arnis::cache::facade_cache_root().string();
	const ::arnis::Args freeminer_defaults = arnis;

	// Settings::getNoEx only exposes strings, so parse values here while leaving
	// Args defaults intact whenever a setting is absent or malformed.
	const auto get_setting = [](const char *name, std::string &value) {
		return g_settings && g_settings->getNoEx(std::string("arnis.") + name, value);
	};
	const auto read_bool = [&](const char *name, bool &value) {
		std::string raw;
		if (!get_setting(name, raw))
			return;
		std::transform(raw.begin(), raw.end(), raw.begin(),
				[](unsigned char c) { return static_cast<char>(std::tolower(c)); });
		if (raw == "true" || raw == "yes" || raw == "1" || raw == "on")
			value = true;
		else if (raw == "false" || raw == "no" || raw == "0" || raw == "off")
			value = false;
	};
	const auto read_int = [&](const char *name, int &value) {
		std::string raw;
		if (!get_setting(name, raw))
			return;
		char *end = nullptr;
		errno = 0;
		const long parsed = std::strtol(raw.c_str(), &end, 10);
		while (end && std::isspace(static_cast<unsigned char>(*end)))
			++end;
		if (errno != ERANGE && end != raw.c_str() && end && *end == '\0' &&
				parsed >= std::numeric_limits<int>::min() &&
				parsed <= std::numeric_limits<int>::max())
			value = static_cast<int>(parsed);
	};
	const auto read_double = [&](const char *name, double &value) {
		std::string raw;
		if (!get_setting(name, raw))
			return;
		char *end = nullptr;
		const double parsed = std::strtod(raw.c_str(), &end);
		while (end && std::isspace(static_cast<unsigned char>(*end)))
			++end;
		if (end != raw.c_str() && end && *end == '\0' && std::isfinite(parsed))
			value = parsed;
	};
	const auto read_optional_double = [&](const char *name,
											  std::optional<double> &value) {
		std::string raw;
		if (!get_setting(name, raw))
			return;
		char *end = nullptr;
		const double parsed = std::strtod(raw.c_str(), &end);
		while (end && std::isspace(static_cast<unsigned char>(*end)))
			++end;
		if (end != raw.c_str() && end && *end == '\0' && std::isfinite(parsed))
			value = parsed;
	};
	const auto read_string = [&](const char *name, std::string &value) {
		std::string raw;
		if (get_setting(name, raw))
			value = std::move(raw);
	};
	const auto read_optional_string = [&](const char *name,
											  std::optional<std::string> &value) {
		std::string raw;
		if (get_setting(name, raw))
			value = std::move(raw);
	};
	const auto read_optional_bool = [&](const char *name, std::optional<bool> &value) {
		std::string raw;
		if (!get_setting(name, raw))
			return;
		std::transform(raw.begin(), raw.end(), raw.begin(),
				[](unsigned char c) { return static_cast<char>(std::tolower(c)); });
		if (raw == "true" || raw == "yes" || raw == "1" || raw == "on")
			value = true;
		else if (raw == "false" || raw == "no" || raw == "0" || raw == "off")
			value = false;
	};

	// Keep the complete CLI-facing input/output option surface configurable for
	// library users embedding Arnis in Freeminer. These are intentionally
	// optional: an absent setting leaves the host-provided/default value intact.
	read_optional_string("file", arnis.file);
	read_optional_string("save_json_file", arnis.save_json_file);
	read_optional_string("path", arnis.path);
	read_optional_string("world_name", arnis.world_name);
	read_bool("bedrock", arnis.bedrock);
	read_bool("luanti", arnis.luanti);

	// Core generation and terrain settings.
	read_double("scale", arnis.scale);
	read_double("height_multiplier", arnis.height_multiplier);
	read_double("celestial_latitude_degrees", arnis.celestial_latitude_degrees);
	read_int("ground_level", arnis.ground_level);
	read_double("rotation", arnis.rotation);
	read_bool("use_3d", arnis.use_3d);
	read_bool("interior", arnis.interior);
	read_bool("roof", arnis.roof);
	read_bool("fillground", arnis.fillground);
	read_bool("disable_height_limit", arnis.disable_height_limit);
	read_bool("legacy_terrain", arnis.legacy_terrain);
	read_bool("legacy_trees", arnis.legacy_trees);
	read_bool("canopy_height", arnis.canopy_height);
	read_bool("overture", arnis.overture);
	read_bool("aws_only_elevation", arnis.aws_only_elevation);
	read_bool("city_boundaries", arnis.city_boundaries);
	read_bool("caves", arnis.caves);
	read_bool("benchmark", arnis.benchmark);
	read_bool("debug", arnis.debug);
	{
		std::string mode;
		if (get_setting("mode", mode)) {
			arnis.mode = ::arnis::generation_mode_from_string(mode);
		} else {
			// `terrain` is the legacy boolean equivalent of `mode`. Keep it
			// configurable for existing Freeminer settings, while the richer
			// mode setting takes precedence when both are present.
			bool terrain = arnis.terrain;
			read_bool("terrain", terrain);
			arnis.mode = terrain ? ::arnis::GenerationMode::GeoTerrain
								 : ::arnis::GenerationMode::GeoOnly;
		}
	}
	arnis.apply_mode_defaults();
	{
		std::string body;
		if (get_setting("body", body))
			arnis.body = ::arnis::celestial_body_from_string(body);
	}
	{
		std::string projection;
		if (get_setting("projection", projection))
			arnis.projection =
					::arnis::projection::projection_kind_from_string(projection);
	}
	{
		std::string size;
		if (get_setting("max_tree_size", size))
			arnis.max_tree_size = ::arnis::trees::tree_size_from_string(size);
	}

	// Facade, Mapillary, signage, and Overture options.
	read_bool("building_facades", arnis.building_facades);
	read_optional_bool("mapillary_facades", arnis.mapillary_facades);
	read_bool("mapillary_probe", arnis.mapillary_probe);
	{
		int pixels = static_cast<int>(arnis.facade_px);
		read_int("facade_px", pixels);
		if (pixels >= 0)
			arnis.facade_px = static_cast<std::uint32_t>(pixels);
	}
	{
		std::string detail;
		if (get_setting("facade_detail", detail))
			arnis.facade_detail = ::arnis::facade_detail_from_string(detail);
	}
	{
		std::string mode;
		if (get_setting("mapillary_facade_mode", mode))
			arnis.mapillary_facade_mode = ::arnis::facade_mode_from_string(mode);
	}
	{
		std::string level;
		if (get_setting("signage", level))
			arnis.signage = ::arnis::signage_level_from_string(level);
	}
	{
		std::string source;
		if (get_setting("overture_source", source))
			arnis.overture_source = ::arnis::overture_source_from_string(source);
	}
	std::string mapillary_token;
	read_string("mapillary_token", mapillary_token);
	if (!mapillary_token.empty()) {
		if (!arnis.mapillary_facades.has_value())
			arnis.mapillary_facades = true;
		std::string probe_setting;
		if (!get_setting("mapillary_probe", probe_setting))
			arnis.mapillary_probe = true;
		arnis.mapillary_token = mapillary_token;
	}
	read_optional_string("mapillary_debug_dir", arnis.mapillary_debug_dir);
	read_optional_string("mapillary_facades_dir", arnis.mapillary_facades_dir);
	read_optional_string("mapillary_facade_debug_dir", arnis.mapillary_facade_debug_dir);
	read_string("mapillary_facade_debug_walls", arnis.mapillary_facade_debug_walls);
	read_optional_string("building_facades_dir", arnis.building_facades_dir);

	// Cave tuning and tree-library paths.
	read_optional_string("cave_biomes", arnis.cave_biomes);
	read_optional_string("cave_asset_pack", arnis.cave_asset_pack);
	read_string("downloader", arnis.downloader);
	read_string("osm_tiles_url", arnis.osm_tiles_url);
	read_bool("no_tile_archive", arnis.no_tile_archive);
	read_bool("bake_lighting", arnis.bake_lighting);
	read_bool("map_preview", arnis.map_preview);
	read_bool("map_item", arnis.map_item);
	read_bool("voxy_lod", arnis.voxy_lod);
	{
		std::string raw;
		if (get_setting("world_time", raw)) {
			char *end = nullptr;
			errno = 0;
			const long long parsed = std::strtoll(raw.c_str(), &end, 10);
			while (end && std::isspace(static_cast<unsigned char>(*end)))
				++end;
			if (errno != ERANGE && end != raw.c_str() && end && *end == '\0')
				arnis.world_time = static_cast<std::int64_t>(parsed);
		}
	}
	{
		std::optional<double> latitude;
		std::optional<double> longitude;
		read_optional_double("spawn_lat", latitude);
		read_optional_double("spawn_lng", longitude);
		arnis.spawn_lat = latitude;
		arnis.spawn_lng = longitude;
	}
	{
		std::string gamemode;
		if (get_setting("gamemode", gamemode))
			arnis.gamemode = ::arnis::game_mode_from_string(gamemode);
	}
	{
		std::optional<double> seconds;
		read_optional_double("timeout", seconds);
		if (seconds && *seconds >= 0.0 &&
				*seconds <=
						static_cast<double>(std::numeric_limits<std::int64_t>::max()) /
								1000.0)
			arnis.timeout = std::chrono::milliseconds(
					static_cast<std::int64_t>(*seconds * 1000.0));
	}
	// Rust applies celestial defaults after parsing all CLI options. Keep the
	// settings-backed Freeminer adapter on the same path so a non-Earth body
	// cannot accidentally retain Earth-only providers or structure passes.
	arnis.apply_body_defaults();
	arnis.apply_mode_defaults();
	// A malformed or mutually incompatible optional setting must not disable
	// the entire Freeminer mapgen. Keep the host's known-good defaults and let
	// generation proceed; valid settings continue to override them normally.
	if (!arnis.valid()) {
		static std::once_flag invalid_settings_warning;
		std::call_once(invalid_settings_warning, [] {
			errorstream << "Earth: invalid or incompatible arnis.* settings; using "
						   "Freeminer Arnis defaults for this generation\n";
		});
		arnis = freeminer_defaults;
	}

	return arnis;
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
				maximum, mg->get_height(static_cast<pos_t>(x), static_cast<pos_t>(z), 0));
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

struct EarthElementBounds
{
	int min_x = std::numeric_limits<int>::max();
	int min_z = std::numeric_limits<int>::max();
	int max_x = std::numeric_limits<int>::lowest();
	int max_z = std::numeric_limits<int>::lowest();
	bool valid = false;
};

EarthElementBounds earth_element_bounds(const arnis::ProcessedElement &element)
{
	EarthElementBounds bounds;
	const auto include = [&bounds](int x, int z) {
		bounds.min_x = std::min(bounds.min_x, x);
		bounds.min_z = std::min(bounds.min_z, z);
		bounds.max_x = std::max(bounds.max_x, x);
		bounds.max_z = std::max(bounds.max_z, z);
		bounds.valid = true;
	};
	if (element.is_node()) {
		const auto &node = element.as_node();
		include(node.x, node.z);
	} else if (element.is_way()) {
		for (const auto &node : element.as_way().nodes)
			include(node.x, node.z);
	} else {
		for (const auto &member : element.as_relation().members)
			for (const auto &node : member.way.nodes)
				include(node.x, node.z);
	}
	return bounds;
}

bool earth_bounds_intersect(const EarthElementBounds &element, const XZBBox &bbox)
{
	return element.valid && element.max_x >= bbox.min_x() &&
		   element.min_x <= bbox.max_x() && element.max_z >= bbox.min_z() &&
		   element.min_z <= bbox.max_z();
}

std::optional<arnis::geographic::LLBBox> earth_elements_geographic_bounds(
		const std::vector<arnis::ProcessedElement> &elements)
{
	double min_lat = std::numeric_limits<double>::infinity();
	double min_lon = std::numeric_limits<double>::infinity();
	double max_lat = -std::numeric_limits<double>::infinity();
	double max_lon = -std::numeric_limits<double>::infinity();
	const auto include = [&](const arnis::ProcessedNode &node) {
		if (!std::isfinite(node.latitude) || !std::isfinite(node.longitude) ||
				node.latitude < -90.0 || node.latitude > 90.0 ||
				node.longitude < -180.0 || node.longitude > 180.0)
			return;
		min_lat = std::min(min_lat, node.latitude);
		min_lon = std::min(min_lon, node.longitude);
		max_lat = std::max(max_lat, node.latitude);
		max_lon = std::max(max_lon, node.longitude);
	};
	for (const auto &element : elements) {
		if (element.is_node()) {
			include(element.as_node());
		} else if (element.is_way()) {
			for (const auto &node : element.as_way().nodes)
				include(node);
		} else {
			for (const auto &member : element.as_relation().members)
				for (const auto &node : member.way.nodes)
					include(node);
		}
	}
	if (!std::isfinite(min_lat) || !std::isfinite(min_lon) || !std::isfinite(max_lat) ||
			!std::isfinite(max_lon))
		return std::nullopt;
	return arnis::geographic::LLBBox(min_lat, min_lon, max_lat, max_lon);
}

struct CachedArnisChunk
{
	std::once_flag elements_once;
	std::once_flag flood_once;
	std::once_flag footprints_once;
	std::mutex flood_wave_mutex;
	std::size_t active_generators = 0;
	bool flood_released = false;
	std::vector<arnis::ProcessedElement> elements;
	std::unique_ptr<arnis::FloodFillCache> flood_fill_cache;
	std::shared_ptr<arnis::BuildingFootprintBitmap> building_footprints;
};

struct CachedArnisExtract
{
	std::once_flag parse_once;
	std::mutex chunks_mutex;
	std::unordered_map<EarthHorizontalKey, std::shared_ptr<CachedArnisChunk>,
			EarthHorizontalKeyHash>
			chunks;
	std::vector<arnis::ProcessedElement> elements;
	std::vector<EarthElementBounds> element_bounds;
	std::unique_ptr<arnis::PreparedBuildingData> prepared_buildings;
	std::shared_ptr<arnis::mapillary::FacadeJob> facade_job;
	pos_t authored_max_y = std::numeric_limits<pos_t>::lowest();
};

class FloodWaveGuard
{
	CachedArnisChunk &cached;

public:
	explicit FloodWaveGuard(CachedArnisChunk &cached) : cached(cached)
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

void generate_cached_arnis(
		MapgenEarth *mg, const CachedArnisExtract &tile, CachedArnisChunk &chunk)
{
	if (chunk.elements.empty() || !chunk.flood_fill_cache || !chunk.building_footprints)
		return;
	arnis::Ground ground;
	ground.mg = mg;
	arnis::WorldEditor editor;
	editor.set_generation_error_sink([](const std::string &message) {
		errorstream << "Earth: Arnis generation error: " << message << '\n';
	});
	editor.mg = mg;
	if (mg->projection.curved) {
		const auto anchor = mg->pos_to_ll((mg->node_min + mg->node_max) / 2);
		editor.projection_frame.bind(mg, anchor.lat, anchor.lon, 0.0);
	}
	editor.set_ground_origin(mg->node_min.X, mg->node_min.Z);
	// Entity/decal ownership must match block ownership so off-chunk signage
	// is rejected before terrain queries and PNG texture generation.
	editor.set_strict_bounds(
			mg->node_min.X, mg->node_min.Z, mg->node_max.X, mg->node_max.Z);
	editor.set_tile_hooks(
			[mg](int min_x, int min_z, int max_x, int max_z) {
				return mg->beginTileOverlay(min_x, min_z, max_x, max_z);
			},
			[mg](int, int, int, int) { return mg->mergeTileOverlay(); });
	editor.ground = &ground;
	editor.set_schem_entity_sink(
			[mg](int x, int y, int z, const std::vector<std::uint8_t> &nbt) {
				mg->queueGeneratedSchemEntity(v3pos_t(x, y, z), nbt);
			});
	const auto args = earth_arnis_args();
	// Raster providers use the editor bounds, even when the host enters through
	// generate_world rather than generate_world_with_options.
	const pos_t sample_y = ((mg->node_min + mg->node_max) / 2).Y;
	const std::array corners{
			mg->pos_to_ll(v3pos_t(mg->node_min.X, sample_y, mg->node_min.Z)),
			mg->pos_to_ll(v3pos_t(mg->node_max.X, sample_y, mg->node_min.Z)),
			mg->pos_to_ll(v3pos_t(mg->node_min.X, sample_y, mg->node_max.Z)),
			mg->pos_to_ll(v3pos_t(mg->node_max.X, sample_y, mg->node_max.Z))};
	double min_lat = corners.front().lat, max_lat = min_lat;
	double min_lon = corners.front().lon, max_lon = min_lon;
	for (const auto &corner : corners) {
		min_lat = std::min(min_lat, corner.lat);
		max_lat = std::max(max_lat, corner.lat);
		min_lon = std::min(min_lon, corner.lon);
		max_lon = std::max(max_lon, corner.lon);
	}
	editor.set_geographic_bounds(min_lat, max_lat, min_lon, max_lon);
	if (!arnis::generate_world(editor, chunk.elements, args, *chunk.flood_fill_cache,
				*chunk.building_footprints, true, tile.prepared_buildings.get(),
				tile.facade_job.get()))
		errorstream << "Earth: Arnis world generation failed; check generation options "
					   "and provider configuration\n";
}

} // namespace earth_osmium_detail

class hdl : public handler_i
{
	using index_t = osmium::index::map::SparseMemArray<osmium::unsigned_object_id_type,
			osmium::Location>;
	using cache_t = osmium::handler::NodeLocationsForWays<index_t>;

	const std::string path_name;
	// The input path identifies a reusable extracted OSM tile. Parsed/projected
	// elements are shared; bounded chunk views and their flood fills are cached
	// separately so each mapchunk does not dispatch the entire source tile.
	earth_osmium_detail::CachedArnisExtract cached;

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

		try {
			std::call_once(cached.parse_once, [&]() {
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
				const auto source_header = reader.header();
				const auto source_bounds = source_header.joined_boxes();
				MyHandler handler;
				handler.mg = mg;
				osmium::apply(reader, node_cache, handler,
						mp_manager.handler(
								[&handler](const osmium::memory::Buffer &area_buffer) {
									osmium::apply(area_buffer, handler);
								}));
				handler.finish_relations();
				cached.elements = std::move(handler.elements);
				const auto args = earth_osmium_detail::earth_arnis_args();
				std::optional<arnis::geographic::LLBBox> bbox;
				if (source_bounds.valid()) {
					bbox.emplace(source_bounds.bottom_left().lat(),
							source_bounds.bottom_left().lon(),
							source_bounds.top_right().lat(),
							source_bounds.top_right().lon());
				} else {
					bbox = earth_osmium_detail::earth_elements_geographic_bounds(
							cached.elements);
				}
				if (bbox) {
					const std::array<double, 4> geographic_bounds = {bbox->min().lat(),
							bbox->max().lat(), bbox->min().lng(), bbox->max().lng()};
					cached.facade_job =
							arnis::start_mapillary_facade_job(args, geographic_bounds);
				}
				if (args.overture && !args.skip_objects()) {
					if (bbox) {
						try {
							actionstream
									<< "Earth: fetching Overture building data for cached OSM extract\n";
							auto overture = arnis::overture::fetch_overture_data(
									bbox->min().lat(), bbox->min().lng(),
									bbox->max().lat(), bbox->max().lng(), args.scale,
									args.debug, args.overture_source);
							for (auto &element : overture.elements) {
								if (!element.is_way())
									continue;
								auto &way = std::get<arnis::ProcessedWay>(
										static_cast<arnis::variant_t &>(element));
								for (auto &node : way.nodes) {
									if (!std::isfinite(node.latitude) ||
											!std::isfinite(node.longitude))
										continue;
									const auto position = mg->ll_to_pos(
											{static_cast<ll_t>(node.latitude),
													static_cast<ll_t>(node.longitude)});
									node.x = position.X;
									node.z = position.Y;
								}
							}
							const auto [enriched, added] =
									overture.merge_into(cached.elements);
							if (enriched || added)
								actionstream << "Earth: Overture enriched " << enriched
											 << " OSM building(s), added " << added
											 << " footprint(s)\n";
						} catch (const std::exception &e) {
							errorstream
									<< "Earth: Overture fetch failed; continuing with OSM data: "
									<< e.what() << '\n';
						}
					} else if (args.debug) {
						errorstream
								<< "Earth: Overture skipped; cached OSM extract has no geographic bounds\n";
					}
				}
				const auto dropped = arnis::drop_buildings_on_aircraft_pavement(
						cached.elements, args.scale);
				if (dropped > 0)
					actionstream << "Earth: skipped " << dropped
								 << " building(s) on aircraft pavement\n";
				cached.prepared_buildings = std::make_unique<arnis::PreparedBuildingData>(
						arnis::prepare_building_data(cached.elements));
				// The flat-world terrain-max scan samples every X/Z column. On
				// curved projections that becomes a very expensive cube conversion
				// loop and does not describe a single horizontal ceiling.
				const pos_t terrain_max = earth_osmium_detail::earth_element_terrain_max(
						mg, cached.elements, std::numeric_limits<pos_t>::lowest());
				const pos_t margin = earth_osmium_detail::earth_authored_height_margin(
						cached.elements);
				const long double maximum =
						static_cast<long double>(terrain_max) + margin;
				cached.authored_max_y =
						maximum >= static_cast<long double>(
										   std::numeric_limits<pos_t>::max())
								? std::numeric_limits<pos_t>::max()
								: static_cast<pos_t>(maximum);
				arnis::prepare_elements_for_generation(cached.elements);
				cached.element_bounds.reserve(cached.elements.size());
				for (const auto &element : cached.elements)
					cached.element_bounds.push_back(
							earth_osmium_detail::earth_element_bounds(element));
			});

			mg->cacheAuthoredMaxY(cached.authored_max_y);
			if (mg->node_min.Y > cached.authored_max_y)
				return;
			if (cached.elements.empty())
				return;

			std::shared_ptr<earth_osmium_detail::CachedArnisChunk> chunk;
			{
				std::lock_guard<std::mutex> lock(cached.chunks_mutex);
				const auto found = cached.chunks.find(key);
				if (found != cached.chunks.end()) {
					chunk = found->second;
				} else {
					// Keep a small set of vertically reusable chunk views. In-flight
					// chunks remain alive through their shared_ptr even if evicted.
					if (cached.chunks.size() >= 4)
						cached.chunks.erase(cached.chunks.begin());
					chunk = std::make_shared<earth_osmium_detail::CachedArnisChunk>();
					cached.chunks.emplace(key, chunk);
				}
			}

			std::call_once(chunk->elements_once, [&]() {
				constexpr int extra = MAP_BLOCKSIZE * 2;
				const XZBBox context_bbox(mg->node_min.X - extra, mg->node_min.Z - extra,
						mg->node_max.X + extra, mg->node_max.Z + extra);
				for (std::size_t i = 0; i < cached.elements.size(); ++i)
					if (earth_osmium_detail::earth_bounds_intersect(
								cached.element_bounds[i], context_bbox))
						chunk->elements.push_back(cached.elements[i]);
			});
			if (chunk->elements.empty())
				return;

			std::call_once(chunk->flood_once, [&]() {
				auto args = earth_osmium_detail::earth_arnis_args();
				auto flood =
						arnis::FloodFillCache::precompute(chunk->elements, args.timeout);
				flood.retain_entries();
				chunk->flood_fill_cache =
						std::make_unique<arnis::FloodFillCache>(std::move(flood));
			});

			// The parsed tile stays shared, but expensive dispatch/fill preparation
			// uses only this chunk's spatial view plus the original two-mapblock halo.
			earth_osmium_detail::FloodWaveGuard flood_wave(*chunk);
			std::call_once(chunk->footprints_once, [&]() {
				XZBBox xzbbox(
						mg->node_min.X, mg->node_min.Z, mg->node_max.X, mg->node_max.Z);
				chunk->building_footprints =
						std::make_shared<arnis::BuildingFootprintBitmap>(
								chunk->flood_fill_cache->collect_building_footprints(
										chunk->elements, xzbbox));
			});
			arnis::init(mg);
			earth_osmium_detail::generate_cached_arnis(mg, cached, *chunk);
		} catch (const std::exception &ex) {
			errorstream << "Earth exception: " << ex.what() << "\n";
		}
	}
};

namespace earth_osmium_detail
{

std::shared_ptr<handler_i> make_handler(MapgenEarth *mg, const std::string &path)
{
	return std::make_shared<hdl>(mg, path);
}

} // namespace earth_osmium_detail
