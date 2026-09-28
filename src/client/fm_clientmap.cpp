/*
Copyright (C) 2026 proller <proler@gmail.com>
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

#include "client/fm_farmesh.h"
#include "client/fm_far_draw.h"
#include "client/fm_far_mesh_clip.h"
#include "client/fm_mesh_priority.h"
#include "client/mapblock_mesh.h"
#include "client/node_visuals.h"
#include "clientmap.h"

#include "client.h"
#include "fm_far_calc.h"
#include "mapgen/mapgen.h"
#include "mapblock.h"
#include "profiler.h"
#include "settings.h"
#include "util/numeric.h"

#include <IVideoDriver.h>
#include <matrix4.h>

#include <algorithm>
#include <atomic>
#include <cmath>
#include <cstdint>
#include <unordered_map>

irr_ptr<ClientMap> ClientMap::create(Client *client, RenderingEngine *rendering_engine,
		MapDrawControl &control, s32 id)
{
	return irr_ptr<ClientMap>(new ClientMap(client, rendering_engine, control, id));
}

void ClientMap::updateDrawListFm(float dtime, unsigned int max_cycle_ms)
{
	// fm: Switch the far grid before calculating this frame's near/far ownership.
	if (m_client->farmesh)
		m_client->farmesh->commitFarGrid();
	// ===
	ScopeProfiler sp(g_profiler, "CM::updateDrawList()", SPT_AVG);
	TimeTaker timer_step("ClientMap::updateDrawList");

	drawlist_map drawlist{MapBlockComparer(getNodeBlockPos(m_camera_position_node))};

	//auto is_frustum_culled = m_client->getCamera()->getFrustumCuller();

	//v3f camera_position = m_camera_position;
	//f32 camera_fov = m_camera_fov;

	// Use a higher fov to accomodate faster camera movements.
	// Blocks are cropped better when they are drawn.
	// Or maybe they aren't? Well whatever.
	//camera_fov *= 1.2;

	//v3s16 cam_pos_nodes = floatToInt(camera_position, BS);
	//v3pos_t cam_pos_nodes = m_camera_position_node;
	/*
	v3pos_t p_blocks_min;
	v3pos_t p_blocks_max;
	getBlocksInViewRange(cam_pos_nodes, &p_blocks_min, &p_blocks_max);
*/

	// Number of blocks currently loaded by the client
	//u32 blocks_loaded = 0;
	// Number of blocks with mesh in rendering range
	u32 blocks_in_range_with_mesh = 0;
	// Number of blocks occlusion culled
	u32 blocks_occlusion_culled = 0;

	// Number of blocks in rendering range
	u32 blocks_in_range = 0;
	// Number of blocks in rendering range but don't have a mesh
	u32 blocks_in_range_without_mesh = 0;
	// Blocks that were drawn and had a mesh
	//u32 blocks_drawn = 0;
	// Blocks which had a corresponding meshbuffer for this pass
	//u32 blocks_had_pass_meshbuf = 0;
	// Blocks from which stuff was actually drawn
	//u32 blocks_without_stuff = 0;
	// Distance to farthest drawn block
	float farthest_drawn = 0;
	int m_mesh_queued = 0;

	//bool free_move = g_settings->getBool("free_move");

	const auto wanted_range = m_control.wanted_range.load(std::memory_order_relaxed);

	const auto speedf = m_client->getEnv().getLocalPlayerSpeedLength();

	const int maxq = 1000;

	// Number of blocks frustum culled
	//u32 blocks_frustum_culled = 0;

	MeshGrid mesh_grid = m_client->getMeshGrid();
	// No occlusion culling when free_move is on and camera is inside ground
	// No occlusion culling for chunk sizes of 4 and above
	//   because the current occlusion culling test is highly inefficient at these sizes
	bool occlusion_culling_enabled = mesh_grid.cell_size < 4;

	if (m_control.allow_noclip) {
		MapNode n = getNode(m_camera_position_node);
		if (n.getContent() == CONTENT_IGNORE || m_nodedef->get(n).visuals->solidness == 2)
			occlusion_culling_enabled = false;
	}

	auto far_draw = std::make_shared<farmesh::FarDrawState>();
	std::shared_ptr<const farmesh::FarDrawState> previous_far_draw;
	{
		std::lock_guard<std::recursive_mutex> lock(m_drawlist_mutex);
		previous_far_draw = m_far_draw_state;
	}

	unordered_map_v3pos<bool> occlude_cache;

	std::vector<std::pair<v3bpos_t, MapBlockPtr>> vector;
	{
		const auto lock = m_blocks.lock_shared_rec();
		vector.reserve(m_blocks.size());
		for (const auto &it : m_blocks) {
			vector.emplace_back(it);
		}
	}

	// fm: A loaded member block can need a mesh even when its chunk origin was
	// omitted by the server. Keep one request source and only draw at the origin.
	struct NearChunk
	{
		MapBlockPtr origin;
		MapBlockPtr source;
	};
	std::unordered_map<v3bpos_t, NearChunk> near_chunks;
	for (const auto &[bp, block] : vector) {
		if (!block)
			continue;
		const auto pos = mesh_grid.getMeshPos(bp);
		auto &chunk = near_chunks[pos];
		if (!chunk.source)
			chunk.source = block;
		if (pos == bp)
			chunk.origin = block;
	}
	// ===

	struct NearCandidate
	{
		v3bpos_t pos;
		MapBlockPtr block;
		MapBlock::mesh_type mesh;
		int range_blocks;
	};

	std::vector<NearCandidate> direct_near;
	const auto camera_block = getNodeBlockPos(m_camera_position_node);
	const pos_t near_cell_width = mesh_grid.cell_size * MAP_BLOCKSIZE;

	// Near meshes are independent of the old far-grid origin after a teleport.
	// Cover the step-0 core, but never request an entire coarse fallback cell.
	const uint64_t near_handoff_range = std::max<uint64_t>(
			std::max(0, wanted_range), farmesh::getFarMeshNearRange(m_control));

	const auto requested_range = static_cast<int32_t>(
			std::min<uint64_t>(near_handoff_range, std::numeric_limits<int32_t>::max()));
	m_near_farmesh_range.store(requested_range, std::memory_order_relaxed);

	const auto request_mesh = [&](const v3bpos_t &bp, const MapBlockPtr &block,
									  const block_step_t mesh_step,
									  const MapBlock::mesh_type &mesh,
									  const int range_blocks) {
		const auto mesh_revision = block->getMeshRevision();
		const bool missing_or_wrong = !mesh || mesh->lod_step != mesh_step;

		// fm: Deduplicate actual work, not a revision marker that can outlive
		// rejected results or an unloaded chunk origin.
		if (missing_or_wrong && (m_mesh_queued < maxq || range_blocks <= 2) &&
				!m_client->isMeshUpdatePending(bp)) {
			m_client->addUpdateMeshTask(bp, false, false, mesh_step);
			++m_mesh_queued;
		}

		if (mesh && mesh->lod_step == mesh_step && mesh_revision > mesh->mesh_revision &&
				(m_mesh_queued < maxq * 1.5 || range_blocks <= 2) &&
				!m_client->isMeshUpdatePending(bp)) {
			if (mesh_step > 1)
				m_client->addUpdateMeshTask(bp, false, false, mesh_step);
			else
				m_client->addUpdateMeshTaskWithEdge(bp, false, false, mesh_step);
			++m_mesh_queued;
		}
		// ===
	};

	// fm: Keep nearby loaded data alive while its meshes are built.
	// ===
	for (const auto &[bp, block] : vector) {
		if (!block)
			continue;

		const auto block_min = bp * MAP_BLOCKSIZE;
		const bool keep_alive = m_control.range_all ||
								farmesh::cellIntersectsRange(block_min, MAP_BLOCKSIZE,
										m_camera_position_node, requested_range);
		if (!keep_alive) {
			if (wanted_range > 0) {
				const auto distance = radius_box(block_min, m_camera_position_node);
				if (distance > static_cast<uint64_t>(wanted_range) * 4)
					block->usage_timer_multiplier = distance / wanted_range;
			}
			continue;
		}

		block->resetUsageTimer();
		++blocks_in_range;
	}

	// Fill the per-update budget nearest first, independent of hash-map order.
	std::vector<std::pair<v3bpos_t, NearChunk>> ordered_chunks(
			near_chunks.begin(), near_chunks.end());
	sortMeshUpdatesNearFirst(
			ordered_chunks, camera_block, [](const auto &chunk) { return chunk.first; });

	// fm: Queue each populated chunk once, including an absent origin. Meshing
	// from a loaded member creates the origin block when the result is installed.
	for (const auto &[bp, chunk] : ordered_chunks) {
		const auto &block = chunk.origin;
		if (!m_control.range_all &&
				!farmesh::cellIntersectsRange(bp * MAP_BLOCKSIZE, near_cell_width,
						m_camera_position_node, requested_range))
			continue;
		// ===

		const auto near_cell_min = bp * MAP_BLOCKSIZE;
		const auto distance = radius_box(near_cell_min, m_camera_position_node);
		const int range_blocks = distance / MAP_BLOCKSIZE;
		const auto mesh_step = farmesh::getLodStep(m_control, camera_block, bp, speedf);
		// fm: A non-origin member is only a request source, never a mesh owner.
		const auto mesh =
				block ? block->getLodMesh(mesh_step, true) : MapBlock::mesh_type{};
		// ===
		// A completed near LOD covers the chunk while its requested LOD builds.
		// Requiring an exact LOD here hides terrain again whenever the camera moves.
		const bool covers_cell = bool(mesh);

		// fm: Empty completed chunks are ready. Missing origins need a request
		// through a member that exists in the map, as addUpdateMeshTask requires.
		if (!covers_cell)
			++blocks_in_range_without_mesh;
		const auto &source = block ? block : chunk.source;
		request_mesh(source->getPos(), source, mesh_step, mesh, range_blocks);
		// ===

		NearCandidate candidate{bp, block, mesh, range_blocks};
		if (mesh)
			direct_near.emplace_back(std::move(candidate));
	}

	// Pending requests must also follow the new camera position. Deduplication
	// alone would leave the nearest chunks behind the previous view's backlog.
	m_client->prioritizeMeshUpdates(camera_block);

	const auto draw_near = [&](const NearCandidate &candidate) {
		if (!candidate.mesh)
			return;
		far_draw->near_meshes.emplace(candidate.pos, candidate.mesh);
		if (candidate.mesh->isEmpty())
			return;

		if (candidate.range_blocks > 3 && !m_control.range_all &&
				occlusion_culling_enabled && m_enable_raytraced_culling &&
				isMeshOccluded(candidate.block.get(), mesh_grid.cell_size,
						m_camera_position_node)) {
			++blocks_occlusion_culled;
			return;
		}

		++blocks_in_range_with_mesh;
		candidate.mesh->last_used = m_client->m_uptime;
		drawlist.insert_or_assign(candidate.pos, candidate.block);
		farthest_drawn = std::max(farthest_drawn,
				static_cast<float>(candidate.range_blocks * MAP_BLOCKSIZE));
	};

	for (const auto &candidate : direct_near)
		draw_near(candidate);

	{
		//if (m_client->farmesh_remake.empty())
		m_far_blocks_delete_current = !m_far_blocks_delete_current;
		auto &m_far_blocks_delete = m_far_blocks_delete_current ? m_far_blocks_delete_1
																: m_far_blocks_delete_2;
		m_far_blocks_delete.clear();
		size_t farblocks_drawn = 0;

		std::vector<std::pair<v3bpos_t, MapBlockPtr>> far_blocks;
		{
			const auto lock = m_far_blocks.lock_unique_rec();
			for (auto it = m_far_blocks.begin(); it != m_far_blocks.end();) {
				const auto &block = it->second;
				if (far_iteration_clean && block->far_iteration < far_iteration_clean) {
					m_far_blocks_delete.emplace_back(block);
					it = m_far_blocks.erase(it);
				} else {
					if (block->far_iteration >= far_iteration_draw)
						far_blocks.emplace_back(*it);
					++it;
				}
			}
		}
		for (auto it = far_blocks.begin(); it != far_blocks.end(); ++it) {
			const auto &block = it->second;
			{
				const auto mesh = block->getFarMesh(block->far_step);
				std::vector<v3bpos_t> chunks;
				if (mesh) {
					// Far nodes can extend beyond their nominal cell (especially
					// downwards). Match actual geometry, including adjacent owners.
					aabb3f bounds = mesh->getMesh()->getBoundingBox();
					for (u8 layer = 1; layer < MAX_TILE_LAYERS; ++layer)
						bounds.addInternalBox(mesh->getMesh(layer)->getBoundingBox());
					const auto camera =
							(v3opos_t::from(m_camera_position_node) -
									v3opos_t::from(it->first) * MAP_BLOCKSIZE) *
							BS;
					const float reach = (double(requested_range) + near_cell_width) * BS;
					bool nearby = m_control.range_all;
					if (!nearby) {
						nearby = true;
						for (u8 axis = 0; axis < 3; ++axis)
							nearby &= bounds.MaxEdge[axis] >= camera[axis] - reach &&
									  bounds.MinEdge[axis] <= camera[axis] + reach;
					}
					if (nearby)
						for (const auto &[pos, near] : far_draw->near_meshes) {
							const auto minimum =
									(v3opos_t::from(pos) - v3opos_t::from(it->first)) *
											(MAP_BLOCKSIZE * BS) -
									v3opos_t(0.5 * BS);
							bool intersects = true;
							for (u8 axis = 0; axis < 3; ++axis)
								intersects &=
										bounds.MaxEdge[axis] >= minimum[axis] &&
										bounds.MinEdge[axis] <=
												minimum[axis] + near_cell_width * BS;
							if (intersects)
								chunks.push_back(pos);
						}
				}
				if (!chunks.empty()) {
					std::sort(chunks.begin(), chunks.end());
					std::shared_ptr<farmesh::ClippedFarMesh> clipped;
					if (previous_far_draw) {
						const auto old = previous_far_draw->clipped.find(it->first);
						if (old != previous_far_draw->clipped.end() &&
								old->second->source == mesh &&
								old->second->cell_size == mesh_grid.cell_size &&
								old->second->chunks == chunks)
							clipped = old->second;
					}
					if (!clipped) {
						clipped = std::make_shared<farmesh::ClippedFarMesh>();
						clipped->source = mesh;
						clipped->cell_size = mesh_grid.cell_size;
						clipped->chunks = chunks;
						farmesh::FarMeshClipMask mask(
								it->first, block->far_step, mesh_grid.cell_size);
						for (const auto &pos : chunks)
							mask.addChunk(pos);
						for (u8 layer = 0; layer < MAX_TILE_LAYERS; ++layer) {
							const auto &source = *mesh->getMesh(layer);
							for (u32 i = 0; i < source.getMeshBufferCount(); ++i)
								for (auto &buffer : mask.clip(*source.getMeshBuffer(i)))
									clipped->buffers.push_back(
											{std::move(buffer), source.getMeshBuffer(i)});
						}
					}
					far_draw->clipped.emplace(it->first, std::move(clipped));
				} else {
					drawlist.emplace(it->first, block);
				}
				++farblocks_drawn;
			}
		}

		g_profiler->avg("Client: Farmesh drawn", farblocks_drawn);
		g_profiler->avg("Client: Farmesh clipped cells", far_draw->clipped.size());
#if !NDEBUG
		g_profiler->avg("Client: Farmesh total", m_far_blocks.size());
#endif
	}

	//for (auto & ir : *m_drawlist)
	//	ir.second->refDrop();

	const auto drawlist_size = drawlist.size();
	{
		std::lock_guard<std::recursive_mutex> lock(m_drawlist_mutex);
		const bool current = m_drawlist_current.load(std::memory_order_relaxed);
		auto &back_drawlist = current ? m_drawlist_0 : m_drawlist_1;
		back_drawlist = std::move(drawlist);
		m_far_draw_state = std::move(far_draw);
		m_drawlist_current.store(!current, std::memory_order_release);
	}

	/*
	m_control.blocks_would_have_drawn = blocks_would_have_drawn;
	m_control.blocks_drawn = blocks_drawn;
*/
	m_control.farthest_drawn = farthest_drawn;

	/*
	g_profiler->avg("CM: blocks total", m_blocks.size());
	g_profiler->avg("CM: blocks in range", blocks_in_range);
	g_profiler->avg("CM: blocks occlusion culled", blocks_occlusion_culled);
	if (blocks_in_range != 0)
		g_profiler->avg("CM: blocks in range without mesh (frac)",
				(float)blocks_in_range_without_mesh / blocks_in_range);
	g_profiler->avg("CM: blocks drawn", blocks_drawn);
	g_profiler->avg("CM: farthest drawn", farthest_drawn);
	//g_profiler->avg("CM: wanted max blocks", m_control.wanted_max_blocks);
*/
	g_profiler->avg("MapBlock without meshs in range [#]", blocks_in_range_without_mesh);

	g_profiler->avg("MapBlock meshes in range [#]", blocks_in_range_with_mesh);
	g_profiler->avg("MapBlocks in range [#]", blocks_in_range);
	g_profiler->avg("MapBlocks occlusion culled [#]", blocks_occlusion_culled);
	g_profiler->avg("MapBlocks drawn [#]", drawlist_size);
	//g_profiler->avg("MapBlocks loaded [#]", blocks_loaded);
	g_profiler->avg("MapBlocks loaded [#]", m_blocks.size());
}

void MapDrawControl::fm_init()
{
	g_settings->getS32NoEx("farmesh", farmesh);
	g_settings->getS32NoEx("lodmesh", lodmesh);
	static const auto headless_optimize = g_settings->getBool("headless_optimize");
	if (headless_optimize)
		lodmesh = 0;

	fov_want = fov = g_settings->getFloat("fov");
}

void MapDrawControl::registerSettingsCallbacks()
{
	/*
	g_settings->registerChangedCallback("farmesh", [](const std::string &name, void *data) {
		static_cast<MapDrawControl*>(data)->onSettingChanged(name);
	}, this);
	
	g_settings->registerChangedCallback("lodmesh", [](const std::string &name, void *data) {
		static_cast<MapDrawControl*>(data)->onSettingChanged(name);
	}, this);
	
	g_settings->registerChangedCallback("farmesh_quality", [](const std::string &name, void *data) {
		static_cast<MapDrawControl*>(data)->onSettingChanged(name);
	}, this);
	
	g_settings->registerChangedCallback("farmesh_stable", [](const std::string &name, void *data) {
		static_cast<MapDrawControl*>(data)->onSettingChanged(name);
	}, this);
	
	g_settings->registerChangedCallback("farmesh_all_changed", [](const std::string &name, void *data) {
		static_cast<MapDrawControl*>(data)->onSettingChanged(name);
	}, this);
	*/
}

void MapDrawControl::onSettingChanged(const std::string &name)
{
	if (name == "farmesh")
		farmesh = g_settings->getS32("farmesh");
	if (name == "lodmesh")
		lodmesh = g_settings->getS32("lodmesh");
	if (name == "farmesh_quality") {
		farmesh_quality = g_settings->getU16("farmesh_quality");
		farmesh_quality_pow = farmesh::rangeToStep(farmesh_quality);
	}
	if (name == "farmesh_stable")
		farmesh_stable = g_settings->getU16("farmesh_stable");
	if (name == "farmesh_all_changed")
		farmesh_all_changed = g_settings->getPos("farmesh_all_changed");
}
