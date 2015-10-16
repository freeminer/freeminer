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
#include "log_types.h"
#include "filesys.h"
#include "exceptions.h"
#include "util/string.h"

#include "leveldb/db.h"


#define ENSURE_STATUS_OK(s) \
	if (!(s).ok()) { \
		throw FileNotGoodException(std::string("LevelDB error: ") + \
				(s).ToString()); \
	}


Database_LevelDB::Database_LevelDB(const std::string &savedir)
	: m_database(savedir, "map")
{
}

Database_LevelDB::~Database_LevelDB()
{
}

bool Database_LevelDB::saveBlock(const v3s16 &pos, const std::string &data)
{
	if (!m_database.put(getBlockAsString(pos), data)) {
		warningstream << "WARNING: saveBlock: LevelDB error saving block "
			<< pos << ": "<< m_database.get_error() << std::endl;
		return false;
	}
	m_database.del(i64tos(getBlockAsInteger(pos))); // delete old format

	return true;
}

std::string Database_LevelDB::loadBlock(const v3s16 &pos)
{
	std::string datastr;

	m_database.get(getBlockAsString(pos), datastr);
	if (datastr.length())
		return datastr;

	m_database.get(i64tos(getBlockAsInteger(pos)), datastr);

	return datastr;

}

bool Database_LevelDB::deleteBlock(const v3s16 &pos)
{
	auto ok = m_database.del(getBlockAsString(pos));
	if (ok) {
		warningstream << "WARNING: deleteBlock: LevelDB error deleting block "
			<< (pos) << ": " << m_database.get_error() << std::endl;
		return false;
	}

	return true;
}

void Database_LevelDB::listAllLoadableBlocks(std::vector<v3s16> &dst)
{
#if USE_LEVELDB
	auto it = m_database.new_iterator();
	if (!it)
		return;
	for (it->SeekToFirst(); it->Valid(); it->Next()) {
		dst.push_back(getStringAsBlock(it->key().ToString()));
	}
	ENSURE_STATUS_OK(it->status());  // Check for any errors found during the scan
	delete it;
#endif
}

#endif // USE_LEVELDB

