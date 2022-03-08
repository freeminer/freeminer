/*
clientmap.cpp
Copyright (C) 2010-2013 celeron55, Perttu Ahola <celeron55@gmail.com>
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

#include "clientmap.h"
#include "client.h"
#include "mapblock_mesh.h"
#include <IMaterialRenderer.h>
#include <matrix4.h>
<<<<<<< HEAD:src/clientmap.cpp
#include "log_types.h"
#include "nodedef.h"
=======
#include "mapsector.h"
>>>>>>> 5.5.0:src/client/clientmap.cpp
#include "mapblock.h"
#include "profiler.h"
#include "settings.h"
#include "camera.h"               // CameraModes
#include "util/basic_macros.h"
#include <algorithm>
<<<<<<< HEAD:src/clientmap.cpp
#include <unordered_map>
#include <utility>

void MapDrawControl::fm_init() {
	farmesh = g_settings->getS32("farmesh");
	farmesh_step = g_settings->getS32("farmesh_step");
	fov_want = fov = g_settings->getFloat("fov");
}
=======
#include "client/renderingengine.h"

// struct MeshBufListList
void MeshBufListList::clear()
{
	for (auto &list : lists)
		list.clear();
}

void MeshBufListList::add(scene::IMeshBuffer *buf, v3s16 position, u8 layer)
{
	// Append to the correct layer
	std::vector<MeshBufList> &list = lists[layer];
	const video::SMaterial &m = buf->getMaterial();
	for (MeshBufList &l : list) {
		// comparing a full material is quite expensive so we don't do it if
		// not even first texture is equal
		if (l.m.TextureLayer[0].Texture != m.TextureLayer[0].Texture)
			continue;

		if (l.m == m) {
			l.bufs.emplace_back(position, buf);
			return;
		}
	}
	MeshBufList l;
	l.m = m;
	l.bufs.emplace_back(position, buf);
	list.emplace_back(l);
}

// ClientMap
>>>>>>> 5.5.0:src/client/clientmap.cpp

ClientMap::ClientMap(
		Client *client,
		RenderingEngine *rendering_engine,
		MapDrawControl &control,
		s32 id
):
<<<<<<< HEAD:src/clientmap.cpp
	Map(gamedef),
	scene::ISceneNode(parent, mgr, id),
=======
	Map(client),
	scene::ISceneNode(rendering_engine->get_scene_manager()->getRootSceneNode(),
		rendering_engine->get_scene_manager(), id),
>>>>>>> 5.5.0:src/client/clientmap.cpp
	m_client(client),
	m_rendering_engine(rendering_engine),
	m_control(control),
<<<<<<< HEAD:src/clientmap.cpp
	m_camera_position(0,0,0),
	m_camera_direction(0,0,1),
	m_camera_fov(M_PI)
	,m_drawlist(&m_drawlist_1),
	m_drawlist_current(0)
{
	m_drawlist_last = 0;
=======
	m_drawlist(MapBlockComparer(v3s16(0,0,0)))
{

	/*
	 * @Liso: Sadly C++ doesn't have introspection, so the only way we have to know
	 * the class is whith a name ;) Name property cames from ISceneNode base class.
	 */
	Name = "ClientMap";
>>>>>>> 5.5.0:src/client/clientmap.cpp
	m_box = aabb3f(-BS*1000000,-BS*1000000,-BS*1000000,
			BS*1000000,BS*1000000,BS*1000000);

	/* TODO: Add a callback function so these can be updated when a setting
	 *       changes.  At this point in time it doesn't matter (e.g. /set
	 *       is documented to change server settings only)
	 *
	 * TODO: Local caching of settings is not optimal and should at some stage
	 *       be updated to use a global settings object for getting thse values
	 *       (as opposed to the this local caching). This can be addressed in
	 *       a later release.
	 */
	m_cache_trilinear_filter  = g_settings->getBool("trilinear_filter");
	m_cache_bilinear_filter   = g_settings->getBool("bilinear_filter");
	m_cache_anistropic_filter = g_settings->getBool("anisotropic_filter");

}

<<<<<<< HEAD:src/clientmap.cpp
ClientMap::~ClientMap()
{
	SceneManager->getVideoDriver()->removeAllHardwareBuffers();

	/*MutexAutoLock lock(mesh_mutex);

	if(mesh != NULL)
	{
		mesh->drop();
		mesh = NULL;
	}*/

}

#if WTF
=======
>>>>>>> 5.5.0:src/client/clientmap.cpp
MapSector * ClientMap::emergeSector(v2s16 p2d)
{
	// Check that it doesn't exist already
	MapSector *sector = getSectorNoGenerate(p2d);

	// Create it if it does not exist yet
	if (!sector) {
		sector = new MapSector(this, p2d, m_gamedef);
		m_sectors[p2d] = sector;
	}

	return sector;
}
#endif

void ClientMap::OnRegisterSceneNode()
{
	if(IsVisible)
	{
		SceneManager->registerNodeForRendering(this, scene::ESNRP_SOLID);
		SceneManager->registerNodeForRendering(this, scene::ESNRP_TRANSPARENT);
	}

	ISceneNode::OnRegisterSceneNode();

<<<<<<< HEAD:src/clientmap.cpp
static bool isOccluded(Map *map, v3s16 p0, v3s16 p1, float step, float stepfac,
		float start_off, float end_off, u32 needed_count, INodeDefManager *nodemgr,
		unordered_map_v3POS<bool> & occlude_cache)
{
	float d0 = (float)BS * p0.getDistanceFrom(p1);
	v3s16 u0 = p1 - p0;
	v3f uf = v3f(u0.X, u0.Y, u0.Z) * BS;
	uf.normalize();
	v3f p0f = v3f(p0.X, p0.Y, p0.Z) * BS;
	u32 count = 0;
	for(float s=start_off; s<d0+end_off; s+=step){
		v3f pf = p0f + uf * s;
		v3s16 p = floatToInt(pf, BS);
		bool is_transparent = false;
		bool cache = true;
		if (occlude_cache.count(p)) {
			cache = false;
			is_transparent = occlude_cache[p];
		} else {
		MapNode n = map->getNodeTry(p);
		if (n.getContent() == CONTENT_IGNORE) {
			cache = false;
		}
		const ContentFeatures &f = nodemgr->get(n);
		if(f.solidness == 0)
			is_transparent = (f.visual_solidness != 2);
		else
			is_transparent = (f.solidness != 2);
		}
		if (cache)
			occlude_cache[p] = is_transparent;
		if(!is_transparent){
			count++;
			if(count >= needed_count)
				return true;
		}
		step *= stepfac;
=======
	if (!m_added_to_shadow_renderer) {
		m_added_to_shadow_renderer = true;
		if (auto shadows = m_rendering_engine->get_shadow_renderer())
			shadows->addNodeToShadowList(this);
>>>>>>> 5.5.0:src/client/clientmap.cpp
	}
}

void ClientMap::getBlocksInViewRange(v3s16 cam_pos_nodes,
		v3s16 *p_blocks_min, v3s16 *p_blocks_max, float range)
{
	if (range <= 0.0f)
		range = m_control.wanted_range;

	v3s16 box_nodes_d = range * v3s16(1, 1, 1);
	// Define p_nodes_min/max as v3s32 because 'cam_pos_nodes -/+ box_nodes_d'
	// can exceed the range of v3s16 when a large view range is used near the
	// world edges.
	v3s32 p_nodes_min(
		cam_pos_nodes.X - box_nodes_d.X,
		cam_pos_nodes.Y - box_nodes_d.Y,
		cam_pos_nodes.Z - box_nodes_d.Z);
	v3s32 p_nodes_max(
		cam_pos_nodes.X + box_nodes_d.X,
		cam_pos_nodes.Y + box_nodes_d.Y,
		cam_pos_nodes.Z + box_nodes_d.Z);
	// Take a fair amount as we will be dropping more out later
	// Umm... these additions are a bit strange but they are needed.
	*p_blocks_min = v3s16(
			p_nodes_min.X / MAP_BLOCKSIZE - 3,
			p_nodes_min.Y / MAP_BLOCKSIZE - 3,
			p_nodes_min.Z / MAP_BLOCKSIZE - 3);
	*p_blocks_max = v3s16(
			p_nodes_max.X / MAP_BLOCKSIZE + 1,
			p_nodes_max.Y / MAP_BLOCKSIZE + 1,
			p_nodes_max.Z / MAP_BLOCKSIZE + 1);
}

<<<<<<< HEAD:src/clientmap.cpp
void ClientMap::updateDrawList(video::IVideoDriver* driver, float dtime, unsigned int max_cycle_ms)
{
	ScopeProfiler sp(g_profiler, "CM::updateDrawList()", SPT_AVG);
	//g_profiler->add("CM::updateDrawList() count", 1);
	TimeTaker timer_step("ClientMap::updateDrawList");
=======
void ClientMap::updateDrawList()
{
	ScopeProfiler sp(g_profiler, "CM::updateDrawList()", SPT_AVG);
>>>>>>> 5.5.0:src/client/clientmap.cpp

	m_needs_update_drawlist = false;

<<<<<<< HEAD:src/clientmap.cpp
	if (!m_drawlist_last)
		m_drawlist_current = !m_drawlist_current;
	auto & drawlist = m_drawlist_current ? m_drawlist_1 : m_drawlist_0;

	if (!max_cycle_ms)
		max_cycle_ms = 300/getControl().fps_wanted;

	v3f camera_position = m_camera_position;
	f32 camera_fov = m_camera_fov;
=======
	for (auto &i : m_drawlist) {
		MapBlock *block = i.second;
		block->refDrop();
	}
	m_drawlist.clear();

	const v3f camera_position = m_camera_position;
	const v3f camera_direction = m_camera_direction;
>>>>>>> 5.5.0:src/client/clientmap.cpp

	// Use a higher fov to accomodate faster camera movements.
	// Blocks are cropped better when they are drawn.
	const f32 camera_fov = m_camera_fov * 1.1f;

	v3s16 cam_pos_nodes = floatToInt(camera_position, BS);

	v3s16 p_blocks_min;
	v3s16 p_blocks_max;
	getBlocksInViewRange(cam_pos_nodes, &p_blocks_min, &p_blocks_max);

	// Read the vision range, unless unlimited range is enabled.
	float range = m_control.range_all ? 1e7 : m_control.wanted_range;

	// Number of blocks currently loaded by the client
	u32 blocks_loaded = 0;
	// Number of blocks with mesh in rendering range
	u32 blocks_in_range_with_mesh = 0;
	// Number of blocks occlusion culled
	u32 blocks_occlusion_culled = 0;
<<<<<<< HEAD:src/clientmap.cpp
	// Number of blocks in rendering range but don't have a mesh
	u32 blocks_in_range_without_mesh = 0;
	// Blocks that had mesh that would have been drawn according to
	// rendering range (if max blocks limit didn't kick in)
	u32 blocks_would_have_drawn = 0;
	// Blocks that were drawn and had a mesh
	u32 blocks_drawn = 0;
	// Blocks which had a corresponding meshbuffer for this pass
	//u32 blocks_had_pass_meshbuf = 0;
	// Blocks from which stuff was actually drawn
	//u32 blocks_without_stuff = 0;
	// Distance to farthest drawn block
	float farthest_drawn = 0;
	int m_mesh_queued = 0;

	bool free_move = g_settings->getBool("free_move");

	float range_max = m_control.range_all ? MAX_MAP_GENERATION_LIMIT*2 : m_control.wanted_range * (m_control.wanted_range > 200 ? 1.2 : 1.5);

	if (draw_nearest.empty()) {
		//ScopeProfiler sp(g_profiler, "CM::updateDrawList() make list", SPT_AVG);
		TimeTaker timer_step("ClientMap::updateDrawList make list");

		auto lock = m_blocks.try_lock_shared_rec();
		if (!lock->owns_lock())
			return;

		draw_nearest.clear();

		for(auto & ir : m_blocks) {
			auto bp = ir.first;

/*
		if (m_control.range_all == false) {
			if (bp.X < p_blocks_min.X || bp.X > p_blocks_max.X
			|| bp.Z > p_blocks_max.Z || bp.Z < p_blocks_min.Z
			|| bp.Y < p_blocks_min.Y || bp.Y > p_blocks_max.Y)
=======

	// No occlusion culling when free_move is on and camera is
	// inside ground
	bool occlusion_culling_enabled = true;
	if (g_settings->getBool("free_move") && g_settings->getBool("noclip")) {
		MapNode n = getNode(cam_pos_nodes);
		if (n.getContent() == CONTENT_IGNORE ||
				m_nodedef->get(n).solidness == 2)
			occlusion_culling_enabled = false;
	}

	v3s16 camera_block = getContainerPos(cam_pos_nodes, MAP_BLOCKSIZE);
	m_drawlist = std::map<v3s16, MapBlock*, MapBlockComparer>(MapBlockComparer(camera_block));

	// Uncomment to debug occluded blocks in the wireframe mode
	// TODO: Include this as a flag for an extended debugging setting
	//if (occlusion_culling_enabled && m_control.show_wireframe)
	//    occlusion_culling_enabled = porting::getTimeS() & 1;

	for (const auto &sector_it : m_sectors) {
		MapSector *sector = sector_it.second;
		v2s16 sp = sector->getPos();

		blocks_loaded += sector->size();
		if (!m_control.range_all) {
			if (sp.X < p_blocks_min.X || sp.X > p_blocks_max.X ||
					sp.Y < p_blocks_min.Z || sp.Y > p_blocks_max.Z)
>>>>>>> 5.5.0:src/client/clientmap.cpp
				continue;
		}

			v3s16 blockpos_nodes = bp * MAP_BLOCKSIZE;
			// Block center position
			v3f blockpos(
				((float)blockpos_nodes.X + MAP_BLOCKSIZE/2) * BS,
				((float)blockpos_nodes.Y + MAP_BLOCKSIZE/2) * BS,
				((float)blockpos_nodes.Z + MAP_BLOCKSIZE/2) * BS
			);
*/

			f32 d = radius_box(bp*MAP_BLOCKSIZE, cam_pos_nodes); //blockpos_relative.getLength();
			if (d > range_max) {
				if (d > range_max * 4 && ir.second) {
					int mul = d / range_max;
					ir.second->usage_timer_multiplier = mul;
				}
				continue;
			}
			int range = d / MAP_BLOCKSIZE;
			draw_nearest.emplace_back(std::make_pair(bp, range));
		}
	}

	const int maxq = 1000;

<<<<<<< HEAD:src/clientmap.cpp
			// No occlusion culling when free_move is on and camera is
			// inside ground
			bool occlusion_culling_enabled = true;
			if(free_move){
				MapNode n = getNodeNoEx(cam_pos_nodes);
				if(n.getContent() == CONTENT_IGNORE ||
						nodemgr->get(n).solidness == 2)
					occlusion_culling_enabled = false;
			}

	u32 calls = 0, end_ms = porting::getTimeMs() + u32(max_cycle_ms);

	unordered_map_v3POS<bool> occlude_cache;

	while (!draw_nearest.empty()) {
		auto ir = draw_nearest.back();

		auto bp = ir.first;
		int range = ir.second;
		draw_nearest.pop_back();
		++calls;

		//auto block = getBlockNoCreateNoEx(bp);
		auto block = m_blocks.get(bp);
		if (!block)
			continue;

			int mesh_step = getFarmeshStep(m_control, getNodeBlockPos(cam_pos_nodes), bp);
=======
		for (MapBlock *block : sectorblocks) {
>>>>>>> 5.5.0:src/client/clientmap.cpp
			/*
				Compare block position to camera position, skip
				if not seen on display
			*/

<<<<<<< HEAD:src/clientmap.cpp
			auto mesh = block->getMesh(mesh_step);
			if (mesh)
				mesh->updateCameraOffset(m_camera_offset);

			blocks_in_range++;

			unsigned int smesh_size = block->mesh_size;
			/*
				Ignore if mesh doesn't exist
			*/
			{
				if(!mesh) {
					blocks_in_range_without_mesh++;
					if (m_mesh_queued < maxq || range <= 2) {
						m_client->addUpdateMeshTask(bp, false);
						++m_mesh_queued;
					}
					continue;
				}
				if(mesh_step == mesh->step && block->getTimestamp() <= mesh->timestamp && !smesh_size) {
					blocks_in_range_without_mesh++;
					continue;
				}
			}

			/*
				Occlusion culling
			*/

			v3POS cpn = bp * MAP_BLOCKSIZE;
			cpn += v3s16(MAP_BLOCKSIZE / 2, MAP_BLOCKSIZE / 2, MAP_BLOCKSIZE / 2);
			float step = BS * 1;
			float stepfac = 1.2;
			float startoff = BS * 1;
			// The occlusion search of 'isOccluded()' must stop short of the target
			// point by distance 'endoff' (end offset) to not enter the target mapblock.
			// For the 8 mapblock corners 'endoff' must therefore be the maximum diagonal
			// of a mapblock, because we must consider all view angles.
			// sqrt(1^2 + 1^2 + 1^2) = 1.732
			float endoff = -BS * MAP_BLOCKSIZE * 1.732050807569;
			v3s16 spn = cam_pos_nodes;
			s16 bs2 = MAP_BLOCKSIZE / 2 + 1;
			// to reduce the likelihood of falsely occluded blocks
			// require at least two solid blocks
			// this is a HACK, we should think of a more precise algorithm
			u32 needed_count = 2;
			if (occlusion_culling_enabled &&
				range > 1 && smesh_size &&
					// For the central point of the mapblock 'endoff' can be halved
					isOccluded(this, spn, cpn,
						step, stepfac, startoff, endoff / 2.0f, needed_count, nodemgr, occlude_cache) &&
					isOccluded(this, spn, cpn + v3s16(bs2,bs2,bs2),
						step, stepfac, startoff, endoff, needed_count, nodemgr, occlude_cache) &&
					isOccluded(this, spn, cpn + v3s16(bs2,bs2,-bs2),
						step, stepfac, startoff, endoff, needed_count, nodemgr, occlude_cache) &&
					isOccluded(this, spn, cpn + v3s16(bs2,-bs2,bs2),
						step, stepfac, startoff, endoff, needed_count, nodemgr, occlude_cache) &&
					isOccluded(this, spn, cpn + v3s16(bs2,-bs2,-bs2),
						step, stepfac, startoff, endoff, needed_count, nodemgr, occlude_cache) &&
					isOccluded(this, spn, cpn + v3s16(-bs2,bs2,bs2),
						step, stepfac, startoff, endoff, needed_count, nodemgr, occlude_cache) &&
					isOccluded(this, spn, cpn + v3s16(-bs2,bs2,-bs2),
						step, stepfac, startoff, endoff, needed_count, nodemgr, occlude_cache) &&
					isOccluded(this, spn, cpn + v3s16(-bs2,-bs2,bs2),
						step, stepfac, startoff, endoff, needed_count, nodemgr, occlude_cache) &&
					isOccluded(this, spn, cpn + v3s16(-bs2,-bs2,-bs2),
						step, stepfac, startoff, endoff, needed_count, nodemgr, occlude_cache)) {
=======
			if (!block->mesh) {
				// Ignore if mesh doesn't exist
				continue;
			}

			v3s16 block_coord = block->getPos();
			v3s16 block_position = block->getPosRelative() + MAP_BLOCKSIZE / 2;

			// First, perform a simple distance check, with a padding of one extra block.
			if (!m_control.range_all &&
					block_position.getDistanceFrom(cam_pos_nodes) > range + MAP_BLOCKSIZE)
				continue; // Out of range, skip.

			// Keep the block alive as long as it is in range.
			block->resetUsageTimer();
			blocks_in_range_with_mesh++;

			// Frustum culling
			float d = 0.0;
			if (!isBlockInSight(block_coord, camera_position,
					camera_direction, camera_fov, range * BS, &d))
				continue;

			// Occlusion culling
			if ((!m_control.range_all && d > m_control.wanted_range * BS) ||
					(occlusion_culling_enabled && isBlockOccluded(block, cam_pos_nodes))) {
>>>>>>> 5.5.0:src/client/clientmap.cpp
				blocks_occlusion_culled++;
				continue;
			}

<<<<<<< HEAD:src/clientmap.cpp
			// This block is in range. Reset usage timer.
			block->resetUsageTimer();

			// Limit block count in case of a sudden increase
			blocks_would_have_drawn++;
/*
			if (blocks_drawn >= m_control.wanted_max_blocks &&
					!m_control.range_all &&
					d > m_control.wanted_range * BS)
				continue;
*/

			if (mesh_step != mesh->step && (m_mesh_queued < maxq*1.2 || range <= 2)) {
				m_client->addUpdateMeshTask(bp);
				++m_mesh_queued;
			}
			if (block->getTimestamp() > mesh->timestamp + (smesh_size ? 0 : range >= 1 ? 60 : 5) && (m_mesh_queued < maxq*1.5 || range <= 2)) {
				if (mesh_step > 1)
					m_client->addUpdateMeshTask(bp);
				else
					m_client->addUpdateMeshTaskWithEdge(bp);
				++m_mesh_queued;
			}

			if(!smesh_size)
				continue;

			mesh->incrementUsageTimer(dtime);

			// Add to set
			//block->refGrab();
			//block->resetUsageTimer();
			drawlist.set(bp, block);

			blocks_drawn++;

			if(range * MAP_BLOCKSIZE > farthest_drawn)
				farthest_drawn = range * MAP_BLOCKSIZE;

			if (farthest_drawn > m_control.farthest_drawn)
				m_control.farthest_drawn = farthest_drawn;

		if (porting::getTimeMs() > end_ms) {
			break;
		}
=======
			// Add to set
			block->refGrab();
			m_drawlist[block_coord] = block;

			sector_blocks_drawn++;
		} // foreach sectorblocks
>>>>>>> 5.5.0:src/client/clientmap.cpp

	}
	m_drawlist_last = draw_nearest.size();

	//if (m_drawlist_last) infostream<<"breaked UDL "<<m_drawlist_last<<" collected="<<drawlist.size()<<" calls="<<calls<<" s="<<m_blocks.size()<<" maxms="<<max_cycle_ms<<" fw="<<getControl().fps_wanted<<" morems="<<porting::getTimeMs() - end_ms<< " meshq="<<m_mesh_queued<<" occache="<<occlude_cache.size()<<std::endl;

	if (m_drawlist_last)
		return;

	//for (auto & ir : *m_drawlist)
	//	ir.second->refDrop();

	auto m_drawlist_old = !m_drawlist_current ? &m_drawlist_1 : &m_drawlist_0;
	m_drawlist = m_drawlist_current ? &m_drawlist_1 : &m_drawlist_0;
	m_drawlist_old->clear();

<<<<<<< HEAD:src/clientmap.cpp
	m_control.blocks_would_have_drawn = blocks_would_have_drawn;
	m_control.blocks_drawn = blocks_drawn;
	m_control.farthest_drawn = farthest_drawn;

	g_profiler->avg("CM: blocks total", m_blocks.size());
	g_profiler->avg("CM: blocks in range", blocks_in_range);
	g_profiler->avg("CM: blocks occlusion culled", blocks_occlusion_culled);
	if (blocks_in_range != 0)
		g_profiler->avg("CM: blocks in range without mesh (frac)",
				(float)blocks_in_range_without_mesh / blocks_in_range);
	g_profiler->avg("CM: blocks drawn", blocks_drawn);
	g_profiler->avg("CM: farthest drawn", farthest_drawn);
	//g_profiler->avg("CM: wanted max blocks", m_control.wanted_max_blocks);
=======
	g_profiler->avg("MapBlock meshes in range [#]", blocks_in_range_with_mesh);
	g_profiler->avg("MapBlocks occlusion culled [#]", blocks_occlusion_culled);
	g_profiler->avg("MapBlocks drawn [#]", m_drawlist.size());
	g_profiler->avg("MapBlocks loaded [#]", blocks_loaded);
>>>>>>> 5.5.0:src/client/clientmap.cpp
}

void ClientMap::renderMap(video::IVideoDriver* driver, s32 pass)
{
	bool is_transparent_pass = pass == scene::ESNRP_TRANSPARENT;

	std::string prefix;
	if (pass == scene::ESNRP_SOLID)
		prefix = "renderMap(SOLID): ";
	else
		prefix = "renderMap(TRANSPARENT): ";

	//ScopeProfiler sp(g_profiler, "CM::renderMap() " + prefix, SPT_AVG);

	/*
<<<<<<< HEAD:src/clientmap.cpp
		Get time for measuring timeout.

		Measuring time is very useful for long delays when the
		machine is swapping a lot.
	*/
	//int time1 = time(0);

	/*
=======
>>>>>>> 5.5.0:src/client/clientmap.cpp
		Get animation parameters
	*/
	const float animation_time = m_client->getAnimationTime();
	const int crack = m_client->getCrackLevel();
	const u32 daynight_ratio = m_client->getEnv().getDayNightRatio();

<<<<<<< HEAD:src/clientmap.cpp
	v3f camera_position = m_camera_position;
	f32 camera_fov = m_camera_fov * 1.1;
	//v3f camera_direction = m_camera_direction;
	float range_max_bs = (m_control.range_all ? MAX_MAP_GENERATION_LIMIT*2 : m_control.wanted_range) * BS;
=======
	const v3f camera_position = m_camera_position;
>>>>>>> 5.5.0:src/client/clientmap.cpp

	/*
		Get all blocks and draw all visible ones
	*/

<<<<<<< HEAD:src/clientmap.cpp
	v3s16 cam_pos_nodes = floatToInt(camera_position, BS);
/*
	v3s16 p_blocks_min;
	v3s16 p_blocks_max;
	getBlocksInViewRange(cam_pos_nodes, &p_blocks_min, &p_blocks_max);
*/

=======
>>>>>>> 5.5.0:src/client/clientmap.cpp
	u32 vertex_count = 0;
	u32 drawcall_count = 0;

	// For limiting number of mesh animations per frame
	u32 mesh_animate_count = 0;
	//u32 mesh_animate_count_far = 0;

	/*
		Draw the selected MapBlocks
	*/

<<<<<<< HEAD:src/clientmap.cpp
	{

/*
	ScopeProfiler sp(g_profiler, prefix + "drawing blocks", SPT_AVG);
*/
=======
	MeshBufListList grouped_buffers;
>>>>>>> 5.5.0:src/client/clientmap.cpp

	struct DrawDescriptor {
		v3s16 m_pos;
		scene::IMeshBuffer *m_buffer;
		bool m_reuse_material;

<<<<<<< HEAD:src/clientmap.cpp
	std::vector<MapBlock::mesh_type> used_meshes; //keep shared_ptr
	auto drawlist = m_drawlist.load();
	auto lock = drawlist->lock_shared_rec();
	used_meshes.reserve(drawlist->size());
	//g_profiler->add("CM::renderMap()cnt"+ prefix, drawlist->size());
	for (auto & ir : *drawlist) {
		auto block = ir.second;
=======
		DrawDescriptor(const v3s16 &pos, scene::IMeshBuffer *buffer, bool reuse_material) :
			m_pos(pos), m_buffer(buffer), m_reuse_material(reuse_material)
		{}
	};

	std::vector<DrawDescriptor> draw_order;
	video::SMaterial previous_material;

	for (auto &i : m_drawlist) {
		v3s16 block_pos = i.first;
		MapBlock *block = i.second;
>>>>>>> 5.5.0:src/client/clientmap.cpp

		int mesh_step = getFarmeshStep(m_control, getNodeBlockPos(cam_pos_nodes), block->getPos());
		// If the mesh of the block happened to get deleted, ignore it
<<<<<<< HEAD:src/clientmap.cpp
		auto mapBlockMesh = block->getMesh(mesh_step);
		if (!mapBlockMesh)
			continue;

		float d = 0.0;
		if (!isBlockInSight(block->getPos(), camera_position,
				m_camera_direction, camera_fov, range_max_bs, &d))
			continue;
		used_meshes.emplace_back(mapBlockMesh);

		// Mesh animation
		//if (mesh_step <= 1)
		{
			//MutexAutoLock lock(block->mesh_mutex);

			mapBlockMesh->updateCameraOffset(m_camera_offset);

=======
		if (!block->mesh)
			continue;

		v3f block_pos_r = intToFloat(block->getPosRelative() + MAP_BLOCKSIZE / 2, BS);
		float d = camera_position.getDistanceFrom(block_pos_r);
		d = MYMAX(0,d - BLOCK_MAX_RADIUS);

		// Mesh animation
		if (pass == scene::ESNRP_SOLID) {
			MapBlockMesh *mapBlockMesh = block->mesh;
			assert(mapBlockMesh);
>>>>>>> 5.5.0:src/client/clientmap.cpp
			// Pretty random but this should work somewhat nicely
#if __ANDROID__
			bool faraway = d >= BS * 16;
#else
			bool faraway = d >= BS * 50;
<<<<<<< HEAD:src/clientmap.cpp
#endif
			//bool faraway = d >= m_control.wanted_range * BS;
=======
>>>>>>> 5.5.0:src/client/clientmap.cpp
			if (mapBlockMesh->isAnimationForced() || !faraway ||
					mesh_animate_count < (m_control.range_all ? 200 : 50)) {

				bool animated = mapBlockMesh->animate(faraway, animation_time,
					crack, daynight_ratio);
				if (animated)
					mesh_animate_count++;
			} else {
				mapBlockMesh->decreaseAnimationForceTimer();
			}
		}

		/*
			Get the meshbuffers of the block
		*/
		{
<<<<<<< HEAD:src/clientmap.cpp
			//MutexAutoLock lock(block->mesh_mutex);

			auto *mesh = mapBlockMesh->getMesh();
			if (!mesh)
				continue;
=======
			MapBlockMesh *mapBlockMesh = block->mesh;
			assert(mapBlockMesh);

			for (int layer = 0; layer < MAX_TILE_LAYERS; layer++) {
				scene::IMesh *mesh = mapBlockMesh->getMesh(layer);
				assert(mesh);
>>>>>>> 5.5.0:src/client/clientmap.cpp

				u32 c = mesh->getMeshBufferCount();
				for (u32 i = 0; i < c; i++) {
					scene::IMeshBuffer *buf = mesh->getMeshBuffer(i);

					video::SMaterial& material = buf->getMaterial();
					video::IMaterialRenderer* rnd =
						driver->getMaterialRenderer(material.MaterialType);
					bool transparent = (rnd && rnd->isTransparent());
					if (transparent == is_transparent_pass) {
						if (buf->getVertexCount() == 0)
							errorstream << "Block [" << analyze_block(block)
								<< "] contains an empty meshbuf" << std::endl;

						material.setFlag(video::EMF_TRILINEAR_FILTER,
							m_cache_trilinear_filter);
						material.setFlag(video::EMF_BILINEAR_FILTER,
							m_cache_bilinear_filter);
						material.setFlag(video::EMF_ANISOTROPIC_FILTER,
							m_cache_anistropic_filter);
						material.setFlag(video::EMF_WIREFRAME,
							m_control.show_wireframe);

						if (is_transparent_pass) {
							// Same comparison as in MeshBufListList
							bool new_material = material.getTexture(0) != previous_material.getTexture(0) ||
									material != previous_material;

							draw_order.emplace_back(block_pos, buf, !new_material);

							if (new_material)
								previous_material = material;
						}
						else {
							grouped_buffers.add(buf, block_pos, layer);
						}
					}
				}
			}
		}
	}

<<<<<<< HEAD:src/clientmap.cpp
	std::vector<MeshBufList> &lists = drawbufs.lists;

/*
	int timecheck_counter = 0;
*/
	for (std::vector<MeshBufList>::iterator i = lists.begin();
			i != lists.end(); ++i) {
#if 0
		timecheck_counter++;
		if (timecheck_counter > 50) {
			timecheck_counter = 0;
			int time2 = time(0);
			if (time2 > time1 + 4) {
				infostream << "ClientMap::renderMap(): "
					"Rendering takes ages, returning."
					<< std::endl;
				return;
			}
		}
#endif
=======
	// Capture draw order for all solid meshes
	for (auto &lists : grouped_buffers.lists) {
		for (MeshBufList &list : lists) {
			// iterate in reverse to draw closest blocks first
			for (auto it = list.bufs.rbegin(); it != list.bufs.rend(); ++it) {
				draw_order.emplace_back(it->first, it->second, it != list.bufs.rbegin());
			}
		}
	}
>>>>>>> 5.5.0:src/client/clientmap.cpp

	TimeTaker draw("Drawing mesh buffers");

	core::matrix4 m; // Model matrix
	v3f offset = intToFloat(m_camera_offset, BS);
	u32 material_swaps = 0;

	// Render all mesh buffers in order
	drawcall_count += draw_order.size();
	for (auto &descriptor : draw_order) {
		scene::IMeshBuffer *buf = descriptor.m_buffer;

		// Check and abort if the machine is swapping a lot
		if (draw.getTimerTime() > 2000) {
			infostream << "ClientMap::renderMap(): Rendering took >2s, " <<
					"returning." << std::endl;
			return;
		}

		if (!descriptor.m_reuse_material) {
			auto &material = buf->getMaterial();
			// pass the shadow map texture to the buffer texture
			ShadowRenderer *shadow = m_rendering_engine->get_shadow_renderer();
			if (shadow && shadow->is_active()) {
				auto &layer = material.TextureLayer[3];
				layer.Texture = shadow->get_texture();
				layer.TextureWrapU = video::E_TEXTURE_CLAMP::ETC_CLAMP_TO_EDGE;
				layer.TextureWrapV = video::E_TEXTURE_CLAMP::ETC_CLAMP_TO_EDGE;
				// Do not enable filter on shadow texture to avoid visual artifacts
				// with colored shadows.
				// Filtering is done in shader code anyway
				layer.TrilinearFilter = false;
			}
			driver->setMaterial(material);
			++material_swaps;
		}

		v3f block_wpos = intToFloat(descriptor.m_pos * MAP_BLOCKSIZE, BS);
		m.setTranslation(block_wpos - offset);

		driver->setTransform(video::ETS_WORLD, m);
		driver->drawMeshBuffer(buf);
		vertex_count += buf->getVertexCount();
	}

	g_profiler->avg(prefix + "draw meshes [ms]", draw.stop(true));

	// Log only on solid pass because values are the same
	if (pass == scene::ESNRP_SOLID) {
		g_profiler->avg("renderMap(): animated meshes [#]", mesh_animate_count);
	}

	if (pass == scene::ESNRP_TRANSPARENT) {
		g_profiler->avg("renderMap(): transparent buffers [#]", draw_order.size());
	}

<<<<<<< HEAD:src/clientmap.cpp
	g_profiler->avg("CM: PrimitiveDrawn", driver->getPrimitiveCountDrawn());

	/*infostream<<"renderMap(): is_transparent_pass="<<is_transparent_pass
			<<", rendered "<<vertex_count<<" vertices."<<std::endl;*/
=======
	g_profiler->avg(prefix + "vertices drawn [#]", vertex_count);
	g_profiler->avg(prefix + "drawcalls [#]", drawcall_count);
	g_profiler->avg(prefix + "material swaps [#]", material_swaps);
>>>>>>> 5.5.0:src/client/clientmap.cpp
}

static bool getVisibleBrightness(Map *map, const v3f &p0, v3f dir, float step,
	float step_multiplier, float start_distance, float end_distance,
	const NodeDefManager *ndef, u32 daylight_factor, float sunlight_min_d,
	int *result, bool *sunlight_seen)
{
	int brightness_sum = 0;
	int brightness_count = 0;
	float distance = start_distance;
	dir.normalize();
	v3f pf = p0;
	pf += dir * distance;
	int noncount = 0;
	bool nonlight_seen = false;
	bool allow_allowing_non_sunlight_propagates = false;
	bool allow_non_sunlight_propagates = false;
	// Check content nearly at camera position
	{
		v3s16 p = floatToInt(p0 /*+ dir * 3*BS*/, BS);
		MapNode n = map->getNode(p);
		if(ndef->get(n).param_type == CPT_LIGHT &&
				!ndef->get(n).sunlight_propagates)
			allow_allowing_non_sunlight_propagates = true;
	}
	// If would start at CONTENT_IGNORE, start closer
	{
		v3s16 p = floatToInt(pf, BS);
		MapNode n = map->getNode(p);
		if(n.getContent() == CONTENT_IGNORE){
			float newd = 2*BS;
			pf = p0 + dir * 2*newd;
			distance = newd;
			sunlight_min_d = 0;
		}
	}
	for (int i=0; distance < end_distance; i++) {
		pf += dir * step;
		distance += step;
		step *= step_multiplier;

		v3s16 p = floatToInt(pf, BS);
		MapNode n = map->getNode(p);
		if (allow_allowing_non_sunlight_propagates && i == 0 &&
				ndef->get(n).param_type == CPT_LIGHT &&
				!ndef->get(n).sunlight_propagates) {
			allow_non_sunlight_propagates = true;
		}

		if (ndef->get(n).param_type != CPT_LIGHT ||
				(!ndef->get(n).sunlight_propagates &&
					!allow_non_sunlight_propagates)){
			nonlight_seen = true;
			noncount++;
			if(noncount >= 4)
				break;
			continue;
		}

		if (distance >= sunlight_min_d && !*sunlight_seen && !nonlight_seen)
			if (n.getLight(LIGHTBANK_DAY, ndef) == LIGHT_SUN)
				*sunlight_seen = true;
		noncount = 0;
		brightness_sum += decode_light(n.getLightBlend(daylight_factor, ndef));
		brightness_count++;
	}
	*result = 0;
	if(brightness_count == 0)
		return false;
	*result = brightness_sum / brightness_count;
	/*std::cerr<<"Sampled "<<brightness_count<<" points; result="
			<<(*result)<<std::endl;*/
	return true;
}

int ClientMap::getBackgroundBrightness(float max_d, u32 daylight_factor,
		int oldvalue, bool *sunlight_seen_result)
{
	ScopeProfiler sp(g_profiler, "CM::getBackgroundBrightness", SPT_AVG);
	static v3f z_directions[50] = {
		v3f(-100, 0, 0)
	};
	static f32 z_offsets[50] = {
		-1000,
	};

	if (z_directions[0].X < -99) {
		for (u32 i = 0; i < ARRLEN(z_directions); i++) {
			// Assumes FOV of 72 and 16/9 aspect ratio
			z_directions[i] = v3f(
				0.02 * myrand_range(-100, 100),
				1.0,
				0.01 * myrand_range(-100, 100)
			).normalize();
			z_offsets[i] = 0.01 * myrand_range(0,100);
		}
	}

	int sunlight_seen_count = 0;
	float sunlight_min_d = max_d*0.8;
	if(sunlight_min_d > 35*BS)
		sunlight_min_d = 35*BS;
	std::vector<int> values;
	values.reserve(ARRLEN(z_directions));
	for (u32 i = 0; i < ARRLEN(z_directions); i++) {
		v3f z_dir = z_directions[i];
		core::CMatrix4<f32> a;
		a.buildRotateFromTo(v3f(0,1,0), z_dir);
		v3f dir = m_camera_direction;
		a.rotateVect(dir);
		int br = 0;
		float step = BS*1.5;
		if(max_d > 35*BS)
			step = max_d / 35 * 1.5;
		float off = step * z_offsets[i];
		bool sunlight_seen_now = false;
		bool ok = getVisibleBrightness(this, m_camera_position, dir,
				step, 1.0, max_d*0.6+off, max_d, m_nodedef, daylight_factor,
				sunlight_min_d,
				&br, &sunlight_seen_now);
		if(sunlight_seen_now)
			sunlight_seen_count++;
		if(!ok)
			continue;
		values.push_back(br);
		// Don't try too much if being in the sun is clear
		if(sunlight_seen_count >= 20)
			break;
	}
	int brightness_sum = 0;
	int brightness_count = 0;
	std::sort(values.begin(), values.end());
	u32 num_values_to_use = values.size();
	if(num_values_to_use >= 10)
		num_values_to_use -= num_values_to_use/2;
	else if(num_values_to_use >= 7)
		num_values_to_use -= num_values_to_use/3;
	u32 first_value_i = (values.size() - num_values_to_use) / 2;

	for (u32 i=first_value_i; i < first_value_i + num_values_to_use; i++) {
		brightness_sum += values[i];
		brightness_count++;
	}

	int ret = 0;
	if(brightness_count == 0){
		MapNode n = getNode(floatToInt(m_camera_position, BS));
		if(m_nodedef->get(n).param_type == CPT_LIGHT){
			ret = decode_light(n.getLightBlend(daylight_factor, m_nodedef));
		} else {
			ret = oldvalue;
		}
	} else {
		ret = brightness_sum / brightness_count;
	}

	*sunlight_seen_result = (sunlight_seen_count > 0);
	return ret;
}

void ClientMap::renderPostFx(CameraMode cam_mode)
{
	// Sadly ISceneManager has no "post effects" render pass, in that case we
	// could just register for that and handle it in renderMap().

	MapNode n = getNode(floatToInt(m_camera_position, BS));

	// - If the player is in a solid node, make everything black.
	// - If the player is in liquid, draw a semi-transparent overlay.
	// - Do not if player is in third person mode
	const ContentFeatures& features = m_nodedef->get(n);
	video::SColor post_effect_color = features.post_effect_color;
	if(features.solidness == 2 && !(g_settings->getBool("noclip") &&
			m_client->checkLocalPrivilege("noclip")) &&
			cam_mode == CAMERA_MODE_FIRST)
	{
		post_effect_color = video::SColor(255, 0, 0, 0);
	}
	if (post_effect_color.getAlpha() != 0)
	{
		// Draw a full-screen rectangle
		video::IVideoDriver* driver = SceneManager->getVideoDriver();
		v2u32 ss = driver->getScreenSize();
		core::rect<s32> rect(0,0, ss.X, ss.Y);
		driver->draw2DRectangle(post_effect_color, rect);
	}
}

void ClientMap::renderBlockBoundaries(const std::map<v3POS, MapBlock*> & blocks)
{
	video::IVideoDriver* driver = SceneManager->getVideoDriver();
	video::SMaterial mat;
	mat.Lighting = false;
	mat.ZWriteEnable = false;

	core::aabbox3d<f32> bound;
	//auto & blocks = *m_drawlist;
	const v3f inset(BS/2);
	const v3f blocksize(MAP_BLOCKSIZE);

	for (int pass = 0; pass < 2; ++pass) {
		video::SColor color_offset(0, 0, 0, 0);
		if (pass == 0) {
			mat.Thickness = 1;
			mat.ZBuffer = video::ECFN_ALWAYS;
			color_offset.setGreen(64);
		} else {
			mat.Thickness = 3;
			mat.ZBuffer = video::ECFN_LESSEQUAL;
		}
		driver->setMaterial(mat);

		for(auto i = blocks.begin(); i != blocks.end(); ++i) {
			video::SColor color(255, 0, 0, 0);
			if (i->second) {
				color.setBlue(255);
			} else {
				color.setRed(255);
				color.setGreen(128);
			}

			bound.MinEdge = intToFloat(i->first, BS)*blocksize
				+ inset
				- v3f(BS)*0.5
				- intToFloat(m_camera_offset, BS);
			bound.MaxEdge = bound.MinEdge
				+ blocksize*BS
				- inset
				- inset;
			color = color + color_offset;

			driver->draw3DBox(bound, color);
		}
	}
}


void ClientMap::PrintInfo(std::ostream &out)
{
	out<<"ClientMap: ";
}

void ClientMap::renderMapShadows(video::IVideoDriver *driver,
		const video::SMaterial &material, s32 pass, int frame, int total_frames)
{
	bool is_transparent_pass = pass != scene::ESNRP_SOLID;
	std::string prefix;
	if (is_transparent_pass)
		prefix = "renderMap(SHADOW TRANS): ";
	else
		prefix = "renderMap(SHADOW SOLID): ";

	u32 drawcall_count = 0;
	u32 vertex_count = 0;

	MeshBufListList drawbufs;

	int count = 0;
	int low_bound = is_transparent_pass ? 0 : m_drawlist_shadow.size() / total_frames * frame;
	int high_bound = is_transparent_pass ? m_drawlist_shadow.size() : m_drawlist_shadow.size() / total_frames * (frame + 1);

	// transparent pass should be rendered in one go
	if (is_transparent_pass && frame != total_frames - 1) {
		return;
	}

	for (auto &i : m_drawlist_shadow) {
		// only process specific part of the list & break early
		++count;
		if (count <= low_bound)
			continue;
		if (count > high_bound)
			break;

		v3s16 block_pos = i.first;
		MapBlock *block = i.second;

		// If the mesh of the block happened to get deleted, ignore it
		if (!block->mesh)
			continue;

		/*
			Get the meshbuffers of the block
		*/
		{
			MapBlockMesh *mapBlockMesh = block->mesh;
			assert(mapBlockMesh);

			for (int layer = 0; layer < MAX_TILE_LAYERS; layer++) {
				scene::IMesh *mesh = mapBlockMesh->getMesh(layer);
				assert(mesh);

				u32 c = mesh->getMeshBufferCount();
				for (u32 i = 0; i < c; i++) {
					scene::IMeshBuffer *buf = mesh->getMeshBuffer(i);

					video::SMaterial &mat = buf->getMaterial();
					auto rnd = driver->getMaterialRenderer(mat.MaterialType);
					bool transparent = rnd && rnd->isTransparent();
					if (transparent == is_transparent_pass)
						drawbufs.add(buf, block_pos, layer);
				}
			}
		}
	}

	TimeTaker draw("Drawing shadow mesh buffers");

	core::matrix4 m; // Model matrix
	v3f offset = intToFloat(m_camera_offset, BS);

	// Render all layers in order
	for (auto &lists : drawbufs.lists) {
		for (MeshBufList &list : lists) {
			// Check and abort if the machine is swapping a lot
			if (draw.getTimerTime() > 1000) {
				infostream << "ClientMap::renderMapShadows(): Rendering "
						"took >1s, returning." << std::endl;
				break;
			}
			for (auto &pair : list.bufs) {
				scene::IMeshBuffer *buf = pair.second;

				// override some material properties
				video::SMaterial local_material = buf->getMaterial();
				local_material.MaterialType = material.MaterialType;
				local_material.BackfaceCulling = material.BackfaceCulling;
				local_material.FrontfaceCulling = material.FrontfaceCulling;
				local_material.BlendOperation = material.BlendOperation;
				local_material.Lighting = false;
				driver->setMaterial(local_material);

				v3f block_wpos = intToFloat(pair.first * MAP_BLOCKSIZE, BS);
				m.setTranslation(block_wpos - offset);

				driver->setTransform(video::ETS_WORLD, m);
				driver->drawMeshBuffer(buf);
				vertex_count += buf->getVertexCount();
			}

			drawcall_count += list.bufs.size();
		}
	}

	// restore the driver material state 
	video::SMaterial clean;
	clean.BlendOperation = video::EBO_ADD;
	driver->setMaterial(clean); // reset material to defaults
	driver->draw3DLine(v3f(), v3f(), video::SColor(0));
	
	g_profiler->avg(prefix + "draw meshes [ms]", draw.stop(true));
	g_profiler->avg(prefix + "vertices drawn [#]", vertex_count);
	g_profiler->avg(prefix + "drawcalls [#]", drawcall_count);
}

/*
	Custom update draw list for the pov of shadow light.
*/
void ClientMap::updateDrawListShadow(const v3f &shadow_light_pos, const v3f &shadow_light_dir, float shadow_range)
{
	ScopeProfiler sp(g_profiler, "CM::updateDrawListShadow()", SPT_AVG);

	const v3f camera_position = shadow_light_pos;
	const v3f camera_direction = shadow_light_dir;
	// I "fake" fov just to avoid creating a new function to handle orthographic
	// projection.
	const f32 camera_fov = m_camera_fov * 1.9f;

	v3s16 cam_pos_nodes = floatToInt(camera_position, BS);
	v3s16 p_blocks_min;
	v3s16 p_blocks_max;
	getBlocksInViewRange(cam_pos_nodes, &p_blocks_min, &p_blocks_max, shadow_range);

	std::vector<v2s16> blocks_in_range;

	for (auto &i : m_drawlist_shadow) {
		MapBlock *block = i.second;
		block->refDrop();
	}
	m_drawlist_shadow.clear();

	// We need to append the blocks from the camera POV because sometimes
	// they are not inside the light frustum and it creates glitches.
	// FIXME: This could be removed if we figure out why they are missing
	// from the light frustum.
	for (auto &i : m_drawlist) {
		i.second->refGrab();
		m_drawlist_shadow[i.first] = i.second;
	}

	// Number of blocks currently loaded by the client
	u32 blocks_loaded = 0;
	// Number of blocks with mesh in rendering range
	u32 blocks_in_range_with_mesh = 0;
	// Number of blocks occlusion culled
	u32 blocks_occlusion_culled = 0;

	for (auto &sector_it : m_sectors) {
		MapSector *sector = sector_it.second;
		if (!sector)
			continue;
		blocks_loaded += sector->size();

		MapBlockVect sectorblocks;
		sector->getBlocks(sectorblocks);

		/*
			Loop through blocks in sector
		*/
		for (MapBlock *block : sectorblocks) {
			if (!block->mesh) {
				// Ignore if mesh doesn't exist
				continue;
			}

			float range = shadow_range;

			float d = 0.0;
			if (!isBlockInSight(block->getPos(), camera_position,
					    camera_direction, camera_fov, range, &d))
				continue;

			blocks_in_range_with_mesh++;

			/*
				Occlusion culling
			*/
			if (isBlockOccluded(block, cam_pos_nodes)) {
				blocks_occlusion_culled++;
				continue;
			}

			// This block is in range. Reset usage timer.
			block->resetUsageTimer();

			// Add to set
			if (m_drawlist_shadow.find(block->getPos()) == m_drawlist_shadow.end()) {
				block->refGrab();
				m_drawlist_shadow[block->getPos()] = block;
			}
		}
	}

	g_profiler->avg("SHADOW MapBlock meshes in range [#]", blocks_in_range_with_mesh);
	g_profiler->avg("SHADOW MapBlocks occlusion culled [#]", blocks_occlusion_culled);
	g_profiler->avg("SHADOW MapBlocks drawn [#]", m_drawlist_shadow.size());
	g_profiler->avg("SHADOW MapBlocks loaded [#]", blocks_loaded);
}
