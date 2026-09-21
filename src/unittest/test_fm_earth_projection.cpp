// SPDX-License-Identifier: GPL-3.0-or-later
#include "test.h"
#include "mapgen/fm_earth_projection.h"
#include "client/fm_projected_surface.h"

class TestFmEarthProjection : public TestBase
{
public:
	TestFmEarthProjection() { TestManager::registerTestModule(this); }
	const char *getName() override { return "TestFmEarthProjection"; }
	void runTests(IGameDef *) override
	{
		TEST(testFlat);
		TEST(testRoundTrips);
		TEST(testSolidsAndNormals);
		TEST(testCoverage);
		TEST(testVerticalRays);
		TEST(testFarSurface);
		TEST(testFarCubeEdge);
	}
	void testFlat();
	void testRoundTrips();
	void testSolidsAndNormals();
	void testCoverage();
	void testVerticalRays();
	void testFarSurface();
	void testFarCubeEdge();
};

static TestFmEarthProjection g_test_instance;

void TestFmEarthProjection::testFarCubeEdge()
{
	fm_earth::Parameters p;
	p.radius = 100;
	fm_earth::Adapter projection;
	projection.bind<fm_earth::Cube>(p);
	// A tied sample chooses +X, but the adjacent +Y triangle still belongs
	// to the surface. Its visibility must be decided after projection.
	UASSERT(projection.up({99, 99, 0}) == v3d(1, 0, 0));
	const auto project = [&](const v3opos_t &world) {
		return farmesh::projectSurface(
				projection, world, 0, [](const auto &) { return 0.0; }, true);
	};
	const auto a = project({98, 101, -1});
	const auto b = project({98, 101, 1});
	const auto c = project({100, 101, 1});
	const auto normal = v3opos_t::from(a.normal + b.normal + c.normal);
	UASSERT(farmesh::surfaceTriangleWinding(a.position, b.position, c.position, normal) ==
			1);
	UASSERT(farmesh::surfaceTriangleWinding(a.position, c.position, b.position, normal) ==
			-1);
	UASSERT(farmesh::surfaceTriangleWinding(a.position, a.position, c.position, normal) ==
			0);
	// The same geometric winding must reverse for an inward-facing surface.
	UASSERT(farmesh::surfaceTriangleWinding(
					a.position, b.position, c.position, -normal) == -1);
}

void TestFmEarthProjection::testFarSurface()
{
	fm_earth::Parameters p;
	p.origin = {123, -456, 789};
	p.radius = 10000;
	p.major_radius = 30000;
	fm_earth::Adapter projection;
	const auto check = [&]() {
		for (double lat : {-80.0, -40.0, 0.0, 40.0, 80.0})
			for (double lon : {-179.9, -90.0, 0.0, 90.0, 179.9})
				for (double terrain : {-150.0, 120.0}) {
					const auto world = projection.place(lat, lon, -200);
					// Fog must retain cells intersecting the atmosphere on every
					// side, including cells spanning a cube edge or torus hole.
					for (double side : {64.0, 30000.0}) {
						const auto bounds =
								farmesh::surfaceAltitudeBounds(projection, world, side);
						for (double x : {-0.5, 0.0, 0.5})
							for (double y : {-0.5, 0.0, 0.5})
								for (double z : {-0.5, 0.0, 0.5}) {
									const auto altitude = projection.altitude(
											world +
											v3opos_t(x * side, y * side, z * side));
									UASSERT(altitude >= bounds.first - 0.01 &&
											altitude <= bounds.second + 0.01);
								}
					}
					const auto surface = farmesh::projectSurface(
							projection, world, 5, [&](const fm_earth::Sample &sample) {
								UASSERT(std::abs(sample.lat - lat) < 0.01);
								UASSERT(std::abs(fm_earth::wrap(sample.lon - lon)) <
										0.01);
								return terrain;
							});
					UASSERT(std::abs(projection.altitude(surface.position) -
									 std::max(5.0, terrain)) < 0.01);
					UASSERT(std::abs(surface.normal.getLength() - 1) < 1e-6);
					bool queried_terrain = false;
					const auto water = farmesh::projectSurface(
							projection, world, 5,
							[&](const auto &) {
								queried_terrain = true;
								return terrain;
							},
							true);
					UASSERT(!queried_terrain);
					UASSERT(std::abs(projection.altitude(water.position) - 5) < 0.01);
					const auto resampled_water = farmesh::projectSurface(
							projection, water.position, 5,
							[](const auto &) { return 1000.0; }, true);
					UASSERT((resampled_water.position - water.position).getLength() <
							0.01);
					// Up must point into the atmosphere on every hemisphere and
					// on the inner wall of the torus.
					UASSERT(projection.altitude(
									surface.position + v3opos_t::from(surface.normal)) >
							projection.altitude(surface.position));
				}
	};
	projection.bind<fm_earth::Sphere>(p);
	check();
	projection.bind<fm_earth::Cube>(p);
	check();
	projection.bind<fm_earth::Torus>(p);
	check();
}

void TestFmEarthProjection::testFlat()
{
	fm_earth::Parameters p;
	p.scale = {2, 4, 3};
	p.center = {12, 50, -20};
	const fm_earth::Flat flat{p};
	const auto sample = flat.sample({123, 45, -678});
	UASSERT(std::abs(sample.lon - (12 + 246 / fm_earth::metres_per_degree)) < 1e-9);
	UASSERT(std::abs(sample.lat - (-20 - 2034 / fm_earth::metres_per_degree)) < 1e-9);
	UASSERT(sample.altitude == 45);
	UASSERT((flat.place(sample.lat, sample.lon, sample.altitude) -
					v3opos_t(123, 45, -678))
					.getLength() < 0.01);
	const auto outside = flat.sample({100000000, 0, 0});
	UASSERT(outside.lat == 89.9999 && outside.lon == 0);
}

void TestFmEarthProjection::testRoundTrips()
{
	fm_earth::Parameters p;
	p.origin = {123, -456, 789};
	p.radius = 100;
	p.major_radius = 300;
	fm_earth::Adapter adapter;
	const auto check = [&]() {
		UASSERT(adapter.curved);
		for (double lat : {-89.9, -60.0, -35.26438968, 0.0, 35.26438968, 60.0, 89.9})
			for (double lon : {-180.0, -135.0, -45.0, 0.0, 45.0, 135.0, 179.9})
				for (double altitude : {-25.0, 0.0, 17.0}) {
					const auto pos = adapter.place(lat, lon, altitude);
					const auto sample = adapter.sample(pos);
					UASSERT(std::abs(sample.lat - lat) < 0.002);
					UASSERT(std::abs(fm_earth::wrap(sample.lon - lon)) <
							(std::is_same_v<opos_t, float> ? 0.05 : 1e-7));
					UASSERT(std::abs(sample.altitude - altitude) < 0.002);
					UASSERT(std::abs(adapter.altitude(pos) - altitude) < 0.002);
					UASSERT(std::abs(adapter.up(pos).getLength() - 1) < 1e-6);
				}
	};
	adapter.bind<fm_earth::Sphere>(p);
	check();
	adapter.bind<fm_earth::Cube>(p);
	check();
	adapter.bind<fm_earth::Torus>(p);
	check();
	adapter.bind<fm_earth::Flat>(p);
	UASSERT(!adapter.curved);
}

void TestFmEarthProjection::testSolidsAndNormals()
{
	fm_earth::Parameters p;
	p.radius = 100;
	p.major_radius = 300;
	const fm_earth::Sphere sphere{p};
	const fm_earth::Cube cube{p};
	const fm_earth::Torus torus{p};
	UASSERT(sphere.altitude({0, 0, 0}) == -100);
	UASSERT(sphere.altitude({0, -101, 0}) == 1);
	UASSERT(sphere.up({0, -100, 0}) == v3d(0, -1, 0));
	UASSERT(cube.altitude({100, 100, 100}) == 0);
	UASSERT(cube.altitude({99, -99, 99}) == -1);
	UASSERT(cube.up({0, 0, -100}) == v3d(0, 0, -1));
	UASSERT(cube.up({100, 100, 0}) == v3d(1, 0, 0));
	UASSERT(torus.altitude({0, 0, 0}) == 200); // Hole, not underground.
	UASSERT(torus.altitude({300, 0, 0}) == -100);
	UASSERT(torus.altitude({300, -101, 0}) == 1);
	UASSERT(torus.up({200, 0, 0}) == v3d(-1, 0, 0));
	UASSERT(std::abs(torus.sample({200, 0, 0}).lat) == 90);
	for (const auto &normal : {sphere.up({0, 0, 0}), cube.up({0, 0, 0}),
				 torus.up({300, 0, 0}), torus.up({0, 0, 0})})
		UASSERT(std::isfinite(normal.X) && std::abs(normal.getLength() - 1) < 1e-6);
}

void TestFmEarthProjection::testCoverage()
{
	fm_earth::Parameters p;
	p.radius = 100;
	p.major_radius = 300;
	p.origin = {10, -20, 30};
	fm_earth::Adapter adapter;
	const auto check = [&]() {
		// Longitude seam, polar axis, torus inner seam, ring and ordinary patch.
		for (const auto &center :
				{v3opos_t(-100, 0, 0), v3opos_t(0, 100, 0), v3opos_t(200, 0, 0),
						v3opos_t(300, 0, 0), v3opos_t(80, 60, 20), v3opos_t(0, 0, 0)}) {
			const auto lo = center + p.origin - v3opos_t(12, 12, 12);
			const auto hi = center + p.origin + v3opos_t(12, 12, 12);
			const auto regions = adapter.coverage(lo, hi);
			for (int x = 0; x <= 8; ++x)
				for (int y = 0; y <= 8; ++y)
					for (int z = 0; z <= 8; ++z) {
						const auto sample =
								adapter.sample(lo + v3opos_t(x * 3, y * 3, z * 3));
						bool covered = false;
						for (const auto &region : regions)
							covered |= sample.lat >= region.min_lat - 1e-6 &&
									   sample.lat <= region.max_lat + 1e-6 &&
									   sample.lon >= region.min_lon - 1e-6 &&
									   sample.lon <= region.max_lon + 1e-6;
						UASSERT(covered);
					}
		}
	};
	adapter.bind<fm_earth::Sphere>(p);
	check();
	adapter.bind<fm_earth::Cube>(p);
	check();
	adapter.bind<fm_earth::Torus>(p);
	check();
}

void TestFmEarthProjection::testVerticalRays()
{
	fm_earth::Parameters p;
	p.radius = 100;
	p.major_radius = 300;
	fm_earth::Sphere sphere{p};
	fm_earth::Cube cube{p};
	fm_earth::Torus torus{p};
	UASSERT(sphere.top(0, 0, 0).value() == 100);
	UASSERT(!sphere.top(101, 0, 0));
	UASSERT(cube.top(100, 100, 5).value() == 105);
	UASSERT(!cube.top(106, 0, 5));
	UASSERT(!torus.top(0, 0, 0));
	UASSERT(torus.top(300, 0, 0).value() == 100);
	UASSERT(torus.top(200, 0, 0).value() == 0);
	UASSERT(!torus.top(401, 0, 0));
}
