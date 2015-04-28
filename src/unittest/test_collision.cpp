/*
Minetest
Copyright (C) 2013 celeron55, Perttu Ahola <celeron55@gmail.com>

This program is free software; you can redistribute it and/or modify
it under the terms of the GNU Lesser General Public License as published by
the Free Software Foundation; either version 2.1 of the License, or
(at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU Lesser General Public License for more details.

You should have received a copy of the GNU Lesser General Public License along
with this program; if not, write to the Free Software Foundation, Inc.,
51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
*/

#include "test.h"

#include "collision.h"

class TestCollision : public TestBase {
public:
	TestCollision() { TestManager::registerTestModule(this); }
	const char *getName() { return "TestCollision"; }

	void runTests(IGameDef *gamedef);

	void testAxisAlignedCollision();
};

static TestCollision g_test_instance;

void TestCollision::runTests(IGameDef *gamedef)
{
	TEST(testAxisAlignedCollision);
}

////////////////////////////////////////////////////////////////////////////////

void TestCollision::testAxisAlignedCollision()
{
	for (s16 bx = -3; bx <= 3; bx++)
	for (s16 by = -3; by <= 3; by++)
	for (s16 bz = -3; bz <= 3; bz++) {
		// X-
		{
			aabb3f s(bx, by, bz, bx+1, by+1, bz+1);
			aabb3f m(bx-2, by, bz, bx-1, by+1, bz+1);
			v3f v(1, 0, 0);
			f32 dtime = 0;
			UASSERT(axisAlignedCollision(s, m, v, 0, dtime) == 0);
			UASSERT(fabs(dtime - 1.000) < 0.001);
		}
		{
			aabb3f s(bx, by, bz, bx+1, by+1, bz+1);
			aabb3f m(bx-2, by, bz, bx-1, by+1, bz+1);
			v3f v(-1, 0, 0);
			f32 dtime = 0;
			UASSERT(axisAlignedCollision(s, m, v, 0, dtime) == -1);
		}
		{
			aabb3f s(bx, by, bz, bx+1, by+1, bz+1);
			aabb3f m(bx-2, by+1.5, bz, bx-1, by+2.5, bz-1);
			v3f v(1, 0, 0);
			f32 dtime;
			UASSERT(axisAlignedCollision(s, m, v, 0, dtime) == -1);
		}
		{
			aabb3f s(bx, by, bz, bx+1, by+1, bz+1);
			aabb3f m(bx-2, by-1.5, bz, bx-1.5, by+0.5, bz+1);
			v3f v(0.5, 0.1, 0);
			f32 dtime;
			UASSERT(axisAlignedCollision(s, m, v, 0, dtime) == 0);
			UASSERT(fabs(dtime - 3.000) < 0.001);
		}
		{
			aabb3f s(bx, by, bz, bx+1, by+1, bz+1);
			aabb3f m(bx-2, by-1.5, bz, bx-1.5, by+0.5, bz+1);
			v3f v(0.5, 0.1, 0);
			f32 dtime;
			UASSERT(axisAlignedCollision(s, m, v, 0, dtime) == 0);
			UASSERT(fabs(dtime - 3.000) < 0.001);
		}

		// X+
		{
			aabb3f s(bx, by, bz, bx+1, by+1, bz+1);
			aabb3f m(bx+2, by, bz, bx+3, by+1, bz+1);
			v3f v(-1, 0, 0);
			f32 dtime;
			UASSERT(axisAlignedCollision(s, m, v, 0, dtime) == 0);
			UASSERT(fabs(dtime - 1.000) < 0.001);
		}
		{
			aabb3f s(bx, by, bz, bx+1, by+1, bz+1);
			aabb3f m(bx+2, by, bz, bx+3, by+1, bz+1);
			v3f v(1, 0, 0);
			f32 dtime;
			UASSERT(axisAlignedCollision(s, m, v, 0, dtime) == -1);
		}
		{
			aabb3f s(bx, by, bz, bx+1, by+1, bz+1);
			aabb3f m(bx+2, by, bz+1.5, bx+3, by+1, bz+3.5);
			v3f v(-1, 0, 0);
			f32 dtime;
			UASSERT(axisAlignedCollision(s, m, v, 0, dtime) == -1);
		}
		{
			aabb3f s(bx, by, bz, bx+1, by+1, bz+1);
			aabb3f m(bx+2, by-1.5, bz, bx+2.5, by-0.5, bz+1);
			v3f v(-0.5, 0.2, 0);
			f32 dtime;
			UASSERT(axisAlignedCollision(s, m, v, 0, dtime) == 1);  // Y, not X!
			UASSERT(fabs(dtime - 2.500) < 0.001);
		}
		{
			aabb3f s(bx, by, bz, bx+1, by+1, bz+1);
			aabb3f m(bx+2, by-1.5, bz, bx+2.5, by-0.5, bz+1);
			v3f v(-0.5, 0.3, 0);
			f32 dtime;
			UASSERT(axisAlignedCollision(s, m, v, 0, dtime) == 0);
			UASSERT(fabs(dtime - 2.000) < 0.001);
		}

		// TODO: Y-, Y+, Z-, Z+

		// misc
		{
			aabb3f s(bx, by, bz, bx+2, by+2, bz+2);
			aabb3f m(bx+2.3, by+2.29, bz+2.29, bx+4.2, by+4.2, bz+4.2);
			v3f v(-1./3, -1./3, -1./3);
			f32 dtime;
			UASSERT(axisAlignedCollision(s, m, v, 0, dtime) == 0);
			UASSERT(fabs(dtime - 0.9) < 0.001);
		}
		{
			aabb3f s(bx, by, bz, bx+2, by+2, bz+2);
			aabb3f m(bx+2.29, by+2.3, bz+2.29, bx+4.2, by+4.2, bz+4.2);
			v3f v(-1./3, -1./3, -1./3);
			f32 dtime;
			UASSERT(axisAlignedCollision(s, m, v, 0, dtime) == 1);
			UASSERT(fabs(dtime - 0.9) < 0.001);
		}
		{
			aabb3f s(bx, by, bz, bx+2, by+2, bz+2);
			aabb3f m(bx+2.29, by+2.29, bz+2.3, bx+4.2, by+4.2, bz+4.2);
			v3f v(-1./3, -1./3, -1./3);
			f32 dtime;
			UASSERT(axisAlignedCollision(s, m, v, 0, dtime) == 2);
			UASSERT(fabs(dtime - 0.9) < 0.001);
		}
		{
			aabb3f s(bx, by, bz, bx+2, by+2, bz+2);
			aabb3f m(bx-4.2, by-4.2, bz-4.2, bx-2.3, by-2.29, bz-2.29);
			v3f v(1./7, 1./7, 1./7);
			f32 dtime;
			UASSERT(axisAlignedCollision(s, m, v, 0, dtime) == 0);
			UASSERT(fabs(dtime - 16.1) < 0.001);
		}
		{
			aabb3f s(bx, by, bz, bx+2, by+2, bz+2);
			aabb3f m(bx-4.2, by-4.2, bz-4.2, bx-2.29, by-2.3, bz-2.29);
			v3f v(1./7, 1./7, 1./7);
			f32 dtime;
			UASSERT(axisAlignedCollision(s, m, v, 0, dtime) == 1);
			UASSERT(fabs(dtime - 16.1) < 0.001);
		}
		{
			aabb3f s(bx, by, bz, bx+2, by+2, bz+2);
			aabb3f m(bx-4.2, by-4.2, bz-4.2, bx-2.29, by-2.29, bz-2.3);
			v3f v(1./7, 1./7, 1./7);
			f32 dtime;
			UASSERT(axisAlignedCollision(s, m, v, 0, dtime) == 2);
			UASSERT(fabs(dtime - 16.1) < 0.001);
		}
	}
}
