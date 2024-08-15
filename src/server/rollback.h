/*
Minetest
Copyright (C) 2013 celeron55, Perttu Ahola <celeron55@gmail.com>

This program is free software; you can redistribute it and/or modify
it under the terms of the GNU Lesser General Public License as published by
the Free Software Foundation; either version 2.1 of the License, or
(at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU Lesser General Public License for more details.

You should have received a copy of the GNU Lesser General Public License along
with this program; if not, write to the Free Software Foundation, Inc.,
51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
*/

#pragma once

#include <string>
#include "irr_v3d.h"
#include "rollback_interface.h"
#include <list>
#include <vector>
#include "sqlite3.h"

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
	std::string getSuspect(v3pos_t p, float nearness_shortcut,
			float min_nearness);
	void flush();

	void addAction(const RollbackAction & action);
	std::list<RollbackAction> getNodeActors(v3pos_t pos, int range,
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
	bool initDatabase();
	bool registerRow(const ActionRow & row);
	const std::list<ActionRow> actionRowsFromSelect(sqlite3_stmt * stmt);
	ActionRow actionRowFromRollbackAction(const RollbackAction & action);
	const std::list<RollbackAction> rollbackActionsFromActionRows(
			const std::list<ActionRow> & rows);
	const std::list<ActionRow> getRowsSince(time_t firstTime,
			const std::string & actor);
	const std::list<ActionRow> getRowsSince_range(time_t firstTime, v3pos_t p,
			int range, int limit);
	const std::list<RollbackAction> getActionsSince_range(time_t firstTime, v3pos_t p,
			int range, int limit);
	const std::list<RollbackAction> getActionsSince(time_t firstTime,
			const std::string & actor = "");
	static float getSuspectNearness(bool is_guess, v3pos_t suspect_p,
		time_t suspect_t, v3pos_t action_p, time_t action_t);


	IGameDef *gamedef = nullptr;

	std::string current_actor;
	bool current_actor_is_guess = false;

	std::list<RollbackAction> action_todisk_buffer;
	std::list<RollbackAction> action_latest_buffer;

	std::string database_path;
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

	std::vector<Entity> knownActors;
	std::vector<Entity> knownNodes;
};
