/*
Copyright (C) 2024 proller <proler@gmail.com>
*/

/*
This file is part of Freeminer.

Freeminer is free software: you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation, either version 3 of the License, or
(at your option) any later version.

Freeminer  is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with Freeminer.  If not, see <http://www.gnu.org/licenses/>.
*/

#include "fm_far_calc.h"
#include <array>
#include <bit>
#include <cstdint>
#include <limits>
#include <optional>

#include "client/clientmap.h"
#include "irr_v3d.h"
#include "irrlichttypes.h"

namespace farmesh
{

block_step_t getLodStep(const MapDrawControl &draw_control,
		const v3bpos_t &playerblockpos, const v3bpos_t &blockpos, const pos_t speedf)
{
	if (draw_control.lodmesh) {
		const auto range = radius_box(playerblockpos, blockpos);
		/* todo: make stable, depend on speed increase/decrease
		const auto speed_blocks = speedf / (BS * MAP_BLOCKSIZE);
		if (range > 1 && speed_blocks > 1) {
			range += speed_blocks;
		}
		*/
		const auto &cells = draw_control.cell_size_pow;
		const auto max_lod = MAP_BLOCKP + draw_control.cell_size_pow;
		for (int i = max_lod; i >= 1; --i) {
			if (range >= (1 << cells) + (draw_control.lodmesh) * (1 << (i - 1))) {
				return i;
			}
		}
	}
	return 0;
};

block_step_t rangeToStep(const int range)
{
	const unsigned int r = static_cast<unsigned int>(range);
	return r ? static_cast<int>(std::bit_width(r) - 1) : 0;
}

block_step_t settingToStep(const int range)
{
	// really 4 ?
	return rangeToStep(range / 4);
}

uint64_t getFarMeshNearRange(const MapDrawControl &draw_control)
{
	if (draw_control.farmesh <= 0)
		return 0;

	// A step-1 tree cell has a side of 2 * cell_size blocks.  It is split
	// into step-0 cells while its center is closer than
	//     cell_side << (1 + max(quality_pow, cell_size_pow) - cell_size_pow)
	// Add half a cell on the outside and one mapblock for the camera's offset
	// inside its current block.  This deliberately overestimates by at most one
	// mapblock and avoids enumerating the complete far tree every draw-list update.
	const unsigned cell_pow = draw_control.cell_size_pow;
	const unsigned distance_pow =
			2 + std::max<unsigned>(draw_control.farmesh_quality_pow, cell_pow);
	if (cell_pow >= 63 || distance_pow >= 63)
		return std::numeric_limits<uint64_t>::max();

	const uint64_t blocks = (uint64_t{1} << distance_pow) + (uint64_t{1} << cell_pow);
	if (blocks > std::numeric_limits<uint64_t>::max() / MAP_BLOCKSIZE)
		return std::numeric_limits<uint64_t>::max();
	return blocks * MAP_BLOCKSIZE;
}

bool cellIntersectsRange(const v3pos_t &cell_min, const pos_t cell_width,
		const v3pos_t &camera_pos, const pos_t range)
{
	if (cell_width <= 0 || range < 0)
		return false;

	for (u8 axis = 0; axis < 3; ++axis) {
		const int64_t cell_min_axis = cell_min[axis];
		const int64_t cell_max_axis = cell_min_axis + cell_width - 1;
		const int64_t range_min_axis = static_cast<int64_t>(camera_pos[axis]) - range;
		const int64_t range_max_axis = static_cast<int64_t>(camera_pos[axis]) + range;
		if (cell_max_axis < range_min_axis || cell_min_axis > range_max_axis)
			return false;
	}
	return true;
}

uint64_t cellMaxDistance(
		const v3pos_t &cell_min, const pos_t cell_width, const v3pos_t &camera_pos)
{
	if (cell_width <= 0)
		return 0;

	uint64_t distance = 0;
	for (u8 axis = 0; axis < 3; ++axis) {
		const int64_t cell_min_axis = cell_min[axis];
		const int64_t cell_max_axis = cell_min_axis + cell_width - 1;
		const int64_t camera_axis = camera_pos[axis];
		const auto axis_distance =
				static_cast<uint64_t>(std::max(std::abs(cell_min_axis - camera_axis),
						std::abs(cell_max_axis - camera_axis)));
		distance = std::max(distance, axis_distance);
	}
	return distance;
}

block_step_t getFarStepBad(const MapDrawControl &draw_control,
		const v3bpos_t &playerblockpos, const v3bpos_t &blockpos)
{
	if (!draw_control.farmesh)
		return 1;

	int range = radius_box(playerblockpos, blockpos);

	const auto next_step = 1;
	range >>= next_step; // TODO: configurable

	if (range <= 1)
		return 1;

	int skip = rangeToStep(range);
	//skip += rangeToStep(draw_control.cell_size);
	range = radius_box(v3pos_t((playerblockpos.X >> skip) << skip,
							   (playerblockpos.Y >> skip) << skip,
							   (playerblockpos.Z >> skip) << skip),
			v3pos_t((blockpos.X >> skip) << skip, (blockpos.Y >> skip) << skip,
					(blockpos.Z >> skip) << skip));
	range >>= next_step + draw_control.cell_size_pow;
	if (range > 1) {
		skip = rangeToStep(range);
	}
	if (skip > FARMESH_STEP_MAX)
		skip = FARMESH_STEP_MAX;
	return skip;
};

auto align_shift(auto pos, const auto amount)
{
	(pos.X >>= amount) <<= amount;
	(pos.Y >>= amount) <<= amount;
	(pos.Z >>= amount) <<= amount;
	return pos;
}

v3bpos_t playerBlockAlign(
		const MapDrawControl &draw_control, const v3bpos_t &player_block_pos)
{
	const auto step_pow2 = draw_control.cell_size_pow + draw_control.farmesh_quality_pow;
	return align_shift(player_block_pos, step_pow2) + (step_pow2 >> 1);
}

#if 1

#if USE_POS32
using tpos_t = bpos_t;
using v3tpos_t = v3bpos_t;
#define to_v3bpos(pos) pos
#define to_bpos(pos) pos
#else
using tpos_t = int32_t;
using v3tpos_t = v3s32;
#define to_v3bpos(pos)                                                                   \
	v3bpos_t                                                                             \
	{                                                                                    \
		static_cast<bpos_t>(pos.X), static_cast<bpos_t>(pos.Y),                          \
				static_cast<bpos_t>(pos.Z)                                               \
	}
#define to_bpos(pos) static_cast<bpos_t>(pos)
#endif
bool inFarGrid(const MapDrawControl &draw_control, const v3bpos_t &player_block_pos,
		const v3bpos_t &blockpos, const block_step_t step, bool cell_each)
{
	const auto res = getFarParams(draw_control, player_block_pos, blockpos, cell_each);
	if (!res) {
		return false;
	}
	return res->pos == blockpos && res->step == step;
}

struct find_param_t
{
	const v3tpos_t &player_pos;
	const v3tpos_t &block_pos;
	const block_step_t cell_size_pow;
	const block_step_t farmesh_quality_pow;
	const bool cell_size_each{0};
};

struct child_t
{
	v3tpos_t pos;
	tpos_t size;
};

bool contains(const child_t &child, const v3tpos_t &pos)
{
	return pos.X >= child.pos.X && pos.X < child.pos.X + child.size &&
		   pos.Y >= child.pos.Y && pos.Y < child.pos.Y + child.size &&
		   pos.Z >= child.pos.Z && pos.Z < child.pos.Z + child.size;
}

bool is_tree_cell(const child_t &child, const v3tpos_t &player_pos,
		block_step_t cell_size_pow, block_step_t farmesh_quality_pow)
{
	if (child.size <= (1 << cell_size_pow))
		return true;

	const tpos_t child_size = child.size >> 1;
	const tpos_t distance = std::max({
			std::abs(player_pos.X - (child.pos.X + child_size)),
			std::abs(player_pos.Y - (child.pos.Y + child_size)),
			std::abs(player_pos.Z - (child.pos.Z + child_size)),
	});
	const auto quality_shift =
			1 + std::max(farmesh_quality_pow, cell_size_pow) - cell_size_pow;
	const tpos_t next_child_size = child.size << quality_shift;
	return distance >= next_child_size;
}

std::array<child_t, 8> split(const child_t &child)
{
	const tpos_t size = child.size >> 1;
	const auto x = child.pos.X;
	const auto y = child.pos.Y;
	const auto z = child.pos.Z;
	return {{
			{{x, y, z}, size},
			{{x + size, y, z}, size},
			{{x, y, z + size}, size},
			{{x + size, y, z + size}, size},
			{{x, y + size, z}, size},
			{{x + size, y + size, z}, size},
			{{x, y + size, z + size}, size},
			{{x + size, y + size, z + size}, size},
	}};
}

tree_result_t make_tree_result(
		const child_t &child, block_step_t cell_size_pow, bool cell_size_each)
{
	const block_step_t cell_shift = cell_size_each ? 0 : cell_size_pow;
	return tree_result_t{
			.pos{to_v3bpos(child.pos)},
			.size{to_bpos(child.size >> cell_shift)},
			.step{static_cast<block_step_t>(rangeToStep(child.size) - cell_shift)},
	};
}

tree_result_t make_find_result(const child_t &mesh_child, const v3tpos_t &block_pos,
		block_step_t cell_size_pow, bool cell_size_each)
{
	if (!cell_size_each)
		return make_tree_result(mesh_child, cell_size_pow, false);

	// A mesh cell consists of cell_size^3 independently stored blocks.  Select
	// one by position after the adaptive mesh grid has been decided; running a
	// second adaptive traversal would make boundary cells disagree.
	const tpos_t storage_size = mesh_child.size >> cell_size_pow;
	child_t storage_child{
			.pos{
					mesh_child.pos.X + ((block_pos.X - mesh_child.pos.X) / storage_size) *
											   storage_size,
					mesh_child.pos.Y + ((block_pos.Y - mesh_child.pos.Y) / storage_size) *
											   storage_size,
					mesh_child.pos.Z + ((block_pos.Z - mesh_child.pos.Z) / storage_size) *
											   storage_size,
			},
			.size = storage_size,
	};
	return make_tree_result(storage_child, cell_size_pow, true);
}

std::optional<tree_result_t> find(const find_param_t &param, const child_t &child)
{
	if (!contains(child, param.block_pos))
		return {};

	if (is_tree_cell(
				child, param.player_pos, param.cell_size_pow, param.farmesh_quality_pow))
		return make_find_result(
				child, param.block_pos, param.cell_size_pow, param.cell_size_each);

	for (const auto &child : split(child)) {
		if (const auto res = find(param, child); res)
			return res;
	}
	return {};
}
/*
const auto nearest_pow2 = [](const int v) -> int8_t {
	if (v == 0)
		return 0;
	int p = 1;
	int8_t n = 0;
	while (p < v) {
		p <<= 1;
		++n;
	}
	return n;
};
*/
struct tree_params_t
{
	const block_step_t tree_pow;
	const int tree_size = 1 << tree_pow;
	const block_step_t tree_align = tree_pow - 1;
	const int tree_align_size = 1 << (tree_align);
	const block_step_t external_pow = tree_pow - 2;

#if USE_POS32
	static constexpr block_step_t tree_pow_max = FARMESH_STEP_MAX;
#else
	static constexpr block_step_t tree_pow_max = 12;
#endif
};

const auto farmesh_to_tree_pow = [](const auto farmesh) {
	return std::min<block_step_t>(tree_params_t::tree_pow_max,
			rangeToStep(farmesh) - 1); // -2 ? TODO: test and tune
};

child_t tree_params_to_child(const tree_params_t &tree_params,
		const v3bpos_t &player_block_pos, pos_t two_d = {})
{
	return {.pos = v3tpos_t((((tpos_t)player_block_pos.X >> tree_params.tree_align)
									<< tree_params.tree_align) -
									(tree_params.tree_align_size >> 1),
					two_d
							?: (((tpos_t)(player_block_pos.Y) >> tree_params.tree_align)
									   << tree_params.tree_align) -
									   (tree_params.tree_align_size >> 1),
					(((tpos_t)(player_block_pos.Z) >> tree_params.tree_align)
							<< tree_params.tree_align) -
							(tree_params.tree_align_size >> 1)),
			.size{tree_params.tree_size}};
}

std::optional<tree_result_t> getFarParams(const MapDrawControl &draw_control,
		const v3bpos_t &player_block_pos, const v3bpos_t &blockpos, bool cell_each)
{
	const auto blockpos_aligned_cell =
			cell_each ? blockpos : align_shift(blockpos, draw_control.cell_size_pow);
	const tree_params_t tree_params{.tree_pow{farmesh_to_tree_pow(draw_control.farmesh)}};
	const auto start = tree_params_to_child(tree_params, player_block_pos);
	const auto res =
			find({.player_pos{player_block_pos.X, player_block_pos.Y, player_block_pos.Z},
						 .block_pos{blockpos_aligned_cell.X, blockpos_aligned_cell.Y,
								 blockpos_aligned_cell.Z},
						 .cell_size_pow{draw_control.cell_size_pow},
						 .farmesh_quality_pow{draw_control.farmesh_quality_pow},
						 .cell_size_each{cell_each}},
					start);
	return res;
}

block_step_t getFarStep(const MapDrawControl &draw_control,
		const v3bpos_t &player_block_pos, const v3bpos_t &blockpos, bool cell_each)
{
	const auto res = getFarParams(draw_control, player_block_pos, blockpos, cell_each);
	if (res) {
		return res->step;
	}
	return 0;
}

v3bpos_t getFarActualBlockPos(const MapDrawControl &draw_control,
		const v3bpos_t &player_block_pos, const v3bpos_t &blockpos, const bool cell_each)
{
	const auto res = getFarParams(draw_control, player_block_pos, blockpos, cell_each);
	if (res) {
#if USE_POS32
		return res->pos;
#else
		/*
		const auto szw = 1 << (res->size + cell_size_pow);
		if (res->pos.X + szw > MAX_MAP_GENERATION_LIMIT ||
				res->pos.X - szw< -MAX_MAP_GENERATION_LIMIT ||
				res->pos.Y + szw > MAX_MAP_GENERATION_LIMIT ||
				res->pos.Y - szw< -MAX_MAP_GENERATION_LIMIT ||
				res->pos.Z + szw > MAX_MAP_GENERATION_LIMIT ||
				res->pos.Z - szw< -MAX_MAP_GENERATION_LIMIT)
			return {};
*/
		return v3bpos_t(res->pos.X, res->pos.Y, res->pos.Z);
#endif
	}
	const tree_params_t tree_params{.tree_pow{farmesh_to_tree_pow(draw_control.farmesh)}};
	const auto &ext_align = tree_params.external_pow; // + cell_size_pow;
	const v3bpos_t ret{to_bpos((blockpos.X >> ext_align) << ext_align),
			to_bpos((blockpos.Y >> ext_align) << ext_align),
			to_bpos((blockpos.Z >> ext_align) << ext_align)};
	return ret;
}

struct each_param_t
{
	const v3tpos_t &player_pos;
	const uint8_t cell_size_pow;
	const uint8_t farmesh_quality_pow;
	const bool cell_size_each{1};
	const std::function<bool(const tree_result_t &)> &func;
	const bool two_d{false};
};

bool emit_tree_cell(const each_param_t &param, const child_t &mesh_child)
{
	if (!param.cell_size_each) {
		return param.func(make_tree_result(mesh_child, param.cell_size_pow, false));
	}

	const tpos_t storage_size = mesh_child.size >> param.cell_size_pow;
	const size_t blocks_per_side = 1 << param.cell_size_pow;
	const size_t y_count = param.two_d ? 1 : blocks_per_side;
	for (size_t y = 0; y < y_count; ++y)
		for (size_t z = 0; z < blocks_per_side; ++z)
			for (size_t x = 0; x < blocks_per_side; ++x) {
				const child_t storage_child{
						.pos = mesh_child.pos + v3tpos_t(x * storage_size,
														y * storage_size,
														z * storage_size),
						.size = storage_size,
				};
				if (param.func(
							make_tree_result(storage_child, param.cell_size_pow, true)))
					return true;
			}
	return false;
}

bool each(const each_param_t &param, const child_t &child)
{
	if (is_tree_cell(
				child, param.player_pos, param.cell_size_pow, param.farmesh_quality_pow))
		return emit_tree_cell(param, child);

	const auto children = split(child);
	const size_t child_count = param.two_d ? 4 : children.size();
	for (size_t i = 0; i < child_count; ++i) {
		if (each(param, children[i]))
			return true;
	}
	return false;
}

void runFarAll(const v3bpos_t &player_block_pos, uint8_t cell_size_pow, int farmesh,
		uint8_t farmesh_quality_pow, pos_t two_d, bool cell_each, block_step_t max_step,
		const std::function<bool(const v3bpos_t &, const bpos_t &, const block_step_t &)>
				&func)
{
	// A tree cell cannot be smaller than one client mesh cell.
	const auto tree_pow = std::max<block_step_t>(
			cell_size_pow, max_step ?: farmesh_to_tree_pow(farmesh));
	const tree_params_t tree_params{.tree_pow{tree_pow}};
	const auto start = tree_params_to_child(tree_params, player_block_pos, two_d);
	const auto func_convert = [&func](const tree_result_t &child) {
		return func(
				v3bpos_t{child.pos.X, child.pos.Y, child.pos.Z}, child.size, child.step);
	};

	//DUMP(start.pos, start.size, (int)tree_params.tree_align, max_step);
	each(
			{
					.player_pos{
							player_block_pos.X, player_block_pos.Y, player_block_pos.Z},
					.cell_size_pow{cell_size_pow},
					.farmesh_quality_pow{farmesh_quality_pow},
					.cell_size_each{cell_each},
					.func{func_convert},
					.two_d{
							static_cast<bool>(two_d),
					},
			},
			start);
}

#endif
}
