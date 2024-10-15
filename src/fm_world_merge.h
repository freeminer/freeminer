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

#include <unordered_set>
#include "mapblock.h"

class Server;
class MapDatabase;

struct WorldMerger
{
	Server *m_server{};

	std::function<bool(void)> stop_func;
	std::function<bool(void)> throttle_func;

	bool stop()
	{
		if (stop_func)
			return stop_func();
		return false;
	}

	bool throttle()
	{
		if (throttle_func)
			return throttle_func();
		return false;
	}

	uint32_t world_merge_throttle{};
	uint32_t world_merge_max_clients{};
	int16_t world_merge_load_all{}; // -1 : auto;  0 : disable;   1 : force
	bool partial{};
	uint32_t lazy_up{};

	void merge_one_block(MapDatabase *dbase, MapDatabase *dbase_up,
			const v3bpos_t &bpos_aligned, MapBlock::block_step_t step);

	bool merge_one_step(
			MapBlock::block_step_t step, std::unordered_set<v3bpos_t> &blocks_todo);
	bool merge_list(std::unordered_set<v3bpos_t> &blocks_todo);
	bool merge_all();
	bool merge_server_diff();
};
