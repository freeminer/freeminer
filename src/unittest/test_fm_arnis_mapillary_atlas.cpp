#include "test.h"

#include "mapgen/earth/arnis-cpp/src/mapillary/atlas.h"

namespace
{
std::vector<std::uint8_t> solid_rgb(std::size_t pixels, std::uint8_t value)
{
	return std::vector<std::uint8_t>(pixels * 3, value);
}
} // namespace

class TestFmArnisMapillaryAtlas : public TestBase
{
public:
	TestFmArnisMapillaryAtlas() { TestManager::registerTestModule(this); }
	const char *getName() override { return "TestFmArnisMapillaryAtlas"; }
	void runTests(IGameDef *) override { TEST(testAdmissionStopsAfterFirstOverflow); }

	void testAdmissionStopsAfterFirstOverflow()
	{
		namespace atlas = arnis::mapillary::atlas;
		struct RestoreAtlas
		{
			~RestoreAtlas()
			{
				arnis::mapillary::atlas::set_atlas_side(
						arnis::mapillary::atlas::ATLAS_SIDE_STANDARD);
			}
		} restore;
		atlas::set_atlas_side(4); // 9 pixels after the conservative packing allowance.

		const auto full = solid_rgb(9, 1);
		const auto small = solid_rgb(1, 2);
		const auto later = solid_rgb(1, 3);
		UASSERT(atlas::reserve_texture(full, 3, 3));
		UASSERT(atlas::reserve_texture(
				full, 3, 3)); // Identical panels share one atlas entry.
		UASSERT(!atlas::reserve_texture(small, 1, 1)); // First overflow closes admission.
		UASSERT(atlas::reserve_texture(
				full, 3, 3)); // Existing entries remain admissible.
		UASSERT(atlas::admission_closed());

		atlas::release_texture(full, 3, 3);
		atlas::release_texture(full, 3, 3);
		atlas::release_texture(full, 3, 3);
		UASSERTEQ(std::uint64_t, atlas::used_texture_area(), 0);
		UASSERT(!atlas::reserve_texture(later, 1, 1)); // Freed space does not reopen it.
		UASSERTEQ(std::uint64_t, atlas::rejected_textures(), 2);

		atlas::reset_texture_usage();
		UASSERT(!atlas::admission_closed());
		UASSERT(atlas::reserve_texture(small, 1, 1));
	}
};

static TestFmArnisMapillaryAtlas g_test_fm_arnis_mapillary_atlas;
