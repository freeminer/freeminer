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

#include "log_types.h"

std::ostream & operator<<(std::ostream & s, v3s16 p) {
	s << "(" << p.X << "," << p.Y << "," << p.Z << ")";
	return s;
}

std::ostream & operator<<(std::ostream & s, std::map<v3s16, unsigned int> & p) {
	for (auto & i: p)
		s << i.first << "=" << i.second<<" ";
	return s;
}

#include "mapnode.h"
std::ostream & operator<<(std::ostream & s, MapNode n) {
	s << "node["<<(int)n.param0<<","<<(int)n.param1<<","<<(int)n.param1<<"]";
	return s;
}
