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
#include <cstdint>
#include <utility>

#include "fm_farmesh.h"

#include "client/client.h"
#include "client/clientmap.h"
#include "client/fm_far_calc.h"
#include "client/mapblock_mesh.h"
#include "constants.h"
#include "emerge.h"
#include "irr_v3d.h"
#include "mapblock.h"
#include "mapnode.h"
#include "profiler.h"
#include "server.h"
#include "threading/lock.h"
#include "util/numeric.h"
#include "util/timetaker.h"

const v3opos_t g_6dirso[6] = {
		// +right, +top, +back
		v3opos_t(0, 0, 1),	// back
		v3opos_t(1, 0, 0),	// right
		v3opos_t(0, 0, -1), // front
		v3opos_t(-1, 0, 0), // left
		v3opos_t(0, -1, 0), // bottom
		v3opos_t(0, 1, 0),	// top
};

void FarMesh::makeFarBlock(
		const v3bpos_t &blockpos, MapBlock::block_step_t step, bool near)
{
	g_profiler->add("Client: Farmesh make", 1);

	auto &client_map = m_client->getEnv().getClientMap();
	const auto &draw_control = client_map.getControl();
	const auto blockpos_actual =
			near ? blockpos
				 : getFarActual(blockpos, getNodeBlockPos(m_camera_pos_aligned), step,
						   draw_control);
	auto &far_blocks = //near ? m_client->getEnv().getClientMap().m_far_near_blocks :
			client_map.m_far_blocks;
	if (const auto it = client_map.far_blocks_storage[step].find(blockpos_actual);
			it != client_map.far_blocks_storage[step].end()) {
		auto &block = it->second;
		{
			const auto lock = far_blocks.lock_unique_rec();
			if (const auto &fbit = far_blocks.find(blockpos_actual);
					fbit != far_blocks.end()) {
				if (fbit->second.get() == block.get()) {
						block->setTimestampNoChangedFlag(timestamp_complete);
					return;
				}
				client_map.m_far_blocks_delete.emplace_back(fbit->second);
			}
			far_blocks.insert_or_assign(blockpos_actual, block);
			++m_client->m_new_meshes;
		}
		block->setTimestampNoChangedFlag(timestamp_complete);
		return;
	}
	MapBlockP block;
	bool new_block = false;
	{
		const auto lock = far_blocks.lock_unique_rec();
		if (const auto &it = far_blocks.find(blockpos_actual);
				it != far_blocks.end() && it->second->far_step == step) {
			block = it->second;
		} else {
			if (!block) {
				m_client->getEnv().getClientMap().m_far_blocks_ask.emplace(
						blockpos_actual, std::make_pair(step, timestamp_complete));

				new_block = true;
				block = std::make_shared<MapBlock>(
						&client_map, blockpos_actual, m_client);
				block->far_step = step;
				far_blocks.insert_or_assign(blockpos_actual, block);
				++m_client->m_new_meshes;
			}
		block->setTimestampNoChangedFlag(timestamp_complete);
		}
	}
	if (new_block) {
		std::async(std::launch::async, [this, block]() mutable {
			m_client->createFarMesh(block);
		});
	}
	return;
}

void FarMesh::makeFarBlocks(const v3bpos_t &blockpos, MapBlock::block_step_t step)
{
#if FARMESH_DEBUG || FARMESH_FAST || 1

	auto block_step_correct = getFarStep(m_client->getEnv().getClientMap().getControl(),
			getNodeBlockPos(m_camera_pos_aligned), blockpos);

	return makeFarBlock(blockpos, block_step_correct);
#endif

	// TODO: fix finding correct near blocks respecting their steps and enable:

	const static auto far = std::vector<v3pos_t>{
			v3pos_t(0, 0, 0), // self
	};
	const static auto near = std::vector<v3pos_t>{
			v3pos_t(0, 0, 0),  // self
			v3pos_t(0, 0, 1),  // back
			v3pos_t(1, 0, 0),  // right
			v3pos_t(0, 0, -1), // front
			v3pos_t(-1, 0, 0), // left
			v3pos_t(0, 1, 0),  // top
			v3pos_t(0, -1, 0), // bottom
	};
	const auto &use_dirs = near;
	const auto step_width = 1 << step;
	for (const auto &dir : use_dirs) {
		const auto bpos = blockpos + dir * step_width;
		auto block_step_correct =
				getFarStep(m_client->getEnv().getClientMap().getControl(),
						getNodeBlockPos(m_camera_pos_aligned), bpos);
		makeFarBlock(bpos, block_step_correct);
	}
}

#if 0
void FarMesh::makeFarBlocks(const v3bpos_t &blockpos)
{
	int radius = 20;
	int &dr = m_make_far_blocks_last;
	//int end_ms = os.clock() + tnt.time_max
	bool last = false;

	const int max_cycle_ms = 500;
	u32 end_ms = porting::getTimeMs() + max_cycle_ms;

	while (dr < radius) {
		if (porting::getTimeMs() > end_ms) {
			return;
			//last = 1;
		}

		if (m_make_far_blocks_list.empty()) {
			++dr;
			//if os.clock() > end_ms or dr>=radius then last=1 end
			for (pos_t dx = -dr; dx <= dr; dx += dr * 2) {
				for (pos_t dy = -dr; dy <= dr; ++dy) {
					for (pos_t dz = -dr; dz <= dr; ++dz) {
						m_make_far_blocks_list.emplace_back(dx, dy, dz);
					}
				}
			}
			for (int dy = -dr; dy <= dr; dy += dr * 2) {
				for (int dx = -dr + 1; dx <= dr - 1; ++dx) {
					for (int dz = -dr; dz <= dr; ++dz) {
						m_make_far_blocks_list.emplace_back(dx, dy, dz);
					}
				}
			}
			for (int dz = -dr; dz <= dr; dz += dr * 2) {
				for (int dx = -dr + 1; dx <= dr - 1; ++dx) {
					for (int dy = -dr + 1; dy <= dr - 1; ++dy) {
						m_make_far_blocks_list.emplace_back(dx, dy, dz);
					}
				}
			}
		}
		for (const auto p : m_make_far_blocks_list) {
			//DUMP(dr, p, blockpos);
			makeFarBlock(blockpos + p);
		}
		m_make_far_blocks_list.clear();

		if (last) {
			break;
		}
	}
	if (m_make_far_blocks_last >= radius) {
		m_make_far_blocks_last = 0;
	}
}
#endif

FarMesh::FarMesh(Client *client, Server *server, MapDrawControl *control) :
		m_client{client}, m_control{control}
{

	EmergeManager *emerge_use = server			   ? server->getEmergeManager()
								: client->m_emerge ? client->m_emerge.get()
												   : nullptr;
	if (emerge_use) {
		if (emerge_use->mgparams)
			mg = emerge_use->getFirstMapgen();

		m_client->far_container.m_mg = mg;
		const auto &ndef = m_client->getNodeDefManager();
		mg->visible_surface = ndef->getId("default:stone");
		mg->visible_water = ndef->getId("default:water_source");
		mg->visible_ice = ndef->getId("default:ice");
		mg->visible_surface_green = ndef->getId("default:dirt_with_grass");
		mg->visible_surface_dry = ndef->getId("default:dirt_with_dry_grass");
		mg->visible_surface_cold = ndef->getId("default:dirt_with_snow");
		mg->visible_surface_hot = ndef->getId("default:sand");
	}

	//for (size_t i = 0; i < process_order.size(); ++i)
	//	process_order[i] = i;
	//auto rng = std::default_random_engine{};
	//std::shuffle(std::begin(process_order), std::end(process_order), rng);
}

FarMesh::~FarMesh()
{
}

auto align_shift(auto pos, const auto amount)
{
	(pos.X >>= amount) <<= amount;
	(pos.Y >>= amount) <<= amount;
	(pos.Z >>= amount) <<= amount;
	return pos;
}

int FarMesh::go_direction(const size_t dir_n)
{
	TimeTaker time("Cleint: Farmesh [ms]");
	time.start();

	constexpr auto block_step_reduce = 1;
	constexpr auto align_reduce = 1;

	auto &cache = direction_caches[dir_n];
	auto &mg_cache = mg_caches[dir_n];

	auto &draw_control = m_client->getEnv().getClientMap().getControl();

	const auto dir = g_6dirso[dir_n];
	const auto grid_size_xy = grid_size_x * grid_size_y;

	int processed = 0;
	for (uint16_t i = 0; i < grid_size_xy; ++i) {
		auto &ray_cache = cache[i];
		if (ray_cache.finished > last_distance_max) {
			continue;
		}
		//uint16_t y = uint16_t(process_order[i] / grid_size_x);
		//uint16_t x = process_order[i] % grid_size_x;
		uint16_t y = uint16_t(i / grid_size_x);
		uint16_t x = i % grid_size_x;

		auto dir_first = dir * distance_min / 2;
		auto pos_center = dir_first + m_camera_pos;

		if (!dir.X)
			dir_first.X += distance_min / grid_size_x * (x - grid_size_x / 2);
		if (!dir.Y)
			dir_first.Y += distance_min / grid_size_x * (y - grid_size_x / 2);
		if (!dir.Z)
			dir_first.Z +=
					distance_min / grid_size_x * ((!dir.Y ? x : y) - grid_size_x / 2);

		auto dir_l = dir_first.normalize();

		auto pos_last = dir_l * ray_cache.finished * BS + pos_center;
		++ray_cache.step_num;
		for (size_t steps = 0; steps < 200; ++ray_cache.step_num, ++steps) {
#if !NDEBUG
			g_profiler->avg("Client: Farmesh processed", 1);
#endif
			//const auto dstep = ray_cache.step_num; // + 1;
			auto block_step_prev =
					getFarStepBad(draw_control, getNodeBlockPos(m_camera_pos_aligned),
							getNodeBlockPos(floatToInt(pos_last, BS)));

			const auto step_width_shift = (block_step_prev - block_step_reduce);
			const auto step_width = MAP_BLOCKSIZE
									<< (step_width_shift > 0 ? step_width_shift : 0);
			const auto &depth = ray_cache.finished;

			//if (depth > last_distance_max) {
			//ray_cache.finished = distance_min + step_width;// * (dstep - 1);
			//break;
			//}

			const auto pos = dir_l * depth * BS + m_camera_pos;
			pos_last = pos;

#if !USE_POS32

			const auto step_width_real =
					MAP_BLOCKSIZE << (block_step_prev + draw_control.cell_size_pow);
#else
			const auto step_width_real = step_width;
#endif

			if (pos.X + step_width_real * BS > MAX_MAP_GENERATION_LIMIT * BS ||
					pos.X < -MAX_MAP_GENERATION_LIMIT * BS ||
					pos.Y + step_width_real * BS > MAX_MAP_GENERATION_LIMIT * BS ||
					pos.Y < -MAX_MAP_GENERATION_LIMIT * BS ||
					pos.Z + step_width_real * BS > MAX_MAP_GENERATION_LIMIT * BS ||
					pos.Z < -MAX_MAP_GENERATION_LIMIT * BS) {
				ray_cache.finished = -1;
				break;
			}

			const int step_aligned_pow = ceil(log(step_width) / log(2)) - align_reduce;
			const auto pos_int = align_shift(
					floatToInt(pos, BS), step_aligned_pow > 0 ? step_aligned_pow : 0);

			if (radius_box(pos_int, m_camera_pos_aligned) > last_distance_max)
			{
				break;
			}

			++processed;

			if (depth >= draw_control.wanted_range) {
				auto &visible = ray_cache.visible;
				if (!visible) {
					if (const auto &it = mg_cache.find(pos_int); it != mg_cache.end()) {
						visible = it->second;
					} else {
						visible =
								mg->visible(pos_int) || mg->visible_water_level(pos_int);
						mg_cache[pos_int] = visible;
					}
				}

			}
			if (ray_cache.visible) {
				if (depth > MAP_BLOCKSIZE * 8) {
					ray_cache.finished = -1;
				}
				const auto block_pos_unaligned = getNodeBlockPos(pos_int);

				// /* todo

// TODO: glue between blocks and far blocks
#if 0
				const auto actual_blockpos = getFarActual(blockpos,
						m_camera_pos_aligned / MAP_BLOCKSIZE, block_step, *m_control);
				//DUMP(actual_blockpos, blockpos, m_camera_pos_aligned/MAP_BLOCKSIZE, block_step);
				if (m_client->getEnv().getClientMap().blocks_skip_farmesh.contains(
							actual_blockpos)) {
					//const auto block_step_m1 = block_step - 1;
					//makeFarBlock(blockpos + v3bpos_t{0, 0, 0}, block_step_m1);
					//DUMP(actual_blockpos, blockpos, blocks);
					//for (const auto bp : seven_blocks)
					const bpos_t blocks =
							pow(2, block_step + log(draw_control.cell_size) / log(2));
					//const bpos_t blocks =					pow(2, block_step);
					DUMP("mis", actual_blockpos, blockpos, pos_int, block_step,
							/*block_step_m1,*/ blocks);
					for (bpos_t x = 0; x < blocks; ++x)
						for (bpos_t y = 0; y < blocks; ++y)
							for (bpos_t z = 0; z < blocks; ++z) {
								makeFarBlock(
										actual_blockpos + v3bpos_t{x, y, z}, 0, true);
							}
					/*
					makeFarBlock(blockpos + v3bpos_t{1, 0, 0}, block_step_m1);
					makeFarBlock(blockpos + v3bpos_t{0, 1, 0}, block_step_m1);
					makeFarBlock(blockpos + v3bpos_t{1, 1, 0}, block_step_m1);
					makeFarBlock(blockpos + v3bpos_t{0, 0, 1}, block_step_m1);
					makeFarBlock(blockpos + v3bpos_t{1, 0, 1}, block_step_m1);
					makeFarBlock(blockpos + v3bpos_t{0, 1, 1}, block_step_m1);
					makeFarBlock(blockpos + v3bpos_t{1, 1, 1}, block_step_m1);
					*/
				} //else
#endif
				if (block_step_prev && depth >= draw_control.wanted_range) {
					makeFarBlocks(block_pos_unaligned, block_step_prev);
					ray_cache.finished = -1;
					break;
				}
			}

			ray_cache.finished += step_width;
		}
	}

	g_profiler->avg("Client: Farmesh [ms]", time.stop(true));
	// g_profiler->avg("Client: Farmesh processed", processed);

	return processed;
}

uint8_t FarMesh::update(v3opos_t camera_pos,
		//v3f camera_dir,
		//f32 camera_fov,
		//CameraMode camera_mode,
		//f32 camera_pitch, f32 camera_yaw,
		v3pos_t camera_offset,
		//float brightness,
		int render_range, float speed)
{
	if (!mg)
		return {};

	m_speed = speed;

	const auto camera_pos_aligned_int =
			playerBlockAlign(*m_control, floatToInt(camera_pos, BS * 16)) * MAP_BLOCKSIZE;
	const auto distance_max =
			(std::min<unsigned int>(render_range, 1.2 * m_client->fog_range / BS) >> 7)
			<< 7;

	auto &clientMap = m_client->getEnv().getClientMap();
	const auto far_fast = false;

/*
	const auto far_fast =
			!m_control->farmesh_stable &&
			(
					//m_client->getEnv().getClientMap().m_far_fast &&
					m_speed > 200 * BS ||
					m_camera_pos_aligned.getDistanceFrom(camera_pos_aligned_int) > 1000);
*/

	const auto set_new_cam_pos = [&]() {
		if (m_camera_pos_aligned == camera_pos_aligned_int)
			return;

		m_camera_pos_aligned = camera_pos_aligned_int;
		m_camera_pos = intToFloat(m_camera_pos_aligned, BS);
		plane_processed.fill({});

		direction_caches.fill({});
		direction_caches_pos = m_camera_pos_aligned;
	};

	if (!timestamp_complete) {
		if (!m_camera_pos_aligned.X && !m_camera_pos_aligned.Y &&
				!m_camera_pos_aligned.Z) {
			set_new_cam_pos();
		}
		clientMap.m_far_blocks_last_cam_pos = m_camera_pos_aligned;
		if (!last_distance_max)
			last_distance_max = distance_max;
	}

	if (last_distance_max < distance_max) {
		plane_processed.fill({});
		last_distance_max = distance_max; // * 1.1;
	}
	if (m_client->m_new_farmeshes) {
		m_client->m_new_farmeshes = 0;
		plane_processed.fill({});
	}

	/*
	if (mg->surface_2d()) {
		// TODO: use fast simple quadtree based direct mesh create
	} else 
    */
	{
		uint8_t planes_processed = 0;
		for (uint8_t i = 0; i < sizeof(g_6dirso) / sizeof(g_6dirso[0]); ++i) {
#if FARMESH_DEBUG
			if (i) {
				break;
			}
#endif
			if (!plane_processed[i].processed)
				continue;
			++planes_processed;
			async[i].step([this, i = i]() {
				//for (int depth = 0; depth < 100; ++depth) {
				plane_processed[i].processed = go_direction(i);
				//	if (!plane_processed[i].processed)
				//		break;
				//}
			});
		}
		planes_processed_last = planes_processed;

		if (planes_processed) {
			complete_set = false;
		} else if (!far_fast) {
			set_new_cam_pos();
		}
		if (m_camera_pos_aligned != camera_pos_aligned_int) {
			clientMap.m_far_blocks_last_cam_pos =
					far_fast ? camera_pos_aligned_int : m_camera_pos_aligned;
			if (far_fast) {
				set_new_cam_pos();
			}
		}
		if (!planes_processed && !complete_set) {
			clientMap.m_far_blocks_last_cam_pos = m_camera_pos_aligned;
			clientMap.m_far_blocks_use_timestamp = timestamp_complete;

// TODO: test correct times
#if FARMESH_DEBUG
			constexpr auto clean_old_time = 2;
#else
			constexpr auto clean_old_time = 30;
#endif
			if (timestamp_complete > clean_old_time)
				clientMap.m_far_blocks_clean_timestamp =
						timestamp_complete - clean_old_time;
			timestamp_complete = m_client->m_uptime;
			complete_set = true;
		}
		/*
			{
			auto &clientMap = m_client->getEnv().getClientMap();
			if (clientMap.m_far_blocks_use != clientMap.m_far_blocks_fill)
				clientMap.m_far_blocks_use = clientMap.m_far_blocks_currrent
													 ? &clientMap.m_far_blocks_1
													 : &clientMap.m_far_blocks_2;
			clientMap.m_far_blocks_fill = clientMap.m_far_blocks_currrent
												  ? &clientMap.m_far_blocks_2
												  : &clientMap.m_far_blocks_1;
			clientMap.m_far_blocks_currrent = !clientMap.m_far_blocks_currrent;
			clientMap.m_far_blocks_fill->clear();
			clientMap.m_far_blocks_created = m_client->m_uptime;
			//clientMap.far_blocks_sent_timer = 0;
		}
*/
		return planes_processed;
	}
}
