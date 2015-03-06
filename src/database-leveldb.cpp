/*
database-leveldb.cpp
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

#include "config.h"

#if USE_LEVELDB

#include "database-leveldb.h"
#include "map.h"
#include "mapblock.h"
#include "serialization.h"
#include "main.h"
#include "settings.h"
#include "log_types.h"
#include "filesys.h"

#define ENSURE_STATUS_OK(s) \
	if (!(s).ok()) { \
		throw FileNotGoodException(std::string("LevelDB error: ") + (s).ToString()); \
	}

Database_LevelDB::Database_LevelDB(ServerMap *map, std::string savedir)
{
	m_database = new KeyValueStorage(savedir, "map");
	//srvmap = map;
}

int Database_LevelDB::Initialized(void)
{
	return 1;
}

void Database_LevelDB::beginSave() {}
void Database_LevelDB::endSave() {}

bool Database_LevelDB::saveBlock(v3POS blockpos, std::string &data)
{
	if (!m_database->put(getBlockAsString(blockpos), data)) {
		errorstream << "WARNING: saveBlock: LevelDB error saving block "
			<< blockpos <<": "<< m_database->get_error() << std::endl;
		return false;
	}
	m_database->del(i64tos(getBlockAsInteger(blockpos))); // delete old format

	return true;
}

std::string Database_LevelDB::loadBlock(v3POS blockpos)
{
	std::string datastr;

	m_database->get(getBlockAsString(blockpos), datastr);
	if (datastr.length())
		return datastr;

	m_database->get(i64tos(getBlockAsInteger(blockpos)), datastr);

	return datastr;

}

bool Database_LevelDB::deleteBlock(v3s16 blockpos)
{
	auto ok = m_database->del(
			(getBlockAsString(blockpos)));
	if (ok) {
		errorstream << "WARNING: deleteBlock: LevelDB error deleting block "
			<< (blockpos) << std::endl;
		return false;
	}

	return true;
}

void Database_LevelDB::listAllLoadableBlocks(std::vector<v3s16> &dst)
{
#if USE_LEVELDB
	auto it = m_database->new_iterator();
	for (it->SeekToFirst(); it->Valid(); it->Next()) {
		dst.push_back(getStringAsBlock(it->key().ToString()));
	}
	ENSURE_STATUS_OK(it->status());  // Check for any errors found during the scan
	delete it;
#endif
}

Database_LevelDB::~Database_LevelDB()
{
	delete m_database;
}
#endif
