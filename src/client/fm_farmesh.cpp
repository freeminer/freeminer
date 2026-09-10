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
#include <initializer_list>
#include <utility>
#include <vector>

#include "fm_farmesh.h"

#include "client/client.h"
#include "client/clientmap.h"
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
	if (!step || step >= FARMESH_STEP_MAX || farmesh_thread_stop)
		return false;

	g_profiler->add("Client: Farmesh make", 1);
	auto &client_map = m_client->getEnv().getClientMap();
	const auto iteration = client_map.far_iteration_grid;
	MapBlockPtr block;
	{
		auto &storage = client_map.far_blocks_storage[step];
		const auto lock = storage.lock_unique_rec();
		auto [it, inserted] = storage.try_emplace(blockpos);
		auto &entry = it->second;
		entry.far_last_used = m_client->m_uptime;
		if (!entry.block) {
			entry.block = client_map.createBlankBlockNoInsert(blockpos);
			entry.block->far_step = step;
			entry.block->far_status = MapBlock::far_status_e::s1_created;
		}
		block = entry.block;
	}
	block->far_iteration = iteration;
	m_pending_far_blocks.insert_or_assign(blockpos, block);

	if (block->far_status < MapBlock::far_status_e::s2_requested) {
		// A received block can enter storage before its asynchronous invalidation
		// runs. Initialize its deadline too, instead of leaving UINT32_MAX here.
		block->far_make_mesh_timestamp =
				m_client->m_uptime + (m_fast_move ? 0 : farmesh_wait_server);
		for (pos_t x = 0; x < 1 << m_control->cell_size_pow; ++x)
			for (pos_t y = 0; y < 1 << m_control->cell_size_pow; ++y)
				for (pos_t z = 0; z < 1 << m_control->cell_size_pow; ++z)
					client_map.m_far_blocks_ask.insert_or_assign(
							blockpos + v3bpos_t{x, y, z} * (1 << step),
							std::make_pair(step, iteration));
		block->far_status = MapBlock::far_status_e::s2_requested;
	}
	return queueFarBlock(block, low_priority);
}

bool FarMesh::queueFarBlock(const MapBlockPtr &block, bool low_priority)
{
	const auto status = block->far_status.load();
	if (status != MapBlock::far_status_e::s2_requested &&
			status != MapBlock::far_status_e::s3_recieved)
		return false;
	if (!m_fast_move && m_client->m_uptime < block->far_make_mesh_timestamp)
		return false;
	return enqueueFarMeshForBlock(
			block->getPos(), block->far_step, block, m_client->m_uptime, low_priority);
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
		const auto valid_content = [](content_t content) {
			return content != CONTENT_IGNORE && content != CONTENT_UNKNOWN &&
				   content != CONTENT_AIR;
		};
		const auto node_id = [&](std::initializer_list<const char *> names,
									 content_t fallback) -> content_t {
			content_t content = CONTENT_IGNORE;
			for (const auto *name : names) {
				if (ndef->getId(name, content) && valid_content(content))
					return content;
			}
			return valid_content(fallback) ? fallback
										   : static_cast<content_t>(CONTENT_AIR);
		};

		mg->visible_surface = node_id({"mapgen_stone", "default:stone"}, CONTENT_AIR);
		mg->visible_water =
				node_id({"mapgen_water_source", "default:water_source"}, CONTENT_AIR);
		mg->visible_ice =
				node_id({"mapgen_ice", "default:ice"}, mg->visible_water.getContent());
		mg->visible_surface_green =
				node_id({"default:dirt_with_grass"}, mg->visible_surface.getContent());
		mg->visible_surface_dry = node_id(
				{"default:dirt_with_dry_grass", "default:dry_dirt_with_dry_grass"},
				mg->visible_surface_green.getContent());
		mg->visible_surface_cold =
				node_id({"mapgen_dirt_with_snow", "default:dirt_with_snow"},
						mg->visible_surface.getContent());
		mg->visible_surface_hot = node_id(
				{"default:sand", "mapgen_stone"}, mg->visible_surface.getContent());
		mg->visible_surface_rainforest = node_id({"default:dirt_with_rainforest_litter"},
				mg->visible_surface_green.getContent());
		mg->visible_surface_coniferous = node_id({"default:dirt_with_coniferous_litter"},
				mg->visible_surface_green.getContent());
		mg->visible_surface_tundra =
				node_id({"default:permafrost_with_moss", "default:permafrost"},
						mg->visible_surface_cold.getContent());
		mg->visible_surface_permafrost =
				node_id({"default:permafrost_with_stones", "default:permafrost"},
						mg->visible_surface_cold.getContent());
		mg->visible_surface_desert = node_id({"default:desert_sand", "default:sand"},
				mg->visible_surface_hot.getContent());
		mg->visible_surface_beach = node_id({"default:sand", "default:silver_sand"},
				mg->visible_surface_hot.getContent());
		mg->visible_surface_rock =
				node_id({"default:gravel", "mapgen_stone", "default:stone"},
						mg->visible_surface.getContent());
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

FarMesh::~FarMesh()
{
	g_settings->deregisterAllChangedCallbacks(this);
	farmesh_thread_stop = true;
	for (auto &a : async_direction)
		a.wait();
	async_cleaner.wait();
	if (farmesh_thread.joinable())
		farmesh_thread.join();
}

auto align_shift(auto pos, const auto amount)
{
	(pos.X >>= amount) <<= amount;
	(pos.Y >>= amount) <<= amount;
	(pos.Z >>= amount) <<= amount;
	return pos;
}

namespace
{

double heightLimitedFarRange(const v3pos_t &camera_pos, pos_t water_level, int max_range)
{
	const double height = std::max(0.0, double(camera_pos.Y) - water_level);
	return std::min(double(max_range), 5000.0 + height * 30.0);
}

bool outsideFarRange(const v3bpos_t &bpos, bpos_t size, uint8_t cell_size_pow,
		const v3pos_t &camera_pos, double range)
{
	// Keep cells overlapping the boundary, including their full mesh footprint.
	const double side = double(size) * (1u << cell_size_pow) * MAP_BLOCKSIZE;
	const double x = double(bpos.X) * MAP_BLOCKSIZE;
	const double z = double(bpos.Z) * MAP_BLOCKSIZE;
	const double dx = std::max({x - camera_pos.X, double(camera_pos.X) - x - side, 0.0});
	const double dz = std::max({z - camera_pos.Z, double(camera_pos.Z) - z - side, 0.0});
	return range <= 0.0 || radius_box(v3opos_t(dx, 0, dz), v3opos_t{}) > range;
}

} // namespace

int FarMesh::go_container(bool only_received, const block_step_t step_limit)
{
	const auto &draw_control = *m_control;
	// Do not limit 3d worlds
	// const auto &camera_pos = m_client->getEnv().getClientMap().far_cam_pos_grid;
	// const double far_range = heightLimitedFarRange(camera_pos, mg->water_level, draw_control.farmesh);
	const auto player_block_pos =
			getNodeBlockPos(m_client->getEnv().getClientMap().far_cam_pos_grid);

	size_t blocks_enqueued = 0;
	farmesh::runFarAll(player_block_pos, draw_control.cell_size_pow, draw_control.farmesh,
			draw_control.farmesh_quality_pow, 0, false, 0,
			[this, &step_limit, &only_received, &blocks_enqueued
					//, &draw_control ,&camera_pos, far_range
	](const v3bpos_t &bpos, const bpos_t &size, const block_step_t &step) -> bool {
				// if (outsideFarRange(bpos, size, draw_control.cell_size_pow, camera_pos, far_range))	return false;

				if (!step || step >= FARMESH_STEP_MAX) {
					return false;
				}

				// TODO: use block center
				if (step_limit && step > step_limit) {
					return false;
				}

				if (only_received) {
					auto &step_blocks =
							m_client->getEnv().getClientMap().far_blocks_storage[step];
					bool contains;
					{
						const auto lock = step_blocks.lock_shared_rec();
						const auto it = step_blocks.find(bpos);
						contains = it != step_blocks.end() && it->second.block;
					}

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
	const auto &camera_pos = m_client->getEnv().getClientMap().far_cam_pos_grid;
	const double far_range =
			heightLimitedFarRange(camera_pos, mg->water_level, draw_control.farmesh);
	const auto player_block_pos =
			getNodeBlockPos(m_client->getEnv().getClientMap().far_cam_pos_grid);
	constexpr bool cell_each = false;
	std::array<std::unordered_map<v3bpos_t, bool>, FARMESH_STEP_MAX> blocks;
	// Collect the full footprint once; waiting at each radius makes fast movement
	// continually outrun the outer terrain. All positions use the same grid.
	farmesh::runFarAll(player_block_pos, draw_control.cell_size_pow, draw_control.farmesh,
			draw_control.farmesh_quality_pow, 1, cell_each,
			farmesh::settingToStep(draw_control.farmesh),
			[this, &draw_control, &blocks, &player_block_pos, &camera_pos, far_range](
					const v3bpos_t &bpos, const bpos_t &size,
					const block_step_t &step) -> bool {
				if (outsideFarRange(bpos, size, draw_control.cell_size_pow, camera_pos,
							far_range))
					return false;

				const auto add_size = 1 << (step);
				int low_priority = 0;
				for (const auto &add : {
							 v3bpos_t{0, 0, 0},
							 v3bpos_t{0, static_cast<bpos_t>(add_size), 0},
							 v3bpos_t{0, static_cast<bpos_t>(-add_size), 0},
					 }) {
					v3bpos_t bpos_new{static_cast<bpos_t>(bpos.X + add.X), add.Y,
							static_cast<bpos_t>(bpos.Z + add.Z)};
					bpos_new.Y +=
							mg->getGroundLevelAtPoint(v2pos_t{
									static_cast<pos_t>((bpos_new.X << MAP_BLOCKP) - 1),
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
					auto [it, inserted] =
							blocks[step_new].emplace(bpos_correct, low_priority != 0);
					if (!inserted && !low_priority)
						it->second = false;
					++low_priority;
				}
				return farmesh_thread_stop.load();
			});
	for (size_t step = 1; step < blocks.size(); ++step)
		for (const auto &[bpos, low_priority] : blocks[step])
			makeFarBlock(bpos, step, low_priority);
	return 0;
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
						visible = mg->visible(pos_int, {}, step_aligned_pow) ||
								  mg->visible_water_level(pos_int);
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

void FarMesh::processFarmeshQueue()
{
	for (;;) {
		{
			const std::lock_guard lock(m_queue_mutex);
			m_mesh_jobs.poll([](const std::exception &e) {
				errorstream << "Far mesh job failed: " << e.what() << std::endl;
			});
			if (farmesh_thread_stop && m_mesh_jobs.empty())
				break;

			if (!farmesh_thread_stop && !m_queue_paused) {
				// Smaller steps cover terrain nearer the player. Submit each step's
				// surface and surrounding cells before submitting more distant steps,
				// so nearby replacement groups can become drawable sooner.
				for (size_t step = 1; step < FARMESH_STEP_MAX && !m_mesh_jobs.full();
						++step) {
					for (size_t priority = 0; priority < 2 && !m_mesh_jobs.full();
							++priority) {
						auto &queue =
								farmesh_make_queue[step + priority * FARMESH_STEP_MAX];
						for (auto it = queue.begin();
								it != queue.end() && !m_mesh_jobs.full();) {
							auto block = it->second.block;
							bool expected = false;
							if (!block->creating_far_mesh.compare_exchange_strong(
										expected, true)) {
								++it;
								continue;
							}
							m_mesh_jobs.add(m_client->mesh_thread_pool.enqueue_block(
									[this, block]() mutable {
										try {
											m_client->createFarMesh(block);
										} catch (...) {
											block->far_status =
													MapBlock::far_status_e::s2_requested;
											block->far_make_mesh_timestamp =
													m_client->m_uptime;
											block->creating_far_mesh = false;
											throw;
										}
										block->creating_far_mesh = false;
									}));
							it = queue.erase(it);
						}
					}
				}
			}
			farmesh_make_queue_complete =
					m_mesh_jobs.empty() &&
					std::all_of(farmesh_make_queue.begin(), farmesh_make_queue.end(),
							[](const auto &queue) { return queue.empty(); });
		}
		std::this_thread::sleep_for(std::chrono::milliseconds(10));
	}
}

bool FarMesh::enqueueFarMeshForBlock(const v3bpos_t &blockpos, const block_step_t step,
		const MapBlockPtr &block, const double timestamp, const bool low_priority)
{
	if (!block || !step || step >= FARMESH_STEP_MAX || farmesh_thread_stop)
		return false;
	const std::lock_guard lock(m_queue_mutex);
	if (m_queue_paused)
		return false;
	// Another scanner may have queued or dispatched this block since the caller
	// inspected its state. Only a new request or invalidation needs another job.
	const auto status = block->far_status.load();
	if (status != MapBlock::far_status_e::s2_requested &&
			status != MapBlock::far_status_e::s3_recieved)
		return false;
	if (farmesh_make_queue[step].contains(blockpos))
		return false;
	if (!low_priority)
		farmesh_make_queue[step + FARMESH_STEP_MAX].erase(blockpos);

	const auto [it, inserted] =
			farmesh_make_queue[step + FARMESH_STEP_MAX * low_priority].try_emplace(
					blockpos, BlockTodo{block, timestamp});
	if (inserted) {
		block->far_status = MapBlock::far_status_e::s4_mesh_enqueued;
		farmesh_make_queue_complete = false;
	}
	return inserted;
}

void FarMesh::commitFarGrid()
{
	// Near draw-list publication must not wait for the far-grid updater.
	// Keep the displayed grid for this frame and retry at the next update.
	const std::unique_lock grid_lock(m_grid_mutex, std::try_to_lock);
	if (!grid_lock.owns_lock() || !m_grid_scanned || m_grid_committed)
		return;
	auto &client_map = m_client->getEnv().getClientMap();
	const auto lock = client_map.m_far_blocks.lock_unique_rec();
	farmesh::publishReadyGrid(
			m_pending_far_blocks, client_map.m_far_blocks,
			[](const auto &block) { return block && block->getFarMesh(block->far_step); },
			[](const auto &block) { return block->far_step; }, m_control->cell_size_pow,
			[&](const auto &entry) {
				// Keep old terrain in the new near-only core. The draw-list handoff
				// hides it once all corresponding near chunks are available.
				const auto target = farmesh::getFarParams(*m_control,
						getNodeBlockPos(client_map.far_cam_pos_mesh), entry.first);
				return target && !target->step;
			});
	// Retained owners remain drawable and must not be evicted as stale data.
	for (const auto &[pos, block] : client_map.m_far_blocks)
		block->far_iteration = client_map.far_iteration_mesh;

	client_map.far_cam_pos_draw = client_map.far_cam_pos_mesh;
	client_map.far_iteration_draw = client_map.far_iteration_mesh;
	client_map.far_iteration_clean = client_map.far_iteration_mesh;
	m_grid_committed = m_grid_ready;
}

uint8_t FarMesh::update(
		v3opos_t camera_pos, v3pos_t camera_offset, int render_range, float speed)
{
	const std::lock_guard grid_lock(m_grid_mutex);
	if (!mg || farmesh_thread_stop)
		return true;
	auto &client_map = m_client->getEnv().getClientMap();
	const auto camera_pos_aligned = align_shift(floatToInt(camera_pos, BS), MAP_BLOCKP);
	const auto height_range = heightLimitedFarRange(
			camera_pos_aligned, mg->water_level, m_control->farmesh);
	const auto distance_max =
			(static_cast<unsigned int>(std::max(
					 0.0, std::min({double(render_range), 1.2 * m_client->fog_range / BS,
								  height_range}))) >>
					7)
			<< 7;
	m_fast_move = speed > 200 * BS ||
				  m_camera_pos_aligned.getDistanceFrom(camera_pos_aligned) > 1000;

	if (want_reset.exchange(false))
		m_grid_started = false;

	// Finish and publish one grid before accepting the latest camera position.
	// Constant movement must not cancel every replacement before it can appear.
	// Mesh jobs sample far_cam_pos_mesh, so it stays fixed until they all finish.
	if (!m_grid_started ||
			(m_grid_committed && (m_camera_pos_aligned != camera_pos_aligned ||
										 m_client->m_uptime >= m_next_refresh ||
										 last_distance_max != distance_max))) {
		if (!farmesh_make_queue_complete)
			return false;
		for (const auto &scan : async_direction)
			if (!scan.ready())
				return false;

		m_camera_pos_aligned = camera_pos_aligned;
		client_map.far_cam_pos_grid = client_map.far_cam_pos_mesh = camera_pos_aligned;
		client_map.far_iteration_grid = client_map.far_iteration_mesh =
				++far_iteration_pos;
		last_distance_max = distance_max;
		m_pending_far_blocks.clear();
		plane_processed.fill({});
		direction_caches.fill({});
		m_grid_started = true;
		m_grid_scanned = false;
		m_grid_ready = m_grid_committed = false;
		m_next_refresh = m_client->m_uptime + 1;
	}
	if (m_grid_committed)
		return true;
	if (m_grid_ready)
		return false; // The next draw-list build publishes the remaining groups.

	bool grid_finished = true;
	const bool flat = farmesh_flat && mg->surface_2d();
	const size_t directions = !flat && farmesh_ray ? async_direction.size() : 1;
	for (size_t i = 0; i < directions; ++i) {
		// A ready future synchronizes reads of the scanner's progress and caches.
		if (!async_direction[i].ready()) {
			grid_finished = false;
			continue;
		}
		if (!plane_processed[i].processed)
			continue;
		grid_finished = false;
		async_direction[i].step([this, i, flat]() {
			if (flat) {
				go_flat();
				go_container(true, farmesh::settingToStep(
										   g_settings->getU32("farmesh_all_changed")));
				plane_processed[i].processed = 0;
			} else if (farmesh_ray) {
				plane_processed[i].processed = go_direction(i);
			} else {
				go_container(false);
				plane_processed[i].processed = 0;
			}
		});
	}
	if (!grid_finished)
		return false;
	m_grid_scanned = true;

	bool meshes_ready = true;
	for (const auto &[pos, block] : m_pending_far_blocks) {
		// Also retries failed jobs and revisits blocks after their server wait.
		queueFarBlock(block);
		if (!block->getFarMesh(block->far_step))
			meshes_ready = false;
	}
	// Refresh jobs may keep arriving for cells that already have usable meshes.
	// They must not prevent this grid from appearing. Changing the mesh origin
	// still waits for our queue and in-flight jobs at the start of update().
	m_grid_ready = meshes_ready;

#if FARMESH_CLEAN
	const auto now = m_client->m_uptime.load();
	if (m_grid_ready && now > async_cleaner_next) {
		const auto timeout =
				g_settings->getFloat("client_unload_unused_data_timeout") * 2;
		async_cleaner_next = now + timeout;
		const auto clean_iteration = client_map.far_iteration_clean;
		async_cleaner.step([this, timeout, clean_iteration]() {
			auto &client_map = m_client->getEnv().getClientMap();
			for (auto &storage : client_map.far_blocks_storage) {
				const auto lock = storage.try_lock_unique_rec();
				if (!lock->owns_lock())
					continue;
				for (auto &[pos, entry] : storage) {
					if (entry.block && entry.block->far_iteration < clean_iteration &&
							entry.far_last_used &&
							m_client->m_uptime > entry.far_last_used + timeout) {
						entry.far_last_used = 0;
						entry.block.reset();
					}
				}
			}
		});
	}
#endif
	return false;
}

void FarMesh::restart()
{
	m_client->farmesh_async.wait();
	const std::lock_guard grid_lock(m_grid_mutex);
	for (auto &scan : async_direction)
		scan.wait();
	async_cleaner.wait();
	{
		const std::lock_guard lock(m_queue_mutex);
		m_queue_paused = true;
		for (auto &queue : farmesh_make_queue) {
			for (auto &[pos, todo] : queue)
				if (todo.block->far_status == MapBlock::far_status_e::s4_mesh_enqueued)
					todo.block->far_status = MapBlock::far_status_e::s2_requested;
			queue.clear();
		}
	}
	// The queue thread drains our submitted futures even while dispatch is paused.
	while (!farmesh_make_queue_complete && !farmesh_thread_stop)
		std::this_thread::sleep_for(std::chrono::milliseconds(10));
	{
		const std::lock_guard lock(m_queue_mutex);
		m_queue_paused = false;
	}
	m_grid_scanned = m_grid_ready = false;
	want_reset = true;
}
