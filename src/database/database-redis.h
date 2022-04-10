/*
<<<<<<< HEAD:src/database-sqlite3.h
database-sqlite3.h
Copyright (C) 2013 celeron55, Perttu Ahola <celeron55@gmail.com>
*/
=======
Minetest
Copyright (C) 2014 celeron55, Perttu Ahola <celeron55@gmail.com>
>>>>>>> 5.5.0:src/database/database-redis.h

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

#pragma once

#include "config.h"

#if USE_REDIS

#include <hiredis.h>
#include <string>
#include "database.h"

<<<<<<< HEAD:src/database-sqlite3.h
#include "config.h"

#if USE_SQLITE3

#include "threading/mutex.h"

extern "C" {
	#include "sqlite3.h"
}
=======
class Settings;
>>>>>>> 5.5.0:src/database/database-redis.h

class Database_Redis : public MapDatabase
{
public:
	Database_Redis(Settings &conf);
	~Database_Redis();

	void beginSave();
	void endSave();

	bool saveBlock(const v3s16 &pos, const std::string &data);
	void loadBlock(const v3s16 &pos, std::string *block);
	bool deleteBlock(const v3s16 &pos);
	void listAllLoadableBlocks(std::vector<v3s16> &dst);

private:
<<<<<<< HEAD:src/database-sqlite3.h
	// Open the database
	void openDatabase();
	// Create the database structure
	void createDatabase();
	// Open and initialize the database if needed
	void verifyDatabase();

	void bindPos(sqlite3_stmt *stmt, const v3s16 &pos, int index=1);

	bool m_initialized;

	std::string m_savedir;

	sqlite3 *m_database;
	sqlite3_stmt *m_stmt_read;
	sqlite3_stmt *m_stmt_write;
	sqlite3_stmt *m_stmt_list;
	sqlite3_stmt *m_stmt_delete;
	sqlite3_stmt *m_stmt_begin;
	sqlite3_stmt *m_stmt_end;

	std::mutex mutex;

	s64 m_busy_handler_data[2];

	static int busyHandler(void *data, int count);
};

#endif
#endif
=======
	redisContext *ctx = nullptr;
	std::string hash = "";
};

#endif // USE_REDIS
>>>>>>> 5.5.0:src/database/database-redis.h
