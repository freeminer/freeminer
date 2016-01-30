/*
serialization.h
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

#ifndef SERIALIZATION_HEADER
#define SERIALIZATION_HEADER

#include "irrlichttypes.h"
#include "exceptions.h"
#include <iostream>
#include "util/pointer.h"

/*
	Map format serialization version
	--------------------------------

	For map data (blocks, nodes, sectors).

	NOTE: The goal is to increment this so that saved maps will be
	      loadable by any version. Other compatibility is not
		  maintained.

	0: original networked test with 1-byte nodes
	1: update with 2-byte nodes
	2: lighting is transmitted in param
	3: optional fetching of far blocks
	4: block compression
	5: sector objects NOTE: block compression was left accidentally out
	6: failed attempt at switching block compression on again
	7: block compression switched on again
	8: server-initiated block transfers and all kinds of stuff
	9: block objects
	10: water pressure
	11: zlib'd blocks, block flags
	12: UnlimitedHeightmap now uses interpolated areas
	13: Mapgen v2
	14: NodeMetadata
	15: StaticObjects
	16: larger maximum size of node metadata, and compression
	17: MapBlocks contain timestamp
	18: new generator (not really necessary, but it's there)
	19: new content type handling
	20: many existing content types translated to extended ones
	21: dynamic content type allocation
	22: minerals removed, facedir & wallmounted changed
	23: new node metadata format
	24: 16-bit node ids and node timers (never released as stable)
	25: Improved node timer format
	26: Never written; read the same as 25
*/
// This represents an uninitialized or invalid format
#define SER_FMT_VER_INVALID 255
// Highest supported serialization version
#define SER_FMT_VER_HIGHEST_READ 26
// Saved on disk version
#define SER_FMT_VER_HIGHEST_WRITE 25
// Lowest supported serialization version
#define SER_FMT_VER_LOWEST_READ 0
// Lowest serialization version for writing
// Can't do < 24 anymore; we have 16-bit dynamically allocated node IDs
// in memory; conversion just won't work in this direction.
#define SER_FMT_VER_LOWEST_WRITE 24

inline bool ser_ver_supported(s32 v) {
	return v >= SER_FMT_VER_LOWEST_READ && v <= SER_FMT_VER_HIGHEST_READ;
}

/*
	Misc. serialization functions
*/

void compressZlib(SharedBuffer<u8> data, std::ostream &os, int level = 2);
void compressZlib(const std::string &data, std::ostream &os, int level = 2);
void decompressZlib(std::istream &is, std::ostream &os);

// These choose between zlib and a self-made one according to version
void compress(SharedBuffer<u8> data, std::ostream &os, u8 version);
//void compress(const std::string &data, std::ostream &os, u8 version);
void decompress(std::istream &is, std::ostream &os, u8 version);

//freeminer:
void compressZlib(const std::string &data, std::string &os, int level = 2);
void decompressZlib(const std::string &is, std::string &os);

#endif

