/*
clouds.h
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

#ifndef CLOUDS_HEADER
#define CLOUDS_HEADER

#include "irrlichttypes_extrabloated.h"
#include <iostream>
#include "constants.h"

class Clouds : public scene::ISceneNode
{
public:
	Clouds(
			scene::ISceneNode* parent,
			scene::ISceneManager* mgr,
			s32 id,
			u32 seed,
			s16 cloudheight=0
	);

	~Clouds();

	/*
		ISceneNode methods
	*/

	virtual void OnRegisterSceneNode();

	virtual void render();
	
	virtual const core::aabbox3d<f32>& getBoundingBox() const
	{
		return m_box;
	}

	virtual u32 getMaterialCount() const
	{
		return 1;
	}

	virtual video::SMaterial& getMaterial(u32 i)
	{
		return m_material;
	}
	
	/*
		Other stuff
	*/

	void step(float dtime);

	void update(v2f camera_p, video::SColorf color);
	
	void updateCameraOffset(v3s16 camera_offset)
	{
		m_camera_offset = camera_offset;
		m_box = core::aabbox3d<f32>(-BS * 1000000, m_cloud_y - BS - BS * camera_offset.Y, -BS * 1000000,
			BS * 1000000, m_cloud_y + BS - BS * camera_offset.Y, BS * 1000000);
	}

private:
	video::SMaterial m_material;
	core::aabbox3d<f32> m_box;
public:
	float m_cloud_y;
private:
	video::SColorf m_color;
	u32 m_seed;
	v2f m_camera_pos;
	float m_time;
	v3s16 m_camera_offset;
};

extern Clouds *g_menuclouds;

extern irr::scene::ISceneManager *g_menucloudsmgr;

#endif

