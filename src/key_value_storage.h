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

#ifndef KEY_VALUE_STORAGE_H
#define KEY_VALUE_STORAGE_H

#include <string>
#include <leveldb/db.h>
#include "exceptions.h"
#include "json/json.h"

class KeyValueStorage
{
public:
	KeyValueStorage(const std::string &savedir, const std::string &name) throw(KeyValueStorageException);
	~KeyValueStorage();
	bool put(const char *key, const char *data);
	bool put_json(const char *key, const Json::Value & data);
	bool get(const char *key, std::string &data);
	bool get_json(const char *key, Json::Value & data);
	bool del(const char *key);
private:
	std::string m_savedir;
	leveldb::DB *m_db;
	const char *m_db_name;
	Json::FastWriter json_writer;
	Json::Reader json_reader;
};

#endif
