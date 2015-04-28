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

#include "irrlichttypes_extrabloated.h"
#include "log.h"
#include "serialization.h"
#include "nodedef.h"
#include "noise.h"

class TestCompression : public TestBase {
public:
	TestCompression() { TestManager::registerTestModule(this); }
	const char *getName() { return "TestCompression"; }

	void runTests(IGameDef *gamedef);

	void testRLECompression();
	void testZlibCompression();
	void testZlibLargeData();
};

static TestCompression g_test_instance;

void TestCompression::runTests(IGameDef *gamedef)
{
	TEST(testRLECompression);
	TEST(testZlibCompression);
	TEST(testZlibLargeData);
}

////////////////////////////////////////////////////////////////////////////////

void TestCompression::testRLECompression()
{
	SharedBuffer<u8> fromdata(4);
	fromdata[0]=1;
	fromdata[1]=5;
	fromdata[2]=5;
	fromdata[3]=1;

	std::ostringstream os(std::ios_base::binary);
	compress(fromdata, os, 0);

	std::string str_out = os.str();

	infostream << "str_out.size()="<<str_out.size()<<std::endl;
	infostream << "TestCompress: 1,5,5,1 -> ";
	for (u32 i = 0; i < str_out.size(); i++)
		infostream << (u32)str_out[i] << ",";
	infostream << std::endl;

	UASSERT(str_out.size() == 10);

	UASSERT(str_out[0] == 0);
	UASSERT(str_out[1] == 0);
	UASSERT(str_out[2] == 0);
	UASSERT(str_out[3] == 4);
	UASSERT(str_out[4] == 0);
	UASSERT(str_out[5] == 1);
	UASSERT(str_out[6] == 1);
	UASSERT(str_out[7] == 5);
	UASSERT(str_out[8] == 0);
	UASSERT(str_out[9] == 1);

	std::istringstream is(str_out, std::ios_base::binary);
	std::ostringstream os2(std::ios_base::binary);

	decompress(is, os2, 0);
	std::string str_out2 = os2.str();

	infostream << "decompress: ";
	for (u32 i = 0; i < str_out2.size(); i++)
		infostream << (u32)str_out2[i] << ",";
	infostream << std::endl;

	UASSERTEQ(size_t, str_out2.size(), fromdata.getSize());

	for (u32 i = 0; i < str_out2.size(); i++)
		UASSERT(str_out2[i] == fromdata[i]);
}

void TestCompression::testZlibCompression()
{
	SharedBuffer<u8> fromdata(4);
	fromdata[0]=1;
	fromdata[1]=5;
	fromdata[2]=5;
	fromdata[3]=1;

	std::ostringstream os(std::ios_base::binary);
	compress(fromdata, os, SER_FMT_VER_HIGHEST_READ);

	std::string str_out = os.str();

	infostream << "str_out.size()=" << str_out.size() <<std::endl;
	infostream << "TestCompress: 1,5,5,1 -> ";
	for (u32 i = 0; i < str_out.size(); i++)
		infostream << (u32)str_out[i] << ",";
	infostream << std::endl;

	std::istringstream is(str_out, std::ios_base::binary);
	std::ostringstream os2(std::ios_base::binary);

	decompress(is, os2, SER_FMT_VER_HIGHEST_READ);
	std::string str_out2 = os2.str();

	infostream << "decompress: ";
	for (u32 i = 0; i < str_out2.size(); i++)
		infostream << (u32)str_out2[i] << ",";
	infostream << std::endl;

	UASSERTEQ(size_t, str_out2.size(), fromdata.getSize());

	for (u32 i = 0; i < str_out2.size(); i++)
		UASSERT(str_out2[i] == fromdata[i]);
}

void TestCompression::testZlibLargeData()
{
	infostream << "Test: Testing zlib wrappers with a large amount "
		"of pseudorandom data" << std::endl;

	u32 size = 50000;
	infostream << "Test: Input size of large compressZlib is "
		<< size << std::endl;

	std::string data_in;
	data_in.resize(size);
	PseudoRandom pseudorandom(9420);
	for (u32 i = 0; i < size; i++)
		data_in[i] = pseudorandom.range(0, 255);

	std::ostringstream os_compressed(std::ios::binary);
	compressZlib(data_in, os_compressed);
	infostream << "Test: Output size of large compressZlib is "
		<< os_compressed.str().size()<<std::endl;

	std::istringstream is_compressed(os_compressed.str(), std::ios::binary);
	std::ostringstream os_decompressed(std::ios::binary);
	decompressZlib(is_compressed, os_decompressed);
	infostream << "Test: Output size of large decompressZlib is "
		<< os_decompressed.str().size() << std::endl;

	std::string str_decompressed = os_decompressed.str();
	UASSERTEQ(size_t, str_decompressed.size(), data_in.size());

	for (u32 i = 0; i < size && i < str_decompressed.size(); i++) {
		UTEST(str_decompressed[i] == data_in[i],
				"index out[%i]=%i differs from in[%i]=%i",
				i, str_decompressed[i], i, data_in[i]);
	}
}
