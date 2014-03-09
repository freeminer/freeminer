/*
content_object.h
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

#ifndef CONTENT_OBJECT_HEADER
#define CONTENT_OBJECT_HEADER

#define ACTIVEOBJECT_TYPE_TEST 1
#define ACTIVEOBJECT_TYPE_ITEM 2
#define ACTIVEOBJECT_TYPE_RAT 3
#define ACTIVEOBJECT_TYPE_OERKKI1 4
#define ACTIVEOBJECT_TYPE_FIREFLY 5
#define ACTIVEOBJECT_TYPE_MOBV2 6

#define ACTIVEOBJECT_TYPE_LUAENTITY 7

// Special type, not stored as a static object
#define ACTIVEOBJECT_TYPE_PLAYER 100

// Special type, only exists as CAO
#define ACTIVEOBJECT_TYPE_GENERIC 101

#endif

