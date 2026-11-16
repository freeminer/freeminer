// SPDX-License-Identifier: LGPL-2.1-or-later
#pragma once

#include "irr_v3d.h"
#include "constants.h"
#include <cmath>
#include <sstream>
#include <string>
#include <utility>
#include <vector>

// Sources use x,y,z,strength,falloff or x,y,z,mass=tonnes,falloff.
// One node is one meter. Strength is acceleration at zero distance.
class FmGravityField
{
public:
	// Parse an acceleration in nodes/s^2.
	static bool parseBackground(const std::string &text, float &result)
	{
		std::istringstream input(text);
		std::string extra;
		double value;
		if (!(input >> value) || (input >> extra) || !std::isfinite(value) || value < 0 ||
				value > 10000)
			return false;
		result = static_cast<float>(value);
		return true;
	}

	bool setSources(const std::string &text)
	{
		std::vector<Source> sources;
		std::istringstream input(text);
		std::string entry;
		while (std::getline(input, entry, ';')) {
			if (entry.find_first_not_of(" \t\r\n") == std::string::npos)
				continue;
			for (char &c : entry)
				if (c == ',')
					c = ' ';
			std::istringstream fields(entry);
			Source s;
			std::string amount, extra;
			if (!(fields >> s.x >> s.y >> s.z >> amount >> s.falloff) ||
					(fields >> extra) || !std::isfinite(s.x) || !std::isfinite(s.y) ||
					!std::isfinite(s.z) || !std::isfinite(s.falloff) ||
					std::abs(s.x) > 1e12 || std::abs(s.y) > 1e12 ||
					std::abs(s.z) > 1e12 || s.falloff < 1e-6 || s.falloff > 1e12 ||
					sources.size() >= 256)
				return false;
			const bool is_mass = amount.compare(0, 5, "mass=") == 0;
			std::istringstream value(is_mass ? amount.substr(5) : amount);
			double number;
			if (!(value >> number) || (value >> extra) || !std::isfinite(number) ||
					(!is_mass && number < 0))
				return false;
			// G * 1000 converts metric tonnes to kg; the existing softened
			// field is G*M / (r*r + falloff*falloff), in nodes/s^2.
			s.strength = is_mass ? (6.6743e-8 * number / s.falloff / s.falloff) : number;
			if (!std::isfinite(s.strength) || std::abs(s.strength) > 10000)
				return false;
			sources.push_back(s);
		}
		m_sources = std::move(sources);
		m_config = text;
		return true;
	}

	bool empty() const { return m_sources.empty(); }
	const std::string &configuration() const { return m_config; }

	// Calculate once per block-sized region using its center in node units.
	v3f sample(const v3bpos_t &block) const
	{
		const double x = double(block.X) * MAP_BLOCKSIZE + (MAP_BLOCKSIZE - 1) / 2.0;
		const double y = double(block.Y) * MAP_BLOCKSIZE + (MAP_BLOCKSIZE - 1) / 2.0;
		const double z = double(block.Z) * MAP_BLOCKSIZE + (MAP_BLOCKSIZE - 1) / 2.0;
		double ax = 0, ay = 0, az = 0;
		for (const Source &s : m_sources) {
			const double dx = s.x - x, dy = s.y - y, dz = s.z - z;
			const double distance = std::hypot(std::hypot(dx, dy), dz);
			if (distance == 0)
				continue;
			const double ratio = distance / s.falloff;
			const double factor = s.strength / (1.0 + ratio * ratio) / distance;
			ax += dx * factor;
			ay += dy * factor;
			az += dz * factor;
		}
		return v3f(ax * BS, ay * BS, az * BS);
	}

	v3f sampleWithBackground(const v3bpos_t &block, float downward) const
	{
		return sample(block) + v3f(0.0f, -downward, 0.0f);
	}

private:
	struct Source
	{
		double x, y, z, strength, falloff;
	};
	std::vector<Source> m_sources;
	std::string m_config;
};
