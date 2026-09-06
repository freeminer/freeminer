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
#include <memory>

class Mapgen;
class Client;
class FarContainer : public NodeContainer
{
	Client *m_client{};
	int m_surface_depth{2};
	struct Cache;
	std::unique_ptr<Cache> m_cache;
	std::pair<const MapNode, bool> sample(const v3pos_t &p);

public:
	Mapgen *m_mg{};
	bool use_weather{true};
	bool have_params{};
	FarContainer(Client *client);
	// A fresh sampler for each job: misses and received data never leak into
	// subsequent meshes, and workers never share mutable sampling caches.
	FarContainer(const FarContainer &source, const v3pos_t &origin, pos_t side,
			block_step_t step);
	~FarContainer();
	std::pair<const MapNode, bool> getNodeRefAndVisible(const v3pos_t &p) override;
	const MapNode getNodeRefUnsafe(const v3pos_t &p) override
	{
		return getNodeRefAndVisible(p).first;
	};
};
