/*
rollback.cpp
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

#include "rollback.h"
#include <fstream>
#include <list>
#include <sstream>
#include "log.h"
#include "mapnode.h"
#include "gamedef.h"
#include "nodedef.h"
#include "util/serialize.h"
#include "util/string.h"
#include "util/numeric.h"
#include "inventorymanager.h" // deserializing InventoryLocations
#include "filesys.h"

#define POINTS_PER_NODE (16.0)

#define SQLRES(f, good) \
	if ((f) != (good)) {\
		throw FileNotGoodException(std::string("RollbackManager: " \
			"SQLite3 error (" __FILE__ ":" TOSTRING(__LINE__) \
			"): ") + sqlite3_errmsg(db)); \
	}
#define SQLOK(f) SQLRES(f, SQLITE_OK)


class ItemStackRow : public ItemStack {
public:
	ItemStackRow & operator = (const ItemStack & other)
	{
		*static_cast<ItemStack *>(this) = other;
		return *this;
	}

	int id;
};

struct ActionRow {
	int          id;
	int          actor;
	time_t       timestamp;
	int          type;
	std::string  location, list;
	int          index, add;
	ItemStackRow stack;
	int          nodeMeta;
	int          x, y, z;
	int          oldNode;
	int          oldParam1, oldParam2;
	std::string  oldMeta;
	int          newNode;
	int          newParam1, newParam2;
	std::string  newMeta;
	int          guessed;
};


struct Entity {
	int         id;
	std::string name;
};



RollbackManager::RollbackManager(const std::string & world_path,
		IGameDef * gamedef_) :
	gamedef(gamedef_),
	current_actor_is_guess(false)
{
	verbosestream << "RollbackManager::RollbackManager(" << world_path
		<< ")" << std::endl;

	std::string txt_filename = world_path + DIR_DELIM "rollback.txt";
	std::string migrating_flag = txt_filename + ".migrating";
	database_path = world_path + DIR_DELIM "rollback.sqlite";

	initDatabase();

	if (fs::PathExists(txt_filename) && (fs::PathExists(migrating_flag) ||
			!fs::PathExists(database_path))) {
		std::ofstream of(migrating_flag.c_str());
		of.close();
		migrate(txt_filename);
		fs::DeleteSingleFileOrEmptyDirectory(migrating_flag);
	}
}


RollbackManager::~RollbackManager()
{
#if USE_SQLITE3
	SQLOK(sqlite3_finalize(stmt_insert));
	SQLOK(sqlite3_finalize(stmt_replace));
	SQLOK(sqlite3_finalize(stmt_select));
	SQLOK(sqlite3_finalize(stmt_select_range));
	SQLOK(sqlite3_finalize(stmt_select_withActor));
	SQLOK(sqlite3_finalize(stmt_knownActor_select));
	SQLOK(sqlite3_finalize(stmt_knownActor_insert));
	SQLOK(sqlite3_finalize(stmt_knownNode_select));
	SQLOK(sqlite3_finalize(stmt_knownNode_insert));

	SQLOK(sqlite3_close(db));
#endif
}


void RollbackManager::registerNewActor(const int id, const std::string &name)
{
	Entity actor = {id, name};
	knownActors.push_back(actor);
}


void RollbackManager::registerNewNode(const int id, const std::string &name)
{
	Entity node = {id, name};
	knownNodes.push_back(node);
}


int RollbackManager::getActorId(const std::string &name)
{
#if USE_SQLITE3
	for (std::vector<Entity>::const_iterator iter = knownActors.begin();
			iter != knownActors.end(); ++iter) {
		if (iter->name == name) {
			return iter->id;
		}
	}

	SQLOK(sqlite3_bind_text(stmt_knownActor_insert, 1, name.c_str(), name.size(), NULL));
	SQLRES(sqlite3_step(stmt_knownActor_insert), SQLITE_DONE);
	SQLOK(sqlite3_reset(stmt_knownActor_insert));

	int id = sqlite3_last_insert_rowid(db);
	registerNewActor(id, name);

	return id;
#else
	return 0;
#endif
}


int RollbackManager::getNodeId(const std::string &name)
{
#if USE_SQLITE3
	for (std::vector<Entity>::const_iterator iter = knownNodes.begin();
			iter != knownNodes.end(); ++iter) {
		if (iter->name == name) {
			return iter->id;
		}
	}

	SQLOK(sqlite3_bind_text(stmt_knownNode_insert, 1, name.c_str(), name.size(), NULL));
	SQLRES(sqlite3_step(stmt_knownNode_insert), SQLITE_DONE);
	SQLOK(sqlite3_reset(stmt_knownNode_insert));

	int id = sqlite3_last_insert_rowid(db);
	registerNewNode(id, name);

	return id;
#else
	return 0;
#endif
}


const char * RollbackManager::getActorName(const int id)
{
	for (std::vector<Entity>::const_iterator iter = knownActors.begin();
			iter != knownActors.end(); ++iter) {
		if (iter->id == id) {
			return iter->name.c_str();
		}
	}

	return "";
}


const char * RollbackManager::getNodeName(const int id)
{
	for (std::vector<Entity>::const_iterator iter = knownNodes.begin();
			iter != knownNodes.end(); ++iter) {
		if (iter->id == id) {
			return iter->name.c_str();
		}
	}

	return "";
}


bool RollbackManager::createTables()
{
#if USE_SQLITE3
	SQLOK(sqlite3_exec(db,
		"CREATE TABLE IF NOT EXISTS `actor` (\n"
		"	`id` INTEGER PRIMARY KEY AUTOINCREMENT NOT NULL,\n"
		"	`name` TEXT NOT NULL\n"
		");\n"
		"CREATE TABLE IF NOT EXISTS `node` (\n"
		"	`id` INTEGER PRIMARY KEY AUTOINCREMENT NOT NULL,\n"
		"	`name` TEXT NOT NULL\n"
		");\n"
		"CREATE TABLE IF NOT EXISTS `action` (\n"
		"	`id` INTEGER PRIMARY KEY AUTOINCREMENT,\n"
		"	`actor` INTEGER NOT NULL,\n"
		"	`timestamp` TIMESTAMP NOT NULL,\n"
		"	`type` INTEGER NOT NULL,\n"
		"	`list` TEXT,\n"
		"	`index` INTEGER,\n"
		"	`add` INTEGER,\n"
		"	`stackNode` INTEGER,\n"
		"	`stackQuantity` INTEGER,\n"
		"	`nodeMeta` INTEGER,\n"
		"	`x` INT,\n"
		"	`y` INT,\n"
		"	`z` INT,\n"
		"	`oldNode` INTEGER,\n"
		"	`oldParam1` INTEGER,\n"
		"	`oldParam2` INTEGER,\n"
		"	`oldMeta` TEXT,\n"
		"	`newNode` INTEGER,\n"
		"	`newParam1` INTEGER,\n"
		"	`newParam2` INTEGER,\n"
		"	`newMeta` TEXT,\n"
		"	`guessedActor` INTEGER,\n"
		"	FOREIGN KEY (`actor`) REFERENCES `actor`(`id`),\n"
		"	FOREIGN KEY (`stackNode`) REFERENCES `node`(`id`),\n"
		"	FOREIGN KEY (`oldNode`)   REFERENCES `node`(`id`),\n"
		"	FOREIGN KEY (`newNode`)   REFERENCES `node`(`id`)\n"
		");\n"
		"CREATE INDEX IF NOT EXISTS `actionActor` ON `action`(`actor`);\n"
		"CREATE INDEX IF NOT EXISTS `actionTimestamp` ON `action`(`timestamp`);\n",
		NULL, NULL, NULL));
	verbosestream << "SQL Rollback: SQLite3 database structure was created" << std::endl;

#endif
	return true;
}


void RollbackManager::initDatabase()
{
#if USE_SQLITE3
	verbosestream << "RollbackManager: Database connection setup" << std::endl;

	bool needsCreate = !fs::PathExists(database_path);
	SQLOK(sqlite3_open_v2(database_path.c_str(), &db,
			SQLITE_OPEN_READWRITE | SQLITE_OPEN_CREATE, NULL));

	if (needsCreate) {
		createTables();
	}

	SQLOK(sqlite3_prepare_v2(db,
		"INSERT INTO `action` (\n"
		"	`actor`, `timestamp`, `type`,\n"
		"	`list`, `index`, `add`, `stackNode`, `stackQuantity`, `nodeMeta`,\n"
		"	`x`, `y`, `z`,\n"
		"	`oldNode`, `oldParam1`, `oldParam2`, `oldMeta`,\n"
		"	`newNode`, `newParam1`, `newParam2`, `newMeta`,\n"
		"	`guessedActor`\n"
		") VALUES (\n"
		"	?, ?, ?,\n"
		"	?, ?, ?, ?, ?, ?,\n"
		"	?, ?, ?,\n"
		"	?, ?, ?, ?,\n"
		"	?, ?, ?, ?,\n"
		"	?"
		");",
		-1, &stmt_insert, NULL));

	SQLOK(sqlite3_prepare_v2(db,
		"REPLACE INTO `action` (\n"
		"	`actor`, `timestamp`, `type`,\n"
		"	`list`, `index`, `add`, `stackNode`, `stackQuantity`, `nodeMeta`,\n"
		"	`x`, `y`, `z`,\n"
		"	`oldNode`, `oldParam1`, `oldParam2`, `oldMeta`,\n"
		"	`newNode`, `newParam1`, `newParam2`, `newMeta`,\n"
		"	`guessedActor`, `id`\n"
		") VALUES (\n"
		"	?, ?, ?,\n"
		"	?, ?, ?, ?, ?, ?,\n"
		"	?, ?, ?,\n"
		"	?, ?, ?, ?,\n"
		"	?, ?, ?, ?,\n"
		"	?, ?\n"
		");",
		-1, &stmt_replace, NULL));

	SQLOK(sqlite3_prepare_v2(db,
		"SELECT\n"
		"	`actor`, `timestamp`, `type`,\n"
		"	`list`, `index`, `add`, `stackNode`, `stackQuantity`, `nodemeta`,\n"
		"	`x`, `y`, `z`,\n"
		"	`oldNode`, `oldParam1`, `oldParam2`, `oldMeta`,\n"
		"	`newNode`, `newParam1`, `newParam2`, `newMeta`,\n"
		"	`guessedActor`\n"
		" FROM `action`\n"
		" WHERE `timestamp` >= ?\n"
		" ORDER BY `timestamp` DESC, `id` DESC",
		-1, &stmt_select, NULL));

	SQLOK(sqlite3_prepare_v2(db,
		"SELECT\n"
		"	`actor`, `timestamp`, `type`,\n"
		"	`list`, `index`, `add`, `stackNode`, `stackQuantity`, `nodemeta`,\n"
		"	`x`, `y`, `z`,\n"
		"	`oldNode`, `oldParam1`, `oldParam2`, `oldMeta`,\n"
		"	`newNode`, `newParam1`, `newParam2`, `newMeta`,\n"
		"	`guessedActor`\n"
		"FROM `action`\n"
		"WHERE `timestamp` >= ?\n"
		"	AND `x` IS NOT NULL\n"
		"	AND `y` IS NOT NULL\n"
		"	AND `z` IS NOT NULL\n"
		"	AND `x` BETWEEN ? AND ?\n"
		"	AND `y` BETWEEN ? AND ?\n"
		"	AND `z` BETWEEN ? AND ?\n"
		"ORDER BY `timestamp` DESC, `id` DESC\n"
		"LIMIT 0,?",
		-1, &stmt_select_range, NULL));

	SQLOK(sqlite3_prepare_v2(db,
		"SELECT\n"
		"	`actor`, `timestamp`, `type`,\n"
		"	`list`, `index`, `add`, `stackNode`, `stackQuantity`, `nodemeta`,\n"
		"	`x`, `y`, `z`,\n"
		"	`oldNode`, `oldParam1`, `oldParam2`, `oldMeta`,\n"
		"	`newNode`, `newParam1`, `newParam2`, `newMeta`,\n"
		"	`guessedActor`\n"
		"FROM `action`\n"
		"WHERE `timestamp` >= ?\n"
		"	AND `actor` = ?\n"
		"ORDER BY `timestamp` DESC, `id` DESC\n",
		-1, &stmt_select_withActor, NULL));

	SQLOK(sqlite3_prepare_v2(db, "SELECT `id`, `name` FROM `actor`",
			-1, &stmt_knownActor_select, NULL));

	SQLOK(sqlite3_prepare_v2(db, "INSERT INTO `actor` (`name`) VALUES (?)",
			-1, &stmt_knownActor_insert, NULL));

	SQLOK(sqlite3_prepare_v2(db, "SELECT `id`, `name` FROM `node`",
			-1, &stmt_knownNode_select, NULL));

	SQLOK(sqlite3_prepare_v2(db, "INSERT INTO `node` (`name`) VALUES (?)",
			-1, &stmt_knownNode_insert, NULL));

	verbosestream << "SQL prepared statements setup correctly" << std::endl;

	while (sqlite3_step(stmt_knownActor_select) == SQLITE_ROW) {
		registerNewActor(
		        sqlite3_column_int(stmt_knownActor_select, 0),
		        reinterpret_cast<const char *>(sqlite3_column_text(stmt_knownActor_select, 1))
		);
	}
	SQLOK(sqlite3_reset(stmt_knownActor_select));

	while (sqlite3_step(stmt_knownNode_select) == SQLITE_ROW) {
		registerNewNode(
		        sqlite3_column_int(stmt_knownNode_select, 0),
		        reinterpret_cast<const char *>(sqlite3_column_text(stmt_knownNode_select, 1))
		);
	}
	SQLOK(sqlite3_reset(stmt_knownNode_select));
#endif
}


bool RollbackManager::registerRow(const ActionRow & row)
{
#if USE_SQLITE3
	sqlite3_stmt * stmt_do = (row.id) ? stmt_replace : stmt_insert;

	bool nodeMeta = false;

	SQLOK(sqlite3_bind_int  (stmt_do, 1, row.actor));
	SQLOK(sqlite3_bind_int64(stmt_do, 2, row.timestamp));
	SQLOK(sqlite3_bind_int  (stmt_do, 3, row.type));

	if (row.type == RollbackAction::TYPE_MODIFY_INVENTORY_STACK) {
		const std::string & loc = row.location;
		nodeMeta = (loc.substr(0, 9) == "nodemeta:");

		SQLOK(sqlite3_bind_text(stmt_do, 4, row.list.c_str(), row.list.size(), NULL));
		SQLOK(sqlite3_bind_int (stmt_do, 5, row.index));
		SQLOK(sqlite3_bind_int (stmt_do, 6, row.add));
		SQLOK(sqlite3_bind_int (stmt_do, 7, row.stack.id));
		SQLOK(sqlite3_bind_int (stmt_do, 8, row.stack.count));
		SQLOK(sqlite3_bind_int (stmt_do, 9, (int) nodeMeta));

		if (nodeMeta) {
			std::string::size_type p1, p2;
			p1 = loc.find(':') + 1;
			p2 = loc.find(',');
			std::string x = loc.substr(p1, p2 - p1);
			p1 = p2 + 1;
			p2 = loc.find(',', p1);
			std::string y = loc.substr(p1, p2 - p1);
			std::string z = loc.substr(p2 + 1);
			SQLOK(sqlite3_bind_int(stmt_do, 10, atoi(x.c_str())));
			SQLOK(sqlite3_bind_int(stmt_do, 11, atoi(y.c_str())));
			SQLOK(sqlite3_bind_int(stmt_do, 12, atoi(z.c_str())));
		}
	} else {
		SQLOK(sqlite3_bind_null(stmt_do, 4));
		SQLOK(sqlite3_bind_null(stmt_do, 5));
		SQLOK(sqlite3_bind_null(stmt_do, 6));
		SQLOK(sqlite3_bind_null(stmt_do, 7));
		SQLOK(sqlite3_bind_null(stmt_do, 8));
		SQLOK(sqlite3_bind_null(stmt_do, 9));
	}

	if (row.type == RollbackAction::TYPE_SET_NODE) {
		SQLOK(sqlite3_bind_int (stmt_do, 10, row.x));
		SQLOK(sqlite3_bind_int (stmt_do, 11, row.y));
		SQLOK(sqlite3_bind_int (stmt_do, 12, row.z));
		SQLOK(sqlite3_bind_int (stmt_do, 13, row.oldNode));
		SQLOK(sqlite3_bind_int (stmt_do, 14, row.oldParam1));
		SQLOK(sqlite3_bind_int (stmt_do, 15, row.oldParam2));
		SQLOK(sqlite3_bind_text(stmt_do, 16, row.oldMeta.c_str(), row.oldMeta.size(), NULL));
		SQLOK(sqlite3_bind_int (stmt_do, 17, row.newNode));
		SQLOK(sqlite3_bind_int (stmt_do, 18, row.newParam1));
		SQLOK(sqlite3_bind_int (stmt_do, 19, row.newParam2));
		SQLOK(sqlite3_bind_text(stmt_do, 20, row.newMeta.c_str(), row.newMeta.size(), NULL));
		SQLOK(sqlite3_bind_int (stmt_do, 21, row.guessed ? 1 : 0));
	} else {
		if (!nodeMeta) {
			SQLOK(sqlite3_bind_null(stmt_do, 10));
			SQLOK(sqlite3_bind_null(stmt_do, 11));
			SQLOK(sqlite3_bind_null(stmt_do, 12));
		}
		SQLOK(sqlite3_bind_null(stmt_do, 13));
		SQLOK(sqlite3_bind_null(stmt_do, 14));
		SQLOK(sqlite3_bind_null(stmt_do, 15));
		SQLOK(sqlite3_bind_null(stmt_do, 16));
		SQLOK(sqlite3_bind_null(stmt_do, 17));
		SQLOK(sqlite3_bind_null(stmt_do, 18));
		SQLOK(sqlite3_bind_null(stmt_do, 19));
		SQLOK(sqlite3_bind_null(stmt_do, 20));
		SQLOK(sqlite3_bind_null(stmt_do, 21));
	}

	if (row.id) {
		SQLOK(sqlite3_bind_int(stmt_do, 22, row.id));
	}

	int written = sqlite3_step(stmt_do);

	SQLOK(sqlite3_reset(stmt_do));

	return written == SQLITE_DONE;
#else
	return false;
#endif
}


#if USE_SQLITE3
const std::list<ActionRow> RollbackManager::actionRowsFromSelect(sqlite3_stmt* stmt)
{
	std::list<ActionRow> rows;
	const unsigned char * text;
	size_t size;

	while (sqlite3_step(stmt) == SQLITE_ROW) {
		ActionRow row;

		row.actor     = sqlite3_column_int  (stmt, 0);
		row.timestamp = sqlite3_column_int64(stmt, 1);
		row.type      = sqlite3_column_int  (stmt, 2);

		if (row.type == RollbackAction::TYPE_MODIFY_INVENTORY_STACK) {
			text = sqlite3_column_text (stmt, 3);
			size = sqlite3_column_bytes(stmt, 3);
			row.list        = std::string(reinterpret_cast<const char*>(text), size);
			row.index       = sqlite3_column_int(stmt, 4);
			row.add         = sqlite3_column_int(stmt, 5);
			row.stack.id    = sqlite3_column_int(stmt, 6);
			row.stack.count = sqlite3_column_int(stmt, 7);
			row.nodeMeta    = sqlite3_column_int(stmt, 8);
		}

		if (row.type == RollbackAction::TYPE_SET_NODE || row.nodeMeta) {
			row.x = sqlite3_column_int(stmt,  9);
			row.y = sqlite3_column_int(stmt, 10);
			row.z = sqlite3_column_int(stmt, 11);
		}

		if (row.type == RollbackAction::TYPE_SET_NODE) {
			row.oldNode   = sqlite3_column_int(stmt, 12);
			row.oldParam1 = sqlite3_column_int(stmt, 13);
			row.oldParam2 = sqlite3_column_int(stmt, 14);
			text = sqlite3_column_text (stmt, 15);
			size = sqlite3_column_bytes(stmt, 15);
			row.oldMeta   = std::string(reinterpret_cast<const char*>(text), size);
			row.newNode   = sqlite3_column_int(stmt, 16);
			row.newParam1 = sqlite3_column_int(stmt, 17);
			row.newParam2 = sqlite3_column_int(stmt, 18);
			text = sqlite3_column_text(stmt, 19);
			size = sqlite3_column_bytes(stmt, 19);
			row.newMeta   = std::string(reinterpret_cast<const char*>(text), size);
			row.guessed   = sqlite3_column_int(stmt, 20);
		}

		if (row.nodeMeta) {
			row.location = "nodemeta:";
			row.location += itos(row.x);
			row.location += ',';
			row.location += itos(row.y);
			row.location += ',';
			row.location += itos(row.z);
		} else {
			row.location = getActorName(row.actor);
		}

		rows.push_back(row);
	}

	SQLOK(sqlite3_reset(stmt));

	return rows;
}
#endif


ActionRow RollbackManager::actionRowFromRollbackAction(const RollbackAction & action)
{
	ActionRow row;

	row.id        = 0;
	row.actor     = getActorId(action.actor);
	row.timestamp = action.unix_time;
	row.type      = action.type;

	if (row.type == RollbackAction::TYPE_MODIFY_INVENTORY_STACK) {
		row.location = action.inventory_location;
		row.list     = action.inventory_list;
		row.index    = action.inventory_index;
		row.add      = action.inventory_add;
		row.stack    = action.inventory_stack;
		row.stack.id = getNodeId(row.stack.name);
	} else {
		row.x         = action.p.X;
		row.y         = action.p.Y;
		row.z         = action.p.Z;
		row.oldNode   = getNodeId(action.n_old.name);
		row.oldParam1 = action.n_old.param1;
		row.oldParam2 = action.n_old.param2;
		row.oldMeta   = action.n_old.meta;
		row.newNode   = getNodeId(action.n_new.name);
		row.newParam1 = action.n_new.param1;
		row.newParam2 = action.n_new.param2;
		row.newMeta   = action.n_new.meta;
		row.guessed   = action.actor_is_guess;
	}

	return row;
}


const std::list<RollbackAction> RollbackManager::rollbackActionsFromActionRows(
		const std::list<ActionRow> & rows)
{
	std::list<RollbackAction> actions;

	for (std::list<ActionRow>::const_iterator it = rows.begin();
			it != rows.end(); ++it) {
		RollbackAction action;
		action.actor     = (it->actor) ? getActorName(it->actor) : "";
		action.unix_time = it->timestamp;
		action.type      = static_cast<RollbackAction::Type>(it->type);

		switch (action.type) {
		case RollbackAction::TYPE_MODIFY_INVENTORY_STACK:
			action.inventory_location = it->location.c_str();
			action.inventory_list     = it->list;
			action.inventory_index    = it->index;
			action.inventory_add      = it->add;
			action.inventory_stack    = it->stack;
			if (action.inventory_stack.name.empty()) {
				action.inventory_stack.name = getNodeName(it->stack.id);
			}
			break;

		case RollbackAction::TYPE_SET_NODE:
			action.p            = v3s16(it->x, it->y, it->z);
			action.n_old.name   = getNodeName(it->oldNode);
			action.n_old.param1 = it->oldParam1;
			action.n_old.param2 = it->oldParam2;
			action.n_old.meta   = it->oldMeta;
			action.n_new.name   = getNodeName(it->newNode);
			action.n_new.param1 = it->newParam1;
			action.n_new.param2 = it->newParam2;
			action.n_new.meta   = it->newMeta;
			break;

		default:
			throw ("W.T.F.");
			break;
		}

		actions.push_back(action);
	}

	return actions;
}


const std::list<ActionRow> RollbackManager::getRowsSince(time_t firstTime, const std::string & actor)
{
#if USE_SQLITE3
	sqlite3_stmt *stmt_stmt = actor.empty() ? stmt_select : stmt_select_withActor;
	sqlite3_bind_int64(stmt_stmt, 1, firstTime);

	if (!actor.empty()) {
		sqlite3_bind_int(stmt_stmt, 2, getActorId(actor));
	}

	const std::list<ActionRow> & rows = actionRowsFromSelect(stmt_stmt);
	sqlite3_reset(stmt_stmt);

	return rows;
#else
	return std::list<ActionRow>();
#endif
}


const std::list<ActionRow> RollbackManager::getRowsSince_range(
		time_t start_time, v3s16 p, int range, int limit)
{
#if USE_SQLITE3

	sqlite3_bind_int64(stmt_select_range, 1, start_time);
	sqlite3_bind_int  (stmt_select_range, 2, static_cast<int>(p.X - range));
	sqlite3_bind_int  (stmt_select_range, 3, static_cast<int>(p.X + range));
	sqlite3_bind_int  (stmt_select_range, 4, static_cast<int>(p.Y - range));
	sqlite3_bind_int  (stmt_select_range, 5, static_cast<int>(p.Y + range));
	sqlite3_bind_int  (stmt_select_range, 6, static_cast<int>(p.Z - range));
	sqlite3_bind_int  (stmt_select_range, 7, static_cast<int>(p.Z + range));
	sqlite3_bind_int  (stmt_select_range, 8, limit);

	const std::list<ActionRow> & rows = actionRowsFromSelect(stmt_select_range);
	sqlite3_reset(stmt_select_range);

	return rows;
#else
	return std::list<ActionRow>();
#endif

}


const std::list<RollbackAction> RollbackManager::getActionsSince_range(
		time_t start_time, v3s16 p, int range, int limit)
{
	return rollbackActionsFromActionRows(getRowsSince_range(start_time, p, range, limit));
}


const std::list<RollbackAction> RollbackManager::getActionsSince(
		time_t start_time, const std::string & actor)
{
	return rollbackActionsFromActionRows(getRowsSince(start_time, actor));
}


void RollbackManager::migrate(const std::string & file_path)
{
#if USE_SQLITE3
	std::cout << "Migrating from rollback.txt to rollback.sqlite." << std::endl;

	std::ifstream fh(file_path.c_str(), std::ios::in | std::ios::ate);
	if (!fh.good()) {
		throw FileNotGoodException("Unable to open rollback.txt");
	}

	std::streampos file_size = fh.tellg();

	if (file_size > 10) {
		errorstream << "Empty rollback log." << std::endl;
		return;
	}

	fh.seekg(0);

	std::string bit;
	int i = 0;
	int id = 1;
	time_t start = time(0);
	time_t t = start;
	sqlite3_exec(db, "BEGIN", NULL, NULL, NULL);
	do {
		ActionRow row;

		row.id = id;

		// Get the timestamp
		std::getline(fh, bit, ' ');
		bit = trim(bit);
		if (!atoi(bit.c_str())) {
			std::getline(fh, bit);
			continue;
		}
		row.timestamp = atoi(bit.c_str());

		// Get the actor
		row.actor = getActorId(deSerializeJsonString(fh));

		// Get the action type
		std::getline(fh, bit, '[');
		std::getline(fh, bit, ' ');

		if (bit == "modify_inventory_stack") {
			row.type = RollbackAction::TYPE_MODIFY_INVENTORY_STACK;
			row.location = trim(deSerializeJsonString(fh));
			std::getline(fh, bit, ' ');
			row.list     = trim(deSerializeJsonString(fh));
			std::getline(fh, bit, ' ');
			std::getline(fh, bit, ' ');
			row.index    = atoi(trim(bit).c_str());
			std::getline(fh, bit, ' ');
			row.add      = (int)(trim(bit) == "add");
			row.stack.deSerialize(deSerializeJsonString(fh));
			row.stack.id = getNodeId(row.stack.name);
			std::getline(fh, bit);
		} else if (bit == "set_node") {
			row.type = RollbackAction::TYPE_SET_NODE;
			std::getline(fh, bit, '(');
			std::getline(fh, bit, ',');
			row.x       = atoi(trim(bit).c_str());
			std::getline(fh, bit, ',');
			row.y       = atoi(trim(bit).c_str());
			std::getline(fh, bit, ')');
			row.z       = atoi(trim(bit).c_str());
			std::getline(fh, bit, ' ');
			row.oldNode = getNodeId(trim(deSerializeJsonString(fh)));
			std::getline(fh, bit, ' ');
			std::getline(fh, bit, ' ');
			row.oldParam1 = atoi(trim(bit).c_str());
			std::getline(fh, bit, ' ');
			row.oldParam2 = atoi(trim(bit).c_str());
			row.oldMeta   = trim(deSerializeJsonString(fh));
			std::getline(fh, bit, ' ');
			row.newNode   = getNodeId(trim(deSerializeJsonString(fh)));
			std::getline(fh, bit, ' ');
			std::getline(fh, bit, ' ');
			row.newParam1 = atoi(trim(bit).c_str());
			std::getline(fh, bit, ' ');
			row.newParam2 = atoi(trim(bit).c_str());
			row.newMeta   = trim(deSerializeJsonString(fh));
			std::getline(fh, bit, ' ');
			std::getline(fh, bit, ' ');
			std::getline(fh, bit);
			row.guessed = (int)(trim(bit) == "actor_is_guess");
		} else {
			errorstream << "Unrecognized rollback action type \""
				<< bit << "\"!" << std::endl;
			continue;
		}

		registerRow(row);
		++i;

		if (time(0) - t >= 1) {
			sqlite3_exec(db, "COMMIT", NULL, NULL, NULL);
			t = time(0);
			std::cout
				<< " Done: " << static_cast<int>((static_cast<float>(fh.tellg()) / static_cast<float>(file_size)) * 100) << "%"
				<< " Speed: " << i / (t - start) << "/second     \r" << std::flush;
			sqlite3_exec(db, "BEGIN", NULL, NULL, NULL);
		}
	} while (fh.good());

	std::cout
		<< " Done: 100%                                   " << std::endl
		<< "Now you can delete the old rollback.txt file." << std::endl;
#endif
}


// Get nearness factor for subject's action for this action
// Return value: 0 = impossible, >0 = factor
float RollbackManager::getSuspectNearness(bool is_guess, v3s16 suspect_p,
		time_t suspect_t, v3s16 action_p, time_t action_t)
{
	// Suspect cannot cause things in the past
	if (action_t < suspect_t) {
		return 0;        // 0 = cannot be
	}
	// Start from 100
	int f = 100;
	// Distance (1 node = -x points)
	f -= POINTS_PER_NODE * intToFloat(suspect_p, 1).getDistanceFrom(intToFloat(action_p, 1));
	// Time (1 second = -x points)
	f -= 1 * (action_t - suspect_t);
	// If is a guess, halve the points
	if (is_guess) {
		f *= 0.5;
	}
	// Limit to 0
	if (f < 0) {
		f = 0;
	}
	return f;
}


void RollbackManager::reportAction(const RollbackAction &action_)
{
	// Ignore if not important
	if (!action_.isImportant(gamedef)) {
		return;
	}

	RollbackAction action = action_;
	action.unix_time = time(0);

	// Figure out actor
	action.actor = current_actor;
	action.actor_is_guess = current_actor_is_guess;

	if (action.actor.empty()) { // If actor is not known, find out suspect or cancel
		v3s16 p;
		if (!action.getPosition(&p)) {
			return;
		}

		action.actor = getSuspect(p, 83, 1);
		if (action.actor.empty()) {
			return;
		}

		action.actor_is_guess = true;
	}

	addAction(action);
}

std::string RollbackManager::getActor()
{
	return current_actor;
}

bool RollbackManager::isActorGuess()
{
	return current_actor_is_guess;
}

void RollbackManager::setActor(const std::string & actor, bool is_guess)
{
	current_actor = actor;
	current_actor_is_guess = is_guess;
}

std::string RollbackManager::getSuspect(v3s16 p, float nearness_shortcut,
		float min_nearness)
{
	if (current_actor != "") {
		return current_actor;
	}
	int cur_time = time(0);
	time_t first_time = cur_time - (100 - min_nearness);
	RollbackAction likely_suspect;
	float likely_suspect_nearness = 0;
	for (std::list<RollbackAction>::const_reverse_iterator
	     i = action_latest_buffer.rbegin();
	     i != action_latest_buffer.rend(); i++) {
		if (i->unix_time < first_time) {
			break;
		}
		if (i->actor == "") {
			continue;
		}
		// Find position of suspect or continue
		v3s16 suspect_p;
		if (!i->getPosition(&suspect_p)) {
			continue;
		}
		float f = getSuspectNearness(i->actor_is_guess, suspect_p,
					     i->unix_time, p, cur_time);
		if (f >= min_nearness && f > likely_suspect_nearness) {
			likely_suspect_nearness = f;
			likely_suspect = *i;
			if (likely_suspect_nearness >= nearness_shortcut) {
				break;
			}
		}
	}
	// No likely suspect was found
	if (likely_suspect_nearness == 0) {
		return "";
	}
	// Likely suspect was found
	return likely_suspect.actor;
}


void RollbackManager::flush()
{
#if USE_SQLITE3
	sqlite3_exec(db, "BEGIN", NULL, NULL, NULL);

	std::list<RollbackAction>::const_iterator iter;

	for (iter  = action_todisk_buffer.begin();
			iter != action_todisk_buffer.end();
			iter++) {
		if (iter->actor == "") {
			continue;
		}

		registerRow(actionRowFromRollbackAction(*iter));
	}

	sqlite3_exec(db, "COMMIT", NULL, NULL, NULL);
	action_todisk_buffer.clear();
#endif
}


void RollbackManager::addAction(const RollbackAction & action)
{
	action_todisk_buffer.push_back(action);
	action_latest_buffer.push_back(action);

	// Flush to disk sometimes
	if (action_todisk_buffer.size() >= 500) {
		flush();
	}
}

std::list<RollbackAction> RollbackManager::getEntriesSince(time_t first_time)
{
	flush();
	return getActionsSince(first_time);
}

std::list<RollbackAction> RollbackManager::getNodeActors(v3s16 pos, int range,
		time_t seconds, int limit)
{
	time_t cur_time = time(0);
	time_t first_time = cur_time - seconds;

	return getActionsSince_range(first_time, pos, range, limit);
}

std::list<RollbackAction> RollbackManager::getRevertActions(
		const std::string &actor_filter,
		time_t seconds)
{
	time_t cur_time = time(0);
	time_t first_time = cur_time - seconds;

	flush();

	return getActionsSince(first_time, actor_filter);
}

