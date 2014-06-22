/*
convert_json.h
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

#ifndef __CONVERT_JSON_H__
#define __CONVERT_JSON_H__

#include "json/json.h"

struct ModStoreMod;
struct ModStoreModDetails;

std::vector<ModStoreMod>    readModStoreList(Json::Value& modlist);
ModStoreModDetails          readModStoreModDetails(Json::Value& details);

Json::Value                 fetchJsonValue(const std::string &url,
                                           std::vector<std::string> *extra_headers);

#endif
