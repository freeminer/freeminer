/*
Minetest
Copyright (C) 2010-2013 celeron55, Perttu Ahola <celeron55@gmail.com>
Copyright (C) 2017 numzero, Lobachevskiy Vitaliy <numzer0@yandex.ru>
Copyright (C) 2020 appgurueu, Lars Mueller <appgurulars@gmx.de>

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

#include "secondstage.h"
#include "client/client.h"
#include "client/shader.h"
#include "client/tile.h"
#include "settings.h"

PostProcessingStep::PostProcessingStep(u32 _shader_id, const std::vector<u8> &_texture_map) :
	shader_id(_shader_id), texture_map(_texture_map)
{
	assert(texture_map.size() <= video::MATERIAL_MAX_TEXTURES);
	configureMaterial();
}

void PostProcessingStep::configureMaterial()
{
	material.UseMipMaps = false;
	material.ZBuffer = true;
	material.ZWriteEnable = video::EZW_ON;
	for (u32 k = 0; k < texture_map.size(); ++k) {
		material.TextureLayers[k].AnisotropicFilter = 0;
		material.TextureLayers[k].MinFilter = video::ETMINF_NEAREST_MIPMAP_NEAREST;
		material.TextureLayers[k].MagFilter = video::ETMAGF_NEAREST;
		material.TextureLayers[k].TextureWrapU = video::ETC_CLAMP_TO_EDGE;
		material.TextureLayers[k].TextureWrapV = video::ETC_CLAMP_TO_EDGE;
	}
}

void PostProcessingStep::setRenderSource(RenderSource *_source)
{
	source = _source;
}

void PostProcessingStep::setRenderTarget(RenderTarget *_target)
{
	target = _target;
}

void PostProcessingStep::reset(PipelineContext &context)
{
}

void PostProcessingStep::run(PipelineContext &context)
{
	if (target)
		target->activate(context);

	// attach the shader
	material.MaterialType = context.client->getShaderSource()->getShaderInfo(shader_id).material;

	auto driver = context.device->getVideoDriver();

	for (u32 i = 0; i < texture_map.size(); i++)
		material.TextureLayers[i].Texture = source->getTexture(texture_map[i]);

	static const video::SColor color = video::SColor(0, 0, 0, 255);
	static const video::S3DVertex vertices[4] = {
			video::S3DVertex(1.0, -1.0, 0.0, 0.0, 0.0, -1.0,
					color, 1.0, 0.0),
			video::S3DVertex(-1.0, -1.0, 0.0, 0.0, 0.0, -1.0,
					color, 0.0, 0.0),
			video::S3DVertex(-1.0, 1.0, 0.0, 0.0, 0.0, -1.0,
					color, 0.0, 1.0),
			video::S3DVertex(1.0, 1.0, 0.0, 0.0, 0.0, -1.0,
					color, 1.0, 1.0),
	};
	static const u16 indices[6] = {0, 1, 2, 2, 3, 0};
	driver->setMaterial(material);
	driver->drawVertexPrimitiveList(&vertices, 4, &indices, 2);
}

void PostProcessingStep::setBilinearFilter(u8 index, bool value)
{
	assert(index < video::MATERIAL_MAX_TEXTURES);
	material.TextureLayers[index].MinFilter = value ? video::ETMINF_LINEAR_MIPMAP_NEAREST : video::ETMINF_NEAREST_MIPMAP_NEAREST;
	material.TextureLayers[index].MagFilter = value ? video::ETMAGF_LINEAR : video::ETMAGF_NEAREST;
}

RenderStep *addPostProcessing(RenderPipeline *pipeline, RenderStep *previousStep, v2f scale, Client *client)
{
	auto buffer = pipeline->createOwned<TextureBuffer>();
	auto driver = client->getSceneManager()->getVideoDriver();

	// configure texture formats
	video::ECOLOR_FORMAT color_format = video::ECF_A8R8G8B8;
	if (driver->queryTextureFormat(video::ECF_A16B16G16R16F))
		color_format = video::ECF_A16B16G16R16F;

	video::ECOLOR_FORMAT depth_format = video::ECF_D16; // fallback depth format
	if (driver->queryTextureFormat(video::ECF_D32))
		depth_format = video::ECF_D32;
	else if (driver->queryTextureFormat(video::ECF_D24S8))
		depth_format = video::ECF_D24S8;


	// init post-processing buffer
	static const u8 TEXTURE_COLOR = 0;
	static const u8 TEXTURE_DEPTH = 1;
	static const u8 TEXTURE_BLOOM = 2;
	static const u8 TEXTURE_EXPOSURE_1 = 3;
	static const u8 TEXTURE_EXPOSURE_2 = 4;
	static const u8 TEXTURE_FXAA = 5;
	static const u8 TEXTURE_VOLUME = 6;
	static const u8 TEXTURE_SCALE_DOWN = 10;
	static const u8 TEXTURE_SCALE_UP = 20;

	// Super-sampling is simply rendering into a larger texture.
	// Downscaling is done by the final step when rendering to the screen.
	const std::string antialiasing = g_settings->get("antialiasing");
	const bool enable_bloom = g_settings->getBool("enable_bloom");
	const bool enable_auto_exposure = g_settings->getBool("enable_auto_exposure");
	const bool enable_ssaa = antialiasing == "ssaa";
	const bool enable_fxaa = antialiasing == "fxaa";
	const bool enable_volumetric_light = g_settings->getBool("enable_volumetric_lighting") && enable_bloom;

	if (enable_ssaa) {
		u16 ssaa_scale = MYMAX(2, g_settings->getU16("fsaa"));
		scale *= ssaa_scale;
	}

	buffer->setTexture(TEXTURE_COLOR, scale, "3d_render", color_format);
	buffer->setTexture(TEXTURE_EXPOSURE_1, core::dimension2du(1,1), "exposure_1", color_format, /*clear:*/ true);
	buffer->setTexture(TEXTURE_EXPOSURE_2, core::dimension2du(1,1), "exposure_2", color_format, /*clear:*/ true);
	buffer->setTexture(TEXTURE_DEPTH, scale, "3d_depthmap", depth_format);

	// attach buffer to the previous step
	previousStep->setRenderTarget(pipeline->createOwned<TextureBufferOutput>(buffer, std::vector<u8> { TEXTURE_COLOR }, TEXTURE_DEPTH));

	// shared variables
	u32 shader_id;

	// Number of mipmap levels of the bloom downsampling texture
	const u8 MIPMAP_LEVELS = 4;


	// post-processing stage

	u8 source = TEXTURE_COLOR;

	// common downsampling step for bloom or autoexposure
	if (enable_bloom || enable_auto_exposure) {

		v2f downscale = scale * 0.5;
		for (u8 i = 0; i < MIPMAP_LEVELS; i++) {
			buffer->setTexture(TEXTURE_SCALE_DOWN + i, downscale, std::string("downsample") + std::to_string(i), color_format);
			if (enable_bloom)
				buffer->setTexture(TEXTURE_SCALE_UP + i, downscale, std::string("upsample") + std::to_string(i), color_format);
			downscale *= 0.5;
		}

		if (enable_bloom) {
			buffer->setTexture(TEXTURE_BLOOM, scale, "bloom", color_format);

			// get bright spots
			u32 shader_id = client->getShaderSource()->getShader("extract_bloom", TILE_MATERIAL_PLAIN, NDT_MESH);
			RenderStep *extract_bloom = pipeline->addStep<PostProcessingStep>(shader_id, std::vector<u8> { source, TEXTURE_EXPOSURE_1 });
			extract_bloom->setRenderSource(buffer);
			extract_bloom->setRenderTarget(pipeline->createOwned<TextureBufferOutput>(buffer, TEXTURE_BLOOM));
			source = TEXTURE_BLOOM;
		}

		if (enable_volumetric_light) {
			buffer->setTexture(TEXTURE_VOLUME, scale, "volume", color_format);

			shader_id = client->getShaderSource()->getShader("volumetric_light", TILE_MATERIAL_PLAIN, NDT_MESH);
			auto volume = pipeline->addStep<PostProcessingStep>(shader_id, std::vector<u8> { source, TEXTURE_DEPTH });
			volume->setRenderSource(buffer);
			volume->setRenderTarget(pipeline->createOwned<TextureBufferOutput>(buffer, TEXTURE_VOLUME));
			source = TEXTURE_VOLUME;
		}

		// downsample
		shader_id = client->getShaderSource()->getShader("bloom_downsample", TILE_MATERIAL_PLAIN, NDT_MESH);
		for (u8 i = 0; i < MIPMAP_LEVELS; i++) {
			auto step = pipeline->addStep<PostProcessingStep>(shader_id, std::vector<u8> { source });
			step->setRenderSource(buffer);
			step->setBilinearFilter(0, true);
			step->setRenderTarget(pipeline->createOwned<TextureBufferOutput>(buffer, TEXTURE_SCALE_DOWN + i));
			source = TEXTURE_SCALE_DOWN + i;
		}
	}

	// Bloom pt 2
	if (enable_bloom) {
		// upsample
		shader_id = client->getShaderSource()->getShader("bloom_upsample", TILE_MATERIAL_PLAIN, NDT_MESH);
		for (u8 i = MIPMAP_LEVELS - 1; i > 0; i--) {
			auto step = pipeline->addStep<PostProcessingStep>(shader_id, std::vector<u8> { u8(TEXTURE_SCALE_DOWN + i - 1), source });
			step->setRenderSource(buffer);
			step->setBilinearFilter(0, true);
			step->setBilinearFilter(1, true);
			step->setRenderTarget(pipeline->createOwned<TextureBufferOutput>(buffer, u8(TEXTURE_SCALE_UP + i - 1)));
			source = TEXTURE_SCALE_UP + i - 1;
		}
	}

	// Dynamic Exposure pt2
	if (enable_auto_exposure) {
		shader_id = client->getShaderSource()->getShader("update_exposure", TILE_MATERIAL_PLAIN, NDT_MESH);
		auto update_exposure = pipeline->addStep<PostProcessingStep>(shader_id, std::vector<u8> { TEXTURE_EXPOSURE_1, u8(TEXTURE_SCALE_DOWN + MIPMAP_LEVELS - 1) });
		update_exposure->setBilinearFilter(1, true);
		update_exposure->setRenderSource(buffer);
		update_exposure->setRenderTarget(pipeline->createOwned<TextureBufferOutput>(buffer, TEXTURE_EXPOSURE_2));
	}

	// FXAA
	u8 final_stage_source = TEXTURE_COLOR;

	if (enable_fxaa) {
		final_stage_source = TEXTURE_FXAA;

		buffer->setTexture(TEXTURE_FXAA, scale, "fxaa", color_format);
		shader_id = client->getShaderSource()->getShader("fxaa", TILE_MATERIAL_PLAIN);
		PostProcessingStep *effect = pipeline->createOwned<PostProcessingStep>(shader_id, std::vector<u8> { TEXTURE_COLOR });
		pipeline->addStep(effect);
		effect->setBilinearFilter(0, true);
		effect->setRenderSource(buffer);
		effect->setRenderTarget(pipeline->createOwned<TextureBufferOutput>(buffer, TEXTURE_FXAA));
	}

	// final merge
	shader_id = client->getShaderSource()->getShader("second_stage", TILE_MATERIAL_PLAIN, NDT_MESH);
	PostProcessingStep *effect = pipeline->createOwned<PostProcessingStep>(shader_id, std::vector<u8> { final_stage_source, TEXTURE_SCALE_UP, TEXTURE_EXPOSURE_2 });
	pipeline->addStep(effect);
	if (enable_ssaa)
		effect->setBilinearFilter(0, true);
	effect->setBilinearFilter(1, true);
	effect->setRenderSource(buffer);

	if (enable_auto_exposure) {
		pipeline->addStep<SwapTexturesStep>(buffer, TEXTURE_EXPOSURE_1, TEXTURE_EXPOSURE_2);
	}

	return effect;
}
