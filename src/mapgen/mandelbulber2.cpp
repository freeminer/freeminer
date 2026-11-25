// Freeminer adapter for Mandelbulber2's CPU formula implementations.
// GPL-3.0-or-later, like the Mandelbulber2 source files included below.
#include "mandelbulber2.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <charconv>
#include <cstdlib>
#include <limits>
#include <unordered_map>
#include <variant>
#include <vector>
#include <memory>
#include <sstream>
#include <stdexcept>
#include <string>
#include <type_traits>

#ifdef USE_MANDELBULBER2
#include "formula/definition/all_fractal_definitions.h"
#include "src/fractal.h"

// The upstream definitions for these methods pull in Qt for UI paths and logging.
// The map generator needs only the formula metadata and an identity rotation.
cAbstractFractal::cAbstractFractal() :
		internalID(fractal::invalid), DEType(fractal::undefinedDEType),
		DEFunctionType(fractal::undefinedDEFunction),
		cpixelAddition(fractal::cpixelUndefined), defaultBailout(-1.0),
		DEAnalyticFunction(fractal::analyticFunctionUndefined),
		coloringFunction(fractal::coloringFunctionUndefined)
{
}
cAbstractFractal::~cAbstractFractal() = default;
std::string cAbstractFractal::getIconName() const
{
	return {};
}
std::string cAbstractFractal::getUiFilename() const
{
	return {};
}
std::string cAbstractFractal::getOpenCLFilename() const
{
	return {};
}
CVector3 wrap(CVector3 point, const CVector3 &period, const CVector3 &origin)
{
	point -= origin;
	return {point.x - period.x * floor(point.x / period.x) + origin.x,
			point.y - period.y * floor(point.y / period.y) + origin.y,
			point.z - period.z * floor(point.z / period.z) + origin.z};
}

#include "mandelbulber2_compat/parameters.h"

std::shared_ptr<cParameterContainer> mandelbulber2FractalDefaults();

static std::unique_ptr<cAbstractFractal> makeFormula(const std::string &name)
{
	std::unique_ptr<cAbstractFractal> result;
	int formula_id = -1;
	if (!name.empty() && name[0] == '#') {
		const char *begin = name.data() + 1;
		const char *end = name.data() + name.size();
		auto parsed = std::from_chars(begin, end, formula_id);
		if (parsed.ec != std::errc() || parsed.ptr != end || formula_id < 0)
			return nullptr;
	}
	auto add = [&]<class Formula>() {
		if constexpr (!std::is_same_v<Formula, cFractalCustom>) {
			if (!result) {
				auto formula = std::make_unique<Formula>();
				if (formula->getInternalName() == name ||
						(formula_id >= 0 &&
								static_cast<int>(formula->getInternalId()) == formula_id))
					result = std::move(formula);
			}
		}
	};
	// Reuse upstream's complete list without parsing formula source files.
#undef MANDELBULBER2_FORMULA_DEFINITION_ALL_FRACTAL_DEFINITIONS_H_
#undef FRACTAL_CLASS
#define FRACTAL_CLASS(formula) add.template operator()<formula>();
#include "formula/definition/all_fractal_definitions.h"
#undef FRACTAL_CLASS
	return result;
}

struct FmMandelbulber2::Impl
{
	FmMandelbulber2Options options;
	sFractal parameters;
	std::vector<std::unique_ptr<cAbstractFractal>> formulas;
	const cAbstractFractal *estimator = nullptr;

	Impl(const std::string &name, const FmMandelbulber2Options &o) :
			options(o), parameters(makeParameters(o))
	{
		std::istringstream names(name);
		for (std::string part; std::getline(names, part, ',');) {
			auto formula = makeFormula(part);
			if (!formula)
				throw std::invalid_argument("Unknown Mandelbulber2 CPU formula: " + part);
			if (formula->getDeFunctionType() != fractal::withoutDEFunction)
				estimator = formula.get();
			formulas.push_back(std::move(formula));
		}
		if (!estimator)
			throw std::invalid_argument(
					"Mandelbulber2 requires a formula with a distance estimator");
		if (options.iterations < 1 || !std::isfinite(options.power) ||
				options.power <= 0.0)
			throw std::invalid_argument("Invalid Mandelbulber2 iteration count or power");
	}
	static sFractal makeParameters(const FmMandelbulber2Options &options)
	{
		auto defaults = mandelbulber2FractalDefaults();
		defaults->addParam("power", options.power, 0, 0);
		if (!options.parameters.isNull())
			defaults->applyJson(options.parameters);
		return sFractal(defaults);
	}
};

FmMandelbulber2::FmMandelbulber2(const std::string &formula,
		const FmMandelbulber2Options &options) : impl(new Impl(formula, options))
{
}
FmMandelbulber2::~FmMandelbulber2() = default;

double FmMandelbulber2::distance(double x, double y, double z) const
{
	const auto &options = impl->options;
	const auto &parameters = impl->parameters;
	const auto &estimator = *impl->estimator;
	struct Orbit
	{
		CVector4 value;
		sExtendedAux aux{};
		int iterations = 0;
		bool escaped = false;
		bool valid = true;
	};
	auto iterate = [&](CVector4 point, int limit, bool bailout) {
		Orbit orbit;
		auto &v = orbit.value;
		auto &aux = orbit.aux;
		v = point;
		const CVector4 constant =
				options.julia
						? CVector4(options.julia_x, options.julia_y, options.julia_z, 0)
						: point * options.constant_factor;
		aux.c = aux.const_c = aux.old_z = point;
		aux.DE = aux.pos_neg = aux.color = aux.pseudoKleinianDE = 1;
		aux.dist = aux.temp1000 = 1000;
		aux.actualScale = parameters.mandelbox.scale;
		aux.r = v.Length();
		for (aux.i = 0; aux.i < limit; ++aux.i) {
			aux.old_z = v;
			auto &formula = *impl->formulas[aux.i % impl->formulas.size()];
			formula.FormulaCode(v, &parameters, aux);
			if (formula.getCpixelAddition() == fractal::cpixelEnabledByDefault) {
				const auto id = formula.getInternalId();
				if (id == fractal::aboxMod1 || id == fractal::amazingSurf)
					v += CVector4(constant.y, constant.x, constant.z, constant.w);
				else
					v += constant;
			}
			aux.r = v.Length();
			orbit.iterations++;
			if (!std::isfinite(aux.r) || !std::isfinite(aux.DE)) {
				orbit.valid = false;
				break;
			}
			if (bailout && aux.r > estimator.getDefaultBailout()) {
				orbit.escaped = true;
				break;
			}
		}
		return orbit;
	};
	const CVector4 point(x, y, z, 0);
	const Orbit orbit = iterate(point, options.iterations, true);
	const auto &v = orbit.value;
	const auto &aux = orbit.aux;
	const auto type = estimator.getDeAnalyticFunction();
	if (!orbit.valid)
		return 0;
	if (type == fractal::analyticFunctionCustomDE)
		return std::isfinite(aux.dist) ? std::max(0.0, aux.dist) : 0;
	if (!orbit.escaped && type != fractal::analyticFunctionJosKleinian &&
			type != fractal::analyticFunctionPseudoKleinian)
		return 0;

	double derivative = aux.DE;
	double numerator = aux.r;
	const double max_axis = std::max({std::abs(v.x), std::abs(v.y), std::abs(v.z)});
	if (estimator.getDeType() == fractal::deltaDEType) {
		const double step = std::max(point.Length() * 1e-14, 1e-5);
		auto partial = [&](CVector4 offset) {
			const Orbit shifted = iterate(point + offset, orbit.iterations, false);
			return shifted.valid ? std::abs(shifted.aux.r - aux.r) / step : 0.0;
		};
		const double dx = partial({step, 0, 0, 0});
		const double dy = partial({0, step, 0, 0});
		const double dz = partial({0, 0, step, 0});
		derivative = std::hypot(dx, dy, dz);
		switch (estimator.getDeFunctionType()) {
		case fractal::linearDEFunction:
			numerator *= 0.5;
			break;
		case fractal::logarithmicDEFunction:
			numerator *= 0.5 * std::log(aux.r);
			break;
		case fractal::maxAxisDEFunction:
			numerator = 0.5 * max_axis;
			derivative = std::max({dx, dy, dz});
			break;
		case fractal::pseudoKleinianDEFunction: {
			const double rxy = std::hypot(v.x, v.y);
			numerator = std::max(rxy - 0.92784, std::abs(rxy * v.z) / aux.r);
			break;
		}
		case fractal::josKleinianDEFunction:
			numerator = std::abs(std::hypot(v.x, v.z) * v.y) / aux.r;
			break;
		default:
			break;
		}
	} else {
		switch (type) {
		case fractal::analyticFunctionLogarithmic:
			numerator *= 0.5 * std::log(aux.r);
			break;
		case fractal::analyticFunctionIFS:
			numerator -= 2;
			break;
		case fractal::analyticFunctionMaxAxis:
			numerator = max_axis;
			break;
		case fractal::analyticFunctionPseudoKleinian: {
			const double rxy = std::hypot(v.x, v.y);
			numerator = std::max(rxy - aux.pseudoKleinianDE, std::abs(rxy * v.z) / aux.r);
			break;
		}
		case fractal::analyticFunctionJosKleinian: {
			const double y =
					parameters.transformCommon.spheresEnabled
							? std::min(v.y, parameters.transformCommon.foldingValue - v.y)
							: v.y;
			numerator = std::min(y, parameters.analyticDE.tweak005);
			derivative = std::max(derivative, parameters.analyticDE.offset1);
			break;
		}
		default:
			break;
		}
	}
	const double result = derivative > 0 ? numerator / derivative : 0;
	return std::isfinite(result) ? std::max(0.0, result) : 0;
}

#else
struct FmMandelbulber2::Impl
{
};
FmMandelbulber2::FmMandelbulber2(const std::string &, const FmMandelbulber2Options &)
{
	throw std::runtime_error(
			"Mandelbulber2 source was not available when Freeminer was built");
}
FmMandelbulber2::~FmMandelbulber2() = default;
double FmMandelbulber2::distance(double, double, double) const
{
	return 0.0;
}
#endif
