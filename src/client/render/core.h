// Luanti
// SPDX-License-Identifier: LGPL-2.1-or-later
// Copyright (C) 2010-2013 celeron55, Perttu Ahola <celeron55@gmail.com>
// Copyright (C) 2017 numzero, Lobachevskiy Vitaliy <numzer0@yandex.ru>

#pragma once
#include "irrlichttypes_extrabloated.h"

namespace irr
{
	class IrrlichtDevice;
}

class ShadowRenderer;
class Camera;
class Client;
class Hud;
class Minimap;
class RenderPipeline;
class RenderTarget;

class RenderingCore
{
protected:
	IrrlichtDevice *device;
	Client *client;
	Hud *hud;
	ShadowRenderer *shadow_renderer;

	RenderPipeline *pipeline;

	v2f virtual_size_scale;
	v2u32 virtual_size { 0, 0 };

public:
	RenderingCore(IrrlichtDevice *device, Client *client, Hud *hud,
			ShadowRenderer *shadow_renderer, RenderPipeline *pipeline,
			v2f virtual_size_scale);
	RenderingCore(const RenderingCore &) = delete;
	RenderingCore(RenderingCore &&) = delete;
	virtual ~RenderingCore();

	RenderingCore &operator=(const RenderingCore &) = delete;
	RenderingCore &operator=(RenderingCore &&) = delete;

	void draw(video::SColor _skycolor, bool _show_hud,
			bool _draw_wield_tool, bool _draw_crosshair);

	v2u32 getVirtualSize() const;

	ShadowRenderer *get_shadow_renderer() { return shadow_renderer; };
};
