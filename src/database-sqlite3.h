/*
database-sqlite3.h
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

#ifndef DATABASE_SQLITE3_HEADER
#define DATABASE_SQLITE3_HEADER

#include <mutex>
#include "database.h"
#include <string>

#include "config.h"

#if USE_SQLITE3

extern "C" {
	#include "sqlite3.h"
}

class ServerMap;

class Database_SQLite3 : public Database
{
public:
	Database_SQLite3(ServerMap *map, std::string savedir);
	virtual void beginSave();
	virtual void endSave();

	virtual bool saveBlock(v3s16 blockpos, std::string &data);
	virtual std::string loadBlock(v3s16 blockpos);
	virtual void listAllLoadableBlocks(std::list<v3s16> &dst);
	virtual int Initialized(void);
	~Database_SQLite3();
private:
	ServerMap *srvmap;
	std::string m_savedir;
	sqlite3 *m_database;
	sqlite3_stmt *m_database_read;
	sqlite3_stmt *m_database_write;
#ifdef __ANDROID__
	sqlite3_stmt *m_database_delete;
#endif
	sqlite3_stmt *m_database_list;
	std::mutex mutex;

	// Create the database structure
	void createDatabase();
	// Verify we can read/write to the database
	void verifyDatabase();
	void createDirs(std::string path);
};

#endif
#endif
