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
#include <mutex>

#include "config.h"
#if USE_LEVELDB
#include <leveldb/db.h>
#endif
#include "exceptions.h"
#include "json/json.h"

class KeyValueStorage
{
public:
	KeyValueStorage(const std::string &savedir, const std::string &name) throw(KeyValueStorageException);
	~KeyValueStorage();
	bool put(const std::string & key, const std::string & data);
	bool put_json(const std::string & key, const Json::Value & data);
	bool get(const std::string & key, std::string &data);
	bool get_json(const std::string & key, Json::Value & data);
	bool del(const std::string & key);
#if USE_LEVELDB
	leveldb::Iterator* new_iterator();
	leveldb::DB *m_db;
	leveldb::ReadOptions read_options;
	leveldb::WriteOptions write_options;
#else
	char *m_db;
#endif
private:
	const std::string &m_db_name;
	Json::FastWriter json_writer;
	Json::Reader json_reader;
	std::mutex mutex;
};

#endif
