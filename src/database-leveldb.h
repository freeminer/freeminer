/*
database-leveldb.h
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

#ifndef DATABASE_LEVELDB_HEADER
#define DATABASE_LEVELDB_HEADER

#include "config.h"

#if USE_LEVELDB

#include "database.h"
#include "key_value_storage.h"
#include <string>

class ServerMap;

class Database_LevelDB : public Database
{
public:
	Database_LevelDB(ServerMap *map, std::string savedir);
	virtual void beginSave();
	virtual void endSave();
        virtual bool saveBlock(MapBlock *block);
        virtual MapBlock *loadBlock(v3s16 blockpos);
        virtual void listAllLoadableBlocks(std::list<v3s16> &dst);
        virtual int Initialized(void);
	~Database_LevelDB();
private:
	ServerMap *srvmap;
	KeyValueStorage *m_database;
	//leveldb::DB* m_database;
};
#endif
#endif
