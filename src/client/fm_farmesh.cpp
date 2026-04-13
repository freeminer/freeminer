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

#include <array>
#include <cstddef>
#include <cstdint>
#include <utility>
#include <vector>

#include "fm_farmesh.h"

#include "client/client.h"
#include "client/clientmap.h"
#include "debug/dump.h"
#include "fm_far_calc.h"
#include "client/mapblock_mesh.h"
#include "constants.h"
#include "emerge.h"
#include "irr_v3d.h"
#include "irrlichttypes.h"
#include "mapblock.h"
#include "mapgen/mapgen.h"
#include "mapnode.h"
#include "profiler.h"
#include "server.h"
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

bool FarMesh::makeFarBlock(
		const v3bpos_t &blockpos, block_step_t step, const bool low_priority)
{
	g_profiler->add("Client: Farmesh make", 1);

	auto &client_map = m_client->getEnv().getClientMap();
	//const auto &draw_control = client_map.getControl();
	const auto &draw_control = *m_control;
	const auto &blockpos_actual = blockpos;
	auto &far_blocks = //near ? m_client->getEnv().getClientMap().m_far_near_blocks :
			client_map.m_far_blocks;

	const auto far_iteration_use = client_map.far_iteration_grid;

	MapBlockPtr block;
#if 0
	{
		const auto lock = far_blocks.lock_unique_rec();
		if (const auto &it = far_blocks.find(blockpos_actual); it != far_blocks.end()) {
			if (it->second->far_step == step) {
				block = it->second;
			}
			// if (block->far_status > MapBlock::far_status_e::s2_requested) {
			// 	return;
			// }
		}
	}
#endif
	{
		{
			auto &far_blocks_storage_step = client_map.far_blocks_storage[step];

			const auto lock = far_blocks_storage_step.lock_unique_rec();

			if (const auto it = far_blocks_storage_step.find(blockpos_actual);
					it != far_blocks_storage_step.end() && it->second.block) {
				block = it->second.block;
				it->second.far_last_used = m_client->m_uptime;
				{
					//far_blocks.insert_or_assign(blockpos_actual, block);
				}
			}

			if (!block) {
				block = client_map.createBlankBlockNoInsert(blockpos_actual);
				block->far_step = step;
				block->far_status = MapBlock::far_status_e::s1_created;
				collect_reset_timestamp =
						m_client->m_uptime + (farmesh_wait_server ?: 1) * step;
				block->far_make_mesh_timestamp =
						farmesh_wait_server ? collect_reset_timestamp : 0;

				far_blocks_storage_step.insert_or_assign(blockpos_actual,
						Map::BlockUsed{block, (int32_t)m_client->m_uptime});
				//far_blocks.insert_or_assign(blockpos_actual, block);
			}
		}

		// if (block->far_status >= MapBlock::far_status_e::s6_mesh_complete)
		{
			// Check if old block exists and old step + 1 == new step
			MapBlockPtr old_block;
			{
				const auto lock = far_blocks.lock_shared_rec();
				if (const auto &it = far_blocks.find(blockpos_actual);
						it != far_blocks.end()) {
					old_block = it->second;
				}
			}

			if (old_block && old_block->far_step + 1 == step) {
				// Find other 7 old blocks filling new block volume
				// Make these 7 blocks not renderable by setting their far_iteration to 0
				const bpos_t blocks_per_side = 2; // 2x2x2 = 8 blocks total
				const bpos_t step_shift =
						1 << (step - 1 +
								draw_control
										.cell_size_pow); // Calculate shift based on step and cell size
				for (bpos_t x = 0; x < blocks_per_side; ++x) {
					for (bpos_t y = 0; y < blocks_per_side; ++y) {
						for (bpos_t z = 0; z < blocks_per_side; ++z) {
							if (x == 0 && y == 0 && z == 0)
								continue; // Skip the main block

							// Calculate block positions using shifting size to step as around
							v3bpos_t sub_block_pos =
									blockpos_actual +
									v3bpos_t{static_cast<bpos_t>(x * step_shift),
											static_cast<bpos_t>(y * step_shift),
											static_cast<bpos_t>(z * step_shift)};
							const auto lock = far_blocks.lock_shared_rec();
							if (const auto &it = far_blocks.find(sub_block_pos);
									it != far_blocks.end()) {
								if (it->second) {
									it->second->far_iteration = 0; // Make non-renderable
								}
							}
						}
					}
				}
			}
		}

		far_blocks.insert_or_assign(blockpos_actual, block);
	}

	block->far_iteration = far_iteration_use;

	if (block->far_status < MapBlock::far_status_e::s2_requested) {
		for (pos_t x = 0; x < 1 << draw_control.cell_size_pow; ++x) {
			for (pos_t y = 0; y < 1 << draw_control.cell_size_pow; ++y) {
				for (pos_t z = 0; z < 1 << draw_control.cell_size_pow; ++z) {
					client_map.m_far_blocks_ask.emplace(
							blockpos_actual + v3bpos_t{x, y, z} * (1 << step),
							std::make_pair(step, far_iteration_use));
				}
			}
		}
		block->far_status = MapBlock::far_status_e::s2_requested;
	}

	// Make mesh for blocks without data
	if ((block->far_status >= MapBlock::far_status_e::s2_requested &&
				block->far_status <= MapBlock::far_status_e::s4_mesh_enqueued &&
				m_client->m_uptime >= block->far_make_mesh_timestamp)) {
		const auto size = 1 << (step + draw_control.cell_size_pow);
		return enqueueFarMeshForBlock(
				blockpos_actual, step, block, m_client->m_uptime, low_priority);
	} else if (m_client->m_uptime >= block->far_make_mesh_timestamp) {
		block->far_status = MapBlock::far_status_e::
				s2_requested; // BUG! removeme, status should be always sync with far_make_mesh_timestamp
		collect_reset_timestamp =
				std::min(collect_reset_timestamp, block->far_make_mesh_timestamp);
	} else {
	}
	return false;
}

size_t FarMesh::makeFarBlocks(const v3bpos_t &blockpos, const block_step_t step)
{
	const auto &control = *m_control;
#if FARMESH_DEBUG || FARMESH_FAST
	{
		const auto tree_result = farmesh::getFarParams(
				control, getNodeBlockPos(m_camera_pos_aligned), blockpos);
		if (!tree_result) {
			return 0;
		}
		const auto &block_step_correct = tree_result->step;
		if (!block_step_correct)
			return 0;
		const v3bpos_t &bpos = tree_result->pos;
		return makeFarBlock(bpos, block_step_correct /*, {}, bpos*/);
	}
#endif

	// TODO: fix finding correct near blocks respecting their steps and enable:

	//const static auto pfar = std::vector<v3pos_t>{
	//		v3pos_t(0, 0, 0), // self
	//};
	const static auto pnear = std::vector<v3pos_t>{
			v3pos_t(0, 0, 0),  // self
			v3pos_t(0, 0, 1),  // back
			v3pos_t(1, 0, 0),  // right
			v3pos_t(0, 0, -1), // front
			v3pos_t(-1, 0, 0), // left
			v3pos_t(0, 1, 0),  // top
			v3pos_t(0, -1, 0), // bottom
	};
	const auto &use_dirs = pnear;
	const auto step_width = 1 << (step - 1 + control.cell_size_pow);
	int low_priority = 0;
	size_t res = 0;
	for (const auto &dir : use_dirs) {
		const auto bpos_dir = blockpos + dir * step_width;
		const auto tree_result = farmesh::getFarParams(
				control, getNodeBlockPos(m_camera_pos_aligned), bpos_dir);
		if (!tree_result) {
			continue;
		}
		const auto &block_step_correct = tree_result->step;
		if (!block_step_correct) {
			continue;
		}
		const v3bpos_t &bpos = tree_result->pos;
		res += makeFarBlock(bpos, block_step_correct, low_priority++);
	}
	return res;
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

static const std::string FarMesh_settings[] = {
		"farmesh",			   // MapDrawControl
		"lodmesh",			   // MapDrawControl
		"farmesh_quality",	   // MapDrawControl
		"farmesh_stable",	   // MapDrawControl
		"farmesh_all_changed", // MapDrawControl
		"client_mesh_chunk",   // ClientMap
		"farmesh_flat",		   // Farmesh
		"farmesh_ray",		   // Farmesh
		"farmesh_wait_server"  // Farmesh
};

void FarMesh::onSettingChanged(const std::string &name)
{
	restart();
	if (name == "farmesh_flat") {
		g_settings->getBoolNoEx("farmesh_flat", farmesh_flat);
	} else if (name == "farmesh_ray") {
		g_settings->getBoolNoEx("farmesh_ray", farmesh_ray);
	} else if (name == "farmesh_wait_server") {
		g_settings->getU16NoEx("farmesh_wait_server", farmesh_wait_server);
	}

	m_client->getEnv().getClientMap().getControl().onSettingChanged(name);
	m_client->onSettingChanged(name);

	restart();
}

FarMesh::FarMesh(Client *client, Server *server) :
		m_client{client}, m_control{&m_client->getEnv().getClientMap().getControl()}
{

	EmergeManager *emerge_use = server			   ? server->getEmergeManager()
								: client->m_emerge ? client->m_emerge.get()
												   : nullptr;

	if (!emerge_use) {
		// Non freeminer server without mapgen params
		Settings settings;
		MapgenType mgtype = FARMESH_DEFAULT_MAPGEN;
		settings.set("mg_name", Mapgen::getMapgenName(mgtype));
		m_client->MakeEmerge(settings, mgtype);
		emerge_use = m_client->m_emerge.get();
		m_client->far_container.use_weather = false;
	}

	if (emerge_use) {
		if (emerge_use->mgparams) {
			mg = emerge_use->getFirstMapgen();
		}

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

	g_settings->getBoolNoEx("farmesh_flat", farmesh_flat);
	g_settings->getBoolNoEx("farmesh_ray", farmesh_ray);
	g_settings->getU16NoEx("farmesh_wait_server", farmesh_wait_server);

	//for (size_t i = 0; i < process_order.size(); ++i)
	//	process_order[i] = i;
	//auto rng = std::default_random_engine{};
	//std::shuffle(std::begin(process_order), std::end(process_order), rng);
	farmesh_thread = std::thread(&FarMesh::processFarmeshQueue, this);

	for (const auto &name : FarMesh_settings) {
		g_settings->registerChangedCallback(
				name,
				[](const std::string &name, void *data) {
					static_cast<FarMesh *>(data)->onSettingChanged(name);
				},
				this);
	}
}

void FarMesh::processFarmeshQueue()
{
	while (!farmesh_thread_stop) {
		m_client->mesh_thread_pool.wait_until_empty();
		size_t processed = 0;
		block_step_t step = 0;
		{
			TimeTaker time("Client: Farmesh mesh [ms]");

			for (auto &one_step : farmesh_make_queue) {
				//size_t processed_in_step = 0;
				bool next = true;
				{
					const auto lock = one_step.try_lock_unique_rec();
					if (lock->owns_lock()) {
						for (auto it = one_step.begin(); it != one_step.end();) {
							m_client->mesh_thread_pool.enqueue(
									[this, block = it->second.block]() mutable {
										m_client->createFarMesh(block);
									});

							it = one_step.erase(it);
							--farmesh_make_queue_size;
							//++processed_in_step;
							++processed;
							++farmesh_make_queue_processed;
							if (processed * (1 << (3 * m_control->cell_size_pow)) > 500) {
								next = false;
								break;
							}
						}
					}
				}
				if (!next)
					break;

				++step;
			}
		}
		if (processed) {
			farmesh_make_queue_complete = false;
		} else {
			if (farmesh_make_queue_processed) {
				farmesh_make_queue_complete = true;
			} else {
				++farmesh_make_queue_processed;
			}
			std::this_thread::sleep_for(std::chrono::milliseconds(100));
		}
	}
}

FarMesh::~FarMesh()
{
	g_settings->deregisterAllChangedCallbacks(this);
	farmesh_thread_stop = true;
	if (farmesh_thread.joinable()) {
		farmesh_thread.join();
	}
}

auto align_shift(auto pos, const auto amount)
{
	(pos.X >>= amount) <<= amount;
	(pos.Y >>= amount) <<= amount;
	(pos.Z >>= amount) <<= amount;
	return pos;
}

int FarMesh::go_container(bool only_received, const block_step_t step_limit)
{
	const auto &draw_control = *m_control;
	const auto player_block_pos =
			getNodeBlockPos(m_client->getEnv().getClientMap().far_cam_pos_grid);

	size_t blocks_enqueued = 0;
	farmesh::runFarAll(player_block_pos, draw_control.cell_size_pow, draw_control.farmesh,
			draw_control.farmesh_quality_pow, 0, false, 0,
			[this, &step_limit, &only_received, &blocks_enqueued](const v3bpos_t &bpos,
					const bpos_t &size, const block_step_t &step) -> bool {
				if (step >= FARMESH_STEP_MAX) {
					return false;
				}

				// TODO: use block center
				if (step_limit && step > step_limit) {
					return false;
				}

				if (only_received) {
					auto &step_blocks =
							m_client->getEnv().getClientMap().far_blocks_storage[step];
					const auto it = step_blocks.find(bpos);
					const auto contains = it != step_blocks.end() && it->second.block;

					if (contains) {
						blocks_enqueued += makeFarBlock(bpos, step);
					}
				} else {
					blocks_enqueued += makeFarBlock(bpos, step);
				}

				return false;
			});
	return blocks_enqueued;
}

int FarMesh::go_flat()
{
	const auto &draw_control = *m_control;

	auto &dcache = direction_caches[0][0];
	auto &last_step = dcache.step_num;
	// todo: slowly increase range here
	const auto max_step = farmesh::settingToStep(draw_control.farmesh);
	for (; last_step < max_step;) {
		if (!farmesh_make_queue_complete) {
			return last_step ?: 1;
		}
		farmesh_make_queue_complete = false;
		++last_step;
		const auto player_block_pos =
				getNodeBlockPos(m_client->getEnv().getClientMap().far_cam_pos_grid);
		constexpr bool cell_each = false;
		// todo: maybe save blocks while cam pos not changed
		std::array<std::unordered_map<v3bpos_t, bool>, FARMESH_STEP_MAX> blocks;
		farmesh::runFarAll(player_block_pos, draw_control.cell_size_pow,
				draw_control.farmesh, draw_control.farmesh_quality_pow, 1, cell_each,
				last_step,
				[this, &draw_control, &blocks, &player_block_pos, &max_step](
						const v3bpos_t &bpos, const bpos_t &size,
						const block_step_t &step) -> bool {
#if 0 // test only
				{
					v3bpos_t bpos_new{bpos.X, 0, bpos.Z};
					bpos_new.Y = mg->getGroundLevelAtPoint(
										 v2pos_t((bpos_new.X << MAP_BLOCKP) - 1,
												 (bpos_new.Z << MAP_BLOCKP) - 1)) >>
								 MAP_BLOCKP;

					const auto res = farmesh::getFarParams(
							draw_control, player_block_pos, bpos_new);
					if (!res)
						return false;
					const auto &step_new = res->step;
					const auto &bpos_new_correct = res->pos;
					if (step_new >= FARMESH_STEP_MAX)
						return false;

					blocks[step_new].emplace(bpos_new_correct);
					return false;
				}
#endif
					const auto add_size = 1 << (step);
					int low_priority = 0;
					for (const auto &add : {
								 v3bpos_t{0, 0, 0},
								 v3bpos_t{0, static_cast<bpos_t>(add_size), 0},
								 v3bpos_t{0, static_cast<bpos_t>(-add_size), 0},
						 }) {
						v3bpos_t bpos_new{static_cast<bpos_t>(bpos.X + add.X), add.Y,
								static_cast<bpos_t>(bpos.Z + add.Z)};
						bpos_new.Y += mg->getGroundLevelAtPoint(v2pos_t{
											  static_cast<pos_t>(
													  (bpos_new.X << MAP_BLOCKP) - 1),
											  static_cast<pos_t>(
													  (bpos_new.Z << MAP_BLOCKP) - 1)}) >>
									  MAP_BLOCKP;
						const auto res = farmesh::getFarParams(
								draw_control, player_block_pos, bpos_new, cell_each);
						if (!res) {
							continue;
						}

						const auto &bpos_correct = res->pos;
						const auto &step_new = res->step;

						if (step_new >= FARMESH_STEP_MAX)
							continue;
						blocks[step_new].emplace(bpos_correct, low_priority++);
					}
					return false;
				});
		size_t blocks_enqueued = 0;
		size_t blocks_collected = 0;
		for (size_t step = 1; step < blocks.size(); ++step) {
			for (const auto &[bpos, low_priority] : blocks[step]) {
				++blocks_collected;
				blocks_enqueued += makeFarBlock(bpos, step, low_priority);
			}
		}
		if (blocks_enqueued) {
			return last_step;
		}
	}
	return 0; //last_step != max_step;
}

int FarMesh::go_direction(const size_t dir_n)
{
	TimeTaker time("Client: Farmesh [ms]");
	time.start();

	constexpr auto block_step_reduce = 1;
	constexpr auto align_reduce = 1;

	auto &cache = direction_caches[dir_n];
	auto &mg_cache = mg_caches[dir_n];

	const auto &draw_control = *m_control;

	const auto dir = g_6dirso[dir_n];
	const auto grid_size_xy = grid_size_x * grid_size_y;

	const auto &far_cam_pos_grid = m_client->getEnv().getClientMap().far_cam_pos_grid;
	const auto camera_pos = intToFloat(far_cam_pos_grid, BS);
	int processed = 0;
	size_t blocks_enqueued = 0;
	for (uint16_t i = 0; i < grid_size_xy; ++i) {
		auto &ray_cache = cache[i];
		if (ray_cache.finished > last_distance_max) {
			continue;
		}
		//uint16_t y = uint16_t(process_order[i] / grid_size_x);
		//uint16_t x = process_order[i] % grid_size_x;
		const uint16_t y = uint16_t(i / grid_size_x);
		const uint16_t x = i % grid_size_x;

		auto dir_first = dir * distance_min / 2;
		const auto pos_center = dir_first + camera_pos;

		if (!dir.X)
			dir_first.X += distance_min / grid_size_x * (x - grid_size_x / 2);
		if (!dir.Y)
			dir_first.Y += distance_min / grid_size_x * (y - grid_size_x / 2);
		if (!dir.Z)
			dir_first.Z +=
					distance_min / grid_size_x * ((!dir.Y ? x : y) - grid_size_x / 2);

		const auto dir_l = dir_first.normalize();

		auto pos_last = dir_l * ray_cache.finished * BS + pos_center;
		++ray_cache.step_num;
		for (size_t steps = 0; steps < 200; ++ray_cache.step_num, ++steps) {
#if !NDEBUG
			g_profiler->avg("Client: Farmesh processed", 1);
#endif
			//const auto dstep = ray_cache.step_num; // + 1;
			auto block_step_prev = farmesh::getFarStepBad(draw_control,
					getNodeBlockPos(far_cam_pos_grid),
					getNodeBlockPos(floatToInt(pos_last, BS)));

			const auto step_width_shift = (block_step_prev - block_step_reduce);
			const auto step_width = MAP_BLOCKSIZE
									<< (step_width_shift > 0 ? step_width_shift : 0);
			const auto &depth = ray_cache.finished;

			//if (depth > last_distance_max) {
			//ray_cache.finished = distance_min + step_width;// * (dstep - 1);
			//break;
			//}

			const auto pos = dir_l * depth * BS + camera_pos;
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

			const int step_aligned_pow =
					farmesh::rangeToStep(step_width) - align_reduce; // ceil ?
			const auto pos_int = align_shift(
					floatToInt(pos, BS), step_aligned_pow > 0 ? step_aligned_pow : 0);

			if (radius_box(pos_int, far_cam_pos_grid) > last_distance_max) {
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
							pow(2, block_step + rangeToStep(draw_control.cell_size);
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
					blocks_enqueued +=
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
	if (!mg) {
		return {};
	}
	auto &client_map = m_client->getEnv().getClientMap();

	if (want_reset) {
		want_reset = false;

		far_iteration_pos = client_map.far_iteration_grid =
				client_map.far_iteration_mesh = client_map.far_iteration_draw =
						client_map.far_iteration_clean = 0;
	}

	m_speed = speed;

	const auto camera_pos_aligned_int =
			align_shift(floatToInt(camera_pos, BS), MAP_BLOCKP); // not aligned
	const auto distance_max =
			(std::min<unsigned int>(render_range, 1.2 * m_client->fog_range / BS) >> 7)
			<< 7;

	const auto far_old =
			far_iteration_pos_time + m_control->farmesh_stable < m_client->m_uptime;
	const auto far_fast =
			far_old &&
			(
					//m_client->getEnv().getClientMap().m_far_fast &&
					m_speed > 200 * BS ||
					m_camera_pos_aligned.getDistanceFrom(camera_pos_aligned_int) > 1000);

	const auto set_new_mesh_pos = [&]() {
		for (auto &stepit : farmesh_make_queue) {
			stepit.clear();
		}
		farmesh_make_queue_size = 0;
		farmesh_make_queue_processed = 0;
		farmesh_make_queue_complete = false;
		auto &client_map = m_client->getEnv().getClientMap();
		client_map.m_far_blocks_ask.clear();
		client_map.far_iteration_mesh = client_map.far_iteration_grid;
		client_map.far_cam_pos_mesh = client_map.far_cam_pos_grid;
	};
	bool grid_finished{};
	const auto set_new_grid_pos = [&]() {
		client_map.far_iteration_grid = far_iteration_pos;
		client_map.far_cam_pos_grid = m_camera_pos_aligned;
		collect_reset_timestamp = -1;
		plane_processed.fill({});
		direction_caches.fill({});
		grid_finished = false;
	};

	const auto set_new_cam_pos = [&]() {
		if (m_camera_pos_aligned == camera_pos_aligned_int) {
			return false;
		}

		++far_iteration_pos;
		far_iteration_pos_time = m_client->m_uptime;

		m_camera_pos_aligned = camera_pos_aligned_int;
		return true;
	};

	if (!far_iteration_pos) {
		++far_iteration_pos;
		set_new_cam_pos();
		set_new_grid_pos();
		set_new_mesh_pos();
	}
	const auto mesh_complete_set =
			client_map.far_iteration_draw == client_map.far_iteration_mesh;
	if (mesh_complete_set && m_client->m_uptime > collect_reset_timestamp) {
		set_new_grid_pos();
	}

	{
		thread_local static const s16 farmesh_all_changed =
				g_settings->getU32("farmesh_all_changed");

		uint8_t planes_processed{};
		if (farmesh_flat && mg->surface_2d()) {
			// For 2d mapgens only: use simple 2d mesh grid
			if (plane_processed[0].processed) {
				++planes_processed;
				async_direction[0].step([this]() {
					plane_processed[0].processed = go_flat();
					if (!plane_processed[0].processed) {
						go_container(true, farmesh::settingToStep(farmesh_all_changed));
					}
				});
			} else {
				grid_finished = true;
			}
		} else if (farmesh_ray) {
			// Try find surface via raytrace
			if (mesh_complete_set) {
				if (last_distance_max < distance_max) {
					plane_processed.fill({});
					last_distance_max = distance_max; // * 1.1;
				}
			}

			for (uint8_t i = 0; i < sizeof(g_6dirso) / sizeof(g_6dirso[0]); ++i) {
#if FARMESH_DEBUG
				if (i) {
					break;
				}
#endif
				if (!plane_processed[i].processed) {
					continue;
				}

				++planes_processed;
				async_direction[i].step([this, i = i]() {
					plane_processed[i].processed = go_direction(i);
				});
			}
			grid_finished = !planes_processed;
		} else {
			// Use 3d full grid (will try make mesh for whole volume including not visible top air and bottom undergrounds)
			if (plane_processed[0].processed) {
				++planes_processed;
				async_direction[0].step(
						[this]() { plane_processed[0].processed = go_container(false); });
			} else {
				grid_finished = true;
			}
		}
		grid_finished = !planes_processed;

		bool cam_pos_updated{};
		if (far_old || (mesh_complete_set && farmesh_make_queue_complete)) {
			cam_pos_updated = set_new_cam_pos();
			if (cam_pos_updated) {
				set_new_grid_pos();
				set_new_mesh_pos();
			}
		}
		if (grid_finished &&
				client_map.far_iteration_draw != client_map.far_iteration_mesh &&
				farmesh_make_queue_complete) {
			client_map.far_iteration_draw = client_map.far_iteration_mesh;
			client_map.far_cam_pos_draw = client_map.far_cam_pos_mesh;
			client_map.far_iteration_clean = client_map.far_iteration_mesh; // - 1;
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

#if FARMESH_CLEAN
		if (mesh_complete_set) {
			const auto now = m_client->m_uptime.load(); //porting::getTimeMs();
			if (now > async_cleaner_next) {
				thread_local static const auto client_unload_unused_data_timeout =
						g_settings->getFloat("client_unload_unused_data_timeout") * 2;
				async_cleaner_next = now + client_unload_unused_data_timeout;
				async_cleaner.step([this]() {
					auto &client_map = m_client->getEnv().getClientMap();
					//const auto &far_blocks = client_map.m_far_blocks;
					//block_step_t step = 0;
					for (auto &blocks_step : client_map.far_blocks_storage) {
						//std::vector<v3pos_t> del;
						{
							const auto lock = blocks_step.try_lock_shared_rec();
							if (!lock->owns_lock()) {
								continue;
							}
							for (auto &block_used : blocks_step) {
								auto &block = block_used.second.block;
								if (!block) {
									continue;
								}
								if (block->far_step >= client_map.far_iteration_clean) {
									continue;
								}
								if (block_used.second.far_last_used &&
										m_client->m_uptime >
												block_used.second.far_last_used +
														client_unload_unused_data_timeout) {
									block_used.second.far_last_used = 0;
									block.reset();
								}
							}
						}
						/*
						if (const auto sz = del.size(); sz) {
							infostream << "Deleting old far blocks step=" << step << " "
									   << sz << " / " << bs.size() << "\n";
							const auto lock = bs.lock_unique_rec();
							for (const auto &pos : del) {
								bs.erase(pos);
							}
						}*/
						//++step;
					}
				});
			}
		}
#endif

		return grid_finished && mesh_complete_set;
	}
}

bool FarMesh::enqueueFarMeshForBlock(const v3bpos_t &blockpos, const block_step_t step,
		const MapBlockPtr &block, const double timestamp, const bool low_priority)
{
	if (low_priority && farmesh_make_queue[step].contains(blockpos)) {
		return false;
	}

	block->far_status = MapBlock::far_status_e::s4_mesh_enqueued;

	farmesh_make_queue_complete = false;
	const auto &[_, inserted] =
			farmesh_make_queue[step + FARMESH_STEP_MAX * low_priority].insert_or_assign(
					blockpos, BlockTodo{block, timestamp});
	farmesh_make_queue_size += inserted;
	return true;
}

void FarMesh::restart()
{
	want_reset = true;
	m_client->farmesh_async.wait();
	for (auto &a : async_direction) {
		a.wait();
	}

	for (auto &stepit : farmesh_make_queue) {
		stepit.clear();
	}
	farmesh_make_queue_size = 0;
	farmesh_make_queue_complete = 1;
	m_client->mesh_thread_pool.wait_until_empty();
}
