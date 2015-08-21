/*
content_mapnode.h
Copyright (C) 2010-2013 celeron55, Perttu Ahola <celeron55@gmail.com>
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

#ifndef CONTENT_MAPNODE_HEADER
#define CONTENT_MAPNODE_HEADER

#include "mapnode.h"

/*
	Legacy node definitions
*/

// Backwards compatibility for non-extended content types in v19
extern content_t trans_table_19[21][2];
MapNode mapnode_translate_to_internal(MapNode n_from, u8 version);

// Get legacy node name mapping for loading old blocks
class NameIdMapping;
void content_mapnode_get_name_id_mapping(NameIdMapping *nimap);

// Convert "CONTENT_STONE"-style names to dynamic ids
std::string content_mapnode_get_new_name(const std::string &oldname);
class INodeDefManager;

#endif
