#pragma once

/*
struct RGB {
    int r;
    int g;
    int b;
};
*/
//usngg RGB =

namespace arnis
{

enum class StairFacing
{
	North,
	East,
	South,
	West,
};

enum class StairShape
{
	Straight,
	InnerLeft,
	InnerRight,
	OuterLeft,
	OuterRight,
};

inline const char *StairFacing_as_str(StairFacing f) noexcept
{
	switch (f) {
	case StairFacing::North:
		return "north";
	case StairFacing::East:
		return "east";
	case StairFacing::South:
		return "south";
	case StairFacing::West:
		return "west";
	}
	return "";
}

inline const char *StairShape_as_str(StairShape s) noexcept
{
	switch (s) {
	case StairShape::Straight:
		return "straight";
	case StairShape::InnerLeft:
		return "inner_left";
	case StairShape::InnerRight:
		return "inner_right";
	case StairShape::OuterLeft:
		return "outer_left";
	case StairShape::OuterRight:
		return "outer_right";
	}
	return "";
}

Block get_building_wall_block_for_color(const RGB &color);
Block get_fallback_building_block();
Block get_random_floor_block();
Block get_window_block_for_building_type(const std::string &building_type);
Block get_stair_block_for_material(const Block &material);
BlockWithProperties create_stair_with_properties(const Block& base_stair_block, StairFacing facing, StairShape shape) ;

}