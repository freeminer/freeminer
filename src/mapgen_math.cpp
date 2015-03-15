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

#include <cmath>
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
#include "environment.h"
#include "mg_biome.h"
#include "log_types.h"

// can use ported lib from http://mandelbulber.googlecode.com/svn/trunk/src
#if USE_MANDELBULBER
#include "mandelbulber/algebra.cpp"
#include "mandelbulber/fractal.cpp"
#endif

double mandelbox(double x, double y, double z, double d, int nn = 10) {
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

double mengersponge(double x, double y, double z, double d, int MI = 10) {
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

double sphere(double x, double y, double z, double d, int ITR = 1) {
	return v3f(x, y, z).getLength() < d;
}

//////////////////////// Mapgen Math parameter read/write

void MapgenMathParams::readParams(Settings *settings) {
	try {
		MapgenV7Params::readParams(settings);
	} catch (...) {}
	params = settings->getJson("mg_math");
}

void MapgenMathParams::writeParams(Settings *settings) const {
	settings->setJson("mg_math", params);
	try {
		MapgenV7Params::writeParams(settings);
	} catch (...) {}
}

///////////////////////////////////////////////////////////////////////////////

MapgenMath::MapgenMath(int mapgenid, MapgenParams *params_, EmergeManager *emerge) : MapgenV7(mapgenid, params_, emerge) {
	ndef = emerge->ndef;
	mg_params = (MapgenMathParams *)params_->sparams;

	Json::Value & params = mg_params->params;

	if (params.get("light", 0).asBool())
		this->flags &= ~MG_LIGHT;

	n_air		= MapNode(ndef, params.get("air", "air").asString(), LIGHT_SUN);
	n_water	= MapNode(ndef, params.get("water_source", "mapgen_water_source").asString(), LIGHT_SUN);
	n_stone		= MapNode(ndef, params.get("stone", "mapgen_stone").asString(), LIGHT_SUN);

	if (params["generator"].empty()) {
		params["generator"] = "menger_sponge";
	}

	invert = params.get("invert", 1).asBool(); //params["invert"].empty()?1:params["invert"].asBool();
	invert_yz = params.get("invert_yz", 1).asBool();
	invert_xy = params.get("invert_xy", 0).asBool();
	size = params.get("size", (MAP_GENERATION_LIMIT - 1000)).asDouble(); // = max_r
	if (!params.get("center", Json::Value()).empty()) 
		center = v3f(params["center"]["x"].asDouble(), params["center"]["y"].asDouble(), params["center"]["z"].asDouble()); //v3f(5, -size - 5, 5);
	iterations = params.get("N", 15).asInt(); //10;

	result_max = params.get("result_max", 1.0).asDouble();

	internal = 0;
	func = &sphere;

	//if (params["generator"].empty()) params["generator"] = "mandelbox";
	if (params["generator"].asString() == "mengersponge") {
		internal = 1;
		func = &mengersponge;
		size = params.get("size", (MAP_GENERATION_LIMIT - 1000) / 2).asDouble();
		//scale = params.get("scale", 1.0 / size).asDouble();
		iterations = params.get("N", 13).asInt();
		//if(!center.getLength()) center = v3f(-size, -size, -size);
		if(!center.getLength()) center = v3f(-size / 3, -size / 3, -size / 3);
	} else if (params["generator"].asString() == "mandelbox") {
		internal = 1;
		func = &mandelbox;
		iterations = params.get("N", 15).asInt();
		size = params.get("size", 1000).asDouble();
		//scale = params.get("scale", 1.0 / size).asDouble();
		invert = params.get("invert", 0).asBool();
		//if(!center.getLength()) center = v3f(size * 0.3, -size * 0.6, size * 0.5);
		if(!center.getLength()) center = v3f(size * 0.333333, -size * 0.666666, size * 0.5);
	} else if (params["generator"].asString() == "sphere") {
		internal = 1;
		func = &sphere;
		invert = params.get("invert", 0).asBool();
		size = params.get("size", 100).asDouble();
		//scale = params.get("scale", 1.0 / size).asDouble();
	}


#if USE_MANDELBULBER
	sFractal & par = mg_params->par;
	//par.minN = params.get("minN", 1).asInt();

	par.limits_enabled  = params.get("limits_enabled", 0).asBool();
	par.iterThresh  = params.get("iteration_threshold_mode", 0).asBool();
	par.analitycDE  = params.get("analityc_DE_mode", 0).asBool();
	par.juliaMode  = params.get("julia_mode", 0).asBool();
	par.tgladFoldingMode  = params.get("tglad_folding_mode", 0).asBool();
	par.sphericalFoldingMode  = params.get("spherical_folding_mode", 0).asBool();
	par.interiorMode  = params.get("interior_mode", 0).asBool();
	par.hybridCyclic  = params.get("hybrid_cyclic", 0).asBool();
	par.linearDEmode  = params.get("linear_DE_mode", 0).asBool();
	par.constantDEThreshold  = params.get("constant_DE_threshold", 0).asBool();

	par.frameNo = params.get("frameNo", 1).asInt();
	par.itersOut = params.get("itersOut", 1).asInt();
	par.fakeLightsMinIter = params.get("fakeLightsMinIter", 1).asInt();

	par.doubles.N = params.get("N", iterations).asDouble();
	par.doubles.constantFactor = params.get("fractal_constant_factor", 1.0).asDouble();
	par.doubles.FoldingIntPowZfactor = params.get("FoldingIntPow_z_factor", 1).asDouble();
	par.doubles.FoldingIntPowFoldFactor = params.get("FoldingIntPow_folding_factor", 1).asDouble();
	par.doubles.foldingSphericalFixed = params.get("foldingSphericalFixed", 1.0).asDouble();
	par.doubles.foldingSphericalMin = params.get("foldingSphericalMin", 0.5).asDouble();
	par.doubles.detailSize = params.get("detailSize", 1).asDouble();
	par.doubles.power = params.get("power", 9.0).asDouble();
	par.doubles.cadd = params.get("cadd", 1).asDouble();
	par.doubles.julia.x = params.get("julia_a", 0).asDouble();
	par.doubles.julia.y = params.get("julia_b", 0).asDouble();
	par.doubles.julia.z = params.get("julia_c", 0).asDouble();
	par.doubles.foldingLimit = params.get("folding_limit", 1.0).asDouble();
	par.doubles.foldingValue = params.get("folding_value", 2.0).asDouble();
	par.mandelbox.doubles.scale = params.get("mandelbox_scale", 2).asDouble();
	par.mandelbox.doubles.foldingLimit = params.get("mandelbox_folding_limit", 1.0).asDouble();
	par.mandelbox.doubles.foldingValue = params.get("mandelbox_folding_value", 2.0).asDouble();
	par.mandelbox.doubles.foldingSphericalMin = params.get("mandelbox_folding_min_radius", 0.5).asDouble();
	par.mandelbox.doubles.foldingSphericalFixed = params.get("mandelbox_folding_fixed_radius", 1.0).asDouble();
	par.mandelbox.doubles.sharpness = params.get("mandelbox_sharpness", 3).asDouble();
	par.mandelbox.doubles.offset.x = params.get("mandelbox_offset_X", 0).asDouble();
	par.mandelbox.doubles.offset.y = params.get("mandelbox_offset_Y", 0).asDouble();
	par.mandelbox.doubles.offset.z = params.get("mandelbox_offset_Z", 0).asDouble();
	par.mandelbox.doubles.colorFactorX = params.get("mandelbox_color_X", 0.03).asDouble();
	par.mandelbox.doubles.colorFactorY = params.get("mandelbox_color_Y", 0.05).asDouble();
	par.mandelbox.doubles.colorFactorZ = params.get("mandelbox_color_Z", 0.07).asDouble();
	par.mandelbox.doubles.colorFactorR = params.get("mandelbox_color_R", 0).asDouble();
	par.mandelbox.doubles.colorFactorSp1 = params.get("mandelbox_color_Sp1", 0.2).asDouble();
	par.mandelbox.doubles.colorFactorSp2 = params.get("mandelbox_color_Sp2", 1).asDouble();
	par.mandelbox.doubles.solid = params.get("mandelbox_solid", 1).asDouble();
	par.mandelbox.doubles.melt = params.get("mandelbox_melt", 0).asDouble();
	par.mandelbox.rotationsEnabled  = params.get("mandelbox_rotation_enabled", 0).asBool();
	par.mandelbox.doubles.vary4D.scaleVary =  params.get("mandelbox_vary_scale_vary", 0.1).asDouble();
	par.mandelbox.doubles.vary4D.fold = params.get("mandelbox_vary_fold", 1).asDouble();
	par.mandelbox.doubles.vary4D.minR = params.get("mandelbox_vary_minr", 0.5).asDouble();
	par.mandelbox.doubles.vary4D.rPower = params.get("mandelbox_vary_rpower", 1).asDouble();
	par.mandelbox.doubles.vary4D.wadd = params.get("mandelbox_vary_wadd", 0).asDouble();
	//par.formulaSequence = params.get("formulaSequence", 1).asDouble();
	//vector3 par.IFS.doubles.offset = params.get("IFS.doubles.offset", 1).asDouble();
	par.IFS.doubles.scale = params.get("IFS_scale", 2).asDouble();
	par.IFS.doubles.rotationAlfa = params.get("IFS_rot_alfa", 0).asDouble();
	par.IFS.doubles.rotationBeta = params.get("IFS_rot_beta", 0).asDouble();
	par.IFS.doubles.rotationGamma = params.get("IFS_rot_gamma", 0).asDouble();
	par.IFS.doubles.offset.x = params.get("IFS_offsetX", 1).asDouble();
	par.IFS.doubles.offset.y = params.get("IFS_offsetY", 0).asDouble();
	par.IFS.doubles.offset.z = params.get("IFS_offsetZ", 0).asDouble();
	par.IFS.doubles.edge.x = params.get("IFS_edgeX", 0).asDouble();
	par.IFS.doubles.edge.y = params.get("IFS_edgeY", 0).asDouble();
	par.IFS.doubles.edge.z = params.get("IFS_edgeZ", 0).asDouble();
	par.IFS.absX = params.get("IFS.absX", 0).asBool();
	par.IFS.absY = params.get("IFS.absY", 0).asBool();
	par.IFS.absZ = params.get("IFS.absZ", 0).asBool();
	par.IFS.mengerSpongeMode = params.get("IFS_menger_sponge_mode", 0).asBool();
	par.IFS.foldingMode = params.get("IFS_folding_mode", 0).asBool();
	//par.IFS.foldingCount = params.get("IFS.foldingCount", 1).asInt();

	if (params["mode"].asString() == "")
		mg_params->mode = normal_mode;
	if (params["mode"].asString() == "normal")
		mg_params->mode = normal_mode;
	if (params["mode"].asString() == "colouring")
		mg_params->mode = colouring;
	if (params["mode"].asString() == "fake_AO")
		mg_params->mode = fake_AO;
	if (params["mode"].asString() == "deltaDE1")
		mg_params->mode = deltaDE1;
	if (params["mode"].asString() == "deltaDE2")
		mg_params->mode = deltaDE2;
	if (params["mode"].asString() == "orbitTrap")
		mg_params->mode = orbitTrap;

	if (params["mandelbox_fold_mode"].asString() == "foldTet")
		par.genFoldBox.type = foldTet;
	if (params["mandelbox_fold_mode"].asString() == "foldCube")
		par.genFoldBox.type = foldCube;
	if (params["mandelbox_fold_mode"].asString() == "foldOct")
		par.genFoldBox.type = foldOct;
	if (params["mandelbox_fold_mode"].asString() == "foldDodeca")
		par.genFoldBox.type = foldDodeca;
	if (params["mandelbox_fold_mode"].asString() == "foldOctCube")
		par.genFoldBox.type = foldOctCube;
	if (params["mandelbox_fold_mode"].asString() == "foldIcosa")
		par.genFoldBox.type = foldIcosa;
	if (params["mandelbox_fold_mode"].asString() == "foldBox6")
		par.genFoldBox.type = foldBox6;
	if (params["mandelbox_fold_mode"].asString() == "foldBox5")
		par.genFoldBox.type = foldBox5;

	if (params["generator"].asString() == "none")
		par.formula = none;
	if (params["generator"].asString() == "trig_DE")
		par.formula = trig_DE;
	if (params["generator"].asString() == "trig_optim")
		par.formula = trig_optim;
	if (params["generator"].asString() == "fast_trig")
		par.formula = fast_trig;
	if (params["generator"].asString() == "hypercomplex")
		par.formula = hypercomplex;
	if (params["generator"].asString() == "quaternion")
		par.formula = quaternion;
	if (params["generator"].asString() == "minus_fast_trig")
		par.formula = minus_fast_trig;
	if (params["generator"].asString() == "menger_sponge")
		par.formula = menger_sponge;
	if (params["generator"].asString() == "tglad")
		par.formula = tglad;
	if (params["generator"].asString() == "kaleidoscopic")
		par.formula = kaleidoscopic;
	if (params["generator"].asString() == "xenodreambuie")
		par.formula = xenodreambuie;
	if (params["generator"].asString() == "hybrid")
		par.formula = hybrid;
	if (params["generator"].asString() == "mandelbulb2")
		par.formula = mandelbulb2;
	if (params["generator"].asString() == "mandelbulb3")
		par.formula = mandelbulb3;
	if (params["generator"].asString() == "mandelbulb4")
		par.formula = mandelbulb4;
	if (params["generator"].asString() == "foldingIntPow2")
		par.formula = foldingIntPow2;
	if (params["generator"].asString() == "smoothMandelbox")
		par.formula = smoothMandelbox;
	if (params["generator"].asString() == "mandelboxVaryScale4D")
		par.formula = mandelboxVaryScale4D;
	if (params["generator"].asString() == "aexion")
		par.formula = aexion;
	if (params["generator"].asString() == "benesi")
		par.formula = benesi;
	if (params["generator"].asString() == "bristorbrot")
		par.formula = bristorbrot;
	if (params["generator"].asString() == "invertX")
		par.formula = invertX;
	if (params["generator"].asString() == "invertY")
		par.formula = invertY;
	if (params["generator"].asString() == "invertZ")
		par.formula = invertZ;
	if (params["generator"].asString() == "invertR")
		par.formula = invertR;
	if (params["generator"].asString() == "sphericalFold")
		par.formula = sphericalFold;
	if (params["generator"].asString() == "powXYZ")
		par.formula = powXYZ;
	if (params["generator"].asString() == "scaleX")
		par.formula = scaleX;
	if (params["generator"].asString() == "scaleY")
		par.formula = scaleY;
	if (params["generator"].asString() == "scaleZ")
		par.formula = scaleZ;
	if (params["generator"].asString() == "offsetX")
		par.formula = offsetX;
	if (params["generator"].asString() == "offsetY")
		par.formula = offsetY;
	if (params["generator"].asString() == "offsetZ")
		par.formula = offsetZ;
	if (params["generator"].asString() == "angleMultiplyX")
		par.formula = angleMultiplyX;
	if (params["generator"].asString() == "angleMultiplyY")
		par.formula = angleMultiplyY;
	if (params["generator"].asString() == "angleMultiplyZ")
		par.formula = angleMultiplyZ;
	if (params["generator"].asString() == "generalizedFoldBox")
		par.formula = generalizedFoldBox;
	if (params["generator"].asString() == "ocl_custom")
		par.formula = ocl_custom;

	char parameterName[100];
	for (int i = 1; i <= HYBRID_COUNT; ++i) {
		sprintf(parameterName, "hybrid_formula_%d", i);
		par.hybridFormula[i - 1] = (enumFractalFormula)params.get(parameterName, i == 5 ? 2 : 0).asInt();
		sprintf(parameterName, "hybrid_iterations_%d", i);
		par.hybridIters[i - 1] = params.get(parameterName, 1).asDouble();
		sprintf(parameterName, "hybrid_power_%d", i);
		par.doubles.hybridPower[i - 1] = params.get(parameterName, 2).asDouble();
	}

	if (params["generator"].asString() == "mandelboxVaryScale4D") {
		par.doubles.N = params.get("N", 50).asInt();
		//scale = params.get("scale", 1).asDouble();
	}

	if (params["generator"].asString() == "menger_sponge") {
		invert = params.get("invert", 1).asBool();
		//size = params.get("size", (MAP_GENERATION_LIMIT - 1000) / 2).asDouble();
		//if(!center.getLength()) center = v3f(-1.0 / scale / 2, -1.0 / scale + (-2 * -(int)invert), 2);
		size = params.get("size", 15000 ).asDouble();
		//scale = params.get("scale", 1.0 / size).asDouble(); //(double)1 / size;
		if(!center.getLength()) center = v3f(5000-5,5000+5,5000-5);
		//par.doubles.N = params.get("N", 4).asInt();
	}

	if (params["generator"].asString() == "mandelbulb2") {
		if(!center.getLength()) center = v3f(5, -1.0 / (1.0 / size) - 5, 0); //ok
	}
	if (params["generator"].asString() == "hypercomplex") {
		par.doubles.N = params.get("N", 20).asInt();
		//scale = params.get("scale", 0.0001).asDouble();
	}
#endif

	float s = 1.0 / size;
	scale = v3f(s,s,s);
	if (params["scale"].isDouble()) {
		s = params.get("scale", 1.0 / size).asDouble();
		scale = v3f(s,s,s);
	}
	else if (!params.get("scale", Json::Value()).empty())
		scale = v3f(params["scale"].get("x", 1.0 / size).asDouble(), params["scale"].get("y", 1.0 / size).asDouble(), params["scale"].get("z", 1.0 / size).asDouble());


	if (params["center_auto_top"].asBool() && params.get("center", Json::Value()).empty() && !center.getLength()) center = v3f(3, -1.0 / (1.0 / size) + (-5 - (-(int)invert * 10)), 3);
}

MapgenMath::~MapgenMath() {
}

//////////////////////// Map generator

MapNode MapgenMath::layers_get(float value, float max) {
	auto layer_index = rangelim((unsigned int)myround((value/max) * layers_node.size()), 0, layers_node.size()-1);
	return layers_node[layer_index];
}

int MapgenMath::generateTerrain() {

	MapNode n_ice(c_ice);
	u32 index = 0;
	v3POS em = vm->m_area.getExtent();

	/* debug
	v3f vec0 = (v3f(node_min.X, node_min.Y, node_min.Z) - center) * scale ;
	errorstream << " X=" << node_min.X << " Y=" << node_min.Y << " Z=" << node_min.Z
	            //<< " N="<< mengersponge(vec0.X, vec0.Y, vec0.Z, distance, iterations)
	            << " N=" << (*func)(vec0.X, vec0.Y, vec0.Z, distance, iterations)
	            << " Sc=" << scale << " gen=" << params["generator"].asString() << " J=" << Json::FastWriter().write(params) << std::endl;
	*/
	//errorstream << Json::StyledWriter().write( mg_params->params ).c_str()<< std::endl;
	//errorstream << " iterations="<<iterations<< " scale="<<scale <<" invert="<<invert<< std::endl;

#if USE_MANDELBULBER
	v3f vec0(node_min.X, node_min.Y, node_min.Z);
	vec0 = (vec0 - center) * scale;
	/*
		errorstream << " X=" << node_min.X << " Y=" << node_min.Y << " Z=" << node_min.Z
		            << " N=" << Compute<normal_mode>(CVector3(vec0.X, vec0.Y, vec0.Z), mg_params->par)
		            //<<" F="<< Compute<fake_AO>(CVector3(node_min.X,node_min.Y,node_min.Z), par)
		            //<<" L="<<node_min.getLength()<< " -="<<node_min.getLength() - Compute<normal_mode>(CVector3(node_min.X,node_min.Y,node_min.Z), par)
		            << " Sc=" << scale << " internal=" << internal
		            << std::endl;
	*/
#endif

	double d = 0;
	for (s16 z = node_min.Z; z <= node_max.Z; z++) {
		for (s16 x = node_min.X; x <= node_max.X; x++, index++) {
			s16 heat = m_emerge->env->m_use_weather ? m_emerge->env->getServerMap().updateBlockHeat(m_emerge->env, v3POS(x,node_max.Y,z), nullptr, &heat_cache) : 0;

			u32 i = vm->m_area.index(x, node_min.Y, z);
			for (s16 y = node_min.Y; y <= node_max.Y; y++) {
				v3f vec = (v3f(x, y, z) - center) * scale ;
				if (invert_xy)
					std::swap(vec.X, vec.Y);
				if (invert_yz)
					std::swap(vec.Y, vec.Z);

#if USE_MANDELBULBER
				if (!internal)
					d = Compute<normal_mode>(CVector3(vec.X, vec.Y, vec.Z), mg_params->par);
				else
#endif
				if (internal)
					d = (*func)(vec.X, vec.Y, vec.Z, scale.X, iterations);
				if ((!invert && d > 0) || (invert && d == 0)  ) {
					if (!vm->m_data[i].getContent()) {
						//vm->m_data[i] = (y > water_level + biome->filler) ?
						//     MapNode(biome->c_filler) : n_stone;
						if (invert) {
							int index3 = (z - node_min.Z) * zstride +
								(y - node_min.Y) * ystride +
								(x - node_min.X);
							vm->m_data[i] = Mapgen_features::layers_get(index3);
						} else {
							vm->m_data[i] = layers_get(d, result_max);
						}
//						vm->m_data[i] = (y > water_level + biome->filler) ?
//						     MapNode(biome->c_filler) : layers_get(d, result_max);

					}
				} else if (y <= water_level) {
					vm->m_data[i] = (heat < 0 && y > heat/3) ? n_ice : n_water;
				} else {
					vm->m_data[i] = n_air;
				}
				vm->m_area.add_y(em, i, 1);
			}
		}
	}
	return 0;
}

int MapgenMath::getGroundLevelAtPoint(v2POS p) {
	return 0;
}

void MapgenMath::calculateNoise() {
	//TimeTaker t("calculateNoise", NULL, PRECISION_MICRO);
	int x = node_min.X;
	//int y = node_min.Y;
	int z = node_min.Z;

	noise_filler_depth->perlinMap2D(x, z);

	noise_heat->perlinMap2D(x, z);
	noise_humidity->perlinMap2D(x, z);
}
