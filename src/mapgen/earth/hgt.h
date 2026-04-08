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

#include <cmath>
#include <cstdint>
#include <functional>
#include <map>
#include <memory>
#include <mutex>
#include <string>
#include <vector>
#include <unordered_map>
#include <thread>

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

#if 0
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
#endif

class height_seabed_tif final : public height
{
	const std::string folder;
	int lat_loading = 200;
	int lon_loading = 200;
	std::string file_name(ll_t lat, ll_t lon) override;

protected:
	std::tuple<size_t, size_t, ll_t, ll_t> ll_to_xy(ll_t lat, ll_t lon) override;
	int16_t read(uint16_t y, uint16_t x) override;

public:
	height_seabed_tif(const std::string &folder, ll_t lat, ll_t lon);
	bool load(ll_t lat, ll_t lon) override;
	bool ok(ll_t lat, ll_t lon) override;
	static int lat_start(ll_t lat) { return floor(lat); }
	static int lon_start(ll_t lon) { return floor(lon); }
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
	std::map<int, std::map<int, std::shared_ptr<height>>> map1, map1_seabed; //, map90;
	const std::string folder;
	std::mutex mutex;

	// Thread-local cache for container access
	struct ThreadLocalContainerCache {
		std::unordered_map<uint64_t, std::weak_ptr<height>> map1_cache;
		std::unordered_map<uint64_t, std::weak_ptr<height>> map1_seabed_cache;
	};
	
	static thread_local ThreadLocalContainerCache tl_container_cache;

	// Cache key generation for container access (lat/lon tile coordinates)
	static uint64_t make_tile_key(int lat, int lon) {
		return (uint64_t(static_cast<uint32_t>(lat)) << 32) | uint64_t(static_cast<uint32_t>(lon));
	}

	// Layer definition structure
	struct Layer {
		std::map<int, std::map<int, std::shared_ptr<height>>>& container;
		std::unordered_map<uint64_t, std::weak_ptr<height>>& cache;
		std::function<std::shared_ptr<height>(const std::string&, height::ll_t, height::ll_t)> factory;
		std::function<height::height_t(height::ll_t)> post_process;
		std::function<void(std::map<int, std::map<int, std::shared_ptr<height>>>&, 
						  std::unordered_map<uint64_t, std::weak_ptr<height>>&, 
						  int, int)> place_dummy;
		height::height_t min_height;
		height::height_t max_height;
	};
	
	// Get layer definitions
	std::vector<Layer> get_layers(const height::ll_t lat, const height::ll_t lon);

public:
	hgts(const std::string &folder);
	height::height_t get(const height::ll_t lat, const height::ll_t lon);
};
