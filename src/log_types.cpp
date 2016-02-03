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

std::ostream & operator<<(std::ostream & s, v2POS p) {
	s << "(" << p.X << "," << p.Y << ")";
	return s;
}

std::ostream & operator<<(std::ostream & s, v3POS p) {
	s << "(" << p.X << "," << p.Y << "," << p.Z << ")";
	return s;
}

std::ostream & operator<<(std::ostream & s, v3f p) {
	s << "(" << p.X << "," << p.Y << "," << p.Z << ")";
	return s;
}

std::ostream & operator<<(std::ostream & s, std::map<v3POS, unsigned int> & p) {
	for (auto & i: p)
		s << i.first << "=" << i.second<<" ";
	return s;
}

std::ostream & operator<<(std::ostream & s, irr::video::SColor c) {
	s << "c32(" << c.color << ": a=" << c.getAlpha()<< ",r=" << c.getRed()<< ",g=" << c.getGreen()<< ",b=" << c.getBlue() << ")";
	return s;
}

std::ostream & operator<<(std::ostream & s, irr::video::SColorf c) {
	s << "cf32(" << "a=" << c.getAlpha()<< ",r=" << c.getRed()<< ",g=" << c.getGreen()<< ",b=" << c.getBlue() << ")";
	return s;
}

#include "util/string.h"
std::ostream & operator<<(std::ostream & s, const std::wstring & w) {
	s << wide_to_narrow(w);
	return s;
}

#include "json/json.h"
Json::StyledWriter writer;
std::ostream & operator<<(std::ostream & s, Json::Value & json) {
	s << writer.write(json);
	return s;
}
