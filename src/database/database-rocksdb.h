// Luanti
// SPDX-License-Identifier: LGPL-2.1-or-later
// Copyright (C) 2013 celeron55, Perttu Ahola <celeron55@gmail.com>

#pragma once

#include "config.h"

#if USE_ROCKSDB

#include <json/reader.h>
#include <memory>
#include <string>
#include "database.h"
#include "rocksdb/db.h"

class Database_RocksDB : public MapDatabase
{
public:
	Database_RocksDB(const std::string &savedir);
	~Database_RocksDB() = default;

	bool saveBlock(const v3pos_t &pos, std::string_view data);
	void loadBlock(const v3pos_t &pos, std::string *block);
	bool deleteBlock(const v3pos_t &pos);
	void listAllLoadableBlocks(std::vector<v3pos_t> &dst);

	void beginSave() {}
	void endSave() {}

private:
	std::unique_ptr<rocksdb::DB> m_database;
};

class PlayerDatabaseRocksDB : public PlayerDatabase
{
public:
	PlayerDatabaseRocksDB(
			const std::string &savedir, const std::string &name = "players.rocksdb");
	~PlayerDatabaseRocksDB() = default;

	void savePlayer(RemotePlayer *player);
	bool loadPlayer(RemotePlayer *player, PlayerSAO *sao);
	bool removePlayer(const std::string &name);
	void listPlayers(std::vector<std::string> &res);

private:
	std::unique_ptr<rocksdb::DB> m_database;
	Json::CharReaderBuilder m_json_char_reader_builder;
	const std::string m_prefix{"p."};
};

class AuthDatabaseRocksDB : public AuthDatabase
{
public:
	AuthDatabaseRocksDB(
			const std::string &savedir, const std::string &name = "players_auth.rocksdb");
	virtual ~AuthDatabaseRocksDB() = default;

	virtual bool getAuth(const std::string &name, AuthEntry &res);
	virtual bool saveAuth(const AuthEntry &authEntry);
	virtual bool createAuth(AuthEntry &authEntry);
	virtual bool deleteAuth(const std::string &name);
	virtual void listNames(std::vector<std::string> &res);
	virtual void reload();

private:
	std::unique_ptr<rocksdb::DB> m_database;
	Json::CharReaderBuilder m_json_char_reader_builder;
	const std::string m_prefix{"auth_"};
};

#endif // USE_ROCKSDB
