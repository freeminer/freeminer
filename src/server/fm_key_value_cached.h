/*
Copyright (C) 2024 proller <proler@gmail.com>
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

#pragma once

#include <unordered_map>
#include "key_value_storage.h"

class KeyValueCached
{
public:
	KeyValueStorage database;
	std::unordered_map<std::string, std::string> stats;

	KeyValueCached(const std::string &savedir, const std::string &name);
	~KeyValueCached();

	void save();
	void unload();
	void open();
	void close();

	const std::string &get(const std::string &key);
	const std::string &put(const std::string &key, const std::string &value);

private:
	std::mutex mutex;
};
