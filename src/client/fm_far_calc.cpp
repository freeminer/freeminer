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

#include "client/clientmap.h"
#include "irr_v3d.h"
#include "mapblock.h"

int getLodStep(const MapDrawControl &draw_control, const v3bpos_t &playerblockpos,
		const v3bpos_t &blockpos)
{
	int range = radius_box(playerblockpos, blockpos);
	if (draw_control.lodmesh) {
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

#if 0
// Fast math but not finised

int getFarStep(const MapDrawControl &draw_control, const v3bpos_t &playerblockpos,
		const v3bpos_t &blockpos)
{
	if (!draw_control.farmesh)
		return 0;
	const auto step_pow2 = int(log(draw_control.cell_size) / log(2)) + draw_control.farmesh_quality;
	const auto player_aligned = playerBlockAlign(draw_control, playerblockpos);
	const auto block_aligned = align(blockpos, step_pow2);

	auto calc_step = [&](const auto &player_aligned, const auto &block_aligned) {
		const auto len_vec = player_aligned - block_aligned;
		const auto distance = std::max({abs(len_vec.X), abs(len_vec.Y), abs(len_vec.Z)});
		auto step = int(log(distance >> step_pow2) / log(2));
		return step;
	};

	auto step = calc_step(player_aligned, block_aligned);

	// bug here, but where?
	// maybe need check distance of block end, or some neighbor block with next step


	if (step < 0)
		step = 0;
	if (step > FARMESH_STEP_MAX)
		step = FARMESH_STEP_MAX;
	return step;
}


v3bpos_t getFarActual(v3bpos_t blockpos, int step, int cell_size)
{
	step += log(cell_size) / log(2);
	const auto blockpos_aligned = align(blockpos, step);
	return blockpos_aligned;
}
#endif

bool inFarGrid(
		const v3bpos_t &blockpos, const v3bpos_t &playerblockpos, int step, int cell_size)
{
	const auto act = getFarActual(blockpos, playerblockpos, step, cell_size);
	return act == blockpos;
}

#if 1
// slower using tree

class OctoTree
{
	v3bpos_t for_player_block_pos = {-1337, -1337, -1337};
	const v3s32 pos;
	const int32_t size;
	std::optional<std::array<std::unique_ptr<OctoTree>, 8>> children;

public:
	OctoTree(const v3s32 &pos, const int32_t size) : pos{pos}, size{size} {}

	bool isLeaf() { return !children.has_value(); }

	void ensureChildren()
	{
		if (isLeaf()) {
			const auto childSize = size >> 1;
			children = {std::make_unique<OctoTree>(pos, childSize),
					std::make_unique<OctoTree>(
							v3s32(pos.X + childSize, pos.Y, pos.Z), childSize),
					std::make_unique<OctoTree>(
							v3s32(pos.X, pos.Y + childSize, pos.Z), childSize),
					std::make_unique<OctoTree>(
							v3s32(pos.X + childSize, pos.Y + childSize, pos.Z),
							childSize),
					std::make_unique<OctoTree>(
							v3s32(pos.X, pos.Y, pos.Z + childSize), childSize),
					std::make_unique<OctoTree>(
							v3s32(pos.X + childSize, pos.Y, pos.Z + childSize),
							childSize),
					std::make_unique<OctoTree>(
							v3s32(pos.X, pos.Y + childSize, pos.Z + childSize),
							childSize),
					std::make_unique<OctoTree>(v3s32(pos.X + childSize, pos.Y + childSize,
													   pos.Z + childSize),
							childSize)};
		}
	}

	std::optional<std::pair<v3bpos_t, bpos_t>> find(const v3bpos_t &p)
	{
		if (!(p.X >= pos.X && p.X < pos.X + size && p.Y >= pos.Y && p.Y < pos.Y + size &&
					p.Z >= pos.Z && p.Z < pos.Z + size)) {
			return {};
		}

		if (isLeaf()) {
			return {{v3bpos_t(pos.X, pos.Y, pos.Z), size}};
		} else {
			for (const auto &child : children.value()) {
				const auto res = child->find(p);
				if (res)
					return res;
			}
		}
		return {};
	}

	void rasterizeByDistance(const v3bpos_t &from, int cell_size_pow)
	{
		// TODO: lru cache with 10 size here
		if (for_player_block_pos == from)
			return;
		for_player_block_pos = from;
		if (size < (1 << (1 + cell_size_pow)))
			return;
		auto distance = std::max({std::abs(from.X - pos.X - (size >> 1)),
				std::abs(from.Y - pos.Y - (size >> 1)),
				std::abs(from.Z - pos.Z - (size >> 1))});
		//distance>>=1; // farmesh_quality

		if (distance >= size) {
			children = {};
		} else {
			ensureChildren();
			for (const auto &child : children.value()) {
				child->rasterizeByDistance(from, cell_size_pow);
			}
		}
	}
};

const auto sz = 1 << FARMESH_STEP_MAX;
const auto start_pos = -(sz >> 1);
thread_local auto tree = OctoTree({start_pos, start_pos, start_pos}, sz);

int getFarStep(const MapDrawControl &draw_control, const v3bpos_t &playerblockpos,
		const v3bpos_t &blockpos)
{
	const auto blockpos_aligned_cell = align(blockpos, draw_control.cell_size_pow);

	tree.rasterizeByDistance(playerblockpos, draw_control.cell_size_pow);
	const auto res = tree.find(blockpos_aligned_cell);
	if (!res) {
		return {};
	}
	const auto step = log(res->second) / log(2) - draw_control.cell_size_pow;
	return step;
}

v3bpos_t getFarActual(
		v3bpos_t blockpos, const v3bpos_t &playerblockpos, int step, int cell_size)
{
	const auto cell_size_pow = int(log(cell_size) / log(2));
	const auto blockpos_aligned_cell = align(blockpos, cell_size_pow);
	tree.rasterizeByDistance(playerblockpos, cell_size_pow);
	const auto res = tree.find(blockpos_aligned_cell);
	if (!res) {
		return {};
	}
	return res->first;
}

#endif
