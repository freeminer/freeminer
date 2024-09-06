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

#include "client/clientmap.h"
#include "constants.h"
#include "irr_v3d.h"
#include "irrlichttypes.h"

int getLodStep(const MapDrawControl &draw_control, const v3bpos_t &playerblockpos,
		const v3bpos_t &blockpos, const pos_t speedf)
{
	if (draw_control.lodmesh) {
		int range = radius_box(playerblockpos, blockpos);
		const auto speed_blocks = speedf / (BS * MAP_BLOCKSIZE);
		if (static auto i = 0; !(++i % 1000))
		if (range > 1 && speed_blocks > 1) {
			range += speed_blocks;
		}

		const auto cells = std::max<int>(draw_control.cell_size * 2,
				draw_control.lodmesh / draw_control.cell_size);
		// for (int i = 8; i >= 0; --i) {
		// 	if (range >= cells + draw_control.lodmesh * pow(2, i))
		// 		return i;
		// }

		if (range >= cells + draw_control.lodmesh * 64) // cell_size = 4
			return 8;
		if (range >= cells + draw_control.lodmesh * 32)
			return 7;
		if (range >= cells + draw_control.lodmesh * 16)
			return 6;
		if (range >= cells + draw_control.lodmesh * 8)
			return 5;
		if (range >= cells + draw_control.lodmesh * 4)
			return 4;
		else if (range >= cells + draw_control.lodmesh * 2)
			return 3;
		else if (range >= cells + draw_control.lodmesh)
			return 2;
		else if (range >= cells)
			return 1;
	}
	return 0;
};

#if 0
int getFarStep(const MapDrawControl &draw_control, const v3bpos_t &playerblockpos,
		const v3bpos_t &blockpos)
{
	if (!draw_control.farmesh)
		return 1;

	int range = radius_box(playerblockpos, blockpos);

	const auto next_step = 1;
	range >>= next_step; // TODO: configurable

	if (range <= 1)
		return 1;

	int skip = log(range) / log(2);
	//skip += log(draw_control.cell_size) / log(2);
	range = radius_box(v3pos_t((playerblockpos.X >> skip) << skip,
							   (playerblockpos.Y >> skip) << skip,
							   (playerblockpos.Z >> skip) << skip),
			v3pos_t((blockpos.X >> skip) << skip, (blockpos.Y >> skip) << skip,
					(blockpos.Z >> skip) << skip));
	range >>= next_step + int(log(draw_control.cell_size) / log(2)); // TODO: configurable
	if (range > 1) {
		skip = log(range) / log(2);
	}
	if (skip > FARMESH_STEP_MAX)
		skip = FARMESH_STEP_MAX;
	return skip;
};
#endif

auto align(auto pos, const int amount)
{
	(pos.X >>= amount) <<= amount;
	(pos.Y >>= amount) <<= amount;
	(pos.Z >>= amount) <<= amount;
	return pos;
}

v3bpos_t playerBlockAlign(
		const MapDrawControl &draw_control, const v3bpos_t &playerblockpos)
{
	const auto step_pow2 = draw_control.cell_size_pow + draw_control.farmesh_quality;
	return align(playerblockpos, step_pow2) + (draw_control.cell_size >> 1);
}

#if 1

#if USE_POS32
using tpos_t = bpos_t;
using v3tpos_t = v3bpos_t;
#else
using tpos_t = int32_t;
using v3tpos_t = v3s32;
#endif
bool inFarGrid(const v3bpos_t &blockpos, const v3bpos_t &playerblockpos, int step,
		const MapDrawControl &draw_control)
{
	const auto act = getFarActual(blockpos, playerblockpos, step, draw_control);
	return act == blockpos;
}

struct child_t
{
	v3tpos_t pos;
	tpos_t size;
};

std::optional<child_t> find(const v3bpos_t &block_pos, const v3bpos_t &player_pos,
		const child_t &child, const int cell_size_pow, uint16_t farmesh_quality)
{
	if (!(block_pos.X >= child.pos.X && block_pos.X < child.pos.X + child.size &&
				block_pos.Y >= child.pos.Y && block_pos.Y < child.pos.Y + child.size &&
				block_pos.Z >= child.pos.Z && block_pos.Z < child.pos.Z + child.size))
		return {};

	if (child.size < (1 << (1 + cell_size_pow)))
		return child;

	auto distance = std::max({std::abs(player_pos.X - child.pos.X - (child.size >> 1)),
			std::abs(player_pos.Y - child.pos.Y - (child.size >> 1)),
			std::abs(player_pos.Z - child.pos.Z - (child.size >> 1))});

	if (farmesh_quality)
		distance /= farmesh_quality;
	if (distance >= child.size) {
		return child;
	}
	const tpos_t childSize = child.size >> 1;
	for (const auto &child : {
				 child_t{.pos{child.pos}, .size = childSize},
				 child_t{.pos = v3tpos_t(
								 child.pos.X + childSize, child.pos.Y, child.pos.Z),
						 .size = childSize},
				 child_t{.pos = v3tpos_t(
								 child.pos.X, child.pos.Y + childSize, child.pos.Z),
						 .size = childSize},
				 child_t{.pos = v3tpos_t(child.pos.X + childSize, child.pos.Y + childSize,
								 child.pos.Z),
						 .size = childSize},
				 child_t{.pos = v3tpos_t(
								 child.pos.X, child.pos.Y, child.pos.Z + childSize),
						 .size = childSize},
				 child_t{.pos = v3tpos_t(child.pos.X + childSize, child.pos.Y,
								 child.pos.Z + childSize),
						 .size = childSize},
				 child_t{.pos = v3tpos_t(child.pos.X, child.pos.Y + childSize,
								 child.pos.Z + childSize),
						 .size = childSize},
				 child_t{.pos = v3tpos_t(child.pos.X + childSize, child.pos.Y + childSize,
								 child.pos.Z + childSize),
						 .size = childSize},
		 }) {
		const auto res =
				find(block_pos, player_pos, child, cell_size_pow, farmesh_quality);
		if (res) {
			return res;
		}
	}
	return {};
}

#if USE_POS32
const auto tree_pow = FARMESH_STEP_MAX;
#else
const auto tree_pow = 12;
#endif
const auto tree_size = 1 << tree_pow;
const auto tree_align = tree_pow - 1;
const auto tree_align_size = 1 << (tree_align);
const auto external_pow = tree_pow - 2;

int getFarStep(const MapDrawControl &draw_control, const v3bpos_t &ppos,
		const v3bpos_t &blockpos)
{
	const auto blockpos_aligned_cell = align(blockpos, draw_control.cell_size_pow);

	const auto start = child_t{
			.pos = v3tpos_t(
					((ppos.X >> tree_align) << tree_align) - (tree_align_size >> 1),
					((ppos.Y >> tree_align) << tree_align) - (tree_align_size >> 1),
					((ppos.Z >> tree_align) << tree_align) - (tree_align_size >> 1)),
			.size = tree_size};
	const auto res = find(blockpos_aligned_cell, ppos, start, draw_control.cell_size_pow,
			draw_control.farmesh_quality);
	if (res) {
		/*
#if !USE_POS32
		if (res->pos.X > MAX_MAP_GENERATION_LIMIT ||
				res->pos.X < -MAX_MAP_GENERATION_LIMIT ||
				res->pos.Y > MAX_MAP_GENERATION_LIMIT ||
				res->pos.Y < -MAX_MAP_GENERATION_LIMIT ||
				res->pos.Z > MAX_MAP_GENERATION_LIMIT ||
				res->pos.Z < -MAX_MAP_GENERATION_LIMIT)
			return {};
#endif
*/
		const auto step = int(log(res->size) / log(2)) - draw_control.cell_size_pow;
		return step;
	}
	return 0; // TODO! fix intersection with cell_size_pow
			  //return external_pow; //+ draw_control.cell_size_pow;
}

v3bpos_t getFarActual(v3bpos_t blockpos, const v3bpos_t &ppos, int step,
		const MapDrawControl &draw_control)
{
	const auto cell_size_pow = int(log(draw_control.cell_size) / log(2));
	const auto blockpos_aligned_cell = align(blockpos, cell_size_pow);

	const auto start = child_t{
			.pos = v3tpos_t(
					((ppos.X >> tree_align) << tree_align) - (tree_align_size >> 1),
					((ppos.Y >> tree_align) << tree_align) - (tree_align_size >> 1),
					((ppos.Z >> tree_align) << tree_align) - (tree_align_size >> 1)),
			.size = tree_size};
	const auto res = find(blockpos_aligned_cell, ppos, start, cell_size_pow,
			draw_control.farmesh_quality);
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
	const auto ext_align = external_pow; // + cell_size_pow;
	return v3bpos_t((blockpos.X >> ext_align) << ext_align,
			(blockpos.Y >> ext_align) << ext_align,
			(blockpos.Z >> ext_align) << ext_align);
}

#endif
