#include "circuit.h"
#include "circuit_element.h"
#include "debug.h"
#include "nodedef.h"
#include "mapblock.h"
#include "mapnode.h"
#include "scripting_game.h"
#include "map.h"
#include "serialization.h"
#include "main.h"
#include "settings.h"
#include "log.h"
#include "key_value_storage.h"

#include <map>
#include <iomanip>
#include <cassert>
#include <string>
#include <sstream>

#define PP(x) ((x).X)<<" "<<((x).Y)<<" "<<((x).Z)<<" "

const u32 Circuit::circuit_simulator_version = 1ul;
const char Circuit::elements_states_file[] = "circuit_elements_states";
const char Circuit::elements_func_file[] = "circuit_elements_func";

Circuit::Circuit(GameScripting* script, Map* map, INodeDefManager* ndef, std::string savedir) :
	m_script(script),
	m_map(map),
	m_ndef(ndef),
	m_min_update_delay(0.2f),
	m_since_last_update(0.0f),
	m_max_id(0ul),
	m_max_virtual_id(1ul),
	m_savedir(savedir),
	m_updating_process(false) {
		load();
}

Circuit::~Circuit() {
	save();
	m_elements.clear();
	delete m_database;
	delete m_virtual_database;
	m_script = nullptr;
	m_map = nullptr;
	m_ndef = nullptr;
	m_database = nullptr;
	m_virtual_database = nullptr;
}

void Circuit::addBlock(MapBlock* block) {
	// v3s16 pos;
	// for(pos.X = 0; pos.X < 16; ++pos.X)
	// {
	// 	for(pos.Y = 0; pos.Y < 16; ++pos.Y)
	// 	{
	// 		for(pos.Z = 0; pos.Z < 16; ++pos.Z)
	// 		{
	// 			MapNode tmp_node = block->getNode(pos);
	// 			if(ndef->get(tmp_node).is_circuit_element)
	// 			{
	// 				pos.X = m_pos.X * MAP_BLOCKSIZE + x;
	// 				pos.Y = m_pos.Y * MAP_BLOCKSIZE + y;
	// 				pos.Z = m_pos.Z * MAP_BLOCKSIZE + z;
	// 			}
	// 		}
	// 	}
	// }
}

void Circuit::addNode(v3s16 pos) {
	MapNode n = m_map->getNodeNoEx(pos);
	if(m_ndef->get(n).is_wire || m_ndef->get(n).is_wire_connector) {
		addWire(pos);
	}
	// Call circuit update
	if(m_ndef->get(n).is_circuit_element) {
		addElement(pos);
	}
}

void Circuit::removeNode(v3s16 pos, const MapNode& n_old) {
	if(m_ndef->get(n_old).is_wire || m_ndef->get(n_old).is_wire_connector) {
		removeWire(pos);
	}
	if(m_ndef->get(n_old).is_circuit_element) {
		removeElement(pos);
	}
}

void Circuit::swapNode(v3s16 pos, const MapNode& n_old, const MapNode& n_new) {
	if(m_ndef->get(n_new).is_circuit_element) {
		if(m_ndef->get(n_old).is_circuit_element) {
			updateElement(n_new, pos, m_ndef->get(n_new).circuit_element_func);
		} else {
			if(m_ndef->get(n_old).is_wire) {
				removeWire(pos);
			}
			addElement(pos);
		}
	} else {
		if(m_ndef->get(n_old).is_circuit_element) {
			removeElement(pos);
		} else if(m_ndef->get(n_old).is_wire) {
			removeWire(pos);
		}
		if(m_ndef->get(n_new).is_wire) {
			addWire(pos);
		}
	}
}

void Circuit::addElement(v3s16 pos) {
	JMutexAutoLock lock(m_elements_mutex);

	bool already_existed[6];
	bool connected_faces[6] = {0};

	std::vector <std::pair <std::list <CircuitElement>::iterator, u8> > connected;
	MapNode node = m_map->getNodeNoEx(pos);

	std::list <CircuitElement>::iterator current_element_iterator =
		m_elements.insert(m_elements.begin(), CircuitElement(pos, m_ndef->get(node).circuit_element_func,
	                      m_max_id++, m_ndef->get(node).circuit_element_delay));
	m_pos_to_iterator[pos] = current_element_iterator;

	// For each face add all other connected faces.
	for(int i = 0; i < 6; ++i) {
		if(!connected_faces[i]) {
			connected.clear();
			CircuitElement::findConnectedWithFace(connected, m_map, m_ndef, pos, SHIFT_TO_FACE(i), m_pos_to_iterator, connected_faces);
			if(connected.size() > 0) {
				dstream << "For face_id = " << i <<  " found connected: ";
				for(auto i : connected) {
					dstream << PP(i.first->getPos()) << static_cast<u32>(i.second) << ", ";
				}
				dstream << std::endl;
				std::list <CircuitElementVirtual>::iterator virtual_element_it;
				bool found = false;
				for(auto j = connected.begin(); j != connected.end(); ++j) {
					if(j->first->getFace(j->second).is_connected) {
						virtual_element_it = j->first->getFace(j->second).list_pointer;
						found = true;
						break;
					}
				}

				// If virtual element already exist
				if(found) {
					already_existed[i] = true;
				} else {
					already_existed[i] = false;
					virtual_element_it = m_virtual_elements.insert(m_virtual_elements.begin(),
					                     CircuitElementVirtual(m_max_virtual_id++));
				}

				std::list <CircuitElementVirtualContainer>::iterator it;
				for(auto j = connected.begin(); j != connected.end(); ++j) {
					if(!j->first->getFace(j->second).is_connected) {
						it = virtual_element_it->insert(virtual_element_it->begin(), CircuitElementVirtualContainer());
						it->shift = j->second;
						it->element_pointer = j->first;
						j->first->connectFace(j->second, it, virtual_element_it);
					}
				}
				it = virtual_element_it->insert(virtual_element_it->begin(), CircuitElementVirtualContainer());
				it->shift = i;
				it->element_pointer = current_element_iterator;
				current_element_iterator->connectFace(i, it, virtual_element_it);
			}

		}
	}

	for(int i = 0; i < 6; ++i) {
		if(current_element_iterator->getFace(i).is_connected && !already_existed[i]) {
			saveVirtualElement(current_element_iterator->getFace(i).list_pointer, true);
		}
	}
	saveElement(current_element_iterator, true);
}

void Circuit::removeElement(v3s16 pos) {
	JMutexAutoLock lock(m_elements_mutex);

	std::vector <std::list <CircuitElementVirtual>::iterator> virtual_elements_for_update;
	std::list <CircuitElement>::iterator current_element = m_pos_to_iterator[pos];
	m_database->del(itos(current_element->getId()));

	current_element->getNeighbors(virtual_elements_for_update);

	m_elements.erase(current_element);

	for(auto i = virtual_elements_for_update.begin(); i != virtual_elements_for_update.end(); ++i) {
		if((*i)->size() > 1) {
			std::ostringstream out(std::ios_base::binary);
			(*i)->serialize(out);
			m_virtual_database->put(itos((*i)->getId()), out.str());
		} else {
			m_virtual_database->del(itos((*i)->getId()));
			std::list <CircuitElement>::iterator element_to_save;
			for(auto j = (*i)->begin(); j != (*i)->end(); ++j) {
				element_to_save = j->element_pointer;
			}
			m_virtual_elements.erase(*i);
			saveElement(element_to_save, false);
		}
	}

	m_pos_to_iterator.erase(pos);
}

void Circuit::addWire(v3s16 pos) {
	JMutexAutoLock lock(m_elements_mutex);

	// This is used for converting elements of current_face_connected to their ids in all_connected.
	std::vector <std::pair <std::list <CircuitElement>::iterator, u8> > all_connected;
	std::vector <std::list <CircuitElementVirtual>::iterator> created_virtual_elements;

	bool used[6][6];
	bool connected_faces[6];

	MapNode node = m_map->getNode(pos);
	std::vector <std::pair <std::list <CircuitElement>::iterator, u8> > connected_to_face[6];
	for(int i = 0; i < 6; ++i) {
		CircuitElement::findConnectedWithFace(connected_to_face[i], m_map, m_ndef, pos, SHIFT_TO_FACE(i),
		                                      m_pos_to_iterator, connected_faces);
	}

	for(int i = 0; i < 6; ++i) {
		for(int j = 0; j < 6; ++j) {
			used[i][j] = false;
		}
	}

	// For each face connect faces, that are not yet connected.
	for(int i = 0; i < 6; ++i) {
		all_connected.clear();
		u8 acceptable_faces = CircuitElement::getAcceptableFaces(node, m_ndef->get(node), i);
		for(int j = 0; j < 6; ++j) {
			if((acceptable_faces & (SHIFT_TO_FACE(j))) && !used[i][j]) {
				all_connected.insert(all_connected.end(), connected_to_face[j].begin(), connected_to_face[j].end());
				used[i][j] = true;
				used[j][i] = true;
			}
		}

		if(all_connected.size() > 1) {
			CircuitElementContainer element_with_virtual;
			bool found_virtual = false;
			for(auto i = all_connected.begin(); i != all_connected.end(); ++i) {
				if(i->first->getFace(i->second).is_connected) {
					element_with_virtual = i->first->getFace(i->second);
					found_virtual = true;
					break;
				}
			}

			if(found_virtual) {
				// Clear old connections (remove some virtual elements)
				for(auto i = all_connected.begin(); i != all_connected.end(); ++i) {
					if(i->first->getFace(i->second).is_connected
					        && (i->first->getFace(i->second).list_pointer != element_with_virtual.list_pointer)) {
						m_virtual_database->del(itos(i->first->getFace(i->second).list_pointer->getId()));
						i->first->disconnectFace(i->second);
						m_virtual_elements.erase(i->first->getFace(i->second).list_pointer);
					}
				}
			} else {
				element_with_virtual.list_pointer = m_virtual_elements.insert(m_virtual_elements.begin(),
				                                    CircuitElementVirtual(m_max_virtual_id++));
			}
			created_virtual_elements.push_back(element_with_virtual.list_pointer);

			// Create new connections
			for(auto i = all_connected.begin(); i != all_connected.end(); ++i) {
				if(!(i->first->getFace(i->second).is_connected)) {
					auto it = element_with_virtual.list_pointer->insert(
						element_with_virtual.list_pointer->begin(), CircuitElementVirtualContainer());
					it->element_pointer = i->first;
					it->shift = i->second;
					i->first->connectFace(i->second, it, element_with_virtual.list_pointer);
				}
			}
		}
	}

	for(u32 i = 0; i < created_virtual_elements.size(); ++i) {
		saveVirtualElement(created_virtual_elements[i], true);
	}
}

void Circuit::removeWire(v3s16 pos) {
	JMutexAutoLock lock(m_elements_mutex);

	std::vector <std::pair <std::list <CircuitElement>::iterator, u8> > current_face_connected;

	bool connected_faces[6];
	for(int i = 0; i < 6; ++i) {
		connected_faces[i] = false;
	}

	// Find and remove virtual elements
	bool found_virtual_elements = false;
	for(int i = 0; i < 6; ++i) {
		if(!connected_faces[i]) {
			current_face_connected.clear();
			CircuitElement::findConnectedWithFace(current_face_connected, m_map, m_ndef, pos,
			                                      SHIFT_TO_FACE(i), m_pos_to_iterator, connected_faces);
			for(u32 j = 0; j < current_face_connected.size(); ++j) {
				CircuitElementContainer current_edge = current_face_connected[j].first->getFace(current_face_connected[j].second);
				if(current_edge.is_connected) {
					found_virtual_elements = true;
					m_virtual_database->del(itos(current_edge.list_pointer->getId()));

					m_virtual_elements.erase(current_edge.list_pointer);
					break;
				}
			}

			for(u32 j = 0; j < current_face_connected.size(); ++j) {
				saveElement(current_face_connected[j].first, false);
			}

		}
	}

	for(int i = 0; i < 6; ++i) {
		connected_faces[i] = false;
	}

	if(found_virtual_elements) {
		// Restore some previously deleted connections.
		for(int i = 0; i < 6; ++i) {
			if(!connected_faces[i]) {
				current_face_connected.clear();
				CircuitElement::findConnectedWithFace(current_face_connected, m_map, m_ndef, pos, SHIFT_TO_FACE(i),
				                                      m_pos_to_iterator, connected_faces);

				if(current_face_connected.size() > 1) {
					std::list <CircuitElementVirtual>::iterator new_virtual_element = m_virtual_elements.insert(
					            m_virtual_elements.begin(), CircuitElementVirtual(m_max_virtual_id++));

					for(u32 j = 0; j < current_face_connected.size(); ++j) {
						std::list <CircuitElementVirtualContainer>::iterator new_container = new_virtual_element->insert(
						            new_virtual_element->begin(), CircuitElementVirtualContainer());
						new_container->element_pointer = current_face_connected[j].first;
						new_container->shift = current_face_connected[j].second;
						current_face_connected[j].first->connectFace(current_face_connected[j].second,
						        new_container, new_virtual_element);

						saveElement(current_face_connected[j].first, false);
					}

					saveVirtualElement(new_virtual_element, false);
				}
			}
		}
	}
}

void Circuit::update(float dtime) {
	if(m_since_last_update > m_min_update_delay) {
		dstream << "----------------------------------------" << std::endl;
		JMutexAutoLock lock(m_elements_mutex);
		m_updating_process = true;

		m_since_last_update -= m_min_update_delay;
		// Each element send signal to other connected virtual elements.
		for(std::list <CircuitElement>::iterator i = m_elements.begin();
		        i != m_elements.end(); ++i) {
			i->update();
		}

		// Each virtual element send signal to other connected elements.
		for(std::list <CircuitElementVirtual>::iterator i = m_virtual_elements.begin();
		        i != m_virtual_elements.end(); ++i) {
			i->update();
		}

		// Update state of each element.
		for(std::list <CircuitElement>::iterator i = m_elements.begin();
		        i != m_elements.end(); ++i) {
			i->updateState(m_script, m_map, m_ndef);
		}

		m_updating_process = false;
#ifdef CIRCUIT_DEBUG
		dstream << "Dt: " << dtime << " " << m_since_last_update << std::endl;
		for(std::list <CircuitElement>::iterator i = m_elements.begin();
		        i != m_elements.end(); ++i) {
			dstream << PP(i->getPos()) << " " << i->getId() << ": ";
			for(int j = 0; j < 6; ++j) {
				CircuitElementContainer tmp_face = i->getFace(j);
				if(tmp_face.list_pointer) {
					dstream << tmp_face.list_pointer->getId();
				}
				dstream << ", ";
			}
			dstream << std::endl;
		}
#endif
	} else {
		m_since_last_update += dtime;
	}
}


void Circuit::updateElement(const MapNode& node, v3s16 pos, const u8* func) {
	if(!m_updating_process) {
		m_elements_mutex.Lock();
	}

	std::list <CircuitElement>::iterator current_element = m_pos_to_iterator[pos];
	current_element->setFunc(m_ndef->get(node).circuit_element_func);
	current_element->setDelay(m_ndef->get(node).circuit_element_delay);
	saveElement(current_element, false);

	if(!m_updating_process) {
		m_elements_mutex.Unlock();
	}
}

void Circuit::load() {
	u32 element_id;
	u32 version = 0;
	std::istringstream in(std::ios_base::binary);

	m_database = new KeyValueStorage(m_savedir, "circuit");
	m_virtual_database = new KeyValueStorage(m_savedir, "circuit_virtual");

	std::ifstream input_elements_states((m_savedir + DIR_DELIM + elements_states_file).c_str());

	if(input_elements_states.good()) {
		input_elements_states.read(reinterpret_cast<char*>(&version), sizeof(version));
	}
#if USE_LEVELDB
	// Filling list with empty virtual elements
	dstream << "ok!" << std::endl;
	auto virtual_it = m_virtual_database->new_iterator();
	std::map <u32, std::list <CircuitElementVirtual>::iterator> id_to_virtual_pointer;
	for(virtual_it->SeekToFirst(); virtual_it->Valid(); virtual_it->Next()) {
		element_id = stoi(virtual_it->key().ToString());
		id_to_virtual_pointer[element_id] =
		    m_virtual_elements.insert(m_virtual_elements.begin(), CircuitElementVirtual(element_id));
		if(element_id + 1 > m_max_virtual_id) {
			m_max_virtual_id = element_id + 1;
		}
	}
	dstream << "ok!" << std::endl;

	// Filling list with empty elements
	auto it = m_database->new_iterator();
	std::map <u32, std::list <CircuitElement>::iterator> id_to_pointer;
	for(it->SeekToFirst(); it->Valid(); it->Next()) {
		element_id = stoi(it->key().ToString());
		id_to_pointer[element_id] =
		    m_elements.insert(m_elements.begin(), CircuitElement(element_id));
		if(element_id + 1 > m_max_id) {
			m_max_id = element_id + 1;
		}
	}

	// Loading states of elements
	if(input_elements_states.good()) {
		for(u32 i = 0; i < m_elements.size(); ++i) {
			input_elements_states.read(reinterpret_cast<char*>(&element_id), sizeof(element_id));
			if(id_to_pointer.find(element_id) != id_to_pointer.end()) {
				id_to_pointer[element_id]->deSerializeState(input_elements_states);
			} else {
				throw SerializationError(static_cast<std::string>("File \"")
				                         + elements_states_file + "\" seems to be corrupted.");
			}
		}
	}

	// Loading elements data
	for(it->SeekToFirst(); it->Valid(); it->Next()) {
		in.str(it->value().ToString());
		element_id = stoi(it->key().ToString());
		std::list <CircuitElement>::iterator current_element = id_to_pointer[element_id];
		current_element->deSerialize(in, id_to_virtual_pointer);
		// current_element->setFunc(m_circuit_elements_states.getFunc(current_element->getFuncId()), current_element->getFuncId());
		m_pos_to_iterator[current_element->getPos()] = current_element;
	}
	delete it;

	// Loading virtual elements data
	for(virtual_it->SeekToFirst(); virtual_it->Valid(); virtual_it->Next()) {
		in.str(virtual_it->value().ToString());
		element_id = stoi(virtual_it->key().ToString());
		std::list <CircuitElementVirtual>::iterator current_element = id_to_virtual_pointer[element_id];
		current_element->deSerialize(in, current_element, id_to_pointer);
	}

	delete virtual_it;
#endif
}

void Circuit::save() {
	JMutexAutoLock lock(m_elements_mutex);
	std::ostringstream ostr(std::ios_base::binary);
	std::ofstream out((m_savedir + DIR_DELIM + elements_states_file).c_str(), std::ios_base::binary);
	out.write(reinterpret_cast<const char*>(&circuit_simulator_version), sizeof(circuit_simulator_version));
	for(std::list<CircuitElement>::iterator i = m_elements.begin(); i != m_elements.end(); ++i) {
		i->serializeState(ostr);
	}
	out << ostr.str();
}

inline void Circuit::saveElement(std::list<CircuitElement>::iterator element, bool save_edges) {
	std::ostringstream out(std::ios_base::binary);
	element->serialize(out);
	m_database->put(itos(element->getId()), out.str());
	if(save_edges) {
		for(int i = 0; i < 6; ++i) {
			CircuitElementContainer tmp_container = element->getFace(i);
			if(tmp_container.is_connected) {
				std::ostringstream out(std::ios_base::binary);
				tmp_container.list_pointer->serialize(out);
				m_virtual_database->put(itos(tmp_container.list_pointer->getId()), out.str());
			}
		}
	}
}

inline void Circuit::saveVirtualElement(std::list <CircuitElementVirtual>::iterator element, bool save_edges) {
	std::ostringstream out(std::ios_base::binary);
	element->serialize(out);
	m_virtual_database->put(itos(element->getId()), out.str());
	if(save_edges) {
		for(std::list <CircuitElementVirtualContainer>::iterator i = element->begin(); i != element->end(); ++i) {
			std::ostringstream out(std::ios_base::binary);
			i->element_pointer->serialize(out);
			m_database->put(itos(i->element_pointer->getId()), out.str());
		}
	}
}
