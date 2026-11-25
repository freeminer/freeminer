// SPDX-License-Identifier: LGPL-2.1-or-later
#include "test.h"
#include "mapgen/mandelbulber2.h"
#include "formula/definition/all_fractal_definitions.h"

#include <cmath>
#include <stdexcept>
#include <type_traits>

class TestFmMandelbulber2 : public TestBase
{
public:
	TestFmMandelbulber2() { TestManager::registerTestModule(this); }
	const char *getName() override { return "TestFmMandelbulber2"; }
	void runTests(IGameDef *) override
	{
		TEST(testFormulas);
		TEST(testFormulaIds);
		TEST(testAllCpuFormulas);
		TEST(testParameters);
		TEST(testInvalidParameters);
	}

	void testFormulaIds()
	{
		FmMandelbulber2 by_name("kaleidoscopic_ifs", {});
		FmMandelbulber2 by_id("#10", {});
		UASSERT(by_name.distance(1.2, 0.8, 0.4) == by_id.distance(1.2, 0.8, 0.4));
		bool rejected = false;
		try {
			FmMandelbulber2 invalid("#10x", {});
		} catch (const std::invalid_argument &) {
			rejected = true;
		}
		UASSERT(rejected);
	}

	void testAllCpuFormulas()
	{
		unsigned int count = 0;
		auto check = [&]<class Formula>() {
			if constexpr (!std::is_same_v<Formula, cFractalCustom>) {
				Formula formula;
				FmMandelbulber2 fractal("mandelbulb," + formula.getInternalName(), {});
				const double distance = fractal.distance(0.17, 0.23, 0.31);
				UASSERT(std::isfinite(distance));
				UASSERT(distance >= 0.0);
				++count;
			}
		};
#undef MANDELBULBER2_FORMULA_DEFINITION_ALL_FRACTAL_DEFINITIONS_H_
#undef FRACTAL_CLASS
#define FRACTAL_CLASS(formula) check.template operator()<formula>();
#include "formula/definition/all_fractal_definitions.h"
#undef FRACTAL_CLASS
		UASSERT(count > 0);
	}

	void testFormulas()
	{
		FmMandelbulber2Options options;
		for (const char *name :
				{"mandelbulb", "menger_sponge", "mandelbox", "transf_scale,mandelbulb"}) {
			FmMandelbulber2 fractal(name, options);
			UASSERT(std::isfinite(fractal.distance(1.2, 0.8, 0.4)));
			UASSERT(fractal.distance(1.2, 0.8, 0.4) >= 0.0);
		}
	}

	void testParameters()
	{
		FmMandelbulber2Options options;
		options.parameters["mandelbox_scale"] = -1.5;
		Json::Value offset(Json::arrayValue);
		offset.append(0.1);
		offset.append(0.2);
		offset.append(0.3);
		options.parameters["transf_offset_000"] = offset;
		FmMandelbulber2 configured("mandelbox", options);
		UASSERT(std::isfinite(configured.distance(1.2, 0.8, 0.4)));
	}

	void testInvalidParameters()
	{
		FmMandelbulber2Options options;
		options.parameters["unknown_parameter"] = 42;
		bool rejected = false;
		try {
			FmMandelbulber2 fractal("mandelbulb", options);
		} catch (const std::invalid_argument &) {
			rejected = true;
		}
		UASSERT(rejected);
	}
};

static TestFmMandelbulber2 g_test_instance;
