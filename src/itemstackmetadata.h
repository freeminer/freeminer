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

#pragma once

#include "metadata.h"
#include "tool.h"

#include <optional>

class Inventory;
class IItemDefManager;

class ItemStackMetadata : public SimpleMetadata
{
public:
	ItemStackMetadata():
			toolcaps_overridden(false)
	{}

	// Overrides
	void clear() override;
	bool setString(const std::string &name, std::string_view var) override;

	void serialize(std::ostream &os) const;
	void deSerialize(std::istream &is);

	const ToolCapabilities &getToolCapabilities(
			const ToolCapabilities &default_caps) const
	{
		return toolcaps_overridden ? toolcaps_override : default_caps;
	}

	void setToolCapabilities(const ToolCapabilities &caps);
	void clearToolCapabilities();

	const std::optional<WearBarParams> &getWearBarParamOverride() const
	{
		return wear_bar_override;
	}


	void setWearBarParams(const WearBarParams &params);
	void clearWearBarParams();

private:
	void updateToolCapabilities();
	void updateWearBarParams();

	bool toolcaps_overridden;
	ToolCapabilities toolcaps_override;
	std::optional<WearBarParams> wear_bar_override;
};
