/*
mapgen_math.cpp
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

#include "irrlichttypes.h"
#include "servermap.h"

#include <cmath>
#include <functional>
// Legacy Mandelbulber generator names map to Mandelbulber2 formulas.
#include <map>
#include <cstdint>

#include "mapgen_math.h"
#include "voxel.h"
#include "mapblock.h"
#include "mapnode.h"
#include "map.h"
#include "nodedef.h"
#include "voxelalgorithms.h"
#include "profiler.h"
#include "settings.h"
#include "emerge.h"
#include "serverenvironment.h"
#include "mg_biome.h"
#include "log_types.h"

void MapgenMathParams::setDefaultSettings(Settings *settings)
{
	settings->setDefault("mgmath_spflags", flagdesc_mapgen_v7,
			MGV7_FLOATLANDS | MGV7_MOUNTAINS | MGV7_RIDGES | MGV7_CAVERNS);
}

/* TODO:
http://k3dsurf.s4.bizhat.com/k3dsurf-ftopic119-0-asc-15.html

(cos(x / (0.02 * (x^2+y^2+z^2))) * sin(y / (0.02 * (x^2+y^2+z^2)))
+cos(y / (0.02 * (x^2+y^2+z^2))) * sin(z / (0.02 * (x^2+y^2+z^2)))
+cos(z / (0.02 * (x^2+y^2+z^2))) * sin(x / (0.02 * (x^2+y^2+z^2)))+1.0)
-0.1*(1-0.016*((x^2+y^2+z^2)- 10/((0.35* x)^2+(0.35* y)^2+(0.35* z)^2)))
-0.03



F(): cos(x/(0.02*(x^4+y^4+z^4))) * sin(y/(0.02*(x^4+y^4+z^4)))
   + cos(y/(0.02*(x^4+y^4+z^4))) * sin(z/(0.02*(x^4+y^4+z^4)))
   + cos(z/(0.02*(x^4+y^4+z^4))) * sin(x/(0.02*(x^4+y^4+z^4)))
   + 1.2*(x*x+y*y+z*z)
   - 4.5
[x]: -2.2 , 2.2
[y]: -2.2 , 2.2
[z]: -2.2 , 2.2

rho(x,y,z)=3+50/(1+(sqrt(x*x+y*y+z*z)+1)^2)

F(x,y,z)=
 cos(x*rho(x,y,z))*sin(y*rho(x,y,z))
+cos(y*rho(x,y,z))*sin(z*rho(x,y,z))
+cos(z*rho(x,y,z))*sin(x*rho(x,y,z))
+2-16/(16+x^4+y^4+z^4)=0

x: [-2.0 , 2.0]
y: [-2.0 , 2.0]
z: [-2.0 , 2.0]


*/

inline double mandelbox(double x, double y, double z, double d, int nn = 10, int seed = 1)
{
	int s = 7;
	x *= s;
	y *= s;
	z *= s;
	d *= s;

	double posX = x;
	double posY = y;
	double posZ = z;

	double dr = 1.0;
	double r = 0.0;

	double scale = 2;

	double minRadius2 = 0.25;
	double fixedRadius2 = 1;

	for (int n = 0; n < nn; n++) {
		// Reflect
		if (x > 1.0)
			x = 2.0 - x;
		else if (x < -1.0)
			x = -2.0 - x;
		if (y > 1.0)
			y = 2.0 - y;
		else if (y < -1.0)
			y = -2.0 - y;
		if (z > 1.0)
			z = 2.0 - z;
		else if (z < -1.0)
			z = -2.0 - z;

		// Sphere Inversion
		double r2 = x * x + y * y + z * z;

		if (r2 < minRadius2) {
			x = x * fixedRadius2 / minRadius2;
			y = y * fixedRadius2 / minRadius2;
			z = z * fixedRadius2 / minRadius2;
			dr = dr * fixedRadius2 / minRadius2;
		} else if (r2 < fixedRadius2) {
			x = x * fixedRadius2 / r2;
			y = y * fixedRadius2 / r2;
			z = z * fixedRadius2 / r2;
			fixedRadius2 *= fixedRadius2 / r2;
		}

		x = x * scale + posX;
		y = y * scale + posY;
		z = z * scale + posZ;
		dr *= scale;
	}
	r = sqrt(x * x + y * y + z * z);
	return ((r / fabs(dr)) < d);
}

inline double mengersponge(
		double x, double y, double z, double d, int MI = 10, int seed = 1)
{
	double r = x * x + y * y + z * z;
	double scale = 3;
	int i = 0;

	for (i = 0; i < MI && r < 9; i++) {
		x = fabs(x);
		y = fabs(y);
		z = fabs(z);

		if (x - y < 0) {
			double x1 = y;
			y = x;
			x = x1;
		}
		if (x - z < 0) {
			double x1 = z;
			z = x;
			x = x1;
		}
		if (y - z < 0) {
			double y1 = z;
			z = y;
			y = y1;
		}

		x = scale * x - 1 * (scale - 1);
		y = scale * y - 1 * (scale - 1);
		z = scale * z;

		if (z > 0.5 * 1 * (scale - 1))
			z -= 1 * (scale - 1);
		r = x * x + y * y + z * z;
	}
	return ((sqrt(r)) * pow(scale, (-i)) < d);
}

inline double sphere(double x, double y, double z, double d, int ITR = 1, int seed = 1)
{
	return v3f(x, y, z).getLength() < d;
}

const unsigned FNV_32_PRIME = 0x01000193;
unsigned int fnv_32_buf(char *buf, size_t len = 0)
{
	unsigned int hval = 0x811c9dc5;

	while (len-- != 0) {
		hval *= FNV_32_PRIME;
		hval ^= (unsigned int)*buf++;
	}

	return hval;
}

inline double rooms(double dx, double dy, double dz, double d, int ITR = 1, int seed = 1)
{
	int x = dx, y = dy, z = dz;
	// if (x < y && x < z) return 0; // debug slice
	const auto rooms_pow_min = 2, rooms_pow_max = 9;
	const auto rooms_pow_cut_max = 4;
	const auto rooms_pow_fill_max = 5;
	const auto room_fill_every = 9;
	const auto room_big_every = 17;

	const auto rooms_limit = (MAX_MAP_GENERATION_LIMIT >> rooms_pow_max) << rooms_pow_max;
	// errorstream << " t "<<" x=" << x << " y="<< y << " x="<<z << " pw="<<pw<< " every="<<every<< " ty="<<((int)y%every)<<"\n";
	const int cxi = (x >> rooms_pow_max), cyi = (y >> rooms_pow_max),
			  czi = (z >> rooms_pow_max);
	int cx = cxi << rooms_pow_max, cy = cyi << rooms_pow_max, cz = czi << rooms_pow_max;
	int room_n = cxi + (cyi * MAX_MAP_GENERATION_LIMIT >> rooms_pow_max) +
				 (czi * (MAX_MAP_GENERATION_LIMIT >> (rooms_pow_max - 1)));
	int wall = 0;

	if (x > rooms_limit || y > rooms_limit || z > rooms_limit || x < -rooms_limit ||
			y < -rooms_limit || z < -rooms_limit)
		return 0;

	for (int pw2 = rooms_pow_max; pw2 >= rooms_pow_min; --pw2) {
		int every = 2 << pw2;
		// errorstream << " t "<<" x=" << x << " y="<< y << " x="<<z << " pw2="<<pw2<< " every="<<every<< " tx="<<((int)x%every)<<"\n";
		auto xhit = !(x % every), yhit = !(y % every), zhit = !(z % every);
		int lv = 1;
		lv += x < cx;
		lv += (y < cy) << 1;
		lv += (z < cz) << 2;
		// errorstream << " t "<<" x=" << x << " y="<< y << " z="<<z << " room_n=" << room_n << " pw2="<<pw2<< " hash=" << std::hash<int>()(room_n)<<" test="<<(!( std::hash<int>()(room_n) % 7))<< "\n";
		room_n = lv + room_n * 10;

		auto room_n_hash_1 = room_n + seed + 1;

		// errorstream << " fill "<<" x=" << x << " y="<< y << " z="<<z << " room_n=" << room_n << " cx=" << cx << " cy=" << cy << " cz=" << cz << " lv=" << lv << " pw2="<<pw2<< " seed=" << seed << " room_n_hash_1=" << room_n_hash_1 << " fnv=" << fnv_32_buf(reinterpret_cast<char*>(&room_n_hash_1), sizeof(room_n_hash_1)) << " sz=" << sizeof(room_n_hash_1) << "\n";

		if (pw2 < rooms_pow_fill_max &&
				!(fnv_32_buf(reinterpret_cast<char *>(&room_n_hash_1),
						  sizeof(room_n_hash_1)) %
						room_fill_every)) {
			// errorstream << " pw=" << pw  << " room_n="<<room_n<< " hash="<< fnv_32_buf(&room_n_hash_1, sizeof(room_n_hash_1))<<"\n";
			return pw2;
		}
		// errorstream << " t "<<" x=" << x << " y="<< y << " z="<<z << " room_n=" << room_n << " pw="<<pw<< " hash=" << std::hash<double>()(room_n + 1)<<" test="<<(!( std::hash<double>()(room_n + 1) % 13))<< " rf="<<room_filled<<"\n";

		auto room_n_hash_2 = room_n + seed + 2;

		// errorstream << " space "<<" x=" << x << " y=" << y << " z="<< z << " pw2=" << pw2 << " room_n=" << room_n << " seed=" << seed << " room_n_hash_2=" << room_n_hash_2 << " fnv=" << fnv_32_buf(reinterpret_cast<char*>(&room_n_hash_2), sizeof(room_n_hash_2))<< " sz=" << sizeof(room_n_hash_2) << "\n";

		if (pw2 <= rooms_pow_cut_max &&
				!(fnv_32_buf(reinterpret_cast<char *>(&room_n_hash_2),
						  sizeof(room_n_hash_2)) %
						room_big_every)) {
			int pw3 = pw2 + 1;
			// errorstream << " cutt "<<" x=" << x << " y="<< y << " z="<<z << " every="<< every<<" room_n=" << room_n  << " pw2="<<pw2<< " pw3" << pw3 << "(x >> pw3)="<<(x >> pw3)  << " == " << "((x - 1) >> pw3)=" <<  ((x - 1) >> pw3) << "\n";
			// errorstream << " x>>pw2" << (x>>pw2)  << " (x-1)>>pw2" << ((x-1)>>pw2) << " y>>pw2" << (y>>pw2)  << " (y-1)>>pw2" << ((y-1)>>pw2) << " z>>pw2" << (z>>pw2)  << " (z-1)>>pw2" << ((z-1)>>pw2) << "\n";
			if ((x >> pw3) == (x - 1) >> pw3 && (y >> pw3) == (y - 1) >> pw3 &&
					(z >> pw3) == (z - 1) >> pw3) {
				return 0;
			}
		}

		if (xhit || yhit || zhit) {
			wall = pw2;
		}

		// errorstream << " t "<<" x=" << x << " y="<< y << " z="<<z   <<" cx=" << cx << " cy="<< cy << " cz="<<cz<< "pw="<<pw<< " every="<<every<< " lv="<< lv << " room_n="<<room_n<< room_size="<<room_size <<"\n";
		const int room_size = 2 << (pw2 - 1);
		cx += ((x < cx) ? -1 : 1) * room_size;
		cy += ((y < cy) ? -1 : 1) * room_size;
		cz += ((z < cz) ? -1 : 1) * room_size;
	}
	return wall;
}

//////////////////////// Mapgen Math parameter read/write

void MapgenMathParams::readParams(const Settings *settings)
{
	try {
		MapgenV7Params::readParams(settings);
	} catch (...) {
	}
	auto mg_math = settings->getJson("mg_math");
	if (!mg_math.isNull())
		params = mg_math;
}

void MapgenMathParams::writeParams(Settings *settings) const
{
	settings->setJson("mg_math", params);
	try {
		MapgenV7Params::writeParams(settings);
	} catch (...) {
	}
}

///////////////////////////////////////////////////////////////////////////////

MapgenMath::MapgenMath(MapgenMathParams *params_, EmergeParams *emerge) :
		MapgenV7((MapgenV7Params *)params_, emerge)
{
	ndef = emerge->ndef;
	//mg_params = (MapgenMathParams *)params_->sparams;
	mg_params = params_;

	Json::Value &params = mg_params->params;

	if (params.get("light", 0).asBool())
		this->flags &= ~MG_LIGHT;

	n_air = MapNode(ndef->getId(params.get("air", "air").asString()), LIGHT_SUN);
	n_water = MapNode(
			ndef->getId(params.get("water_source", "mapgen_water_source").asString()),
			LIGHT_SUN);
	n_stone = MapNode(
			ndef->getId(params.get("stone", "mapgen_stone").asString()), LIGHT_SUN);

	if (params["generator"].empty()) {
		params["generator"] = "menger_sponge";
	}
	// The Mandelbulber2 adapter also handles legacy generator names.
	const std::string generator = params["generator"].asString();

	invert = params.get("invert", 1)
					 .asBool(); //params["invert"].empty()?1:params["invert"].asBool();
	invert_yz = params.get("invert_yz", 1).asBool();
	invert_xy = params.get("invert_xy", 0).asBool();
	size = params.get("size", static_cast<double>(MAX_MAP_GENERATION_LIMIT - 1000))
				   .asDouble(); // = max_r
	if (params.get("center", Json::Value()).isObject())
		center = v3f(params["center"]["x"].asDouble(), params["center"]["y"].asDouble(),
				params["center"]["z"].asDouble()); //v3f(5, -size - 5, 5);
	iterations = params.get("N", 15).asInt();	   //10;

	result_max = params.get("result_max", 1.0).asDouble();

	internal = 0;
	func = &sphere;

	//if (params["generator"].empty()) params["generator"] = "mandelbox";
	if (params["generator"].asString() == "mengersponge") {
		internal = 1;
		func = &mengersponge;
		size = params.get("size",
							 static_cast<double>((MAX_MAP_GENERATION_LIMIT - 1000) / 2))
					   .asDouble();
		//scale = params.get("scale", 1.0 / size).asDouble();
		iterations = params.get("N", 13).asInt();
		//if(!center.getLength()) center = v3f(-size, -size, -size);
		if (!center.getLength())
			center = v3f(-size / 3, -size / 3, -size / 3);
	} else if (params["generator"].asString() == "mandelbox") {
		internal = 1;
		func = &mandelbox;
		iterations = params.get("N", 15).asInt();
		size = params.get("size", 1000).asDouble();
		//scale = params.get("scale", 1.0 / size).asDouble();
		invert = params.get("invert", 0).asBool();
		//if(!center.getLength()) center = v3f(size * 0.3, -size * 0.6, size * 0.5);
		if (!center.getLength())
			center = v3f(size * 0.333333, -size * 0.666666, size * 0.5);
	} else if (params["generator"].asString() == "sphere") {
		internal = 1;
		func = &sphere;
		invert = params.get("invert", 0).asBool();
		size = params.get("size", 100).asDouble();
		//scale = params.get("scale", 1.0 / size).asDouble();
	} else if (params["generator"].asString() == "rooms") {
		internal = 1;
		func = &rooms;
		invert = params.get("invert", 0).asBool();
		//result_max = 10;
		size = params.get("size", 1).asDouble();
	}

	// Translate legacy names and parameters to Mandelbulber2.
	if (!internal) {
		std::string formula = generator;
		if (formula.compare(0, 4, "mb2:") == 0)
			formula.erase(0, 4);
		else {
			static const std::map<std::string, std::string> names = {
					{"trig_DE", "mandelbulb"}, {"trig_optim", "mandelbulb"},
					{"fast_trig", "mandelbulb_power2"}, {"tglad", "mandelbox"},
					{"kaleidoscopic", "kaleidoscopic_ifs"},
					{"foldingIntPow2", "box_fold_bulb_pow2"},
					{"smoothMandelbox", "mandelbox_smooth"},
					{"mandelboxVaryScale4D", "mandelbox_vary_scale4d"},
					{"generalizedFoldBox", "generalized_fold_box"}};
			auto found = names.find(formula);
			if (found != names.end())
				formula = found->second;
		}
		FmMandelbulber2Options options;
		options.iterations =
				params.get("N", generator == "hypercomplex" ? 20 : 15).asInt();
		options.power =
				params.get("power", generator.compare(0, 4, "mb2:") == 0 ? 8.0 : 9.0)
						.asDouble();
		options.constant_factor = params.get("fractal_constant_factor", 1.0).asDouble();
		options.julia = params.get("julia_mode", false).asBool();
		options.julia_x = params.get("julia_a", 0.0).asDouble();
		options.julia_y = params.get("julia_b", 0.0).asDouble();
		options.julia_z = params.get("julia_c", 0.0).asDouble();
		options.parameters = params.get("mandelbulber", Json::Value(Json::objectValue));
		for (const auto &name : {"cadd", "mandelbox_scale", "mandelbox_folding_limit",
					 "mandelbox_folding_value", "mandelbox_folding_min_radius",
					 "mandelbox_folding_fixed_radius", "mandelbox_sharpness",
					 "mandelbox_vary_fold", "mandelbox_vary_minr",
					 "mandelbox_vary_rpower", "mandelbox_vary_scale_vary",
					 "mandelbox_vary_wadd", "IFS_scale"}) {
			if (params.isMember(name) && !options.parameters.isMember(name))
				options.parameters[name] = params[name];
		}
		mandelbulber2.reset(new FmMandelbulber2(formula, options));
		if (generator == "menger_sponge") {
			size = params.get("size", 15000).asDouble();
			if (!center.getLength())
				center = v3f(4995, 5005, 4995);
		} else if (generator == "mandelbulb2" && !center.getLength()) {
			center = v3f(5, -size - 5, 0);
		}
	}

	float s = 1.0 / size;
	scale = v3f(s, s, s);
	if (params["scale"].isDouble()) {
		s = params.get("scale", 1.0 / size).asDouble();
		scale = v3f(s, s, s);
	} else if (params.get("scale", Json::Value()).isObject())
		scale = v3f(params["scale"].get("x", 1.0 / size).asDouble(),
				params["scale"].get("y", 1.0 / size).asDouble(),
				params["scale"].get("z", 1.0 / size).asDouble());

	auto center_auto_top = params.get("center_auto_top", Json::Value(false));
	if (center_auto_top.isBool() && center_auto_top.asBool() &&
			!params.get("center", Json::Value()).isObject() && !center.getLength())
		center = v3f(3, -1.0 / (1.0 / size) + (-5 - (-(int)invert * 10)), 3);
}

MapgenMath::~MapgenMath()
{
}

//////////////////////// Map generator

MapNode MapgenMath::layers_get(float value, float max)
{
	auto layer_index = rangelim((unsigned int)myround((value / max) * layers_node_size),
			0, layers_node_size - 1);
	//errorstream<<"lsM: "<< " layer_index="<<layer_index<< " value="<<value<<" max="<< max<<" noise_layers_width="<<noise_layers_width<<" layers_node_size="<<layers_node_size<<std::endl;
	return layers_node[layer_index];
}

std::pair<bool, double> MapgenMath::calc_point(pos_t x, pos_t y, pos_t z)
{
	v3f vec = (v3f(x, y, z) - center) * scale;
	if (invert_xy)
		std::swap(vec.X, vec.Y);
	if (invert_yz)
		std::swap(vec.Y, vec.Z);
	double d = 0;
	// Evaluate the selected Mandelbulber2 or built-in formula.
	if (mandelbulber2)
		d = mandelbulber2->distance(vec.X, vec.Y, vec.Z);
	else if (internal)
		d = (*func)(vec.X, vec.Y, vec.Z, scale.X, iterations, mg_params->seed);

	return {(!invert && d > 0) || (invert && d == 0), d};
}

bool MapgenMath::visible(
		const v3pos_t &p, std::optional<pos_t> surface_y, block_step_t step)
{
	const bool have = calc_point(p.X, p.Y, p.Z).first;
	return have;
}

MapNode MapgenMath::visible_content(const v3pos_t &p, bool use_weather, block_step_t step)
{
	const auto valid = [](const MapNode &node) {
		const content_t content = node.getContent();
		return content != CONTENT_IGNORE && content != CONTENT_UNKNOWN &&
			   content != CONTENT_AIR;
	};
	const auto node_or = [&valid](const MapNode &node, const MapNode &fallback) {
		return valid(node) ? node : fallback;
	};

	const bool have = calc_point(p.X, p.Y, p.Z).first;
	if (have && p.Y > water_level)
		return Mapgen::visible_content(p, use_weather, step);

	const float timeofday = env ? env->getTimeOfDayF() : 0.0f;
	const float totaltime = env ? env->getGameTime() * env->m_time_of_day_speed : 0.0f;
	const bool weather = use_weather && env && env->m_use_weather;
	const auto heat = calcBlockHeat(p, seed, timeofday, totaltime, weather);

	if (have) {
		const auto humidity = calcBlockHumidity(p, seed, timeofday, totaltime, weather);
		return visible_surface_by_climate(heat, humidity);
	}

	if (p.Y <= water_level) {
		if (heat < 0 && p.Y > heat / 3)
			return node_or(MapNode(c_ice, LIGHT_SUN), visible_ice);

		return node_or(n_water, visible_water);
	}

	return visible_transparent;
}

int MapgenMath::generateTerrain()
{

	MapNode n_ice(c_ice);
	//u32 index = 0;
	auto em = vm->m_area.getExtent();
	/* debug
	v3f vec0 = (v3f(node_min.X, node_min.Y, node_min.Z) - center) * scale ;
	errorstream << " X=" << node_min.X << " Y=" << node_min.Y << " Z=" << node_min.Z
	            //<< " N="<< mengersponge(vec0.X, vec0.Y, vec0.Z, distance, iterations)
	            << " N=" << (*func)(vec0.X, vec0.Y, vec0.Z, distance, iterations)
	            << " Sc=" << scale << " gen=" << params["generator"].asString() << " J=" << Json::FastWriter().write(params) << std::endl;
	*/
	//errorstream << Json::StyledWriter().write( mg_params->params ).c_str()<< std::endl;
	//errorstream << " iterations="<<iterations<< " scale="<<scale <<" invert="<<invert<< std::endl;

	for (pos_t z = node_min.Z; z <= node_max.Z; z++) {
		for (pos_t x = node_min.X; x <= node_max.X; x++) {
			const auto heat =
					m_emerge->env->m_use_weather
							? m_emerge->env->getServerMap().updateBlockHeat(m_emerge->env,
									  v3pos_t(x, node_max.Y, z), nullptr, &heat_cache)
							: 0;

			u32 index3d = (z - node_min.Z) * zstride_1u1d + (x - node_min.X);
			u32 vi = vm->m_area.index(x, node_min.Y - y_oversize_down, z);
			for (pos_t y = node_min.Y - y_oversize_down; y <= node_max.Y + y_oversize_up;
					y++, index3d += ystride, VoxelArea::add_y(em, vi, 1)
					//cache_index++
			) {
				//for (pos_t y = node_min.Y - y_oversize_down; y <= node_max.Y + y_oversize_up; y++, index3d += ystride) {
				auto [have, d] = calc_point(x, y, z);
				if ((!invert && d > 0) || (invert && d == 0)) {
					if (!vm->m_data[vi]) {
						//vm->m_data[i] = (y > water_level + biome->filler) ?
						//     MapNode(biome->c_filler) : n_stone;
						if (invert || !no_layers) {
							vm->m_data[vi] = Mapgen_features::layers_get(index3d);
						} else {
							vm->m_data[vi] = layers_get(d, result_max);
						}
						//						vm->m_data[i] = (y > water_level + biome->filler) ?
						//						     MapNode(biome->c_filler) : layers_get(d, result_max);
					}
				} else if (y <= water_level) {
					vm->m_data[vi] = (heat < 0 && y > heat / 3) ? n_ice : n_water;
				} else {
					vm->m_data[vi] = n_air;
				}
				//vm->m_area.add_y(em, i, 1);
			}
		}
	}
	return 0;
}

void MapgenMath::generateRidgeTerrain()
{
}

void MapgenMath::calculateNoise()
{
//delete after merge?
#if 0
	//TimeTaker t("calculateNoise", NULL, PRECISION_MICRO);
	int x = node_min.X;
	int y = node_min.Y - y_offset;
	int z = node_min.Z;

	if (flags & MG_CAVES) {
		noise_cave1->perlinMap3D(x, y - 1, z);
		noise_cave2->perlinMap3D(x, y - 1, z);
	}

	noise_filler_depth->perlinMap2D(x, z);

	//noise_heat->perlinMap2D(x, z);
	//noise_humidity->perlinMap2D(x, z);
#endif
}
