/*
database-dummy.h
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

#include <map>
#include <string>
#include "database.h"
#include "irrlichttypes.h"
#include "threading/concurrent_map.h"

class Database_Dummy : public MapDatabase, public PlayerDatabase, public ModStorageDatabase
{
public:
	bool saveBlock(const v3bpos_t &pos, const std::string &data);
	void loadBlock(const v3bpos_t &pos, std::string *block);
	bool deleteBlock(const v3bpos_t &pos);
	void listAllLoadableBlocks(std::vector<v3bpos_t> &dst);

	void savePlayer(RemotePlayer *player);
	bool loadPlayer(RemotePlayer *player, PlayerSAO *sao);
	bool removePlayer(const std::string &name);
	void listPlayers(std::vector<std::string> &res);

	void getModEntries(const std::string &modname, StringMap *storage);
	void getModKeys(const std::string &modname, std::vector<std::string> *storage);
	bool getModEntry(const std::string &modname,
			const std::string &key, std::string *value);
	bool hasModEntry(const std::string &modname, const std::string &key);
	bool setModEntry(const std::string &modname,
			const std::string &key, const std::string &value);
	bool removeModEntry(const std::string &modname, const std::string &key);
	bool removeModEntries(const std::string &modname);
	void listMods(std::vector<std::string> *res);

	void beginSave() {}
	void endSave() {}

private:
	concurrent_map<std::string, std::string> m_database;
	std::set<std::string> m_player_database;
	std::unordered_map<std::string, StringMap> m_mod_storage_database;
};
