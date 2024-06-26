/*
convert_json.cpp
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

#include "convert_json.h"

#include <json/json.h>

#include <iostream>
#include <sstream>
#include <memory>

void fastWriteJson(const Json::Value &value, std::ostream &to)
{
	Json::StreamWriterBuilder builder;
	builder["indentation"] = "";
	std::unique_ptr<Json::StreamWriter> writer(builder.newStreamWriter());
	writer->write(value, &to);
}

std::string fastWriteJson(const Json::Value &value)
{
	std::ostringstream oss;
	fastWriteJson(value, oss);
	return oss.str();
}
