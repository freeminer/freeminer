/*
Minetest
Copyright (C) 2010-2015 celeron55, Perttu Ahola <celeron55@gmail.com>

This program is free software; you can redistribute it and/or modify
it under the terms of the GNU Lesser General Public License as published by
the Free Software Foundation; either version 2.1 of the License, or
(at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU Lesser General Public License for more details.

You should have received a copy of the GNU Lesser General Public License along
with this program; if not, write to the Free Software Foundation, Inc.,
51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
*/

#ifndef MINIMAP_HEADER
#define MINIMAP_HEADER

#include "irrlichttypes_extrabloated.h"
#include "client.h"
#include "voxel.h"
#include "threading/mutex.h"
#include "threading/semaphore.h"
#include <map>
#include <string>
#include <vector>
#include "camera.h"

#include "util/unordered_map_hash.h"

#define MINIMAP_MAX_SX 512
#define MINIMAP_MAX_SY 512


enum MinimapMode {
	MINIMAP_MODE_OFF,
	MINIMAP_MODE_SURFACEx1,
	MINIMAP_MODE_SURFACEx2,
	MINIMAP_MODE_SURFACEx4,
	MINIMAP_MODE_RADARx1,
	MINIMAP_MODE_RADARx2,
	MINIMAP_MODE_RADARx4,
	MINIMAP_MODE_COUNT,
};

struct MinimapModeDef {
	bool is_radar;
	u16 scan_height;
	u16 map_size;
};

struct MinimapPixel {
	u16 id;
	u16 height;
	u16 air_count;
	u16 light;
};

struct MinimapMapblock {
	void getMinimapNodes(VoxelManipulator *vmanip, v3s16 pos);

	MinimapPixel data[MAP_BLOCKSIZE * MAP_BLOCKSIZE];
};

struct MinimapData {
	bool is_radar;
	MinimapMode mode;
	v3s16 pos;
	v3s16 old_pos;
	u16 scan_height;
	u16 map_size;
	MinimapPixel minimap_scan[MINIMAP_MAX_SX * MINIMAP_MAX_SY];
	std::atomic_bool map_invalidated;
	bool minimap_shape_round;
	video::IImage *minimap_image;
	video::IImage *heightmap_image;
	video::IImage *minimap_mask_round;
	video::IImage *minimap_mask_square;
	video::ITexture *texture;
	video::ITexture *heightmap_texture;
	video::ITexture *minimap_overlay_round;
	video::ITexture *minimap_overlay_square;
	video::ITexture *player_marker;
	Mutex m_mutex;
	video::ITexture *object_marker_red;
};

struct QueuedMinimapUpdate {
	v3s16 pos;
	MinimapMapblock *data;
};

class MinimapUpdateThread : public UpdateThread {
public:
	MinimapUpdateThread() : UpdateThread("Minimap") { next_update = 0; }
	virtual ~MinimapUpdateThread();

	void getMap(v3s16 pos, s16 size, s16 height, bool radar);
	MinimapPixel *getMinimapPixel(v3s16 pos, s16 height, s16 *pixel_height);
	s16 getAirCount(v3s16 pos, s16 height);
	video::SColor getColorFromId(u16 id);

	void enqueueBlock(v3s16 pos, MinimapMapblock *data);

	bool pushBlockUpdate(v3s16 pos, MinimapMapblock *data);
	bool popBlockUpdate(QueuedMinimapUpdate *update);

	MinimapData *data;
	u32 next_update;

protected:
	virtual void doUpdate();

private:
	Mutex m_queue_mutex;
	std::deque<QueuedMinimapUpdate> m_update_queue;
	unordered_map_v3POS<MinimapMapblock *> m_blocks_cache;
	//simple: unordered_map_v2POS<std::vector<MinimapMapblock*>> getmap_cache
	unordered_map_v2POS<std::map<POS, MinimapMapblock*>> getmap_cache;
};

class Mapper {
public:
	Mapper(IrrlichtDevice *device, Client *client);
	~Mapper();

	void addBlock(v3s16 pos, MinimapMapblock *data);

	v3f getYawVec();
	MinimapMode getMinimapMode();

	void setPos(v3s16 pos);
	void setAngle(f32 angle);
	void setMinimapMode(MinimapMode mode);
	void toggleMinimapShape();


	video::ITexture *getMinimapTexture();

	void blitMinimapPixelsToImageRadar(video::IImage *map_image);
	void blitMinimapPixelsToImageSurface(video::IImage *map_image,
		video::IImage *heightmap_image);

	scene::SMeshBuffer *getMinimapMeshBuffer();

	void updateActiveMarkers();
	void drawMinimap();

	video::IVideoDriver *driver;
	Client* client;
	MinimapData *data;

private:
	ITextureSource *m_tsrc;
	IShaderSource *m_shdrsrc;
	INodeDefManager *m_ndef;
	MinimapUpdateThread *m_minimap_update_thread;
	scene::SMeshBuffer *m_meshbuffer;
	bool m_enable_shaders;
	u16 m_surface_mode_scan_height;
	f32 m_angle;
/*
	Mutex m_mutex;
*/
	std::list<v2f> m_active_markers;
};

#endif
