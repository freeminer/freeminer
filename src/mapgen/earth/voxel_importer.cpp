// voxel_importer.cpp
// Translation of voxel_importer.lua -> C++ (C++17)
// Dependencies: nlohmann::json (https://github.com/nlohmann/json)

#include <iostream>
#include <fstream>
#include <string>
#include <vector>
#include <unordered_map>
#include <filesystem>
#include <cmath>
#include <limits>
#include <algorithm>
#include <atomic>
#include <cstdint>
#include <memory>
#include <mutex>
#include <tuple>
#include <unordered_set>

#include "nlohmann/json.hpp"
#include "nodedef.h"
#include "mapgen/earth/png_holder.h"

namespace voxel_importer
{
using json = nlohmann::json;
namespace filesystem = std::filesystem;

// Simple base64 decoder (for binary voxel payloads encoded as base64 in JSON)
static std::string base64_decode(const std::string &in)
{
	static const std::string chars =
			"ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
	std::vector<int> T(256, -1);
	for (int i = 0; i < 64; ++i)
		T[static_cast<unsigned char>(chars[i])] = i;

	std::string out;
	int val = 0, valb = -8;
	for (unsigned char c : in) {
		if (T[c] == -1)
			break;
		val = (val << 6) + T[c];
		valb += 6;
		if (valb >= 0) {
			out.push_back(char((val >> valb) & 0xFF));
			valb -= 8;
		}
	}
	return out;
}

// Palette and helpers
struct PaletteEntry
{
	std::string name;
	int r, g, b;
	bool pure = false;
};

static const std::vector<PaletteEntry> BLOCK_PALETTE = {
#if 0
    // === Mineclonia: Colored Blocks (High Priority for Pure Color Mode) ===
    {"mcl_color:concrete_white",      207, 213, 214, true},
    {"mcl_color:concrete_orange",     224, 97,  0,   true},
    {"mcl_color:concrete_magenta",    169, 48,  159, true},
    {"mcl_color:concrete_light_blue", 35,  137, 198, true},
    {"mcl_color:concrete_yellow",     240, 175, 21,  true},
    {"mcl_color:concrete_lime",       94,  168, 24,  true},
    {"mcl_color:concrete_pink",       213, 101, 142, true},
    {"mcl_color:concrete_gray",       54,  57,  61,  true},
    {"mcl_color:concrete_silver",     125, 125, 115, true},
    {"mcl_color:concrete_cyan",       21,  119, 136, true},
    {"mcl_color:concrete_purple",     100, 31,  156, true},
    {"mcl_color:concrete_blue",       44,  46,  143, true},
    {"mcl_color:concrete_brown",      96,  59,  31,  true},
    {"mcl_color:concrete_green",      73,  91,  36,  true},
    {"mcl_color:concrete_red",        142, 32,  32,  true},
    {"mcl_color:concrete_black",      8,   10,  15,  true},

    {"mcl_wool:white",                233, 236, 236, true},
    {"mcl_wool:orange",               240, 118, 19,  true},
    {"mcl_wool:magenta",              189, 68,  179, true},
    {"mcl_wool:light_blue",           58,  175, 217, true},
    {"mcl_wool:yellow",               248, 197, 39,  true},
    {"mcl_wool:lime",                 112, 185, 25,  true},
    {"mcl_wool:pink",                 237, 141, 172, true},
    {"mcl_wool:gray",                 62,  68,  71,  true},
    {"mcl_wool:silver",               142, 142, 134, true},
    {"mcl_wool:cyan",                 21,  137, 145, true},
    {"mcl_wool:purple",               121, 42,  172, true},
    {"mcl_wool:blue",                 53,  57,  157, true},
    {"mcl_wool:brown",                114, 71,  40,  true},
    {"mcl_wool:green",                84,  109, 27,  true},
    {"mcl_wool:red",                  160, 39,  34,  true},
    {"mcl_wool:black",                20,  21,  25,  true},

    //-- === Mineclonia: Natural Blocks ===
    {"mcl_core:stone",                125, 125, 125},
    {"mcl_core:cobble",               100, 100, 100},
    {"mcl_core:stonebrick",           110, 110, 110},
    {"mcl_core:andesite",             115, 115, 115},
    {"mcl_core:diorite",              180, 180, 180},
    {"mcl_core:granite",              150, 110, 100},
    {"mcl_core:dirt",                 134, 96,  67},
    {"mcl_core:coarse_dirt",          119, 85,  59},
    {"mcl_core:podzol",               90,  63,  42},
    {"mcl_core:grass_block_green",    100, 150, 50}, // -- Approximate
    {"mcl_core:mycelium",             110, 100, 110},
    {"mcl_core:clay",                 160, 165, 178},
    {"mcl_core:sandstone",            216, 203, 155},
    {"mcl_core:red_sandstone",        176, 86,  35},
    {"mcl_core:obsidian",             20,  18,  29},
    {"mcl_core:bedrock",              50,  50,  50},
    {"mcl_core:snow",                 249, 254, 254}, // -- Snow block
    {"mcl_core:ice",                  160, 190, 255},
    {"mcl_core:packed_ice",           170, 200, 255},
    {"mcl_core:blue_ice",             180, 210, 255},
    {"mcl_core:prismarine",           99,  156, 157},
    {"mcl_core:prismarine_bricks",    99,  171, 164},
    {"mcl_core:dark_prismarine",      51,  91,  75},
    {"mcl_deepslate:deepslate",       80,  80,  80},
    {"mcl_deepslate:cobbled_deepslate"70, 70,  70},
    {"mcl_nether:netherrack",         110, 50,  50},
    {"mcl_nether:nether_bricks",      44,  21,  26},
    {"mcl_nether:red_nether_bricks",  69,  7,   9},
    {"mcl_nether:basalt",             80,  80,  85},
    {"mcl_nether:blackstone",         40,  35,  40},
    {"mcl_end:end_stone",             222, 222, 175},
    {"mcl_end:end_bricks",            220, 225, 180},
    {"mcl_end:purpur_block",          169, 125, 169},
#endif

		// -- === Minetest Game: Colored Blocks (Fallback/Pure) ===
		{"wool:white", 230, 230, 230, true},
		{"wool:grey", 100, 100, 100, true},
		{"wool:dark_grey", 50, 50, 50, true},
		{"wool:black", 20, 20, 20, true},
		{"wool:red", 200, 0, 0, true},
		{"wool:green", 0, 200, 0, true},
		{"wool:blue", 0, 0, 200, true},
		{"wool:yellow", 255, 255, 0, true},
		{"wool:cyan", 0, 255, 255, true},
		{"wool:magenta", 255, 0, 255, true},
		{"wool:orange", 255, 128, 0, true},
		{"wool:violet", 128, 0, 255, true},
		{"wool:brown", 100, 50, 0, true},
		{"wool:pink", 255, 150, 150, true},

		// === Minetest Game: Natural Blocks ===
		{"default:stone", 128, 128, 128},
		{"default:cobble", 100, 100, 100},
		{"default:stonebrick", 110, 110, 110},
		{"default:dirt", 92, 64, 51},
		{"default:dirt_with_grass", 100, 150, 50},
		{"default:sandstone", 235, 220, 170},
		{"default:desert_stone", 200, 100, 50},
		{"default:clay", 150, 150, 160},
		{"default:snowblock", 240, 240, 240},
		{"default:ice", 150, 200, 255},
		{"default:obsidian", 20, 20, 20},
		{"default:glass", 200, 220, 255},
		{"default:leaves", 50, 100, 50},
		{"default:jungleleaves", 30, 80, 30},
		{"default:pine_needles", 40, 70, 40},
		{"default:acacia_leaves", 80, 120, 40},
		{"default:wood", 150, 100, 50},
		{"default:junglewood", 100, 50, 30},
		{"default:pine_wood", 120, 80, 40},
		{"default:acacia_wood", 180, 50, 30},
		{"default:brick", 150, 50, 50},
};

static std::vector<std::pair<std::string, std::tuple<int, int, int>>>
		PURE_PALETTE; // filled later if colors provided
static bool USE_PURE_COLORS = false;
static std::shared_ptr<const std::vector<PaletteEntry>> ACTIVE_BLOCK_PALETTE;
static std::mutex ACTIVE_BLOCK_PALETTE_MUTEX;

// Basic logging fallback
static void log_warning(const std::string &msg)
{
	std::cerr << "[warning] " << msg << "\n";
}
static void log_error(const std::string &msg)
{
	std::cerr << "[error] " << msg << "\n";
}
static void log_info(const std::string &msg)
{
	std::cout << "[info] " << msg << "\n";
}

// get_safe_node: since we don't have a node registry, prefer given name or fallback to default:stone
static std::string get_safe_node(const std::string &name)
{
	if (!name.empty())
		return name;
	log_warning("Unknown node '" + name + "', falling back to default:stone");
	return "default:stone";
}

// Structures to represent voxels and placements
struct Voxel
{
	int x, y, z;
	int r = 128, g = 128, b = 128;
};

struct Placement
{
	int x, y, z;
	std::string node;
};

// Read an int32 little endian from raw bytes (starting at offset)
static int32_t read_int32_le(const std::string &buf, size_t offset)
{
	if (offset + 4 > buf.size())
		return 0;
	uint8_t b1 = static_cast<uint8_t>(buf[offset]);
	uint8_t b2 = static_cast<uint8_t>(buf[offset + 1]);
	uint8_t b3 = static_cast<uint8_t>(buf[offset + 2]);
	uint8_t b4 = static_cast<uint8_t>(buf[offset + 3]);
	uint32_t n = (uint32_t)b1 | ((uint32_t)b2 << 8) | ((uint32_t)b3 << 16) |
				 ((uint32_t)b4 << 24);
	// handle signed
	if (n > 0x7fffffff)
		return static_cast<int32_t>(n - 0x100000000ULL);
	return static_cast<int32_t>(n);
}

// Euclidean color distance
static double color_dist_sq(int r1, int g1, int b1, int r2, int g2, int b2)
{
	double dr = double(r1 - r2);
	double dg = double(g1 - g2);
	double db = double(b1 - b2);
	return dr * dr + dg * dg + db * db;
}

static bool color_is_visible(const video::SColor &c)
{
	return c.getAlpha() > 0;
}

static bool color_is_default_white(const video::SColor &c)
{
	return c.getAlpha() == 255 && c.getRed() == 255 && c.getGreen() == 255 &&
		   c.getBlue() == 255;
}

static bool node_is_ice_or_snow(const ContentFeatures &f)
{
	return f.getGroup("snowy") || //f.getGroup("slippery") ||
		   f.name.find("snow") != std::string::npos ||
		   f.name.find("ice") != std::string::npos;
}

static bool node_is_palette_candidate(const ContentFeatures &f)
{
	if (f.name.empty() || f.name == "air" || f.name == "ignore" || f.name == "unknown")
		return false;
	if (f.isLiquid() || f.isLiquidRender() || f.liquid_move_physics)
		return false;
	if (f.drawtype != NDT_NORMAL)
		return false;
	if (node_is_ice_or_snow(f))
		return false;
	if (!f.walkable || f.buildable_to)
		return false;
	return true;
}

using RGB = std::tuple<int, int, int>;

static std::string first_texture_name(const std::string &texture)
{
	if (texture.empty() || texture[0] == '[')
		return "";

	size_t end = texture.find('^');
	if (end == std::string::npos)
		end = texture.find('[');

	std::string name = texture.substr(0, end);
	if (name.size() >= 2 && name.front() == '(' && name.back() == ')')
		name = name.substr(1, name.size() - 2);

	return name;
}

static std::string find_png_texture_path(
		const std::string &texture_name, const std::vector<std::string> &texture_dirs)
{
	if (texture_name.empty())
		return "";

	const filesystem::path texture_path(texture_name);
	const std::string extension = texture_path.extension().string();
	if (!extension.empty() && extension != ".png")
		return "";

	for (const std::string &dir : texture_dirs) {
		filesystem::path candidate = filesystem::path(dir) / texture_name;
		if (candidate.extension().empty())
			candidate.replace_extension(".png");
		if (filesystem::exists(candidate))
			return candidate.string();
	}

	return "";
}

static bool average_png_texture(const std::string &texture,
		const std::vector<std::string> &texture_dirs,
		std::unordered_map<std::string, RGB> &texture_cache, int &r, int &g, int &b)
{
	const std::string texture_name = first_texture_name(texture);
	if (texture_name.empty())
		return false;

	const std::string path = find_png_texture_path(texture_name, texture_dirs);
	if (path.empty())
		return false;

	auto cached = texture_cache.find(path);
	if (cached != texture_cache.end()) {
		std::tie(r, g, b) = cached->second;
		return true;
	}

	try {
		PngImage image(path);
		uint64_t rs = 0;
		uint64_t gs = 0;
		uint64_t bs = 0;
		uint64_t weight = 0;

		for (int y = 0; y < image.height(); ++y) {
			for (int x = 0; x < image.width(); ++x) {
				const auto pixel = image.get_pixel(x, y);
				if (!pixel || pixel->getAlpha() == 0)
					continue;
				const uint64_t alpha = pixel->getAlpha();
				rs += uint64_t(pixel->getRed()) * alpha;
				gs += uint64_t(pixel->getGreen()) * alpha;
				bs += uint64_t(pixel->getBlue()) * alpha;
				weight += alpha;
			}
		}

		if (weight == 0)
			return false;

		r = static_cast<int>(rs / weight);
		g = static_cast<int>(gs / weight);
		b = static_cast<int>(bs / weight);
		texture_cache.emplace(path, RGB{r, g, b});
		return true;
	} catch (const std::exception &e) {
		log_warning("Failed to average texture '" + path + "': " + e.what());
		return false;
	}
}

static void tint_color(int &r, int &g, int &b, const video::SColor &tint)
{
	if (!color_is_visible(tint) || color_is_default_white(tint))
		return;

	r = r * tint.getRed() / 255;
	g = g * tint.getGreen() / 255;
	b = b * tint.getBlue() / 255;
}

static bool node_average_color(const ContentFeatures &f,
		const std::vector<std::string> &texture_dirs,
		std::unordered_map<std::string, RGB> &texture_cache, int &r, int &g, int &b)
{
	uint64_t rs = 0;
	uint64_t gs = 0;
	uint64_t bs = 0;
	uint64_t weight = 0;

	const auto add_color = [&](const video::SColor &c, uint64_t w) {
		if (!color_is_visible(c))
			return;
		rs += uint64_t(c.getRed()) * w;
		gs += uint64_t(c.getGreen()) * w;
		bs += uint64_t(c.getBlue()) * w;
		weight += w;
	};

	for (const TileDef &tile : f.tiledef) {
		int tr, tg, tb;
		if (average_png_texture(tile.name, texture_dirs, texture_cache, tr, tg, tb)) {
			tint_color(tr, tg, tb, f.color);
			if (tile.has_color)
				tint_color(tr, tg, tb, tile.color);
			rs += tr;
			gs += tg;
			bs += tb;
			++weight;
		} else if (tile.has_color) {
			add_color(tile.color, 1);
		}
	}

	if (weight == 0 && !color_is_default_white(f.color))
		add_color(f.color, 6);

	if (weight == 0)
		return false;

	r = static_cast<int>(rs / weight);
	g = static_cast<int>(gs / weight);
	b = static_cast<int>(bs / weight);
	return true;
}

static bool load_colors_lua_palette(
		const std::string &path, const NodeDefManager *ndef, std::string &err)
{
	std::ifstream ifs(path);
	if (!ifs) {
		err = "Could not open file: " + path;
		return false;
	}

	std::vector<std::pair<std::string, std::tuple<int, int, int>>> palette;
	std::string line;
	while (std::getline(ifs, line)) {
		const std::string key_prefix = "full[\"";
		size_t key_begin = line.find(key_prefix);
		if (key_begin == std::string::npos)
			continue;
		key_begin += key_prefix.size();
		size_t key_end = line.find("\"]", key_begin);
		if (key_end == std::string::npos)
			continue;

		size_t hex_begin = line.find("\"#", key_end);
		if (hex_begin == std::string::npos)
			continue;
		hex_begin += 2;
		if (hex_begin + 6 > line.size())
			continue;

		std::string id = line.substr(key_begin, key_end - key_begin);
		const std::string hex = line.substr(hex_begin, 6);
		const std::string node_name = "luanti_earth:color_" + id;
		if (ndef && ndef->getId(node_name) == CONTENT_IGNORE)
			continue;

		int r = std::stoi(hex.substr(0, 2), nullptr, 16);
		int g = std::stoi(hex.substr(2, 2), nullptr, 16);
		int b = std::stoi(hex.substr(4, 2), nullptr, 16);
		palette.emplace_back(id, std::make_tuple(r, g, b));
	}

	if (palette.empty()) {
		err = "No registered luanti_earth color nodes found in " + path;
		return false;
	}

	PURE_PALETTE = std::move(palette);
	return true;
}

static void init_block_palette(const NodeDefManager *ndef,
		const std::vector<std::string> &texture_dirs = {},
		const std::string &pure_colors_path = "")
{
	if (!ndef)
		return;

	std::lock_guard<std::mutex> lock(ACTIVE_BLOCK_PALETTE_MUTEX);

	USE_PURE_COLORS = false;
	if (!pure_colors_path.empty()) {
		std::string err;
		if (load_colors_lua_palette(pure_colors_path, ndef, err)) {
			USE_PURE_COLORS = true;
			log_info("Voxel importer pure color palette initialized with " +
					 std::to_string(PURE_PALETTE.size()) + " luanti_earth colors");
		} else {
			log_warning("Failed to initialize luanti_earth pure color palette: " + err);
		}
	}

	std::vector<PaletteEntry> palette;
	std::unordered_set<std::string> added;
	std::unordered_map<std::string, RGB> texture_cache;

	for (const auto &entry : BLOCK_PALETTE) {
		content_t id = CONTENT_IGNORE;
		if (!ndef->getId(entry.name, id))
			continue;

		const ContentFeatures &f = ndef->get(id);
		if (!node_is_palette_candidate(f))
			continue;

		PaletteEntry active_entry = entry;
		int r, g, b;
		if (node_average_color(f, texture_dirs, texture_cache, r, g, b)) {
			active_entry.r = r;
			active_entry.g = g;
			active_entry.b = b;
		}

		palette.push_back(active_entry);
		added.insert(entry.name);
	}

	for (u32 id = 0; id < ndef->size(); ++id) {
		const ContentFeatures &f = ndef->get(static_cast<content_t>(id));
		if (!node_is_palette_candidate(f) || added.count(f.name))
			continue;

		int r, g, b;
		if (!node_average_color(f, texture_dirs, texture_cache, r, g, b))
			continue;

		palette.push_back({f.name, r, g, b, false});
		added.insert(f.name);
	}

	if (palette.empty()) {
		log_warning(
				"No usable registered blocks found for voxel color palette; using built-in fallback");
		return;
	}

	const auto active_palette =
			std::make_shared<const std::vector<PaletteEntry>>(std::move(palette));
	std::atomic_store(&ACTIVE_BLOCK_PALETTE, active_palette);
	log_info("Voxel importer palette initialized with " +
			 std::to_string(active_palette->size()) + " registered non-liquid blocks");
}

// The equivalent of rgb_to_block
static std::string rgb_to_block(int r, int g, int b, bool use_pure = false)
{
	if ((use_pure || USE_PURE_COLORS) && !PURE_PALETTE.empty()) {
		double best = std::numeric_limits<double>::infinity();
		std::string best_id = "0";
		for (const auto &p : PURE_PALETTE) {
			int pr, pg, pb;
			std::tie(pr, pg, pb) = p.second;
			double d = color_dist_sq(r, g, b, pr, pg, pb);
			if (d < best) {
				best = d;
				best_id = p.first;
			}
		}
		return "luanti_earth:color_" + best_id;
	} else {
		double best = std::numeric_limits<double>::infinity();
		std::string best_block = "default:stone";
		const auto active_palette = std::atomic_load(&ACTIVE_BLOCK_PALETTE);
		const auto &palette = active_palette && !active_palette->empty() ? *active_palette
																		 : BLOCK_PALETTE;
		for (const auto &e : palette) {
			double d = color_dist_sq(r, g, b, e.r, e.g, e.b);
			if (d < best) {
				best = d;
				best_block = e.name;
			}
		}
		return best_block;
		//return get_safe_node(best_block);
	}
}

// Load JSON from file and return json object
static bool load_json_file(const std::string &path, json &out, std::string &err)
{
	std::ifstream ifs(path, std::ios::binary);
	if (!ifs) {
		err = "Could not open file: " + path;
		return false;
	}
	try {
		ifs >> out;
	} catch (std::exception &e) {
		err = std::string("JSON parse error: ") + e.what();
		return false;
	}
	return true;
}

// Load voxel file and produce a list of voxels.
// Supports either:
//  - { "voxels": [ {x:,y:,z:, r:,g:,b:}, ... ] }
//  - { "voxel_bytes": "<base64 string>" } where each record is 16 bytes: x(4), y(4), z(4), r(1), g(1), b(1), a(1)
static bool loadVoxelFile(
		const std::string &filepath, std::vector<Voxel> &out_voxels, std::string &err)
{
	json j;
	if (!load_json_file(filepath, j, err))
		return false;

	out_voxels.clear();
	if (j.contains("voxel_bytes")) {
		std::string b64 = j["voxel_bytes"].get<std::string>();
		std::string bytes = base64_decode(b64);
		size_t stride = 16;
		if (bytes.size() % stride != 0) {
			// warn, but proceed with floor(bytes/stride)
			log_warning("voxel_bytes length not multiple of 16, truncating");
		}
		for (size_t i = 0; i + 12 < bytes.size(); i += stride) {
			int32_t vx = read_int32_le(bytes, i);
			int32_t vy = read_int32_le(bytes, i + 4);
			int32_t vz = read_int32_le(bytes, i + 8);
			uint8_t rr = static_cast<uint8_t>(bytes[i + 12]);
			uint8_t gg = static_cast<uint8_t>(bytes[i + 13]);
			uint8_t bb = static_cast<uint8_t>(bytes[i + 14]);
			Voxel v;
			v.x = vx;
			v.y = vy;
			v.z = vz;
			v.r = rr;
			v.g = gg;
			v.b = bb;
			out_voxels.push_back(v);
		}
		return true;
	} else if (j.contains("voxels") && j["voxels"].is_array()) {
		for (auto &it : j["voxels"]) {
			Voxel v;
			// support keys x,y,z or wx,wy,wz; default to 0
			if (it.contains("wx"))
				v.x = it["wx"].get<int>();
			else if (it.contains("x"))
				v.x = it["x"].get<int>();
			else
				v.x = 0;
			if (it.contains("wy"))
				v.y = it["wy"].get<int>();
			else if (it.contains("y"))
				v.y = it["y"].get<int>();
			else
				v.y = 0;
			if (it.contains("wz"))
				v.z = it["wz"].get<int>();
			else if (it.contains("z"))
				v.z = it["z"].get<int>();
			else
				v.z = 0;

			if (it.contains("r"))
				v.r = it["r"].get<int>();
			if (it.contains("g"))
				v.g = it["g"].get<int>();
			if (it.contains("b"))
				v.b = it["b"].get<int>();
			out_voxels.push_back(v);
		}
		return true;
	} else {
		err = "Invalid voxel data: no voxels or voxel_bytes";
		return false;
	}
}

// importFromDirectory: iterate JSON files in a directory and for each call loadVoxelFile and then placeVoxels
// offset is applied to voxel positions; if use_color=false, only default:stone is used
static size_t importFromDirectory(const std::string &dir_path,
		const std::tuple<int, int, int> &offset, bool use_color,
		std::vector<Placement> &out_placements)
{
	out_placements.clear();
	if (!filesystem::is_directory(dir_path)) {
		log_error("Not a directory: " + dir_path);
		return 0;
	}
	size_t total_placed = 0;
	for (auto &entry : filesystem::directory_iterator(dir_path)) {
		if (!entry.is_regular_file())
			continue;
		auto p = entry.path();
		if (p.extension() == ".json") {
			log_info("Importing: " + p.filename().string());
			std::vector<Voxel> voxels;
			std::string err;
			if (!loadVoxelFile(p.string(), voxels, err)) {
				log_error("Failed to load " + p.string() + ": " + err);
				continue;
			}
			// place voxels (here: append placements)
			for (auto &v : voxels) {
				Placement pl;
				pl.x = v.x + std::get<0>(offset);
				pl.y = v.y + std::get<1>(offset);
				pl.z = v.z + std::get<2>(offset);
				if (use_color)
					pl.node = rgb_to_block(v.r, v.g, v.b, true);
				else
					pl.node = get_safe_node("default:stone");
				out_placements.push_back(pl);
				++total_placed;
			}
		}
	}
	return total_placed;
}

// placeVoxels: from a voxel_data json object or vector<Voxel>, produce placements with offset and color option
static size_t placeVoxels(const std::vector<Voxel> &voxels,
		const std::tuple<int, int, int> &offset, bool use_color,
		std::vector<Placement> &out_placements)
{
	out_placements.clear();
	for (auto &v : voxels) {
		Placement pl;
		pl.x = v.x + std::get<0>(offset);
		pl.y = v.y + std::get<1>(offset);
		pl.z = v.z + std::get<2>(offset);
		if (use_color)
			pl.node = rgb_to_block(v.r, v.g, v.b, false);
		else
			pl.node = get_safe_node("default:stone");
		out_placements.push_back(pl);
	}
	return out_placements.size();
}

// exportForViz: write placements or voxels to a simple JSON array
static bool exportForViz(const std::string &output_path, const std::vector<Voxel> &voxels,
		std::string &err)
{
	json arr = json::array();
	for (auto &v : voxels) {
		json o;
		o["x"] = v.x;
		o["y"] = v.y;
		o["z"] = v.z;
		o["r"] = v.r;
		o["g"] = v.g;
		o["b"] = v.b;
		arr.push_back(o);
	}
	std::ofstream ofs(output_path);
	if (!ofs) {
		err = "Could not open output file: " + output_path;
		return false;
	}
	ofs << arr.dump(2) << std::endl;
	return true;
}

// Helper: parse colors map from a JSON file mapping id -> hex (like colors.lua had)
// Example JSON:
// {
//   "0": "#ffffff",
//   "1": "#ff0000"
// }
// Fills PURE_PALETTE vector of (id, (r,g,b))
static bool load_colors_json(const std::string &path, std::string &err)
{
	json j;
	if (!load_json_file(path, j, err))
		return false;
	PURE_PALETTE.clear();
	for (auto it = j.begin(); it != j.end(); ++it) {
		std::string id = it.key();
		std::string hex = it.value().get<std::string>();
		// hex to rgb
		if (hex.size() > 0 && hex[0] == '#')
			hex = hex.substr(1);
		if (hex.size() != 6)
			continue;
		int r = std::stoi(hex.substr(0, 2), nullptr, 16);
		int g = std::stoi(hex.substr(2, 2), nullptr, 16);
		int b = std::stoi(hex.substr(4, 2), nullptr, 16);
		PURE_PALETTE.emplace_back(id, std::make_tuple(r, g, b));
	}
	return true;
}

// Example usage in main()
#ifdef VOXEL_IMPORTER_MAIN
int main(int argc, char **argv)
{
	if (argc < 2) {
		std::cerr
				<< "Usage: voxel_importer <directory_with_jsons> [offset_x offset_y offset_z] [use_color 0|1] [colors.json]\n";
		return 1;
	}
	std::string dir = argv[1];
	int ox = 0, oy = 0, oz = 0;
	if (argc >= 5) {
		ox = std::atoi(argv[2]);
		oy = std::atoi(argv[3]);
		oz = std::atoi(argv[4]);
	}
	bool use_color = true;
	if (argc >= 6)
		use_color = (std::atoi(argv[5]) != 0);
	if (argc >= 7) {
		std::string err;
		if (!load_colors_json(argv[6], err)) {
			std::cerr << "Failed to load colors: " << err << "\n";
		}
	}

	std::vector<Placement> placements;
	size_t count = importFromDirectory(dir, {ox, oy, oz}, use_color, placements);
	std::cout << "Total placed (collected): " << count << "\n";

	// optionally write placements to a file for visualization
	json out = json::array();
	for (auto &p : placements) {
		out.push_back({{"x", p.x}, {"y", p.y}, {"z", p.z}, {"node", p.node}});
	}
	std::ofstream ofs("placements.json");
	ofs << out.dump(2) << std::endl;
	std::cout << "Wrote placements.json\n";
	return 0;
}
#endif

}
