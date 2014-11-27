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

#include <ctime>

#include "stat.h"
//#include "log.h"


Stat::Stat(std::string savedir) :
	database(savedir, "stat") {
	update_time();
};

Stat::~Stat() {
	save();
};

void Stat::save() {
	for(const auto & ir : stats) {
		//errorstream<<"stat saving: "<<ir.first<< " = "<< ir.second<<std::endl;
		if (ir.second)
			database.put(ir.first, ir.second);
	}
	update_time();
}

void Stat::unload() {
	save();
	stats.clear();
}

void Stat::open() {
	database.open();
}

void Stat::close() {
	unload();
	database.close();
}

stat_value Stat::get(const std::string & key) {
	if (!stats.count(key))
		database.get(key, stats[key]);
	//errorstream<<"stat get: "<<key<<" = "<< stats[key]<<std::endl;
	return stats[key];
}

stat_value Stat::write_one(const std::string & key, const stat_value & value) {
	//errorstream<<"stat one: "<<key<< " = "<< value<<std::endl;
	get(key);
	return stats[key] += value;
}

stat_value Stat::add(const std::string & key, const std::string & player, stat_value value) {
	//errorstream<<"stat adding: "<<key<< " player="<<player<<" = "<< value<<std::endl;
	stat_value ret = write_one("total|" + key, value);
	write_one("day|"+ key + "|" + day , value);
	write_one("week|"+ key + "|" + week, value);
	write_one("month|"+ key + "|" + month, value);
	if (!player.empty())
		ret = write_one("player|" + key  + "|" + player, value);
	return ret;
}

void Stat::update_time() {
	auto t = time(NULL);
	auto tm = localtime(&t);
	char cs[20];
	strftime(cs, 20, "%Y_%m", tm);
	month = cs;
	strftime(cs, 20, "%Y_%W", tm);
	week = cs;
	strftime(cs, 20, "%Y_%j", tm);
	day = cs;
}
