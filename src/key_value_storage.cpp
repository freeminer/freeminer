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

#include <mutex>

#include "convert_json.h"
#include "filesys.h"
#include "json/reader.h"
#include "key_value_storage.h"
#include "log.h"
#include "util/string.h"

KeyValueStorage::KeyValueStorage(const std::string &savedir, const std::string &name) :
		db_name(name)
{
	fullpath = savedir + DIR_DELIM + db_name + ".db";
	open();
}

#if USE_LEVELDB
bool KeyValueStorage::process_status(const leveldb::Status &status, bool reopen)
{
	if (status.ok()) {
		return true;
	}
	std::lock_guard<std::mutex> lock(mutex);
	error = status.ToString();
	if (status.IsCorruption()) {
		if (++repairs > 2)
			return false;
		errorstream << "Trying to repair database [" << db_name << "] try=" << repairs
					<< " [" << error << "]" << std::endl;
		leveldb::Options options;
		options.create_if_missing = true;
		leveldb::Status status_repair;
		try {
			status_repair = leveldb::RepairDB(fullpath, options);
		} catch (const std::exception &e) {
			errorstream << "First repair [" << db_name << "] exception [" << e.what()
						<< "]" << std::endl;
			auto options_repair = options;
			options_repair.paranoid_checks = true;
			try {
				status_repair = leveldb::RepairDB(fullpath, options_repair);
			} catch (const std::exception &e) {
				errorstream << "Second repair [" << db_name << "] exception [" << e.what()
							<< "]" << std::endl;
			}
		}
		if (!status.ok()) {
			error = status.ToString();
			errorstream << "Repair [" << db_name << "] fail [" << error << "]"
						<< std::endl;
			delete db;
			db = nullptr;
			return false;
		}
		if (reopen) {
			auto status_open = leveldb::DB::Open(options, fullpath, &db);
			if (!status_open.ok()) {
				error = status_open.ToString();
				errorstream << "Trying to reopen database [" << db_name << "] fail ["
							<< error << "]" << std::endl;
				delete db;
				db = nullptr;
				return false;
			}
		}
	}
	return status.ok();
}
#endif

bool KeyValueStorage::open()
{
#if USE_LEVELDB
	leveldb::Options options;
	options.create_if_missing = true;
	auto status = leveldb::DB::Open(options, fullpath, &db);
	verbosestream << "KeyValueStorage::open() db_name=" << db_name
				  << " status=" << status.ok() << " error=" << status.ToString()
				  << std::endl;
	return process_status(status, true);
#else
	return true;
#endif
}

void KeyValueStorage::close()
{
	repairs = 0;
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
	auto status = db->Put(write_options, key, data);
	return process_status(status);
#else
	return true;
#endif
}

bool KeyValueStorage::put(const std::string &key, const float &data)
{
	return put(key, ftos(data));
}

bool KeyValueStorage::put_json(const std::string &key, const Json::Value &data)
{
	return put(key, fastWriteJson(data));
}

bool KeyValueStorage::get(const std::string &key, std::string &data)
{
	if (!db)
		return false;
#if USE_LEVELDB
	auto status = db->Get(read_options, key, &data);
	return process_status(status);
#else
	return true;
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

bool KeyValueStorage::get_json(const std::string &key, Json::Value &data)
{
	std::string value, errors;
	get(key, value);
	if (value.empty())
		return false;
	std::istringstream stream(value);
	return Json::parseFromStream(json_char_reader_builder, stream, &data, &errors);
}

std::string KeyValueStorage::get_error()
{
	std::lock_guard<std::mutex> lock(mutex);
	return error;
}

bool KeyValueStorage::del(const std::string &key)
{
	if (!db)
		return false;
#if USE_LEVELDB
	//std::lock_guard<std::mutex> lock(mutex);
	auto status = db->Delete(write_options, key);
	return process_status(status);
#else
	return true;
#endif
}

#if USE_LEVELDB
leveldb::Iterator *KeyValueStorage::new_iterator()
{
	if (!db)
		return nullptr;
	return db->NewIterator(read_options);
}
#endif
