// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include "nodedef.h"

namespace farmesh
{
inline TileDef opaqueFarTileDef(TileDef tile)
{
	// Apply after the node's own modifiers, so every base texel closes the coarse
	// surface. Texture slicing for animation happens afterwards as usual.
	if (!tile.name.empty())
		tile.name += "^[noalpha";
	return tile;
}
}
