/*
database.cpp
Copyright (C) 2013 celeron55, Perttu Ahola <celeron55@gmail.com>
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

#include "database.h"
#include "irrlichttypes.h"
#include <sstream>
#include "util/string.h"

static inline s16 unsigned_to_signed(u16 i, u16 max_positive)
{
	if (i < max_positive) {
		return i;
	} else {
		return i - (max_positive * 2);
	}
}


s64 Database::getBlockAsInteger(const v3s16 pos) const
{
	return (((u64) pos.Z) << 24) +
		(((u64) pos.Y) << 12) +
		((u64) pos.X);
}

v3s16 Database::getIntegerAsBlock(const s64 i) const
{
        v3s16 pos;
        pos.Z = unsigned_to_signed((i >> 24) & 0xFFF, 0x1000 / 2);
        pos.Y = unsigned_to_signed((i >> 12) & 0xFFF, 0x1000 / 2);
        pos.X = unsigned_to_signed((i      ) & 0xFFF, 0x1000 / 2);
        return pos;
}

std::string Database::getBlockAsString(const v3s16 &pos) const {
	std::ostringstream os;
	os << "a" << pos.X << "," << pos.Y << "," << pos.Z;
	return os.str().c_str();
}

v3s16 Database::getStringAsBlock(const std::string &i) const {
	std::istringstream is(i);
	v3s16 pos;
	char c;
	if (i[0] == 'a') {
		is >> c; // 'a'
		is >> pos.X;
		is >> c; // ','
		is >> pos.Y;
		is >> c; // ','
		is >> pos.Z;
	} else { // old format
		return getIntegerAsBlock(stoi64(i));
	}
	return pos;
}
