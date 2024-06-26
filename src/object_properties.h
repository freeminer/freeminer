/*
object_properties.h
Copyright (C) 2013 celeron55, Perttu Ahola <celeron55@gmail.com>
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

#pragma once

#include <optional>
#include <string>
#include "irrlichttypes_bloated.h"
#include <iostream>
#include <map>
#include <vector>
#include "threading/concurrent_vector.h"
#include "threading/lock.h"

struct ObjectProperties : public shared_locker 
{
	u16 hp_max = 1;
	u16 breath_max = 0;
	bool physical = false;
	bool collideWithObjects = true;
	// Values are BS=1
	aabb3f collisionbox = aabb3f(-0.5f, -0.5f, -0.5f, 0.5f, 0.5f, 0.5f);
	aabb3f selectionbox = aabb3f(-0.5f, -0.5f, -0.5f, 0.5f, 0.5f, 0.5f);
	bool rotate_selectionbox = false;
	bool pointable = true;
	std::string visual = "sprite";
	std::string mesh = "";
	v3f visual_size = v3f(1, 1, 1);
	concurrent_vector<std::string> textures;
	std::string damage_texture_modifier = "^[brighten";
	std::vector<video::SColor> colors;
	v2s16 spritediv = v2s16(1, 1);
	v2s16 initial_sprite_basepos;
	bool is_visible = true;
	bool makes_footstep_sound = false;
	f32 stepheight = 0.0f;
	float automatic_rotate = 0.0f;
	bool automatic_face_movement_dir = false;
	f32 automatic_face_movement_dir_offset = 0.0f;
	bool force_load = false;
	bool backface_culling = true;
	s8 glow = 0;
	std::string nametag = "";
	video::SColor nametag_color = video::SColor(255, 255, 255, 255);
	std::optional<video::SColor> nametag_bgcolor = std::nullopt;
	f32 automatic_face_movement_max_rotation_per_sec = -1.0f;
	std::string infotext;
	//! For dropped items, this contains item information.
	std::string wield_item;
	bool static_save = true;
	float eye_height = 1.625f;
	float zoom_fov = 0.0f;
	bool use_texture_alpha = false;
	bool shaded = true;
	bool show_on_minimap = false;

	ObjectProperties();
	std::string dump();
	// check limits of some important properties (strings) that'd cause exceptions later on
	bool validate();
	void serialize(std::ostream &os) const;
	void deSerialize(std::istream &is);
};
