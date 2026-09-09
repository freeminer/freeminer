// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <IImage.h>

namespace farmesh
{
inline void makeOpaqueFarImage(video::IImage *image)
{
	const auto size = image->getDimension();
	u64 weight = 0, red = 0, green = 0, blue = 0;
	// Inspect every pixel: sparse details in large textures must not be missed.
	// Hidden RGB in fully transparent pixels must not influence the fill color.
	for (u32 y = 0; y < size.Height; ++y) {
		for (u32 x = 0; x < size.Width; ++x) {
			const auto color = image->getPixel(x, y);
			const u64 alpha = color.getAlpha();
			weight += alpha;
			red += alpha * color.getRed();
			green += alpha * color.getGreen();
			blue += alpha * color.getBlue();
		}
	}

	// Entirely invisible textures have no representative visible color.
	const video::SColor fill(255, weight ? (red + weight / 2) / weight : 128,
			weight ? (green + weight / 2) / weight : 128,
			weight ? (blue + weight / 2) / weight : 128);
	for (u32 y = 0; y < size.Height; ++y) {
		for (u32 x = 0; x < size.Width; ++x) {
			const auto color = image->getPixel(x, y);
			const u32 alpha = color.getAlpha();
			if (alpha == 255)
				continue;
			// Composite over the representative color before removing alpha.
			const auto blend = [alpha](u32 channel, u32 background) {
				return (channel * alpha + background * (255 - alpha) + 127) / 255;
			};
			image->setPixel(x, y,
					video::SColor(255, blend(color.getRed(), fill.getRed()),
							blend(color.getGreen(), fill.getGreen()),
							blend(color.getBlue(), fill.getBlue())));
		}
	}
}
}
