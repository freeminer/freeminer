#include <iostream>
#include <vector>
#include <queue>
#include <unordered_set>
#include <chrono>
#include <algorithm>
#include <boost/geometry.hpp>
#include <boost/geometry/geometries/point.hpp>
#include <boost/geometry/geometries/polygon.hpp>
#include "debug/dump.h"

#include "floodfill.h"

namespace arnis {

namespace floodfill {


namespace bg = boost::geometry;

// Type aliases for convenience
using Point2D = bg::model::d2::point_xy<int>;
using Point2D_Float = bg::model::d2::point_xy<double>;
using Polygon = bg::model::polygon<Point2D_Float>;

// Hash function for pair<int,int> to use in unordered_set
struct pair_hash {
    size_t operator()(const std::pair<int,int>& p) const {
        return std::hash<int>()(p.first) ^ (std::hash<int>()(p.second) << 1);
    }
};

// Check if the current time exceeds the timeout duration
inline bool timeout_exceeded(const std::chrono::steady_clock::time_point& start_time, const std::chrono::milliseconds& timeout_duration) {
    return (std::chrono::steady_clock::now() - start_time) > timeout_duration;
}

//namespace FloodFill  
//{
    std::vector<std::pair<int,int>> original_flood_fill(
        const std::vector<std::pair<int,int>>& polygon_coords,
        int min_x, int max_x, int min_y, int max_y,
        const std::chrono::milliseconds* timeout
    );
     std::vector<std::pair<int,int>> optimized_flood_fill(
        const std::vector<std::pair<int,int>>& polygon_coords,
        int min_x, int max_x, int min_y, int max_y,
        const std::chrono::milliseconds* timeout
    );

//class FloodFill {
//public:
    // Main function: chooses algorithm based on polygon size
    //static 
    std::vector<std::pair<int,int>> flood_fill_area(
        const std::vector<std::pair<int,int>>& polygon_coords,
        const std::chrono::milliseconds* timeout // = nullptr
    ) {
        if (polygon_coords.size() < 3) {
            return {}; // Not a valid polygon
        }

        // Calculate bounding box
        int min_x = polygon_coords[0].first;
        int max_x = polygon_coords[0].first;
        int min_y = polygon_coords[0].second;
        int max_y = polygon_coords[0].second;

        for (const auto& coord : polygon_coords) {
            min_x = std::min(min_x, coord.first);
            max_x = std::max(max_x, coord.first);
            min_y = std::min(min_y, coord.second);
            max_y = std::max(max_y, coord.second);
        }

        // Approximate area
        int width = max_x - min_x + 1;
        int height = max_y - min_y + 1;
        int area = width * height;

//DUMP(area, width, height);
        if (area < 50000) {
            return optimized_flood_fill(polygon_coords, min_x, max_x, min_y, max_y, timeout);
        } else {
            return original_flood_fill(polygon_coords, min_x, max_x, min_y, max_y, timeout);
        }
    }

//private:
    // Converts polygon coordinates to Boost Polygon
    static Polygon create_polygon(const std::vector<std::pair<int,int>>& coords) {
        Polygon poly;
        for (const auto& c : coords) {
            bg::append(poly.outer(), Point2D_Float(c.first, c.second));
        }
        bg::correct(poly);
        return poly;
    }

    // Optimized flood fill for smaller polygons
    //static
     std::vector<std::pair<int,int>> optimized_flood_fill(
        const std::vector<std::pair<int,int>>& polygon_coords,
        int min_x, int max_x, int min_y, int max_y,
        const std::chrono::milliseconds* timeout
    ) {
        auto start_time = std::chrono::steady_clock::now();

        std::vector<std::pair<int,int>> filled_area;
        std::unordered_set<std::pair<int,int>, pair_hash> global_visited;

        Polygon polygon = create_polygon(polygon_coords);

        int width = max_x - min_x + 1;
        int height = max_y - min_y + 1;

        int step_x = std::clamp(width / 6, 1, 8);
        int step_y = std::clamp(height / 6, 1, 8);

        for (int y = min_y; y <= max_y; y += step_y) {
            for (int x = min_x; x <= max_x; x += step_x) {
                if (timeout && timeout_exceeded(start_time, *timeout)) {
                    return filled_area;
                }

                auto point = std::make_pair(x, y);
                if (global_visited.count(point)) continue;

                Point2D_Float test_point(x, y);
                if (!bg::within(test_point, polygon)) continue;

                // BFS flood fill from seed
                std::queue<std::pair<int,int>> q;
                q.push(point);
                global_visited.insert(point);

                while (!q.empty()) {
                    auto [cx, cy] = q.front(); q.pop();
                    filled_area.emplace_back(cx, cy);

                    const std::array<std::pair<int,int>,4> neighbors = {
                        { {cx - 1, cy}, {cx + 1, cy}, {cx, cy - 1}, {cx, cy + 1} }
                    };

                    for (const auto& n : neighbors) {
                        int nx = n.first;
                        int ny = n.second;
                        if (nx < min_x || nx > max_x || ny < min_y || ny > max_y)
                            continue;
                        if (global_visited.count({nx, ny})) continue;

                        Point2D_Float neighbor_point(nx, ny);
                        if (bg::within(neighbor_point, polygon)) {
                            global_visited.insert({nx, ny});
                            q.emplace(nx, ny);
                        }
                    }

                    if (timeout && timeout_exceeded(start_time, *timeout)) {
                        return filled_area;
                    }
                }
            }
        }
        return filled_area;
    }

    // Larger polygon, coarser sampling
    //static 
    std::vector<std::pair<int,int>> original_flood_fill(
        const std::vector<std::pair<int,int>>& polygon_coords,
        int min_x, int max_x, int min_y, int max_y,
        const std::chrono::milliseconds* timeout
    ) {
        auto start_time = std::chrono::steady_clock::now();

        std::vector<std::pair<int,int>> filled_area;
        std::unordered_set<std::pair<int,int>, pair_hash> global_visited;

        Polygon polygon = create_polygon(polygon_coords);

        int width = max_x - min_x + 1;
        int height = max_y - min_y + 1;

        int step_x = std::clamp(width / 8, 1, 12);
        int step_y = std::clamp(height / 8, 1, 12);

        for (int y = min_y; y <= max_y; y += step_y) {
            for (int x = min_x; x <= max_x; x += step_x) {
                if (timeout && timeout_exceeded(start_time, *timeout)) {
                    return filled_area;
                }

                auto point = std::make_pair(x, y);
                if (global_visited.count(point)) continue;

                Point2D_Float test_point(x, y);
                if (!bg::within(test_point, polygon)) continue;

                // BFS flood fill
                std::queue<std::pair<int,int>> q;
                q.push(point);
                global_visited.insert(point);

                while (!q.empty()) {
                    auto [cx, cy] = q.front(); q.pop();
                    filled_area.emplace_back(cx, cy);

                    const std::array<std::pair<int,int>,4> neighbors = {
                        { {cx - 1, cy}, {cx + 1, cy}, {cx, cy - 1}, {cx, cy + 1} }
                    };

                    for (const auto& n : neighbors) {
                        int nx = n.first;
                        int ny = n.second;
                        if (nx < min_x || nx > max_x || ny < min_y || ny > max_y)
                            continue;
                        if (global_visited.count({nx, ny})) continue;

                        Point2D_Float neighbor_point(nx, ny);
                        if (bg::within(neighbor_point, polygon)) {
                            global_visited.insert({nx, ny});
                            q.emplace(nx, ny);
                        }
                    }

                    if (timeout && timeout_exceeded(start_time, *timeout)) {
                        return filled_area;
                    }
                }
            }
        }
        return filled_area;
    }
//};

/*
int main() {
    // Example usage:
    std::vector<std::pair<int,int>> polygon_coords = {
        {0,0}, {10,0}, {10,10}, {0,10}
    };

    // Optional: define a timeout duration (e.g., 2 seconds)
    std::chrono::milliseconds timeout_duration(2000);

    auto filled_points = FloodFill::flood_fill_area(polygon_coords, &timeout_duration);

    std::cout << "Filled points count: " << filled_points.size() << "\n";
    // Output points or further processing...

    return 0;
}
*/

#if 0
#include <vector>
#include <unordered_set>
#include <deque>
#include <chrono>
#include <algorithm>
#include <functional>
#include <memory>
#include <cmath>

/////////// Manual
std::vector<std::pair<int32_t, int32_t>> optimized_flood_fill_area(
    const std::vector<std::pair<int32_t, int32_t>>& polygon_coords,
    const std::chrono::duration<double>* timeout,
    int32_t min_x, int32_t max_x, int32_t min_z, int32_t max_z);

std::vector<std::pair<int32_t, int32_t>> original_flood_fill_area(
    const std::vector<std::pair<int32_t, int32_t>>& polygon_coords,
    const std::chrono::duration<double>* timeout,
    int32_t min_x, int32_t max_x, int32_t min_z, int32_t max_z);
////////////



// Assuming you have a Point class with contains method
// You'll need to implement or use a geometry library for this
class Point {
public:
    Point(double x, double z) : x_(x), z_(z) {}
    double x() const { return x_; }
    double z() const { return z_; }
private:
    double x_, z_;
};

class LineString {
public:
    LineString(const std::vector<std::pair<double, double>>& coords) {
        for (const auto& coord : coords) {
            points_.emplace_back(coord.first, coord.second);
        }
    }
    const std::vector<Point>& points() const { return points_; }
private:
    std::vector<Point> points_;
};

class Polygon {
public:
    Polygon(const LineString& exterior) : exterior_(exterior) {}
    bool contains(const Point& point) const {
        // Implement point-in-polygon algorithm here
        // This is a placeholder - you'll need to implement the actual algorithm
        return true;
    }
    const LineString& exterior() const { return exterior_; }
private:
    LineString exterior_;
};

// Helper function to get min and max values
template<typename T>
std::pair<T, T> minmax(const std::vector<std::pair<T, T>>& coords, 
                      std::function<T(const std::pair<T, T>&)> extractor) {
    if (coords.empty()) return {T(), T()};
    
    T min_val = extractor(coords[0]);
    T max_val = extractor(coords[0]);
    
    for (const auto& coord : coords) {
        T val = extractor(coord);
        if (val < min_val) min_val = val;
        if (val > max_val) max_val = val;
    }
    
    return {min_val, max_val};
}


/// Main flood fill function with automatic algorithm selection
std::vector<std::pair<int32_t, int32_t>> flood_fill_area(
    const std::vector<std::pair<int32_t, int32_t>>& polygon_coords,
    const std::chrono::duration<double>* timeout = nullptr) {
    
    if (polygon_coords.size() < 3) {
        return {}; // Not a valid polygon
    }

    // Calculate bounding box of the polygon
    auto [min_x, max_x] = minmax<int32_t>(polygon_coords, [](const auto& p) { return p.first; });
    auto [min_z, max_z] = minmax<int32_t>(polygon_coords, [](const auto& p) { return p.second; });

    int64_t area = static_cast<int64_t>(max_x - min_x + 1) * static_cast<int64_t>(max_z - min_z + 1);

    // For small and medium areas, use optimized flood fill with span filling
    if (area < 50000) {
        return optimized_flood_fill_area(polygon_coords, timeout, min_x, max_x, min_z, max_z);
    } else {
        // For larger areas, use original flood fill with grid sampling
        return original_flood_fill_area(polygon_coords, timeout, min_x, max_x, min_z, max_z);
    }
}

/// Optimized flood fill for larger polygons with multi-seed detection
std::vector<std::pair<int32_t, int32_t>> optimized_flood_fill_area(
    const std::vector<std::pair<int32_t, int32_t>>& polygon_coords,
    const std::chrono::duration<double>* timeout,
    int32_t min_x, int32_t max_x, int32_t min_z, int32_t max_z) {
    
    auto start_time = std::chrono::steady_clock::now();
    std::vector<std::pair<int32_t, int32_t>> filled_area;
    std::unordered_set<std::pair<int32_t, int32_t>> global_visited;

    // Create polygon for containment testing
    std::vector<std::pair<double, double>> exterior_coords;
    for (const auto& coord : polygon_coords) {
        exterior_coords.emplace_back(static_cast<double>(coord.first), static_cast<double>(coord.second));
    }
    LineString exterior(exterior_coords);
    Polygon polygon(exterior);

    // Optimized step sizes
    int32_t width = max_x - min_x + 1;
    int32_t height = max_z - min_z + 1;
    int32_t step_x = std::clamp(width / 6, 1, 8);
    int32_t step_z = std::clamp(height / 6, 1, 8);

    // Pre-allocate queue
    std::deque<std::pair<int32_t, int32_t>> queue;
    queue.reserve(1024);

    for (int32_t z = min_z; z <= max_z; z += step_z) {
        for (int32_t x = min_x; x <= max_x; x += step_x) {
            // Fast timeout check
            if (filled_area.size() % 100 == 0 && timeout != nullptr) {
                if (std::chrono::steady_clock::now() - start_time > *timeout) {
                    return filled_area;
                }
            }

            // Skip if already visited or not inside polygon
            if (global_visited.find({x, z}) != global_visited.end() ||
                !polygon.contains(Point(static_cast<double>(x), static_cast<double>(z)))) {
                continue;
            }

            // Start flood fill from this seed point
            queue.clear();
            queue.push_back({x, z});
            global_visited.insert({x, z});

            while (!queue.empty()) {
                auto [curr_x, curr_z] = queue.front();
                queue.pop_front();

                // Add current point to filled area
                filled_area.push_back({curr_x, curr_z});

                // Check all four directions
                std::pair<int32_t, int32_t> neighbors[] = {
                    {curr_x - 1, curr_z},
                    {curr_x + 1, curr_z},
                    {curr_x, curr_z - 1},
                    {curr_x, curr_z + 1}
                };

                for (const auto& [nx, nz] : neighbors) {
                    if (nx >= min_x && nx <= max_x && nz >= min_z && nz <= max_z &&
                        global_visited.find({nx, nz}) == global_visited.end()) {
                        
                        if (polygon.contains(Point(static_cast<double>(nx), static_cast<double>(nz)))) {
                            global_visited.insert({nx, nz});
                            queue.push_back({nx, nz});
                        }
                    }
                }
            }
        }
    }

    return filled_area;
}

/// Original flood fill algorithm with enhanced multi-seed detection
std::vector<std::pair<int32_t, int32_t>> original_flood_fill_area(
    const std::vector<std::pair<int32_t, int32_t>>& polygon_coords,
    const std::chrono::duration<double>* timeout,
    int32_t min_x, int32_t max_x, int32_t min_z, int32_t max_z) {
    
    auto start_time = std::chrono::steady_clock::now();
    std::vectorvector<std::pair<int32_t, int32_t>> filled_area;
    std::unordered_set<std::pair<int32_t, int32_t>> global_visited;

    // Create polygon for containment testing
    std::vector<std::pair<double, double>> exterior_coords;
    for (const auto& coord : polygon_coords) {
        exterior_coords.emplace_back(static_cast<double>(coord.first), static_cast<double>(coord.second));
    }
    LineString exterior(exterior_coords);
    Polygon polygon(exterior);

    // Optimized step sizes for large polygons
    int32_t width = max_x - min_x + 1;
    int32_t height = max_z - min_z + 1;
    int32_t step_x = std::clamp(width / 8, 1, 12);
    int32_t step_z = std::clamp(height / 8, 1, 12);

    // Pre-allocate queue
    std::deque<std::pair<int32_t, int32_t>> queue;
    queue.reserve(2048);
    filled_area.reserve(1000);

    // Scan for multiple seed points
    for (int32_t z = min_z; z <= max_z; z += step_z) {
        for (int32_t x = min_x; x <= max_x; x += step_x) {
            // Reduced timeout checking frequency
            if (global_visited.size() % 200 == 0 && timeout != nullptr) {
                if (std::chrono::steady_clock::now() - start_time > *timeout) {
                    return filled_area;
                }
            }

            // Skip if already processed or not inside polygon
            if (global_visited.find({x, z}) != global_visited.end() ||
                !polygon.contains(Point(static_cast<double>(x), static_cast<double>(z)))) {
                continue;
            }

            // Start flood-fill from this seed point
            queue.clear();
            queue.push_back({x, z});
            global_visited.insert({x, z});

            while (!queue.empty()) {
                auto [curr_x, curr_z] = queue.front();
                queue.pop_front();

                if (polygon.contains(Point(static_cast<double>(curr_x), static_cast<double>(curr_z)))) {
                    filled_area.push_back({curr_x, curr_z});

                    // Check adjacent points
                    std::pair<int32_t, int32_t> neighbors[] = {
                        {curr_x - 1, curr_z},
                        {curr_x + 1, curr_z},
                        {curr_x, curr_z - 1},
                        {curr_x, curr_z + 1}
                    };

                    for (const auto& [nx, nz] : neighbors) {
                        if (nx >= min_x && nx <= max_x && nz >= min_z && nz <= max_z &&
                            global_visited.find({nx, nz}) == global_visited.end()) {
                            
                            global_visited.insert({nx, nz});
                            queue.push_back({nx, nz});
                        }
                    }
                }
            }
        }
    }

    return filled_area;
}
#endif

}
}
