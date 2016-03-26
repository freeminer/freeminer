/*
Minetest
Copyright (C) 2015 est31, <MTest31@outlook.com>

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

#include "util/areastore.h"

class TestAreaStore : public TestBase {
public:
	TestAreaStore() { TestManager::registerTestModule(this); }
	const char *getName() { return "TestAreaStore"; }

	void runTests(IGameDef *gamedef);

	void genericStoreTest(AreaStore *store);
	void testVectorStore();
	void testSpatialStore();
	void testSerialization();
};

static TestAreaStore g_test_instance;

void TestAreaStore::runTests(IGameDef *gamedef)
{
	TEST(testVectorStore);
#if USE_SPATIAL
	TEST(testSpatialStore);
#endif
	TEST(testSerialization);
}

////////////////////////////////////////////////////////////////////////////////

void TestAreaStore::testVectorStore()
{
	VectorAreaStore store;
	genericStoreTest(&store);
}

void TestAreaStore::testSpatialStore()
{
#if USE_SPATIAL
	SpatialAreaStore store;
	genericStoreTest(&store);
#endif
}

void TestAreaStore::genericStoreTest(AreaStore *store)
{
	Area a(v3s16(-10, -3, 5), v3s16(0, 29, 7));
	Area b(v3s16(-5, -2, 5), v3s16(0, 28, 6));
	Area c(v3s16(-7, -3, 6), v3s16(-1, 27, 7));
	std::vector<Area *> res;

	UASSERTEQ(size_t, store->size(), 0);
	store->reserve(2); // sic
	store->insertArea(&a);
	store->insertArea(&b);
	store->insertArea(&c);
	UASSERTEQ(size_t, store->size(), 3);

	store->getAreasForPos(&res, v3s16(-1, 0, 6));
	UASSERTEQ(size_t, res.size(), 3);
	res.clear();
	store->getAreasForPos(&res, v3s16(0, 0, 7));
	UASSERTEQ(size_t, res.size(), 1);
	res.clear();

	store->removeArea(a.id);

	store->getAreasForPos(&res, v3s16(0, 0, 7));
	UASSERTEQ(size_t, res.size(), 0);
	res.clear();

	store->insertArea(&a);

	store->getAreasForPos(&res, v3s16(0, 0, 7));
	UASSERTEQ(size_t, res.size(), 1);
	res.clear();

	store->getAreasInArea(&res, v3s16(-10, -3, 5), v3s16(0, 29, 7), false);
	UASSERTEQ(size_t, res.size(), 3);
	res.clear();

	store->getAreasInArea(&res, v3s16(-100, 0, 6), v3s16(200, 0, 6), false);
	UASSERTEQ(size_t, res.size(), 0);
	res.clear();

	store->getAreasInArea(&res, v3s16(-100, 0, 6), v3s16(200, 0, 6), true);
	UASSERTEQ(size_t, res.size(), 3);
	res.clear();

	store->removeArea(a.id);
	store->removeArea(b.id);
	store->removeArea(c.id);

	Area d(v3s16(-100, -300, -200), v3s16(-50, -200, -100));
	d.data = "Hi!";
	store->insertArea(&d);

	store->getAreasForPos(&res, v3s16(-75, -250, -150));
	UASSERTEQ(size_t, res.size(), 1);
	UASSERTEQ(u16, res[0]->data.size(), 3);
	UASSERT(strncmp(res[0]->data.c_str(), "Hi!", 3) == 0);
	res.clear();

	store->removeArea(d.id);
}

void TestAreaStore::testSerialization()
{
	VectorAreaStore store;

	Area a(v3s16(-1, 0, 1), v3s16(0, 1, 2));
	a.data = "Area A";
	store.insertArea(&a);

	Area b(v3s16(123, 456, 789), v3s16(32000, 100, 10));
	b.data = "Area B";
	store.insertArea(&b);

	std::ostringstream os;
	store.serialize(os);
	std::string str = os.str();

	std::string str_wanted("\x00"  // Version
			"\x00\x02"  // Count
			"\xFF\xFF\x00\x00\x00\x01"  // Area A min edge
			"\x00\x00\x00\x01\x00\x02"  // Area A max edge
			"\x00\x06"  // Area A data length
			"Area A"  // Area A data
			"\x00\x7B\x00\x64\x00\x0A"  // Area B min edge (last two swapped with max edge for sorting)
			"\x7D\x00\x01\xC8\x03\x15"  // Area B max edge (^)
			"\x00\x06"  // Area B data length
			"Area B",  // Area B data
			1 + 2 +
			6 + 6 + 2 + 6 +
			6 + 6 + 2 + 6);
	UASSERTEQ(std::string, str, str_wanted);

	std::istringstream is(str);
	store.deserialize(is);

	UASSERTEQ(size_t, store.size(), 4);  // deserialize() doesn't clear the store
}

