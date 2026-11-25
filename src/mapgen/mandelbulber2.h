#pragma once

#include <memory>
#include <string>
#include <json/json.h>

struct FmMandelbulber2Options
{
	int iterations = 15;
	double power = 8.0;
	double constant_factor = 1.0;
	bool julia = false;
	double julia_x = 0.0, julia_y = 0.0, julia_z = 0.0;
	Json::Value parameters;
};

class FmMandelbulber2
{
public:
	FmMandelbulber2(const std::string &formula, const FmMandelbulber2Options &options);
	~FmMandelbulber2();
	double distance(double x, double y, double z) const;

private:
	struct Impl;
	std::unique_ptr<Impl> impl;
};
