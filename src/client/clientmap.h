// Luanti
// SPDX-License-Identifier: LGPL-2.1-or-later
// Copyright (C) 2010-2013 celeron55, Perttu Ahola <celeron55@gmail.com>

#pragma once

#include "CMeshBuffer.h"
#include "fm_weather.h"
#include "threading/async.h"
#include "settings.h"

#include "irrlichttypes_bloated.h"
#include "map.h"
#include <ISceneNode.h>
#include <map>
#include <functional>
#include <mutex>
#include <unordered_map>

struct MapDrawControl
{

	// freeminer:
	int32_t farmesh{30000};
	uint8_t farmesh_quality{};
	uint16_t farmesh_stable{};
	pos_t farmesh_all_changed{};
	int32_t lodmesh{4};
	int cell_size{1};
	uint8_t cell_size_pow{};
	uint8_t farmesh_quality_pow{};

	float fps{30};
	float fps_avg{30};
	float fps_wanted{30};
	float drawtime_avg{30};

	float fov{180};
	float fov_add{};
	float fov_want{180}; // smooth change

	float farthest_drawn{};
	bool enable_fog = g_settings->getBool("enable_fog");

	void fm_init();
	void registerSettingsCallbacks();
	void onSettingChanged(const std::string &name);
	MapDrawControl() { fm_init(); }
	// == 


	// Wanted drawing range
	std::atomic_int32_t wanted_range = 0.0f;
	// Overrides limits by drawing everything
	bool range_all = false;
	// Allow rendering out of bounds
	bool allow_noclip = false;
	// show a wire frame for debugging
	bool show_wireframe = false;
};

class Client;
class RenderingEngine;

enum CameraMode : int;

namespace scene
{
	class IMeshBuffer;
}

namespace video
{
	class IVideoDriver;
}

struct CachedMeshBuffer {
	std::vector<scene::IMeshBuffer*> buf;
	u8 age = 0;

	void drop();
};

using CachedMeshBuffers = std::unordered_map<std::string, CachedMeshBuffer>;

using ModifyMaterialCallback = std::function<void(video::SMaterial& /* material */, bool /* is_foliage */)>;

/*
	ClientMap

	This is the only map class that is able to render itself on screen.
*/

class ClientMap : public Map, public scene::ISceneNode
{
public:
	ClientMap(
			Client *client,
			RenderingEngine *rendering_engine,
			MapDrawControl &control,
			s32 id
	);

	bool maySaveBlocks() override
	{
		return false;
	}

	void updateCamera(v3f pos, v3f dir, f32 fov, v3s16 offset, video::SColor light_color);

	/*
		Forcefully get a sector from somewhere
	*/
	//MapSector * emergeSector(v2s16 p) override;

	/*
		ISceneNode methods
	*/

	virtual void OnRegisterSceneNode() override;

	virtual void render() override;

	virtual const aabb3f &getBoundingBox() const override
	{
		return m_box;
	}

	void getBlocksInViewRange(v3s16 cam_pos_nodes,
		v3s16 *p_blocks_min, v3s16 *p_blocks_max, float range=-1.0f);

    void updateDrawList(float dtime, unsigned int max_cycle_ms = 0);
	void updateDrawListFm(float dtime, unsigned int max_cycle_ms = 0);

	//void updateDrawList();
	/// @brief clears m_drawlist and m_keeplist
	void clearDrawList();

	/// @brief Calculate statistics about the map and keep the blocks alive
	void touchMapBlocks();

	void updateDrawListShadow(v3f shadow_light_pos, v3f shadow_light_dir, float radius, float length);
	void clearDrawListShadow();

	// Returns true if draw list needs updating before drawing the next frame.
	bool needsUpdateDrawList() { return m_needs_update_drawlist; }

	void renderMap(video::IVideoDriver* driver, s32 pass);

	void renderMapShadows(video::IVideoDriver *driver,
			ModifyMaterialCallback cb, s32 pass, int frame, int total_frames);

	int getBackgroundBrightness(float max_d, u32 daylight_factor,
			int oldvalue, bool *sunlight_seen_result);

	void renderPostFx(CameraMode cam_mode);

	void invalidateMapBlockMesh(MapBlockMesh *mesh);

	// For debug printing
	void PrintInfo(std::ostream &out) override;

	MapDrawControl & getControl() const { return m_control; }
	f32 getWantedRange() const { return m_control.wanted_range; }
	f32 getCameraFov() const { return m_camera_fov; }

	void onSettingChanged(std::string_view name, bool all);

protected:
	// use drop() instead
	virtual ~ClientMap();

	void reportMetrics(u64 save_time_us, u32 saved_blocks, u32 all_blocks) override;
private:
	bool isMeshOccluded(MapBlock *mesh_block, u16 mesh_size, v3s16 cam_pos_nodes);

	// update the vertex order in transparent mesh buffers
	void updateTransparentMeshBuffers();

	// Orders blocks by distance to the camera
	class MapBlockComparer
	{
	public:
		MapBlockComparer(const v3s16 &camera_block) : m_camera_block(camera_block) {}

		bool operator() (const v3s16 &left, const v3s16 &right) const
		{
			auto distance_left = left.getDistanceFromSQ(m_camera_block);
			auto distance_right = right.getDistanceFromSQ(m_camera_block);
			return distance_left > distance_right || (distance_left == distance_right && left > right);
		}

	private:
		v3s16 m_camera_block;
	};

	Client *m_client;
	RenderingEngine *m_rendering_engine;

	aabb3f m_box = aabb3f(-BS * 1000000, -BS * 1000000, -BS * 1000000,
		BS * 1000000, BS * 1000000, BS * 1000000);

	MapDrawControl &m_control;

	v3f m_camera_position = v3f(0,0,0);
	v3f m_camera_direction = v3f(0,0,1);
	f32 m_camera_fov = M_PI;
	v3s16 m_camera_offset;
	video::SColor m_camera_light_color = video::SColor(0xFFFFFFFF);
	bool m_needs_update_transparent_meshes = true;


// fm:
public:
	static irr_ptr<ClientMap> create(Client *client, RenderingEngine *rendering_engine,
			MapDrawControl &control, s32 id);
private:
	v3pos_t m_camera_position_node;
    using drawlist_map = std::map<v3bpos_t, MapBlockPtr, MapBlockComparer>;
	drawlist_map m_drawlist_0, m_drawlist_1;
	std::atomic_bool m_drawlist_current = false;
	std::recursive_mutex m_drawlist_mutex;
    using drawlist_shadow_map = std::map<v3bpos_t, MapBlockPtr>;
	drawlist_shadow_map m_drawlist_shadow_0, m_drawlist_shadow_1;
	std::atomic_bool m_drawlist_shadow_current = false;
public:
    async_step_runner update_drawlist_async;
    async_step_runner update_shadows_async;
	std::map<v3pos_t, MapBlock*> m_block_boundary;
	void cleanPerodic(uint32_t uptime);
private:

	void initFarFogMaterial();
	void updateFarFogCells();
	u32 rebuildFarFogMeshBuffer();
	u32 renderFarFog(video::IVideoDriver *driver);

	bool m_far_fog_material_ready = false;
	video::SMaterial m_far_fog_material;
	std::vector<irr_ptr<scene::SMeshBuffer>> m_far_fog_meshbuffers;
	struct FarFogCell {
		v3bpos_t block_pos;
		block_step_t step = 0;
		bpos_t block_span = 0;
		float distance = 0.0f;
		MapBlockPtr block;
		bool has_climate = false;
		float heat = 0.0f;
		float humidity = 0.0f;
		float terrain_y = 0.0f;
		weather::wind_t wind;
	};
	std::array<std::vector<FarFogCell>, 2> m_far_fog_cells;
	std::mutex m_far_fog_cells_mutex;
	std::unordered_map<std::size_t, float> m_far_fog_terrain_cache;
	std::mutex m_far_fog_terrain_cache_mutex;
	std::atomic_uint8_t m_far_fog_cells_current = 0;
	std::atomic_bool m_far_fog_cells_ready = false;
	v3bpos_t m_far_fog_cells_origin;
	uint32_t m_far_fog_cells_iteration_draw = 0;
	async_step_runner m_far_fog_async;
	bool m_far_fog_mesh_valid = false;
	bool m_far_fog_mesh_cells_ready = false;
	uint8_t m_far_fog_mesh_cells_current = 0;
	v3pos_t m_far_fog_mesh_camera_bucket;
	v3pos_t m_far_fog_mesh_camera_offset;
	v3f m_far_fog_mesh_camera_direction = v3f(0.0f, 0.0f, 1.0f);
	uint16_t m_far_fog_mesh_time_bucket = 0;
	uint32_t m_far_fog_mesh_iteration_draw = 0;
// ===

	//std::map<v3bpos_t, MapBlock*, MapBlockComparer> m_drawlist;
	// List of additional blocks to keep (relevant with mesh_chunk > 1, since
	// not all blocks contain a mesh)
	std::vector<MapBlockPtr> m_keeplist;
/*
	std::map<v3s16, MapBlock*> m_drawlist_shadow;
*/	
	bool m_needs_update_drawlist;
	CachedMeshBuffers m_dynamic_buffers;

	bool m_cache_trilinear_filter;
	bool m_cache_bilinear_filter;
	bool m_cache_anistropic_filter;
	bool m_cache_transparency_sorting_group_by_buffers;
	u16 m_cache_transparency_sorting_distance;

	bool m_loops_occlusion_culler;
	bool m_enable_raytraced_culling;
};

bool isOccluded(Map *map, v3pos_t p0, v3pos_t p1, float step, float stepfac,
		float start_off, float end_off, u32 needed_count, NodeDefManager *nodemgr,
		unordered_map_v3pos<bool> & occlude_cache);
