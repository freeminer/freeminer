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

struct MapDrawControl;

int getLodStep(const MapDrawControl &draw_control, const v3bpos_t &playerblockpos,
		const v3bpos_t &block_pos, const pos_t speedf);
int getFarStep(const MapDrawControl &draw_control, const v3bpos_t &playerblockpos,
		const v3bpos_t &block_pos);
bool inFarGrid(const v3bpos_t &blockpos, const v3bpos_t &playerblockpos, int step,
		const MapDrawControl &draw_control);
v3bpos_t getFarActual(v3bpos_t blockpos, const v3bpos_t &playerblockpos, int step,
		const MapDrawControl &draw_control);
v3bpos_t playerBlockAlign(
		const MapDrawControl &draw_control, const v3bpos_t &playerblockpos);
