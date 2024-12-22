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

#include <cstdint>
#include <future>
#include <unordered_set>
#include "servermap.h"
#include "mapblock.h"

class Server;
class MapDatabase;
class WorldMerger
{
public:
	std::function<bool(void)> stop_func;
	std::function<bool(void)> throttle_func;
	std::function<uint32_t(void)> get_time_func;

	uint32_t world_merge_throttle{};
	uint32_t world_merge_max_clients{};
	int16_t world_merge_load_all{}; // -1 : auto;  0 : disable;   1 : force
	bool partial{};
	uint32_t lazy_up{};
	const NodeDefManager *const ndef{};
	Map *const smap{};
	ServerMap::far_dbases_t &far_dbases;
	std::unordered_set<v3bpos_t> changed_blocks_for_merge;
	int16_t m_map_compression_level{7};
	MapDatabase *const dbase{};
	std::string save_dir;
	std::future<void> last_async;
	~WorldMerger();
	void init();
	bool stop();
	bool throttle();

	void merge_one_block(MapDatabase *dbase, MapDatabase *dbase_up,
			const v3bpos_t &bpos_aligned, block_step_t step);

	bool merge_one_step(block_step_t step, std::unordered_set<v3bpos_t> &blocks_todo);
	bool merge_list(std::unordered_set<v3bpos_t> &blocks_todo);
	bool merge_all();
	bool merge_changed();
	bool merge_server_diff(
			concurrent_unordered_set<v3bpos_t> &smap_changed_blocks_for_merge,
			size_t min_blocks = 1);
	bool add_changed(const v3bpos_t &bpos);
};
