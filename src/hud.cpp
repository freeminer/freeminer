﻿/*
hud.cpp
Copyright (C) 2010-2013 celeron55, Perttu Ahola <celeron55@gmail.com>
Copyright (C) 2010-2013 blue42u, Jonathon Anderson <anderjon@umail.iu.edu>
Copyright (C) 2010-2013 kwolekr, Ryan Kwolek <kwolekr@minetest.net>
*/

/*
This file is part of Freeminer.

Freeminer is free software: you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation, either version 3 of the License, or
(at your option) any later version.

Freeminer  is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with Freeminer.  If not, see <http://www.gnu.org/licenses/>.
*/

#include "hud.h"
#include "settings.h"
#include "util/numeric.h"
#include "log.h"
#include "gamedef.h"
#include "itemdef.h"
#include "inventory.h"
#include "client/tile.h"
#include "localplayer.h"
#include "game.h" // CameraModes
#include "camera.h"
#include "porting.h"
#include "fontengine.h"
#include "guiscalingfilter.h"
#include "mesh.h"
#include <IGUIStaticText.h>

#ifdef HAVE_TOUCHSCREENGUI
#include "touchscreengui.h"
#endif

Hud::Hud(video::IVideoDriver *driver, scene::ISceneManager* smgr,
		gui::IGUIEnvironment* guienv, IGameDef *gamedef, LocalPlayer *player,
		Inventory *inventory) {
	this->driver      = driver;
	this->smgr        = smgr;
	this->guienv      = guienv;
	this->gamedef     = gamedef;
	this->player      = player;
	this->inventory   = inventory;

	m_screensize       = v2u32(0, 0);
	m_displaycenter    = v2s32(0, 0);
	m_hotbar_imagesize = floor(HOTBAR_IMAGE_SIZE * porting::getDisplayDensity() + 0.5);
	m_hotbar_imagesize *= g_settings->getFloat("hud_scaling");
	m_padding = m_hotbar_imagesize / 12;

	const video::SColor hbar_color(255, 255, 255, 255);
	for (unsigned int i=0; i < 4; i++ ){
		hbar_colors[i] = hbar_color;
	}

	tsrc = gamedef->getTextureSource();

	v3f crosshair_color = g_settings->getV3F("crosshair_color");
	u32 cross_r = rangelim(myround(crosshair_color.X), 0, 255);
	u32 cross_g = rangelim(myround(crosshair_color.Y), 0, 255);
	u32 cross_b = rangelim(myround(crosshair_color.Z), 0, 255);
	u32 cross_a = rangelim(g_settings->getS32("crosshair_alpha"), 0, 255);
	crosshair_argb = video::SColor(cross_a, cross_r, cross_g, cross_b);

	v3f selectionbox_color = g_settings->getV3F("selectionbox_color");
	u32 sbox_r = rangelim(myround(selectionbox_color.X), 0, 255);
	u32 sbox_g = rangelim(myround(selectionbox_color.Y), 0, 255);
	u32 sbox_b = rangelim(myround(selectionbox_color.Z), 0, 255);
	selectionbox_argb = video::SColor(255, sbox_r, sbox_g, sbox_b);

	use_crosshair_image = tsrc->isKnownSourceImage("crosshair.png");

	hotbar_image = "";
	hotbar_image_items = 0;
	use_hotbar_image = false;
	hotbar_selected_image = "";
	use_hotbar_selected_image = false;

	m_selection_mesh = NULL;
	m_selection_boxes.clear();
	m_halo_boxes.clear();

	m_selection_pos = v3f(0.0, 0.0, 0.0);
	std::string mode = g_settings->get("node_highlighting");
	m_selection_material.Lighting = false;

	if (g_settings->getBool("enable_shaders")) {
		IShaderSource *shdrsrc = gamedef->getShaderSource();
		u16 shader_id = shdrsrc->getShader(
			mode == "halo" ? "selection_shader" : "default_shader", 1, 1);
		m_selection_material.MaterialType = shdrsrc->getShaderInfo(shader_id).material;
	} else {
		m_selection_material.MaterialType = video::EMT_TRANSPARENT_ALPHA_CHANNEL;
	}

	if (mode == "box") {
		m_use_selection_mesh = false;
		m_selection_material.Thickness =
			rangelim(g_settings->getS16("selectionbox_width"), 1, 5);	
	} else if (mode == "halo") {
		m_use_selection_mesh = true;
		m_selection_material.setTexture(0, tsrc->getTextureForMesh("halo.png"));
		m_selection_material.setFlag(video::EMF_BACK_FACE_CULLING, true);
	} else {
		m_selection_material.MaterialType = video::EMT_SOLID;
	}
}

Hud::~Hud()
{
	if (m_selection_mesh)
		m_selection_mesh->drop();
}

void Hud::drawItem(const ItemStack &item, const core::rect<s32>& rect,
		bool selected)
{
	if (selected) {
			/* draw hihlighting around selected item */
			if (use_hotbar_selected_image) {
				core::rect<s32> imgrect2 = rect;
				imgrect2.UpperLeftCorner.X  -= (m_padding*2);
				imgrect2.UpperLeftCorner.Y  -= (m_padding*2);
				imgrect2.LowerRightCorner.X += (m_padding*2);
				imgrect2.LowerRightCorner.Y += (m_padding*2);
					video::ITexture *texture = tsrc->getTexture(hotbar_selected_image);
					core::dimension2di imgsize(texture->getOriginalSize());
				draw2DImageFilterScaled(driver, texture, imgrect2,
						core::rect<s32>(core::position2d<s32>(0,0), imgsize),
						NULL, hbar_colors, true);
			} else {
				video::SColor c_outside(255,255,0,0);
				//video::SColor c_outside(255,0,0,0);
				//video::SColor c_inside(255,192,192,192);
				s32 x1 = rect.UpperLeftCorner.X;
				s32 y1 = rect.UpperLeftCorner.Y;
				s32 x2 = rect.LowerRightCorner.X;
				s32 y2 = rect.LowerRightCorner.Y;
				// Black base borders
				driver->draw2DRectangle(c_outside,
					core::rect<s32>(
					v2s32(x1 - m_padding, y1 - m_padding),
					v2s32(x2 + m_padding, y1)
					), NULL);
				driver->draw2DRectangle(c_outside,
					core::rect<s32>(
					v2s32(x1 - m_padding, y2),
					v2s32(x2 + m_padding, y2 + m_padding)
					), NULL);
				driver->draw2DRectangle(c_outside,
					core::rect<s32>(
					v2s32(x1 - m_padding, y1),
						v2s32(x1, y2)
					), NULL);
				driver->draw2DRectangle(c_outside,
					core::rect<s32>(
						v2s32(x2, y1),
					v2s32(x2 + m_padding, y2)
					), NULL);
				/*// Light inside borders
				driver->draw2DRectangle(c_inside,
					core::rect<s32>(
						v2s32(x1 - padding/2, y1 - padding/2),
						v2s32(x2 + padding/2, y1)
					), NULL);
				driver->draw2DRectangle(c_inside,
					core::rect<s32>(
						v2s32(x1 - padding/2, y2),
						v2s32(x2 + padding/2, y2 + padding/2)
					), NULL);
				driver->draw2DRectangle(c_inside,
					core::rect<s32>(
						v2s32(x1 - padding/2, y1),
						v2s32(x1, y2)
					), NULL);
				driver->draw2DRectangle(c_inside,
					core::rect<s32>(
						v2s32(x2, y1),
						v2s32(x2 + padding/2, y2)
					), NULL);
				*/
			}
		}

		video::SColor bgcolor2(128, 0, 0, 0);
		if (!use_hotbar_image)
			driver->draw2DRectangle(bgcolor2, rect, NULL);
		drawItemStack(driver, g_fontengine->getFont(), item, rect, NULL,
			gamedef, selected ? IT_ROT_SELECTED : IT_ROT_NONE);
	}

//NOTE: selectitem = 0 -> no selected; selectitem 1-based
void Hud::drawItems(v2s32 upperleftpos, s32 itemcount, s32 offset,
		InventoryList *mainlist, u16 selectitem, u16 direction)
{
#ifdef HAVE_TOUCHSCREENGUI
	if ( (g_touchscreengui) && (offset == 0))
		g_touchscreengui->resetHud();
#endif

	//only for !hotbar_image_items ===
	s32 height  = m_hotbar_imagesize + m_padding * 2;
	s32 width   = (itemcount - offset) * (m_hotbar_imagesize + m_padding * 2);

	if (direction == HUD_DIR_TOP_BOTTOM || direction == HUD_DIR_BOTTOM_TOP) {
		width  = m_hotbar_imagesize + m_padding * 2;
		height = (itemcount - offset) * (m_hotbar_imagesize + m_padding * 2);
	}
	//================================

	// Position of upper left corner of bar
	v2s32 pos = upperleftpos;

	if (hotbar_image != player->hotbar_image) {
		hotbar_image = player->hotbar_image;
		hotbar_image_items = player->hotbar_image_items;
		if (hotbar_image != "")
			use_hotbar_image = tsrc->isKnownSourceImage(hotbar_image);
		else
			use_hotbar_image = false;
	}

	if (hotbar_selected_image != player->hotbar_selected_image) {
		hotbar_selected_image = player->hotbar_selected_image;
		if (hotbar_selected_image != "")
			use_hotbar_selected_image = tsrc->isKnownSourceImage(hotbar_selected_image);
		else
			use_hotbar_selected_image = false;
	}

	/* draw customized item background */
	if (use_hotbar_image) {
	  if (!hotbar_image_items) {
		core::rect<s32> imgrect2(-m_padding/2, -m_padding/2,
				width+m_padding/2, height+m_padding/2);
		core::rect<s32> rect2 = imgrect2 + pos;
		video::ITexture *texture = tsrc->getTexture(hotbar_image);
		core::dimension2di imgsize(texture->getOriginalSize());
		draw2DImageFilterScaled(driver, texture, rect2,
			core::rect<s32>(core::position2d<s32>(0,0), imgsize),
			NULL, hbar_colors, true);

	  } else {
		video::ITexture *texture = tsrc->getTexture(hotbar_image);
		core::dimension2di imgsize(texture->getOriginalSize());
		// todo: maybe deal with hotbar_image_items>1
		core::rect<s32> rect(-m_padding, -m_padding,
			m_hotbar_imagesize + m_padding, m_hotbar_imagesize + m_padding);
		rect += pos;
		core::position2d<s32> step(0, 0);
		(direction == HUD_DIR_TOP_BOTTOM || direction == HUD_DIR_BOTTOM_TOP ?
			step.Y : step.X) = m_hotbar_imagesize + m_padding * 2;
		for (int i = 0; i < itemcount - offset; i++) {
			draw2DImageFilterScaled(driver,texture, rect,
				core::rect<s32>(core::position2d<s32>(0, 0), imgsize),
				NULL, hbar_colors, true);
			rect += step;
		}
	  }

	}

	for (s32 i = offset; i < itemcount && (size_t)i < mainlist->getSize(); i++)
	{
		v2s32 steppos;
		s32 fullimglen = m_hotbar_imagesize + m_padding * 2;

		core::rect<s32> imgrect;
		if (!hotbar_image_items)
			imgrect = core::rect<s32>(0, 0, m_hotbar_imagesize, m_hotbar_imagesize);
		else
			imgrect = core::rect<s32>(-m_padding, -m_padding, m_hotbar_imagesize - m_padding, m_hotbar_imagesize - m_padding);

		switch (direction) {
			case HUD_DIR_RIGHT_LEFT:
				steppos = v2s32(-(m_padding + (i - offset) * fullimglen), m_padding);
				break;
			case HUD_DIR_TOP_BOTTOM:
				steppos = v2s32(m_padding, m_padding + (i - offset) * fullimglen);
				break;
			case HUD_DIR_BOTTOM_TOP:
				steppos = v2s32(m_padding, -(m_padding + (i - offset) * fullimglen));
				break;
			default:
				steppos = v2s32(m_padding + (i - offset) * fullimglen, m_padding);
				break;
		}

		drawItem(mainlist->getItem(i), (imgrect + pos + steppos), (i +1) == selectitem );

#ifdef HAVE_TOUCHSCREENGUI
		if (g_touchscreengui)
			g_touchscreengui->registerHudItem(i, (imgrect + pos + steppos));
#endif
	}
}


void Hud::drawLuaElements(const v3s16 &camera_offset) {
	u32 text_height = g_fontengine->getTextHeight();
	irr::gui::IGUIFont* font = g_fontengine->getFont();
	for (size_t i = 0; i != player->maxHudId(); i++) {
		HudElement *e = player->getHud(i);
		if (!e)
			continue;

		v2s32 pos(floor(e->pos.X * (float) m_screensize.X + 0.5),
				floor(e->pos.Y * (float) m_screensize.Y + 0.5));
		switch (e->type) {
			case HUD_ELEM_IMAGE: {
				video::ITexture *texture = tsrc->getTexture(e->text);
				if (!texture)
					continue;

				const video::SColor color(255, 255, 255, 255);
				const video::SColor colors[] = {color, color, color, color};
				core::dimension2di imgsize(texture->getOriginalSize());
				v2s32 dstsize(imgsize.Width * e->scale.X,
				              imgsize.Height * e->scale.Y);
				if (e->scale.X < 0)
					dstsize.X = m_screensize.X * (e->scale.X * -0.01);
				if (e->scale.Y < 0)
					dstsize.Y = m_screensize.Y * (e->scale.Y * -0.01);
				v2s32 offset((e->align.X - 1.0) * dstsize.X / 2,
				             (e->align.Y - 1.0) * dstsize.Y / 2);
				core::rect<s32> rect(0, 0, dstsize.X, dstsize.Y);
				rect += pos + offset + v2s32(e->offset.X, e->offset.Y);
				draw2DImageFilterScaled(driver, texture, rect,
					core::rect<s32>(core::position2d<s32>(0,0), imgsize),
					NULL, colors, true);
				break; }
			case HUD_ELEM_TEXT: {
				video::SColor color(255, (e->number >> 16) & 0xFF,
										 (e->number >> 8)  & 0xFF,
										 (e->number >> 0)  & 0xFF);
				core::rect<s32> size(0, 0, e->scale.X, text_height * e->scale.Y);
				std::wstring text = utf8_to_wide(e->text);
				core::dimension2d<u32> textsize = font->getDimension(text.c_str());
				v2s32 offset((e->align.X - 1.0) * (textsize.Width / 2),
				             (e->align.Y - 1.0) * (textsize.Height / 2));
				v2s32 offs(e->offset.X, e->offset.Y);
				font->draw(text.c_str(), size + pos + offset + offs, color);
				break; }
			case HUD_ELEM_STATBAR: {
				v2s32 offs(e->offset.X, e->offset.Y);
				drawStatbar(pos, HUD_CORNER_UPPER, e->dir, e->text, e->number, offs, e->size);
				break; }
			case HUD_ELEM_INVENTORY: {
				InventoryList *inv = inventory->getList(e->text);
				drawItems(pos, e->number, 0, inv, e->item, e->dir);
				break; }
			case HUD_ELEM_WAYPOINT: {
				v3f p_pos = player->getPosition() / BS;
				v3f w_pos = e->world_pos * BS;
				float distance = floor(10 * p_pos.getDistanceFrom(e->world_pos)) / 10;
				scene::ICameraSceneNode* camera = smgr->getActiveCamera();
				w_pos -= intToFloat(camera_offset, BS);
				core::matrix4 trans = camera->getProjectionMatrix();
				trans *= camera->getViewMatrix();
				f32 transformed_pos[4] = { w_pos.X, w_pos.Y, w_pos.Z, 1.0f };
				trans.multiplyWith1x4Matrix(transformed_pos);
				if (transformed_pos[3] < 0)
					break;
				f32 zDiv = transformed_pos[3] == 0.0f ? 1.0f :
					core::reciprocal(transformed_pos[3]);
				pos.X = m_screensize.X * (0.5 * transformed_pos[0] * zDiv + 0.5);
				pos.Y = m_screensize.Y * (0.5 - transformed_pos[1] * zDiv * 0.5);
				video::SColor color(255, (e->number >> 16) & 0xFF,
										 (e->number >> 8)  & 0xFF,
										 (e->number >> 0)  & 0xFF);
				core::rect<s32> size(0, 0, 200, 2 * text_height);
				std::wstring text = utf8_to_wide(e->name);
				font->draw(text.c_str(), size + pos, color);
				std::ostringstream os;
				os << distance << e->text;
				text = utf8_to_wide(os.str());
				pos.Y += text_height;
				font->draw(text.c_str(), size + pos, color);
				break; }
			default:
				infostream << "Hud::drawLuaElements: ignoring drawform " << e->type <<
					" of hud element ID " << i << " due to unrecognized type" << std::endl;
		}
	}
}


void Hud::drawStatbar(v2s32 pos, u16 corner, u16 drawdir, std::string texture,
		s32 count, v2s32 offset, v2s32 size)
{
	const video::SColor color(255, 255, 255, 255);
	const video::SColor colors[] = {color, color, color, color};

	video::ITexture *stat_texture = tsrc->getTexture(texture);
	if (!stat_texture)
		return;

	core::dimension2di srcd(stat_texture->getOriginalSize());
	core::dimension2di dstd;
	if (size == v2s32()) {
		dstd = srcd;
	} else {
		double size_factor = g_settings->getFloat("hud_scaling") *
				porting::getDisplayDensity();
		dstd.Height = size.Y * size_factor;
		dstd.Width  = size.X * size_factor;
		offset.X *= size_factor;
		offset.Y *= size_factor;
	}

	v2s32 p = pos;
	if (corner & HUD_CORNER_LOWER)
		p -= dstd.Height;

	p += offset;

	v2s32 steppos;
	switch (drawdir) {
		case HUD_DIR_RIGHT_LEFT:
			steppos = v2s32(-1, 0);
			break;
		case HUD_DIR_TOP_BOTTOM:
			steppos = v2s32(0, 1);
			break;
		case HUD_DIR_BOTTOM_TOP:
			steppos = v2s32(0, -1);
			break;
		default:
			steppos = v2s32(1, 0);
	}
	steppos.X *= dstd.Width;
	steppos.Y *= dstd.Height;

	for (s32 i = 0; i < count / 2; i++)
	{
		core::rect<s32> srcrect(0, 0, srcd.Width, srcd.Height);
		core::rect<s32> dstrect(0,0, dstd.Width, dstd.Height);

		dstrect += p;
		draw2DImageFilterScaled(driver, stat_texture, dstrect, srcrect, NULL, colors, true);
		p += steppos;
	}

	if (count % 2 == 1)
	{
		core::rect<s32> srcrect(0, 0, srcd.Width / 2, srcd.Height);
		core::rect<s32> dstrect(0,0, dstd.Width / 2, dstd.Height);

		dstrect += p;
		draw2DImageFilterScaled(driver, stat_texture, dstrect, srcrect, NULL, colors, true);
	}
}


void Hud::drawHotbar(u16 playeritem) {

	v2s32 centerlowerpos(m_displaycenter.X, m_screensize.Y);

	InventoryList *mainlist = inventory->getList("main");
	if (mainlist == NULL) {
		//silently ignore this we may not be initialized completely
		return;
	}

	s32 hotbar_itemcount = player->hud_hotbar_itemcount;
	s32 width = hotbar_itemcount * (m_hotbar_imagesize + m_padding * 2);
	v2s32 pos = centerlowerpos - v2s32(width / 2, m_hotbar_imagesize + m_padding * 3);

	if ( (float) width / (float) porting::getWindowSize().X <=
			g_settings->getFloat("hud_hotbar_max_width")) {
		if (player->hud_flags & HUD_FLAG_HOTBAR_VISIBLE) {
			drawItems(pos, hotbar_itemcount, 0, mainlist, playeritem + 1, 0);
		}
	}
	else {
		pos.X += width/4;

		v2s32 secondpos = pos;
		pos = pos - v2s32(0, m_hotbar_imagesize + m_padding * (hotbar_image_items ? 2 : 1));

		if (player->hud_flags & HUD_FLAG_HOTBAR_VISIBLE) {
			drawItems(pos, hotbar_itemcount/2, 0, mainlist, playeritem + 1, 0);
			drawItems(secondpos, hotbar_itemcount, hotbar_itemcount/2, mainlist, playeritem + 1, 0);
		}
	}
}


void Hud::drawCrosshair() {

	if (use_crosshair_image) {
		video::ITexture *crosshair = tsrc->getTexture("crosshair.png");
		v2u32 size  = crosshair->getOriginalSize();
		v2s32 lsize = v2s32(m_displaycenter.X - (size.X / 2),
				m_displaycenter.Y - (size.Y / 2));
		driver->draw2DImage(crosshair, lsize,
				core::rect<s32>(0, 0, size.X, size.Y),
				0, crosshair_argb, true);
	} else {
		driver->draw2DLine(m_displaycenter - v2s32(10, 0),
				m_displaycenter + v2s32(10, 0), crosshair_argb);
		driver->draw2DLine(m_displaycenter - v2s32(0, 10),
				m_displaycenter + v2s32(0, 10), crosshair_argb);
	}
}

void Hud::setSelectionPos(const v3f &pos, const v3s16 &camera_offset)
{
	m_camera_offset = camera_offset;
	m_selection_pos = pos;
	m_selection_pos_with_offset = pos - intToFloat(camera_offset, BS);
}
	
void Hud::drawSelectionMesh()
{	
	if (!m_use_selection_mesh) {
		// Draw 3D selection boxes
		video::SMaterial oldmaterial = driver->getMaterial2D();
		driver->setMaterial(m_selection_material);
		for (std::vector<aabb3f>::const_iterator
				i = m_selection_boxes.begin();
				i != m_selection_boxes.end(); ++i) {
			aabb3f box = aabb3f(
				i->MinEdge + m_selection_pos_with_offset,
				i->MaxEdge + m_selection_pos_with_offset);
			
			u32 r = (selectionbox_argb.getRed() *
					m_selection_mesh_color.getRed() / 255);		
			u32 g = (selectionbox_argb.getGreen() *
					m_selection_mesh_color.getGreen() / 255);
			u32 b = (selectionbox_argb.getBlue() *
					m_selection_mesh_color.getBlue() / 255);
			driver->draw3DBox(box, video::SColor(255, r, g, b));
		}
		driver->setMaterial(oldmaterial);
	} else if (m_selection_mesh) {
		// Draw selection mesh
		video::SMaterial oldmaterial = driver->getMaterial2D();
		driver->setMaterial(m_selection_material);
		setMeshColor(m_selection_mesh, m_selection_mesh_color);
		scene::IMesh* mesh = cloneMesh(m_selection_mesh);
		translateMesh(mesh, m_selection_pos_with_offset);
		u32 mc = m_selection_mesh->getMeshBufferCount();
		for (u32 i = 0; i < mc; i++) {
			scene::IMeshBuffer *buf = mesh->getMeshBuffer(i);
			driver->drawMeshBuffer(buf);
		}
		mesh->drop();
		driver->setMaterial(oldmaterial);
	}
}

void Hud::updateSelectionMesh(const v3s16 &camera_offset)
{
	m_camera_offset = camera_offset;
	if (!m_use_selection_mesh)
		return;

	if (m_selection_mesh) {
		m_selection_mesh->drop();
		m_selection_mesh = NULL;
	}

	if (!m_selection_boxes.size()) {
		// No pointed object
		return;
	}

	// New pointed object, create new mesh.

	// Texture UV coordinates for selection boxes
	static f32 texture_uv[24] = {
		0,0,1,1,
		0,0,1,1,
		0,0,1,1,
		0,0,1,1,
		0,0,1,1,
		0,0,1,1
	};

	// Use single halo box instead of multiple overlapping boxes.
	// Temporary solution - problem can be solved with multiple
	// rendering targets, or some method to remove inner surfaces.
	// Thats because of halo transparency.

	aabb3f halo_box(100.0, 100.0, 100.0, -100.0, -100.0, -100.0);
	m_halo_boxes.clear();

	for (std::vector<aabb3f>::iterator
			i = m_selection_boxes.begin();
			i != m_selection_boxes.end(); ++i) {
		halo_box.addInternalBox(*i);
	}

	m_halo_boxes.push_back(halo_box);
	m_selection_mesh = convertNodeboxesToMesh(
		m_halo_boxes, texture_uv, 0.5);
}

void Hud::resizeHotbar() {
	if (m_screensize != porting::getWindowSize()) {
		m_hotbar_imagesize = floor(HOTBAR_IMAGE_SIZE * porting::getDisplayDensity() + 0.5);
		m_hotbar_imagesize *= g_settings->getFloat("hud_scaling");
		m_padding = m_hotbar_imagesize / 12;
		m_screensize = porting::getWindowSize();
		m_displaycenter = v2s32(m_screensize.X/2,m_screensize.Y/2);
	}
}

struct MeshTimeInfo {
	s32 time;
	scene::IMesh *mesh;
};

void drawItemStack(video::IVideoDriver *driver,
		gui::IGUIFont *font,
		const ItemStack &item,
		const core::rect<s32> &rect,
		const core::rect<s32> *clip,
		IGameDef *gamedef,
		ItemRotationKind rotation_kind)
{
	static MeshTimeInfo rotation_time_infos[IT_ROT_NONE];
	static bool enable_animations =
		g_settings->getBool("inventory_items_animations");

	if (item.empty()) {
		if (rotation_kind < IT_ROT_NONE) {
			rotation_time_infos[rotation_kind].mesh = NULL;
		}
		return;
	}

	const ItemDefinition &def = item.getDefinition(gamedef->idef());
	scene::IMesh* mesh = gamedef->idef()->getWieldMesh(def.name, gamedef);

	if (mesh) {
		driver->clearZBuffer();
		s32 delta = 0;
		if (rotation_kind < IT_ROT_NONE) {
			MeshTimeInfo &ti = rotation_time_infos[rotation_kind];
			if (mesh != ti.mesh) {
				ti.mesh = mesh;
				ti.time = getTimeMs();
			} else {
				delta = porting::getDeltaMs(ti.time, getTimeMs()) % 100000;
			}
		}
		core::rect<s32> oldViewPort = driver->getViewPort();
		core::matrix4 oldProjMat = driver->getTransform(video::ETS_PROJECTION);
		core::matrix4 oldViewMat = driver->getTransform(video::ETS_VIEW);
		core::matrix4 ProjMatrix;
		ProjMatrix.buildProjectionMatrixOrthoLH(2, 2, -1, 100);
		driver->setTransform(video::ETS_PROJECTION, ProjMatrix);
		driver->setTransform(video::ETS_VIEW, ProjMatrix);
		core::matrix4 matrix;
		matrix.makeIdentity();

		if (enable_animations) {
			float timer_f = (float)delta / 5000.0;
			matrix.setRotationDegrees(core::vector3df(0, 360 * timer_f, 0));
		}

		driver->setTransform(video::ETS_WORLD, matrix);
		driver->setViewPort(rect);

		u32 mc = mesh->getMeshBufferCount();
		for (u32 j = 0; j < mc; ++j) {
			scene::IMeshBuffer *buf = mesh->getMeshBuffer(j);
			video::SMaterial &material = buf->getMaterial();
			material.MaterialType = video::EMT_TRANSPARENT_ALPHA_CHANNEL_REF;
			material.Lighting = false;
			driver->setMaterial(material);
			driver->drawMeshBuffer(buf);
		}

		driver->setTransform(video::ETS_VIEW, oldViewMat);
		driver->setTransform(video::ETS_PROJECTION, oldProjMat);
		driver->setViewPort(oldViewPort);
	}

	if(def.type == ITEM_TOOL && item.wear != 0)
	{
		// Draw a progressbar
		float barheight = rect.getHeight()/16;
		float barpad_x = rect.getWidth()/16;
		float barpad_y = rect.getHeight()/16;
		core::rect<s32> progressrect(
			rect.UpperLeftCorner.X + barpad_x,
			rect.LowerRightCorner.Y - barpad_y - barheight,
			rect.LowerRightCorner.X - barpad_x,
			rect.LowerRightCorner.Y - barpad_y);

		// Shrink progressrect by amount of tool damage
		float wear = item.wear / 65535.0;
		int progressmid =
			wear * progressrect.UpperLeftCorner.X +
			(1-wear) * progressrect.LowerRightCorner.X;

		// Compute progressbar color
		//   wear = 0.0: green
		//   wear = 0.5: yellow
		//   wear = 1.0: red
		video::SColor color(255,255,255,255);
		int wear_i = MYMIN(floor(wear * 600), 511);
		wear_i = MYMIN(wear_i + 10, 511);
		if(wear_i <= 255)
			color.set(255, wear_i, 255, 0);
		else
			color.set(255, 255, 511-wear_i, 0);

		core::rect<s32> progressrect2 = progressrect;
		progressrect2.LowerRightCorner.X = progressmid;
		driver->draw2DRectangle(color, progressrect2, clip);

		color = video::SColor(255,0,0,0);
		progressrect2 = progressrect;
		progressrect2.UpperLeftCorner.X = progressmid;
		driver->draw2DRectangle(color, progressrect2, clip);
	}

	if(font != NULL && item.count >= 2)
	{
		// Get the item count as a string
		std::string text = itos(item.count);
		v2u32 dim = font->getDimension(utf8_to_wide(text).c_str());
		v2s32 sdim(dim.X,dim.Y);

		core::rect<s32> rect2(
			/*rect.UpperLeftCorner,
			core::dimension2d<u32>(rect.getWidth(), 15)*/
			rect.LowerRightCorner - sdim,
			sdim
		);

		video::SColor bgcolor(128,0,0,0);
		driver->draw2DRectangle(bgcolor, rect2, clip);

		video::SColor color(255,255,255,255);
		font->draw(text.c_str(), rect2, color, false, false, clip);
	}
}
