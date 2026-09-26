// SPDX-License-Identifier: LGPL-2.1-or-later
#pragma once
#include "pipeline.h"

class Draw3D;
bool fmFarFogHalfResolution(Client *client);
// Appends fog accumulation and depth-aware compositing. Slots 30 and 31 are reserved.
RenderStep *fmAddFarFog(RenderPipeline *pipeline, Draw3D *scene, TextureBuffer *buffer,
		u8 color, u8 depth, v2f scale, Client *client);
