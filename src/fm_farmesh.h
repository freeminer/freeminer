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

#include "client/camera.h"
#include "fm_nodecontainer.h"
#include "irr_v3d.h"
#include "threading/async.h"

class Client;
class Mapgen;
class Server;
constexpr size_t FARMESH_MATERIAL_COUNT = 2;

class FarContainer : public NodeContainer
{

public:
	FarContainer();
	const MapNode &getNodeRefUnsafe(const v3pos_t &p) override;
	MapNode getNodeNoExNoEmerge(const v3pos_t &p) override;
	MapNode getNodeNoEx(const v3pos_t &p) override;
	Mapgen *m_mg;
};

class FarMesh
{
public:
	FarMesh(Client *client, Server *server, MapDrawControl *m_control);

	~FarMesh();

	void update(v3opos_t camera_pos,
			//v3f camera_dir, f32 camera_fov, CameraMode camera_mode, f32 camera_pitch, f32 camera_yaw,
			v3pos_t m_camera_offset,
			//float brightness,
			int render_range, float speed);
	void makeFarBlock(const v3bpos_t &blockpos);
	void makeFarBlock7(const v3bpos_t &blockpos, size_t step);
	//void makeFarBlocks(const v3bpos_t &blockpos);

private:
	std::vector<v3bpos_t> m_make_far_blocks_list;

	v3opos_t m_camera_pos = {-1337, -1337, -1337};
	v3pos_t m_camera_pos_aligned {0,0,0};
	/*v3f m_camera_dir;
	f32 m_camera_fov;
	f32 m_camera_pitch;
	f32 m_camera_yaw;*/
	Client *m_client;
	MapDrawControl *m_control;
	static constexpr pos_t distance_min = 8 * MAP_BLOCKSIZE;
	v3pos_t m_camera_offset;
	float m_speed;
	constexpr static uint16_t grid_size_max_y = 64;
	//constexpr static uint16_t grid_size_max_y = 32;
	//constexpr static uint16_t grid_size_max_y = 48;
	//constexpr static uint16_t grid_size_max_y = 128;
	//constexpr static uint16_t grid_size_max_y = 256;
	constexpr static uint16_t grid_size_max_x = grid_size_max_y;
	static constexpr uint16_t grid_size_x = grid_size_max_x;
	static constexpr uint16_t grid_size_y = grid_size_max_y;
	static constexpr uint16_t grid_size_xy = grid_size_x * grid_size_y;
	Mapgen *mg = nullptr;

	FarContainer farcontainer;

	struct ray_cache
	{
		unsigned int finished =
				distance_min - MAP_BLOCKSIZE; /// last depth, -1 if visible
		content_t visible = {};
		size_t step_num = {};
	};
	using direction_cache = std::array<ray_cache, grid_size_xy>;
	std::array<direction_cache, 6> direction_caches;
	v3pos_t direction_caches_pos;
	std::array<unordered_map_v3pos<bool>, 6> mg_caches;
	struct plane_cache
	{
		int processed = -1;
	};
	std::array<plane_cache, 6> plane_processed;
	unsigned int last_distance_max = 0;
	int go_direction(const size_t dir_n);
	std::array<async_step_runner, 6> async;
	int timestamp_complete = 0;
	//int timestamp_clean = 0;
	bool complete_set = false;
	int planes_processed_last = 0;
};
