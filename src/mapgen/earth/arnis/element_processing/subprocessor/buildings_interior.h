#pragma once
namespace arnis {
void generate_building_interior(
    WorldEditor & editor,
    const std::vector<std::pair<int,int>> & floor_area,
    int min_x,
    int min_z,
    int max_x,
    int max_z,
    int start_y_offset,
    int building_height,
    Block wall_block,
    const std::vector<int> & floor_levels,
    const Args & args,
    const ProcessedWay & element,
    int abs_terrain_offset
);
}