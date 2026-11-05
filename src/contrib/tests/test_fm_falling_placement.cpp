#include "src/contrib/falling_placement.h"
#include <cassert>
#include <set>
#include <utility>
struct Pos
{
	int X, Y, Z;
	Pos(int x = 0, int y = 0, int z = 0) : X(x), Y(y), Z(z) {}
};
int main()
{
	Pos source(10, 5, 20), found;
	std::set<std::pair<int, int>> allowed{{10, 20}, {9, 20}};
	auto usable = [&](const Pos &p) { return p.Y == 5 && allowed.count({p.X, p.Z}); };
	assert(epixel::findFallingSupport(source, usable, found));
	assert(found.X == 10 && found.Z == 20);
	allowed.erase({10, 20});
	assert(epixel::findFallingSupport(source, usable, found));
	assert(found.X == 9 && found.Z == 20);
	allowed = {{11, 21}};
	assert(epixel::findFallingSupport(source, usable, found));
	assert(found.X == 11 && found.Z == 21);
	allowed.clear();
	assert(!epixel::findFallingSupport(source, usable, found));
}
