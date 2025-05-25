// Luanti
// SPDX-License-Identifier: LGPL-2.1-or-later
// Copyright (C) 2013 celeron55, Perttu Ahola <celeron55@gmail.com>

#include "database.h"
#include "irr_v3d.h"
#include "irrlichttypes.h"
#include <sstream>
#include "util/string.h"


/****************
 * The position encoding is a bit messed up because negative
 * values were not taken into account.
 * But this also maps 0,0,0 to 0, which is nice, and we mostly
 * need forward encoding in Luanti.
 */
s64 MapDatabase::getBlockAsInteger(const v3s16 &pos)
{
	return ((s64) pos.Z << 24) + ((s64) pos.Y << 12) + pos.X;
}


v3s16 MapDatabase::getIntegerAsBlock(s64 i)
{
	// Offset so that all negative coordinates become non-negative
	i = i + 0x800800800;
	// Which is now easier to decode using simple bit masks:
	return { (s16)( (i        & 0xFFF) - 0x800),
	         (s16)(((i >> 12) & 0xFFF) - 0x800),
	         (s16)(((i >> 24) & 0xFFF) - 0x800) };
}

std::string MapDatabase::getBlockAsString(const v3bpos_t &pos) {
	std::ostringstream os;
	os << "a" << pos.X << "," << pos.Y << "," << pos.Z;
	return os.str().c_str();
}

v3bpos_t MapDatabase::getStringAsBlock(const std::string &i) {
	std::istringstream is(i);
	v3bpos_t pos;
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
