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
Dummy "database" class
*/


#include "database-dummy.h"

#include "map.h"
#include "mapblock.h"
#include "serialization.h"
#include "main.h"
#include "settings.h"
#include "log.h"

Database_Dummy::Database_Dummy(ServerMap *map)
{
	srvmap = map;
}

int Database_Dummy::Initialized(void)
{
	return 1;
}

void Database_Dummy::beginSave() {}
void Database_Dummy::endSave() {}

bool Database_Dummy::saveBlock(v3s16 blockpos, std::string &data)
{
	m_database[getBlockAsString(blockpos)] = data;
	return true;
}

std::string Database_Dummy::loadBlock(v3s16 blockpos)
{
	if (m_database.count(getBlockAsString(blockpos)))
		return m_database[getBlockAsString(blockpos)];
	else
		return "";
}

void Database_Dummy::listAllLoadableBlocks(std::list<v3s16> &dst)
{
	for(auto &x : m_database)
	{
		v3s16 p = getStringAsBlock(x.first);
		//dstream<<"block_i="<<block_i<<" p="<<PP(p)<<std::endl;
		dst.push_back(p);
	}
}

Database_Dummy::~Database_Dummy()
{
	m_database.clear();
}
