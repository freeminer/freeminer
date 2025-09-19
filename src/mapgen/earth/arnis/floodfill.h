#pragma once
#include <chrono>
#include <cstdint>
#include <optional>
#include <utility>
#include <vector>

namespace arnis
{
namespace floodfill {
std::vector<std::pair<int, int>> flood_fill_area(
		const std::vector<std::pair<int, int>> &polygon_coords,
		const std::chrono::milliseconds *timeout = nullptr);

inline std::vector<std::pair<int, int>> flood_fill_area(
		const std::vector<std::pair<int, int>> &polygon_coords,
		const std::chrono::milliseconds &timeout)
{
	return flood_fill_area(polygon_coords, &timeout);
}

inline std::vector<std::pair<int, int>> flood_fill_area(
		const std::vector<std::pair<int, int>> &polygon_coords,
		const std::optional<std::chrono::duration<double>> &timeout)
{
	std::chrono::milliseconds t{
			timeout.has_value() ? (int64_t)timeout.value().count() : 200};

	return flood_fill_area(polygon_coords, &t);
}
}
using namespace floodfill;
}