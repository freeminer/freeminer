/*
database-dummy.cpp
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

/*
Dummy database class
*/

#include "database-dummy.h"

bool Database_Dummy::saveBlock(const v3s16 &pos, const std::string &data)
{
	m_database.set(getBlockAsString(pos), data);
	return true;
}

std::string Database_Dummy::loadBlock(const v3s16 &pos)
{
	if (m_database.count(getBlockAsString(pos)))
		return m_database.get(getBlockAsString(pos));
	else
		return "";
}

bool Database_Dummy::deleteBlock(const v3s16 &pos)
{
	m_database.erase(getBlockAsString(pos));
	return true;
}

void Database_Dummy::listAllLoadableBlocks(std::vector<v3s16> &dst)
{
	for(auto &x : m_database) {
		dst.push_back(getStringAsBlock(x.first));
	}
}

