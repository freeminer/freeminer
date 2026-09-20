// SPDX-License-Identifier: GPL-3.0-or-later
#include "test.h"
#include "mapgen/fm_earth_projection.h"

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
	}
	void testFlat();
	void testRoundTrips();
	void testSolidsAndNormals();
	void testCoverage();
	void testVerticalRays();
};

static TestFmEarthProjection g_test_instance;

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
