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

#include <atomic>
#include <cstdint>
#include <thread>
#include "client/camera.h"
#include "irr_v3d.h"
#include "irrlichttypes.h"
#include "mapblock.h"
#include "threading/async.h"
#include "threading/concurrent_unordered_map.h"
#include "util/unordered_map_hash.h"

class Client;
class Mapgen;
class Server;

#ifdef __EMSCRIPTEN__
#define FARMESH_FAST 1
#endif

// #define FARMESH_FAST 1
// #define FARMESH_DEBUG 1 // One direction, one thread, no neighborhoods
#define FARMESH_SHADOWS 1
//#if defined(__ANDROID__) || defined(__EMSCRIPTEN__)
#define FARMESH_CLEAN 1
//#endif

class FarMesh
{
public:
	FarMesh(Client *client, Server *server);

	~FarMesh();

	uint8_t update(v3opos_t camera_pos,
			//v3f camera_dir, f32 camera_fov, CameraMode camera_mode, f32 camera_pitch, f32 camera_yaw,
			v3pos_t m_camera_offset,
			//float brightness,
			int render_range, float speed);
	bool makeFarBlock(
			const v3bpos_t &blockpos, block_step_t step, const bool low_priority = false);
	size_t makeFarBlocks(const v3bpos_t &blockpos, block_step_t step);
	void publishFarBlock(const MapBlockPtr &block, bool check_coarser = true);

	bool enqueueFarMeshForBlock(const v3bpos_t &blockpos, const block_step_t step,
			const MapBlockPtr &block, const double timestamp,
			const bool low_priority = false);
	void stop() { farmesh_thread_stop = true; }
	void restart();

	bool game_update_complete{};

private:
	//std::vector<v3bpos_t> m_make_far_blocks_list;

	//v3opos_t m_camera_pos = {-1337, -1337, -1337};
	v3pos_t m_camera_pos_aligned{-1337, -1337, -1337};
	/*v3f m_camera_dir;
	f32 m_camera_fov;
	f32 m_camera_pitch;
	f32 m_camera_yaw;*/
	Client *m_client{};
	const MapDrawControl *m_control{};
	pos_t distance_min{MAP_BLOCKSIZE * 9};
	//v3pos_t m_camera_offset;
	float m_speed{};

#if FARMESH_FAST
	constexpr static uint16_t grid_size_max_y{32};
#else
	constexpr static uint16_t grid_size_max_y{64};
#endif

	//constexpr static uint16_t grid_size_max_y = 48;
	//constexpr static uint16_t grid_size_max_y = 128;
	//constexpr static uint16_t grid_size_max_y = 256;
	constexpr static uint16_t grid_size_max_x{grid_size_max_y};
	static constexpr uint16_t grid_size_x{grid_size_max_x};
	static constexpr uint16_t grid_size_y{grid_size_max_y};
	static constexpr uint16_t grid_size_xy{grid_size_x * grid_size_y};

	Mapgen *mg{};

	struct ray_cache
	{
		unsigned int finished{MAP_BLOCKSIZE * 2}; // last depth, -1 if visible
		content_t visible{};
		size_t step_num{};
	};
	using direction_cache = std::array<ray_cache, grid_size_xy>;
	std::array<direction_cache, 6> direction_caches;
	//v3pos_t direction_caches_pos;
	std::array<unordered_map_v3pos<bool>, 6> mg_caches;
	struct plane_cache
	{
		int processed{-1};
	};
	std::array<plane_cache, 6> plane_processed;
	std::atomic_uint last_distance_max{};
	int go_direction(const size_t dir_n);
	int go_flat();
	int go_container(bool only_received, const block_step_t step_limit = 0);
	uint32_t far_iteration_pos{};
	double far_iteration_pos_time{};
	bool want_reset = false;
	//bool mesh_complete_set{};
	//bool grid_finished{};
	uint32_t collect_reset_timestamp{static_cast<uint32_t>(-1)};
	//uint8_t planes_processed_last{};
	std::array<async_step_runner, 6> async_direction;
	async_step_runner async_cleaner;
	int async_cleaner_next{};
	bool farmesh_flat{true};
	bool farmesh_ray{true};
	uint16_t farmesh_wait_server{2};
	struct BlockTodo
	{
		MapBlockPtr block;
		double timestamp;
	};
	std::array<concurrent_unordered_map<v3bpos_t, BlockTodo>, FARMESH_STEP_MAX * 2>
			farmesh_make_queue;
	std::atomic_bool farmesh_make_queue_complete{true};
	std::atomic_size_t farmesh_make_queue_processed{};
	std::atomic_size_t farmesh_make_queue_size{};

	std::thread farmesh_thread;
	bool farmesh_thread_stop{};
	void processFarmeshQueue();
	void onSettingChanged(const std::string &name);
};
