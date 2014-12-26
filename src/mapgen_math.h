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

#ifndef MAPGEN_MATH_HEADER
#define MAPGEN_MATH_HEADER

#include "config.h"
#include "mapgen.h"
#include "mapgen_v7.h"
#include "json/json.h"

#if USE_MANDELBULBER
#define MANDELBULBER_EMBEDDED
#include "util/mathconstants.h"
//#include "mandelbulber/algebra.cpp"
#include "mandelbulber/fractal.h"
#endif

struct MapgenMathParams : public MapgenV7Params {

	MapgenMathParams() {}
	~MapgenMathParams() {}

	Json::Value params;

#if USE_MANDELBULBER
	sFractal par;
	enumCalculationMode mode;
#endif

	void readParams(Settings *settings);
	void writeParams(Settings *settings);
};

class MapgenMath : public MapgenV7 {
public:
	MapgenMathParams * mg_params;

	MapgenMath(int mapgenid, MapgenParams *mg_params, EmergeManager *emerge);
	~MapgenMath();

	virtual void calculateNoise();
	virtual int generateTerrain();
	int getGroundLevelAtPoint(v2POS p);

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

	MapNode n_air, n_water, n_stone;

	double (*func)(double, double, double, double, int);
	MapNode layers_get(float value, float max);
};

struct MapgenFactoryMath : public MapgenFactory {
	Mapgen *createMapgen(int mgid, MapgenParams *params, EmergeManager *emerge) {
		return new MapgenMath(mgid, params, emerge);
	};

	MapgenSpecificParams *createMapgenParams() {
		return new MapgenMathParams();
	};
};

#endif
