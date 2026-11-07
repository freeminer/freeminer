// Luanti
// SPDX-License-Identifier: LGPL-2.1-or-later
// Copyright (C) 2013 celeron55, Perttu Ahola <celeron55@gmail.com>

#include "config.h"

#if USE_ROCKSDB

#include "convert_json.h"
#include "database-rocksdb.h"

#include "debug.h"
#include "log.h"
#include "filesys.h"
#include "exceptions.h"
#include "remoteplayer.h"
#include "irrlicht_changes/printing.h"
#include "server/player_sao.h"
#include "util/serialize.h"
#include "util/string.h"

#include "rocksdb/db.h"

#define ENSURE_STATUS_OK(s)                                                              \
	if (!(s).ok()) {                                                                     \
		throw DatabaseException(std::string("RocksDB error: ") + (s).ToString());        \
	}

template <typename DB>
static rocksdb::Status openRocksDB(const rocksdb::Options &options,
		const std::string &path, std::unique_ptr<DB> &database)
{
	if constexpr (requires { DB::Open(options, path, &database); }) {
		return DB::Open(options, path, &database);
	} else {
		DB *raw = nullptr;
		auto status = DB::Open(options, path, &raw);
		database.reset(raw);
		return status;
	}
}

Database_RocksDB::Database_RocksDB(const std::string &savedir)
{
	rocksdb::Options options;
	options.create_if_missing = true;
	rocksdb::Status status =
			openRocksDB(options, savedir + DIR_DELIM + "map.rocksdb", m_database);
	ENSURE_STATUS_OK(status);
}

bool Database_RocksDB::saveBlock(const v3pos_t &pos, std::string_view data)
{
	rocksdb::Slice data_s(data.data(), data.size());
	rocksdb::Status status =
			m_database->Put(rocksdb::WriteOptions(), getBlockAsString(pos), data_s);
	//i64tos(getBlockAsInteger(pos)), data_s);
	if (!status.ok()) {
		warningstream << "saveBlock: RocksDB error saving block " << pos << ": "
					  << status.ToString() << std::endl;
		return false;
	}

	// delete old format
	auto status_del =
			m_database->Delete(rocksdb::WriteOptions(), i64tos(getBlockAsInteger(pos)));

	return true;
}

void Database_RocksDB::loadBlock(const v3pos_t &pos, std::string *block)
{
	rocksdb::Status status0 =
			m_database->Get(rocksdb::ReadOptions(), getBlockAsString(pos), block);

	if (status0.ok() && !block->empty())
		return;

	rocksdb::Status status = m_database->Get(
			rocksdb::ReadOptions(), i64tos(getBlockAsInteger(pos)), block);

	if (!status.ok())
		block->clear();
}

bool Database_RocksDB::deleteBlock(const v3pos_t &pos)
{
	auto status = m_database->Delete(rocksdb::WriteOptions(), getBlockAsString(pos));
	if (!status.ok()) {
		warningstream << "deleteBlock: RocksDB error deleting block " << pos << ": "
					  << status.ToString() << std::endl;
		return false;
	}

	return true;
}

void Database_RocksDB::listAllLoadableBlocks(std::vector<v3pos_t> &dst)
{
	std::unique_ptr<rocksdb::Iterator> it(
			m_database->NewIterator(rocksdb::ReadOptions()));
	if (!it)
		return;
	for (it->SeekToFirst(); it->Valid(); it->Next()) {
		dst.push_back(getStringAsBlock(it->key().ToString()));
	}
	ENSURE_STATUS_OK(it->status()); // Check for any errors found during the scan
}

PlayerDatabaseRocksDB::PlayerDatabaseRocksDB(
		const std::string &savedir, const std::string &name)
{
	rocksdb::Options options;
	options.create_if_missing = true;
	rocksdb::Status status = openRocksDB(options, savedir + DIR_DELIM + name, m_database);
	ENSURE_STATUS_OK(status);
}

AuthDatabaseRocksDB::AuthDatabaseRocksDB(
		const std::string &savedir, const std::string &name)
{
	rocksdb::Options options;
	options.create_if_missing = true;
	rocksdb::Status status = openRocksDB(options, savedir + DIR_DELIM + name, m_database);
	ENSURE_STATUS_OK(status);
}

bool AuthDatabaseRocksDB::createAuth(AuthEntry &authEntry)
{
	return saveAuth(authEntry);
}

void AuthDatabaseRocksDB::reload()
{
}

//fm:

void PlayerDatabaseRocksDB::savePlayer(RemotePlayer *player)
{
	if (!player || !player->getPlayerSAO())
		return;
	Json::Value json;
	json << *player;

	rocksdb::Status status = m_database->Put(
			rocksdb::WriteOptions(), m_prefix + player->getName(), fastWriteJson(json));
	ENSURE_STATUS_OK(status);
	player->onSuccessfulSave();
}

bool PlayerDatabaseRocksDB::removePlayer(const std::string &name)
{
	rocksdb::Status s = m_database->Delete(rocksdb::WriteOptions(), m_prefix + name);
	return s.ok();
}

bool PlayerDatabaseRocksDB::loadPlayer(RemotePlayer *player, PlayerSAO *sao)
{
	try {
		Json::Value json;
		verbosestream << "Reading kv player " << player->getName() << std::endl;

		std::string raw;
		rocksdb::Status s = m_database->Get(
				rocksdb::ReadOptions(), m_prefix + player->getName(), &raw);
		if (!s.ok())
			return false;

		std::istringstream stream(raw);
		std::string errors;
		if (!Json::parseFromStream(m_json_char_reader_builder, stream, &json, &errors)) {
			errorstream << "Failed to load player. player_name=" << player->getName()
						<< " " << errors << std::endl;
			return false;
		}

		if (!json.empty()) {
			player->setPlayerSAO(sao);
			json >> *player;
			return true;
		}
	} catch (const std::exception &e) {
		errorstream << "Failed to load player. player_name=" << player->getName() << " "
					<< e.what() << std::endl;
		return false;
	}

	return false;
}

void PlayerDatabaseRocksDB::listPlayers(std::vector<std::string> &res)
{
	rocksdb::Iterator *it = m_database->NewIterator(rocksdb::ReadOptions());
	res.clear();
	const auto prefix_size = m_prefix.size();
	for (it->SeekToFirst(); it->Valid(); it->Next()) {
		const auto key = it->key().ToString();
		if (key.size() < prefix_size || key.substr(0, prefix_size) != m_prefix)
			continue;
		res.emplace_back(key.substr(prefix_size, key.size() - prefix_size));
	}
	delete it;
}

bool AuthDatabaseRocksDB::getAuth(const std::string &name, AuthEntry &res)
{
	std::string raw;
	rocksdb::Status s = m_database->Get(rocksdb::ReadOptions(), m_prefix + name, &raw);
	if (!s.ok())
		return false;
	Json::Value json;
	std::istringstream stream(raw);
	std::string errors;
	if (!Json::parseFromStream(m_json_char_reader_builder, stream, &json, &errors)) {
		errorstream << "Failed to load player auth . player_name=" << name << " "
					<< errors << std::endl;
		return false;
	}

	if (json.empty()) {
		return false;
	}

	res.id = json["version"].asUInt64();
	res.name = name;
	res.password = json["password"].asString();

	res.privileges.clear();
	res.privileges.reserve(json["privileges"].size());
	for (const auto &p : json["privileges"].getMemberNames()) {
		res.privileges.emplace_back(p);
	}

	res.last_login = json["last_login"].asInt64();
	return true;
}

bool AuthDatabaseRocksDB::saveAuth(const AuthEntry &authEntry)
{
	Json::Value json;
	json["version"] = 1;
	json["password"] = authEntry.password;
	for (const std::string &privilege : authEntry.privileges) {
		json["privileges"][privilege] = true;
	}

	json["last_login"] = Json::Int64(authEntry.last_login);

	rocksdb::Status s = m_database->Put(
			rocksdb::WriteOptions(), m_prefix + authEntry.name, fastWriteJson(json));
	return s.ok();
}

bool AuthDatabaseRocksDB::deleteAuth(const std::string &name)
{
	rocksdb::Status s = m_database->Delete(rocksdb::WriteOptions(), m_prefix + name);
	return s.ok();
}

void AuthDatabaseRocksDB::listNames(std::vector<std::string> &res)
{
	rocksdb::Iterator *it = m_database->NewIterator(rocksdb::ReadOptions());
	res.clear();
	const auto prefix_size = m_prefix.size();
	for (it->SeekToFirst(); it->Valid(); it->Next()) {
		const auto key = it->key().ToString();
		if (key.size() < prefix_size || key.substr(0, prefix_size) != m_prefix)
			continue;
		res.emplace_back(key.substr(prefix_size, key.size() - prefix_size));
	}
	delete it;
}

#endif // USE_ROCKSDB
