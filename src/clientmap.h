/*
clientmap.h
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

#ifndef CLIENTMAP_HEADER
#define CLIENTMAP_HEADER

#include "irrlichttypes_extrabloated.h"
#include "map.h"
#include "camera.h"
#include <set>
#include <unordered_set>
#include <vector>
#include <map>

struct MapDrawControl
{
	MapDrawControl();
	// Overrides limits by drawing everything
	bool range_all;
	// Wanted drawing range
	float wanted_range;
	// Maximum number of blocks to draw
	u32 wanted_max_blocks;
	// Blocks in this range are drawn regardless of number of blocks drawn
	float wanted_min_range;
	// Number of blocks rendered is written here by the renderer
	u32 blocks_drawn;
	// Number of blocks that would have been drawn in wanted_range
	u32 blocks_would_have_drawn;
	// Distance to the farthest block drawn
	float farthest_drawn;

	float farmesh;
	int farmesh_step;

	float fps;
	float fps_avg;
	float fps_wanted;
	float drawtime_avg;

	float fov;
	bool block_overflow;
};

class Client;
class ITextureSource;

/*
	ClientMap
	
	This is the only map class that is able to render itself on screen.
*/

class ClientMap : public Map, public scene::ISceneNode
{
public:
	ClientMap(
			Client *client,
			IGameDef *gamedef,
			MapDrawControl &control,
			scene::ISceneNode* parent,
			scene::ISceneManager* mgr,
			s32 id
	);

	~ClientMap();

	s32 mapType() const
	{
		return MAPTYPE_CLIENT;
	}

	void drop()
	{
		ISceneNode::drop();
	}

	void updateCamera(v3f pos, v3f dir, f32 fov, v3s16 offset)
	{
		JMutexAutoLock lock(m_camera_mutex);
		m_camera_position = pos;
		m_camera_direction = dir;
		m_camera_fov = fov;
		m_camera_offset = offset;
	}

	//void deSerializeSector(v2s16 p2d, std::istream &is);

	/*
		ISceneNode methods
	*/

	virtual void OnRegisterSceneNode();

	virtual void render()
	{
		video::IVideoDriver* driver = SceneManager->getVideoDriver();
		if (driver->getDriverType() != video::EDT_NULL) {
			driver->setTransform(video::ETS_WORLD, AbsoluteTransformation);
			renderMap(driver, SceneManager->getSceneNodeRenderPass());
		}
	}
	
	virtual const core::aabbox3d<f32>& getBoundingBox() const
	{
		return m_box;
	}
	
	void updateDrawList(video::IVideoDriver* driver, float dtime, unsigned int max_cycle_ms = 0);
	void renderMap(video::IVideoDriver* driver, s32 pass);

	int getBackgroundBrightness(float max_d, u32 daylight_factor,
			int oldvalue, bool *sunlight_seen_result);

	void renderPostFx(CameraMode cam_mode);

	// For debugging the status and position of MapBlocks
	void renderBlockBoundaries(const std::map<v3POS, MapBlock*> & blocks);

	// For debug printing
	virtual void PrintInfo(std::ostream &out);
	
	MapDrawControl & getControl() { return m_control; }

private:
	Client *m_client;
	
	core::aabbox3d<f32> m_box;
	
	MapDrawControl &m_control;

	v3f m_camera_position;
	v3f m_camera_direction;
	f32 m_camera_fov;
	v3s16 m_camera_offset;
	JMutex m_camera_mutex;

	std::atomic<concurrent_unordered_map<v3POS, MapBlockP, v3POSHash, v3POSEqual> *> m_drawlist;
	concurrent_unordered_map<v3POS, MapBlockP, v3POSHash, v3POSEqual> m_drawlist_0;
	concurrent_unordered_map<v3POS, MapBlockP, v3POSHash, v3POSEqual> m_drawlist_1;
	int m_drawlist_current;
	std::vector<std::pair<v3POS, int>> draw_nearest;
public:
	std::atomic_uint m_drawlist_last;
	std::map<v3POS, MapBlock*> m_block_boundary;
private:

	bool m_cache_trilinear_filter;
	bool m_cache_bilinear_filter;
	bool m_cache_anistropic_filter;
};

#endif

