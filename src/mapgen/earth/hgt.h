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
#include <memory>
#include <mutex>
#include <optional>
#include <string>
#include <vector>

class height
{
protected:
	uint16_t side_length_x = 0, side_length_y = 0; // = 3601;
	uint8_t seconds_per_px = 0;
	int lat_loaded = 200;
	int lon_loaded = 200;
	std::vector<int16_t> heights;
	static std::mutex mutex;

	int16_t read(int16_t y, int16_t x);

public:
	using ll_t = float;
	virtual bool load(int lat_dec, int lon_dec) = 0;
	bool ok(int lat_dec, int lon_dec);
	float get(ll_t lat, ll_t lon);
};
class height_hgt : public height
{
private:
	const std::string folder;
	int lat_loading = 200;
	int lon_loading = 200;

public:
	height_hgt(const std::string &folder, ll_t lat, ll_t lon);
	bool load(int lat_dec, int lon_dec) override;
};

class height_tif : public height
{
	const std::string folder;
	int lat_loading = 200;
	int lon_loading = 200;

public:
	height_tif(const std::string &folder, ll_t lat, ll_t lon);
	bool load(int lat_dec, int lon_dec) override;
};

class hgts
{
	std::map<int, std::map<int, std::unique_ptr<height>>> map;
	const std::string folder;
	std::mutex mutex;

public:
	hgts(const std::string &folder);
	height *get(height::ll_t lat, height::ll_t lon);
};