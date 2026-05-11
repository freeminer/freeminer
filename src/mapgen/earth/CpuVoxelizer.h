/**
 * CpuVoxelizer - C++ port of the JavaCpuVoxelizer library.
 *
 * This header provides the public API mirroring the original Java implementation.
 * The full voxelisation algorithm is not yet implemented; the methods are stubs
 * that compile and can be extended later.
 */

#ifndef CPU_VOXELIZER_H
#define CPU_VOXELIZER_H

#include <functional>
#include <string>
#include <cstdint>
#include <vector>
#include <filesystem>
//#include "mapgen/earth/luanti-earth/native/src/downloader.h"

struct TileData;
class CpuVoxelizer
{
public:
	/** Statistics returned by voxelisation. */
	struct Stats
	{
		//std::string baseName;
		int grid;
		int triangles;
		int filled;
		int ox, oy, oz;
		std::vector<uint32_t> colors;
		/*Sats(const std::string& baseName_, int grid_, int triangles_, int filled_,
              int ox_, int oy_, int oz_)
            : baseName(baseName_), grid(grid_), triangles(triangles_),
              filled(filled_), ox(ox_), oy(oy_), oz(oz_) {}*/
	};

	/**
     * Construct a voxeliser.
     *
     * @param gridSize Minimum grid resolution (will be clamped to >=8).
     * @param tiles3d  Whether to treat the input as 3‑D Tiles.
     * @param verbose  Enable console output.
     */
	CpuVoxelizer(int gridSize, bool tiles3d, bool verbose);

	/**
     * Voxelise a single GLB file.
     *
     * @param glbPath Path to the input GLB file.
     * @param outDir  Directory where JSON output will be written.
     * @return Statistics about the operation.
     *
     * @throws std::runtime_error on failure.
     */
using callback_t = 			std::function<void(const int &x, const int &y, const int &z,
					const uint8_t &r, const uint8_t &g, const uint8_t &b,
					const uint8_t &a)>;


     Stats voxelizeSingleGLB(const TileData &tile,
          const callback_t &callback

			//  const std::filesystem::path& glbPath,
			// const std::filesystem::path& outDir
	) const;

private:
	int grid_;
	bool tiles3d_;
	bool verbose_;

  /*
	// Internal helper functions (stubs for now)
	std::string baseName(const std::filesystem::path &p) const;
	void emitJSON(const std::filesystem::path &out, const std::vector<uint32_t> &colors,
			const std::vector<bool> &occupancy, int G, int ox, int oy, int oz) const;
	void emitPosition(const std::filesystem::path &out, int ox, int oy, int oz) const;
  */
};

#endif // CPU_VOXELIZER_H
