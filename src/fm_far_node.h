// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include "nodedef.h"

namespace farmesh
{
inline bool isGlass(const ContentFeatures &features)
{
	return features.drawtype == NDT_GLASSLIKE ||
		   features.drawtype == NDT_GLASSLIKE_FRAMED ||
		   features.drawtype == NDT_GLASSLIKE_FRAMED_OPTIONAL;
}

inline bool isOpaqueStructure(const ContentFeatures &features)
{
	return (features.drawtype == NDT_NORMAL || features.drawtype == NDT_NODEBOX) &&
		   features.alpha == ALPHAMODE_OPAQUE && !features.isLiquid();
}

inline bool isTransparentCover(const ContentFeatures &features)
{
	return features.drawtype != NDT_AIRLIKE && !features.isLiquid() &&
		   (features.alpha != ALPHAMODE_OPAQUE || isGlass(features) ||
				   features.drawtype == NDT_PLANTLIKE ||
				   features.drawtype == NDT_ALLFACES ||
				   features.drawtype == NDT_ALLFACES_OPTIONAL);
}
}
