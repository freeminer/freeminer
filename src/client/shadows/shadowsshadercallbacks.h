/*
Minetest
Copyright (C) 2021 Liso <anlismon@gmail.com>

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

#pragma once
#include "irrlichttypes_extrabloated.h"
#include <IMaterialRendererServices.h>
#include <IShaderConstantSetCallBack.h>
#include "client/shader.h"

// Used by main game rendering

class ShadowConstantSetter : public IShaderConstantSetter
{
	CachedPixelShaderSetting<f32, 16> m_shadow_view_proj{"m_ShadowViewProj"};
	CachedPixelShaderSetting<f32, 3> m_light_direction{"v_LightDirection"};
	CachedPixelShaderSetting<f32> m_texture_res{"f_textureresolution"};
	CachedPixelShaderSetting<f32> m_shadow_strength{"f_shadow_strength"};
	CachedPixelShaderSetting<f32> m_time_of_day{"f_timeofday"};
	CachedPixelShaderSetting<f32> m_shadowfar{"f_shadowfar"};
	CachedPixelShaderSetting<f32, 4> m_camera_pos{"CameraPos"};
	CachedPixelShaderSetting<s32> m_shadow_texture{"ShadowMapSampler"};
	CachedVertexShaderSetting<f32>
		m_perspective_bias0_vertex{"xyPerspectiveBias0"};
	CachedPixelShaderSetting<f32>
		m_perspective_bias0_pixel{"xyPerspectiveBias0"};
	CachedVertexShaderSetting<f32>
		m_perspective_bias1_vertex{"xyPerspectiveBias1"};
	CachedPixelShaderSetting<f32>
		m_perspective_bias1_pixel{"xyPerspectiveBias1"};
	CachedVertexShaderSetting<f32>
		m_perspective_zbias_vertex{"zPerspectiveBias"};
	CachedPixelShaderSetting<f32> m_perspective_zbias_pixel{"zPerspectiveBias"};

public:
	ShadowConstantSetter() = default;
	~ShadowConstantSetter() = default;

	virtual void onSetConstants(video::IMaterialRendererServices *services) override;
};

class ShadowConstantSetterFactory : public IShaderConstantSetterFactory
{
public:
	virtual IShaderConstantSetter *create() {
		return new ShadowConstantSetter();
	}
};

// Used by depth shader

class ShadowDepthShaderCB : public video::IShaderConstantSetCallBack
{
public:
	void OnSetMaterial(const video::SMaterial &material) override {}

	void OnSetConstants(video::IMaterialRendererServices *services,
			s32 userData) override;

	f32 MaxFar{2048.0f}, MapRes{1024.0f};
	f32 PerspectiveBiasXY {0.9f}, PerspectiveBiasZ {0.5f};
	v3f CameraPos;

private:
	CachedVertexShaderSetting<f32, 16> m_light_mvp_setting{"LightMVP"};
	CachedVertexShaderSetting<f32> m_map_resolution_setting{"MapResolution"};
	CachedVertexShaderSetting<f32> m_max_far_setting{"MaxFar"};
	CachedPixelShaderSetting<s32>
		m_color_map_sampler_setting{"ColorMapSampler"};
	CachedVertexShaderSetting<f32> m_perspective_bias0{"xyPerspectiveBias0"};
	CachedVertexShaderSetting<f32> m_perspective_bias1{"xyPerspectiveBias1"};
	CachedVertexShaderSetting<f32> m_perspective_zbias{"zPerspectiveBias"};
	CachedVertexShaderSetting<f32, 4> m_cam_pos_setting{"CameraPos"};
};
