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

#ifndef CIRCUIT_ELEMENT_VIRTUAL_H
#define CIRCUIT_ELEMENT_VIRTUAL_H

#include <list>
#include <map>
#include <sstream>

#include "irrlichttypes.h"

class CircuitElement;

struct CircuitElementVirtualContainer {
	u8 shift;
	std::list<CircuitElement>::iterator element_pointer;
};

class CircuitElementVirtual : public std::list <CircuitElementVirtualContainer> {
public:
	CircuitElementVirtual(u32 id);
	~CircuitElementVirtual();

	void update();

	void serialize(std::ostream& out);
	void deSerialize(std::istream& is, std::list <CircuitElementVirtual>::iterator current_element_it,
	                 std::map <u32, std::list<CircuitElement>::iterator>& id_to_pointer);

	void setId(u32 id);

	u32 getId();

	inline void addState(const bool state) {
		m_state |= state;
	}

private:
	u32 m_element_id;
	bool m_state;
};

#endif
