/*
database.h
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

#ifndef DATABASE_HEADER
#define DATABASE_HEADER

#include <list>
#include "irr_v3d.h"
#include "irrlichttypes.h"
#include <string>

class MapBlock;

class Database
{
public:
	virtual void beginSave()=0;
	virtual void endSave()=0;

	virtual void saveBlock(MapBlock *block)=0;
	virtual MapBlock* loadBlock(v3s16 blockpos)=0;
	s64 getBlockAsInteger(const v3s16 pos) const;
	v3s16 getIntegerAsBlock(s64 i) const;
	std::string getBlockAsString(const v3s16 &pos) const;
	v3s16 getStringAsBlock(const std::string &i) const;
	virtual void listAllLoadableBlocks(std::list<v3s16> &dst)=0;
	virtual int Initialized(void)=0;
	virtual ~Database() {};
};
#endif
