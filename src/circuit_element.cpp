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

#include "circuit_element.h"
#include "nodedef.h"
#include "mapnode.h"
#include "map.h"
#include "scripting_game.h"

#include <set>
#include <queue>
#include <iomanip>
#include <cassert>
#include <map>

#define PP(x) ((x).X)<<" "<<((x).Y)<<" "<<((x).Z)<<" "

u8 CircuitElement::face_to_shift[] = {
	0, 0, 1, 0, 2, 0, 0, 0, 3, 0, 0, 0, 0, 0, 0, 0,
	4, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 5
};

u8 CircuitElement::opposite_shift[] = {
	1, 0, 3, 2, 5, 4
};

u8 CircuitElement::opposite_face[] = {
	0, 2, 1, 0, 8, 0, 0, 0, 4, 0, 0, 0, 0, 0, 0, 0,
	32, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 16
};

u8 CircuitElement::rotate_face[] = {
	1, 1, 1, 1, 16, 16, 16, 16, 32, 32, 32, 32, 4, 4, 4, 4, 8, 8, 8, 8, 2, 2, 2, 2,
	2, 2, 2, 2, 32, 32, 32, 32, 16, 16, 16, 16, 8, 8, 8, 8, 4, 4, 4, 4, 1, 1, 1, 1,
	4, 32, 8, 16, 4, 1, 8, 2, 4, 2, 8, 1, 2, 32, 1, 16, 1, 32, 2, 16, 8, 32, 4, 16,
	8, 16, 4, 32, 8, 2, 4, 1, 8, 1, 4, 2, 1, 16, 2, 32, 2, 16, 1, 32, 4, 16, 8, 32,
	16, 4, 32, 8, 2, 4, 1, 8, 1, 4, 2, 8, 16, 2, 32, 1, 16, 1, 32, 2, 16, 8, 32, 4,
	32, 8, 16, 4, 1, 8, 2, 4, 2, 8, 1, 4, 32, 1, 16, 2, 32, 2, 16, 1, 32, 4, 16, 8,
};

u8 CircuitElement::reverse_rotate_face[] = {
	1, 1, 1, 1, 32, 4, 16, 8, 16, 8, 32, 4, 8, 32, 4, 16, 4, 16, 8, 32, 2, 2, 2, 2,
	2, 2, 2, 2, 16, 8, 32, 4, 32, 4, 16, 8, 4, 16, 8, 32, 8, 32, 4, 16, 1, 1, 1, 1,
	4, 16, 8, 32, 4, 16, 8, 32, 4, 16, 8, 32, 1, 1, 1, 1, 2, 2, 2, 2, 8, 32, 4, 16,
	8, 32, 4, 16, 8, 32, 4, 16, 8, 32, 4, 16, 2, 2, 2, 2, 1, 1, 1, 1, 4, 16, 8, 32,
	16, 8, 32, 4, 1, 1, 1, 1, 2, 2, 2, 2, 16, 8, 32, 4, 16, 8, 32, 4, 16, 8, 32, 4,
	32, 4, 16, 8, 2, 2, 2, 2, 1, 1, 1, 1, 32, 4, 16, 8, 32, 4, 16, 8, 32, 4, 16, 8,
};

CircuitElement::CircuitElement(v3s16 pos, u32 element_id, u8 delay) :
	m_pos(pos), m_prev_input_state(0), m_current_input_state(0),
	m_next_input_state(0), m_current_output_state(0){
	m_element_id = element_id;
	for(int i = 0; i < 6; ++i) {
		m_faces[i].is_connected = false;
	}
	setDelay(delay);
#ifdef CIRCUIT_DEBUG
	dstream << (OPPOSITE_FACE(FACE_TOP) == FACE_BOTTOM);
	dstream << (OPPOSITE_FACE(FACE_BACK) == FACE_FRONT);
	dstream << (OPPOSITE_FACE(FACE_LEFT) == FACE_RIGHT);
	dstream << (OPPOSITE_FACE(FACE_BOTTOM) == FACE_TOP);
	dstream << (OPPOSITE_FACE(FACE_FRONT) == FACE_BACK);
	dstream << (OPPOSITE_FACE(FACE_RIGHT) == FACE_LEFT);
	dstream << std::endl;
#endif
}

CircuitElement::CircuitElement(const CircuitElement& element) {
	m_pos = element.m_pos;
	m_element_id = element.m_element_id;
	m_prev_input_state = element.m_prev_input_state;
	m_current_input_state = element.m_current_input_state;
	m_next_input_state = element.m_next_input_state;
	m_current_output_state = element.m_current_output_state;
	for(int i = 0; i < 6; ++i) {
		m_faces[i].list_iterator = element.m_faces[i].list_iterator;
		m_faces[i].list_pointer  = element.m_faces[i].list_pointer;
		m_faces[i].is_connected  = element.m_faces[i].is_connected;
	}
	setDelay(element.m_states_queue.size());
}

CircuitElement::CircuitElement(u32 element_id) :
	m_pos(v3s16(0, 0, 0)), m_prev_input_state(0), m_current_input_state(0),
	m_next_input_state(0), m_current_output_state(0) {
	m_element_id = element_id;
	for(int i = 0; i < 6; ++i) {
		m_faces[i].is_connected = false;
	}
}

CircuitElement::~CircuitElement() {
	for(int i = 0; i < 6; ++i) {
		if(m_faces[i].is_connected) {
			m_faces[i].list_pointer->erase(m_faces[i].list_iterator);
		}
	}
}

void CircuitElement::update() {
	if(m_current_output_state) {
		for(int i = 0; i < 6; ++i) {
			if(m_faces[i].is_connected) {
				m_faces[i].list_pointer->addState(static_cast<bool>(m_current_output_state & SHIFT_TO_FACE(i)));
			}
		}
	}
}

bool CircuitElement::updateState(GameScripting* m_script, Map* map, INodeDefManager* ndef) {
	MapNode node = map->getNodeNoEx(m_pos);
	// Map not yet loaded
	if(node.param0 == CONTENT_IGNORE) {
		dstream << "Circuit simulator: Waiting for map blocks loading..." << std::endl;
		return false;
	}
	const ContentFeatures& node_features = ndef->get(node);
	// Update delay (may be not synchronized)
	u32 delay = node_features.circuit_element_delay;
	if(delay != m_states_queue.size()) {
		setDelay(delay);
	}
	m_states_queue.push_back(m_next_input_state);
	m_next_input_state = m_states_queue.front();
	m_states_queue.pop_front();
	m_current_output_state = node_features.circuit_element_func[m_next_input_state];
	if(m_next_input_state && !m_current_input_state && node_features.has_on_activate) {
		m_script->node_on_activate(m_pos, node);
	}
	if(!m_next_input_state && m_current_input_state && node_features.has_on_deactivate) {
		m_script->node_on_deactivate(m_pos, node);
	}
	m_prev_input_state = m_current_input_state;
	m_current_input_state = m_next_input_state;
	m_next_input_state = 0;
	return true;
}

void CircuitElement::resetState() {
	m_next_input_state = 0;
	m_current_input_state = m_prev_input_state;
}

void CircuitElement::serialize(std::ostream& out) const {
	out.write(reinterpret_cast<const char*>(&m_pos), sizeof(m_pos));
	for(int i = 0; i < 6; ++i) {
		u32 tmp = 0;
		if(m_faces[i].is_connected) {
			tmp = m_faces[i].list_pointer->getId();
		}
		out.write(reinterpret_cast<const char*>(&tmp), sizeof(tmp));
	}
}

void CircuitElement::serializeState(std::ostream& out) const {
	out.write(reinterpret_cast<const char*>(&m_element_id), sizeof(m_element_id));
	out.write(reinterpret_cast<const char*>(&m_current_input_state), sizeof(m_current_input_state));
	out.write(reinterpret_cast<const char*>(&m_current_output_state), sizeof(m_current_output_state));
	u32 queue_size = m_states_queue.size();
	out.write(reinterpret_cast<const char*>(&queue_size), sizeof(queue_size));
	for(auto i = m_states_queue.begin(); i != m_states_queue.end(); ++i) {
		out.write(reinterpret_cast<const char*>(&(*i)), sizeof(*i));
	}
}

void CircuitElement::deSerialize(std::istream& in,
                                 std::map <u32, std::list <CircuitElementVirtual>::iterator>& id_to_virtual_pointer) {
	u32 current_element_id;
	in.read(reinterpret_cast<char*>(&m_pos), sizeof(m_pos));
	for(int i = 0; i < 6; ++i) {
		in.read(reinterpret_cast<char*>(&current_element_id), sizeof(current_element_id));
		if(current_element_id > 0) {
			m_faces[i].list_pointer = id_to_virtual_pointer[current_element_id];
			m_faces[i].is_connected = true;
		} else {
			m_faces[i].is_connected = false;
		}
	}
}

void CircuitElement::deSerializeState(std::istream& in) {
	u32 queue_size;
	u8 input_state;
	in.read(reinterpret_cast<char*>(&m_current_input_state), sizeof(m_current_input_state));
	in.read(reinterpret_cast<char*>(&m_current_output_state), sizeof(m_current_output_state));
	in.read(reinterpret_cast<char*>(&queue_size), sizeof(queue_size));
	for(u32 i = 0; i < queue_size; ++i) {
		in.read(reinterpret_cast<char*>(&input_state), sizeof(input_state));
		m_states_queue.push_back(input_state);
	}
}

void CircuitElement::getNeighbors(std::vector <std::list <CircuitElementVirtual>::iterator>& neighbors) const {
	for(int i = 0; i < 6; ++i) {
		if(m_faces[i].is_connected) {
			bool found = false;
			for(auto j = neighbors.begin(); j != neighbors.end(); ++j) {
				if(*j == m_faces[i].list_pointer) {
					found = true;
					break;
				}
			}
			if(!found) {
				neighbors.push_back(m_faces[i].list_pointer);
			}
		}
	}
}

void CircuitElement::findConnectedWithFace(std::vector <std::pair <std::list<CircuitElement>::iterator, u8> >& connected,
        Map* map, INodeDefManager* ndef, v3s16 pos, u8 face,
        std::map<v3s16, std::list<CircuitElement>::iterator>& pos_to_iterator,
        bool connected_faces[6]) {
	static v3s16 directions[6] = {v3s16(0, 1, 0),
	                              v3s16(0, -1, 0),
	                              v3s16(1, 0, 0),
	                              v3s16(-1, 0, 0),
	                              v3s16(0, 0, 1),
	                              v3s16(0, 0, -1),
	                             };
	// First - wire pos, second - acceptable faces
	std::queue <std::pair <v3s16, u8> > q;
	v3s16 current_pos, next_pos;
	MapNode next_node, current_node;
	// used[pos] = or of all faces, that are already processed
	std::map <v3s16, u8> used;
	u8 face_id = FACE_TO_SHIFT(face);
	connected_faces[face_id] = true;
	used[pos] = face;
	current_node = map->getNodeNoEx(pos);
	const ContentFeatures& first_node_features = ndef->get(current_node);
	face = rotateFace(current_node, first_node_features, face);
	face_id = FACE_TO_SHIFT(face);

	current_pos = pos + directions[face_id];
	current_node = map->getNodeNoEx(current_pos);
	const ContentFeatures& current_node_features = ndef->get(current_node);
	u8 real_face = revRotateFace(current_node, current_node_features, face);
	u8 real_face_id = FACE_TO_SHIFT(real_face);

	if(current_node_features.is_wire || current_node_features.is_wire_connector) {
		q.push(std::make_pair(current_pos, current_node_features.wire_connections[real_face_id]));

		while(!q.empty()) {
			current_pos = q.front().first;
			u8 acceptable_faces = q.front().second;
			q.pop();
			current_node = map->getNodeNoEx(current_pos);
			const ContentFeatures& current_node_features = ndef->get(current_node);

			for(int i = 0; i < 6; ++i) {
				u8 real_face = revRotateFace(current_node, current_node_features, SHIFT_TO_FACE(i));
				if(acceptable_faces & real_face) {
					used[current_pos] |= real_face;
					next_pos = current_pos + directions[i];
					next_node = map->getNodeNoEx(next_pos);
					const ContentFeatures& node_features = ndef->get(next_node);
					u8 next_real_face = revRotateFace(next_node, node_features, OPPOSITE_FACE(SHIFT_TO_FACE(i)));
					u8 next_real_shift = FACE_TO_SHIFT(next_real_face);

					// If start element, mark some of it's faces
					if(next_pos == pos) {
						connected_faces[next_real_shift] = true;
					}

					auto next_used_iterator = used.find(next_pos);
					bool is_part_of_circuit = node_features.is_wire_connector || node_features.is_circuit_element ||
						(node_features.is_wire && (next_node.getContent() == current_node.getContent()));
					bool not_used = (next_used_iterator == used.end()) ||
						!(next_used_iterator->second & next_real_face);

					if(is_part_of_circuit && not_used) {
						if(node_features.is_circuit_element) {
							connected.push_back(std::make_pair(pos_to_iterator[next_pos], next_real_shift));
						} else {
							q.push(std::make_pair(next_pos, node_features.wire_connections[next_real_shift]));
						}

						if(next_used_iterator != used.end()) {
							next_used_iterator->second |= next_real_face;
						} else {
							used[next_pos] = next_real_face;
						}
					}
				}
			}
		}
	} else if(current_node_features.is_circuit_element) {
		connected.push_back(std::make_pair(pos_to_iterator[current_pos], OPPOSITE_SHIFT(real_face_id)));
	}
}

CircuitElementContainer CircuitElement::getFace(int id) const
{
	return m_faces[id];
}

v3s16 CircuitElement::getPos() const {
	return m_pos;
}

u32 CircuitElement::getId() const {
	return m_element_id;
}

void CircuitElement::connectFace(int id, std::list <CircuitElementVirtualContainer>::iterator it,
                                 std::list <CircuitElementVirtual>::iterator pt) {
	m_faces[id].list_iterator = it;
	m_faces[id].list_pointer  = pt;
	m_faces[id].is_connected  = true;
}

void CircuitElement::disconnectFace(int id) {
	m_faces[id].is_connected = false;
}

void CircuitElement::setId(u32 id) {
	m_element_id = id;
}

void CircuitElement::setInputState(u8 state) {
	m_current_input_state = state;
}

void CircuitElement::setDelay(u8 delay) {
	if(m_states_queue.size() >= delay) {
		while(m_states_queue.size() > delay) {
			m_states_queue.pop_front();
		}
	} else {
		while(m_states_queue.size() < delay) {
			m_states_queue.push_back(0);
		}
	}
}

void CircuitElement::swap(const MapNode& n_old, const ContentFeatures& n_old_features,
                          const MapNode& n_new, const ContentFeatures& n_new_features) {
	CircuitElementContainer tmp_faces[6];
	for(int i = 0; i < 6; ++i) {
		u8 shift = FACE_TO_SHIFT(rotateFace(n_old, n_old_features, SHIFT_TO_FACE(i)));
		tmp_faces[shift] = m_faces[i];
	}
	for(int i = 0; i < 6; ++i) {
		u8 shift = FACE_TO_SHIFT(revRotateFace(n_new, n_new_features, SHIFT_TO_FACE(i)));
		m_faces[shift] = tmp_faces[i];
		if(m_faces[shift].is_connected) {
			m_faces[shift].list_iterator->shift = shift;
		}
	}
	setDelay(n_new_features.circuit_element_delay);
}
