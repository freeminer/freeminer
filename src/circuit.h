#ifndef CIRCUIT_H
#define CIRCUIT_H

#include <list>
#include <vector>
#include <map>

#include "circuit_element.h"
#include "circuit_element_virtual.h"
#include "irrlichttypes.h"
#include "jthread/jmutexautolock.h"

class INodeDefManager;
class GameScripting;
class Map;
class MapBlock;
class KeyValueStorage;

class Circuit {
public:
	Circuit(GameScripting* script, Map* map, INodeDefManager* ndef, std::string savedir);
	~Circuit();
	void addBlock(MapBlock* block);
	void addNode(v3s16 pos);
	void removeNode(v3s16 pos, const MapNode& n_old);
	void swapNode(v3s16 pos, const MapNode& n_old, const MapNode& n_new);
	void addElement(v3s16 pos);
	void removeElement(v3s16 pos);
	void addWire(v3s16 pos);
	void removeWire(v3s16 pos);
	void update(float dtime);
	void swapElement(const MapNode& n_old, const MapNode& n_new, v3s16 pos);

	void load();
	void save();
	void saveElement(std::list <CircuitElement>::iterator element, bool save_edges);
	void saveVirtualElement(std::list <CircuitElementVirtual>::iterator element, bool save_edges);

private:
	std::list <CircuitElement> m_elements;
	std::list <CircuitElementVirtual> m_virtual_elements;

	std::map <v3s16, std::list<CircuitElement>::iterator> m_pos_to_iterator;
	std::map <const unsigned char*, u32> m_func_to_id;

	GameScripting* m_script;
	Map* m_map;
	INodeDefManager* m_ndef;

	std::vector <v3s16> m_elements_queue;
	float m_min_update_delay;
	float m_since_last_update;

	u32 m_max_id;
	u32 m_max_virtual_id;

	std::string m_savedir;

	bool m_updating_process;

	KeyValueStorage *m_database;
	KeyValueStorage *m_virtual_database;

	JMutex m_elements_mutex;

	static const u32 circuit_simulator_version;
	static const char elements_states_file[];
};

#endif
