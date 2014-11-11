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
#include "util/string.h"

KeyValueStorage::KeyValueStorage(const std::string &savedir, const std::string &name) :
	db(nullptr),
	db_name(name)
{
	fullpath = savedir + DIR_DELIM + db_name + ".db";
	open();
}

bool KeyValueStorage::open() {
#if USE_LEVELDB
	leveldb::Options options;
	options.create_if_missing = true;
	std::lock_guard<std::mutex> lock(mutex);
	auto status = leveldb::DB::Open(options, fullpath, &db);
	if (!status.ok()) {
		error = status.ToString();
		errorstream<< "Trying to repair database ["<<error<<"]"<<std::endl;
		status = leveldb::RepairDB(fullpath, options);
		if (!status.ok()) {
			db = nullptr;
			return true;
		}
		status = leveldb::DB::Open(options, fullpath, &db);
		if (!status.ok()) {
			error = status.ToString();
			db = nullptr;
			return true;
		}
	}
#endif
	return false;
}

void KeyValueStorage::close()
{
	if (!db)
		return;
	delete db;
	db = nullptr;
}

KeyValueStorage::~KeyValueStorage()
{
	close();
}

bool KeyValueStorage::put(const std::string &key, const std::string &data)
{
	if (!db)
		return false;
#if USE_LEVELDB
	std::lock_guard<std::mutex> lock(mutex);
	auto status = db->Put(write_options, key, data);
	return status.ok();
#endif
}

bool KeyValueStorage::put(const std::string &key, const float &data)
{
	return put(key, ftos(data));
}


bool KeyValueStorage::put_json(const std::string &key, const Json::Value &data)
{
	return put(key, json_writer.write(data).c_str());
}

bool KeyValueStorage::get(const std::string &key, std::string &data)
{
	if (!db)
		return false;
#if USE_LEVELDB
	std::lock_guard<std::mutex> lock(mutex);
	auto status = db->Get(read_options, key, &data);
	return status.ok();
#endif
}

bool KeyValueStorage::get(const std::string &key, float &data)
{
	std::string tmpstring;
	if (get(key, tmpstring) && !tmpstring.empty()) {
		data = stof(tmpstring);
		return true;
	}
	return false;
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
	if (!db)
		return false;
#if USE_LEVELDB
	std::lock_guard<std::mutex> lock(mutex);
	auto status = db->Delete(write_options, key);
	return status.ok();
#endif
}

#if USE_LEVELDB
leveldb::Iterator* KeyValueStorage::new_iterator() {
	return db->NewIterator(read_options);
}
#endif
