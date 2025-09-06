#pragma once

#include <chrono>
#include <vector>
#include "irr_v2d.h"

using Point = v2s32;

std::vector<Point> flood_fill_area(
    const std::vector<Point>& polygon_coords, 
    const std::chrono::milliseconds& timeout = std::chrono::milliseconds{200});
