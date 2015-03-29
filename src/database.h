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

#include <vector>
#include <string>
#include "irr_v3d.h"
#include "irrlichttypes.h"
#include <string>

#ifndef PP
	#define PP(x) "("<<(x).X<<","<<(x).Y<<","<<(x).Z<<")"
#endif

class Database
{
public:
	virtual ~Database() {}

	virtual void beginSave() {}
	virtual void endSave() {}

	virtual bool saveBlock(const v3s16 &pos, const std::string &data) = 0;
	virtual std::string loadBlock(const v3s16 &pos) = 0;
	virtual bool deleteBlock(const v3s16 &pos) = 0;

	static s64 getBlockAsInteger(const v3s16 &pos);
	static v3s16 getIntegerAsBlock(s64 i);

	virtual void listAllLoadableBlocks(std::vector<v3s16> &dst) = 0;

	virtual bool initialized() const { return true; }


	std::string getBlockAsString(const v3POS &pos) const;
	v3POS getStringAsBlock(const std::string &i) const;
	virtual void open() {};
	virtual void close() {};
};

#endif

