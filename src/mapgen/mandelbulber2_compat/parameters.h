#pragma once

#include <json/json.h>
#include <stdexcept>
#include <string>
#include <unordered_map>
#include <variant>
#include <vector>
#include "src/fractal.h"

// The CPU adapter supplies the subset of the parameter container used by fractal.cpp.
#define MANDELBULBER2_SRC_PARAMETERS_HPP_
#define MANDELBULBER2_SRC_WRITE_LOG_HPP_

using QString = std::string;
using QStringList = std::vector<std::string>;
enum
{
	morphNone,
	morphLinear,
	morphAkima,
	morphAkimaAngle,
	paramStandard
};
inline void WriteLog(const std::string &, int)
{
}

class cParameterContainer
{
	using Value = std::variant<int, bool, double, CVector3, CVector4, std::string>;
	std::unordered_map<std::string, Value> values;

public:
	std::string GetContainerName() const { return "fractal0"; }

	void applyJson(const Json::Value &overrides)
	{
		if (!overrides.isObject())
			throw std::invalid_argument("mandelbulber parameters must be a JSON object");
		for (const auto &name : overrides.getMemberNames()) {
			auto it = values.find(name);
			if (it == values.end())
				throw std::invalid_argument("Unknown Mandelbulber2 parameter: " + name);
			const auto &value = overrides[name];
			auto component = [&](Json::ArrayIndex index, const char *key) {
				const Json::Value &v = value.isArray() ? value[index] : value[key];
				if (!v.isNumeric())
					throw std::invalid_argument("Invalid vector parameter: " + name);
				return v.asDouble();
			};
			if (std::holds_alternative<int>(it->second) && value.isInt())
				it->second = value.asInt();
			else if (std::holds_alternative<double>(it->second) && value.isNumeric())
				it->second = value.asDouble();
			else if (std::holds_alternative<bool>(it->second) && value.isBool())
				it->second = value.asBool();
			else if (std::holds_alternative<std::string>(it->second) && value.isString())
				it->second = value.asString();
			else if (std::holds_alternative<CVector3>(it->second) &&
					 (value.isArray() || value.isObject()))
				it->second =
						CVector3(component(0, "x"), component(1, "y"), component(2, "z"));
			else if (std::holds_alternative<CVector4>(it->second) &&
					 (value.isArray() || value.isObject()))
				it->second = CVector4(component(0, "x"), component(1, "y"),
						component(2, "z"), component(3, "w"));
			else
				throw std::invalid_argument(
						"Invalid Mandelbulber2 parameter type: " + name);
		}
	}

	template <class T>
	void addParam(const char *name, T value, int, int, QStringList = {})
	{
		values.insert_or_assign(name, value);
	}
	template <class T>
	void addParam(const char *name, T value, T, T, int, int)
	{
		addParam(name, value, 0, 0);
	}
	template <class T>
	void addParam(const char *name, int index, T value, int, int)
	{
		values.insert_or_assign(std::string(name) + "_" + std::to_string(index), value);
	}
	template <class T>
	T Get(const std::string &name) const
	{
		auto it = values.find(name);
		if (it == values.end())
			throw std::logic_error("Missing Mandelbulber2 parameter default: " + name);
		if (const auto *value = std::get_if<T>(&it->second))
			return *value;
		throw std::logic_error("Mandelbulber2 parameter type mismatch: " + name);
	}
	template <class T>
	T Get(const char *name, int index) const
	{
		return Get<T>(std::string(name) + "_" + std::to_string(index));
	}
};
