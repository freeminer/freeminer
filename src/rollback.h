/*
rollback.h
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

#ifndef ROLLBACK_HEADER
#define ROLLBACK_HEADER

#include <string>
#include "irr_v3d.h"
#include "rollback_interface.h"
#include <list>
#include <vector>

#include "config.h"
#if USE_SQLITE3
#include "sqlite3.h"
#endif

class IGameDef;

struct ActionRow;
struct Entity;

class RollbackManager: public IRollbackManager
{
public:
	RollbackManager(const std::string & world_path, IGameDef * gamedef);
	~RollbackManager();

	void reportAction(const RollbackAction & action_);
	std::string getActor();
	bool isActorGuess();
	void setActor(const std::string & actor, bool is_guess);
	std::string getSuspect(v3s16 p, float nearness_shortcut,
			float min_nearness);
	void flush();

	void addAction(const RollbackAction & action);
	std::list<RollbackAction> getEntriesSince(time_t first_time);
	std::list<RollbackAction> getNodeActors(v3s16 pos, int range,
			time_t seconds, int limit);
	std::list<RollbackAction> getRevertActions(
			const std::string & actor_filter, time_t seconds);

private:
	void registerNewActor(const int id, const std::string & name);
	void registerNewNode(const int id, const std::string & name);
	int getActorId(const std::string & name);
	int getNodeId(const std::string & name);
	const char * getActorName(const int id);
	const char * getNodeName(const int id);
	bool createTables();
	void initDatabase();
	bool registerRow(const ActionRow & row);
#if USE_SQLITE3
	const std::list<ActionRow> actionRowsFromSelect(sqlite3_stmt * stmt);
#endif
	ActionRow actionRowFromRollbackAction(const RollbackAction & action);
	const std::list<RollbackAction> rollbackActionsFromActionRows(
			const std::list<ActionRow> & rows);
	const std::list<ActionRow> getRowsSince(time_t firstTime,
			const std::string & actor);
	const std::list<ActionRow> getRowsSince_range(time_t firstTime, v3s16 p,
			int range, int limit);
	const std::list<RollbackAction> getActionsSince_range(time_t firstTime, v3s16 p,
			int range, int limit);
	const std::list<RollbackAction> getActionsSince(time_t firstTime,
			const std::string & actor = "");
	void migrate(const std::string & filepath);
	static float getSuspectNearness(bool is_guess, v3s16 suspect_p,
		time_t suspect_t, v3s16 action_p, time_t action_t);


	IGameDef * gamedef;

	std::string current_actor;
	bool current_actor_is_guess;

	std::list<RollbackAction> action_todisk_buffer;
	std::list<RollbackAction> action_latest_buffer;

	std::string database_path;
#if USE_SQLITE3
	sqlite3 * db;
	sqlite3_stmt * stmt_insert;
	sqlite3_stmt * stmt_replace;
	sqlite3_stmt * stmt_select;
	sqlite3_stmt * stmt_select_range;
	sqlite3_stmt * stmt_select_withActor;
	sqlite3_stmt * stmt_knownActor_select;
	sqlite3_stmt * stmt_knownActor_insert;
	sqlite3_stmt * stmt_knownNode_select;
	sqlite3_stmt * stmt_knownNode_insert;
#endif
	std::vector<Entity> knownActors;
	std::vector<Entity> knownNodes;
};

#endif
