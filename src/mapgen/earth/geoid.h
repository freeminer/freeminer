#pragma once

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <limits>
#include <string>
#include <vector>

namespace earth {
namespace geoid_detail {

struct Grid
{
	bool loaded = false;
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
			(static_cast<std::uint32_t>(p[2]) << 8) |
			static_cast<std::uint32_t>(p[3]);
}

inline float read_be_float(const unsigned char *p)
{
	std::uint32_t bits = read_be_u32(p);
	float out = 0.0f;
	std::memcpy(&out, &bits, sizeof(out));
	return out;
}

inline std::vector<std::string> geoid_paths()
{
	std::vector<std::string> paths;
	if (const char *proj_data = std::getenv("PROJ_DATA"))
		paths.emplace_back(std::string(proj_data) + "/egm96_15.gtx");
	if (const char *proj_lib = std::getenv("PROJ_LIB"))
		paths.emplace_back(std::string(proj_lib) + "/egm96_15.gtx");
	paths.emplace_back("/usr/share/proj/egm96_15.gtx");
	paths.emplace_back("/usr/local/share/proj/egm96_15.gtx");
	return paths;
}

inline Grid load_grid()
{
	Grid grid;
	for (const std::string &path : geoid_paths()) {
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
		return grid;
	}
	return {};
}

inline const Grid &grid()
{
	static const Grid g = load_grid();
	return g;
}

} // namespace geoid_detail

inline double geoid_undulation_m(double lat, double lon)
{
	const geoid_detail::Grid &g = geoid_detail::grid();
	if (!g.loaded)
		return 0.0;

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
	return (1.0 - fx) * (1.0 - fy) * v00 +
			fx * (1.0 - fy) * v10 +
			(1.0 - fx) * fy * v01 +
			fx * fy * v11;
}

inline double ellipsoid_to_orthometric_height(double lat, double lon, double ellipsoid_h)
{
	return ellipsoid_h - geoid_undulation_m(lat, lon);
}

inline double orthometric_to_ellipsoid_height(double lat, double lon, double orthometric_h)
{
	return orthometric_h + geoid_undulation_m(lat, lon);
}

} // namespace earth
