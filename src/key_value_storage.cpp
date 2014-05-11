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

#include "exceptions.h"
#include "filesys.h"
#include "key_value_storage.h"
#include "log.h"
#include "util/pointer.h"

KeyValueStorage::KeyValueStorage(const std::string &savedir, const std::string &name) throw(KeyValueStorageException)
	:
	m_savedir(savedir),
	m_db_name(name.c_str())
{
	leveldb::Options options;
	options.create_if_missing = true;
	leveldb::Status status = leveldb::DB::Open(options, savedir + DIR_DELIM +  m_db_name + ".db", &m_db);
	if(!status.ok()) {
		throw KeyValueStorageException(status.ToString());
	}
}

KeyValueStorage::~KeyValueStorage()
{
	delete m_db;
}

bool KeyValueStorage::put(const char *key, const char *data)
{
	leveldb::WriteOptions write_options;
	leveldb::Status status = m_db->Put(write_options, key, data);
	return status.ok();
}

bool KeyValueStorage::put_json(const char *key, const Json::Value &data)
{
	return put(key, json_writer.write(data).c_str());
}

bool KeyValueStorage::get(const char *key, std::string &data)
{
	leveldb::ReadOptions read_options;
	leveldb::Status status = m_db->Get(read_options, key, &data);
	return status.ok();
}

bool KeyValueStorage::get_json(const char *key, Json::Value & data)
{
	std::string value;
	get(key, value);
	if (value.empty())
		return false;
	return json_reader.parse(value, data);
}

bool KeyValueStorage::del(const char *key)
{
	leveldb::WriteOptions write_options;
	leveldb::Status status = m_db->Delete(write_options, key);
	return status.ok();
}
