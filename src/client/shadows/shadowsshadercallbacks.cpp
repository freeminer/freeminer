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

#include "client/shadows/shadowsshadercallbacks.h"
#include "client/renderingengine.h"

void ShadowConstantSetter::onSetConstants(video::IMaterialRendererServices *services)
{
	auto *shadow = RenderingEngine::get_shadow_renderer();
	if (!shadow)
		return;

	const auto &light = shadow->getDirectionalLight();

	core::matrix4 shadowViewProj = light.getProjectionMatrix();
	shadowViewProj *= light.getViewMatrix();
	m_shadow_view_proj.set(shadowViewProj.pointer(), services);

	m_light_direction.set(light.getDirection(), services);

	f32 TextureResolution = light.getMapResolution();
	m_texture_res.set(&TextureResolution, services);

	f32 ShadowStrength = shadow->getShadowStrength();
	m_shadow_strength.set(&ShadowStrength, services);

	f32 timeOfDay = shadow->getTimeOfDay();
	m_time_of_day.set(&timeOfDay, services);

	f32 shadowFar = shadow->getMaxShadowFar();
	m_shadowfar.set(&shadowFar, services);

	f32 cam_pos[4];
	shadowViewProj.transformVect(cam_pos, light.getPlayerPos());
	m_camera_pos.set(cam_pos, services);

	s32 TextureLayerID = ShadowRenderer::TEXTURE_LAYER_SHADOW;
	m_shadow_texture.set(&TextureLayerID, services);

	f32 bias0 = shadow->getPerspectiveBiasXY();
	m_perspective_bias0_vertex.set(&bias0, services);
	m_perspective_bias0_pixel.set(&bias0, services);
	f32 bias1 = 1.0f - bias0 + 1e-5f;
	m_perspective_bias1_vertex.set(&bias1, services);
	m_perspective_bias1_pixel.set(&bias1, services);
	f32 zbias = shadow->getPerspectiveBiasZ();
	m_perspective_zbias_vertex.set(&zbias, services);
	m_perspective_zbias_pixel.set(&zbias, services);
}

void ShadowDepthShaderCB::OnSetConstants(
		video::IMaterialRendererServices *services, s32 userData)
{
	video::IVideoDriver *driver = services->getVideoDriver();

	core::matrix4 lightMVP = driver->getTransform(video::ETS_PROJECTION);
	lightMVP *= driver->getTransform(video::ETS_VIEW);

	f32 cam_pos[4];
	lightMVP.transformVect(cam_pos, CameraPos);

	lightMVP *= driver->getTransform(video::ETS_WORLD);

	m_light_mvp_setting.set(lightMVP, services);
	m_map_resolution_setting.set(&MapRes, services);
	m_max_far_setting.set(&MaxFar, services);
	s32 TextureId = 0;
	m_color_map_sampler_setting.set(&TextureId, services);
	f32 bias0 = PerspectiveBiasXY;
	m_perspective_bias0.set(&bias0, services);
	f32 bias1 = 1.0f - bias0 + 1e-5f;
	m_perspective_bias1.set(&bias1, services);
	f32 zbias = PerspectiveBiasZ;
	m_perspective_zbias.set(&zbias, services);

	m_cam_pos_setting.set(cam_pos, services);
}
