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

#ifndef OBJECT_PROPERTIES_HEADER
#define OBJECT_PROPERTIES_HEADER

#include <string>
#include "irrlichttypes_bloated.h"
#include <iostream>
#include <map>
#include <vector>

struct ObjectProperties
{
	// Values are BS=1
	s16 hp_max;
	bool physical;
	bool collideWithObjects;
	float weight;
	core::aabbox3d<f32> collisionbox;
	std::string visual;
	std::string mesh;
	v2f visual_size;
	std::vector<std::string> textures;
	std::vector<video::SColor> colors;
	v2s16 spritediv;
	v2s16 initial_sprite_basepos;
	bool is_visible;
	bool makes_footstep_sound;
	float automatic_rotate;
	f32 stepheight;
	bool automatic_face_movement_dir;
	f32 automatic_face_movement_dir_offset;
	bool force_load;
	bool backface_culling;
	std::string nametag;
	video::SColor nametag_color;
	f32 automatic_face_movement_max_rotation_per_sec;

	ObjectProperties();
	std::string dump();
	void serialize(std::ostream &os) const;
	void deSerialize(std::istream &is);
};

#endif
