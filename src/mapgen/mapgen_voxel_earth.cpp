/*
Copyright (C) 2024 proller <proler@gmail.com>
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

#include <algorithm>
#include <cassert>
#include <cmath>
#include <exception>
#include <filesystem>
#include <map>
#include <string>
#include <vector>

#include "content/mods.h"
#include "emerge.h"
#include "filesys.h"
#include "log.h"
#include "mapgen/earth/http.h"
#include "mapgen_voxel_earth.h"
#include "mapnode.h"
#include "server.h"
#include "settings.h"
#include "voxel.h"
#include "voxelalgorithms.h"

#if USE_VOXEL_EARTH
#include "mapgen/earth/luanti-earth/native/src/downloader.h"
#include "mapgen/earth/luanti-earth/native/src/voxelizer.h"

#undef stoi
#include "mapgen/earth/voxel_importer.cpp"

using Vec3 = v3d;

static Vec3 cartesianFromDegrees(double lat, double lon, double h = 0)
{
	const double a = 6378137.0;
	const double f = 1.0 / 298.257223563;
	const double e2 = f * (2.0 - f);
	const double radLat = lat * 3.14159265358979323846 / 180.0;
	const double radLon = lon * 3.14159265358979323846 / 180.0;
	const double sinLat = std::sin(radLat);
	const double cosLat = std::cos(radLat);
	const double N = a / std::sqrt(1.0 - e2 * sinLat * sinLat);
	const double x = (N + h) * cosLat * std::cos(radLon);
	const double y = (N + h) * cosLat * std::sin(radLon);
	const double z = (N * (1.0 - e2) + h) * sinLat;
	return {x, y, z};
}

#include "earth/geoid.h"
#include "earth/CpuVoxelizer.cpp"
#endif

MapgenVoxelEarth::MapgenVoxelEarth(MapgenEarthParams *params_, EmergeParams *emerge) :
		MapgenEarth(params_, emerge)
{
#if USE_VOXEL_EARTH
	const std::string geoid_path =
			maps_holder->data_root + DIR_DELIM + "egm96_15.gtx";
	earth::set_geoid_grid_path(geoid_path);
	if (!earth::geoid_grid_loaded()) {
		const auto lock = std::lock_guard(maps_holder->download_lock);
		if (!earth::reload_geoid_grid()) {
			// Remove a previous empty/partial download so multi_http_to_file()
			// does not mistake it for valid cached data.
			std::error_code ec;
			std::filesystem::remove(geoid_path, ec);
			multi_http_to_file("egm96_15.gtx",
					{"https://download.osgeo.org/proj/vdatum/egm96_15/egm96_15.gtx"},
					geoid_path);
			earth::reload_geoid_grid();
		}
	}
	if (earth::geoid_grid_loaded()) {
		infostream << "Voxel earth EGM96 grid: " << earth::geoid_grid_path() << '\n';
	} else {
		errorstream << "Voxel earth requires egm96_15.gtx; voxel geometry will be "
						"skipped instead of being placed at WGS84 ellipsoid height\n";
	}

	std::vector<std::string> texture_dirs;
	std::string pure_colors_path;
	fs::GetRecursiveDirs(texture_dirs, g_settings->get("texture_path"));
	fs::GetRecursiveDirs(texture_dirs,
			porting::path_user + DIR_DELIM + "textures" + DIR_DELIM + "server");
	if (emerge && emerge->server) {
		if (const SubgameSpec *gamespec = emerge->server->getGameSpec())
			fs::GetRecursiveDirs(texture_dirs, gamespec->path + DIR_DELIM + "textures");
		if (emerge->server->m_modmgr.get()) {
			if (const ModSpec *mod = emerge->server->getModSpec("luanti_earth")) {
				pure_colors_path = mod->path + DIR_DELIM + "colors.lua";
			}
			const auto &mods = emerge->server->getMods();
			for (auto it = mods.crbegin(); it != mods.crend(); ++it) {
				fs::GetRecursiveDirs(texture_dirs, it->path + DIR_DELIM + "textures");
				fs::GetRecursiveDirs(texture_dirs, it->path + DIR_DELIM + "media");
			}
		}
	}
	voxel_importer::init_block_palette(ndef, texture_dirs, pure_colors_path);
#endif
}

void MapgenVoxelEarth::start_download_and_voxelize(double lat, double lon,
		double elevation, double radius, int resolution, const std::string &api_key)
{
#if USE_VOXEL_EARTH
	try {
		if (!earth::geoid_grid_loaded())
			return;
		const std::string &apiKeyStr = api_key;
		const pos_t terrain_y =
				std::max(get_height(node_min.X + csize.X / 2, node_min.Z + csize.Z / 2),
						static_cast<pos_t>(water_level)) +
				1;

		TileDownloader downloader(
				apiKeyStr, maps_holder->data_root + DIR_DELIM + "voxel_earth");
		const double horizontal_radius =
				radius * std::max(std::abs(scale.X), std::abs(scale.Z));
		double vertical_padding = std::max<double>(csize.Y * std::abs(scale.Y), 64.0);
		g_settings->getFloatNoEx("voxel_earth_query_vertical_padding", vertical_padding);
		double query_radius = std::sqrt(horizontal_radius * horizontal_radius +
										vertical_padding * vertical_padding);
		g_settings->getFloatNoEx("voxel_earth_query_radius", query_radius);
		const double terrain_elevation = (terrain_y + center.Y) * scale.Y;
		const double terrain_ellipsoid_elevation =
				earth::orthometric_to_ellipsoid_height(lat, lon, terrain_elevation);
		auto tiles = downloader.downloadTiles(
				lat, lon, terrain_ellipsoid_elevation, query_radius);
		bool use_java_voxelizer = false;
		g_settings->getBoolNoEx("use_java_voxelizer", use_java_voxelizer);
		bool use_native_voxelizer = !use_java_voxelizer;
		g_settings->getBoolNoEx("use_native_voxelizer", use_native_voxelizer);

		Voxelizer voxelizer;
		CpuVoxelizer cpuvoxelizer{csize.X * 2, true, true};
		const double chunk_elevation = (elevation + center.Y) * scale.Y;
		const double chunk_ellipsoid_elevation =
				earth::orthometric_to_ellipsoid_height(lat, lon, chunk_elevation);
		Vec3 origin = cartesianFromDegrees(lat, lon, chunk_ellipsoid_elevation);
		double y_offset_nodes = 0.0;
		g_settings->getFloatNoEx("voxel_earth_y_offset", y_offset_nodes);
		// DUMP(node_min, node_max, lat, lon, origin.X, origin.Y, origin.Z, tiles.size());
		const auto mg = this;

		std::vector<Voxel> voxels;
		const auto collect_voxel = [&](const int x, const int y, const int z,
										 const uint8_t r, const uint8_t g,
										 const uint8_t b, const uint8_t a) {
			const v3pos_t pos_rel{static_cast<pos_t>(x), static_cast<pos_t>(y),
					static_cast<pos_t>(z)};
			if (mg->vm->exists(node_min + pos_rel)) {
				voxels.push_back({x, y, z, r, g, b, a});
			}
		};

		for (const auto &tile : tiles) {
			if (use_java_voxelizer) {
				const auto callback = [&](const int &x, const int &y, const int &z,
											  const uint8_t &r, const uint8_t &g,
											  const uint8_t &b, const uint8_t &a) {
					collect_voxel(x, y, z, r, g, b, a);
				};
				const auto stats = cpuvoxelizer.voxelizeSingleGLB(tile, callback);
			}

			if (use_native_voxelizer) {
				VoxelGrid grid = voxelizer.voxelize(tile, resolution, origin.X, origin.Y,
						origin.Z, y_offset_nodes, center.X, center.Y, center.Z, scale.X,
						scale.Y, scale.Z, node_min.X, node_min.Y, node_min.Z);
				for (const auto &v : grid.voxels) {
					collect_voxel(v.x, v.y, v.z, v.r, v.g, v.b, v.a);
				}
			}
		}

		// Terrain can be a few nodes above the photogrammetry surface. Carve each
		// occupied column from its lowest model voxel upward, then restore the
		// complete model so roofs and upper walls are not erased by the carving.
		std::map<std::pair<pos_t, pos_t>, pos_t> column_bottoms;
		for (const auto &v : voxels) {
			const v3pos_t pos = node_min +
					v3pos_t(static_cast<pos_t>(v.x), static_cast<pos_t>(v.y),
							static_cast<pos_t>(v.z));
			auto [it, inserted] =
					column_bottoms.emplace(std::pair{pos.X, pos.Z}, pos.Y);
			if (!inserted)
				it->second = std::min(it->second, pos.Y);
		}
		for (const auto &[xz, bottom] : column_bottoms) {
			for (pos_t y = bottom + 1; y <= node_max.Y; ++y) {
				const v3pos_t pos{xz.first, y, xz.second};
				if (mg->vm->exists(pos))
					mg->vm->setNode(pos, n_air);
			}
		}

		for (const auto &v : voxels) {
			const v3pos_t pos = node_min +
					v3pos_t(static_cast<pos_t>(v.x), static_cast<pos_t>(v.y),
							static_cast<pos_t>(v.z));
			const auto block_name = voxel_importer::rgb_to_block(v.r, v.g, v.b);
			const auto id = ndef->getId(block_name);
			mg->vm->setNode(pos, MapNode{id, LIGHT_SUN});
		}
	} catch (const std::exception &e) {
		errorstream << "Voxel earth exception: " << e.what() << "\n";
	}
#endif
}

void MapgenVoxelEarth::makeChunk(BlockMakeData *data)
{
#if !USE_VOXEL_EARTH
	warningstream << "voxel_earth mapgen requested but USE_VOXEL_EARTH is disabled; "
					 "falling back to earth mapgen\n";
	MapgenEarth::makeChunk(data);
#else
	// Pre-conditions
	assert(data->vmanip);
	assert(data->nodedef);

	this->generating = true;
	this->vm = data->vmanip;
	this->ndef = data->nodedef;

	auto blockpos_min = data->blockpos_min;
	auto blockpos_max = data->blockpos_max;
	node_min = blockpos_min * MAP_BLOCKSIZE;
	node_max = (blockpos_max + v3pos_t(1, 1, 1)) * MAP_BLOCKSIZE - v3pos_t(1, 1, 1);
	full_node_min = (blockpos_min - 1) * MAP_BLOCKSIZE;
	full_node_max = (blockpos_max + 2) * MAP_BLOCKSIZE - v3pos_t(1, 1, 1);

	blockseed = getBlockSeed2(full_node_min, seed);

	//freeminer:
	layers_prepare(node_min, node_max);
	cave_prepare(node_min, node_max, sp->paramsj.get("cave_indev", -100).asInt());
	//==========

	generateTerrain();

	const auto tc = pos_to_ll(node_min + csize / 2);
	static std::string key;
	if (key.empty())
		g_settings->getNoEx("voxel_earth_api_key", key);

	const auto radius = csize.X / 2; // - MAP_BLOCKSIZE;
	const auto resolution = csize.X;
	const auto elevation = node_min.Y; // + csize.Y / 2;
	start_download_and_voxelize(tc.lat, tc.lon, elevation, radius, resolution, key);

	// Update liquids
	updateLiquid(&data->transforming_liquid, full_node_min, full_node_max);

	// Calculate lighting
	// Limit floatland shadows
	bool propagate_shadow =
			!((spflags & MGV7_FLOATLANDS) && node_max.Y >= floatland_ymin - csize.Y * 2 &&
					node_min.Y <= floatland_ymax);

	if (flags & MG_LIGHT)
		calcLighting(node_min - v3pos_t(0, 1, 0), node_max + v3pos_t(0, 1, 0),
				full_node_min, full_node_max, propagate_shadow);

	this->generating = false;
#endif
}
