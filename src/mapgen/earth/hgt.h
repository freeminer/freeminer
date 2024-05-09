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

#include <map>
#include <mutex>
#include <optional>
#include <string>
#include <vector>

class hgt
{
public:
	using ll_t = float;

private:
	uint16_t side_length = 0; // = 3601;
	uint8_t seconds_per_px = 0;
	const std::string folder;
	int lat_loaded = 200;
	int lon_loaded = 200;
	int lat_loading = 200;
	int lon_loading = 200;
	std::mutex mutex; //, mutex2;
	std::vector<int16_t> heights;

	bool load(int lat_dec, int lon_dec);
	int16_t read(int16_t y, int16_t x);

public:
	hgt(const std::string &folder, ll_t lat, ll_t lon);
	float get(ll_t lat, ll_t lon);
};

class hgts
{
	std::map<int, std::map<int, std::optional<hgt>>> map;
	const std::string folder;
	std::mutex mutex;

public:
	hgts(const std::string &folder);
	hgt *get(hgt::ll_t lat, hgt::ll_t lon);
};