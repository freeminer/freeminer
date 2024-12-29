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

#include <cstdint>
#include <map>
#include <memory>
#include <mutex>
#include <optional>
#include <string>
#include <vector>

class height
{
public:
	using ll_t = float;
	using height_t = float;

protected:
	uint16_t side_length_x = 0, side_length_y = 0, side_length_x_extra = 0; // = 3601;
	uint8_t tile_deg_x{}, tile_deg_y{};
	uint8_t seconds_per_px_x = 0, seconds_per_px_y = 0;
	int lat_loaded = 200;
	int lon_loaded = 200;
	uint16_t pixel_per_deg_x, pixel_per_deg_y;

	std::vector<int16_t> heights;
	static std::mutex mutex;

	virtual int16_t read(uint16_t y, uint16_t x) { return -30000; };
	virtual std::string file_name(ll_t lat, ll_t lon) { return {}; };

protected:
	virtual std::tuple<size_t, size_t, ll_t, ll_t> ll_to_xy(ll_t lat, ll_t lon) = 0;

public:
	virtual bool load(ll_t lat, ll_t lon) { return true; };
	virtual bool ok(ll_t lat, ll_t lon);
	height_t get(ll_t lat, ll_t lon);
	static int lat_start(ll_t lat);
	static int lon_start(ll_t lon);
};
class height_hgt : public height
{
private:
	const std::string folder;
	int lat_loading = 200;
	int lon_loading = 200;

protected:
	std::tuple<size_t, size_t, ll_t, ll_t> ll_to_xy(ll_t lat, ll_t lon) override;
	int16_t read(uint16_t y, uint16_t x) override;

public:
	height_hgt(const std::string &folder, ll_t lat, ll_t lon);
	bool load(ll_t lat, ll_t lon) override;
};

class height_tif final : public height
{
	const std::string folder;
	int lat_loading = 200;
	int lon_loading = 200;

protected:
	std::tuple<size_t, size_t, ll_t, ll_t> ll_to_xy(ll_t lat, ll_t lon) override;

public:
	height_tif(const std::string &folder, ll_t lat, ll_t lon);
	bool load(ll_t lat, ll_t lon) override;
};

class height_gebco_tif final : public height
{
	const std::string folder;
	int lat_loading = 200;
	int lon_loading = 200;
	std::string file_name(ll_t lat, ll_t lon) override;

protected:
	std::tuple<size_t, size_t, ll_t, ll_t> ll_to_xy(ll_t lat, ll_t lon) override;
	int16_t read(uint16_t y, uint16_t x) override;

public:
	height_gebco_tif(const std::string &folder, ll_t lat, ll_t lon);
	bool load(ll_t lat, ll_t lon) override;
	bool ok(ll_t lat, ll_t lon) override;
	static int lat90_start(ll_t lat);
	static int lon90_start(ll_t lon);
};

class height_dummy final : public height
{
protected:
	std::tuple<size_t, size_t, ll_t, ll_t> ll_to_xy(ll_t lat, ll_t lon) override
	{
		return {};
	};

public:
	//height_dummy(ll_t lat, ll_t lon);
	static int lat_start(ll_t lat) { return -300; };
	static int lon_start(ll_t lon) { return -300; };
};

class hgts
{
	std::map<int, std::map<int, std::shared_ptr<height>>> map1, map90;
	const std::string folder;
	std::mutex mutex;

public:
	hgts(const std::string &folder);
	height::height_t get(height::ll_t lat, height::ll_t lon);
};