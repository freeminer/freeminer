/**
 * CpuVoxelizer - C++ port of the JavaCpuVoxelizer library.
 *
 * This file is a direct translation of the original Java implementation
 * (src/main/java/com/example/voxelizer/JavaCpuVoxelizer.java) to C++.
 * It uses tinygltf (header‑only) for GLB parsing and standard C++17
 * facilities for filesystem, threading and containers.
 *
 * The implementation follows the Java logic closely, including:
 *   • GLB loading and extraction of triangles, UVs and the first texture.
 *   • Bounding‑box computation and conversion to a cube.
 *   • Voxel‑space transformation and slab‑parallel rasterisation.
 *   • CUDA‑style bilinear texture sampling.
 *   • JSON emission of the palette/xyzi data and the position file.
 *
 * Dependencies:
 *   • tinygltf (include/tiny_gltf.h) – a minimal placeholder is provided.
 *   • C++17 compiler (std::filesystem, std::optional, etc.).
 *
 * Build with the provided CMakeLists.txt.
 */

#include "CpuVoxelizer.h"
#include "mapgen/earth/luanti-earth/native/src/downloader.h"
#include "tiny_gltf.h"

#include <algorithm>
#include <array>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <functional>
#include <future>
#include <iostream>
#include <optional>
#include <sstream>
#include <tuple>
#include <unordered_map>
#include <vector>
#include <thread>
#include <numeric>
#include <cmath>
#include <limits>
#include <stdexcept>

using namespace std::filesystem;

// Configuration constants (mirroring Java version)
static const bool FLIP_V = false;			// no V‑flip by default
static const int SLAB_HEIGHT = 8;			// number of z‑slices per task
static const float EPS = 1e-6f;				// inside test epsilon
static const float NEAR_W_FRACTION = 0.15f; // second‑slice threshold
static const float TILE_SCALE = 1.3f;
static const float TILE_X = 46.0f * TILE_SCALE;
static const float TILE_Y = 38.0f * TILE_SCALE;
static const float TILE_Z = 24.0f * TILE_SCALE;

/* -------------------------------------------------------------------------- */
/* Helper dynamic array classes (mirroring Java's IntArray, FloatArray, BoolArray) */

class IntArray
{
public:
	std::vector<int> data;
	IntArray(size_t cap = 0) { data.reserve(cap); }
	void add(int v) { data.push_back(v); }
	size_t size() const { return data.size(); }
};

class FloatArray
{
public:
	std::vector<float> data;
	FloatArray(size_t cap = 0) { data.reserve(cap); }
	void add(float v) { data.push_back(v); }
	size_t size() const { return data.size(); }
};

class BoolArray
{
public:
	std::vector<bool> data;
	BoolArray(size_t cap = 0) { data.reserve(cap); }
	void add(bool v) { data.push_back(v); }
	size_t size() const { return data.size(); }
};

/* -------------------------------------------------------------------------- */
/* Data structures used during loading */

struct LoaderResult
{
	int triCount;
	std::vector<float> vx0, vy0, vz0;
	std::vector<float> vx1, vy1, vz1;
	std::vector<float> vx2, vy2, vz2;
	std::vector<float> tu0, tv0;
	std::vector<float> tu1, tv1;
	std::vector<float> tu2, tv2;
	std::vector<bool> hasUV;
	int texW, texH, texStride;
	std::vector<int> texPixels; // ARGB packed ints

	LoaderResult(int n) : triCount(n), texW(0), texH(0), texStride(0)
	{
		vx0.reserve(n);
		vy0.reserve(n);
		vz0.reserve(n);
		vx1.reserve(n);
		vy1.reserve(n);
		vz1.reserve(n);
		vx2.reserve(n);
		vy2.reserve(n);
		vz2.reserve(n);
		tu0.reserve(n);
		tv0.reserve(n);
		tu1.reserve(n);
		tv1.reserve(n);
		tu2.reserve(n);
		tv2.reserve(n);
		hasUV.reserve(n);
	}
};

/* -------------------------------------------------------------------------- */
/* GLB loading – tinygltf based */

class GltfReader
{
public:
	static LoaderResult loadTrianglesAndGlobalTexture(const TileData &tile)
	{
		/*
        // Load file into memory
		std::vector<unsigned char> bytes = readFileBytes(glbFile);
*/
		tinygltf::TinyGLTF loader;
		tinygltf::Model model;
		std::string err, warn;
		/*
        bool ret = loader.LoadBinaryFromMemory(
				&model, &err, &warn, bytes.data(), bytes.size());
		if (!ret) {
			throw std::runtime_error("Failed to load GLB: " + err);
		}
*/
		const auto &glbData = tile.data;
		std::cout << ("[Voxelizer] Loading GLB (" + std::to_string(glbData.size()) +
							 " bytes)")
				  << "\n";
		bool ret = loader.LoadBinaryFromMemory(&model, &err, &warn,
				reinterpret_cast<const unsigned char *>(glbData.data()), glbData.size());
		if (!warn.empty()) {
			std::cerr << "tinygltf warn: " << warn << "\n";
		}
		if (!ret) {
			throw std::runtime_error("Failed to load GLB: " + err);
		}

		// Containers for raw data
		FloatArray vx, vy, vz;
		FloatArray tu, tv;
		BoolArray hv;
		IntArray triIdx;

		// Walk the scene graph
		if (model.scenes.empty()) {
			for (const auto &node : model.nodes) {
				gatherNode(model, node, triIdx, vx, vy, vz, tu, tv, hv);
			}
		} else {
			for (const auto &scene : model.scenes) {
				for (int nodeIdx : scene.nodes) {
					const auto &node = model.nodes[nodeIdx];
					gatherNode(model, node, triIdx, vx, vy, vz, tu, tv, hv);
				}
			}
		}

		// Build LoaderResult
		int nTri = static_cast<int>(triIdx.data.size() / 3);
		LoaderResult lr(nTri);
		lr.triCount = nTri;

		// Fill vertex arrays
		for (int i = 0; i < nTri; ++i) {
			int b = i * 3;
			int i0 = triIdx.data[b];
			int i1 = triIdx.data[b + 1];
			int i2 = triIdx.data[b + 2];
			lr.vx0.push_back(vx.data[i0]);
			lr.vy0.push_back(vy.data[i0]);
			lr.vz0.push_back(vz.data[i0]);
			lr.vx1.push_back(vx.data[i1]);
			lr.vy1.push_back(vy.data[i1]);
			lr.vz1.push_back(vz.data[i1]);
			lr.vx2.push_back(vx.data[i2]);
			lr.vy2.push_back(vy.data[i2]);
			lr.vz2.push_back(vz.data[i2]);
			lr.tu0.push_back(tu.data[i0]);
			lr.tv0.push_back(tv.data[i0]);
			lr.tu1.push_back(tu.data[i1]);
			lr.tv1.push_back(tv.data[i1]);
			lr.tu2.push_back(tu.data[i2]);
			lr.tv2.push_back(tv.data[i2]);
			lr.hasUV.push_back(hv.data[i0] || hv.data[i1] || hv.data[i2]);
		}

		// Load first image as packed RGB ints. tinygltf decodes GLB images to raw
		// pixel bytes for us.
		if (!model.images.empty()) {
			const auto &img = model.images[0];
			if (img.width > 0 && img.height > 0 && img.bits == 8 &&
					!img.image.empty() && img.component > 0) {
				lr.texW = img.width;
				lr.texH = img.height;
				lr.texStride = img.width;
				lr.texPixels.resize(lr.texW * lr.texH);
				for (int y = 0; y < lr.texH; ++y) {
					for (int x = 0; x < lr.texW; ++x) {
						const size_t p =
								(static_cast<size_t>(y) * lr.texW + x) * img.component;
						const uint8_t r = img.image[p + 0];
						const uint8_t g = img.component >= 3 ? img.image[p + 1] : r;
						const uint8_t b = img.component >= 3 ? img.image[p + 2] : r;
						lr.texPixels[static_cast<size_t>(y) * lr.texW + x] =
								(r << 16) | (g << 8) | b;
					}
				}
			}
		}

		return lr;
	}

private:
	static std::vector<unsigned char> readFileBytes(const path &p)
	{
		std::ifstream ifs(p, std::ios::binary);
		if (!ifs)
			throw std::runtime_error("Cannot open file: " + p.string());
		return std::vector<unsigned char>(std::istreambuf_iterator<char>(ifs), {});
	}

	static void gatherNode(const tinygltf::Model &model, const tinygltf::Node &node,
			IntArray &triIdx, FloatArray &vx, FloatArray &vy, FloatArray &vz,
			FloatArray &tu, FloatArray &tv, BoolArray &hv)
	{
		// tinygltf::Node has a single mesh index (or -1 if none)
		if (node.mesh >= 0) {
			const auto &mesh = model.meshes[node.mesh];
			for (const auto &prim : mesh.primitives) {
				// Positions
				const auto &posAcc = model.accessors[prim.attributes.at("POSITION")];
				const auto &posView = model.bufferViews[posAcc.bufferView];
				const auto &posBuf = model.buffers[posView.buffer];
				const float *posData = reinterpret_cast<const float *>(
						&posBuf.data[posView.byteOffset + posAcc.byteOffset]);

				// UVs (optional)
				const float *uvData = nullptr;
				bool hasUV = false;
				if (prim.attributes.find("TEXCOORD_0") != prim.attributes.end()) {
					const auto &uvAcc = model.accessors[prim.attributes.at("TEXCOORD_0")];
					const auto &uvView = model.bufferViews[uvAcc.bufferView];
					const auto &uvBuf = model.buffers[uvView.buffer];
					uvData = reinterpret_cast<const float *>(
							&uvBuf.data[uvView.byteOffset + uvAcc.byteOffset]);
					hasUV = true;
				}

				// Indices (optional)
				const tinygltf::Accessor *idxAcc = nullptr;
				const unsigned short *idxDataU16 = nullptr;
				const unsigned int *idxDataU32 = nullptr;
				const unsigned char *idxDataU8 = nullptr;
				if (prim.indices >= 0) {
					idxAcc = &model.accessors[prim.indices];
					const auto &idxView = model.bufferViews[idxAcc->bufferView];
					const auto &idxBuf = model.buffers[idxView.buffer];
					const void *raw =
							&idxBuf.data[idxView.byteOffset + idxAcc->byteOffset];
					if (idxAcc->componentType == TINYGLTF_COMPONENT_TYPE_UNSIGNED_BYTE) {
						idxDataU8 = static_cast<const unsigned char *>(raw);
					} else if (idxAcc->componentType ==
							   TINYGLTF_COMPONENT_TYPE_UNSIGNED_SHORT) {
						idxDataU16 = static_cast<const unsigned short *>(raw);
					} else if (idxAcc->componentType ==
							   TINYGLTF_COMPONENT_TYPE_UNSIGNED_INT) {
						idxDataU32 = static_cast<const unsigned int *>(raw);
					}
				}

				// Helper to fetch an index
				auto getIdx = [&](size_t i) -> int {
					if (idxDataU8)
						return idxDataU8[i];
					if (idxDataU16)
						return idxDataU16[i];
					if (idxDataU32)
						return static_cast<int>(idxDataU32[i]);
					// No indices – use sequential vertices
					return static_cast<int>(i);
				};

				size_t vertexCount = posAcc.count;
				size_t triCount = (idxAcc ? idxAcc->count : vertexCount) / 3;
				for (size_t t = 0; t < triCount; ++t) {
					int i0 = getIdx(t * 3);
					int i1 = getIdx(t * 3 + 1);
					int i2 = getIdx(t * 3 + 2);
					// Position
					float x0 = posData[i0 * 3];
					float y0 = posData[i0 * 3 + 1];
					float z0 = posData[i0 * 3 + 2];
					float x1 = posData[i1 * 3];
					float y1 = posData[i1 * 3 + 1];
					float z1 = posData[i1 * 3 + 2];
					float x2 = posData[i2 * 3];
					float y2 = posData[i2 * 3 + 1];
					float z2 = posData[i2 * 3 + 2];

					// UVs (if present)
					float u0 = 0, v0 = 0, u1 = 0, v1 = 0, u2 = 0, v2 = 0;
					if (hasUV) {
						u0 = uvData[i0 * 2];
						v0 = uvData[i0 * 2 + 1];
						u1 = uvData[i1 * 2];
						v1 = uvData[i1 * 2 + 1];
						u2 = uvData[i2 * 2];
						v2 = uvData[i2 * 2 + 1];
					}

					// Store vertex data directly
					vx.add(x0);
					vy.add(y0);
					vz.add(z0);
					if (hasUV) {
						tu.add(u0);
						tv.add(v0);
						hv.add(true);
					} else {
						tu.add(NAN);
						tv.add(NAN);
						hv.add(false);
					}

					vx.add(x1);
					vy.add(y1);
					vz.add(z1);
					if (hasUV) {
						tu.add(u1);
						tv.add(v1);
						hv.add(true);
					} else {
						tu.add(NAN);
						tv.add(NAN);
						hv.add(false);
					}

					vx.add(x2);
					vy.add(y2);
					vz.add(z2);
					if (hasUV) {
						tu.add(u2);
						tv.add(v2);
						hv.add(true);
					} else {
						tu.add(NAN);
						tv.add(NAN);
						hv.add(false);
					}
					// Record triangle indices
					int base = static_cast<int>(vx.data.size() - 3);
					triIdx.add(base);
					triIdx.add(base + 1);
					triIdx.add(base + 2);
				}
			}
		}

		// Recurse children
		for (int childIdx : node.children) {
			gatherNode(model, model.nodes[childIdx], triIdx, vx, vy, vz, tu, tv, hv);
		}
	}
};

/* -------------------------------------------------------------------------- */
/* Axis‑aligned bounding box utilities */

class Aabb
{
public:
	float minx, miny, minz, maxx, maxy, maxz;
	Aabb(float minx_, float miny_, float minz_, float maxx_, float maxy_, float maxz_) :
			minx(minx_), miny(miny_), minz(minz_), maxx(maxx_), maxy(maxy_), maxz(maxz_)
	{
	}

	static Aabb fromTrisObj(const std::vector<float> &x0, const std::vector<float> &y0,
			const std::vector<float> &z0, const std::vector<float> &x1,
			const std::vector<float> &y1, const std::vector<float> &z1,
			const std::vector<float> &x2, const std::vector<float> &y2,
			const std::vector<float> &z2)
	{
		float mnx = std::numeric_limits<float>::infinity();
		float mny = std::numeric_limits<float>::infinity();
		float mnz = std::numeric_limits<float>::infinity();
		float mxx = -std::numeric_limits<float>::infinity();
		float mxy = -std::numeric_limits<float>::infinity();
		float mxz = -std::numeric_limits<float>::infinity();

		size_t n = x0.size();
		for (size_t i = 0; i < n; ++i) {
			const float xs[3] = {x0[i], x1[i], x2[i]};
			const float ys[3] = {y0[i], y1[i], y2[i]};
			const float zs[3] = {z0[i], z1[i], z2[i]};
			for (int k = 0; k < 3; ++k) {
				mnx = std::min(mnx, xs[k]);
				mny = std::min(mny, ys[k]);
				mnz = std::min(mnz, zs[k]);
				mxx = std::max(mxx, xs[k]);
				mxy = std::max(mxy, ys[k]);
				mxz = std::max(mxz, zs[k]);
			}
		}
		return Aabb(mnx, mny, mnz, mxx, mxy, mxz);
	}

	static Aabb tileAroundCenter(const Aabb &b, float tx, float ty, float tz)
	{
		float cx = 0.5f * (b.minx + b.maxx);
		float cy = 0.5f * (b.miny + b.maxy);
		float cz = 0.5f * (b.minz + b.maxz);
		return Aabb(cx - tx * 0.5f, cy - ty * 0.5f, cz - tz * 0.5f, cx + tx * 0.5f,
				cy + ty * 0.5f, cz + tz * 0.5f);
	}

	Aabb toCube() const
	{
		float sx = maxx - minx;
		float sy = maxy - miny;
		float sz = maxz - minz;
		float side = std::max({sx, sy, sz});
		float cx = 0.5f * (minx + maxx);
		float cy = 0.5f * (miny + maxy);
		float cz = 0.5f * (minz + maxz);
		float h = side * 0.5f;
		return Aabb(cx - h, cy - h, cz - h, cx + h, cy + h, cz + h);
	}
};

/* -------------------------------------------------------------------------- */
/* Voxel triangle structure‑of‑arrays (SOA) */

class VoxelTriSOA
{
public:
	int n;
	std::vector<float> x0, y0, z0, x1, y1, z1, x2, y2, z2;
	std::vector<float> e10x, e10y, e10z, e20x, e20y, e20z;
	std::vector<float> nx, ny, nz;
	std::vector<float> tu0, tv0, tu1, tv1, tu2, tv2;
	std::vector<bool> hasUV;
	int texW, texH, texStride;
	std::vector<int> texPixels;

	static VoxelTriSOA fromObjectSpace(
			const LoaderResult &L, const Aabb &cube, float unit)
	{
		VoxelTriSOA soa;
		soa.n = L.triCount;
		soa.x0.reserve(soa.n);
		soa.y0.reserve(soa.n);
		soa.z0.reserve(soa.n);
		soa.x1.reserve(soa.n);
		soa.y1.reserve(soa.n);
		soa.z1.reserve(soa.n);
		soa.x2.reserve(soa.n);
		soa.y2.reserve(soa.n);
		soa.z2.reserve(soa.n);
		soa.e10x.reserve(soa.n);
		soa.e10y.reserve(soa.n);
		soa.e10z.reserve(soa.n);
		soa.e20x.reserve(soa.n);
		soa.e20y.reserve(soa.n);
		soa.e20z.reserve(soa.n);
		soa.nx.reserve(soa.n);
		soa.ny.reserve(soa.n);
		soa.nz.reserve(soa.n);
		soa.tu0.reserve(soa.n);
		soa.tv0.reserve(soa.n);
		soa.tu1.reserve(soa.n);
		soa.tv1.reserve(soa.n);
		soa.tu2.reserve(soa.n);
		soa.tv2.reserve(soa.n);
		soa.hasUV.reserve(soa.n);

		for (int i = 0; i < soa.n; ++i) {
			float vx0 = (L.vx0[i] - cube.minx) / unit;
			float vy0 = (L.vy0[i] - cube.miny) / unit;
			float vz0 = (L.vz0[i] - cube.minz) / unit;
			float vx1 = (L.vx1[i] - cube.minx) / unit;
			float vy1 = (L.vy1[i] - cube.miny) / unit;
			float vz1 = (L.vz1[i] - cube.minz) / unit;
			float vx2 = (L.vx2[i] - cube.minx) / unit;
			float vy2 = (L.vy2[i] - cube.miny) / unit;
			float vz2 = (L.vz2[i] - cube.minz) / unit;

			soa.x0.push_back(vx0);
			soa.y0.push_back(vy0);
			soa.z0.push_back(vz0);
			soa.x1.push_back(vx1);
			soa.y1.push_back(vy1);
			soa.z1.push_back(vz1);
			soa.x2.push_back(vx2);
			soa.y2.push_back(vy2);
			soa.z2.push_back(vz2);

			soa.e10x.push_back(vx1 - vx0);
			soa.e10y.push_back(vy1 - vy0);
			soa.e10z.push_back(vz1 - vz0);
			soa.e20x.push_back(vx2 - vx0);
			soa.e20y.push_back(vy2 - vy0);
			soa.e20z.push_back(vz2 - vz0);

			float cx =
					soa.e10y.back() * soa.e20z.back() - soa.e10z.back() * soa.e20y.back();
			float cy =
					soa.e10z.back() * soa.e20x.back() - soa.e10x.back() * soa.e20z.back();
			float cz =
					soa.e10x.back() * soa.e20y.back() - soa.e10y.back() * soa.e20x.back();
			float len = std::sqrt(cx * cx + cy * cy + cz * cz);
			if (len > 1e-20f) {
				soa.nx.push_back(cx / len);
				soa.ny.push_back(cy / len);
				soa.nz.push_back(cz / len);
			} else {
				soa.nx.push_back(0);
				soa.ny.push_back(0);
				soa.nz.push_back(0);
			}

			soa.tu0.push_back(L.tu0[i]);
			soa.tv0.push_back(L.tv0[i]);
			soa.tu1.push_back(L.tu1[i]);
			soa.tv1.push_back(L.tv1[i]);
			soa.tu2.push_back(L.tu2[i]);
			soa.tv2.push_back(L.tv2[i]);
			soa.hasUV.push_back(L.hasUV[i]);
		}

		soa.texW = L.texW;
		soa.texH = L.texH;
		soa.texStride = L.texStride;
		soa.texPixels = L.texPixels;

		return soa;
	}
};

/* -------------------------------------------------------------------------- */
/* Utility functions */

static inline int clamp(int v, int lo, int hi)
{
	return v < lo ? lo : (v > hi ? hi : v);
}
static inline float clamp01(float a)
{
	return a < 0 ? 0 : (a > 1 ? 1 : a);
}

/* CUDA‑style bilinear sample */
static std::array<uint8_t, 4> sampleCUDA(const std::vector<int> &pix, int w, int h,
	int stride, float u, float v, bool flipV)
{
	if (pix.empty() || w <= 0 || h <= 0)
		return {255, 255, 255, 255};
	float U = u * (w - 1);
	float V = (flipV ? (1.0f - v) : v) * (h - 1);
	int x0 = static_cast<int>(std::floor(U));
	int y0 = static_cast<int>(std::floor(V));
	int x1 = std::min(x0 + 1, w - 1);
	int y1 = std::min(y0 + 1, h - 1);
	float dx = U - x0;
	float dy = V - y0;

	int c00 = pix[y0 * stride + x0];
	int c10 = pix[y0 * stride + x1];
	int c01 = pix[y1 * stride + x0];
	int c11 = pix[y1 * stride + x1];

	float r00 = (c00 >> 16) & 255, g00 = (c00 >> 8) & 255, b00 = c00 & 255;
	float r10 = (c10 >> 16) & 255, g10 = (c10 >> 8) & 255, b10 = c10 & 255;
	float r01 = (c01 >> 16) & 255, g01 = (c01 >> 8) & 255, b01 = c01 & 255;
	float r11 = (c11 >> 16) & 255, g11 = (c11 >> 8) & 255, b11 = c11 & 255;

	float w00 = (1 - dx) * (1 - dy);
	float w10 = dx * (1 - dy);
	float w01 = (1 - dx) * dy;
	float w11 = dx * dy;

	int r = static_cast<int>(r00 * w00 + r10 * w10 + r01 * w01 + r11 * w11);
	int g = static_cast<int>(g00 * w00 + g10 * w10 + g01 * w01 + g11 * w11);
	int b = static_cast<int>(b00 * w00 + b10 * w10 + b01 * w01 + b11 * w11);

	r = std::clamp(r, 0, 255);
	g = std::clamp(g, 0, 255);
	b = std::clamp(b, 0, 255);
	//return (r << 16) | (g << 8) | b;
	return {{static_cast<uint8_t>(r), static_cast<uint8_t>(g), static_cast<uint8_t>(b),
			0xff}};
}

/* -------------------------------------------------------------------------- */
/* Rasterisation per slab (parallel) */

static int rasterizeSlab(const VoxelTriSOA &T, const IntArray &triList, int z0, int z1,
		int G, bool flipV, std::vector<bool> &occ,
		std::vector<uint32_t> &colors, std::vector<float> &bestD2)
{
	const float eps = EPS;
	const float nearFrac = NEAR_W_FRACTION;

	const int *tex = T.texPixels.empty() ? nullptr : T.texPixels.data();
	int texW = T.texW, texH = T.texH, texStride = T.texStride;

	const int *triIdx = triList.data.data();
	size_t triCount = triList.size();

	for (size_t ii = 0; ii < triCount; ++ii) {
		int i = triIdx[ii];

		// Vertices
		float x0 = T.x0[i], y0 = T.y0[i], z00 = T.z0[i];
		float x1 = T.x1[i], y1 = T.y1[i], z1f = T.z1[i];
		float x2 = T.x2[i], y2 = T.y2[i], z2f = T.z2[i];

		float nx = T.nx[i], ny = T.ny[i], nz = T.nz[i];

		// AABB in voxel space
		int minZ =
				clamp(static_cast<int>(std::floor(std::min({z00, z1f, z2f}))), 0, G - 1);
		int maxZ =
				clamp(static_cast<int>(std::ceil(std::max({z00, z1f, z2f}))), 0, G - 1);

		// Skip if slab does not intersect
		if (maxZ < z0 || minZ >= z1)
			continue;

		// Dominant axis
		float abx = std::abs(nx), aby = std::abs(ny), abz = std::abs(nz);
		int wAxis;
		if (abz >= abx && abz >= aby) {
			wAxis = 2;
		} else if (aby >= abx) {
			wAxis = 1;
		} else {
			wAxis = 0;
		}

		// Map to (U,V,W)
		float U0, V0, W0, U1, V1, W1, U2, V2, W2;
		if (wAxis == 2) { // Z‑major
			U0 = x0;
			V0 = y0;
			W0 = z00;
			U1 = x1;
			V1 = y1;
			W1 = z1f;
			U2 = x2;
			V2 = y2;
			W2 = z2f;
		} else if (wAxis == 1) { // Y‑major
			U0 = z00;
			V0 = x0;
			W0 = y0;
			U1 = z1f;
			V1 = x1;
			W1 = y1;
			U2 = z2f;
			V2 = x2;
			W2 = y2;
		} else { // X‑major
			U0 = y0;
			V0 = z00;
			W0 = x0;
			U1 = y1;
			V1 = z1f;
			W1 = x1;
			U2 = y2;
			V2 = z2f;
			W2 = x2;
		}

		// 2D bbox on (U,V)
		int uMinRaw = static_cast<int>(std::floor(std::min({U0, U1, U2})));
		int vMinRaw = static_cast<int>(std::floor(std::min({V0, V1, V2})));
		int uMaxRaw = static_cast<int>(std::ceil(std::max({U0, U1, U2})));
		int vMaxRaw = static_cast<int>(std::ceil(std::max({V0, V1, V2})));
		int uMin = clamp(uMinRaw, 0, G - 1);
		int vMin = clamp(vMinRaw, 0, G - 1);
		int uMax = clamp(uMaxRaw, 0, G - 1);
		int vMax = clamp(vMaxRaw, 0, G - 1);
		if (uMin > uMax || vMin > vMax)
			continue;

		// Barycentric gradients
		float denom = (V1 - V2) * (U0 - U2) + (U2 - U1) * (V0 - V2);
		if (std::abs(denom) < 1e-12f)
			continue;
		float invDen = 1.0f / denom;
		float dL0du = (V1 - V2) * invDen;
		float dL1du = (V2 - V0) * invDen;

		// W interpolation coefficients
		float Wc0 = W0 - W2;
		float Wc1 = W1 - W2;
		float WcC = W2;

		// Normal w‑component for depth scale
		float nW = (wAxis == 2 ? nz : (wAxis == 1 ? ny : nx));
		float nLen2 = nx * nx + ny * ny + nz * nz;
		if (nLen2 < 1e-20f)
			continue;
		float depthScale = (nW * nW) / nLen2;

		// UVs
		float tu0 = T.tu0[i], tv0 = T.tv0[i];
		float tu1 = T.tu1[i], tv1 = T.tv1[i];
		float tu2 = T.tu2[i], tv2 = T.tv2[i];
		bool hasUV = T.hasUV[i] && tex != nullptr;
		auto sampleColor = [&](float l0, float l1, float l2) -> uint32_t {
			if (!hasUV)
				return 0xFFFFFF;
			const auto col = sampleCUDA(T.texPixels, texW, texH, texStride,
					clamp01(l0 * tu0 + l1 * tu1 + l2 * tu2),
					clamp01(l0 * tv0 + l1 * tv1 + l2 * tv2), flipV);
			return (static_cast<uint32_t>(col[0]) << 16) |
				   (static_cast<uint32_t>(col[1]) << 8) |
				   static_cast<uint32_t>(col[2]);
		};

		// Scan rows in V
		for (int v = vMin; v <= vMax; ++v) {
			float u0c = uMin + 0.5f;
			float vc = v + 0.5f;

			// L0, L1 at (u0c, vc)
			float L0 = ((V1 - V2) * (u0c - U2) + (U2 - U1) * (vc - V2)) * invDen;
			float L1 = ((V2 - V0) * (u0c - U2) + (U0 - U2) * (vc - V2)) * invDen;
			float L2 = 1.0f - L0 - L1;

			// W at start of row and dW/du
			float W = Wc0 * L0 + Wc1 * L1 + WcC;
			float dWdu = dL0du * Wc0 + dL1du * Wc1;

			float L0du = dL0du, L1du = dL1du;

			for (int u = uMin; u <= uMax; ++u) {
				if (L0 >= -eps && L1 >= -eps && L2 >= -eps) {
					int ix, iy, iz;
					int wIdx = static_cast<int>(std::floor(W));

					// Map back to (x,y,z)
					if (wAxis == 2) {
						ix = u;
						iy = v;
						iz = wIdx;
					} else if (wAxis == 1) {
						ix = v;
						iy = wIdx;
						iz = u;
					} else {
						ix = wIdx;
						iy = u;
						iz = v;
					}

					if (iz >= z0 && iz < z1 && ix >= 0 && ix < G && iy >= 0 && iy < G) {
						int lin = ix + G * (iy + G * iz);
						float delta =
								W - ((wAxis == 2 ? iz : (wAxis == 1 ? iy : ix)) + 0.5f);
						float d2 = delta * delta * depthScale;

						if (d2 < bestD2[lin]) {
							bestD2[lin] = d2;
							occ[lin] = true;
							colors[lin] = sampleColor(L0, L1, L2);
						}
					}

					// Near‑slice handling
					float frac = W - static_cast<float>(std::floor(W));
					if (frac < nearFrac || frac > 1.0f - nearFrac) {
						int w2 = ((W - (wIdx + 0.5f)) < 0) ? (wIdx - 1) : (wIdx + 1);

						int ix2, iy2, iz2;
						if (wAxis == 2) {
							ix2 = u;
							iy2 = v;
							iz2 = w2;
						} else if (wAxis == 1) {
							ix2 = v;
							iy2 = w2;
							iz2 = u;
						} else {
							ix2 = w2;
							iy2 = u;
							iz2 = v;
						}

						if (iz2 >= z0 && iz2 < z1 && ix2 >= 0 && ix2 < G && iy2 >= 0 &&
								iy2 < G && w2 >= 0 && w2 < G) {
							int lin2 = ix2 + G * (iy2 + G * iz2);
							float delta2 = W - (static_cast<float>(w2) + 0.5f);
							float d2b = delta2 * delta2 * depthScale;
							if (d2b < bestD2[lin2]) {
								bestD2[lin2] = d2b;
								occ[lin2] = true;
								colors[lin2] = sampleColor(L0, L1, L2);
							}
						}
					}
				}

				// advance u
				L0 += L0du;
				L1 += L1du;
				L2 = 1.0f - L0 - L1;
				W += dWdu;
			}
		}
	}

	int count = 0;
	const int idxStart = z0 * G * G;
	const int idxEnd = z1 * G * G;
	for (int p = idxStart; p < idxEnd; ++p) {
		if (occ[p])
			++count;
	}
	return count;
}

/* -------------------------------------------------------------------------- */
/* CpuVoxelizer public API implementation */

CpuVoxelizer::CpuVoxelizer(int gridSize, bool tiles3d, bool verbose) :
		grid_(std::max(8, gridSize)), tiles3d_(tiles3d), verbose_(verbose)
{
}
/*
std::string CpuVoxelizer::baseName(const std::filesystem::path &p) const
{
	return p.stem().string();
}
*/

CpuVoxelizer::Stats CpuVoxelizer::voxelizeSingleGLB(
		const TileData &tile, const callback_t &callback
		//const std::filesystem::path& glbPath,
		//const std::filesystem::path& outDir
) const
{
	if (verbose_) {
		std::cout << "[Load] " << tile.url << std::endl;
	}
	tinygltf::Model model;
	tinygltf::TinyGLTF loader;
	std::string err, warn;

	LoaderResult lr = GltfReader::loadTrianglesAndGlobalTexture(tile);
	if (lr.triCount == 0) {
		return {};
		//throw std::runtime_error("No triangles in " + tile.url);
	}

	// Compute bounding box
	Aabb baseBox = Aabb::fromTrisObj(
			lr.vx0, lr.vy0, lr.vz0, lr.vx1, lr.vy1, lr.vz1, lr.vx2, lr.vy2, lr.vz2);
	Aabb rect =
			tiles3d_ ? Aabb::tileAroundCenter(baseBox, TILE_X, TILE_Y, TILE_Z) : baseBox;
	Aabb cube = rect.toCube();

	float maxDim = std::max(
			{cube.maxx - cube.minx, cube.maxy - cube.miny, cube.maxz - cube.minz});
	float unit = maxDim / static_cast<float>(grid_);
	if (unit <= 0)
		return {};
	//	throw std::runtime_error("Non‑positive unit");

	int ox = static_cast<int>(std::round((-cube.minx) / unit));
	int oy = static_cast<int>(std::round((-cube.miny) / unit));
	int oz = static_cast<int>(std::round((-cube.minz) / unit));

	// Transform triangles into voxel space
	VoxelTriSOA soa = VoxelTriSOA::fromObjectSpace(lr, cube, unit);

	// Bin triangles into slabs
	int slabH = std::min(SLAB_HEIGHT, std::max(1, grid_));
	int slabCount = (grid_ + slabH - 1) / slabH;
	std::vector<IntArray> slabBins;
	slabBins.reserve(slabCount);
	for (int s = 0; s < slabCount; ++s)
		slabBins.emplace_back(1024);

	for (int i = 0; i < soa.n; ++i) {
		int z0 = clamp(
				static_cast<int>(std::floor(std::min({soa.z0[i], soa.z1[i], soa.z2[i]}))),
				0, grid_ - 1);
		int z1 = clamp(
				static_cast<int>(std::ceil(std::max({soa.z0[i], soa.z1[i], soa.z2[i]}))),
				0, grid_ - 1);
		int s0 = z0 / slabH;
		int s1 = z1 / slabH;
		for (int s = s0; s <= s1; ++s)
			slabBins[s].add(i);
	}

	// Allocate output buffers
	int total = grid_ * grid_ * grid_;
	std::vector<bool> occ(total, false);
	std::vector<uint32_t> colors(total, 0xFFFFFF);
	std::vector<float> bestD2(total, std::numeric_limits<float>::infinity());

	// Parallel rasterisation (sequential fallback for simplicity)
	int filled = 0;
	for (int s = 0; s < slabCount; ++s) {
		int zStart = s * slabH;
		int zEnd = std::min(grid_, zStart + slabH);
		filled += rasterizeSlab(soa, slabBins[s], zStart, zEnd, grid_, FLIP_V, occ,
				colors, bestD2);
	}

	for (int i = 0; i < total; ++i) {
		if (!occ[i])
			continue;
		const int z = i / (grid_ * grid_);
		const int rem = i - z * grid_ * grid_;
		const int y = rem / grid_;
		const int x = rem - y * grid_;
		const uint32_t rgb = colors[i];
		callback(x - ox, y - oy, z - oz, (rgb >> 16) & 0xFF, (rgb >> 8) & 0xFF,
				rgb & 0xFF, 0xFF);
	}
	/*
	// Emit JSON and position files
	std::filesystem::create_directories(outDir);
	std::string base = baseName(glbPath);
	std::filesystem::path jsonOut =
			outDir / (base + "_" + std::to_string(grid_) + ".json");
	emitJSON(jsonOut, colors, occ, grid_, ox, oy, oz);
	std::filesystem::path posOut = outDir / (base + "_position.json");
	emitPosition(posOut, ox, oy, oz);
*/
	return Stats{grid_, soa.n, filled, ox, oy, oz, colors};
}

/* -------------------------------------------------------------------------- */
/* JSON emission helpers */
/*
void CpuVoxelizer::emitJSON(const std::filesystem::path &out,
		const std::vector<uint32_t> &colors, const std::vector<bool> &occupancy, int G,
		int ox, int oy, int oz) const
{
	std::unordered_map<int, int> palette;
	int nextIdx = 1;
	std::ostringstream xyzi;
	xyzi << "\"xyzi\":[";
	bool first = true;
	for (size_t i = 0; i < occupancy.size(); ++i) {
		if (!occupancy[i])
			continue;
		int z = static_cast<int>(i / (G * G));
		int rem = static_cast<int>(i - z * G * G);
		int y = rem / G;
		int x = rem - y * G;
		int rgb = static_cast<int>(colors[i]);
		int idx;
		auto it = palette.find(rgb);
		if (it == palette.end()) {
			idx = nextIdx++;
			palette[rgb] = idx;
		} else {
			idx = it->second;
		}
		if (!first)
			xyzi << ",";
		xyzi << "[" << (x - ox) << "," << (y - oy) << "," << (z - oz) << "," << idx
			 << "]";
		first = false;
	}
	xyzi << "]";

	std::ostringstream blocks;
	blocks << "\"blocks\":{";
	bool firstBlock = true;
	for (auto &kv : palette) {
		int rgb = kv.first;
		int idx = kv.second;
		int r = (rgb >> 16) & 0xFF;
		int g = (rgb >> 8) & 0xFF;
		int b = rgb & 0xFF;
		if (!firstBlock)
			blocks << ",";
		blocks << "\"" << idx << "\":[" << r << "," << g << "," << b << "]";
		firstBlock = false;
	}
	blocks << "}";

	std::ofstream ofs(out);
	ofs << "{" << blocks.str() << "," << xyzi.str() << "}";
}

void CpuVoxelizer::emitPosition(
		const std::filesystem::path &out, int ox, int oy, int oz) const
{
	std::ofstream ofs(out);
	ofs << "[{\"translation\":[" << ox << "," << oy << "," << oz
		<< "],\"origin\":[0,0,0]}]";
}
*/
