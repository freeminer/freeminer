/*
database-leveldb.h
Copyright (C) 2013 celeron55, Perttu Ahola <celeron55@gmail.com>
*/

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

#pragma once

#include "config.h"

#if USE_LEVELDB

#include <json/reader.h>
#include <string>
#include "database.h"
#include "leveldb/db.h"

class Database_LevelDB : public MapDatabase
{
public:
	Database_LevelDB(const std::string &savedir);
	~Database_LevelDB();

	/* fmtodo?:
	void open() { m_database->open(); };
	void close() { m_database->close(); };
	*/

	bool saveBlock(const v3s16 &pos, const std::string &data);
	void loadBlock(const v3s16 &pos, std::string *block);
	bool deleteBlock(const v3s16 &pos);
	void listAllLoadableBlocks(std::vector<v3s16> &dst);

	void beginSave() {}
	void endSave() {}

private:
	leveldb::DB *m_database;
};

class PlayerDatabaseLevelDB : public PlayerDatabase
{
public:
	PlayerDatabaseLevelDB(const std::string &savedir, const std::string &name = "players.db");
	~PlayerDatabaseLevelDB();

	void savePlayer(RemotePlayer *player);
	bool loadPlayer(RemotePlayer *player, PlayerSAO *sao);
	bool removePlayer(const std::string &name);
	void listPlayers(std::vector<std::string> &res);

protected:
	leveldb::DB *m_database;
};

class AuthDatabaseLevelDB : public AuthDatabase
{
public:
	AuthDatabaseLevelDB(const std::string &savedir, const std::string &name = "auth.db");
	virtual ~AuthDatabaseLevelDB();

	virtual bool getAuth(const std::string &name, AuthEntry &res);
	virtual bool saveAuth(const AuthEntry &authEntry);
	virtual bool createAuth(AuthEntry &authEntry);
	virtual bool deleteAuth(const std::string &name);
	virtual void listNames(std::vector<std::string> &res);
	virtual void reload();

protected:
	leveldb::DB *m_database;
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
