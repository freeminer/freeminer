// Luanti
// SPDX-License-Identifier: LGPL-2.1-or-later
// Copyright (C) 2013 celeron55, Perttu Ahola <celeron55@gmail.com>

#include "config.h"

#if USE_LEVELDB

#include "convert_json.h"
#include "database-leveldb.h"
#include "log_types.h"
#include "filesys.h"
#include "exceptions.h"
#include "remoteplayer.h"
#include "irrlicht_changes/printing.h"
#include "server/player_sao.h"
#include "util/serialize.h"
#include "util/string.h"

#include "leveldb/db.h"


#define ENSURE_STATUS_OK(s) \
	if (!(s).ok()) { \
		throw DatabaseException(std::string("LevelDB error: ") + \
				(s).ToString()); \
	}


Database_LevelDB::Database_LevelDB(const std::string &savedir)
{
	leveldb::Options options;
	options.create_if_missing = true;
	leveldb::DB *db;
	leveldb::Status status = leveldb::DB::Open(options,
		savedir + DIR_DELIM + "map.db", &db);
	ENSURE_STATUS_OK(status);
	m_database.reset(db);
}

bool Database_LevelDB::saveBlock(const v3s16 &pos, std::string_view data)
{
	leveldb::Slice data_s(data.data(), data.size());
	leveldb::Status status = m_database->Put(leveldb::WriteOptions(),
			getBlockAsString(pos), data_s);
			//i64tos(getBlockAsInteger(pos)), data_s);
	if (!status.ok()) {
		warningstream << "saveBlock: LevelDB error saving block "
			<< pos << ": " << status.ToString() << std::endl;
		return false;
	}

        // delete old format
        auto status_del = m_database->Delete(leveldb::WriteOptions(), i64tos(getBlockAsInteger(pos)));

	return true;
}


void Database_LevelDB::loadBlock(const v3s16 &pos, std::string *block)
{
	leveldb::Status status0 = m_database->Get(leveldb::ReadOptions(),
		getBlockAsString(pos), block);

	if (status0.ok() && !block->empty())
		return;

	leveldb::Status status = m_database->Get(leveldb::ReadOptions(),
		i64tos(getBlockAsInteger(pos)), block);

	if (!status.ok())
		block->clear();
}

bool Database_LevelDB::deleteBlock(const v3s16 &pos)
{
	auto status = m_database->Delete(leveldb::WriteOptions(), getBlockAsString(pos));
	if (!status.ok()) {
		warningstream << "deleteBlock: LevelDB error deleting block "
			<< pos << ": " << status.ToString() << std::endl;
		return false;
	}

	return true;
}

void Database_LevelDB::listAllLoadableBlocks(std::vector<v3s16> &dst)
{
	std::unique_ptr<leveldb::Iterator> it(m_database->NewIterator(leveldb::ReadOptions()));
	if (!it)
		return;
	for (it->SeekToFirst(); it->Valid(); it->Next()) {
		dst.push_back(getStringAsBlock(it->key().ToString()));
	}
	ENSURE_STATUS_OK(it->status());  // Check for any errors found during the scan
}

PlayerDatabaseLevelDB::PlayerDatabaseLevelDB(const std::string &savedir, const std::string &name)
{
	leveldb::Options options;
	options.create_if_missing = true;
	leveldb::DB *db;
	leveldb::Status status = leveldb::DB::Open(options,
		savedir + DIR_DELIM + name, &db);
	ENSURE_STATUS_OK(status);
	m_database.reset(db);
}

void PlayerDatabaseLevelDB::savePlayer(RemotePlayer *player)
{
	/*
	u8 version = 1
	u16 hp
	v3f position
	f32 pitch
	f32 yaw
	u16 breath
	u32 attribute_count
	for each attribute {
		std::string name
		std::string (long) value
	}
	std::string (long) serialized_inventory
	*/

	std::ostringstream os(std::ios_base::binary);
	writeU8(os, 1);

	PlayerSAO *sao = player->getPlayerSAO();
	if (!sao)
		return;
	writeU16(os, sao->getHP());
	writeV3F32(os, sao->getBasePosition());
	writeF32(os, sao->getLookPitch());
	writeF32(os, sao->getRotation().Y);
	writeU16(os, sao->getBreath());

	const auto &stringvars = sao->getMeta().getStrings();
	writeU32(os, stringvars.size());
	for (const auto &it : stringvars) {
		os << serializeString16(it.first);
		os << serializeString32(it.second);
	}

	player->inventory.serialize(os);

	leveldb::Status status = m_database->Put(leveldb::WriteOptions(),
		player->getName(), os.str());
	ENSURE_STATUS_OK(status);
	player->onSuccessfulSave();
}

bool PlayerDatabaseLevelDB::removePlayer(const std::string &name)
{
	leveldb::Status s = m_database->Delete(leveldb::WriteOptions(), name);
	return s.ok();
}

bool PlayerDatabaseLevelDB::loadPlayer(RemotePlayer *player, PlayerSAO *sao)
{
	std::string raw;
	leveldb::Status s = m_database->Get(leveldb::ReadOptions(),
		player->getName(), &raw);
	if (!s.ok())
		return false;
	std::istringstream is(raw, std::ios_base::binary);

	if (readU8(is) > 1)
		return false;

	sao->setHPRaw(readU16(is));
	sao->setBasePosition(readV3F32(is));
	sao->setLookPitch(readF32(is));
	sao->setPlayerYaw(readF32(is));
	sao->setBreath(readU16(is), false);

	u32 attribute_count = readU32(is);
	for (u32 i = 0; i < attribute_count; i++) {
		std::string name = deSerializeString16(is);
		std::string value = deSerializeString32(is);
		sao->getMeta().setString(name, value);
	}
	sao->getMeta().setModified(false);

	// This should always be last.
	try {
		player->inventory.deSerialize(is);
	} catch (SerializationError &e) {
		errorstream << "Failed to deserialize player inventory. player_name="
			<< player->getName() << " " << e.what() << std::endl;
	}

	return true;
}

void PlayerDatabaseLevelDB::listPlayers(std::vector<std::string> &res)
{
	std::unique_ptr<leveldb::Iterator> it(m_database->NewIterator(leveldb::ReadOptions()));
	res.clear();
	for (it->SeekToFirst(); it->Valid(); it->Next()) {
		res.push_back(it->key().ToString());
	}
}

AuthDatabaseLevelDB::AuthDatabaseLevelDB(const std::string &savedir, const std::string &name)
{
	leveldb::Options options;
	options.create_if_missing = true;
	leveldb::DB *db;
	leveldb::Status status = leveldb::DB::Open(options,
		savedir + DIR_DELIM + name, &db);
	ENSURE_STATUS_OK(status);
	m_database.reset(db);
}

bool AuthDatabaseLevelDB::getAuth(const std::string &name, AuthEntry &res)
{
	std::string raw;
	leveldb::Status s = m_database->Get(leveldb::ReadOptions(), name, &raw);
	if (!s.ok())
		return false;
	std::istringstream is(raw, std::ios_base::binary);

	/*
	u8 version = 1
	std::string password
	u16 number of privileges
	for each privilege {
		std::string privilege
	}
	s64 last_login
	*/

	if (readU8(is) > 1)
		return false;

	res.id = 1;
	res.name = name;
	res.password = deSerializeString16(is);

	u16 privilege_count = readU16(is);
	res.privileges.clear();
	res.privileges.reserve(privilege_count);
	for (u16 i = 0; i < privilege_count; i++) {
		res.privileges.push_back(deSerializeString16(is));
	}

	res.last_login = readS64(is);
	return true;
}

bool AuthDatabaseLevelDB::saveAuth(const AuthEntry &authEntry)
{
	std::ostringstream os(std::ios_base::binary);
	writeU8(os, 1);
	os << serializeString16(authEntry.password);

	size_t privilege_count = authEntry.privileges.size();
	FATAL_ERROR_IF(privilege_count > U16_MAX,
		"Unsupported number of privileges");
	writeU16(os, privilege_count);
	for (const std::string &privilege : authEntry.privileges) {
		os << serializeString16(privilege);
	}

	writeS64(os, authEntry.last_login);
	leveldb::Status s = m_database->Put(leveldb::WriteOptions(),
		authEntry.name, os.str());
	return s.ok();
}

bool AuthDatabaseLevelDB::createAuth(AuthEntry &authEntry)
{
	return saveAuth(authEntry);
}

bool AuthDatabaseLevelDB::deleteAuth(const std::string &name)
{
	leveldb::Status s = m_database->Delete(leveldb::WriteOptions(), name);
	return s.ok();
}

void AuthDatabaseLevelDB::listNames(std::vector<std::string> &res)
{
	std::unique_ptr<leveldb::Iterator> it(m_database->NewIterator(leveldb::ReadOptions()));
	res.clear();
	for (it->SeekToFirst(); it->Valid(); it->Next()) {
		res.emplace_back(it->key().ToString());
	}
}

void AuthDatabaseLevelDB::reload()
{
	// No-op for LevelDB.
}





//fm:


PlayerDatabaseLevelDBFM::PlayerDatabaseLevelDBFM(const std::string &savedir)
: PlayerDatabaseLevelDB(savedir)
{
}

void PlayerDatabaseLevelDBFM::savePlayer(RemotePlayer *player)
{
	if (!player || !player->getPlayerSAO())
		return;
	Json::Value json;
	json << *player;

	leveldb::Status status = m_database->Put(leveldb::WriteOptions(),
		m_prefix + player->getName(), fastWriteJson(json));
	ENSURE_STATUS_OK(status);
	player->onSuccessfulSave();
}

bool PlayerDatabaseLevelDBFM::removePlayer(const std::string &name)
{
	leveldb::Status s = m_database->Delete(leveldb::WriteOptions(), m_prefix + name);
	return s.ok();
}

bool PlayerDatabaseLevelDBFM::loadPlayer(RemotePlayer *player, PlayerSAO *sao)
{
	try {
		Json::Value json;
		verbosestream << "Reading kv player " << player->getName() << std::endl;

		std::string raw;
		leveldb::Status s =
				m_database->Get(leveldb::ReadOptions(), m_prefix + player->getName(), &raw);
		if (!s.ok())
			return false;

		std::istringstream stream(raw);
		std::string errors;
		if (!Json::parseFromStream(
					m_json_char_reader_builder, stream, &json, &errors)) {
			errorstream << "Failed to load player. player_name=" << player->getName()
						<< " " << errors << std::endl;
			return false;
		}

		if (!json.empty()) {
			player->setPlayerSAO(sao);
			json >> *player;
			return true;
		}
	} catch (const std::exception & e) {
			errorstream << "Failed to load player. player_name=" << player->getName()
						<< " " << e.what() << std::endl;
			return false;
	}

	return false;
}

void PlayerDatabaseLevelDBFM::listPlayers(std::vector<std::string> &res)
{
	leveldb::Iterator *it = m_database->NewIterator(leveldb::ReadOptions());
	res.clear();
	const auto prefix_size = m_prefix.size();
	for (it->SeekToFirst(); it->Valid(); it->Next()) {
			const auto key = it->key().ToString();
			if (key.size() < prefix_size || key.substr(0,prefix_size) != m_prefix)
			continue;
			res.emplace_back(key.substr(prefix_size, key.size() - prefix_size));
	}
	delete it;
}




AuthDatabaseLevelDBFM::AuthDatabaseLevelDBFM(const std::string &savedir):
AuthDatabaseLevelDB(savedir, "players_auth.db")
{}

bool AuthDatabaseLevelDBFM::getAuth(const std::string &name, AuthEntry &res)
{
	std::string raw;
	leveldb::Status s = m_database->Get(leveldb::ReadOptions(), m_prefix + name, &raw);
	if (!s.ok())
			return false;
	std::istringstream is(raw, std::ios_base::binary);

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
	for (const auto & p : json["privileges"].getMemberNames()) {
			res.privileges.emplace_back(p);
	}

	res.last_login = json["last_login"].asInt64();
	return true;
}

bool AuthDatabaseLevelDBFM::saveAuth(const AuthEntry &authEntry)
{
	Json::Value json;
 	json["version"] = 1;
 	json["password"] = authEntry.password;
	for (const std::string &privilege : authEntry.privileges) {
		json["privileges"][privilege] = true;
	}

	json["last_login"] = authEntry.last_login;

	leveldb::Status s = m_database->Put(leveldb::WriteOptions(),
		m_prefix + authEntry.name, fastWriteJson(json));
	return s.ok();
}

bool AuthDatabaseLevelDBFM::deleteAuth(const std::string &name)
{
	leveldb::Status s = m_database->Delete(leveldb::WriteOptions(), m_prefix + name);
	return s.ok();
}

void AuthDatabaseLevelDBFM::listNames(std::vector<std::string> &res)
{
	leveldb::Iterator* it = m_database->NewIterator(leveldb::ReadOptions());
	res.clear();
	const auto prefix_size = m_prefix.size();
	for (it->SeekToFirst(); it->Valid(); it->Next()) {
			const auto key = it->key().ToString();
			if (key.size() < prefix_size || key.substr(0,prefix_size) != m_prefix)
			continue;
			res.emplace_back(key.substr(prefix_size, key.size() - prefix_size));
	}
	delete it;
}




#endif // USE_LEVELDB
