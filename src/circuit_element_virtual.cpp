#include "circuit_element_virtual.h"
#include "circuit_element.h"
#include "debug.h"

CircuitElementVirtual::CircuitElementVirtual(u32 id) : m_state(false) {
	m_element_id = id;
}

CircuitElementVirtual::~CircuitElementVirtual() {
	for(std::list <CircuitElementVirtualContainer>::iterator i = this->begin();
	        i != this->end(); ++i) {
		i->element_pointer->disconnectFace(i->shift);
	}
}

void CircuitElementVirtual::update() {
	if(m_state) {
		for(std::list <CircuitElementVirtualContainer>::iterator i = this->begin();
		        i != this->end(); ++i) {
			i->element_pointer->addState(SHIFT_TO_FACE(i->shift));
		}
		m_state = false;
	}
}

void CircuitElementVirtual::serialize(std::ostream& out) {
	u32 connections_num = this->size();
	out.write(reinterpret_cast<char*>(&connections_num), sizeof(connections_num));
	for(std::list <CircuitElementVirtualContainer>::iterator i = this->begin();
	        i != this->end(); ++i) {
		u32 element_id = i->element_pointer->getId();
		u8  shift = i->shift;
		out.write(reinterpret_cast<char*>(&element_id), sizeof(element_id));
		out.write(reinterpret_cast<char*>(&shift), sizeof(shift));
	}
}

void CircuitElementVirtual::deSerialize(std::istream& in, std::list <CircuitElementVirtual>::iterator current_element_it,
                                        std::map <u32, std::list <CircuitElement>::iterator>& id_to_pointer) {
	u32 connections_num;
	in.read(reinterpret_cast<char*>(&connections_num), sizeof(connections_num));
	for(u32 i = 0; i < connections_num; ++i) {
		u32 element_id;
		CircuitElementVirtualContainer tmp_container;
		in.read(reinterpret_cast<char*>(&element_id), sizeof(element_id));
		in.read(reinterpret_cast<char*>(&(tmp_container.shift)), sizeof(tmp_container.shift));
		tmp_container.element_pointer = id_to_pointer[element_id];
		std::list <CircuitElementVirtualContainer>::iterator it = this->insert(this->begin(), tmp_container);
		it->element_pointer->connectFace(it->shift, it, current_element_it);
	}
}

void CircuitElementVirtual::setId(u32 id) {
	m_element_id = id;
}

u32 CircuitElementVirtual::getId() {
	return m_element_id;
}

