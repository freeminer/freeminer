/*
This file is part of Freeminer.
*/

#pragma once

#include <cstdint>
#include <memory>
#include <string>
#include <vector>

struct TerrainDiffusionSample;

class TerrainDiffusionNativePipeline
{
public:
	TerrainDiffusionNativePipeline(uint64_t seed, int node_scale, float height_scale,
			float height_offset, float residual_std, unsigned int cache_tiles,
			unsigned int cache_mb, const std::string &provider, int device_id,
			int intra_threads, const std::string &conditioning_stats, bool prefetch);
	~TerrainDiffusionNativePipeline();

	bool load(const std::string &model_dir);
	bool loaded() const;
	bool sampleGrid(int min_x, int min_z, int max_x, int max_z,
			std::vector<TerrainDiffusionSample> &samples);
	bool sampleGridCached(int min_x, int min_z, int max_x, int max_z,
			std::vector<TerrainDiffusionSample> &samples);
	static bool runDeterminismSelfTest(std::string &error);

private:
	struct Impl;
	std::unique_ptr<Impl> m_impl;
};
