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

#include "content_abm_core.h"

#include "light.h"
#include "serverenvironment.h"
#include "servermap.h"

#include <algorithm>
#include <cmath>

namespace core_abm
{

double get_number(
		const CoreABMDefinition &definition, const char *name, double default_value)
{
	const auto it = definition.params.find(name);
	if (it == definition.params.end())
		return default_value;
	if (const auto *value = std::get_if<double>(&it->second))
		return *value;
	return default_value;
}

int get_int(const CoreABMDefinition &definition, const char *name, int default_value)
{
	return static_cast<int>(std::lround(get_number(definition, name, default_value)));
}

bool get_bool(
		const CoreABMDefinition &definition, const char *name, bool default_value)
{
	const auto it = definition.params.find(name);
	if (it == definition.params.end())
		return default_value;
	if (const auto *value = std::get_if<bool>(&it->second))
		return *value;
	return default_value;
}

std::string get_string(const CoreABMDefinition &definition,
		const char *name, const std::string &default_value)
{
	const auto it = definition.params.find(name);
	if (it == definition.params.end())
		return default_value;
	if (const auto *value = std::get_if<std::string>(&it->second))
		return *value;
	return default_value;
}

const std::vector<double> *get_number_list(
		const CoreABMDefinition &definition, const char *name)
{
	const auto it = definition.params.find(name);
	return it == definition.params.end() ? nullptr :
			std::get_if<std::vector<double>>(&it->second);
}

const std::vector<std::string> *get_string_list(
		const CoreABMDefinition &definition, const char *name)
{
	const auto it = definition.params.find(name);
	return it == definition.params.end() ? nullptr :
			std::get_if<std::vector<std::string>>(&it->second);
}

namespace
{

bool contains(ParamNames params, const std::string &name)
{
	return std::find(params.begin(), params.end(), name) != params.end();
}

}

bool validate_params_with_lists(const CoreABMDefinition &definition,
		ParamNames number_params, ParamNames bool_params, ParamNames string_params,
		ParamNames number_list_params, ParamNames string_list_params, std::string *error)
{
	for (const auto &[name, value] : definition.params) {
		if (contains(number_params, name)) {
			if (std::holds_alternative<double>(value))
				continue;
		} else if (contains(bool_params, name)) {
			if (std::holds_alternative<bool>(value))
				continue;
		} else if (contains(string_params, name)) {
			if (std::holds_alternative<std::string>(value))
				continue;
		} else if (contains(number_list_params, name)) {
			if (std::holds_alternative<std::vector<double>>(value))
				continue;
		} else if (contains(string_list_params, name)) {
			if (std::holds_alternative<std::vector<std::string>>(value))
				continue;
		} else {
			if (error)
				*error = "unknown parameter '" + name + "' for action '" +
						definition.action + "'";
			return false;
		}

		if (error)
			*error = "invalid type for parameter '" + name + "' in action '" +
					definition.action + "'";
		return false;
	}
	return true;
}

bool validate_params(const CoreABMDefinition &definition, ParamNames number_params,
		ParamNames bool_params, ParamNames string_params, std::string *error)
{
	return validate_params_with_lists(definition, number_params, bool_params,
			string_params, {}, {}, error);
}

u8 get_node_light(ServerEnvironment *env, ServerMap *map,
		const NodeDefManager *ndef, const v3pos_t &p, bool day)
{
	const MapNode node = map->getNodeTry(p);
	if (node.getContent() == CONTENT_IGNORE)
		return 0;
	const auto flags = ndef->getLightingFlags(node);
	return day ? node.getLight(LIGHTBANK_DAY, flags) :
			node.getLightBlend(env->getDayNightRatio(), flags);
}

ConfigurableABM::ConfigurableABM(const CoreABMDefinition &definition) :
		m_definition(definition)
{
}

const std::vector<std::string> &ConfigurableABM::getTriggerContents() const
{
	return m_definition.trigger_contents;
}

const std::vector<std::string> &ConfigurableABM::getRequiredNeighbors(
		uint8_t activate) const
{
	return m_definition.required_neighbors;
}

const std::vector<std::string> &ConfigurableABM::getWithoutNeighbors() const
{
	return m_definition.without_neighbors;
}

float ConfigurableABM::getTriggerInterval()
{
	return m_definition.interval;
}

u32 ConfigurableABM::getTriggerChance()
{
	return m_definition.chance;
}

u32 ConfigurableABM::getNeighborsRange()
{
	return m_definition.neighbors_range;
}

bool ConfigurableABM::getSimpleCatchUp()
{
	return m_definition.catch_up;
}

pos_t ConfigurableABM::getMinY()
{
	return m_definition.min_y;
}

pos_t ConfigurableABM::getMaxY()
{
	return m_definition.max_y;
}

}

ActiveBlockModifier *create_core_abm(const CoreABMDefinition &definition,
		const NodeDefManager *nodedef, std::string *error)
{
	if (definition.trigger_contents.empty()) {
		if (error)
			*error = "nodenames must not be empty";
		return nullptr;
	}

	const std::string &action = definition.action;
	if (action == "water_evaporate" || action == "steam_evaporate")
		return core_abm::create_evaporation(definition, nodedef, error);
	if (action == "rain_fill" || action == "snow_fill" || action == "snow_compact")
		return core_abm::create_precipitation(definition, nodedef, error);
	if (action == "erosion")
		return core_abm::create_erosion(definition, nodedef, error);
	if (action == "soil_weather" || action == "grass_weather" ||
			action == "soil_hydrate")
		return core_abm::create_growth(definition, nodedef, error);

	if (error)
		*error = "unknown core ABM action '" + action + "'";
	return nullptr;
}
