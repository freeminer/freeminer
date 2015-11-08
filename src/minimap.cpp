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

#include "minimap.h"
#include "threading/mutex_auto_lock.h"
#include "threading/semaphore.h"
#include "clientmap.h"
#include "settings.h"
#include "nodedef.h"
#include "porting.h"
#include "util/numeric.h"
#include "util/string.h"
#include <math.h>


////
//// MinimapUpdateThread
////

MinimapUpdateThread::~MinimapUpdateThread()
{
	for (std::map<v3s16, MinimapMapblock *>::iterator
			it = m_blocks_cache.begin();
			it != m_blocks_cache.end(); ++it) {
		delete it->second;
	}

	for (std::deque<QueuedMinimapUpdate>::iterator
			it = m_update_queue.begin();
			it != m_update_queue.end(); ++it) {
		QueuedMinimapUpdate &q = *it;
		delete q.data;
	}
}

bool MinimapUpdateThread::pushBlockUpdate(v3s16 pos, MinimapMapblock *data)
{
	MutexAutoLock lock(m_queue_mutex);

	// Find if block is already in queue.
	// If it is, update the data and quit.
	for (std::deque<QueuedMinimapUpdate>::iterator
			it = m_update_queue.begin();
			it != m_update_queue.end(); ++it) {
		QueuedMinimapUpdate &q = *it;
		if (q.pos == pos) {
			delete q.data;
			q.data = data;
			return false;
		}
	}

	// Add the block
	QueuedMinimapUpdate q;
	q.pos  = pos;
	q.data = data;
	m_update_queue.push_back(q);

	return true;
}

bool MinimapUpdateThread::popBlockUpdate(QueuedMinimapUpdate *update)
{
	MutexAutoLock lock(m_queue_mutex);

	if (m_update_queue.empty())
		return false;

	*update = m_update_queue.front();
	m_update_queue.pop_front();

	return true;
}

void MinimapUpdateThread::enqueueBlock(v3s16 pos, MinimapMapblock *data)
{
	pushBlockUpdate(pos, data);
	deferUpdate();
}


void MinimapUpdateThread::doUpdate()
{
	QueuedMinimapUpdate update;

	while (popBlockUpdate(&update)) {
		if (update.data) {
			// Swap two values in the map using single lookup
			std::pair<std::map<v3s16, MinimapMapblock*>::iterator, bool>
			    result = m_blocks_cache.insert(std::make_pair(update.pos, update.data));
			if (result.second == false) {
				delete result.first->second;
				result.first->second = update.data;
			}
		} else {
			std::map<v3s16, MinimapMapblock *>::iterator it;
			it = m_blocks_cache.find(update.pos);
			if (it != m_blocks_cache.end()) {
				delete it->second;
				m_blocks_cache.erase(it);
			}
		}
	}

	bool do_update;
	{
		MutexAutoLock lock(data->m_mutex);
		do_update = data->map_invalidated && data->mode != MINIMAP_MODE_OFF;
	}
	if (do_update) {
		getMap(data->pos, data->map_size, data->scan_height, data->is_radar);
		data->map_invalidated = false;
	}
}

MinimapPixel *MinimapUpdateThread::getMinimapPixel(v3s16 pos,
	s16 scan_height, s16 *pixel_height)
{
	s16 height = scan_height - MAP_BLOCKSIZE;
	v3s16 blockpos_max, blockpos_min, relpos;

	getNodeBlockPosWithOffset(
		v3s16(pos.X, pos.Y - scan_height / 2, pos.Z),
		blockpos_min, relpos);
	getNodeBlockPosWithOffset(
		v3s16(pos.X, pos.Y + scan_height / 2, pos.Z),
		blockpos_max, relpos);

	for (s16 i = blockpos_max.Y; i > blockpos_min.Y - 1; i--) {
		std::map<v3s16, MinimapMapblock *>::iterator it =
			m_blocks_cache.find(v3s16(blockpos_max.X, i, blockpos_max.Z));
		if (it != m_blocks_cache.end()) {
			MinimapMapblock *mmblock = it->second;
			MinimapPixel *pixel = &mmblock->data[relpos.Z * MAP_BLOCKSIZE + relpos.X];
			if (pixel->id != CONTENT_AIR) {
				*pixel_height = height + pixel->height;
				return pixel;
			}
		}

		height -= MAP_BLOCKSIZE;
	}

	return NULL;
}

s16 MinimapUpdateThread::getAirCount(v3s16 pos, s16 height)
{
	s16 air_count = 0;
	v3s16 blockpos_max, blockpos_min, relpos;

	getNodeBlockPosWithOffset(
		v3s16(pos.X, pos.Y - height / 2, pos.Z),
		blockpos_min, relpos);
	getNodeBlockPosWithOffset(
		v3s16(pos.X, pos.Y + height / 2, pos.Z),
		blockpos_max, relpos);

	for (s16 i = blockpos_max.Y; i > blockpos_min.Y - 1; i--) {
		std::map<v3s16, MinimapMapblock *>::iterator it =
			m_blocks_cache.find(v3s16(blockpos_max.X, i, blockpos_max.Z));
		if (it != m_blocks_cache.end()) {
			MinimapMapblock *mmblock = it->second;
			MinimapPixel *pixel = &mmblock->data[relpos.Z * MAP_BLOCKSIZE + relpos.X];
			air_count += pixel->air_count;
		}
	}

	return air_count;
}

void MinimapUpdateThread::getMap(v3s16 pos, s16 size, s16 height, bool is_radar)
{
	v3s16 p = v3s16(pos.X - size / 2, pos.Y, pos.Z - size / 2);

	for (s16 x = 0; x < size; x++)
	for (s16 z = 0; z < size; z++) {
		u16 id = CONTENT_AIR;
		MinimapPixel *mmpixel = &data->minimap_scan[x + z * size];

		if (!is_radar) {
			s16 pixel_height = 0;
			MinimapPixel *cached_pixel =
				getMinimapPixel(v3s16(p.X + x, p.Y, p.Z + z), height, &pixel_height);
			if (cached_pixel) {
				id = cached_pixel->id;
				mmpixel->height = pixel_height;
			}
		} else {
			mmpixel->air_count = getAirCount(v3s16(p.X + x, p.Y, p.Z + z), height);
		}

		mmpixel->id = id;
	}
}

////
//// Mapper
////

Mapper::Mapper(IrrlichtDevice *device, Client *client)
{
	this->driver    = device->getVideoDriver();
	this->m_tsrc    = client->getTextureSource();
	this->m_shdrsrc = client->getShaderSource();
	this->m_ndef    = client->getNodeDefManager();

	// Initialize static settings
	m_enable_shaders = g_settings->getBool("enable_shaders");
	m_surface_mode_scan_height =
		g_settings->getBool("minimap_double_scan_height") ? 256 : 128;

	setAngle(0);

	// Initialize minimap data
	data = new MinimapData;
	data->mode              = MINIMAP_MODE_OFF;
	data->is_radar          = false;
	data->map_invalidated   = true;
	data->heightmap_image   = NULL;
	data->minimap_image     = NULL;
	data->texture           = NULL;
	data->heightmap_texture = NULL;
	data->minimap_shape_round = g_settings->getBool("minimap_shape_round");

	// Get round minimap textures
	data->minimap_mask_round = driver->createImage(
		m_tsrc->getTexture("minimap_mask_round.png"),
		core::position2d<s32>(0, 0),
		core::dimension2d<u32>(MINIMAP_MAX_SX, MINIMAP_MAX_SY));
	data->minimap_overlay_round = m_tsrc->getTexture("minimap_overlay_round.png");

	// Get square minimap textures
	data->minimap_mask_square = driver->createImage(
		m_tsrc->getTexture("minimap_mask_square.png"),
		core::position2d<s32>(0, 0),
		core::dimension2d<u32>(MINIMAP_MAX_SX, MINIMAP_MAX_SY));
	data->minimap_overlay_square = m_tsrc->getTexture("minimap_overlay_square.png");

	// Create player marker texture
	data->player_marker = m_tsrc->getTexture("player_marker.png");

	// Create mesh buffer for minimap
	m_meshbuffer = getMinimapMeshBuffer();

	// Initialize and start thread
	m_minimap_update_thread = new MinimapUpdateThread();
	m_minimap_update_thread->data = data;
	m_minimap_update_thread->start();
}

Mapper::~Mapper()
{
	m_minimap_update_thread->stop();
	m_minimap_update_thread->wait();

	m_meshbuffer->drop();

	if (data) {
	if (data->minimap_mask_round)
	data->minimap_mask_round->drop();
	if (data->minimap_mask_square)
	data->minimap_mask_square->drop();

	if (data->texture)
	driver->removeTexture(data->texture);
	if (data->heightmap_texture)
	driver->removeTexture(data->heightmap_texture);
	if (data->minimap_overlay_round)
	driver->removeTexture(data->minimap_overlay_round);
	if (data->minimap_overlay_square)
	driver->removeTexture(data->minimap_overlay_square);

	delete data;
	}
	delete m_minimap_update_thread;
}

void Mapper::addBlock(v3s16 pos, MinimapMapblock *data)
{
	m_minimap_update_thread->enqueueBlock(pos, data);
}

MinimapMode Mapper::getMinimapMode()
{
	return data->mode;
}

void Mapper::toggleMinimapShape()
{
	MutexAutoLock lock(data->m_mutex);

	data->minimap_shape_round = !data->minimap_shape_round;
	g_settings->setBool("minimap_shape_round", data->minimap_shape_round);
	m_minimap_update_thread->deferUpdate();
}

void Mapper::setMinimapMode(MinimapMode mode)
{
	static const MinimapModeDef modedefs[MINIMAP_MODE_COUNT] = {
		{false, 0, 0},
		{false, m_surface_mode_scan_height, 256},
		{false, m_surface_mode_scan_height, 128},
		{false, m_surface_mode_scan_height, 64},
		{true, 32, 128},
		{true, 32, 64},
		{true, 32, 32}
	};

	if (mode >= MINIMAP_MODE_COUNT)
		return;

	MutexAutoLock lock(data->m_mutex);

	data->is_radar    = modedefs[mode].is_radar;
	data->scan_height = modedefs[mode].scan_height;
	data->map_size    = modedefs[mode].map_size;
	data->mode        = mode;

	m_minimap_update_thread->deferUpdate();
}

void Mapper::setPos(v3s16 pos)
{
	bool do_update = false;

	{
		MutexAutoLock lock(data->m_mutex);

		if (pos != data->old_pos) {
			data->old_pos = data->pos;
			data->pos = pos;
			do_update = true;
		}
	}

	if (do_update)
		m_minimap_update_thread->deferUpdate();
}

void Mapper::setAngle(f32 angle)
{
	m_angle = angle;
}

void Mapper::blitMinimapPixelsToImageRadar(video::IImage *map_image)
{
	for (s16 x = 0; x < data->map_size; x++)
	for (s16 z = 0; z < data->map_size; z++) {
		MinimapPixel *mmpixel = &data->minimap_scan[x + z * data->map_size];

		video::SColor c(240, 0, 0, 0);
		if (mmpixel->air_count > 0)
			c.setGreen(core::clamp(core::round32(32 + mmpixel->air_count * 8), 0, 255));

		map_image->setPixel(x, data->map_size - z - 1, c);
	}
}

void Mapper::blitMinimapPixelsToImageSurface(
	video::IImage *map_image, video::IImage *heightmap_image)
{
	for (s16 x = 0; x < data->map_size; x++)
	for (s16 z = 0; z < data->map_size; z++) {
		MinimapPixel *mmpixel = &data->minimap_scan[x + z * data->map_size];

		video::SColor c = m_ndef->get(mmpixel->id).minimap_color;
		c.setAlpha(240);

		map_image->setPixel(x, data->map_size - z - 1, c);

		u32 h = mmpixel->height;
		heightmap_image->setPixel(x,data->map_size - z - 1,
			video::SColor(255, h, h, h));
	}
}

video::ITexture *Mapper::getMinimapTexture()
{
	// update minimap textures when new scan is ready
	if (data->map_invalidated)
		return data->texture;

	// create minimap and heightmap images in memory
	core::dimension2d<u32> dim(data->map_size, data->map_size);
	video::IImage *map_image       = driver->createImage(video::ECF_A8R8G8B8, dim);
	video::IImage *heightmap_image = driver->createImage(video::ECF_A8R8G8B8, dim);
	video::IImage *minimap_image   = driver->createImage(video::ECF_A8R8G8B8,
		core::dimension2d<u32>(MINIMAP_MAX_SX, MINIMAP_MAX_SY));

	// Blit MinimapPixels to images
	if (data->is_radar)
		blitMinimapPixelsToImageRadar(map_image);
	else
		blitMinimapPixelsToImageSurface(map_image, heightmap_image);

	map_image->copyToScaling(minimap_image);
	map_image->drop();

	video::IImage *minimap_mask = data->minimap_shape_round ?
		data->minimap_mask_round : data->minimap_mask_square;

	if (minimap_mask) {
		for (s16 y = 0; y < MINIMAP_MAX_SY; y++)
		for (s16 x = 0; x < MINIMAP_MAX_SX; x++) {
			video::SColor mask_col = minimap_mask->getPixel(x, y);
			if (!mask_col.getAlpha())
				minimap_image->setPixel(x, y, video::SColor(0,0,0,0));
		}
	}

	if (data->texture)
		driver->removeTexture(data->texture);
	if (data->heightmap_texture)
		driver->removeTexture(data->heightmap_texture);

	data->texture = driver->addTexture("minimap__", minimap_image);
	data->heightmap_texture =
		driver->addTexture("minimap_heightmap__", heightmap_image);
	minimap_image->drop();
	heightmap_image->drop();

	data->map_invalidated = true;

	return data->texture;
}

v3f Mapper::getYawVec()
{
	if (data->minimap_shape_round) {
		return v3f(
			cos(m_angle * core::DEGTORAD),
			sin(m_angle * core::DEGTORAD),
			1.0);
	} else {
		return v3f(1.0, 0.0, 1.0);
	}
}

scene::SMeshBuffer *Mapper::getMinimapMeshBuffer()
{
	scene::SMeshBuffer *buf = new scene::SMeshBuffer();
	buf->Vertices.set_used(4);
	buf->Indices.set_used(6);
	video::SColor c(255, 255, 255, 255);

	buf->Vertices[0] = video::S3DVertex(-1, -1, 0, 0, 0, 1, c, 0, 1);
	buf->Vertices[1] = video::S3DVertex(-1,  1, 0, 0, 0, 1, c, 0, 0);
	buf->Vertices[2] = video::S3DVertex( 1,  1, 0, 0, 0, 1, c, 1, 0);
	buf->Vertices[3] = video::S3DVertex( 1, -1, 0, 0, 0, 1, c, 1, 1);

	buf->Indices[0] = 0;
	buf->Indices[1] = 1;
	buf->Indices[2] = 2;
	buf->Indices[3] = 2;
	buf->Indices[4] = 3;
	buf->Indices[5] = 0;

	return buf;
}

void Mapper::drawMinimap()
{
	video::ITexture *minimap_texture = getMinimapTexture();
	if (!minimap_texture)
		return;

	v2u32 screensize = porting::getWindowSize();
	const u32 size = 0.25 * screensize.Y;

	core::rect<s32> oldViewPort = driver->getViewPort();
	core::matrix4 oldProjMat = driver->getTransform(video::ETS_PROJECTION);
	core::matrix4 oldViewMat = driver->getTransform(video::ETS_VIEW);

	driver->setViewPort(core::rect<s32>(
		screensize.X - size - 10, 10,
		screensize.X - 10, size + 10));
	driver->setTransform(video::ETS_PROJECTION, core::matrix4());
	driver->setTransform(video::ETS_VIEW, core::matrix4());

	core::matrix4 matrix;
	matrix.makeIdentity();

	video::SMaterial &material = m_meshbuffer->getMaterial();
	material.setFlag(video::EMF_TRILINEAR_FILTER, true);
	material.Lighting = false;
	material.TextureLayer[0].Texture = minimap_texture;
	material.TextureLayer[1].Texture = data->heightmap_texture;

	if (m_enable_shaders && !data->is_radar) {
		u16 sid = m_shdrsrc->getShader("minimap_shader", 1, 1);
		material.MaterialType = m_shdrsrc->getShaderInfo(sid).material;
	} else {
		material.MaterialType = video::EMT_TRANSPARENT_ALPHA_CHANNEL;
	}

	if (data->minimap_shape_round)
		matrix.setRotationDegrees(core::vector3df(0, 0, 360 - m_angle));

	// Draw minimap
	driver->setTransform(video::ETS_WORLD, matrix);
	driver->setMaterial(material);
	driver->drawMeshBuffer(m_meshbuffer);

	// Draw overlay
	video::ITexture *minimap_overlay = data->minimap_shape_round ?
		data->minimap_overlay_round : data->minimap_overlay_square;
	material.TextureLayer[0].Texture = minimap_overlay;
	material.MaterialType = video::EMT_TRANSPARENT_ALPHA_CHANNEL;
	driver->setMaterial(material);
	driver->drawMeshBuffer(m_meshbuffer);

	// If round minimap, draw player marker
	if (!data->minimap_shape_round) {
		matrix.setRotationDegrees(core::vector3df(0, 0, m_angle));
		material.TextureLayer[0].Texture = data->player_marker;

		driver->setTransform(video::ETS_WORLD, matrix);
		driver->setMaterial(material);
		driver->drawMeshBuffer(m_meshbuffer);
	}

	// Reset transformations
	driver->setTransform(video::ETS_VIEW, oldViewMat);
	driver->setTransform(video::ETS_PROJECTION, oldProjMat);
	driver->setViewPort(oldViewPort);
}

////
//// MinimapMapblock
////

void MinimapMapblock::getMinimapNodes(VoxelManipulator *vmanip, v3s16 pos)
{

	for (s16 x = 0; x < MAP_BLOCKSIZE; x++)
	for (s16 z = 0; z < MAP_BLOCKSIZE; z++) {
		s16 air_count = 0;
		bool surface_found = false;
		MinimapPixel *mmpixel = &data[z * MAP_BLOCKSIZE + x];

		for (s16 y = MAP_BLOCKSIZE -1; y >= 0; y--) {
			v3s16 p(x, y, z);
			MapNode n = vmanip->getNodeNoEx(pos + p);
			if (!surface_found && n.getContent() != CONTENT_AIR) {
				mmpixel->height = y;
				mmpixel->id = n.getContent();
				surface_found = true;
			} else if (n.getContent() == CONTENT_AIR) {
				air_count++;
			}
		}

		if (!surface_found)
			mmpixel->id = CONTENT_AIR;

		mmpixel->air_count = air_count;
	}
}
