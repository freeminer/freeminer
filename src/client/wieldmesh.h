/*
Minetest
Copyright (C) 2010-2014 celeron55, Perttu Ahola <celeron55@gmail.com>

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

#pragma once

#include <string>
#include <vector>
#include "irrlichttypes_extrabloated.h"

struct ItemStack;
class Client;
class ITextureSource;
struct ContentFeatures;
class ShadowRenderer;

/*
 * Holds color information of an item mesh's buffer.
 */
class ItemPartColor
{
	/*
	 * Optional color that overrides the global base color.
	 */
	video::SColor override_color;
	/*
	 * Stores the last color this mesh buffer was colorized as.
	 */
	video::SColor last_colorized;

	// saves some bytes compared to two std::optionals
	bool override_color_set = false;
	bool last_colorized_set = false;

public:

	ItemPartColor() = default;

	ItemPartColor(bool override, video::SColor color) :
		override_color(color), override_color_set(override)
	{}

	void applyOverride(video::SColor &dest) const {
		if (override_color_set)
			dest = override_color;
	}

	bool needColorize(video::SColor target) {
		if (last_colorized_set && target == last_colorized)
			return false;
		last_colorized_set = true;
		last_colorized = target;
		return true;
	}
};

struct ItemMesh
{
	scene::IMesh *mesh = nullptr;
	/*
	 * Stores the color of each mesh buffer.
	 */
	std::vector<ItemPartColor> buffer_colors;
	/*
	 * If false, all faces of the item should have the same brightness.
	 * Disables shading based on normal vectors.
	 */
	bool needs_shading = true;

	ItemMesh() = default;
};

/*
	Wield item scene node, renders the wield mesh of some item
*/
class WieldMeshSceneNode : public scene::ISceneNode
{
public:
	WieldMeshSceneNode(scene::ISceneManager *mgr, s32 id = -1, bool lighting = false);
	virtual ~WieldMeshSceneNode();

	void setCube(const ContentFeatures &f, v3f wield_scale);
	void setExtruded(const std::string &imagename, const std::string &overlay_image,
			v3f wield_scale, ITextureSource *tsrc, u8 num_frames);
	void setItem(const ItemStack &item, Client *client,
			bool check_wield_image = true);

	// Sets the vertex color of the wield mesh.
	// Must only be used if the constructor was called with lighting = false
	void setColor(video::SColor color);

	void setNodeLightColor(video::SColor color);

	scene::IMesh *getMesh() { return m_meshnode->getMesh(); }

	virtual void render();

	virtual const aabb3f &getBoundingBox() const { return m_bounding_box; }

private:
	void changeToMesh(scene::IMesh *mesh);

	// Child scene node with the current wield mesh
	scene::IMeshSceneNode *m_meshnode = nullptr;
	video::E_MATERIAL_TYPE m_material_type;

	// True if SMaterial::Lighting should be enabled.
	bool m_lighting;

	bool m_enable_shaders;
	bool m_anisotropic_filter;
	bool m_bilinear_filter;
	bool m_trilinear_filter;
	/*!
	 * Stores the colors of the mesh's mesh buffers.
	 * This does not include lighting.
	 */
	std::vector<ItemPartColor> m_colors;
	/*!
	 * The base color of this mesh. This is the default
	 * for all mesh buffers.
	 */
	video::SColor m_base_color;

	// Bounding box culling is disabled for this type of scene node,
	// so this variable is just required so we can implement
	// getBoundingBox() and is set to an empty box.
	aabb3f m_bounding_box;

	ShadowRenderer *m_shadow;
};

void getItemMesh(Client *client, const ItemStack &item, ItemMesh *result);

scene::SMesh *getExtrudedMesh(ITextureSource *tsrc, const std::string &imagename,
		const std::string &overlay_name);

/*!
 * Applies overlays, textures and optionally materials to the given mesh and
 * extracts tile colors for colorization.
 * \param mattype overrides the buffer's material type, but can also
 * be NULL to leave the original material.
 * \param colors returns the colors of the mesh buffers in the mesh.
 */
void postProcessNodeMesh(scene::SMesh *mesh, const ContentFeatures &f, bool use_shaders,
		bool set_material, const video::E_MATERIAL_TYPE *mattype,
		std::vector<ItemPartColor> *colors, bool apply_scale = false);
