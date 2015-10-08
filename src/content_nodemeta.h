/*
content_nodemeta.h
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

#ifndef CONTENT_NODEMETA_HEADER
#define CONTENT_NODEMETA_HEADER

#include <iostream>

class NodeMetadataList;
class NodeTimerList;
class IItemDefManager;

/*
	Legacy nodemeta definitions
*/

void content_nodemeta_deserialize_legacy(std::istream &is,
		NodeMetadataList *meta, NodeTimerList *timers,
		IItemDefManager *item_def_mgr);

void content_nodemeta_serialize_legacy(std::ostream &os, NodeMetadataList *meta);

#endif

