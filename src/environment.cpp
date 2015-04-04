/*
environment.cpp
Copyright (C) 2010-2013 celeron55, Perttu Ahola <celeron55@gmail.com>
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

#include <fstream>
#include "environment.h"
#include "filesys.h"
#include "porting.h"
#include "collision.h"
#include "content_mapnode.h"
#include "mapblock.h"
#include "serverobject.h"
#include "content_sao.h"
#include "settings.h"
#include "log_types.h"
#include "profiler.h"
#include "scripting_game.h"
#include "nodedef.h"
#include "nodemetadata.h"
//#include <fstream>
#include "gamedef.h"
#ifndef SERVER
#include "clientmap.h"
#include "localplayer.h"
#include "mapblock_mesh.h"
#include "event.h"
#endif
#include "daynightratio.h"
#include "map.h"
#include "emerge.h"
#include "util/serialize.h"
#include "fmbitset.h"
#include "circuit.h"
#include "key_value_storage.h"
#include <random>

#define PP(x) "("<<(x).X<<","<<(x).Y<<","<<(x).Z<<")"

std::random_device random_device; // todo: move me to random.h
std::mt19937 random_gen(random_device());

Environment::Environment():
	m_time_of_day_speed(0),
	m_time_counter(0),
	m_enable_day_night_ratio_override(false),
	m_day_night_ratio_override(0.0f)
{
	m_time_of_day = 9000;
	m_cache_enable_shaders = g_settings->getBool("enable_shaders");
}

Environment::~Environment()
{
	// Deallocate players
	for(std::vector<Player*>::iterator i = m_players.begin();
			i != m_players.end(); ++i) {
		delete (*i);
	}
}

void Environment::addPlayer(Player *player)
{
	DSTACK(__FUNCTION_NAME);
	/*
		Check that peer_ids are unique.
		Also check that names are unique.
		Exception: there can be multiple players with peer_id=0
	*/
	// If peer id is non-zero, it has to be unique.
	if(player->peer_id != 0)
		FATAL_ERROR_IF(getPlayer(player->peer_id) != NULL, "Peer id not unique");
	// Name has to be unique.
	FATAL_ERROR_IF(getPlayer(player->getName()) != NULL, "Player name not unique");
	// Add.
	m_players.push_back(player);
}

/*
void Environment::removePlayer(u16 peer_id)
{
	DSTACK(__FUNCTION_NAME);

	for(std::vector<Player*>::iterator i = m_players.begin();
			i != m_players.end();)
	{
		Player *player = *i;
		if(player->peer_id == peer_id) {
			delete player;
			i = m_players.erase(i);
		} else {
			++i;
		}
	}
}

void Environment::removePlayer(const std::string &name)
{
	for (std::vector<Player*>::iterator it = m_players.begin();
			it != m_players.end(); ++it) {
		if ((*it)->getName() == name) {
			delete *it;
			m_players.erase(it);
			return;
		}
	}
}
*/

Player * Environment::getPlayer(u16 peer_id)
{
	for(std::vector<Player*>::iterator i = m_players.begin();
			i != m_players.end(); ++i) {
		Player *player = *i;
		if(player->peer_id == peer_id)
			return player;
	}
	return NULL;
}

Player * Environment::getPlayer(const std::string &name)
{
	for(auto &player : m_players) {
 		if(player->getName() == name)
			return player;
	}
	return NULL;
}

std::vector<Player*> Environment::getPlayers()
{
	return m_players;
}

std::vector<Player*> Environment::getPlayers(bool ignore_disconnected)
{
	std::vector<Player*> newlist;
	for(std::vector<Player*>::iterator
			i = m_players.begin();
			i != m_players.end(); ++i) {
		Player *player = *i;

		if(ignore_disconnected) {
			// Ignore disconnected players
			if(player->peer_id == 0)
				continue;
		}

		newlist.push_back(player);
	}
	return newlist;
}

u32 Environment::getDayNightRatio()
{
	if(m_enable_day_night_ratio_override)
		return m_day_night_ratio_override;
	return time_to_daynight_ratio(m_time_of_day, m_cache_enable_shaders);
}

void Environment::setTimeOfDaySpeed(float speed)
{
	JMutexAutoLock lock(this->m_timeofday_lock);
	m_time_of_day_speed = speed;
}

float Environment::getTimeOfDaySpeed()
{
	JMutexAutoLock lock(this->m_timeofday_lock);
	float retval = m_time_of_day_speed;
	return retval;
}

void Environment::setTimeOfDay(u32 time)
{
	JMutexAutoLock lock(this->m_time_lock);
	m_time_of_day = time;
}

u32 Environment::getTimeOfDay()
{
	JMutexAutoLock lock(this->m_time_lock);
	u32 retval = m_time_of_day;
	return retval;
}

float Environment::getTimeOfDayF()
{
	JMutexAutoLock lock(this->m_time_lock);
	return (float)m_time_of_day / 24000.0;
}

void Environment::stepTimeOfDay(float dtime)
{
	float day_speed = getTimeOfDaySpeed();

	m_time_counter += dtime;
	f32 speed = day_speed * 24000./(24.*3600);
	u32 units = (u32)(m_time_counter*speed);
	if(units > 0){
		m_time_of_day = (m_time_of_day + units) % 24000;
	}
	if (speed > 0) {
		m_time_counter -= (f32)units / speed;
	}
}

/*
	ABMWithState
*/

ABMWithState::ABMWithState(ActiveBlockModifier *abm_, ServerEnvironment *senv):
	abm(abm_),
	timer(0),
	required_neighbors(CONTENT_ID_CAPACITY),
	required_neighbors_activate(CONTENT_ID_CAPACITY)
{
	auto ndef = senv->getGameDef()->ndef();
	interval = abm->getTriggerInterval();
	if (!interval)
		interval = 10;
	chance = abm->getTriggerChance();
	if (!chance)
		chance = 50;

	// abm process may be very slow if > 1
	neighbors_range = abm->getNeighborsRange();
	int nr_max = g_settings->getS32("abm_neighbors_range_max");
	if (!neighbors_range)
		neighbors_range = 1;
	else if (neighbors_range > nr_max)
		neighbors_range = nr_max;

	// Initialize timer to random value to spread processing
	float itv = MYMAX(0.001, interval); // No less than 1ms
	int minval = MYMAX(-0.51*itv, -60); // Clamp to
	int maxval = MYMIN(0.51*itv, 60);   // +-60 seconds
	timer = myrand_range(minval, maxval);

	for(auto & i : abm->getRequiredNeighbors(0))
		ndef->getIds(i, required_neighbors);

	for(auto & i : abm->getRequiredNeighbors(1))
		ndef->getIds(i, required_neighbors_activate);

	for(auto & i : abm->getTriggerContents())
		ndef->getIds(i, trigger_ids);
}

/*
	ActiveBlockList
*/

void fillRadiusBlock(v3s16 p0, s16 r, std::set<v3s16> &list)
{
	v3s16 p;
	for(p.X=p0.X-r; p.X<=p0.X+r; p.X++)
	for(p.Y=p0.Y-r; p.Y<=p0.Y+r; p.Y++)
	for(p.Z=p0.Z-r; p.Z<=p0.Z+r; p.Z++)
	{
		// Set in list
		list.insert(p);
	}
}

void ActiveBlockList::update(std::vector<v3s16> &active_positions,
		s16 radius,
		std::set<v3s16> &blocks_removed,
		std::set<v3s16> &blocks_added)
{
	/*
		Create the new list
	*/
	std::set<v3s16> newlist = m_forceloaded_list;
	for(std::vector<v3s16>::iterator i = active_positions.begin();
			i != active_positions.end(); ++i)
	{
		fillRadiusBlock(*i, radius, newlist);
	}

	/*
		Find out which blocks on the old list are not on the new list
	*/
	// Go through old list
	for(auto i = m_list.begin();
			i != m_list.end(); ++i)
	{
		v3POS p = i->first;
		// If not on new list, it's been removed
		if(newlist.find(p) == newlist.end())
			blocks_removed.insert(p);
	}

	/*
		Find out which blocks on the new list are not on the old list
	*/
	// Go through new list
	for(std::set<v3s16>::iterator i = newlist.begin();
			i != newlist.end(); ++i)
	{
		v3s16 p = *i;
		// If not on old list, it's been added
		if(m_list.find(p) == m_list.end())
			blocks_added.insert(p);
	}

	/*
		Update m_list
	*/
	m_list.clear();
	for(std::set<v3s16>::iterator i = newlist.begin();
			i != newlist.end(); ++i)
	{
		v3s16 p = *i;
		m_list.set(p, 1);
	}
}

/*
	ServerEnvironment
*/

ServerEnvironment::ServerEnvironment(ServerMap *map,
		GameScripting *scriptIface,
		IGameDef *gamedef,
		const std::string &path_world) :
	m_abmhandler(this),
	m_game_time_start(0),
	m_map(map),
	m_script(scriptIface),
	m_gamedef(gamedef),
	m_circuit(m_script, map, gamedef->ndef(), path_world),
	m_key_value_storage(path_world, "key_value_storage"),
	m_players_storage(path_world, "players"),
	m_path_world(path_world),
	m_send_recommended_timer(0),
	m_active_objects_last(0),
	m_active_block_abm_last(0),
	m_active_block_abm_dtime(0),
	m_active_block_abm_dtime_counter(0),
	m_active_block_timer_last(0),
	m_blocks_added_last(0),
	m_active_block_analyzed_last(0),
	m_game_time_fraction_counter(0),
	m_recommended_send_interval(g_settings->getFloat("dedicated_server_step")),
	m_max_lag_estimate(0.1)
{
	m_game_time = 0;
	m_use_weather = g_settings->getBool("weather");

	if (!m_key_value_storage.db)
		errorstream << "Cant open KV storage: "<< m_key_value_storage.error << std::endl;
	if (!m_players_storage.db)
		errorstream << "Cant open players storage: "<< m_players_storage.error << std::endl;

}

ServerEnvironment::~ServerEnvironment()
{
	// Clear active block list.
	// This makes the next one delete all active objects.
	m_active_blocks.clear();

	// Convert all objects to static and delete the active objects
	deactivateFarObjects(true);

	// Drop/delete map
	m_map->drop();

	// Delete ActiveBlockModifiers
	for(std::vector<ABMWithState>::iterator
			i = m_abms.begin(); i != m_abms.end(); ++i){
		delete i->abm;
	}
}

Map & ServerEnvironment::getMap()
{
	return *m_map;
}

ServerMap & ServerEnvironment::getServerMap()
{
	return *m_map;
}

KeyValueStorage *ServerEnvironment::getKeyValueStorage()
{
	return &m_key_value_storage;
}

bool ServerEnvironment::line_of_sight(v3f pos1, v3f pos2, float stepsize, v3s16 *p)
{
	float distance = pos1.getDistanceFrom(pos2);

	//calculate normalized direction vector
	v3f normalized_vector = v3f((pos2.X - pos1.X)/distance,
				(pos2.Y - pos1.Y)/distance,
				(pos2.Z - pos1.Z)/distance);

	//find out if there's a node on path between pos1 and pos2
	for (float i = 1; i < distance; i += stepsize) {
		v3s16 pos = floatToInt(v3f(normalized_vector.X * i,
				normalized_vector.Y * i,
				normalized_vector.Z * i) +pos1,BS);

		MapNode n = getMap().getNodeNoEx(pos);

		if(n.param0 != CONTENT_AIR) {
			if (p) {
				*p = pos;
			}
			return false;
		}
	}
	return true;
}

void ServerEnvironment::saveLoadedPlayers()
{
	auto i = m_players.begin();
	while (i != m_players.end())
	{
		auto *player = *i;
		savePlayer(player->getName());
		if(!player->peer_id && !player->getPlayerSAO() && player->refs <= 0) {
			delete player;
			i = m_players.erase(i);
		} else {
			++i;
		}
	}
}

void ServerEnvironment::savePlayer(const std::string &playername)
{
	auto *player = getPlayer(playername);
	if (!player)
		return;
	Json::Value player_json;
	player_json << *player;
	m_players_storage.put_json("p." + player->getName(), player_json);
}

Player * ServerEnvironment::loadPlayer(const std::string &playername)
{
	bool newplayer = false;
	bool found = false;
	auto *player = getPlayer(playername);

	if (!player) {
		player = new RemotePlayer(m_gamedef, "");
		newplayer = true;
	}

	try {
		Json::Value player_json;
		m_players_storage.get_json("p." + playername, player_json);
		verbosestream<<"Reading kv player "<<playername<<std::endl;
		if (!player_json.empty()) {
			player_json >> *player;
			if (newplayer) {
				addPlayer(player);
			}
			return player;
		}
	} catch (...)  {
	}

	//TODO: REMOVE OLD SAVE TO FILE:

	if(!string_allowed(playername, PLAYERNAME_ALLOWED_CHARS) || !playername.size()) {
		infostream<<"Not loading player with invalid name: "<<playername<<std::endl;
		return nullptr;
	}

	std::string players_path = m_path_world + DIR_DELIM "players" DIR_DELIM;

	std::string path = players_path + playername;
		// Open file and deserialize
		std::ifstream is(path.c_str(), std::ios_base::binary);
		if (!is.good()) {
			return NULL;
		}
		try {
		player->deSerialize(is, path);
		} catch (SerializationError e) {
			errorstream<<e.what()<<std::endl;
			return nullptr;
		}
		is.close();
		if (player->getName() == playername) {
			found = true;
		}
	if (!found) {
		infostream << "Player file for player " << playername
				<< " not found" << std::endl;
		if (newplayer)
			delete player;
		return NULL;
	}

	if (newplayer)
		addPlayer(player);
	return player;
}

void ServerEnvironment::saveMeta()
{
	std::string path = m_path_world + DIR_DELIM "env_meta.txt";

	// Open file and serialize
	std::ostringstream ss(std::ios_base::binary);

	Settings args;
	args.setU64("game_time", m_game_time);
	args.setU64("time_of_day", getTimeOfDay());
	args.writeLines(ss);
	ss<<"EnvArgsEnd\n";

	if(!fs::safeWriteToFile(path, ss.str()))
	{
		errorstream<<"ServerEnvironment::saveMeta(): Failed to write "
				<<path<<std::endl;
	}
}

void ServerEnvironment::loadMeta()
{
	std::string path = m_path_world + DIR_DELIM "env_meta.txt";

	// Open file and deserialize
	std::ifstream is(path.c_str(), std::ios_base::binary);
	if (!is.good()) {
		infostream << "ServerEnvironment::loadMeta(): Failed to open "
				<< path << std::endl;
		//throw SerializationError("Couldn't load env meta");
	}

	Settings args;

	if (!args.parseConfigLines(is, "EnvArgsEnd")) {
		errorstream << "ServerEnvironment::loadMeta(): EnvArgsEnd not found! in " << path << std::endl;
/*
		throw SerializationError("ServerEnvironment::loadMeta(): "
				"EnvArgsEnd not found!");
*/
	}

	try {
		m_game_time_start =
		m_game_time = args.getU64("game_time");
	} catch (SettingNotFoundException &e) {
		// Getting this is crucial, otherwise timestamps are useless
		//throw SerializationError("Couldn't load env meta game_time");
	}

	try {
		m_time_of_day = args.getU64("time_of_day");
	} catch (SettingNotFoundException &e) {
		// This is not as important
		m_time_of_day = 9000;
	}
}

	ABMHandler::ABMHandler(ServerEnvironment *env):
		m_env(env),
		m_aabms_empty(true)
	{
		m_aabms.fill(nullptr);
	}

	void ABMHandler::init(std::vector<ABMWithState> &abms) {
		for(auto & ai: abms){
			auto i = &ai;
			ActiveABM aabm;
			aabm.abmws = i;
			// Trigger contents
				for (auto &c : i->trigger_ids)
				{
					if (!m_aabms[c]) {
						m_aabms[c] = new std::vector<ActiveABM>;
						m_aabms_list.push_back(m_aabms[c]);
					}
					m_aabms[c]->push_back(aabm);
					m_aabms_empty = false;
				}
		}
	}

	ABMHandler::
	~ABMHandler() {
		for (auto i = m_aabms_list.begin();
				i != m_aabms_list.end(); ++i)
			delete *i;
	}

	// Find out how many objects the given block and its neighbours contain.
	// Returns the number of objects in the block, and also in 'wider' the
	// number of objects in the block and all its neighbours. The latter
	// may an estimate if any neighbours are unloaded.
	u32 ABMHandler::countObjects(MapBlock *block, ServerMap * map, u32 &wider)
	{
		wider = 0;
		u32 wider_unknown_count = 0;
		for(s16 x=-1; x<=1; x++)
		for(s16 y=-1; y<=1; y++)
		for(s16 z=-1; z<=1; z++)
		{
			MapBlock *block2 = map->getBlockNoCreateNoEx(
					block->getPos() + v3s16(x,y,z), true);
			if(block2==NULL){
				wider_unknown_count++;
				continue;
			}
			wider += block2->m_static_objects.m_active.size()
					+ block2->m_static_objects.m_stored.size();
		}
		// Extrapolate
		u32 active_object_count = block->m_static_objects.m_active.size();
		u32 wider_known_count = 3*3*3 - wider_unknown_count;
		if (wider_known_count)
		wider += wider_unknown_count * wider / wider_known_count;
		return active_object_count;
	}

	void ABMHandler::apply(MapBlock *block, bool activate)
	{
		if(m_aabms_empty)
			return;

		//infostream<<"ABMHandler::apply p="<<block->getPos()<<" block->abm_triggers="<<block->abm_triggers<<std::endl;
		{
			std::lock_guard<std::mutex> lock(block->abm_triggers_mutex);
			if (block->abm_triggers)
				block->abm_triggers->clear();
		}

#if ENABLE_THREADS
		auto map = std::unique_ptr<VoxelManipulator> (new VoxelManipulator);
		{
			//ScopeProfiler sp(g_profiler, "ABM copy", SPT_ADD);
			m_env->getServerMap().copy_27_blocks_to_vm(block, *map);
		}
#else
		ServerMap *map = &m_env->getServerMap();
#endif

		{
		//auto lock = block->try_lock_unique_rec();
		//if (!lock->owns_lock())
		//	return;
		}

		ScopeProfiler sp(g_profiler, "ABM select", SPT_ADD);


		u32 active_object_count_wider;
		u32 active_object_count = this->countObjects(block, &m_env->getServerMap(), active_object_count_wider);
		m_env->m_added_objects = 0;

		v3POS bpr = block->getPosRelative();
		v3s16 p0;
		for(p0.X=0; p0.X<MAP_BLOCKSIZE; p0.X++)
		for(p0.Y=0; p0.Y<MAP_BLOCKSIZE; p0.Y++)
		for(p0.Z=0; p0.Z<MAP_BLOCKSIZE; p0.Z++)
		{
			v3POS p = p0 + bpr;
#if ENABLE_THREADS
			MapNode n = map->getNodeTry(p);
#else
			MapNode n = block->getNodeTry(p0);
#endif
			content_t c = n.getContent();
			if (c == CONTENT_IGNORE)
				continue;

			if (!m_aabms[c]) {
				if (block->content_only)
					return;
				continue;
			}

			for(auto & ir: *(m_aabms[c])) {
				auto i = &ir;
				// Check neighbors
				v3POS neighbor_pos;
				auto & required_neighbors = activate ? ir.abmws->required_neighbors_activate : ir.abmws->required_neighbors;
				if(required_neighbors.count() > 0)
				{
					v3s16 p1;
					int neighbors_range = i->abmws->neighbors_range;
					for(p1.X = p.X - neighbors_range; p1.X <= p.X + neighbors_range; ++p1.X)
					for(p1.Y = p.Y - neighbors_range; p1.Y <= p.Y + neighbors_range; ++p1.Y)
					for(p1.Z = p.Z - neighbors_range; p1.Z <= p.Z + neighbors_range; ++p1.Z)
					{
						if(p1 == p)
							continue;
						MapNode n = map->getNodeTry(p1);
						content_t c = n.getContent();
						if (c == CONTENT_IGNORE)
							continue;
						if(required_neighbors.get(c)){
							neighbor_pos = p1;
							goto neighbor_found;
						}
					}
					// No required neighbor found
					continue;
				}
neighbor_found:

				std::lock_guard<std::mutex> lock(block->abm_triggers_mutex);

				if (!block->abm_triggers)
					block->abm_triggers = std::unique_ptr<MapBlock::abm_triggers_type>(new MapBlock::abm_triggers_type); // c++14: make_unique here

				block->abm_triggers->emplace_back(abm_trigger_one{i, p, c, active_object_count, active_object_count_wider, neighbor_pos, activate});
			}
		}
	//infostream<<"ABMHandler::apply reult p="<<block->getPos()<<" apply result:"<< (block->abm_triggers ? block->abm_triggers->size() : 0) <<std::endl;

	}

void MapBlock::abmTriggersRun(ServerEnvironment * m_env, u32 time, bool activate) {
		ScopeProfiler sp(g_profiler, "ABM trigger blocks", SPT_ADD);

		std::unique_lock<std::mutex> lock(abm_triggers_mutex);
		if (!abm_triggers)
			return;

		if (!lock.owns_lock())
			return;

		ServerMap *map = &m_env->getServerMap();

		float dtime = 0;
		if (m_abm_timestamp) {
			dtime = time - m_abm_timestamp;
		} else {
			u32 ts = getActualTimestamp();
			if (ts)
				dtime = time - ts;
			else
				dtime = 1;
		}

		//infostream<<"MapBlock::abmTriggersRun p="<<getPos()<<" abm_triggers="<<abm_triggers<<" size()="<<abm_triggers->size()<<" time="<<time<<" dtime="<<dtime<<" activate="<<activate<<std::endl;
		m_abm_timestamp = time;
		for (auto abm_trigger = abm_triggers->begin(); abm_trigger != abm_triggers->end() ; ++abm_trigger) {
			//ScopeProfiler sp2(g_profiler, "ABM trigger nodes test", SPT_ADD);
			auto & abm = abm_trigger->abm;
			float intervals = dtime / abm->abmws->interval;
			int chance = (abm->abmws->chance / intervals);
			//infostream<<"TST: dtime="<<dtime<<" Achance="<<abm->abmws->chance<<" Ainterval="<<abm->abmws->interval<< " Rchance="<<chance<<" Rintervals="<<intervals << std::endl;

			if(chance && myrand() % chance)
					continue;
			//infostream<<"HIT! dtime="<<dtime<<" Achance="<<abm->abmws->chance<<" Ainterval="<<abm->abmws->interval<< " Rchance="<<chance<<" Rintervals="<<intervals << std::endl;

			MapNode node = map->getNodeTry(abm_trigger->pos);
			if (node.getContent() != abm_trigger->content) {
				if (node)
					abm_trigger = abm_triggers->erase(abm_trigger);
				continue;
			}
			//ScopeProfiler sp3(g_profiler, "ABM trigger nodes call", SPT_ADD);

			abm->abmws->abm->trigger(m_env, abm_trigger->pos, node,
				abm_trigger->active_object_count, abm_trigger->active_object_count_wider, map->getNodeTry(abm_trigger->neighbor_pos), activate);

				// Count surrounding objects again if the abms added any
				if(m_env->m_added_objects > 0) {
					v3POS blockpos = getNodeBlockPos(abm_trigger->pos);
					MapBlock * block = map->getBlock(blockpos);
					if (block)
						abm_trigger->active_object_count = m_env->m_abmhandler.countObjects(block, map, abm_trigger->active_object_count_wider);
					m_env->m_added_objects = 0;
				}
		}
		if (abm_triggers->empty())
			abm_triggers.release();
}

void ServerEnvironment::analyzeBlock(MapBlock * block) {
	u32 block_timestamp = block->getActualTimestamp();
	if (block->m_next_analyze_timestamp > block_timestamp) {
		//infostream<<"not anlalyzing: "<< block->getPos() <<"ats="<<block->m_next_analyze_timestamp<< " bts="<<  block_timestamp<<std::endl;
		return;
	}
	ScopeProfiler sp(g_profiler, "ABM analyze", SPT_ADD);
	block->analyzeContent();
	bool activate = block_timestamp - block->m_next_analyze_timestamp > 3600;
	m_abmhandler.apply(block, activate);
	//infostream<<"ServerEnvironment::analyzeBlock p="<<block->getPos()<< " tdiff="<<block_timestamp - block->m_next_analyze_timestamp <<" co="<<block->content_only <<" triggers="<<(block->abm_triggers ? block->abm_triggers->size() : -1) <<std::endl;
	block->m_next_analyze_timestamp = block_timestamp + 5;
}


void ServerEnvironment::activateBlock(MapBlock *block, u32 additional_dtime)
{
	// Reset usage timer immediately, otherwise a block that becomes active
	// again at around the same time as it would normally be unloaded will
	// get unloaded incorrectly. (I think this still leaves a small possibility
	// of a race condition between this and server::AsyncRunStep, which only
	// some kind of synchronisation will fix, but it at least reduces the window
	// of opportunity for it to break from seconds to nanoseconds)
	block->resetUsageTimer();

	// Get time difference
	u32 dtime_s = 0;
	u32 stamp = block->getTimestamp();
	if(m_game_time > stamp && stamp != BLOCK_TIMESTAMP_UNDEFINED)
		dtime_s = m_game_time - stamp;
	dtime_s += additional_dtime;

	/*infostream<<"ServerEnvironment::activateBlock(): block timestamp: "
			<<stamp<<", game time: "<<m_game_time<<std::endl;*/

	// Set current time as timestamp
	block->setTimestampNoChangedFlag(m_game_time);

	/*infostream<<"ServerEnvironment::activateBlock(): block is "
			<<dtime_s<<" seconds old."<<std::endl;*/

	// Activate stored objects
	activateObjects(block, dtime_s);

//	// Calculate weather conditions
//	m_map->updateBlockHeat(this, block->getPos() *  MAP_BLOCKSIZE, block);

	// Run node timers
	std::map<v3s16, NodeTimer> elapsed_timers =
		block->m_node_timers.step((float)dtime_s);
	if(!elapsed_timers.empty()){
		MapNode n;
		for(std::map<v3s16, NodeTimer>::iterator
				i = elapsed_timers.begin();
				i != elapsed_timers.end(); i++){
			n = block->getNodeNoEx(i->first);
			v3s16 p = i->first + block->getPosRelative();
			if(m_script->node_on_timer(p,n,i->second.elapsed))
				block->setNodeTimer(i->first,NodeTimer(i->second.timeout,0));
		}
	}
}

void ServerEnvironment::addActiveBlockModifier(ActiveBlockModifier *abm)
{
	m_abms.push_back(ABMWithState(abm, this));
}

bool ServerEnvironment::setNode(v3s16 p, const MapNode &n, s16 fast)
{
	INodeDefManager *ndef = m_gamedef->ndef();
	MapNode n_old = m_map->getNodeNoEx(p);

	// Call destructor
	if (ndef->get(n_old).has_on_destruct)
		m_script->node_on_destruct(p, n_old);

	// Replace node

	if (fast) {
		try {
			MapNode nn = n;
			if (fast == 2)
				nn.param1 = n_old.param1;
			m_map->setNode(p, nn);
		} catch(InvalidPositionException &e) { }
	} else {
	if (!m_map->addNodeWithEvent(p, n))
		return false;
	}

	m_circuit.addNode(p);

	// Update active VoxelManipulator if a mapgen thread
	m_map->updateVManip(p);

	// Call post-destructor
	if (ndef->get(n_old).has_after_destruct)
		m_script->node_after_destruct(p, n_old);

	// Call constructor
	if (ndef->get(n).has_on_construct)
		m_script->node_on_construct(p, n);

	return true;
}

bool ServerEnvironment::removeNode(v3s16 p, s16 fast)
{
	INodeDefManager *ndef = m_gamedef->ndef();
	MapNode n_old = m_map->getNodeNoEx(p);

	// Call destructor
	if (ndef->get(n_old).has_on_destruct)
		m_script->node_on_destruct(p, n_old);

	// Replace with air
	// This is slightly optimized compared to addNodeWithEvent(air)
	if (fast) {
		MapNode n(CONTENT_AIR);
		try {
			if (fast == 2)
				n.param1 = n_old.param1;
			m_map->setNode(p, n);
		} catch(InvalidPositionException &e) { }
	} else {
	if (!m_map->removeNodeWithEvent(p))
		return false;
	}

	m_circuit.removeNode(p, n_old);

	// Update active VoxelManipulator if a mapgen thread
	m_map->updateVManip(p);

	// Call post-destructor
	if (ndef->get(n_old).has_after_destruct)
		m_script->node_after_destruct(p, n_old);

	// Air doesn't require constructor
	return true;
}

bool ServerEnvironment::swapNode(v3s16 p, const MapNode &n)
{
	//INodeDefManager *ndef = m_gamedef->ndef();
	MapNode n_old = m_map->getNodeNoEx(p);
	if (!m_map->addNodeWithEvent(p, n, false))
		return false;
	m_circuit.swapNode(p, n_old, n);

	// Update active VoxelManipulator if a mapgen thread
	m_map->updateVManip(p);

	return true;
}

std::set<u16> ServerEnvironment::getObjectsInsideRadius(v3f pos, float radius)
{
	std::set<u16> objects;
	auto lock = m_active_objects.lock_shared_rec();
	for(auto
			i = m_active_objects.begin();
			i != m_active_objects.end(); ++i)
	{
		ServerActiveObject* obj = i->second;
		u16 id = i->first;
		v3f objectpos = obj->getBasePosition();
		if(objectpos.getDistanceFrom(pos) > radius)
			continue;
		objects.insert(id);
	}
	return objects;
}

void ServerEnvironment::clearAllObjects()
{
	infostream<<"ServerEnvironment::clearAllObjects(): "
			<<"Removing all active objects"<<std::endl;
	std::vector<u16> objects_to_remove;
	auto lock = m_active_objects.lock_unique_rec();

	for(auto
			i = m_active_objects.begin();
			i != m_active_objects.end(); ++i) {
		ServerActiveObject* obj = i->second;
		if(obj->getType() == ACTIVEOBJECT_TYPE_PLAYER)
			continue;
		u16 id = i->first;
		// Delete static object if block is loaded
		if(obj->m_static_exists){
			MapBlock *block = m_map->getBlockNoCreateNoEx(obj->m_static_block);
			if(block){
				block->m_static_objects.remove(id);
				block->raiseModified(MOD_STATE_WRITE_NEEDED,
						"clearAllObjects");
				obj->m_static_exists = false;
			}
		}
		// If known by some client, don't delete immediately
		if(obj->m_known_by_count > 0){
			obj->m_pending_deactivation = true;
			obj->m_removed = true;
			continue;
		}

		// Tell the object about removal
		obj->removingFromEnvironment();
		// Deregister in scripting api
		m_script->removeObjectReference(obj);

		// Delete active object
		if(obj->environmentDeletes())
			delete obj;
		// Id to be removed from m_active_objects
		objects_to_remove.push_back(id);
	}

	// Remove references from m_active_objects
	for(std::vector<u16>::iterator i = objects_to_remove.begin();
			i != objects_to_remove.end(); ++i) {
		m_active_objects.erase(*i);
	}

	// Get list of loaded blocks
	std::vector<v3s16> loaded_blocks;
	infostream<<"ServerEnvironment::clearAllObjects(): "
			<<"Listing all loaded blocks"<<std::endl;
	m_map->listAllLoadedBlocks(loaded_blocks);
	infostream<<"ServerEnvironment::clearAllObjects(): "
			<<"Done listing all loaded blocks: "
			<<loaded_blocks.size()<<std::endl;

	// Get list of loadable blocks
	std::vector<v3s16> loadable_blocks;
	infostream<<"ServerEnvironment::clearAllObjects(): "
			<<"Listing all loadable blocks"<<std::endl;
	m_map->listAllLoadableBlocks(loadable_blocks);
	infostream<<"ServerEnvironment::clearAllObjects(): "
			<<"Done listing all loadable blocks: "
			<<loadable_blocks.size()
			<<", now clearing"<<std::endl;

	// Grab a reference on each loaded block to avoid unloading it
	for(std::vector<v3s16>::iterator i = loaded_blocks.begin();
			i != loaded_blocks.end(); ++i) {
		v3s16 p = *i;
		MapBlock *block = m_map->getBlockNoCreateNoEx(p);
		assert(block != NULL);
		block->refGrab();
	}

	// Remove objects in all loadable blocks
	u32 unload_interval = g_settings->getS32("max_clearobjects_extra_loaded_blocks");
	unload_interval = MYMAX(unload_interval, 1);
	u32 report_interval = loadable_blocks.size() / 10;
	u32 num_blocks_checked = 0;
	u32 num_blocks_cleared = 0;
	u32 num_objs_cleared = 0;
	for(std::vector<v3s16>::iterator i = loadable_blocks.begin();
			i != loadable_blocks.end(); ++i) {
		v3s16 p = *i;
		MapBlock *block = m_map->emergeBlock(p, false);
		if(!block){
			errorstream<<"ServerEnvironment::clearAllObjects(): "
					<<"Failed to emerge block "<<PP(p)<<std::endl;
			continue;
		}
		u32 num_stored = block->m_static_objects.m_stored.size();
		u32 num_active = block->m_static_objects.m_active.size();
		if(num_stored != 0 || num_active != 0){
			block->m_static_objects.m_stored.clear();
			block->m_static_objects.m_active.clear();
			block->raiseModified(MOD_STATE_WRITE_NEEDED,
					"clearAllObjects");
			num_objs_cleared += num_stored + num_active;
			num_blocks_cleared++;
		}
		num_blocks_checked++;

		if(report_interval != 0 &&
				num_blocks_checked % report_interval == 0){
			float percent = 100.0 * (float)num_blocks_checked /
					loadable_blocks.size();
			infostream<<"ServerEnvironment::clearAllObjects(): "
					<<"Cleared "<<num_objs_cleared<<" objects"
					<<" in "<<num_blocks_cleared<<" blocks ("
					<<percent<<"%)"<<std::endl;
		}
		if(num_blocks_checked % unload_interval == 0){
			m_map->unloadUnreferencedBlocks();
		}
	}
	m_map->unloadUnreferencedBlocks();

	// Drop references that were added above
	for(std::vector<v3s16>::iterator i = loaded_blocks.begin();
			i != loaded_blocks.end(); ++i) {
		v3s16 p = *i;
		MapBlock *block = m_map->getBlockNoCreateNoEx(p);
		if (!block)
			continue;
		block->refDrop();
	}

	infostream<<"ServerEnvironment::clearAllObjects(): "
			<<"Finished: Cleared "<<num_objs_cleared<<" objects"
			<<" in "<<num_blocks_cleared<<" blocks"<<std::endl;
}

void ServerEnvironment::step(float dtime, float uptime, unsigned int max_cycle_ms)
{
	DSTACK(__FUNCTION_NAME);

	//TimeTaker timer("ServerEnv step");

	/* Step time of day */
	stepTimeOfDay(dtime);

	// Update this one
	// NOTE: This is kind of funny on a singleplayer game, but doesn't
	// really matter that much.
	//m_recommended_send_interval = g_settings->getFloat("dedicated_server_step");

	/*
		Increment game time
	*/
	{
		m_game_time_fraction_counter += dtime;
		u32 inc_i = (u32)m_game_time_fraction_counter;
		m_game_time += inc_i;
		m_game_time_fraction_counter -= (float)inc_i;
	}

	TimeTaker timer_step("Environment step");
	g_profiler->add("SMap: Blocks", getMap().m_blocks.size());

	/*
		Handle players
	*/
	{
		//TimeTaker timer_step_player("player step");
		//ScopeProfiler sp(g_profiler, "SEnv: handle players avg", SPT_AVG);
		for(std::vector<Player*>::iterator i = m_players.begin();
				i != m_players.end(); ++i)
		{
			Player *player = *i;

			// Ignore disconnected players
			if(player->peer_id == 0)
				continue;

			// Move
			player->move(dtime, this, 100*BS);
		}
	}

	/*
	 * Update circuit
	 */
	m_circuit.update(dtime);

	/*
		Manage active block list
	*/
	if(m_blocks_added_last || m_active_blocks_management_interval.step(dtime, 2.0)) {
		//TimeTaker timer_s1("Manage active block list");
		ScopeProfiler sp(g_profiler, "SEnv: manage act. block list avg /2s", SPT_AVG);
		if (!m_blocks_added_last) {
		/*
			Get player block positions
		*/
		std::vector<v3s16> players_blockpos;
		for(std::vector<Player*>::iterator
				i = m_players.begin();
				i != m_players.end(); ++i) {
			Player *player = *i;
			// Ignore disconnected players
			if(player->peer_id == 0)
				continue;

			v3s16 blockpos = getNodeBlockPos(
					floatToInt(player->getPosition(), BS));
			players_blockpos.push_back(blockpos);
		}
		if (!m_blocks_added_last && g_settings->getBool("enable_force_load")) {
			//TimeTaker timer_s2("force load");
			auto lock = m_active_objects.try_lock_shared_rec();
			if (lock->owns_lock())
			for(auto
				i = m_active_objects.begin();
				i != m_active_objects.end(); ++i)
			{
				ServerActiveObject* obj = i->second;
				if(obj->getType() == ACTIVEOBJECT_TYPE_PLAYER)
					continue;
				ObjectProperties* props = obj->accessObjectProperties();
				if(props->force_load){
					v3f objectpos = obj->getBasePosition();
					v3s16 blockpos = getNodeBlockPos(
					floatToInt(objectpos, BS));
					players_blockpos.push_back(blockpos);
				}
			}
		}

		/*
			Update list of active blocks, collecting changes
		*/
		const s16 active_block_range = g_settings->getS16("active_block_range");
		std::set<v3s16> blocks_removed;
		m_active_blocks.update(players_blockpos, active_block_range,
				blocks_removed, m_blocks_added);

		/*
			Handle removed blocks
		*/

		// Convert active objects that are no more in active blocks to static
		deactivateFarObjects(false);

		} // if (!m_blocks_added_last)
		/*
			Handle added blocks
		*/

		u32 n = 0, end_ms = porting::getTimeMs() + max_cycle_ms;
		m_blocks_added_last = 0;
		auto i = m_blocks_added.begin();
		for(; i != m_blocks_added.end(); ++i) {
			++n;
			v3s16 p = *i;
			MapBlock *block = m_map->getBlockOrEmerge(p);
			if(block==NULL){
				m_active_blocks.m_list.erase(p);
				continue;
			}

			activateBlock(block);
			/* infostream<<"Server: Block " << PP(p)
				<< " became active"<<std::endl; */
			if (porting::getTimeMs() > end_ms) {
				m_blocks_added_last = n;
				break;
			}
		}
		m_blocks_added.erase(m_blocks_added.begin(), i);
	}

	if (!m_more_threads)
		analyzeBlocks(dtime, max_cycle_ms);

	/*
		Mess around in active blocks
	*/
	if(m_active_block_timer_last || m_active_blocks_nodemetadata_interval.step(dtime, 1.0)) {
		//if (!m_active_block_timer_last) infostream<<"Start ABM timer cycle s="<<m_active_blocks.m_list.size()<<std::endl;
		//TimeTaker timer_s1("Mess around in active blocks");
		//ScopeProfiler sp(g_profiler, "SEnv: mess in act. blocks avg /1s", SPT_AVG);

		//float dtime = 1.0;

		u32 n = 0, calls = 0, end_ms = porting::getTimeMs() + max_cycle_ms;
		auto lock = m_active_blocks.m_list.lock_shared_rec();
		for(auto
				i = m_active_blocks.m_list.begin();
				i != m_active_blocks.m_list.end(); ++i)
		{
			if (n++ < m_active_block_timer_last)
				continue;
			else
				m_active_block_timer_last = 0;
			++calls;

			v3POS p = i->first;

			/*infostream<<"Server: Block ("<<p.X<<","<<p.Y<<","<<p.Z
					<<") being handled"<<std::endl;*/

			MapBlock *block = m_map->getBlockNoCreateNoEx(p, true);
			if(block==NULL)
				continue;

			// Reset block usage timer
			block->resetUsageTimer();

			// Set current time as timestamp
			block->setTimestampNoChangedFlag(m_game_time);
			// If time has changed much from the one on disk,
			// set block to be saved when it is unloaded
/*
			if(block->getTimestamp() > block->getDiskTimestamp() + 60)
				block->raiseModified(MOD_STATE_WRITE_AT_UNLOAD,
						"Timestamp older than 60s (step)");
*/

			// Run node timers
			if (!block->m_node_timers.m_uptime_last)  // not very good place, but minimum modifications
				block->m_node_timers.m_uptime_last = uptime - dtime;
			std::map<v3s16, NodeTimer> elapsed_timers =
				block->m_node_timers.step(uptime - block->m_node_timers.m_uptime_last);
			block->m_node_timers.m_uptime_last = uptime;
			if(!elapsed_timers.empty()){
				MapNode n;
				for(std::map<v3s16, NodeTimer>::iterator
						i = elapsed_timers.begin();
						i != elapsed_timers.end(); i++){
					n = block->getNodeNoEx(i->first);
					p = i->first + block->getPosRelative();
					if(m_script->node_on_timer(p,n,i->second.elapsed))
						block->setNodeTimer(i->first,NodeTimer(i->second.timeout,0));
				}
			}

			if (porting::getTimeMs() > end_ms) {
				m_active_block_timer_last = n;
				break;
		}
	}
		if (!calls)
			m_active_block_timer_last = 0;
	}

	g_profiler->add("SMap: Blocks: Active", m_active_blocks.m_list.size());
	m_active_block_abm_dtime_counter += dtime;

	const float abm_interval = 1.0;
	if(m_active_block_abm_last || m_active_block_modifier_interval.step(dtime, abm_interval)) {
		ScopeProfiler sp(g_profiler, "SEnv: modify in blocks avg /1s", SPT_AVG);
		TimeTaker timer("modify in active blocks");

		u32 n = 0, calls = 0, end_ms = porting::getTimeMs() + max_cycle_ms;
		auto lock = m_active_blocks.m_list.lock_shared_rec();
		for(auto
				i = m_active_blocks.m_list.begin();
				i != m_active_blocks.m_list.end(); ++i)
		{
			if (n++ < m_active_block_abm_last)
				continue;
			else
				m_active_block_abm_last = 0;
			++calls;

			ScopeProfiler sp(g_profiler, "SEnv: ABM one block avg", SPT_AVG);

			v3POS p = i->first;

			/*infostream<<"Server: Block ("<<p.X<<","<<p.Y<<","<<p.Z
					<<") being handled"<<std::endl;*/

			MapBlock *block = m_map->getBlock(p, true);
			if (!block)
				continue;

			// Set current time as timestamp
			block->setTimestampNoChangedFlag(m_game_time);

			/* Handle ActiveBlockModifiers */
			block->abmTriggersRun(this, m_game_time);

			if (porting::getTimeMs() > end_ms) {
				m_active_block_abm_last = n;
				break;
			}
		}
		if (!calls)
			m_active_block_abm_last = 0;

/*
		if(m_active_block_abm_last) {
			infostream<<"WARNING: active block modifiers ("
					<<calls<<"/"<<m_active_blocks.m_list.size()<<" to "<<m_active_block_abm_last<<") took "
					<<porting::getTimeMs()-end_ms + u32(1000 * m_recommended_send_interval)<<"ms "
					<<std::endl;
		}
*/
		if (!m_active_block_abm_last) {
			m_active_block_abm_dtime = m_active_block_abm_dtime_counter;
			m_active_block_abm_dtime_counter = 0;
		}
	}

	/*
		Step script environment (run global on_step())
	*/
	{
	ScopeProfiler sp(g_profiler, "SEnv: environment_Step AVG", SPT_AVG);
	TimeTaker timer("environment_Step");
	m_script->environment_Step(dtime);
	}
	/*
		Step active objects
	*/
	{
		//ScopeProfiler sp(g_profiler, "SEnv: step act. objs avg", SPT_AVG);
		//TimeTaker timer("Step active objects");

	std::vector<ServerActiveObject*> objects;
	{
		auto lock = m_active_objects.try_lock_shared_rec();
		if (lock->owns_lock()) {
			for(auto & ir : m_active_objects) {
				objects.emplace_back(ir.second);
			}
		}
	}

	if (objects.size())
	{
		g_profiler->add("SEnv: Objects", objects.size());

		// This helps the objects to send data at the same time
		bool send_recommended = false;
		m_send_recommended_timer += dtime;
		if(m_send_recommended_timer > getSendRecommendedInterval())
		{
			m_send_recommended_timer -= getSendRecommendedInterval();
			if (m_send_recommended_timer > getSendRecommendedInterval() * 2) {
				m_send_recommended_timer = 0;
			}
			send_recommended = true;
		}
		u32 n = 0, calls = 0, end_ms = porting::getTimeMs() + max_cycle_ms;

		for(auto & obj : objects) {
			if (n++ < m_active_objects_last)
				continue;
			else
				m_active_objects_last = 0;
			++calls;

			// Don't step if is to be removed or stored statically
			if(obj->m_removed || obj->m_pending_deactivation)
				continue;
			// Step object
			if (!obj->m_uptime_last)  // not very good place, but minimum modifications
				obj->m_uptime_last = uptime - dtime;
			obj->step(uptime - obj->m_uptime_last, send_recommended);
			obj->m_uptime_last = uptime;
			// Read messages from object
/*
			while(!obj->m_messages_out.empty())
			{
				m_active_object_messages.push_back(
						obj->m_messages_out.front());
				obj->m_messages_out.pop();
			}
*/

			if (porting::getTimeMs() > end_ms) {
				m_active_objects_last = n;
				break;
			}
		}
		if (!calls)
			m_active_objects_last = 0;
	}
	}

	/*
		Manage active objects
	*/
	if(m_object_management_interval.step(dtime, 5))
	{
		//TimeTaker timer("Manage active objects");
		//ScopeProfiler sp(g_profiler, "SEnv: remove removed objs avg /.5s", SPT_AVG);
		/*
			Remove objects that satisfy (m_removed && m_known_by_count==0)
		*/
		removeRemovedObjects();
	}
}

int ServerEnvironment::analyzeBlocks(float dtime, unsigned int max_cycle_ms) {
	u32 n = 0, calls = 0, end_ms = porting::getTimeMs() + max_cycle_ms;
	if (m_active_block_analyzed_last || m_analyze_blocks_interval.step(dtime, 1.0)) {
		//if (!m_active_block_analyzed_last) infostream<<"Start ABM analyze cycle s="<<m_active_blocks.m_list.size()<<std::endl;
		TimeTaker timer("env: block analyze and abm apply from " + itos(m_active_block_analyzed_last));

		std::unordered_map<v3POS, bool, v3POSHash, v3POSEqual> active_blocks_list;
		//auto active_blocks_list = m_active_blocks.m_list;
		{
			auto lock = m_active_blocks.m_list.lock_shared_rec();
			active_blocks_list = m_active_blocks.m_list;
		}

		for(auto i = active_blocks_list.begin(); i != active_blocks_list.end(); ++i)
		{
			if (n++ < m_active_block_analyzed_last)
				continue;
			else
				m_active_block_analyzed_last = 0;
			++calls;

			v3POS p = i->first;

			MapBlock *block = m_map->getBlock(p, true);
			if(!block)
				continue;

			analyzeBlock(block);

			if (porting::getTimeMs() > end_ms) {
				m_active_block_analyzed_last = n;
				break;
			}
		}
		if (!calls)
			m_active_block_analyzed_last = 0;
	}


	if (g_settings->getBool("abm_random") && (!m_abm_random_blocks.empty() || m_abm_random_interval.step(dtime, 10.0))) {
		TimeTaker timer("env: random abm " + itos(m_abm_random_blocks.size()));

		u32 end_ms = porting::getTimeMs() + max_cycle_ms/10;

		if (m_abm_random_blocks.empty()) {
			auto lock = m_map->m_blocks.try_lock_shared_rec();
			for (auto ir : m_map->m_blocks) {
				if (!ir.second || !ir.second->abm_triggers)
					continue;
				m_abm_random_blocks.emplace_back(ir.first);
			}
			//infostream<<"Start ABM random cycle s="<<m_abm_random_blocks.size()<<std::endl;
		}

		for (auto i = m_abm_random_blocks.begin(); i != m_abm_random_blocks.end(); ++i) {
			MapBlock* block = m_map->getBlock(*i, true);
			i = m_abm_random_blocks.erase(i);
			//ScopeProfiler sp221(g_profiler, "ABM random look blocks", SPT_ADD);

			if (!block)
				continue;

			if (!block->abm_triggers)
				continue;
			//ScopeProfiler sp354(g_profiler, "ABM random trigger blocks", SPT_ADD);
			block->abmTriggersRun(this, m_game_time);
			if (porting::getTimeMs() > end_ms) {
				break;
			}
		}
	}

	return calls;
}

ServerActiveObject* ServerEnvironment::getActiveObject(u16 id)
{
	auto n = m_active_objects.find(id);
	if(n == m_active_objects.end())
		return NULL;
	return n->second;
}

bool isFreeServerActiveObjectId(u16 id,
		maybe_shared_map<u16, ServerActiveObject*> &objects)
{
	if(id == 0)
		return false;

	return objects.find(id) == objects.end();
}

u16 getFreeServerActiveObjectId(
		maybe_shared_map<u16, ServerActiveObject*> &objects)
{
	//try to reuse id's as late as possible
	static u16 last_used_id = 0;
	u16 startid = last_used_id;
	for(;;)
	{
		last_used_id ++;
		if(isFreeServerActiveObjectId(last_used_id, objects))
			return last_used_id;

		if(last_used_id == startid)
			return 0;
	}
}

u16 ServerEnvironment::addActiveObject(ServerActiveObject *object)
{
	assert(object);	// Pre-condition
	m_added_objects++;
	u16 id = addActiveObjectRaw(object, true, 0);
	return id;
}

#if 0
bool ServerEnvironment::addActiveObjectAsStatic(ServerActiveObject *obj)
{
	assert(obj);

	v3f objectpos = obj->getBasePosition();

	// The block in which the object resides in
	v3s16 blockpos_o = getNodeBlockPos(floatToInt(objectpos, BS));

	/*
		Update the static data
	*/

	// Create new static object
	std::string staticdata = obj->getStaticData();
	StaticObject s_obj(obj->getType(), objectpos, staticdata);
	// Add to the block where the object is located in
	v3s16 blockpos = getNodeBlockPos(floatToInt(objectpos, BS));
	// Get or generate the block
	MapBlock *block = m_map->emergeBlock(blockpos);

	bool succeeded = false;

	if(block)
	{
		block->m_static_objects.insert(0, s_obj);
		block->raiseModified(MOD_STATE_WRITE_AT_UNLOAD,
				"addActiveObjectAsStatic");
		succeeded = true;
	}
	else{
		infostream<<"ServerEnvironment::addActiveObjectAsStatic: "
				<<"Could not find or generate "
				<<"a block for storing static object"<<std::endl;
		succeeded = false;
	}

	if(obj->environmentDeletes())
		delete obj;

	return succeeded;
}
#endif

/*
	Finds out what new objects have been added to
	inside a radius around a position
*/
void ServerEnvironment::getAddedActiveObjects(v3s16 pos, s16 radius,
		s16 player_radius,
		maybe_shared_unordered_map<u16, bool> &current_objects_shared,
		std::set<u16> &added_objects)
{
	v3f pos_f = intToFloat(pos, BS);
	f32 radius_f = radius * BS;
	f32 player_radius_f = player_radius * BS;

	if (player_radius_f < 0)
		player_radius_f = 0;

	std::unordered_map<u16, bool> current_objects;
	{
		auto lock = current_objects_shared.lock_shared_rec();
		current_objects = current_objects_shared;
	}
	/*
		Go through the object list,
		- discard m_removed objects,
		- discard objects that are too far away,
		- discard objects that are found in current_objects.
		- add remaining objects to added_objects
	*/
	auto lock = m_active_objects.lock_shared_rec();
	for(auto
			i = m_active_objects.begin();
			i != m_active_objects.end(); ++i)
	{
		u16 id = i->first;
		// Get object
		ServerActiveObject *object = i->second;
		if(object == NULL)
			continue;
		// Discard if removed or deactivating
		if(object->m_removed || object->m_pending_deactivation)
			continue;

		f32 distance_f = object->getBasePosition().getDistanceFrom(pos_f);
		if (object->getType() == ACTIVEOBJECT_TYPE_PLAYER) {
			// Discard if too far
			if (distance_f > player_radius_f && player_radius_f != 0)
				continue;
		} else if (distance_f > radius_f)
			continue;

		// Discard if already on current_objects
		auto n = current_objects.find(id);
		if(n != current_objects.end())
			continue;
		// Add to added_objects
		added_objects.insert(id);
	}
}

/*
	Finds out what objects have been removed from
	inside a radius around a position
*/
void ServerEnvironment::getRemovedActiveObjects(v3s16 pos, s16 radius,
		s16 player_radius,
		maybe_shared_unordered_map<u16, bool> &current_objects,
		std::set<u16> &removed_objects)
{
	v3f pos_f = intToFloat(pos, BS);
	f32 radius_f = radius * BS;
	f32 player_radius_f = player_radius * BS;

	if (player_radius_f < 0)
		player_radius_f = 0;

	std::vector<u16> current_objects_vector;
	{
		auto lock = current_objects.try_lock_shared_rec();
		if (!lock->owns_lock())
			return;
		for (auto & i : current_objects)
			current_objects_vector.emplace_back(i.first);
	}
	/*
		Go through current_objects; object is removed if:
		- object is not found in m_active_objects (this is actually an
		  error condition; objects should be set m_removed=true and removed
		  only after all clients have been informed about removal), or
		- object has m_removed=true, or
		- object is too far away
	*/
	for(auto
			i = current_objects_vector.begin();
			i != current_objects_vector.end(); ++i)
	{
		u16 id = *i;
		ServerActiveObject *object = getActiveObject(id);

		if(object == NULL){
			infostream<<"ServerEnvironment::getRemovedActiveObjects():"
					<<" object in current_objects is NULL"<<std::endl;
			removed_objects.insert(id);
			continue;
		}

		if(object->m_removed || object->m_pending_deactivation)
		{
			removed_objects.insert(id);
			continue;
		}

		f32 distance_f = object->getBasePosition().getDistanceFrom(pos_f);
		if (object->getType() == ACTIVEOBJECT_TYPE_PLAYER) {
			if (distance_f <= player_radius_f || player_radius_f == 0)
				continue;
		} else if (distance_f <= radius_f)
			continue;

		// Object is no longer visible
		removed_objects.insert(id);
	}
}

ActiveObjectMessage ServerEnvironment::getActiveObjectMessage()
{
	if(m_active_object_messages.empty())
		return ActiveObjectMessage(0);

	return m_active_object_messages.pop_front();
}

/*
	************ Private methods *************
*/

u16 ServerEnvironment::addActiveObjectRaw(ServerActiveObject *object,
		bool set_changed, u32 dtime_s)
{
	if(!object)
		return 0;
	if(object->getId() == 0){
		u16 new_id = getFreeServerActiveObjectId(m_active_objects);
		if(new_id == 0)
		{
			errorstream<<"ServerEnvironment::addActiveObjectRaw(): "
					<<"no free ids available"<<std::endl;
			if(object->environmentDeletes())
				delete object;
			return 0;
		}
		object->setId(new_id);
	}
	else{
		verbosestream<<"ServerEnvironment::addActiveObjectRaw(): "
				<<"supplied with id "<<object->getId()<<std::endl;
	}
	if(isFreeServerActiveObjectId(object->getId(), m_active_objects) == false)
	{
		errorstream<<"ServerEnvironment::addActiveObjectRaw(): "
				<<"id is not free ("<<object->getId()<<")"<<std::endl;
		if(object->environmentDeletes())
			delete object;
		return 0;
	}
	/*infostream<<"ServerEnvironment::addActiveObjectRaw(): "
			<<"added (id="<<object->getId()<<")"<<std::endl;*/

	m_active_objects.set(object->getId(), object);

/*
	m_active_objects[object->getId()] = object;

	verbosestream<<"ServerEnvironment::addActiveObjectRaw(): "
			<<"Added id="<<object->getId()<<"; there are now "
			<<m_active_objects.size()<<" active objects."
			<<std::endl;
*/

	// Register reference in scripting api (must be done before post-init)
	m_script->addObjectReference(object);
	// Post-initialize object
	object->addedToEnvironment(dtime_s);

	// Add static data to block
	if(object->isStaticAllowed())
	{
		// Add static object to active static list of the block
		v3f objectpos = object->getBasePosition();
		std::string staticdata = object->getStaticData();
		StaticObject s_obj(object->getType(), objectpos, staticdata);
		// Add to the block where the object is located in
		v3s16 blockpos = getNodeBlockPos(floatToInt(objectpos, BS));
		MapBlock *block = m_map->emergeBlock(blockpos);
		if(block){
			block->m_static_objects.m_active.set(object->getId(), s_obj);
			object->m_static_exists = true;
			object->m_static_block = blockpos;

			if(set_changed)
				block->raiseModified(MOD_STATE_WRITE_NEEDED,
						"addActiveObjectRaw");
		} else {
			v3s16 p = floatToInt(objectpos, BS);
			errorstream<<"ServerEnvironment::addActiveObjectRaw(): "
					<<"could not emerge block for storing id="<<object->getId()
					<<" statically (pos="<<PP(p)<<")"<<std::endl;
		}
	}

	return object->getId();
}

/*
	Remove objects that satisfy (m_removed && m_known_by_count==0)
*/
void ServerEnvironment::removeRemovedObjects()
{
	TimeTaker timer("ServerEnvironment::removeRemovedObjects()");
	std::list<u16> objects_to_remove;

	std::vector<ServerActiveObject*> objects;
	{
		auto lock = m_active_objects.try_lock_shared_rec();
		if (lock->owns_lock()) {
			for(auto & ir : m_active_objects) {
				auto obj = ir.second;
				if (obj) {
					objects.emplace_back(obj);
				} else {
					auto id = ir.first;
					objects_to_remove.push_back(id);
				}
			}
		}
	}

	if (objects.size())
	for (auto & obj : objects)
	{
		if(!obj)
			continue;

		u16 id = obj->getId();

		/*
			We will delete objects that are marked as removed or thatare
			waiting for deletion after deactivation
		*/
		if(obj->m_removed == false && obj->m_pending_deactivation == false)
			continue;

		/*
			Delete static data from block if is marked as removed
		*/
		if(obj->m_static_exists && obj->m_removed)
		{
			MapBlock *block = m_map->emergeBlock(obj->m_static_block, false);
			if (block) {
				block->m_static_objects.remove(id);
				block->raiseModified(MOD_STATE_WRITE_NEEDED,
						"removeRemovedObjects/remove");
				obj->m_static_exists = false;
			} else {
				infostream<<"Failed to emerge block from which an object to "
						<<"be removed was loaded from. id="<<id<<std::endl;
			}
		}

		// If m_known_by_count > 0, don't actually remove. On some future
		// invocation this will be 0, which is when removal will continue.
		if(obj->m_known_by_count > 0)
			continue;

		/*
			Move static data from active to stored if not marked as removed
		*/
		if(obj->m_static_exists && !obj->m_removed){
			MapBlock *block = m_map->emergeBlock(obj->m_static_block, false);
			if (block) {
				std::map<u16, StaticObject>::iterator i =
						block->m_static_objects.m_active.find(id);
				if(i != block->m_static_objects.m_active.end()){
					block->m_static_objects.m_stored.push_back(i->second);
					block->m_static_objects.m_active.erase(id);
					block->raiseModified(MOD_STATE_WRITE_NEEDED,
							"removeRemovedObjects/deactivate");
				}
			} else {
				infostream<<"Failed to emerge block from which an object to "
						<<"be deactivated was loaded from. id="<<id<<std::endl;
			}
		}

		// Tell the object about removal
		obj->removingFromEnvironment();
		// Deregister in scripting api
		m_script->removeObjectReference(obj);

		// Delete
		if(obj->environmentDeletes()) {
			m_active_objects.set(id, nullptr);
			delete obj;
		}

		// Id to be removed from m_active_objects
		objects_to_remove.push_back(id);
	}

	if (!objects_to_remove.empty()) {
	auto lock = m_active_objects.lock_unique_rec();
	// Remove references from m_active_objects
	for(auto i = objects_to_remove.begin();
			i != objects_to_remove.end(); ++i) {
		m_active_objects.erase(*i);
	}
	}
}

static void print_hexdump(std::ostream &o, const std::string &data)
{
	const int linelength = 16;
	for(int l=0; ; l++){
		int i0 = linelength * l;
		bool at_end = false;
		int thislinelength = linelength;
		if(i0 + thislinelength > (int)data.size()){
			thislinelength = data.size() - i0;
			at_end = true;
		}
		for(int di=0; di<linelength; di++){
			int i = i0 + di;
			char buf[4];
			if(di<thislinelength)
				snprintf(buf, 4, "%.2x ", data[i]);
			else
				snprintf(buf, 4, "   ");
			o<<buf;
		}
		o<<" ";
		for(int di=0; di<thislinelength; di++){
			int i = i0 + di;
			if(data[i] >= 32)
				o<<data[i];
			else
				o<<".";
		}
		o<<std::endl;
		if(at_end)
			break;
	}
}

/*
	Convert stored objects from blocks near the players to active.
*/
void ServerEnvironment::activateObjects(MapBlock *block, u32 dtime_s)
{
	if(block == NULL)
		return;

	// Ignore if no stored objects (to not set changed flag)
	if(block->m_static_objects.m_stored.empty())
		return;
/*
	verbosestream<<"ServerEnvironment::activateObjects(): "
			<<"activating objects of block "<<PP(block->getPos())
			<<" ("<<block->m_static_objects.m_stored.size()
			<<" objects)"<<std::endl;
*/
	bool large_amount = (block->m_static_objects.m_stored.size() > g_settings->getU16("max_objects_per_block"));
	if (large_amount) {
		errorstream<<"suspiciously large amount of objects detected: "
				<<block->m_static_objects.m_stored.size()<<" in "
				<<PP(block->getPos())
				<<"; removing all of them."<<std::endl;
		// Clear stored list
		block->m_static_objects.m_stored.clear();
		block->raiseModified(MOD_STATE_WRITE_NEEDED,
				"stored list cleared in activateObjects due to "
				"large amount of objects");
		return;
	}

	// Activate stored objects
	std::vector<StaticObject> new_stored;
	for (std::vector<StaticObject>::iterator
			i = block->m_static_objects.m_stored.begin();
			i != block->m_static_objects.m_stored.end(); ++i) {
		StaticObject &s_obj = *i;

		// Create an active object from the data
		ServerActiveObject *obj = ServerActiveObject::create
				((ActiveObjectType) s_obj.type, this, 0, s_obj.pos, s_obj.data);
		// If couldn't create object, store static data back.
		if(obj == NULL) {
			errorstream<<"ServerEnvironment::activateObjects(): "
					<<"failed to create active object from static object "
					<<"in block "<<PP(s_obj.pos/BS)
					<<" type="<<(int)s_obj.type<<" data:"<<std::endl;
			print_hexdump(verbosestream, s_obj.data);

			new_stored.push_back(s_obj);
			continue;
		}
/*
		verbosestream<<"ServerEnvironment::activateObjects(): "
				<<"activated static object pos="<<PP(s_obj.pos/BS)
				<<" type="<<(int)s_obj.type<<std::endl;
*/
		// This will also add the object to the active static list
		addActiveObjectRaw(obj, false, dtime_s);
	}
	// Clear stored list
	block->m_static_objects.m_stored.clear();
	// Add leftover failed stuff to stored list
	for(std::vector<StaticObject>::iterator
			i = new_stored.begin();
			i != new_stored.end(); ++i) {
		StaticObject &s_obj = *i;
		block->m_static_objects.m_stored.push_back(s_obj);
	}

	// Turn the active counterparts of activated objects not pending for
	// deactivation
	for(std::map<u16, StaticObject>::iterator
			i = block->m_static_objects.m_active.begin();
			i != block->m_static_objects.m_active.end(); ++i)
	{
		u16 id = i->first;
		ServerActiveObject *object = getActiveObject(id);
		if (!object)
			continue;
		object->m_pending_deactivation = false;
	}

	/*
		Note: Block hasn't really been modified here.
		The objects have just been activated and moved from the stored
		static list to the active static list.
		As such, the block is essentially the same.
		Thus, do not call block->raiseModified(MOD_STATE_WRITE_NEEDED).
		Otherwise there would be a huge amount of unnecessary I/O.
	*/
}

/*
	Convert objects that are not standing inside active blocks to static.

	If m_known_by_count != 0, active object is not deleted, but static
	data is still updated.

	If force_delete is set, active object is deleted nevertheless. It
	shall only be set so in the destructor of the environment.

	If block wasn't generated (not in memory or on disk),
*/
void ServerEnvironment::deactivateFarObjects(bool force_delete)
{
	//ScopeProfiler sp(g_profiler, "SEnv: deactivateFarObjects");

	std::vector<u16> objects_to_remove;

	std::vector<ServerActiveObject*> objects;
	{
		auto lock = m_active_objects.try_lock_shared_rec();
		if (lock->owns_lock()) {
			for(auto & ir : m_active_objects) {
				auto obj = ir.second;
				if (obj) {
					objects.emplace_back(obj);
				} else {
					auto id = ir.first;
					objects_to_remove.push_back(id);
				}
			}
		}
	}

	if (objects.size())
	for (auto & obj : objects)
	{
		if (!obj)
			continue;

		// Do not deactivate if static data creation not allowed
		if(!force_delete && !obj->isStaticAllowed())
			continue;

		// If pending deactivation, let removeRemovedObjects() do it
		if(!force_delete && obj->m_pending_deactivation)
			continue;

		u16 id = obj->getId();
		v3f objectpos = obj->getBasePosition();

		// The block in which the object resides in
		v3s16 blockpos_o = getNodeBlockPos(floatToInt(objectpos, BS));

		// If object's static data is stored in a deactivated block and object
		// is actually located in an active block, re-save to the block in
		// which the object is actually located in.
		if(!force_delete &&
				obj->m_static_exists &&
				!m_active_blocks.contains(obj->m_static_block) &&
				 m_active_blocks.contains(blockpos_o))
		{
			v3s16 old_static_block = obj->m_static_block;

			// Save to block where object is located
			MapBlock *block = m_map->emergeBlock(blockpos_o, false);
			if(!block){
				errorstream<<"ServerEnvironment::deactivateFarObjects(): "
						<<"Could not save object id="<<id
						<<" to it's current block "<<PP(blockpos_o)
						<<std::endl;
				continue;
			}
			std::string staticdata_new = obj->getStaticData();
			StaticObject s_obj(obj->getType(), objectpos, staticdata_new);
			block->m_static_objects.insert(id, s_obj);
			obj->m_static_block = blockpos_o;
			block->raiseModified(MOD_STATE_WRITE_NEEDED,
					"deactivateFarObjects: Static data moved in");

			// Delete from block where object was located
			block = m_map->emergeBlock(old_static_block, false);
			if(!block){
				errorstream<<"ServerEnvironment::deactivateFarObjects(): "
						<<"Could not delete object id="<<id
						<<" from it's previous block "<<PP(old_static_block)
						<<std::endl;
				continue;
			}
			block->m_static_objects.remove(id);
			block->raiseModified(MOD_STATE_WRITE_NEEDED,
					"deactivateFarObjects: Static data moved out");
			continue;
		}

		// If block is active, don't remove
		if(!force_delete && m_active_blocks.contains(blockpos_o))
			continue;

/*
		verbosestream<<"ServerEnvironment::deactivateFarObjects(): "
				<<"deactivating object id="<<id<<" on inactive block "
				<<PP(blockpos_o)<<std::endl;
*/

		// If known by some client, don't immediately delete.
		bool pending_delete = (obj->m_known_by_count > 0 && !force_delete);

		/*
			Update the static data
		*/

		if(obj->isStaticAllowed())
		{
			// Create new static object
			std::string staticdata_new = obj->getStaticData();
			StaticObject s_obj(obj->getType(), objectpos, staticdata_new);

			bool stays_in_same_block = false;
			bool data_changed = true;

			if(obj->m_static_exists){
				if(obj->m_static_block == blockpos_o)
					stays_in_same_block = true;

				MapBlock *block = m_map->emergeBlock(obj->m_static_block, false);
				if (!block)
					continue;

				std::map<u16, StaticObject>::iterator n =
						block->m_static_objects.m_active.find(id);
				if(n != block->m_static_objects.m_active.end()){
					StaticObject static_old = n->second;

					float save_movem = obj->getMinimumSavedMovement();

					if(static_old.data == staticdata_new &&
							(static_old.pos - objectpos).getLength() < save_movem)
						data_changed = false;
				} else {
					infostream<<"ServerEnvironment::deactivateFarObjects(): "
							<<"id="<<id<<" m_static_exists=true but "
							<<"static data doesn't actually exist in "
							<<PP(obj->m_static_block)<<std::endl;
				}
			}

			bool shall_be_written = (!stays_in_same_block || data_changed);

			// Delete old static object
			if(obj->m_static_exists)
			{
				MapBlock *block = m_map->emergeBlock(obj->m_static_block, false);
				if(block)
				{
					block->m_static_objects.remove(id);
					obj->m_static_exists = false;
					// Only mark block as modified if data changed considerably
					if(shall_be_written)
						block->raiseModified(MOD_STATE_WRITE_NEEDED,
								"deactivateFarObjects: Static data "
								"changed considerably");
				}
			}

			// Add to the block where the object is located in
			v3s16 blockpos = getNodeBlockPos(floatToInt(objectpos, BS));
			// Get or generate the block
			MapBlock *block = NULL;
			try{
				block = m_map->emergeBlock(blockpos);
			} catch(InvalidPositionException &e){
				// Handled via NULL pointer
				// NOTE: emergeBlock's failure is usually determined by it
				//       actually returning NULL
			}

			if(block)
			{
				if(block->m_static_objects.m_stored.size() >= g_settings->getU16("max_objects_per_block")){
					infostream<<"ServerEnv: Trying to store id="<<obj->getId()
							<<" statically but block "<<PP(blockpos)
							<<" already contains "
							<<block->m_static_objects.m_stored.size()
							<<" objects."
							<<" Forcing delete."<<std::endl;
					force_delete = true;
				} else {
					// If static counterpart already exists in target block,
					// remove it first.
					// This shouldn't happen because the object is removed from
					// the previous block before this according to
					// obj->m_static_block, but happens rarely for some unknown
					// reason. Unsuccessful attempts have been made to find
					// said reason.
					if(id && block->m_static_objects.m_active.find(id) != block->m_static_objects.m_active.end()){
						infostream<<"ServerEnv: WARNING: Performing hack #83274"
								<<std::endl;
						block->m_static_objects.remove(id);
					}
					// Store static data
					u16 store_id = pending_delete ? id : 0;
					block->m_static_objects.insert(store_id, s_obj);

					// Only mark block as modified if data changed considerably
					if(shall_be_written)
						block->raiseModified(MOD_STATE_WRITE_NEEDED,
								"deactivateFarObjects: Static data "
								"changed considerably");

					obj->m_static_exists = true;
					obj->m_static_block = block->getPos();
				}
			}
			else{
				if(!force_delete){
					v3s16 p = floatToInt(objectpos, BS);
					errorstream<<"ServerEnv: Could not find or generate "
							<<"a block for storing id="<<obj->getId()
							<<" statically (pos="<<PP(p)<<")"<<std::endl;
					continue;
				}
			}
		}

		/*
			If known by some client, set pending deactivation.
			Otherwise delete it immediately.
		*/

		if(pending_delete && !force_delete)
		{
			verbosestream<<"ServerEnvironment::deactivateFarObjects(): "
					<<"object id="<<id<<" is known by clients"
					<<"; not deleting yet"<<std::endl;

			obj->m_pending_deactivation = true;
			continue;
		}

/*
		verbosestream<<"ServerEnvironment::deactivateFarObjects(): "
				<<"object id="<<id<<" is not known by clients"
				<<"; deleting"<<std::endl;
*/

		// Tell the object about removal
		obj->removingFromEnvironment();
		// Deregister in scripting api
		m_script->removeObjectReference(obj);

		// Delete active object
		if(obj->environmentDeletes())
		{
			m_active_objects.set(id, nullptr);
			delete obj;
		}
		// Id to be removed from m_active_objects
		objects_to_remove.push_back(id);
	}

	//if(m_active_objects.size()) verbosestream<<"ServerEnvironment::deactivateFarObjects(): deactivated="<<objects_to_remove.size()<< " from="<<m_active_objects.size()<<std::endl;

	if (!objects_to_remove.empty()) {
	auto lock = m_active_objects.lock_unique_rec();
	// Remove references from m_active_objects
	for(std::vector<u16>::iterator i = objects_to_remove.begin();
			i != objects_to_remove.end(); ++i) {
		m_active_objects.erase(*i);
	}
	}
}


#ifndef SERVER

#include "clientsimpleobject.h"

/*
	ClientEnvironment
*/

ClientEnvironment::ClientEnvironment(ClientMap *map, scene::ISceneManager *smgr,
		ITextureSource *texturesource, IGameDef *gamedef,
		IrrlichtDevice *irr):
	m_map(map),
	m_smgr(smgr),
	m_texturesource(texturesource),
	m_gamedef(gamedef),
	m_irr(irr)
	,m_active_objects_client_last(0),
	m_move_max_loop(10)
{
	char zero = 0;
	memset(m_attachements, zero, sizeof(m_attachements));
}

ClientEnvironment::~ClientEnvironment()
{
	// delete active objects
	for(std::map<u16, ClientActiveObject*>::iterator
			i = m_active_objects.begin();
			i != m_active_objects.end(); ++i)
	{
		delete i->second;
	}

	for(std::vector<ClientSimpleObject*>::iterator
			i = m_simple_objects.begin(); i != m_simple_objects.end(); ++i) {
		delete *i;
	}

	// Drop/delete map
	m_map->drop();
}

Map & ClientEnvironment::getMap()
{
	return *m_map;
}

ClientMap & ClientEnvironment::getClientMap()
{
	return *m_map;
}

void ClientEnvironment::addPlayer(Player *player)
{
	DSTACK(__FUNCTION_NAME);
	/*
		It is a failure if player is local and there already is a local
		player
	*/
	FATAL_ERROR_IF(player->isLocal() == true && getLocalPlayer() != NULL,
		"Player is local but there is already a local player");

	Environment::addPlayer(player);
}

LocalPlayer * ClientEnvironment::getLocalPlayer()
{
	for(std::vector<Player*>::iterator i = m_players.begin();
			i != m_players.end(); ++i) {
		Player *player = *i;
		if(player->isLocal())
			return (LocalPlayer*)player;
	}
	return NULL;
}

void ClientEnvironment::step(float dtime, float uptime, unsigned int max_cycle_ms)
{
	DSTACK(__FUNCTION_NAME);

	/* Step time of day */
	stepTimeOfDay(dtime);

	// Get some settings
	bool fly_allowed = m_gamedef->checkLocalPrivilege("fly");
	bool free_move = fly_allowed && g_settings->getBool("free_move");

	// Get local player
	LocalPlayer *lplayer = getLocalPlayer();
	assert(lplayer);
	// collision info queue
	std::vector<CollisionInfo> player_collisions;

	/*
		Get the speed the player is going
	*/
	bool is_climbing = lplayer->is_climbing;

	f32 player_speed = lplayer->getSpeed().getLength();
	v3f pf = lplayer->getPosition();

	/*
		Maximum position increment
	*/
	//f32 position_max_increment = 0.05*BS;
	f32 position_max_increment = 0.1*BS;

	// Maximum time increment (for collision detection etc)
	// time = distance / speed
	f32 dtime_max_increment = 1;
	if(player_speed > 0.001)
		dtime_max_increment = position_max_increment / player_speed;

	// Maximum time increment is 10ms or lower
	if(dtime_max_increment > 0.01)
		dtime_max_increment = 0.01;

	if(dtime_max_increment*m_move_max_loop < dtime)
		dtime_max_increment = dtime/m_move_max_loop;

	// Don't allow overly huge dtime
	if(dtime > 2)
		dtime = 2;

	f32 dtime_downcount = dtime;

	/*
		Stuff that has a maximum time increment
	*/

	u32 loopcount = 0;
	u32 breaked = 0, lend_ms = porting::getTimeMs() + max_cycle_ms;
	do
	{
		loopcount++;

		f32 dtime_part;
		if(dtime_downcount > dtime_max_increment)
		{
			dtime_part = dtime_max_increment;
			dtime_downcount -= dtime_part;
		}
		else
		{
			dtime_part = dtime_downcount;
			/*
				Setting this to 0 (no -=dtime_part) disables an infinite loop
				when dtime_part is so small that dtime_downcount -= dtime_part
				does nothing
			*/
			dtime_downcount = 0;
		}

		/*
			Handle local player
		*/

		{
			// Apply physics
			if(free_move == false && is_climbing == false)
			{
				f32 viscosity_factor = 0;
				// Gravity
				v3f speed = lplayer->getSpeed();
				if(lplayer->in_liquid == false) {
					speed.Y -= lplayer->movement_gravity * lplayer->physics_override_gravity * dtime_part * 2;
					viscosity_factor = 0.97; // todo maybe depend on speed; 0.96 = ~100 nps max
					viscosity_factor += (1.0-viscosity_factor) *
						(1-(MAP_GENERATION_LIMIT - pf.Y/BS)/
							MAP_GENERATION_LIMIT);
				}

				// Liquid floating / sinking
				if(lplayer->in_liquid && !lplayer->swimming_vertical)
					speed.Y -= lplayer->movement_liquid_sink * dtime_part * 2;

				if(lplayer->in_liquid_stable || lplayer->in_liquid)
				{
					viscosity_factor = 0.3; // todo: must depend on speed^2
				}
				// Liquid resistance
				if(viscosity_factor)
				{
					// How much the node's viscosity blocks movement, ranges between 0 and 1
					// Should match the scale at which viscosity increase affects other liquid attributes

					v3f d_wanted = -speed / lplayer->movement_liquid_fluidity;
					f32 dl = d_wanted.getLength();
					if(dl > lplayer->movement_liquid_fluidity_smooth)
						dl = lplayer->movement_liquid_fluidity_smooth;
					if (lplayer->liquid_viscosity < 1) //rewrite this shit
						dl /= 2;
					dl *= (lplayer->liquid_viscosity * viscosity_factor) + (1 - viscosity_factor);

					v3f d = d_wanted.normalize() * dl;
					speed += d;

#if 0 // old code
					if(speed.X > lplayer->movement_liquid_fluidity + lplayer->movement_liquid_fluidity_smooth)	speed.X -= lplayer->movement_liquid_fluidity_smooth;
					if(speed.X < -lplayer->movement_liquid_fluidity - lplayer->movement_liquid_fluidity_smooth)	speed.X += lplayer->movement_liquid_fluidity_smooth;
					if(speed.Y > lplayer->movement_liquid_fluidity + lplayer->movement_liquid_fluidity_smooth)	speed.Y -= lplayer->movement_liquid_fluidity_smooth;
					if(speed.Y < -lplayer->movement_liquid_fluidity - lplayer->movement_liquid_fluidity_smooth)	speed.Y += lplayer->movement_liquid_fluidity_smooth;
					if(speed.Z > lplayer->movement_liquid_fluidity + lplayer->movement_liquid_fluidity_smooth)	speed.Z -= lplayer->movement_liquid_fluidity_smooth;
					if(speed.Z < -lplayer->movement_liquid_fluidity - lplayer->movement_liquid_fluidity_smooth)	speed.Z += lplayer->movement_liquid_fluidity_smooth;
#endif
				}

				lplayer->setSpeed(speed);
			}

			/*
				Move the lplayer.
				This also does collision detection.
			*/
			lplayer->move(dtime_part, this, position_max_increment,
					&player_collisions);
		}
		if (porting::getTimeMs() >= lend_ms) {
			breaked = loopcount;
			break;
		}

	}
	while(dtime_downcount > 0.001);

	//infostream<<"loop "<<loopcount<<"/"<<m_move_max_loop<<" breaked="<<breaked<<std::endl;

	if (breaked && m_move_max_loop > loopcount)
		--m_move_max_loop;
	if (!breaked && m_move_max_loop < 50)
		++m_move_max_loop;

	for(auto
			i = player_collisions.begin();
			i != player_collisions.end(); ++i)
	{
		CollisionInfo &info = *i;
		v3f speed_diff = info.new_speed - info.old_speed;
		// Handle only fall damage
		// (because otherwise walking against something in fast_move kills you)
		if((speed_diff.Y < 0 || info.old_speed.Y >= 0) &&
			speed_diff.getLength() <= lplayer->movement_speed_fast * 1.1) {
			continue;
		}
		f32 pre_factor = 1; // 1 hp per node/s
		f32 tolerance = PLAYER_FALL_TOLERANCE_SPEED; // 5 without damage
		f32 post_factor = 1; // 1 hp per node/s
		if(info.type == COLLISION_NODE)
		{
			const ContentFeatures &f = m_gamedef->ndef()->
					get(m_map->getNodeNoEx(info.node_p));
			// Determine fall damage multiplier
			int addp = itemgroup_get(f.groups, "fall_damage_add_percent");
			pre_factor = 1.0 + (float)addp/100.0;
		}
		float speed = pre_factor * speed_diff.getLength();
		if(speed > tolerance)
		{
			f32 damage_f = (speed - tolerance)/BS * post_factor;
			u16 damage = (u16)(damage_f+0.5);
			if(damage != 0){
				damageLocalPlayer(damage, true);
				MtEvent *e = new SimpleTriggerEvent("PlayerFallingDamage");
				m_gamedef->event()->put(e);
			}
		}
	}

	/*
		A quick draft of lava damage
	*/
	if(m_lava_hurt_interval.step(dtime, 1.0))
	{
		v3f pf = lplayer->getPosition();

		// Feet, middle and head
		v3s16 p1 = floatToInt(pf + v3f(0, BS*0.1, 0), BS);
		MapNode n1 = m_map->getNodeNoEx(p1);
		v3s16 p2 = floatToInt(pf + v3f(0, BS*0.8, 0), BS);
		MapNode n2 = m_map->getNodeNoEx(p2);
		v3s16 p3 = floatToInt(pf + v3f(0, BS*1.6, 0), BS);
		MapNode n3 = m_map->getNodeNoEx(p3);

		u32 damage_per_second = 0;
		damage_per_second = MYMAX(damage_per_second,
				m_gamedef->ndef()->get(n1).damage_per_second);
		damage_per_second = MYMAX(damage_per_second,
				m_gamedef->ndef()->get(n2).damage_per_second);
		damage_per_second = MYMAX(damage_per_second,
				m_gamedef->ndef()->get(n3).damage_per_second);

		if(damage_per_second != 0)
		{
			damageLocalPlayer(damage_per_second, true);
		}
	}

	/*
		Drowning
	*/
	if(m_drowning_interval.step(dtime, 2.0))
	{
		//v3f pf = lplayer->getPosition();

		// head
		v3s16 p = floatToInt(pf + v3f(0, BS*1.6, 0), BS);
		MapNode n = m_map->getNodeNoEx(p);
		ContentFeatures c = m_gamedef->ndef()->get(n);
		u8 drowning_damage = c.drowning;
		if(drowning_damage > 0 && lplayer->hp > 0){
			u16 breath = lplayer->getBreath();
			if(breath > 10){
				breath = 11;
			}
			if(breath > 0){
				breath -= 1;
			}
			lplayer->setBreath(breath);
			updateLocalPlayerBreath(breath);
		}

		if(lplayer->getBreath() == 0 && drowning_damage > 0){
			damageLocalPlayer(drowning_damage, true);
		}
	}
	if(m_breathing_interval.step(dtime, 0.5))
	{
		v3f pf = lplayer->getPosition();

		// head
		v3s16 p = floatToInt(pf + v3f(0, BS*1.6, 0), BS);
		MapNode n = m_map->getNodeNoEx(p);
		ContentFeatures c = m_gamedef->ndef()->get(n);
		if (!lplayer->hp){
			lplayer->setBreath(11);
		}
		else if(c.drowning == 0){
			u16 breath = lplayer->getBreath();
			if(breath <= 10){
				breath += 1;
				lplayer->setBreath(breath);
				updateLocalPlayerBreath(breath);
			}
		}
	}

	/*
		Stuff that can be done in an arbitarily large dtime
	*/
	for(std::vector<Player*>::iterator i = m_players.begin();
			i != m_players.end(); ++i) {
		Player *player = *i;

		/*
			Handle non-local players
		*/
		if(player->isLocal() == false) {
			// Move
			player->move(dtime, this, 100*BS);

		}
	}

	// Update lighting on local player (used for wield item)
	u32 day_night_ratio = getDayNightRatio();
	{
		// Get node at head
		//float player_light = 1.0;

		// On InvalidPositionException, use this as default
		// (day: LIGHT_SUN, night: 0)
		MapNode node_at_lplayer(CONTENT_AIR, 0x0f, 0);

		v3s16 p = lplayer->getLightPosition();
		node_at_lplayer = m_map->getNodeNoEx(p);
		//player_light = blend_light_f1((float)getDayNightRatio()/1000, LIGHT_SUN, 0);

		u16 light = getInteriorLight(node_at_lplayer, 0, m_gamedef->ndef());
		u8 day = light & 0xff;
		u8 night = (light >> 8) & 0xff;
		finalColorBlend(lplayer->light_color, day, night, day_night_ratio);

		//lplayer->light = node_at_lplayer.getLightBlendF1((float)getDayNightRatio()/1000, m_gamedef->ndef());

	}

	/*
		Step active objects and update lighting of them
	*/

	g_profiler->avg("CEnv: num of objects", m_active_objects.size());
	bool update_lighting = m_active_object_light_update_interval.step(dtime, 0.21);
	u32 n = 0, calls = 0, end_ms = porting::getTimeMs() + u32(500/g_settings->getFloat("wanted_fps"));
	for(std::map<u16, ClientActiveObject*>::iterator
			i = m_active_objects.begin();
			i != m_active_objects.end(); ++i)
	{

		if (n++ < m_active_objects_client_last)
			continue;
		else
			m_active_objects_client_last = 0;
		++calls;

		ClientActiveObject* obj = i->second;
		// Step object
		obj->step(dtime, this);

		if(update_lighting)
		{
			// Update lighting
			u8 light = 0;
			bool pos_ok;

			// Get node at head
			v3s16 p = obj->getLightPosition();
			MapNode n = m_map->getNodeNoEx(p, &pos_ok);
			if (pos_ok)
				light = n.getLightBlend(day_night_ratio, m_gamedef->ndef());
			else
				light = blend_light(day_night_ratio, LIGHT_SUN, 0);

			obj->updateLight(light);
		}
		if (porting::getTimeMs() > end_ms) {
			m_active_objects_client_last = n;
			break;
		}
	}
	if (!calls)
		m_active_objects_client_last = 0;

	/*
		Step and handle simple objects
	*/

	g_profiler->avg("CEnv: num of simple objects", m_simple_objects.size());
	for(std::vector<ClientSimpleObject*>::iterator
			i = m_simple_objects.begin(); i != m_simple_objects.end();) {
		std::vector<ClientSimpleObject*>::iterator cur = i;
		ClientSimpleObject *simple = *cur;

		simple->step(dtime);
		if(simple->m_to_be_removed) {
			delete simple;
			i = m_simple_objects.erase(cur);
		}
		else {
			++i;
		}
	}
}

void ClientEnvironment::addSimpleObject(ClientSimpleObject *simple)
{
	m_simple_objects.push_back(simple);
}

ClientActiveObject* ClientEnvironment::getActiveObject(u16 id)
{
	std::map<u16, ClientActiveObject*>::iterator n;
	n = m_active_objects.find(id);
	if(n == m_active_objects.end())
		return NULL;
	return n->second;
}

bool isFreeClientActiveObjectId(u16 id,
		std::map<u16, ClientActiveObject*> &objects)
{
	if(id == 0)
		return false;

	return objects.find(id) == objects.end();
}

u16 getFreeClientActiveObjectId(
		std::map<u16, ClientActiveObject*> &objects)
{
	//try to reuse id's as late as possible
	static u16 last_used_id = 0;
	u16 startid = last_used_id;
	for(;;)
	{
		last_used_id ++;
		if(isFreeClientActiveObjectId(last_used_id, objects))
			return last_used_id;

		if(last_used_id == startid)
			return 0;
	}
}

u16 ClientEnvironment::addActiveObject(ClientActiveObject *object)
{
	if (!object)
		return 0;
	if(object->getId() == 0)
	{
		u16 new_id = getFreeClientActiveObjectId(m_active_objects);
		if(new_id == 0)
		{
			infostream<<"ClientEnvironment::addActiveObject(): "
					<<"no free ids available"<<std::endl;
			delete object;
			return 0;
		}
		object->setId(new_id);
	}
	if(isFreeClientActiveObjectId(object->getId(), m_active_objects) == false)
	{
		infostream<<"ClientEnvironment::addActiveObject(): "
				<<"id is not free ("<<object->getId()<<")"<<std::endl;
		delete object;
		return 0;
	}
/*
	infostream<<"ClientEnvironment::addActiveObject(): "
			<<"added (id="<<object->getId()<<")"<<std::endl;
*/
	m_active_objects[object->getId()] = object;
	object->addToScene(m_smgr, m_texturesource, m_irr);
	{ // Update lighting immediately
		u8 light = 0;
		bool pos_ok;

		// Get node at head
		v3s16 p = object->getLightPosition();
		MapNode n = m_map->getNodeNoEx(p, &pos_ok);
		if (pos_ok)
			light = n.getLightBlend(getDayNightRatio(), m_gamedef->ndef());
		else
			light = blend_light(getDayNightRatio(), LIGHT_SUN, 0);

		object->updateLight(light);
	}
	return object->getId();
}

void ClientEnvironment::addActiveObject(u16 id, u8 type,
		const std::string &init_data)
{
	ClientActiveObject* obj =
			ClientActiveObject::create((ActiveObjectType) type, m_gamedef, this);
	if(obj == NULL)
	{
		infostream<<"ClientEnvironment::addActiveObject(): "
				<<"id="<<id<<" type="<<type<<": Couldn't create object"
				<<std::endl;
		return;
	}

	obj->setId(id);

	try
	{
		obj->initialize(init_data);
	}
	catch(SerializationError &e)
	{
		errorstream<<"ClientEnvironment::addActiveObject():"
				<<" id="<<id<<" type="<<type
				<<": SerializationError in initialize(): "
				<<e.what()
				<<": init_data="<<serializeJsonString(init_data)
				<<std::endl;
	}

	addActiveObject(obj);
}

void ClientEnvironment::removeActiveObject(u16 id)
{
/*
	verbosestream<<"ClientEnvironment::removeActiveObject(): "
			<<"id="<<id<<std::endl;
*/
	ClientActiveObject* obj = getActiveObject(id);
	if(obj == NULL)
	{
		infostream<<"ClientEnvironment::removeActiveObject(): "
				<<"id="<<id<<" not found"<<std::endl;
		return;
	}
	obj->removeFromScene(true);
	delete obj;
	m_active_objects.erase(id);
}

void ClientEnvironment::processActiveObjectMessage(u16 id,
		const std::string &data)
{
	ClientActiveObject* obj = getActiveObject(id);
	if(obj == NULL)
	{
		infostream<<"ClientEnvironment::processActiveObjectMessage():"
				<<" got message for id="<<id<<", which doesn't exist."
				<<std::endl;
		return;
	}
	try
	{
		obj->processMessage(data);
	}
	catch(SerializationError &e)
	{
		errorstream<<"ClientEnvironment::processActiveObjectMessage():"
				<<" id="<<id<<" type="<<obj->getType()
				<<" SerializationError in processMessage(),"
				<<" message="<<serializeJsonString(data)
				<<std::endl;
	}
}

/*
	Callbacks for activeobjects
*/

void ClientEnvironment::damageLocalPlayer(u8 damage, bool handle_hp)
{
	LocalPlayer *lplayer = getLocalPlayer();

	if (!lplayer)
		return;


	if (handle_hp) {
		if (lplayer->hp > damage)
			lplayer->hp -= damage;
		else
			lplayer->hp = 0;
	}

	ClientEnvEvent event;
	event.type = CEE_PLAYER_DAMAGE;
	event.player_damage.amount = damage;
	event.player_damage.send_to_server = handle_hp;
	m_client_event_queue.push_back(event);
}

void ClientEnvironment::updateLocalPlayerBreath(u16 breath)
{
	ClientEnvEvent event;
	event.type = CEE_PLAYER_BREATH;
	event.player_breath.amount = breath;
	m_client_event_queue.push_back(event);
}

/*
	Client likes to call these
*/

void ClientEnvironment::getActiveObjects(v3f origin, f32 max_d,
		std::vector<DistanceSortedActiveObject> &dest)
{
	for(std::map<u16, ClientActiveObject*>::iterator
			i = m_active_objects.begin();
			i != m_active_objects.end(); ++i)
	{
		ClientActiveObject* obj = i->second;

		f32 d = (obj->getPosition() - origin).getLength();

		if(d > max_d)
			continue;

		DistanceSortedActiveObject dso(obj, d);

		dest.push_back(dso);
	}
}

ClientEnvEvent ClientEnvironment::getClientEvent()
{
	ClientEnvEvent event;
	if(m_client_event_queue.empty())
		event.type = CEE_NONE;
	else {
		event = m_client_event_queue.front();
		m_client_event_queue.pop_front();
	}
	return event;
}

#endif // #ifndef SERVER
