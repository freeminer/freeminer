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

//#include <ctime>

//#include "stat.h"
//#include "gettime.h"
//#include "log.h"

#include "fm_key_value_cached.h"

KeyValueCached::KeyValueCached(const std::string &savedir, const std::string &name) :
		database(savedir, name){};

KeyValueCached::~KeyValueCached()
{
	save();
};

void KeyValueCached::save()
{
	std::lock_guard<std::mutex> lock(mutex);
	for (const auto &ir : stats) {
		//errorstream<<"stat saving: "<<ir.first<< " = "<< ir.second<<std::endl;
		if (ir.second.empty()) {
			database.del(ir.first);
		} else {
			database.put(ir.first, ir.second);
		}
	}
}

void KeyValueCached::unload()
{
	save();
	std::lock_guard<std::mutex> lock(mutex);
	stats.clear();
}

void KeyValueCached::open()
{
	database.open();
}

void KeyValueCached::close()
{
	unload();
	database.close();
}

const std::string & KeyValueCached::get(const std::string &key)
{
	std::lock_guard<std::mutex> lock(mutex);
	if (!stats.contains(key))
		database.get(key, stats[key]);
	//errorstream<<"stat get: "<<key<<" = "<< stats[key]<<std::endl;
	return stats[key];
}

const std::string & KeyValueCached::put(const std::string &key, const std::string &value)
{
	//errorstream<<"stat one: "<<key<< " = "<< value<<std::endl;
	std::lock_guard<std::mutex> lock(mutex);
	return stats[key] = value;
}
