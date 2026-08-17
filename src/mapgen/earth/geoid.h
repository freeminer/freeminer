#pragma once

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <limits>
#include <memory>
#include <mutex>
#include <string>
#include <vector>

namespace earth
{
namespace geoid_detail
{

struct Grid
{
	bool loaded = false;
	std::string source_path;
	double min_lat = 0.0;
	double min_lon = 0.0;
	double d_lat = 0.0;
	double d_lon = 0.0;
	std::uint32_t rows = 0;
	std::uint32_t cols = 0;
	std::vector<float> values;
};

inline double read_be_double(const unsigned char *p)
{
	std::uint64_t bits = 0;
	for (int i = 0; i < 8; ++i)
		bits = (bits << 8) | p[i];
	double out = 0.0;
	std::memcpy(&out, &bits, sizeof(out));
	return out;
}

inline std::uint32_t read_be_u32(const unsigned char *p)
{
	return (static_cast<std::uint32_t>(p[0]) << 24) |
		   (static_cast<std::uint32_t>(p[1]) << 16) |
		   (static_cast<std::uint32_t>(p[2]) << 8) | static_cast<std::uint32_t>(p[3]);
}

inline float read_be_float(const unsigned char *p)
{
	std::uint32_t bits = read_be_u32(p);
	float out = 0.0f;
	std::memcpy(&out, &bits, sizeof(out));
	return out;
}

inline std::vector<std::string> geoid_paths(const std::string &preferred_path)
{
	std::vector<std::string> paths;
	if (!preferred_path.empty())
		paths.emplace_back(preferred_path);
	if (const char *proj_data = std::getenv("PROJ_DATA"))
		paths.emplace_back(std::string(proj_data) + "/egm96_15.gtx");
	if (const char *proj_lib = std::getenv("PROJ_LIB"))
		paths.emplace_back(std::string(proj_lib) + "/egm96_15.gtx");
	paths.emplace_back("/usr/share/proj/egm96_15.gtx");
	paths.emplace_back("/usr/local/share/proj/egm96_15.gtx");
	return paths;
}

inline Grid load_grid(const std::string &preferred_path = {})
{
	Grid grid;
	for (const std::string &path : geoid_paths(preferred_path)) {
		std::ifstream in(path, std::ios::binary);
		if (!in)
			continue;

		unsigned char header[40];
		in.read(reinterpret_cast<char *>(header), sizeof(header));
		if (in.gcount() != static_cast<std::streamsize>(sizeof(header)))
			continue;

		grid.min_lat = read_be_double(header);
		grid.min_lon = read_be_double(header + 8);
		grid.d_lat = read_be_double(header + 16);
		grid.d_lon = read_be_double(header + 24);
		grid.rows = read_be_u32(header + 32);
		grid.cols = read_be_u32(header + 36);
		if (grid.rows < 2 || grid.cols < 2 || grid.d_lat <= 0.0 || grid.d_lon <= 0.0)
			continue;

		const size_t count = static_cast<size_t>(grid.rows) * grid.cols;
		std::vector<unsigned char> raw(count * sizeof(float));
		in.read(reinterpret_cast<char *>(raw.data()), raw.size());
		if (in.gcount() != static_cast<std::streamsize>(raw.size()))
			continue;

		grid.values.resize(count);
		for (size_t i = 0; i < count; ++i)
			grid.values[i] = read_be_float(raw.data() + i * sizeof(float));
		grid.loaded = true;
		grid.source_path = path;
		return grid;
	}
	return {};
}

struct GridCache
{
	std::mutex mutex;
	std::string preferred_path;
	std::shared_ptr<const Grid> grid;
	bool attempted = false;
};

inline GridCache &grid_cache()
{
	static GridCache cache;
	return cache;
}

inline std::shared_ptr<const Grid> grid()
{
	GridCache &cache = grid_cache();
	const std::lock_guard<std::mutex> lock(cache.mutex);
	if (!cache.attempted) {
		cache.grid = std::make_shared<const Grid>(load_grid(cache.preferred_path));
		cache.attempted = true;
	}
	return cache.grid;
}

} // namespace geoid_detail

// Set a cache-file candidate before the first lookup. System PROJ locations are
// still searched if this file is absent or invalid.
inline void set_geoid_grid_path(const std::string &path)
{
	geoid_detail::GridCache &cache = geoid_detail::grid_cache();
	const std::lock_guard<std::mutex> lock(cache.mutex);
	if (cache.preferred_path == path)
		return;
	cache.preferred_path = path;
	if (!cache.grid || !cache.grid->loaded)
		cache.attempted = false;
}

// Retry loading after a missing grid has been downloaded.
inline bool reload_geoid_grid()
{
	geoid_detail::GridCache &cache = geoid_detail::grid_cache();
	const std::lock_guard<std::mutex> lock(cache.mutex);
	cache.grid = std::make_shared<const geoid_detail::Grid>(
			geoid_detail::load_grid(cache.preferred_path));
	cache.attempted = true;
	return cache.grid->loaded;
}

inline bool geoid_grid_loaded()
{
	const auto grid = geoid_detail::grid();
	return grid && grid->loaded;
}

inline std::string geoid_grid_path()
{
	const auto grid = geoid_detail::grid();
	return grid ? grid->source_path : std::string{};
}

inline double geoid_undulation_m(double lat, double lon)
{
	const auto grid = geoid_detail::grid();
	if (!grid || !grid->loaded)
		return 0.0;
	const geoid_detail::Grid &g = *grid;

	while (lon < g.min_lon)
		lon += 360.0;
	while (lon >= g.min_lon + 360.0)
		lon -= 360.0;

	const double y = (lat - g.min_lat) / g.d_lat;
	if (y < 0.0 || y > static_cast<double>(g.rows - 1))
		return 0.0;

	const double x = (lon - g.min_lon) / g.d_lon;
	const auto row0 = static_cast<std::uint32_t>(
			std::clamp(std::floor(y), 0.0, static_cast<double>(g.rows - 1)));
	const auto row1 = std::min<std::uint32_t>(row0 + 1, g.rows - 1);
	const auto col0 = static_cast<std::uint32_t>(
			std::clamp(std::floor(x), 0.0, static_cast<double>(g.cols - 1)));
	const auto col1 = (col0 + 1) % g.cols;
	const double fy = y - row0;
	const double fx = x - std::floor(x);

	auto at = [&](std::uint32_t row, std::uint32_t col) {
		return static_cast<double>(g.values[static_cast<size_t>(row) * g.cols + col]);
	};

	const double v00 = at(row0, col0);
	const double v10 = at(row0, col1);
	const double v01 = at(row1, col0);
	const double v11 = at(row1, col1);
	return (1.0 - fx) * (1.0 - fy) * v00 + fx * (1.0 - fy) * v10 + (1.0 - fx) * fy * v01 +
		   fx * fy * v11;
}

inline double ellipsoid_to_orthometric_height(double lat, double lon, double ellipsoid_h)
{
	return ellipsoid_h - geoid_undulation_m(lat, lon);
}

inline double orthometric_to_ellipsoid_height(
		double lat, double lon, double orthometric_h)
{
	return orthometric_h + geoid_undulation_m(lat, lon);
}

} // namespace earth
