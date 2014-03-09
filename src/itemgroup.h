/*
itemgroup.h
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

#ifndef ITEMGROUP_HEADER
#define ITEMGROUP_HEADER

#include <string>
#include <map>

typedef std::map<std::string, int> ItemGroupList;

static inline int itemgroup_get(const ItemGroupList &groups,
		const std::string &name)
{
	std::map<std::string, int>::const_iterator i = groups.find(name);
	if(i == groups.end())
		return 0;
	return i->second;
}

#endif

