#include <vector>
#include <tuple>
#include <algorithm>
#include "irr_v3d.h"

// Function to generate a line between two 3D points using Bresenham's algorithm
//std::vector<std::tuple<int, int, int>>
std::vector<v3s32> bresenham_line(int x1, int y1, int z1, int x2, int y2, int z2);