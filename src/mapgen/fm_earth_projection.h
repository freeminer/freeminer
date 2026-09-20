// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include "irr_v3d.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <stdexcept>
#include <string>
#include <type_traits>
#include <optional>
#include <vector>

// Geographic angles are degrees; altitude, origin and radii are node units.
// Curved projections use Y as the polar axis, longitude zero along +X.
namespace fm_earth
{
constexpr double pi = 3.14159265358979323846;
constexpr double degrees = 180.0 / pi;
constexpr double metres_per_degree = 40075696.0 / 360.0;

struct Sample
{
	double lat, lon, altitude;
};

struct Region
{
	double min_lat, max_lat, min_lon, max_lon;
};

struct Parameters
{
	v3opos_t origin{0, 0, 0};
	v3d scale{1, 1, 1};
	v3d center{0, 0, 0}; // Legacy flat geographic offset and elevation offset.
	double radius = 10000;
	double major_radius = 20000; // Torus ring radius; radius is the tube radius.
};

inline double wrap(double angle)
{
	return angle - 360.0 * std::floor((angle + 180.0) / 360.0);
}

inline v3d direction(double lat, double lon)
{
	const double a = lat / degrees, b = lon / degrees;
	return {std::cos(a) * std::cos(b), std::sin(a), std::cos(a) * std::sin(b)};
}

inline double max_abs(const v3d &p)
{
	return std::max({std::abs(p.X), std::abs(p.Y), std::abs(p.Z)});
}

inline double nearest_zero(double lo, double hi)
{
	return lo > 0 ? lo : (hi < 0 ? hi : 0);
}

// Conservative angle coverage of a rectangle, split at the -180/180 seam.
inline std::vector<std::array<double, 2>> angle_ranges(
		double xlo, double xhi, double ylo, double yhi)
{
	if (xlo <= 0 && xhi >= 0 && ylo <= 0 && yhi >= 0)
		return {{-180, 180}};
	std::array<double, 4> a{wrap(std::atan2(ylo, xlo) * degrees),
			wrap(std::atan2(yhi, xlo) * degrees), wrap(std::atan2(ylo, xhi) * degrees),
			wrap(std::atan2(yhi, xhi) * degrees)};
	std::sort(a.begin(), a.end());
	double gap = -1;
	size_t end = 0;
	for (size_t i = 0; i < a.size(); ++i) {
		const double next = i + 1 < a.size() ? a[i + 1] : a[0] + 360;
		if (next - a[i] > gap) {
			gap = next - a[i];
			end = i;
		}
	}
	const double lo = a[(end + 1) % a.size()], hi = a[end];
	if (lo <= hi)
		return {{lo, hi}};
	return {{lo, 180}, {-180, hi}};
}

struct Flat
{
	Parameters p;
	double altitude(const v3opos_t &world) const { return world.Y; }
	std::optional<double> top(double, double, double altitude) const { return altitude; }
	Sample sample(const v3opos_t &world) const
	{
		const double lon = world.X * p.scale.X / metres_per_degree + p.center.X;
		const double lat = world.Z * p.scale.Z / metres_per_degree + p.center.Z;
		// Preserve the existing out-of-range flat map behavior.
		if (lat <= -90 || lat >= 90 || lon <= -180 || lon >= 180)
			return {89.9999, 0, static_cast<double>(world.Y)};
		return {lat, lon, static_cast<double>(world.Y)};
	}
	v3opos_t place(double lat, double lon, double altitude) const
	{
		return {static_cast<opos_t>((lon - p.center.X) * metres_per_degree / p.scale.X),
				static_cast<opos_t>(altitude),
				static_cast<opos_t>((lat - p.center.Z) * metres_per_degree / p.scale.Z)};
	}
	v3d up(const v3opos_t &) const { return {0, 1, 0}; }
	std::vector<Region> coverage(const v3opos_t &lo, const v3opos_t &hi) const
	{
		const auto a = sample(lo), b = sample(hi);
		const double lat_lo = lo.Z * p.scale.Z / metres_per_degree + p.center.Z;
		const double lat_hi = hi.Z * p.scale.Z / metres_per_degree + p.center.Z;
		const double lon_lo = lo.X * p.scale.X / metres_per_degree + p.center.X;
		const double lon_hi = hi.X * p.scale.X / metres_per_degree + p.center.X;
		if (std::min(lat_lo, lat_hi) <= -90 || std::max(lat_lo, lat_hi) >= 90 ||
				std::min(lon_lo, lon_hi) <= -180 || std::max(lon_lo, lon_hi) >= 180)
			return {{-90, 90, -180, 180}};
		return {{std::min(a.lat, b.lat), std::max(a.lat, b.lat), std::min(a.lon, b.lon),
				std::max(a.lon, b.lon)}};
	}
};

struct Sphere
{
	Parameters p;
	explicit Sphere(const Parameters &params) : p(params) {}
	v3d relative(const v3opos_t &world) const
	{
		return {double(world.X) - p.origin.X, double(world.Y) - p.origin.Y,
				double(world.Z) - p.origin.Z};
	}
	v3opos_t world(const v3d &q) const
	{
		return {static_cast<opos_t>(q.X + p.origin.X),
				static_cast<opos_t>(q.Y + p.origin.Y),
				static_cast<opos_t>(q.Z + p.origin.Z)};
	}
	double altitude(const v3opos_t &pos) const
	{
		return relative(pos).getLength() - p.radius;
	}
	std::optional<double> top(double x, double z, double altitude) const
	{
		const double r = p.radius + altitude;
		const double d = std::hypot(x - p.origin.X, z - p.origin.Z);
		if (r < 0 || d > r)
			return {};
		return p.origin.Y + std::sqrt(std::max(0.0, r * r - d * d));
	}
	Sample sample(const v3opos_t &pos) const
	{
		const auto q = relative(pos);
		return {std::atan2(q.Y, std::hypot(q.X, q.Z)) * degrees,
				wrap(std::atan2(q.Z, q.X) * degrees), q.getLength() - p.radius};
	}
	v3opos_t place(double lat, double lon, double altitude) const
	{
		return world(direction(lat, lon) * (p.radius + altitude));
	}
	v3d up(const v3opos_t &pos) const
	{
		auto q = relative(pos);
		return q.getLengthSQ() > 0 ? q.normalize() : v3d(0, 1, 0);
	}
	std::vector<Region> coverage(const v3opos_t &lo, const v3opos_t &hi) const
	{
		const auto a = relative(lo), b = relative(hi);
		const double rmin = std::hypot(nearest_zero(a.X, b.X), nearest_zero(a.Z, b.Z));
		const double rmax = std::hypot(std::max(std::abs(a.X), std::abs(b.X)),
				std::max(std::abs(a.Z), std::abs(b.Z)));
		double lat_min = 90, lat_max = -90;
		for (double y : {a.Y, b.Y})
			for (double r : {rmin, rmax}) {
				const double lat = std::atan2(y, r) * degrees;
				lat_min = std::min(lat_min, lat);
				lat_max = std::max(lat_max, lat);
			}
		if (rmin == 0 && a.Y <= 0 && b.Y >= 0) {
			lat_min = -90;
			lat_max = 90;
		}
		std::vector<Region> result;
		for (const auto &lon : angle_ranges(a.X, b.X, a.Z, b.Z))
			result.push_back({lat_min, lat_max, lon[0], lon[1]});
		return result;
	}
};

// Radial cube map: geographic direction is unchanged, altitude is the excess
// of the largest absolute coordinate over the cube half-side. Face ties choose
// X, then Y, then Z for the local normal; geographic coordinates stay continuous.
struct Cube : Sphere
{
	using Sphere::Sphere;
	double altitude(const v3opos_t &pos) const
	{
		return max_abs(relative(pos)) - p.radius;
	}
	std::optional<double> top(double x, double z, double altitude) const
	{
		const double r = p.radius + altitude;
		if (r < 0 || std::max(std::abs(x - p.origin.X), std::abs(z - p.origin.Z)) > r)
			return {};
		return p.origin.Y + r;
	}
	Sample sample(const v3opos_t &pos) const
	{
		const auto q = relative(pos);
		return {std::atan2(q.Y, std::hypot(q.X, q.Z)) * degrees,
				wrap(std::atan2(q.Z, q.X) * degrees), max_abs(q) - p.radius};
	}
	v3opos_t place(double lat, double lon, double altitude) const
	{
		const auto q = direction(lat, lon);
		return world(q * ((p.radius + altitude) / max_abs(q)));
	}
	v3d up(const v3opos_t &pos) const
	{
		const auto q = relative(pos);
		const double m = max_abs(q);
		if (m == 0)
			return {0, 1, 0};
		if (std::abs(q.X) == m)
			return {std::copysign(1.0, q.X), 0, 0};
		if (std::abs(q.Y) == m)
			return {0, std::copysign(1.0, q.Y), 0};
		return {0, 0, std::copysign(1.0, q.Z)};
	}
};

// Longitude goes around the ring; latitude is half the tube angle. The Earth
// poles meet at the inner equator of the tube (an intentional geographic seam).
struct Torus : Sphere
{
	using Sphere::Sphere;
	double altitude(const v3opos_t &pos) const
	{
		const auto q = relative(pos);
		return std::hypot(std::hypot(q.X, q.Z) - p.major_radius, q.Y) - p.radius;
	}
	std::optional<double> top(double x, double z, double altitude) const
	{
		const double r = p.radius + altitude;
		const double d = std::hypot(x - p.origin.X, z - p.origin.Z) - p.major_radius;
		if (r < 0 || std::abs(d) > r)
			return {};
		return p.origin.Y + std::sqrt(std::max(0.0, r * r - d * d));
	}
	Sample sample(const v3opos_t &pos) const
	{
		const auto q = relative(pos);
		const double r = std::hypot(q.X, q.Z) - p.major_radius;
		return {wrap(std::atan2(q.Y, r) * degrees) * 0.5,
				wrap(std::atan2(q.Z, q.X) * degrees), std::hypot(r, q.Y) - p.radius};
	}
	v3opos_t place(double lat, double lon, double altitude) const
	{
		const double tube = p.radius + altitude;
		const double a = 2 * lat / degrees, b = lon / degrees;
		const double ring = p.major_radius + tube * std::cos(a);
		return world({ring * std::cos(b), tube * std::sin(a), ring * std::sin(b)});
	}
	v3d up(const v3opos_t &pos) const
	{
		const auto q = relative(pos);
		const double r = std::hypot(q.X, q.Z);
		v3d normal = r > 0 ? v3d(q.X * (1 - p.major_radius / r), q.Y,
									 q.Z * (1 - p.major_radius / r))
						   : v3d(-p.major_radius, q.Y, 0);
		return normal.getLengthSQ() > 0 ? normal.normalize() : v3d(0, 1, 0);
	}
	std::vector<Region> coverage(const v3opos_t &lo, const v3opos_t &hi) const
	{
		const auto a = relative(lo), b = relative(hi);
		const double rmin = std::hypot(nearest_zero(a.X, b.X), nearest_zero(a.Z, b.Z));
		const double rmax = std::hypot(std::max(std::abs(a.X), std::abs(b.X)),
				std::max(std::abs(a.Z), std::abs(b.Z)));
		std::vector<Region> result;
		for (const auto &lat :
				angle_ranges(rmin - p.major_radius, rmax - p.major_radius, a.Y, b.Y))
			for (const auto &lon : angle_ranges(a.X, b.X, a.Z, b.Z))
				result.push_back({lat[0] * 0.5, lat[1] * 0.5, lon[0], lon[1]});
		return result;
	}
};

// Bind once at initialization. Scalar queries make one indirect call; hot loops
// construct the selected concrete type once and call it directly.
class Adapter
{
public:
	Parameters parameters;
	bool curved = false;
	template <class Projection>
	void bind(const Parameters &p)
	{
		parameters = p;
		curved = !std::is_same_v<Projection, Flat>;
		m_altitude = [](const Parameters &p, const v3opos_t &pos) {
			return Projection{p}.altitude(pos);
		};
		m_top = [](const Parameters &p, double x, double z, double altitude) {
			return Projection{p}.top(x, z, altitude);
		};
		m_sample = [](const Parameters &p, const v3opos_t &pos) {
			return Projection{p}.sample(pos);
		};
		m_place = [](const Parameters &p, double lat, double lon, double altitude) {
			return Projection{p}.place(lat, lon, altitude);
		};
		m_up = [](const Parameters &p, const v3opos_t &pos) {
			return Projection{p}.up(pos);
		};
		m_coverage = [](const Parameters &p, const v3opos_t &lo, const v3opos_t &hi) {
			return Projection{p}.coverage(lo, hi);
		};
	}
	Adapter() { bind<Flat>({}); }
	double altitude(const v3opos_t &p) const { return m_altitude(parameters, p); }
	std::optional<double> top(double x, double z, double altitude) const
	{
		return m_top(parameters, x, z, altitude);
	}
	Sample sample(const v3opos_t &p) const { return m_sample(parameters, p); }
	v3opos_t place(double lat, double lon, double altitude) const
	{
		return m_place(parameters, lat, lon, altitude);
	}
	v3d up(const v3opos_t &p) const { return m_up(parameters, p); }
	std::vector<Region> coverage(const v3opos_t &lo, const v3opos_t &hi) const
	{
		return m_coverage(parameters, lo, hi);
	}

private:
	double (*m_altitude)(const Parameters &, const v3opos_t &);
	std::optional<double> (*m_top)(const Parameters &, double, double, double);
	Sample (*m_sample)(const Parameters &, const v3opos_t &);
	v3opos_t (*m_place)(const Parameters &, double, double, double);
	v3d (*m_up)(const Parameters &, const v3opos_t &);
	std::vector<Region> (*m_coverage)(
			const Parameters &, const v3opos_t &, const v3opos_t &);
};
} // namespace fm_earth
