/*
mapgen_math.h
*/

/*
This file is part of Freeminer.

Freeminer is free software: you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation, either version 3 of the License, or
(at your option) any later version.

Freeminer  is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with Freeminer.  If not, see <http://www.gnu.org/licenses/>.
*/

#pragma once

#include "config.h"
#include "irr_v3d.h"
#include "mapgen/mapgen.h"
#include "mapgen/mapgen_v7.h"
#include "json/json.h"

#if USE_MANDELBULBER
#define MANDELBULBER_EMBEDDED
#include "mandelbulber/fractal.h"
#endif

struct MapgenMathParams : public MapgenV7Params
{

	MapgenMathParams() {}
	~MapgenMathParams() {}

	Json::Value params;

#if USE_MANDELBULBER
	sFractal par;
	enumCalculationMode mode;
#endif

	void readParams(const Settings *settings) override;
	void writeParams(Settings *settings) const override;
	void setDefaultSettings(Settings *settings) override;
};

class MapgenMath : public MapgenV7
{
public:
	MapgenMathParams *mg_params;

	MapgenType getType() const override { return MAPGEN_MATH; }
	MapgenMath(MapgenMathParams *mg_params, EmergeParams *emerge);
	~MapgenMath();

	void calculateNoise();
	int generateTerrain() override;
	void generateRidgeTerrain();
	//int getGroundLevelAtPoint(v2POS p);

	bool internal;
	bool invert;
	bool invert_yz;
	bool invert_xy;
	double size;
	v3f scale;
	v3f center;
	int iterations;
	double distance;
	double result_max;
	bool no_layers = false;

	MapNode n_air, n_water, n_stone;

	double (*func)(double, double, double, double, int, int);
	MapNode layers_get(float value, float max);
	std::pair<bool, double> calc_point(pos_t x, pos_t y, pos_t z);
	bool visible(const v3pos_t &p) override;
	bool surface_2d() override { return false; };
};
