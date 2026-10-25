#include "test.h"

#include "mapgen/earth/arnis-cpp/src/decals/pictograms.h"
#include "mapgen/earth/arnis-cpp/src/element_processing/attractions.h"

#include <initializer_list>
#include <string>
#include <utility>

namespace
{
arnis::tags_t tags(std::initializer_list<std::pair<const char *, const char *>> pairs)
{
	arnis::tags_t result;
	for (const auto &[key, value] : pairs)
		result.emplace(key, value);
	return result;
}
} // namespace

class TestFmArnisAttractions : public TestBase
{
public:
	TestFmArnisAttractions() { TestManager::registerTestModule(this); }
	const char *getName() override { return "TestFmArnisAttractions"; }
	void runTests(IGameDef *) override
	{
		TEST(testRideBuildingsAndHalls);
		TEST(testGenericRideFeatureOwnership);
		TEST(testSpinnerPartReplacement);
		TEST(testBusinessKindMatchesRustTagCoverage);
	}

	void testRideBuildingsAndHalls()
	{
		UASSERT(arnis::attractions::claims(
				tags({{"attraction", "carousel"}, {"building", "yes"}})));
		UASSERT(arnis::attractions::claims(tags({{"attraction", "big_wheel"}})));
		UASSERT(!arnis::attractions::claims(
				tags({{"attraction", "dark_ride"}, {"building", "yes"}})));
		UASSERT(!arnis::attractions::claims(
				tags({{"attraction", "bumper_car"}, {"building", "yes"}})));
		UASSERT(arnis::attractions::claims(tags({{"attraction", "bumper_car"}})));
		UASSERT(!arnis::attractions::claims(tags({{"attraction", "animal"}})));
	}

	void testGenericRideFeatureOwnership()
	{
		for (const auto &[key, value] : {std::pair{"barrier", "fence"},
					 std::pair{"railway", "rail"}, std::pair{"tourism", "attraction"}}) {
			auto ride = tags({{"attraction", "amusement_ride"}});
			ride.emplace(key, value);
			UASSERT(!arnis::attractions::claims(ride));
		}
		UASSERT(arnis::attractions::replaces_parts(tags({{"attraction", "big_wheel"}})));
		UASSERT(!arnis::attractions::replaces_parts(
				tags({{"attraction", "amusement_ride"}})));
	}

	void testSpinnerPartReplacement()
	{
		UASSERT(arnis::attractions::replaces_parts(
				tags({{"attraction", "amusement_ride"}, {"name", "Riesenrad Wien"}})));
		UASSERT(arnis::attractions::replaces_parts(
				tags({{"attraction", "amusement_ride"}, {"name", "Autoscooter"}})));
		UASSERT(!arnis::attractions::replaces_parts(
				tags({{"attraction", "amusement_ride"}, {"name", "Breakdance Meyer"}})));
		UASSERT(!arnis::attractions::claims(
				tags({{"attraction", "amusement_ride"}, {"building", "yes"}})));
		UASSERT(!arnis::attractions::claims(
				tags({{"attraction", "amusement_ride"}, {"railway", "rail"}})));
		UASSERT(arnis::attractions::claims(
				tags({{"attraction", "amusement_ride"}, {"name", "Tagada"}})));
	}

	void testBusinessKindMatchesRustTagCoverage()
	{
		using arnis::decals::pictograms::business_kind;
		UASSERT(business_kind(tags({{"shop", "department_store"}})) == "supermarket");
		UASSERT(business_kind(tags({{"shop", "bakery"}})) == "bakery");
		UASSERT(business_kind(tags({{"shop", "unknown_thing"}})) == "shop");
		UASSERT(business_kind(tags({{"amenity", "conference_centre"}})) == "office");
		UASSERT(business_kind(tags({{"tourism", "gallery"}})) == "museum");
		UASSERT(business_kind(tags({{"leisure", "water_park"}})) == "swimming");
		UASSERT(business_kind(tags({{"healthcare", "physiotherapist"}})) == "doctors");
		UASSERT(!business_kind(tags({{"amenity", "bench"}})));
	}
};

static TestFmArnisAttractions g_test_fm_arnis_attractions;
