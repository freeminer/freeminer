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

#include "fm_nodecontainer.h"
#include "mapblock.h"
#include "threading/concurrent_unordered_map.h"

class Mapgen;
class Client;
class FarContainer : public NodeContainer
{
	Client *m_client{};

public:
	Mapgen *m_mg{};
	bool use_weather{true};
	bool have_params{};
	FarContainer(Client *client);
	std::pair<const MapNode, bool> getNodeRefAndVisible(const v3pos_t &p) override;
	const MapNode getNodeRefUnsafe(const v3pos_t &p) override
	{
		return getNodeRefAndVisible(p).first;
	};
};
