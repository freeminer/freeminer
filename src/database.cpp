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


/****************
 * Black magic! *
 ****************
 * The position hashing is very messed up.
 * It's a lot more complicated than it looks.
 */

static inline s16 unsigned_to_signed(u16 i, u16 max_positive)
{
	if (i < max_positive) {
		return i;
	} else {
		return i - (max_positive * 2);
	}
}


// Modulo of a negative number does not work consistently in C
static inline s64 pythonmodulo(s64 i, s16 mod)
{
	if (i >= 0) {
		return i % mod;
	}
	return mod - ((-i) % mod);
}


s64 Database::getBlockAsInteger(const v3s16 &pos)
{
	return (u64) pos.Z * 0x1000000 +
		(u64) pos.Y * 0x1000 +
		(u64) pos.X;
}


v3s16 Database::getIntegerAsBlock(s64 i)
{
	v3s16 pos;
	pos.X = unsigned_to_signed(pythonmodulo(i, 4096), 2048);
	i = (i - pos.X) / 4096;
	pos.Y = unsigned_to_signed(pythonmodulo(i, 4096), 2048);
	i = (i - pos.Y) / 4096;
	pos.Z = unsigned_to_signed(pythonmodulo(i, 4096), 2048);
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
