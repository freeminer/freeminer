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

#pragma once

#include "irr_v3d.h"
#include "irrlichttypes.h"

struct MapDrawControl;

#if USE_POS32
constexpr uint16_t tree_pow_default = FARMESH_STEP_MAX;
#else
constexpr uint16_t tree_pow_default = 12;
#endif

struct tree_params
{
	const uint16_t tree_pow = tree_pow_default;
	const uint16_t tree_size = 1 << tree_pow;
	const uint16_t tree_align = tree_pow - 1;
	const uint16_t tree_align_size = 1 << (tree_align);
	const uint16_t external_pow = tree_pow - 2;
};

block_step_t getLodStep(const MapDrawControl &draw_control,
		const v3bpos_t &playerblockpos, const v3bpos_t &block_pos, const pos_t speedf);
block_step_t getFarStepCellSize(const MapDrawControl &draw_control, const v3bpos_t &ppos,
		const v3bpos_t &blockpos, uint8_t cell_size_pow);
block_step_t getFarStep(const MapDrawControl &draw_control,
		const v3bpos_t &playerblockpos, const v3bpos_t &block_pos);
block_step_t getFarStepBad(const MapDrawControl &draw_control,
		const v3bpos_t &playerblockpos, const v3bpos_t &block_pos);
bool inFarGrid(const v3bpos_t &blockpos, const v3bpos_t &playerblockpos,
		block_step_t step, const MapDrawControl &draw_control);
v3bpos_t getFarActual(const v3bpos_t &blockpos, const v3bpos_t &playerblockpos,
		block_step_t step, const MapDrawControl &draw_control);
v3bpos_t playerBlockAlign(
		const MapDrawControl &draw_control, const v3bpos_t &playerblockpos);
void runFarAll(const v3bpos_t &ppos,
		uint8_t cell_size_pow,  uint8_t farmesh_quality, pos_t two_d,
		const std::function<bool(const v3bpos_t &, const bpos_t &)> &func);
