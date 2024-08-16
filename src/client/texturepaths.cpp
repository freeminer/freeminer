/*
Minetest
Copyright (C) 2010-2013 celeron55, Perttu Ahola <celeron55@gmail.com>

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

#include "texturepaths.h"

#include "util/container.h"
#include "settings.h"
#include "filesys.h"
#include "porting.h"
#include <vector>

// A cache from texture name to texture path
static MutexedMap<std::string, std::string> g_texturename_to_path_cache;

void clearTextureNameCache()
{
	g_texturename_to_path_cache.clear();
}

// Find out the full path of an image by trying different filename extensions.
// If failed, return "".
std::string getImagePath(std::string_view path)
{
	// A NULL-ended list of possible image extensions
	// (In newer C++ versions a fixed size array and ranges::subrange could be used
	// or just modernise removeStringEnd.)
	static const char *extensions[] = {".png", ".jpg", ".bmp", ".tga", nullptr};

	// Remove present extension
	std::string_view stripped_path = removeStringEnd(path, extensions);
	// If there is no known extension, assume it has been omitted.
	if (stripped_path.empty())
		stripped_path = path;

	for (const char *ext : extensions) {
		if (!ext)
			break;
		std::string extended_path(stripped_path);
		extended_path.append(ext);
		if (fs::PathExists(extended_path))
			return extended_path;
	}

	return "";
}

/* Gets the path to a texture by first checking if the texture exists
 * in texture_path and if not, using the data path.
 *
 * Checks all supported extensions by replacing the original extension.
 *
 * If not found, returns "".
 *
 * Utilizes a thread-safe cache.
*/
std::string getTexturePath(const std::string &filename, bool *is_base_pack)
{
	std::string fullpath;

	// This can set a wrong value on cached textures, but is irrelevant because
	// is_base_pack is only passed when initializing the textures the first time
	if (is_base_pack)
		*is_base_pack = false;

	// Check from cache
	bool incache = g_texturename_to_path_cache.get(filename, &fullpath);
	if (incache)
		return fullpath;

	// Check from texture_path setting
	for (const auto &path : getTextureDirs()) {
		// Check all filename extensions. Returns "" if not found.
		fullpath = getImagePath(path + DIR_DELIM + filename);
		if (!fullpath.empty())
			break;
	}

	// Check from default data directory i.e. .../minetest/textures/base/pack
	if (fullpath.empty()) {
		std::string base_path = porting::path_share + DIR_DELIM + "textures"
				+ DIR_DELIM + "base" + DIR_DELIM + "pack";
		// Check all filename extensions. Returns "" if not found.
		fullpath = getImagePath(base_path + DIR_DELIM + filename);
		if (is_base_pack && !fullpath.empty())
			*is_base_pack = true;
	}

	// Add to cache (also an empty result is cached)
	g_texturename_to_path_cache.set(filename, fullpath);

	// Finally return it
	return fullpath;
}

std::vector<std::string> getTextureDirs()
{
	return fs::GetRecursiveDirs(g_settings->get("texture_path"));
}
