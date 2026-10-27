#include "test.h"

#include "mapgen/earth/arnis-cpp/src/world_utils.h"

class TestFmArnisWorldUtils : public TestBase
{
public:
	TestFmArnisWorldUtils() { TestManager::registerTestModule(this); }
	const char *getName() override { return "TestFmArnisWorldUtils"; }
	void runTests(IGameDef *) override
	{
		TEST(testCustomWorldNameUnicodeSanitization);
		TEST(testCustomWorldNameCharacterLimit);
		TEST(testAreaFilenameUnicodeSanitization);
		TEST(testAreaFilenameByteLimit);
	}

	void testCustomWorldNameUnicodeSanitization()
	{
		using arnis::world_utils::world_folder_name;
		UASSERT(world_folder_name("\xc2\xa0Park\xe3\x80\x80") == "Park");
		UASSERT(world_folder_name("North\xc2\x85Shore") == "North_Shore");
		UASSERT(world_folder_name("\xe2\x80\x83Park.\xc2\xa0") == "Park");
	}

	void testCustomWorldNameCharacterLimit()
	{
		using arnis::world_utils::world_folder_name;
		std::string input;
		for (int i = 0; i < 49; ++i)
			input += "\xe3\x81\x82"; // U+3042, three UTF-8 bytes per character.
		const auto output = world_folder_name(input);
		UASSERT(output.size() == 48 * 3);
		UASSERT(output == input.substr(0, 48 * 3));
	}

	void testAreaFilenameUnicodeSanitization()
	{
		using arnis::world_utils::sanitize_for_filename;
		UASSERT(sanitize_for_filename("\xc2\xa0"
									  "Central Park\xe2\x80\x83") == "Central Park");
		UASSERT(sanitize_for_filename("North\xc2\x85Shore") == "North_Shore");
		UASSERT(sanitize_for_filename("My World... ") == "My World");
	}

	void testAreaFilenameByteLimit()
	{
		using arnis::world_utils::sanitize_for_filename;
		std::string input;
		for (int i = 0; i < 48; ++i)
			input += "\xe3\x81\x82";
		const auto output = sanitize_for_filename(input);
		UASSERT(output.size() == 22 * 3);
		UASSERT(output == input.substr(0, 22 * 3));
	}
};

static TestFmArnisWorldUtils g_test_fm_arnis_world_utils;
