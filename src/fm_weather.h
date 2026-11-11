/*
Copyright (C) 2026 proller <proler@gmail.com>
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
#include <mutex>
#include "irr_v3d.h"

namespace weather
{
using humidity_t = int16_t;
using heat_t = int16_t;
using wind_t = v3f;

// Every access, including serialization and client rendering, takes a snapshot.
class Wind
{
public:
	operator wind_t() const { return load(); }
	wind_t load() const
	{
		std::lock_guard<std::mutex> lock(m_mutex);
		return m_value;
	}
	Wind &operator=(const wind_t &value)
	{
		std::lock_guard<std::mutex> lock(m_mutex);
		m_value = value;
		return *this;
	}

private:
	mutable std::mutex m_mutex;
	wind_t m_value{};
};

// Protected independently of node locks: ABMs may already hold a node lock.
// The floating values are ambient weather; *_add remain instantaneous node
// contributions owned by ABM analysis. Queries never advance this state.
struct SimulationState
{
	std::mutex mutex;
	float heat = 0.0f;
	float humidity = 0.0f;
	uint32_t last_step = 0;
	bool initialized = false;
	bool wind_initialized = false;
};
}
