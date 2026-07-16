/*
Copyright (C) 2026 proller <proler@gmail.com>
*/

/*
This file is part of Freeminer.

Freeminer is free software: you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation, either version 3 of the License, or
(at your option) any later version.

Freeminer is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with Freeminer. If not, see <http://www.gnu.org/licenses/>.
*/

#pragma once

#include "content_abm.h"
#include "server/blockmodifier.h"

#include <initializer_list>
#include <string_view>

class NodeDefManager;
class ServerMap;

namespace core_abm
{

using ParamNames = std::initializer_list<std::string_view>;

double get_number(
		const CoreABMDefinition &definition, const char *name, double default_value);
int get_int(
		const CoreABMDefinition &definition, const char *name, int default_value);
bool get_bool(
		const CoreABMDefinition &definition, const char *name, bool default_value);
std::string get_string(const CoreABMDefinition &definition,
		const char *name, const std::string &default_value);
const std::vector<double> *get_number_list(
		const CoreABMDefinition &definition, const char *name);
const std::vector<std::string> *get_string_list(
		const CoreABMDefinition &definition, const char *name);

bool validate_params(const CoreABMDefinition &definition, ParamNames number_params,
		ParamNames bool_params, ParamNames string_params, std::string *error);
bool validate_params_with_lists(const CoreABMDefinition &definition,
		ParamNames number_params, ParamNames bool_params, ParamNames string_params,
		ParamNames number_list_params, ParamNames string_list_params, std::string *error);

u8 get_node_light(ServerEnvironment *env, ServerMap *map,
		const NodeDefManager *ndef, const v3pos_t &p, bool day);

class ConfigurableABM : public ActiveBlockModifier
{
protected:
	CoreABMDefinition m_definition;

public:
	explicit ConfigurableABM(const CoreABMDefinition &definition);

	const std::vector<std::string> &getTriggerContents() const override;
	const std::vector<std::string> &getRequiredNeighbors(
			uint8_t activate) const override;
	const std::vector<std::string> &getWithoutNeighbors() const override;
	float getTriggerInterval() override;
	u32 getTriggerChance() override;
	u32 getNeighborsRange() override;
	bool getSimpleCatchUp() override;
	pos_t getMinY() override;
	pos_t getMaxY() override;
};

ActiveBlockModifier *create_evaporation(const CoreABMDefinition &definition,
		const NodeDefManager *nodedef, std::string *error);
ActiveBlockModifier *create_precipitation(const CoreABMDefinition &definition,
		const NodeDefManager *nodedef, std::string *error);
ActiveBlockModifier *create_erosion(const CoreABMDefinition &definition,
		const NodeDefManager *nodedef, std::string *error);
ActiveBlockModifier *create_growth(const CoreABMDefinition &definition,
		const NodeDefManager *nodedef, std::string *error);

}
