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

#ifndef LOG_TYPES_HEADER
#define LOG_TYPES_HEADER

#include "log.h" //for replacing log.h to log_types.h in includes

#include "irr_v3d.h"

#include <ostream>
std::ostream & operator<<(std::ostream & s, v3s16 p);
std::ostream & operator<<(std::ostream & s, v3f p);

#include <SColor.h>
std::ostream & operator<<(std::ostream & s, irr::video::SColor c);

#include <map>
std::ostream & operator<<(std::ostream & s, std::map<v3s16, unsigned int> & p);

std::ostream & operator<<(std::ostream & s, const std::wstring & w);

struct MapNode;
std::ostream & operator<<(std::ostream & s, MapNode n);

struct NoiseParams;
std::ostream & operator<<(std::ostream & s, NoiseParams np);

namespace Json {
	class Value;
};

std::ostream & operator<<(std::ostream & s, Json::Value & json);

#endif
