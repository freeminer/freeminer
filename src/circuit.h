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

#ifndef CIRCUIT_H
#define CIRCUIT_H

#include <list>
#include <vector>
#include <map>

#include "circuit_element.h"
#include "circuit_element_virtual.h"
#include "irrlichttypes.h"
#include "threading/lock.h"


class NodeDefManager;
class ServerScripting;
class Map;
class MapBlock;
class KeyValueStorage;

class Circuit {
public:
	Circuit(ServerScripting* script, Map* map, const NodeDefManager* ndef, const std::string & savedir);
	~Circuit();
	void addBlock(MapBlock* block);
	void addNode(v3pos_t pos);
	void removeNode(v3pos_t pos, const MapNode& n_old);
	void swapNode(v3pos_t pos, const MapNode& n_old, const MapNode& n_new);
	void addElement(v3pos_t pos);
	void removeElement(v3pos_t pos);
	void addWire(v3pos_t pos);
	void removeWire(v3pos_t pos);
	void update(float dtime);
	void swapElement(const MapNode& n_old, const MapNode& n_new, v3pos_t pos);

	void load();
	void save();
	void saveElement(std::list <CircuitElement>::iterator element, bool save_edges);
	void saveVirtualElement(std::list <CircuitElementVirtual>::iterator element, bool save_edges);
	void open();
	void close();

private:
	std::list <CircuitElement> m_elements;
	std::list <CircuitElementVirtual> m_virtual_elements;

	std::map <v3pos_t, std::list<CircuitElement>::iterator> m_pos_to_iterator;
	std::map <const unsigned char*, u32> m_func_to_id;

	ServerScripting* m_script;
	Map* m_map;
	const NodeDefManager* m_ndef;

	std::vector <v3pos_t> m_elements_queue;
	float m_min_update_delay;
	float m_since_last_update;
	float m_min_save_delay;
	float m_since_last_save;

	u32 m_max_id;
	u32 m_max_virtual_id;

	std::string m_savedir;

	KeyValueStorage *m_database;
	KeyValueStorage *m_virtual_database;

	shared_locker m_elements_mutex;

	static const u32 circuit_simulator_version;
	static const char elements_states_file[];
};

#endif
