/*
cavegen.h
Copyright (C) 2010-2013 kwolekr, Ryan Kwolek <kwolekr@minetest.net>
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

#ifndef CAVEGEN_HEADER
#define CAVEGEN_HEADER

#define VMANIP_FLAG_CAVE VOXELFLAG_CHECKED1
#define MGV7_LAVA_DEPTH -256

class MapgenV5;
class MapgenV6;
class MapgenV7;

class CaveV5 {
public:
	MapgenV5 *mg;
	MMVManip *vm;
	INodeDefManager *ndef;

	NoiseParams *np_caveliquids;

	s16 min_tunnel_diameter;
	s16 max_tunnel_diameter;
	u16 tunnel_routepoints;
	int dswitchint;
	int part_max_length_rs;

	bool large_cave_is_flat;
	bool flooded;

	s16 max_stone_y;
	v3s16 node_min;
	v3s16 node_max;

	v3f orp;  // starting point, relative to caved space
	v3s16 of; // absolute coordinates of caved space
	v3s16 ar; // allowed route area
	s16 rs;   // tunnel radius size
	v3f main_direction;

	s16 route_y_min;
	s16 route_y_max;

	PseudoRandom *ps;

	content_t c_water_source;
	content_t c_lava_source;
	content_t c_ice;

	int water_level;

	CaveV5() {}
	CaveV5(MapgenV5 *mg, PseudoRandom *ps);
	void makeCave(v3s16 nmin, v3s16 nmax, int max_stone_height);
	void makeTunnel(bool dirswitch);
	void carveRoute(v3f vec, float f, bool randomize_xz, bool is_ravine);
};

class CaveV6 {
public:
	MapgenV6 *mg;
	MMVManip *vm;
	INodeDefManager *ndef;

	s16 min_tunnel_diameter;
	s16 max_tunnel_diameter;
	u16 tunnel_routepoints;
	int dswitchint;
	int part_max_length_rs;

	bool large_cave;
	bool large_cave_is_flat;
	bool flooded;

	s16 max_stone_y;
	v3s16 node_min;
	v3s16 node_max;

	v3f orp;  // starting point, relative to caved space
	v3s16 of; // absolute coordinates of caved space
	v3s16 ar; // allowed route area
	s16 rs;   // tunnel radius size
	v3f main_direction;

	s16 route_y_min;
	s16 route_y_max;

	PseudoRandom *ps;
	PseudoRandom *ps2;

	content_t c_water_source;
	content_t c_lava_source;
	content_t c_ice;

	int water_level;

	CaveV6() {}
	CaveV6(MapgenV6 *mg, PseudoRandom *ps, PseudoRandom *ps2, bool large_cave);
	void makeCave(v3s16 nmin, v3s16 nmax, int max_stone_height);
	void makeTunnel(bool dirswitch);
	void carveRoute(v3f vec, float f, bool randomize_xz);
};

class CaveV7 {
public:
	MapgenV7 *mg;
	MMVManip *vm;
	INodeDefManager *ndef;

	NoiseParams *np_caveliquids;

	s16 min_tunnel_diameter;
	s16 max_tunnel_diameter;
	u16 tunnel_routepoints;
	int dswitchint;
	int part_max_length_rs;

	bool large_cave_is_flat;
	bool flooded;

	s16 max_stone_y;
	v3s16 node_min;
	v3s16 node_max;

	v3f orp;  // starting point, relative to caved space
	v3s16 of; // absolute coordinates of caved space
	v3s16 ar; // allowed route area
	s16 rs;   // tunnel radius size
	v3f main_direction;

	s16 route_y_min;
	s16 route_y_max;

	PseudoRandom *ps;

	content_t c_water_source;
	content_t c_lava_source;
	content_t c_ice;

	int water_level;

	CaveV7() {}
	CaveV7(MapgenV7 *mg, PseudoRandom *ps);
	void makeCave(v3s16 nmin, v3s16 nmax, int max_stone_height);
	void makeTunnel(bool dirswitch);
	void carveRoute(v3f vec, float f, bool randomize_xz, bool is_ravine);
};

#endif
