/*
database-sqlite3.h
Copyright (C) 2013 celeron55, Perttu Ahola <celeron55@gmail.com>
Minetest
Copyright (C) 2014 celeron55, Perttu Ahola <celeron55@gmail.com>

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

class Settings;

class Database_Redis : public MapDatabase
{
public:
	Database_Redis(Settings &conf);
	~Database_Redis();

	void beginSave();
	void endSave();

	bool saveBlock(const v3bpos_t &pos, const std::string &data);
	void loadBlock(const v3bpos_t &pos, std::string *block);
	bool deleteBlock(const v3bpos_t &pos);
	void listAllLoadableBlocks(std::vector<v3bpos_t> &dst);

private:
	redisContext *ctx = nullptr;
	std::string hash = "";
};

#endif // USE_REDIS
