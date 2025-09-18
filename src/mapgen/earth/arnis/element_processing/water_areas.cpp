
#include <vector>
#include <string>
#include <map>
#include <chrono>
#include <iostream>
#include <optional>
#include <algorithm>

#include <boost/geometry.hpp>
#include <boost/geometry/geometries/point_xy.hpp>
#include <boost/geometry/geometries/linestring.hpp>
#include <boost/geometry/geometries/polygon.hpp>
#include <boost/geometry/geometries/box.hpp>

//#include "block_definitions.h"
//#include "coordin_system/caesian/XZPoint.h"
//#include "osm_parser.h"
//#include "world_editor.h"

#include "../../arnis_adapter.h"
#undef stoi
namespace arnis
{

namespace water_areas {

using BgPoint = boost::geometry::model::d2::point_xy<double>;
using BgLinestring = boost::geometry::model::linestring<BgPoint>;
using BgPolygon = boost::geometry::model::polygon<BgPoint>;
using BgBox = boost::geometry::model::box<BgPoint>;

static void merge_loopy_loops(std::vector<std::vector<ProcessedNode>> &loops)
{
	std::vector<std::size_t> removed;
	std::vector<std::vector<ProcessedNode>> merged;

	for (std::size_t i = 0; i < loops.size(); ++i) {
		for (std::size_t j = 0; j < loops.size(); ++j) {
			if (i == j) {
				continue;
			}
			if (std::find(removed.begin(), removed.end(), i) != removed.end() ||
					std::find(removed.begin(), removed.end(), j) != removed.end()) {
				continue;
			}

			const std::vector<ProcessedNode> &x = loops[i];
			const std::vector<ProcessedNode> &y = loops[j];

if			(x.empty() || y.empty())
			{
				continue;
			}

			if (x.front().id == x.back().id) {
				continue;
			}
			if (y.front().id == y.back().id) {
				continue;
			}

			if (x.front().id == y.front().id) {
				removed.push_back(i);
				removed.push_back(j);

				std::vector<ProcessedNode> r = x;
				std::reverse(r.begin(), r.end());
				r.insert(r.end(), y.begin() + 1, y.end());
				merged.push_back(std::move(r));
			} else if (x.back().id == y.back().id) {
				removed.push_back(i);
				removed.push_back(j);

				std::vector<ProcessedNode> r = x;
				r.insert(r.end(), y.rbegin() + 1, y.rend());
				std::reverse(r.begin() + x.size(),
						r.end()); // correct ordering after insert from reverse iterator
				merged.push_back(std::move(r));
			} else if (x.front().id == y.back().id) {
				removed.push_back(i);
				removed.push_back(j);

				std::vector<ProcessedNode> r = y;
				r.insert(r.end(), x.begin() + 1, x.end());
				merged.push_back(std::move(r));
			} else if (x.back().id == y.front().id) {
				removed.push_back(i);
				removed.push_back(j);

				std::vector<ProcessedNode> r = x;
				r.insert(r.end(), y.begin() + 1, y.end());
				merged.push_back(std::move(r));
			}
		}
	}

	std::sort(removed.begin(), removed.end());
	for (auto it = removed.rbegin(); it != removed.rend(); ++it) {
		if (*it < loops.size()) {
			loops.erase(loops.begin() + *it);
		}
	}

	std::size_t merged_len = merged.size();
	for (auto &m : merged) {
		loops.push_back(std::move(m));
	}

	if (merged_len > 0) {
		merge_loopy_loops(loops);
	}
}

static bool verify_loopy_loops(const std::vector<std::vector<ProcessedNode>> &loops)
{
	bool valid = true;
	for (const auto &l : loops) {
		if (l.empty() || l.front().id != l.back().id) {
			std::cerr << "WARN: Disconnected loop" << std::endl;
			valid = false;
		}
	}
	return valid;
}

static void rect_fill(
		int min_x, int max_x, int min_z, int max_z, int ground_level, WorldEditor &editor)
{
	for (int x = min_x; x < max_x; ++x) {
		for (int z = min_z; z < max_z; ++z) {
			editor.set_block(block_definitions::WATER, x, ground_level, z,
					std::optional<int>{}, std::optional<int>{});
		}
	}
}

static void inverse_floodfill_iterative(const std::pair<int, int> &min,
		const std::pair<int, int> &max, int ground_level,
		const std::vector<BgPolygon> &outers, const std::vector<BgPolygon> &inners,
		WorldEditor &editor)
{
	for (int x = min.first; x < max.first; ++x) {
		for (int z = min.second; z < max.second; ++z) {
			BgPoint p(static_cast<double>(x), static_cast<double>(z));
			bool in_outer = false;
			for (const auto &poly : outers) {
				if (boost::geometry::within(p, poly)) {
					in_outer = true;
					break;
				}
			}
			if (!in_outer) {
				continue;
			}
			bool in_inner = false;
			for (const auto &poly : inners) {
				if (boost::geometry::within(p, poly)) {
					in_inner = true;
					break;
				}
			}
			if (!in_inner) {
				editor.set_block(block_definitions::WATER, x, ground_level, z,
						std::optional<int>{}, std::optional<int>{});
			}
		}
	}
}
/*
static void inverse_floodfill_recursive(const std::pair<int, int> &min,
		const std::pair<int, int> &max, const std::vector<BgPolygon> &outers,
		const std::vector<BgPolygon> &inners,

		WorldEditor &editor, const std::chrono::steady_clock::time_point &start_time)
{
	if (std::chrono::duration_cast<std::chrono::seconds>(
				std::chrono::steady_clock::now() - start_time)
					.count() > 25) {
		std::cout << "Water area generation exceeded 25 seconds, continuing anyway"
				  << std::endl;
	}

	const long long ITERATIVE_THRES = 10000LL;

	if (min.first > max.first || min.second > max.second) {
		return;
	}

	long long dx = static_cast<long long>(max.first - min.first);
	long long dz = static_cast<long long>(max.second - min.second);
	if (dx * dz < ITERATIVE_THRES) {
		inverse_floodfill_iterative(min, max, 0, outers, inners, editor);
		return;
	}

	int center_x = (min.first + max.first) / 2;
	int center_z = (min.second + max.second) / 2;
	std::array<std::tuple<int, int, int, int>, 4> quadrants = {
			std::make_tuple(min.first, center_x, min.second, center_z),
			std::make_tuple(center_x, max.first, min.second, center_z),
			std::make_tuple(min.first, center_x, center_z, max.second),
			std::make_tuple(center_x, max.first, center_z, max.second)};

	for (auto [min_x, max_x, min_z, max_z] : quadrants) {
		BgBox rect(BgPoint(static_cast<double>(min_x), static_cast<double>(min_z)),
				BgPoint(static_cast<double>(max_x), static_cast<double>(max_z)));

		bool outer_contains = false;
		for (const auto &outer : outers) {
			if (boost::geometry::within(rect, outer)) {
				outer_contains = true;
				break;
			}
		}

		bool inner_intersects = false;
		for (const auto &inner : inners) {
			if (boost::geometry::intersects(inner, rect)) {
				inner_intersects = true;
				break;
			}
		}

		if (outer_contains && !inner_intersects) {
			rect_fill(min_x, max_x, min_z, max_z, 0, editor);
			continue;
		}

		std::vector<BgPolygon> outers_intersects;
		for (const auto &poly : outers) {
			if (boost::geometry::intersects(poly, rect)) {
				outers_intersects.push_back(poly);
			}
		}
		std::vector<BgPolygon> inners_intersects;
		for (const auto &poly : inners) {
			if (boost::geometry::intersects(poly, rect)) {
				inners_intersects.push_back(poly);
			}
		}

		if (!outers_intersects.empty()) {
			inverse_floodfill_recursive(std::make_pair(min_x, min_z),
					std::make_pair(max_x, max_z), outers_intersects, inners_intersects,
					editor, start_time);
		}
	}
}
*/
/*
void inverse_floodfill_iterative(
    std::pair<int32_t, int32_t> min,
    std::pair<int32_t, int32_t> max,
    int value,
    const std::vector<BgPolygon>& outers,
    const std::vector<BgPolygon>& inners,
    world::WorldEditor& editor);
*/

#if 0
void inverse_floodfill_recursive(
    std::pair<int32_t, int32_t> min,
    std::pair<int32_t, int32_t> max,    const std::vector<BgPolygon>& outers,
    const std::vector<BgPolygon>& inners,
    WorldEditor& editor,
    const std::chrono::steady_clock::time_point& start_time)
{
    using namespace std::chrono;
    constexpr int64_t ITERATIVE_THRES = 10'000;

    if (min.first > max.first || min.second > max.second) {
        return;
    }
/*
    if (duration<seconds>(steady_clock::now() - start_time).count() > 25) {
        std::cout << "Water area generation exceeded 25 seconds, continuing anyway\n";
    }
*/
    int64_t width = static_cast<int64_t>(max.first) - static_cast<int64_t>(min.first);
    int64_t height = static_cast<int64_t>(max.second) - static_cast<int64_t>(min.second);

    if (width * height < ITERATIVE_THRES) {
        inverse_floodfill_iterative(min, max, 0, outers, inners, editor);
        return;
    }

    int32_t center_x = (min.first + max.first) / 2;
    int32_t center_z = (min.second + max.second) / 2;

    std::array<std::tuple<int32_t, int32_t, int32_t, int32_t>, 4> quadrants = {{
        {min.first, center_x, min.second, center_z},
        {center_x, max.first, min.second, center_z},
        {min.first, center_x, center_z, max.second},
        {center_x, max.first, center_z, max.second}
    }};

    for (const auto& [min_x, max_x, min_z, max_z] : quadrants) {
        BgBox rect(BgPoint(static_cast<double>(min_x), static_cast<double>(min_z)),
                   BgPoint(static_cast<double>(max_x), static_cast<double>(max_z)));

        bool any_outer_contains = std::any_of(
            outers.begin(), outers.end(),
            [&rect](const BgPolygon& outer) {
                return boost::geometry::within(rect, outer);
            }
        );

        bool any_inner_intersects = std::any_of(
            inners.begin(), inners.end(),
            [&rect](const BgPolygon& inner) {
                return boost::geometry::intersects(inner, rect);
            }
        );

        if (any_outer_contains && !any_inner_intersects) {
            rect_fill(min_x, max_x, min_z, max_z, 0, editor);
            continue;
        }

        std::vector<BgPolygon> outers_intersects;
        std::copy_if(
            outers.begin(), outers.end(),
            std::back_inserter(outers_intersects),
            [&rect](const BgPolygon& poly) { return boost::geometry::intersects(poly, rect); }
        );

        std::vector<BgPolygon> inners_intersects;
        std::copy_if(
            inners.begin(), inners.end(),
            std::back_inserter(inners_intersects),
            [&rect](const BgPolygon& poly) { return boost::geometry::intersects(poly, rect); }
        );

        if (!outers_intersects.empty()) {
            inverse_floodfill_recursive(
                {min_x, min_z},
                {max_x, max_z},
                outers_intersects,
                inners_intersects,
                editor,
                start_time
            );
        }
    }
}
#endif
/*
#include <vector>
#include <chrono>
#include <iostream>
#include <algorithm>
#include <boost/geometry.hpp>
#include <boost/geometry/geometries/point_xy.hpp>
#include <boost/geometry/geometries/linestring.hpp>
#include <boost/geometry/geometries/polygon.hpp>
using BgPoint = boost::geometry::model::d2::point_xy<double>;
using BgLinestring = boost::geometry::model::linestring<BgPoint>;
using BgPolygon = boost::geometry::model::polygon<BgPoint>;
using BgBox = BgPolygon;

namespace world {
    struct WorldEditor {
        // Editor methods and data
    };
}
*/


/*
void rect_fill(int min_x, int max_x, int min_z, int max_z, int value, world::WorldEditor& editor);

void invers_floodfill_iterative(
    std::pair<int32_t, int32_t> min,
    std::pair<int32_t, int32_t> max,
    int value,
    const std::vector<BgPolygon>& outers,
    const std::vector<BgPolygon>& inners,
    world::WorldEditor& editor);
*/
BgPolygon make_rectangle_polygon(int min_x, int max_x, int min_z, int max_z) {
    BgPolygon poly;
    boost::geometry::append(poly.outer(), BgPoint(min_x, min_z));
    boost::geometry::append(poly.outer(), BgPoint(max_x, min_z));
    boost::geometry::append(poly.outer(), BgPoint(max_x, max_z));
    boost::geometry::append(poly.outer(), BgPoint(min_x, max_z));
    boost::geometry::append(poly.outer(), BgPoint(min_x, min_z)); // close polygon
    boost::geometry::correct(poly);
    return poly;
}

void inverse_floodfill_recursive(
    std::pair<int32_t, int32_t> min,
    std::pair<int32_t, int32_t> max,
    const std::vector<BgPolygon>& outers,
    const std::vector<BgPolygon>& inners,
    WorldEditor& editor,
    const std::chrono::steady_clock::time_point& start_time)
{
    using namespace std::chrono;
    constexpr int64_t ITERATIVE_THRES = 10'000;

    if (min.first > max.first || min.second > max.second) {
        return;
    }

    if (duration_cast<seconds>(steady_clock::now() - start_time).count() > 25) {
        std::cout << "Water area generation exceeded 25 seconds, continuing anyway\n";
    }

    int64_t width = static_cast<int64_t>(max.first) - static_cast<int64_t>(min.first);
    int64_t height = static_cast<int64_t>(max.second) - static_cast<int64_t>(min.second);

    if (width * height < ITERATIVE_THRES) {
        inverse_floodfill_iterative(min, max, 0, outers, inners, editor);
        return;
    }

    int32_t center_x = (min.first + max.first) / 2;
    int32_t center_z = (min.second + max.second) / 2;

    std::array<std::tuple<int32_t, int32_t, int32_t, int32_t>, 4> quadrants = {{
        {min.first, center_x, min.second, center_z},
        {center_x, max.first, min.second, center_z},
        {min.first, center_x, center_z, max.second},
        {center_x, max.first, center_z, max.second}
    }};

    for (const auto& [min_x, max_x, min_z, max_z] : quadrants) {
        auto rect = make_rectangle_polygon(min_x, max_x, min_z, max_z);

        bool any_outer_contains = std::any_of(
            outers.begin(), outers.end(),
            [&rect](const BgPolygon& outer) {
                return boost::geometry::within(rect, outer);
            }
        );

        bool any_inner_intersects = std::any_of(

 inners.begin(), inners.end(),
            [&rect](const BgPolygon& inner) {
                return boost::geometry::intersects(inner, rect);
            }
        );

        if (any_outer_contains && !any_inner_intersects) {
            rect_fill(min_x, max_x, min_z, max_z, 0, editor);
            continue;
        }

        std::vector<BgPolygon> outers_intersects;
        std::copy_if(
            outers.begin(), outers.end(),
            std::back_inserter(outers_intersects),
            [&rect](const BgPolygon& poly) { return boost::geometry::intersects(poly, rect); }
        );

        std::vector<BgPolygon> inners_intersects;
        std::copy_if(
            inners.begin(), inners.end(),
            std::back_inserter(inners_intersects),
            [&rect](const BgPolygon& poly) { return boost::geometry::intersects(poly, rect); }
        );

        if (!outers_intersects.empty()) {
            inverse_floodfill_recursive(
                {min_x, min_z},
                {max_x, max_z},
                outers_intersects,
                inners_intersects,
                editor,
                start_time
            );
        }
    }
}


static void inverse_floodfill(int min_x, int min_z, int max_x, int max_z,
		const std::vector<std::vector<XZPoint>> &outers,
		const std::vector<std::vector<XZPoint>> &inners, WorldEditor &editor,
		const std::chrono::steady_clock::time_point &start_time)
{
	std::vector<BgPolygon> inners_bg;
	inners_bg.reserve(inners.size());
	for (const auto &poly_pts : inners) {
		BgLinestring ls;
		ls.reserve(poly_pts.size());
		for (const auto &pt : poly_pts) {
			ls.emplace_back(static_cast<double>(pt.x), static_cast<double>(pt.z));
		}
		// ensure closed
		if (!ls.empty() &&
				(ls.front().x() != ls.back().x() || ls.front().y() != ls.back().y())) {
			ls.push_back(ls.front());
		}
		BgPolygon poly;
		boost::geometry::assign_points(poly, ls);
		boost::geometry::correct(poly);
		inners_bg.push_back(std::move(poly));
	}

	std::vector<BgPolygon> outers_bg;
	outers_bg.reserve(outers.size());
	for (const auto &poly_pts : outers) {
		BgLinestring ls;
		ls.reserve(poly_pts.size());
		for (const auto &pt : poly_pts) {
			ls.emplace_back(static_cast<double>(pt.x), static_cast<double>(pt.z));
		}
		if (!ls.empty() &&
				(ls.front().x() != ls.back().x() || ls.front().y() != ls.back().y())) {
			ls.push_back(ls.front());
		}
		BgPolygon poly;
		boost::geometry::assign_points(poly, ls);
		boost::geometry::correct(poly);
		outers_bg.push_back(std::move(poly));
	}

	inverse_floodfill_recursive(std::make_pair(min_x, min_z),
			std::make_pair(max_x, max_z), outers_bg, inners_bg, editor, start_time);
}

void generate_water_areas(WorldEditor &editor, const ProcessedRelation &element)
{
	auto start_time = std::chrono::steady_clock::now();

	bool is_water = false;
	auto it = element.tags.find("water");
	if (it != element.tags.end()) {
		is_water = true;
	} else {
		auto it_nat = element.tags.find("natural");
		if (it_nat != element.tags.end() && it_nat->second == "water") {
			is_water = true;
		}
	}

	if (!is_water) {
		return;
	}

	auto it_layer = element.tags.find("layer");
	if (it_layer != element.tags.end()) {
		try {
			int layer = std::stoi(it_layer->second);
			if (layer < 0) {
				return;
			}
		} catch (...) {
			// ignore parse errors
		}
	}

	std::vector<std::vector<ProcessedNode>> outers;
	std::vector<std::vector<ProcessedNode>> inners;

	for (const auto &mem : element.members) {
		if (mem.role == ProcessedMemberRole::Outer) {
			outers.push_back(mem.way.nodes);
		} else if (mem.role == ProcessedMemberRole::Inner) {
			inners.push_back(mem.way.nodes);
		}
	}

	for (std::size_t i = 0; i < outers.size(); ++i) {
		std::vector<std::vector<ProcessedNode>> individual_outers;
		individual_outers.push_back(outers[i]);

		merge_loopy_loops(individual_outers);
		if (!verify_loopy_loops(individual_outers)) {
			std::cout << "Skipping invalid outer polygon " << (i + 1) << " for rela "
					  << element.id << std::endl;
			continue;
		}

		// Work on a copy of inners for merging/verification
		std::vector<std::vector<ProcessedNode>> inners_copy = inners;
		merge_loopy_loops(inners_copy);
		if (!verify_loopy_loops(inners_copy)) {
			std::vector<std::vector<ProcessedNode>> empty_inners;
			std::vector<std::vector<ProcessedNode>> temp_inners = empty_inners;
			merge_loopy_loops(temp_inners);

			auto [min_x, min_z] = editor.get_min_coords();
			auto [max_x, max_z] = editor.get_max_coords();

			std::vector<std::vector<XZPoint>> individual_outers_xz;
			for (const auto &poly : individual_outers) {
				std::vector<XZPoint> v;
				v.reserve(poly.size());
				for (const auto &n : poly) {
					v.push_back(n.xz());
				}
				individual_outers_xz.push_back(std::move(v));
			}
			std::vector<std::vector<XZPoint>> empty_inners_xz;

			inverse_floodfill(min_x, min_z, max_x, max_z, individual_outers_xz,
					empty_inners_xz, editor, start_time);
			continue;
		}

		auto [min_x, min_z] = editor.get_min_coords();
		auto [max_x, max_z] = editor.get_max_coords();

		std::vector<std::vector<XZPoint>> individual_outers_xz;
		for (const auto &poly : individual_outers) {
			std::vector<XZPoint> v;
			v.reserve(poly.size());
			for (const auto &n : poly) {
				v.push_back(n.xz());
			}
			individual_outers_xz.push_back(std::move(v));
		}

		std::vector<std::vector<XZPoint>> inners_xz;
		for (const auto &poly : inners_copy) {
			std::vector<XZPoint> v;
			v.reserve(poly.size());
			for (const auto &n : poly) {
				v.push_back(n.xz());
			}
			inners_xz.push_back(std::move(v));
		}

		inverse_floodfill(min_x, min_z, max_x, max_z, individual_outers_xz, inners_xz,
				editor, start_time);
	}
}
}
}