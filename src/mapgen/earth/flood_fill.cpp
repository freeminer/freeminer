// https://github.com/louis-e/arnis/blob/main/src/floodfill.rs + chatgpt
#include "flood_fill.h"

#include <iostream>
#include <vector>
#include <set>
#include <deque>
#include <cmath>
#include <chrono>
#include <algorithm>
#include "debug/dump.h"

// Struct for representing a 2D point
/*
struct Point {
    int X;
    int Y;
    
    Point(int x_, int z_) : X(x_), Y(z_) {}
    
    //bool operator<(const Point& other) const {
    //    return std::tie(X, Z) < std::tie(other.X, other.Z);
    //}
};
*/

// Function to check if a point is inside a polygon using the ray-casting algorithm
bool is_point_inside_polygon(const Point &point, const std::vector<Point> &polygon)
{
	int n = polygon.size();
	bool inside = false;

	for (int i = 0, j = n - 1; i < n; j = i++) {
		if (((polygon[i].Y > point.Y) != (polygon[j].Y > point.Y)) &&
				(point.X < (polygon[j].X - polygon[i].X) * (point.Y - polygon[i].Y) /
										   (polygon[j].Y - polygon[i].Y) +
								   polygon[i].X)) {
			inside = !inside;
		}
	}

	return inside;
}

// Flood fill algorithm to find the area inside a polygon
std::vector<Point> flood_fill_area(const std::vector<Point> &polygon_coords,
		const std::chrono::milliseconds &timeout)
{
	if (polygon_coords.size() < 3) {
		return {}; // Not a valid polygon
	}

	auto start_time = std::chrono::steady_clock::now();

	// Calculate bounding box
	int min_x = std::min_element(polygon_coords.begin(), polygon_coords.end(),
			[](const Point &a, const Point &b) {
				return a.X < b.X;
			})->X;
	int max_x = std::max_element(polygon_coords.begin(), polygon_coords.end(),
			[](const Point &a, const Point &b) {
				return a.X < b.X;
			})->X;
	int min_z = std::min_element(polygon_coords.begin(), polygon_coords.end(),
			[](const Point &a, const Point &b) {
				return a.Y < b.Y;
			})->Y;
	int max_z = std::max_element(polygon_coords.begin(), polygon_coords.end(),
			[](const Point &a, const Point &b) {
				return a.Y < b.Y;
			})->Y;

	std::vector<Point> filled_area;
	std::set<Point> visited;

	// Determine grid step sizes
	int step_x = std::max(1, (max_x - min_x) / 10);
	int step_z = std::max(1, (max_z - min_z) / 10);

	// Candidate points for flood-fill
	std::deque<Point> candidate_points;
	for (int x = min_x; x <= max_x; x += step_x) {
		for (int z = min_z; z <= max_z; z += step_z) {
			candidate_points.emplace_back(x, z);
		}
	}

	//DUMP(candidate_points.size());
	size_t ticks = 0;
	// Perform flood-fill from each valid candidate point
	while (!candidate_points.empty()) {
		//auto [start_x, start_z] = candidate_points.front();
		auto start = candidate_points.front();
		candidate_points.pop_front();

		//DUMP( std::chrono::steady_clock::now().time_since_epoch().count(), "-", start_time.time_since_epoch().count() ,">", timeout.count());
		if (timeout != std::chrono::milliseconds::zero() &&
				std::chrono::steady_clock::now() - start_time > timeout) {
			DUMP("Floodfill timeout", polygon_coords.size(), candidate_points.size(),
					filled_area.size(), ticks);
			break;
		}

		// Check if the point is inside the polygon
		if (is_point_inside_polygon(start, polygon_coords)) {
			std::deque<Point> queue;
			queue.emplace_back(start);
			visited.insert(start);

			// Perform BFS flood-fill
			while (!queue.empty()) {
				auto tp = queue.front();
				queue.pop_front();

				if (timeout != std::chrono::milliseconds::zero() &&
						std::chrono::steady_clock::now() - start_time > timeout) {
					DUMP("Floodfill timeout", queue.size(), polygon_coords.size(),
							candidate_points.size(), filled_area.size(), ticks);
					break;
				}

				if (is_point_inside_polygon(tp, polygon_coords)) {
					filled_area.emplace_back(tp);

					// Check adjacent points
					std::vector<Point> neighbors = {Point(tp.X - 1, tp.Y),
							Point(tp.X + 1, tp.Y), Point(tp.X, tp.Y - 1),
							Point(tp.X, tp.Y + 1)};
					for (const auto &neighbor : neighbors) {
						++ticks;
						if (neighbor.X >= min_x && neighbor.X <= max_x &&
								neighbor.Y >= min_z && neighbor.Y <= max_z &&
								visited.find(neighbor) == visited.end()) {
							visited.insert(neighbor);
							queue.push_back(neighbor);
						}
					}
				}
			}

			if (!filled_area.empty()) {
				break; // Exit if a valid area has been filled
			}
		}
	}

	return filled_area;
}

/*
int main() {
    // Example polygon: a square
    std::vector<Point> polygon_coords = {
        Point(1, 1), Point(1, 5), Point(5, 5), Point(5, 1)
    };

    // Call the flood-fill function with a timeout of 2 seconds
    auto filled_area = flood_fill_area(polygon_coords, std::chrono::milliseconds(2000));

    // Output the filled area
    std::cout << "Filled Area Coordinates:\n";
    for (const auto& point : filled_area) {
        std::cout << "(" << point.x << ", " << point.z << ")\n";
    }

    return 0;
}
*/