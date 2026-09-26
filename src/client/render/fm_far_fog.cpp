// SPDX-License-Identifier: LGPL-2.1-or-later
#include "fm_far_fog.h"
#include "plain.h"
#include "secondstage.h"
#include "client/client.h"
#include "client/clientmap.h"
#include "client/shader.h"
#include "settings.h"
#include <ISceneManager.h>

namespace
{
constexpr u8 FOG_COLOR = 30;

class DrawFarFog : public TrivialRenderStep
{
public:
	DrawFarFog(TextureBuffer *buffer, u8 depth, TextureBufferOutput *target) :
			m_buffer(buffer), m_depth(depth), m_target(target)
	{
	}

	void run(PipelineContext &context) override
	{
		const auto clear = context.clear_color;
		context.clear_color = video::SColor(0, 0, 0, 0);
		m_target->activate(context);
		context.clear_color = clear;
		context.client->getEnv().getClientMap().renderFarFog(
				context.device->getVideoDriver(), m_buffer->getTexture(m_depth));
	}

private:
	TextureBuffer *m_buffer;
	u8 m_depth;
	TextureBufferOutput *m_target;
};
}

bool fmFarFogHalfResolution(Client *client)
{
	auto *driver = client->getSceneManager()->getVideoDriver();
	// GLES2 lacks guaranteed depth textures and fragment depth output.
	return g_settings->getBool("volumetric_fog_half_resolution") &&
		   g_settings->getPos("volumetric_fog") > 0 &&
		   g_settings->getPos("farmesh") > 0 &&
		   (driver->getDriverType() == video::EDT_OPENGL3 ||
				   (driver->getDriverType() == video::EDT_OGLES2 &&
						   driver->getLimits().GLVersion.X >= 3));
}

RenderStep *fmAddFarFog(RenderPipeline *pipeline, Draw3D *scene, TextureBuffer *buffer,
		u8 color, u8 depth, v2f scale, Client *client)
{
	scene->deferFarFog(true);
	buffer->setTexture(FOG_COLOR, scale * 0.5f, "fm_fog_color", video::ECF_A8R8G8B8);
	auto *target = pipeline->createOwned<TextureBufferOutput>(buffer, FOG_COLOR);
	pipeline->addStep<DrawFarFog>(buffer, depth, target);

	const auto shader = client->getShaderSource()->getShaderRaw("fm_far_fog_composite");
	auto *composite = pipeline->addStep<PostProcessingStep>(
			shader, std::vector<u8>{FOG_COLOR, color, depth});
	composite->setRenderSource(buffer);
	return composite;
}
