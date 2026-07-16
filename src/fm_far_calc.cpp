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
#include <cstdint>
#include <optional>
#include <bit>

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

std::optional<tree_result_t> find(
		const find_param_t &param, const child_t &child, const uint16_t depth = 0)
{
	const auto make_result = [&param](const child_t &result_child) {
		return make_tree_result(result_child, param.cell_size_pow, param.cell_size_each);
	};

	const auto sz = child.size; //<< param.cell_size_pow;
	if (!(param.block_pos.X >= child.pos.X && param.block_pos.X < child.pos.X + sz &&
				param.block_pos.Y >= child.pos.Y &&
				param.block_pos.Y < child.pos.Y + sz &&
				param.block_pos.Z >= child.pos.Z &&
				param.block_pos.Z < child.pos.Z + sz)) {
		return {};
		/*
			if (depth) {
			return {};
		} else {
			return make_result(child);
		}
	    */
	}

	if (child.size <= (1 << (param.cell_size_pow))) {
		return make_result(child);
	}

	const tpos_t childSize = child.size >> 1;
	/* round
	const auto distance =
			((tpos_t)std::hypot(param.player_pos.X - (child.pos.X + childSize),
					 param.player_pos.Y - (child.pos.Y + childSize),
					 param.player_pos.Z - (child.pos.Z + childSize)));
	*/

	const auto distance =
			(std::max({std::abs((tpos_t)param.player_pos.X - (child.pos.X + childSize)),
					std::abs((tpos_t)param.player_pos.Y - (child.pos.Y + childSize)),
					std::abs((tpos_t)param.player_pos.Z - (child.pos.Z + childSize))}));
	const auto next_child_size =
			child.size << (1 + std::max(param.farmesh_quality_pow, param.cell_size_pow) -
						   (param.cell_size_each ? 0 : param.cell_size_pow));
	if (distance >= next_child_size) {
		return make_result(child);
	}
	const auto childSizePos = childSize;
	for (const auto &child : {
				 child_t{.pos{child.pos}, .size = childSize},
				 child_t{.pos = v3tpos_t(
								 child.pos.X + childSizePos, child.pos.Y, child.pos.Z),
						 .size = childSize},
				 child_t{.pos = v3tpos_t(
								 child.pos.X, child.pos.Y + childSizePos, child.pos.Z),
						 .size = childSize},
				 child_t{.pos = v3tpos_t(child.pos.X + childSizePos,
								 child.pos.Y + childSizePos, child.pos.Z),
						 .size = childSize},
				 child_t{.pos = v3tpos_t(
								 child.pos.X, child.pos.Y, child.pos.Z + childSizePos),
						 .size = childSize},
				 child_t{.pos = v3tpos_t(child.pos.X + childSizePos, child.pos.Y,
								 child.pos.Z + childSizePos),
						 .size = childSize},
				 child_t{.pos = v3tpos_t(child.pos.X, child.pos.Y + childSizePos,
								 child.pos.Z + childSizePos),
						 .size = childSize},
				 child_t{.pos = v3tpos_t(child.pos.X + childSizePos,
								 child.pos.Y + childSizePos, child.pos.Z + childSizePos),
						 .size = childSize},
		 }) {

		if (const auto res = find(param, child, depth + 1); res) {
			return res;
		}
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
	const auto blockpos_aligned_cell = align_shift(blockpos, draw_control.cell_size_pow);
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

void each(const each_param_t &param, const child_t &child)
{
	const auto make_result = [&param](const child_t &result_child) {
		return make_tree_result(result_child, param.cell_size_pow, param.cell_size_each);
	};

	if (child.size <= (1 << param.cell_size_pow)) {
		param.func(make_result(child));
		return;
	}

	const tpos_t childSize = child.size >> 1;
	/*
	const auto distance =
			((tpos_t)std::hypot(param.player_pos.X - (child.pos.X + childSize),
					 param.player_pos.Y - (child.pos.Y + childSize),
					 param.player_pos.Z - (child.pos.Z + childSize)) >>
					param.cell_size_pow)
			<< param.cell_size_pow;
			*/

	const auto distance =
			(std::max({std::abs((tpos_t)param.player_pos.X - (child.pos.X + childSize)),
					std::abs((tpos_t)param.player_pos.Y - (child.pos.Y + childSize)),
					std::abs((tpos_t)param.player_pos.Z - (child.pos.Z + childSize))}));

	const auto next_child_size =
			child.size << (1 + std::max(param.farmesh_quality_pow, param.cell_size_pow) -
						   (param.cell_size_each ? 0 : param.cell_size_pow));
	if (distance >= next_child_size) {
		param.func(make_result(child));
		return;
	}

	const auto childPosNext = childSize;

	uint8_t i{0};
	for (const auto &child : {
				 // first with unchanged Y for 2d
				 child_t{.pos{child.pos}, .size = childSize},
				 child_t{.pos = v3tpos_t(
								 child.pos.X + childPosNext, child.pos.Y, child.pos.Z),
						 .size = childSize},
				 child_t{.pos = v3tpos_t(
								 child.pos.X, child.pos.Y, child.pos.Z + childPosNext),
						 .size = childSize},
				 child_t{.pos = v3tpos_t(child.pos.X + childPosNext, child.pos.Y,
								 child.pos.Z + childPosNext),
						 .size = childSize},

				 // two_d ends here

				 child_t{.pos = v3tpos_t(
								 child.pos.X, child.pos.Y + childPosNext, child.pos.Z),
						 .size = childSize},
				 child_t{.pos = v3tpos_t(child.pos.X + childPosNext,
								 child.pos.Y + childPosNext, child.pos.Z),
						 .size = childSize},
				 child_t{.pos = v3tpos_t(child.pos.X, child.pos.Y + childPosNext,
								 child.pos.Z + childPosNext),
						 .size = childSize},
				 child_t{.pos = v3tpos_t(child.pos.X + childPosNext,
								 child.pos.Y + childPosNext, child.pos.Z + childPosNext),
						 .size = childSize},
		 }) {
		if (param.two_d && i++ >= 4) {
			break;
		}

		if (child.size <= (1 << (param.cell_size_pow))) {
			if (param.func(make_result(child))) {
				return;
			}
			continue;
		}

		each(param, child);
	}
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
