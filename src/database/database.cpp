// Luanti
// SPDX-License-Identifier: LGPL-2.1-or-later
// Copyright (C) 2013 celeron55, Perttu Ahola <celeron55@gmail.com>

#include "database.h"
#include "constants.h"
#include "irr_v3d.h"
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
	}

	return i - (max_positive * 2);
}


// Modulo of a negative number does not work consistently in C
static inline s64 pythonmodulo(s64 i, s16 mod)
{
	if (i >= 0) {
		return i % mod;
	}
	return mod - ((-i) % mod);
}


s64 MapDatabase::getBlockAsInteger(const v3bpos_t &pos)
{
	return (u64) pos.Z * 0x1000000 +
		(u64) pos.Y * 0x1000 +
		(u64) pos.X;
}


v3bpos_t MapDatabase::getIntegerAsBlock(s64 i)
{
	v3bpos_t pos;
	pos.X = unsigned_to_signed(pythonmodulo(i, 4096), 2048);
	i = (i - pos.X) / 4096;
	pos.Y = unsigned_to_signed(pythonmodulo(i, 4096), 2048);
	i = (i - pos.Y) / 4096;
	pos.Z = unsigned_to_signed(pythonmodulo(i, 4096), 2048);
	return pos;
}

std::string MapDatabase::getBlockAsString(const v3bpos_t &pos)
{
    // 'a' is like version marker. In future other letters or words can be used.
	std::ostringstream os;
	os << "a" << pos.X << "," << pos.Y << "," << pos.Z;
	return os.str().c_str();
}

std::string MapDatabase::getBlockAsStringCompatible(const v3bpos_t &pos) const
{
#if USE_POS32	
	const bpos_t max_limit_bp = 31000 / MAP_BLOCKSIZE;
	if (pos.X < -max_limit_bp ||
		pos.X >  max_limit_bp ||
		pos.Y < -max_limit_bp ||
		pos.Y >  max_limit_bp ||
		pos.Z < -max_limit_bp ||
		pos.Z >  max_limit_bp)
		return getBlockAsString(pos);
	return std::to_string(getBlockAsInteger(pos));
#else
	return getBlockAsString(pos);
#endif
}

v3bpos_t MapDatabase::getStringAsBlock(const std::string &i)
{
#if USE_POS32	
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
#else
	return getIntegerAsBlock(stoi64(i));
#endif
}
