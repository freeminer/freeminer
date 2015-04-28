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

#include <sstream>

#include "gamedef.h"
#include "inventory.h"

class TestInventory : public TestBase {
public:
	TestInventory() { TestManager::registerTestModule(this); }
	const char *getName() { return "TestInventory"; }

	void runTests(IGameDef *gamedef);

	void testSerializeDeserialize(IItemDefManager *idef);

	static const char *serialized_inventory;
	static const char *serialized_inventory_2;
};

static TestInventory g_test_instance;

void TestInventory::runTests(IGameDef *gamedef)
{
	TEST(testSerializeDeserialize, gamedef->getItemDefManager());
}

////////////////////////////////////////////////////////////////////////////////

void TestInventory::testSerializeDeserialize(IItemDefManager *idef)
{
	Inventory inv(idef);
	std::istringstream is(serialized_inventory, std::ios::binary);

	inv.deSerialize(is);
	UASSERT(inv.getList("0"));
	UASSERT(!inv.getList("main"));

	inv.getList("0")->setName("main");
	UASSERT(!inv.getList("0"));
	UASSERT(inv.getList("main"));
	UASSERTEQ(u32, inv.getList("main")->getWidth(), 3);

	inv.getList("main")->setWidth(5);
	std::ostringstream inv_os(std::ios::binary);
	inv.serialize(inv_os);
	UASSERTEQ(std::string, inv_os.str(), serialized_inventory_2);
}

const char *TestInventory::serialized_inventory =
	"List 0 32\n"
	"Width 3\n"
	"Empty\n"
	"Empty\n"
	"Empty\n"
	"Empty\n"
	"Empty\n"
	"Empty\n"
	"Empty\n"
	"Empty\n"
	"Empty\n"
	"Item default:cobble 61\n"
	"Empty\n"
	"Empty\n"
	"Empty\n"
	"Empty\n"
	"Empty\n"
	"Empty\n"
	"Item default:dirt 71\n"
	"Empty\n"
	"Empty\n"
	"Empty\n"
	"Empty\n"
	"Empty\n"
	"Empty\n"
	"Empty\n"
	"Item default:dirt 99\n"
	"Item default:cobble 38\n"
	"Empty\n"
	"Empty\n"
	"Empty\n"
	"Empty\n"
	"Empty\n"
	"Empty\n"
	"EndInventoryList\n"
	"EndInventory\n";

const char *TestInventory::serialized_inventory_2 =
	"List main 32\n"
	"Width 5\n"
	"Empty\n"
	"Empty\n"
	"Empty\n"
	"Empty\n"
	"Empty\n"
	"Empty\n"
	"Empty\n"
	"Empty\n"
	"Empty\n"
	"Item default:cobble 61\n"
	"Empty\n"
	"Empty\n"
	"Empty\n"
	"Empty\n"
	"Empty\n"
	"Empty\n"
	"Item default:dirt 71\n"
	"Empty\n"
	"Empty\n"
	"Empty\n"
	"Empty\n"
	"Empty\n"
	"Empty\n"
	"Empty\n"
	"Item default:dirt 99\n"
	"Item default:cobble 38\n"
	"Empty\n"
	"Empty\n"
	"Empty\n"
	"Empty\n"
	"Empty\n"
	"Empty\n"
	"EndInventoryList\n"
	"EndInventory\n";
