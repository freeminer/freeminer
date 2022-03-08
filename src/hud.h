/*
hud.h
Copyright (C) 2010-2013 kwolekr, Ryan Kwolek <kwolekr@minetest.net>
<<<<<<< HEAD
*/
=======
Copyright (C) 2017 red-001 <red-001@outlook.ie>
>>>>>>> 5.5.0

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

#pragma once

#include "irrlichttypes_extrabloated.h"
#include <string>
#include "common/c_types.h"

#define HUD_DIR_LEFT_RIGHT 0
#define HUD_DIR_RIGHT_LEFT 1
#define HUD_DIR_TOP_BOTTOM 2
#define HUD_DIR_BOTTOM_TOP 3

#define HUD_CORNER_UPPER  0
#define HUD_CORNER_LOWER  1
#define HUD_CORNER_CENTER 2

#define HUD_STYLE_BOLD   1
#define HUD_STYLE_ITALIC 2
#define HUD_STYLE_MONO   4

// Note that these visibility flags do not determine if the hud items are
// actually drawn, but rather, whether to draw the item should the rest
// of the game state permit it.
#define HUD_FLAG_HOTBAR_VISIBLE        (1 << 0)
#define HUD_FLAG_HEALTHBAR_VISIBLE     (1 << 1)
#define HUD_FLAG_CROSSHAIR_VISIBLE     (1 << 2)
#define HUD_FLAG_WIELDITEM_VISIBLE     (1 << 3)
#define HUD_FLAG_BREATHBAR_VISIBLE     (1 << 4)
#define HUD_FLAG_MINIMAP_VISIBLE       (1 << 5)
#define HUD_FLAG_MINIMAP_RADAR_VISIBLE (1 << 6)

#define HUD_PARAM_HOTBAR_ITEMCOUNT 1
#define HUD_PARAM_HOTBAR_IMAGE 2
static const int  HUD_PARAM_HOTBAR_IMAGE_ITEMS = 104;
#define HUD_PARAM_HOTBAR_SELECTED_IMAGE 3

#define HUD_HOTBAR_ITEMCOUNT_DEFAULT 8
#define HUD_HOTBAR_ITEMCOUNT_MAX     32

#define HOTBAR_IMAGE_SIZE 48

enum HudElementType {
	HUD_ELEM_IMAGE     = 0,
	HUD_ELEM_TEXT      = 1,
	HUD_ELEM_STATBAR   = 2,
	HUD_ELEM_INVENTORY = 3,
	HUD_ELEM_WAYPOINT  = 4,
	HUD_ELEM_IMAGE_WAYPOINT = 5,
	HUD_ELEM_COMPASS   = 6,
	HUD_ELEM_MINIMAP   = 7
};

enum HudElementStat {
	HUD_STAT_POS = 0,
	HUD_STAT_NAME,
	HUD_STAT_SCALE,
	HUD_STAT_TEXT,
	HUD_STAT_NUMBER,
	HUD_STAT_ITEM,
	HUD_STAT_DIR,
	HUD_STAT_ALIGN,
	HUD_STAT_OFFSET,
	HUD_STAT_WORLD_POS,
	HUD_STAT_SIZE,
	HUD_STAT_Z_INDEX,
	HUD_STAT_TEXT2,
	HUD_STAT_STYLE,
};

enum HudCompassDir {
	HUD_COMPASS_ROTATE = 0,
	HUD_COMPASS_ROTATE_REVERSE,
	HUD_COMPASS_TRANSLATE,
	HUD_COMPASS_TRANSLATE_REVERSE,
};

struct HudElement {
	HudElementType type;
	v2f pos;
	std::string name;
	v2f scale;
	std::string text;
	u32 number;
	u32 item;
	u32 dir;
	v2f align;
	v2f offset;
	v3f world_pos;
	v2s32 size;
	s16 z_index = 0;
	std::string text2;
	u32 style;
};

extern const EnumString es_HudElementType[];
extern const EnumString es_HudElementStat[];
extern const EnumString es_HudBuiltinElement[];

// Minimap stuff

<<<<<<< HEAD
class IGameDef;
class ITextureSource;
class Inventory;
class InventoryList;
class LocalPlayer;
struct ItemStack;

class Hud {
public:
	video::IVideoDriver *driver;
	scene::ISceneManager* smgr;
	gui::IGUIEnvironment *guienv;
	IGameDef *gamedef;
	LocalPlayer *player;
	Inventory *inventory;
	ITextureSource *tsrc;

	video::SColor crosshair_argb;
	video::SColor selectionbox_argb;
	bool use_crosshair_image;
	std::string hotbar_image;
	int hotbar_image_items;
	bool use_hotbar_image;
	std::string hotbar_selected_image;
	bool use_hotbar_selected_image;

	Hud(video::IVideoDriver *driver,scene::ISceneManager* smgr,
		gui::IGUIEnvironment* guienv, IGameDef *gamedef, LocalPlayer *player,
		Inventory *inventory);
	~Hud();

	void drawHotbar(u16 playeritem);
	void resizeHotbar();
	void drawCrosshair();
	void drawSelectionMesh();
	void updateSelectionMesh(const v3s16 &camera_offset);

	std::vector<aabb3f> *getSelectionBoxes()
	{ return &m_selection_boxes; }

	void setSelectionPos(const v3f &pos, const v3s16 &camera_offset);

	v3f getSelectionPos() const
	{ return m_selection_pos; }

	void setSelectionMeshColor(const video::SColor &color)
	{ m_selection_mesh_color = color; }

	void setSelectedFaceNormal(const v3f &face_normal)
	{ m_selected_face_normal = face_normal; }

	void drawLuaElements(const v3s16 &camera_offset);

private:
	void drawStatbar(v2s32 pos, u16 corner, u16 drawdir, std::string texture,
			s32 count, v2s32 offset, v2s32 size=v2s32());

	void drawItems(v2s32 upperleftpos, v2s32 screen_offset, s32 itemcount,
		s32 inv_offset, InventoryList *mainlist, u16 selectitem, u16 direction);

	void drawItem(const ItemStack &item, const core::rect<s32>& rect,
		bool selected);

	float m_hud_scaling; // cached minetest setting
	v3s16 m_camera_offset;
	v2u32 m_screensize;
	v2s32 m_displaycenter;
	s32 m_hotbar_imagesize; // Takes hud_scaling into account, updated by resizeHotbar()
	s32 m_padding;  // Takes hud_scaling into account, updated by resizeHotbar()
	video::SColor hbar_colors[4];

	std::vector<aabb3f> m_selection_boxes;
	std::vector<aabb3f> m_halo_boxes;
	v3f m_selection_pos;
	v3f m_selection_pos_with_offset;

	scene::IMesh* m_selection_mesh;
	video::SColor m_selection_mesh_color;
	v3f m_selected_face_normal;

	video::SMaterial m_selection_material;
	bool m_use_selection_mesh;
=======
enum MinimapType {
	MINIMAP_TYPE_OFF,
	MINIMAP_TYPE_SURFACE,
	MINIMAP_TYPE_RADAR,
	MINIMAP_TYPE_TEXTURE,
>>>>>>> 5.5.0
};

