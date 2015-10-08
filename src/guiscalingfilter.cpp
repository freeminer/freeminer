/*
Copyright (C) 2015 Aaron Suen <warr1024@gmail.com>

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

#include "guiscalingfilter.h"
#include "imagefilters.h"
#include "settings.h"
#include "util/numeric.h"
#include <stdio.h>

/* Maintain a static cache to store the images that correspond to textures
 * in a format that's manipulable by code.  Some platforms exhibit issues
 * converting textures back into images repeatedly, and some don't even
 * allow it at all.
 */
std::map<io::path, video::IImage *> g_imgCache;

/* Maintain a static cache of all pre-scaled textures.  These need to be
 * cleared as well when the cached images.
 */
std::map<io::path, video::ITexture *> g_txrCache;

/* Manually insert an image into the cache, useful to avoid texture-to-image
 * conversion whenever we can intercept it.
 */
void guiScalingCache(io::path key, video::IVideoDriver *driver, video::IImage *value)
{
	if (!g_settings->getBool("gui_scaling_filter"))
		return;
	video::IImage *copied = driver->createImage(value->getColorFormat(),
			value->getDimension());
	value->copyTo(copied);
	g_imgCache[key] = copied;
}

// Manually clear the cache, e.g. when switching to different worlds.
void guiScalingCacheClear(video::IVideoDriver *driver)
{
	for (std::map<io::path, video::IImage *>::iterator it = g_imgCache.begin();
			it != g_imgCache.end(); ++it) {
		if (it->second != NULL)
			it->second->drop();
	}
	g_imgCache.clear();
	for (std::map<io::path, video::ITexture *>::iterator it = g_txrCache.begin();
			it != g_txrCache.end(); ++it) {
		if (it->second != NULL)
			driver->removeTexture(it->second);
	}
	g_txrCache.clear();
}

/* Get a cached, high-quality pre-scaled texture for display purposes.  If the
 * texture is not already cached, attempt to create it.  Returns a pre-scaled texture,
 * or the original texture if unable to pre-scale it.
 */
video::ITexture *guiScalingResizeCached(video::IVideoDriver *driver,
		video::ITexture *src, const core::rect<s32> &srcrect,
		const core::rect<s32> &destrect)
{
	if (src == NULL)
		return src;
	if (!g_settings->getBool("gui_scaling_filter"))
		return src;

	// Calculate scaled texture name.
	char rectstr[200];
	snprintf(rectstr, sizeof(rectstr), "%d:%d:%d:%d:%d:%d",
		srcrect.UpperLeftCorner.X,
		srcrect.UpperLeftCorner.Y,
		srcrect.getWidth(),
		srcrect.getHeight(),
		destrect.getWidth(),
		destrect.getHeight());
	io::path origname = src->getName().getPath();
	io::path scalename = origname + "@guiScalingFilter:" + rectstr;

	// Search for existing scaled texture.
	video::ITexture *scaled = g_txrCache[scalename];
	if (scaled)
		return scaled;

	// Try to find the texture converted to an image in the cache.
	// If the image was not found, try to extract it from the texture.
	video::IImage* srcimg = g_imgCache[origname];
	if (srcimg == NULL) {
		if (!g_settings->getBool("gui_scaling_filter_txr2img"))
			return src;
		srcimg = driver->createImageFromData(src->getColorFormat(),
			src->getSize(), src->lock(), false);
		src->unlock();
		g_imgCache[origname] = srcimg;
	}

	// Create a new destination image and scale the source into it.
	imageCleanTransparent(srcimg, 0);
	video::IImage *destimg = driver->createImage(src->getColorFormat(),
			core::dimension2d<u32>((u32)destrect.getWidth(),
			(u32)destrect.getHeight()));
	imageScaleNNAA(srcimg, srcrect, destimg);

#ifdef __ANDROID__
	// Android is very picky about textures being powers of 2, so expand
	// the image dimensions to the next power of 2, if necessary, for
	// that platform.
	video::IImage *po2img = driver->createImage(src->getColorFormat(),
			core::dimension2d<u32>(npot2((u32)destrect.getWidth()),
			npot2((u32)destrect.getHeight())));
	po2img->fill(video::SColor(0, 0, 0, 0));
	destimg->copyTo(po2img);
	destimg->drop();
	destimg = po2img;
#endif

	// Convert the scaled image back into a texture.
	scaled = driver->addTexture(scalename, destimg, NULL);
	destimg->drop();
	g_txrCache[scalename] = scaled;

	return scaled;
}

/* Convenience wrapper for guiScalingResizeCached that accepts parameters that
 * are available at GUI imagebutton creation time.
 */
video::ITexture *guiScalingImageButton(video::IVideoDriver *driver,
		video::ITexture *src, s32 width, s32 height)
{
	if (src == NULL)
		return src;
	return guiScalingResizeCached(driver, src,
		core::rect<s32>(0, 0, src->getSize().Width, src->getSize().Height),
		core::rect<s32>(0, 0, width, height));
}

/* Replacement for driver->draw2DImage() that uses the high-quality pre-scaled
 * texture, if configured.
 */
void draw2DImageFilterScaled(video::IVideoDriver *driver, video::ITexture *txr,
		const core::rect<s32> &destrect, const core::rect<s32> &srcrect,
		const core::rect<s32> *cliprect, const video::SColor *const colors,
		bool usealpha)
{
	// Attempt to pre-scale image in software in high quality.
	video::ITexture *scaled = guiScalingResizeCached(driver, txr, srcrect, destrect);
	if (scaled == NULL)
		return;

	// Correct source rect based on scaled image.
	const core::rect<s32> mysrcrect = (scaled != txr)
		? core::rect<s32>(0, 0, destrect.getWidth(), destrect.getHeight())
		: srcrect;

	driver->draw2DImage(scaled, destrect, mysrcrect, cliprect, colors, usealpha);
}
