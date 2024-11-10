// Luanti
// SPDX-License-Identifier: LGPL-2.1-or-later
// Copyright (C) 2013 celeron55, Perttu Ahola <celeron55@gmail.com>

#pragma once

#include "config.h"

#if USE_LEVELDB

#include <json/reader.h>
#include <memory>
#include <string>
#include "database.h"
#include "leveldb/db.h"

class Database_LevelDB : public MapDatabase
{
public:
	Database_LevelDB(const std::string &savedir);
	~Database_LevelDB() = default;

	/* fmtodo?:
	void open() { m_database->open(); };
	void close() { m_database->close(); };
	*/

	bool saveBlock(const v3s16 &pos, std::string_view data);
	void loadBlock(const v3s16 &pos, std::string *block);
	bool deleteBlock(const v3s16 &pos);
	void listAllLoadableBlocks(std::vector<v3s16> &dst);

	void beginSave() {}
	void endSave() {}

private:
	std::unique_ptr<leveldb::DB> m_database;
};

class PlayerDatabaseLevelDB : public PlayerDatabase
{
public:
	PlayerDatabaseLevelDB(const std::string &savedir, const std::string &name = "players.db");
	~PlayerDatabaseLevelDB() = default;

	void savePlayer(RemotePlayer *player);
	bool loadPlayer(RemotePlayer *player, PlayerSAO *sao);
	bool removePlayer(const std::string &name);
	void listPlayers(std::vector<std::string> &res);

protected:
	std::unique_ptr<leveldb::DB> m_database;
};

class AuthDatabaseLevelDB : public AuthDatabase
{
public:
	AuthDatabaseLevelDB(const std::string &savedir, const std::string &name = "auth.db");
	virtual ~AuthDatabaseLevelDB() = default;

	virtual bool getAuth(const std::string &name, AuthEntry &res);
	virtual bool saveAuth(const AuthEntry &authEntry);
	virtual bool createAuth(AuthEntry &authEntry);
	virtual bool deleteAuth(const std::string &name);
	virtual void listNames(std::vector<std::string> &res);
	virtual void reload();

protected:
	std::unique_ptr<leveldb::DB> m_database;
};


// p.name : json
class PlayerDatabaseLevelDBFM : public PlayerDatabaseLevelDB
{
public:
	PlayerDatabaseLevelDBFM(const std::string &savedir);

	void savePlayer(RemotePlayer *player);
	bool loadPlayer(RemotePlayer *player, PlayerSAO *sao);
	bool removePlayer(const std::string &name);
	void listPlayers(std::vector<std::string> &res);

private:
	Json::CharReaderBuilder m_json_char_reader_builder;
	const std::string m_prefix {"p."};
};


class AuthDatabaseLevelDBFM : public AuthDatabaseLevelDB
{
public:
	AuthDatabaseLevelDBFM(const std::string &savedir);

	virtual bool getAuth(const std::string &name, AuthEntry &res);
	virtual bool saveAuth(const AuthEntry &authEntry);
	virtual bool deleteAuth(const std::string &name);
	virtual void listNames(std::vector<std::string> &res);

private:
	Json::CharReaderBuilder m_json_char_reader_builder;
	const std::string m_prefix {"auth_"};
};


#endif // USE_LEVELDB
