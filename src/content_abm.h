/*
content_abm.h
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
#pragma once

#include <cstdint>
#include <limits>
#include <string>
#include <unordered_map>
#include <variant>
#include <vector>

class ActiveBlockModifier;
class ServerEnvironment;
class NodeDefManager;

using CoreABMParam = std::variant<bool, double, std::string,
		std::vector<double>, std::vector<std::string>>;

struct CoreABMDefinition
{
	std::string name;
	std::string action;
	std::vector<std::string> trigger_contents;
	std::vector<std::string> required_neighbors;
	std::vector<std::string> without_neighbors;
	float interval = 10.0f;
	uint32_t chance = 50;
	uint16_t neighbors_range = 1;
	bool catch_up = true;
	int16_t min_y = std::numeric_limits<int16_t>::min();
	int16_t max_y = std::numeric_limits<int16_t>::max();
	std::unordered_map<std::string, CoreABMParam> params;
};

/*
	Legacy ActiveBlockModifiers
*/

void add_fast_abms(ServerEnvironment *env, NodeDefManager *nodedef);

ActiveBlockModifier *create_core_abm(
		const CoreABMDefinition &definition, const NodeDefManager *nodedef,
		std::string *error = nullptr);
