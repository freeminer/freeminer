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

#pragma once

#include "irr_v3d.h"
#include "irrlichttypes_bloated.h"
#include <matrix4.h>
#include "util/basic_macros.h"
#include "constants.h"

class Camera;
class Client;

struct shadowFrustum
{
	f32 zNear{0.0f};
	f32 zFar{0.0f};
	f32 length{0.0f};
	f32 radius{0.0f};
	core::matrix4 ProjOrthMat;
	core::matrix4 ViewMat;
	v3opos_t position;
	v3f player;
	v3pos_t camera_offset;
};

class DirectionalLight
{
public:
	DirectionalLight(const u32 shadowMapResolution,
			const v3f &position,
			video::SColorf lightColor = video::SColor(0xffffffff),
			f32 farValue = 100.0f);
	~DirectionalLight() = default;

	//DISABLE_CLASS_COPY(DirectionalLight)

	void update_frustum(const Camera *cam, Client *client, bool force = false);

	// when set direction is updated to negative normalized(direction)
	void setDirection(v3f dir);
	v3f getDirection() const{
		return direction;
	};
	v3opos_t getPosition() const;
	v3f getPlayerPos() const;
	v3f getFuturePlayerPos() const;

	/// Gets the light's matrices.
	const core::matrix4 &getViewMatrix() const;
	const core::matrix4 &getProjectionMatrix() const;
	const core::matrix4 &getFutureViewMatrix() const;
	const core::matrix4 &getFutureProjectionMatrix() const;
	core::matrix4 getViewProjMatrix();

	/// Gets the light's maximum far value, i.e. the shadow boundary
	f32 getMaxFarValue() const
	{
		return farPlane * BS;
	}

	/// Gets the current far value of the light
	f32 getFarValue() const
	{
		return shadow_frustum.zFar;
	}


	/// Gets the light's color.
	const video::SColorf &getLightColor() const
	{
		return diffuseColor;
	}

	/// Sets the light's color.
	void setLightColor(const video::SColorf &lightColor)
	{
		diffuseColor = lightColor;
	}

	/// Gets the shadow map resolution for this light.
	u32 getMapResolution() const
	{
		return mapRes;
	}

	bool should_update_map_shadow{true};

	void commitFrustum();

private:
	void createSplitMatrices(const Camera *cam);

	video::SColorf diffuseColor;

	f32 farPlane;
	u32 mapRes;

	v3f pos;
	v3f direction{0};

	v3opos_t last_cam_pos_world{0,0,0};
	v3f last_look{0,1,0};

	shadowFrustum shadow_frustum;
	shadowFrustum future_frustum;
	bool dirty{false};
};
