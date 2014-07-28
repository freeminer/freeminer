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
	//m_savedir(savedir),
	m_db(nullptr),
	m_db_name(name)
{
#if USE_LEVELDB
	leveldb::Options options;
	options.create_if_missing = true;
	auto path = savedir + DIR_DELIM +  m_db_name + ".db";
	std::lock_guard<std::mutex> lock(mutex);
	auto status = leveldb::DB::Open(options, path, &m_db);
	if (!status.ok()) {
		errorstream<< "Trying to repair database ["<<status.ToString()<<"]"<<std::endl;
		status = leveldb::RepairDB(path, options);
		if (!status.ok()) {
			m_db = nullptr;
			throw KeyValueStorageException(status.ToString());
		}
		status = leveldb::DB::Open(options, path, &m_db);
		if (!status.ok()) {
			m_db = nullptr;
			throw KeyValueStorageException(status.ToString());
		}
	}
#endif
}

KeyValueStorage::~KeyValueStorage()
{
	if (!m_db)
		return;
	delete m_db;
}

bool KeyValueStorage::put(const std::string &key, const std::string &data)
{
	if (!m_db)
		return false;
#if USE_LEVELDB
	std::lock_guard<std::mutex> lock(mutex);
	auto status = m_db->Put(write_options, key, data);
	return status.ok();
#endif
}

bool KeyValueStorage::put_json(const std::string &key, const Json::Value &data)
{
	return put(key, json_writer.write(data).c_str());
}

bool KeyValueStorage::get(const std::string &key, std::string &data)
{
	if (!m_db)
		return false;
#if USE_LEVELDB
	std::lock_guard<std::mutex> lock(mutex);
	auto status = m_db->Get(read_options, key, &data);
	return status.ok();
#endif
}

bool KeyValueStorage::get_json(const std::string &key, Json::Value & data)
{
	std::string value;
	get(key, value);
	if (value.empty())
		return false;
	return json_reader.parse(value, data);
}

bool KeyValueStorage::del(const std::string &key)
{
	if (!m_db)
		return false;
#if USE_LEVELDB
	std::lock_guard<std::mutex> lock(mutex);
	auto status = m_db->Delete(write_options, key);
	return status.ok();
#endif
}

#if USE_LEVELDB
leveldb::Iterator* KeyValueStorage::new_iterator() {
	return m_db->NewIterator(read_options);
}
#endif
