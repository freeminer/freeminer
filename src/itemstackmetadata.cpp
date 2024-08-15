/*
Minetest
Copyright (C) 2017-8 rubenwardy <rw@rubenwardy.com>

This program is free software; you can redistribute it and/or modify
it under the terms of the GNU Lesser General Public License as published by
the Free Software Foundation; either version 2.1 of the License, or
(at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU Lesser General Public License for more details.

You should have received a copy of the GNU Lesser General Public License along
with this program; if not, write to the Free Software Foundation, Inc.,
51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
*/


#include "itemstackmetadata.h"
#include "util/serialize.h"
#include "util/strfnd.h"

#include <algorithm>
#include <optional>

#define DESERIALIZE_START '\x01'
#define DESERIALIZE_KV_DELIM '\x02'
#define DESERIALIZE_PAIR_DELIM '\x03'
#define DESERIALIZE_START_STR "\x01"
#define DESERIALIZE_KV_DELIM_STR "\x02"
#define DESERIALIZE_PAIR_DELIM_STR "\x03"

#define TOOLCAP_KEY "tool_capabilities"
#define WEAR_BAR_KEY "wear_color"

void ItemStackMetadata::clear()
{
	SimpleMetadata::clear();
	updateToolCapabilities();
	updateWearBarParams();
}

static void sanitize_string(std::string &str)
{
	str.erase(std::remove(str.begin(), str.end(), DESERIALIZE_START), str.end());
	str.erase(std::remove(str.begin(), str.end(), DESERIALIZE_KV_DELIM), str.end());
	str.erase(std::remove(str.begin(), str.end(), DESERIALIZE_PAIR_DELIM), str.end());
}

bool ItemStackMetadata::setString(const std::string &name, std::string_view var)
{
	std::string clean_name = name;
	std::string clean_var(var);
	sanitize_string(clean_name);
	sanitize_string(clean_var);

	bool result = SimpleMetadata::setString(clean_name, clean_var);
	if (clean_name == TOOLCAP_KEY)
		updateToolCapabilities();
	else if (clean_name == WEAR_BAR_KEY)
		updateWearBarParams();
	return result;
}

void ItemStackMetadata::serialize(std::ostream &os) const
{
	std::ostringstream os2(std::ios_base::binary);
	os2 << DESERIALIZE_START;
	for (const auto &stringvar : m_stringvars) {
		if (!stringvar.first.empty() || !stringvar.second.empty())
			os2 << stringvar.first << DESERIALIZE_KV_DELIM
				<< stringvar.second << DESERIALIZE_PAIR_DELIM;
	}
	os << serializeJsonStringIfNeeded(os2.str());
}

void ItemStackMetadata::deSerialize(std::istream &is)
{
	std::string in = deSerializeJsonStringIfNeeded(is);

	m_stringvars.clear();

	if (!in.empty()) {
		if (in[0] == DESERIALIZE_START) {
			Strfnd fnd(in);
			fnd.to(1);
			while (!fnd.at_end()) {
				std::string name = fnd.next(DESERIALIZE_KV_DELIM_STR);
				std::string var  = fnd.next(DESERIALIZE_PAIR_DELIM_STR);
				m_stringvars[name] = var;
			}
		} else {
			// BACKWARDS COMPATIBILITY
			m_stringvars[""] = in;
		}
	}
	updateToolCapabilities();
	updateWearBarParams();
}

void ItemStackMetadata::updateToolCapabilities()
{
	if (contains(TOOLCAP_KEY)) {
		toolcaps_overridden = true;
		toolcaps_override = ToolCapabilities();
		std::istringstream is(getString(TOOLCAP_KEY));
		toolcaps_override.deserializeJson(is);
	} else {
		toolcaps_overridden = false;
	}
}

void ItemStackMetadata::setToolCapabilities(const ToolCapabilities &caps)
{
	std::ostringstream os;
	caps.serializeJson(os);
	setString(TOOLCAP_KEY, os.str());
}

void ItemStackMetadata::clearToolCapabilities()
{
	setString(TOOLCAP_KEY, "");
}

void ItemStackMetadata::updateWearBarParams()
{
	if (contains(WEAR_BAR_KEY)) {
		std::istringstream is(getString(WEAR_BAR_KEY));
		wear_bar_override = WearBarParams::deserializeJson(is);
	} else {
		wear_bar_override.reset();
	}
}

void ItemStackMetadata::setWearBarParams(const WearBarParams &params)
{
	std::ostringstream os;
	params.serializeJson(os);
	setString(WEAR_BAR_KEY, os.str());
}

void ItemStackMetadata::clearWearBarParams()
{
	setString(WEAR_BAR_KEY, "");
}
