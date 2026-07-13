/*
This file is part of Freeminer.
*/

#include "config.h"
#include "mapgen_terraindiffusion_native.h"

#include <algorithm>
#include <array>
#include <cctype>
#include <cmath>
#include <cstdint>
#include <deque>
#include <fstream>
#include <limits>
#include <map>
#include <mutex>
#include <stdexcept>
#include <unordered_map>
#include <unordered_set>
#include <utility>
#include <vector>

#include <json/json.h>

#if USE_ONNXRUNTIME
#include <onnxruntime_cxx_api.h>
#endif

#include "log.h"
#include "filesys.h"
#include "mapgen_terraindiffusion.h"
#include "noise.h"
#include "porting.h"
#include "threading/ThreadPool.h"
#include "util/numeric.h"

namespace
{
constexpr int COARSE_SIZE = 64;
constexpr int BASE_SIZE = 64;
constexpr int LATENT_COMPRESSION = 8;
constexpr float SIGMA_DATA = 0.5f;
constexpr float SIGMA_MIN = 0.002f;
constexpr float SIGMA_MAX = 80.0f;
constexpr float LOWFREQ_MEAN = -31.4f;
constexpr float LOWFREQ_STD = 38.6f;

int floorDiv(int value, int divisor)
{
	int quotient = value / divisor;
	if (value % divisor < 0)
		--quotient;
	return quotient;
}

uint64_t tileSeed(uint64_t seed, int tile_y, int tile_x)
{
	uint64_t value = seed * UINT64_C(0x9E3779B9);
	value += static_cast<uint32_t>(tile_y);
	value = value * UINT64_C(0x9E3779B9) + static_cast<uint32_t>(tile_x);
	return value;
}

uint32_t pcgNext(uint64_t &state)
{
	state = state * UINT64_C(6364136223846793005) + UINT64_C(1442695040888963407);
	const uint32_t x = static_cast<uint32_t>(((state >> 18U) ^ state) >> 27U);
	const uint32_t rotation = static_cast<uint32_t>(state >> 59U);
	return (x >> rotation) | (x << ((32U - rotation) & 31U));
}

void fillNormal(uint64_t seed, std::vector<float> &output)
{
	uint64_t state = seed;
	size_t i = 0;
	constexpr double INV_U32 = 1.0 / 4294967296.0;
	while (i < output.size()) {
		const double v1 =
				2.0 * (static_cast<double>(pcgNext(state)) + 1.0) * INV_U32 - 1.0;
		const double v2 =
				2.0 * (static_cast<double>(pcgNext(state)) + 1.0) * INV_U32 - 1.0;
		const double s = v1 * v1 + v2 * v2;
		if (s <= 0.0 || s >= 1.0)
			continue;
		const double factor = std::sqrt(-2.0 * std::log(s) / s);
		output[i++] = static_cast<float>(v1 * factor);
		if (i < output.size())
			output[i++] = static_cast<float>(v2 * factor);
	}
}

std::vector<float> noisePatch(uint64_t seed, int y0, int x0, int height, int width,
		int channels, int tile_height, int tile_width)
{
	std::vector<float> output(static_cast<size_t>(channels) * height * width);
	const int ty0 = floorDiv(y0, tile_height);
	const int ty1 = floorDiv(y0 + height - 1, tile_height);
	const int tx0 = floorDiv(x0, tile_width);
	const int tx1 = floorDiv(x0 + width - 1, tile_width);

	for (int ty = ty0; ty <= ty1; ++ty) {
		for (int tx = tx0; tx <= tx1; ++tx) {
			std::vector<float> tile(
					static_cast<size_t>(channels) * tile_height * tile_width);
			fillNormal(tileSeed(seed, ty, tx), tile);
			const int src_y0 = std::max(y0, ty * tile_height) - ty * tile_height;
			const int src_x0 = std::max(x0, tx * tile_width) - tx * tile_width;
			const int dst_y0 = std::max(y0, ty * tile_height) - y0;
			const int dst_x0 = std::max(x0, tx * tile_width) - x0;
			const int copy_h = std::min(y0 + height, (ty + 1) * tile_height) -
							   std::max(y0, ty * tile_height);
			const int copy_w = std::min(x0 + width, (tx + 1) * tile_width) -
							   std::max(x0, tx * tile_width);

			for (int c = 0; c < channels; ++c)
				for (int y = 0; y < copy_h; ++y)
					for (int x = 0; x < copy_w; ++x) {
						const size_t src =
								(static_cast<size_t>(c) * tile_height + src_y0 + y) *
										tile_width +
								src_x0 + x;
						const size_t dst =
								(static_cast<size_t>(c) * height + dst_y0 + y) * width +
								dst_x0 + x;
						output[dst] = tile[src];
					}
		}
	}
	return output;
}

float bilinear(const std::vector<float> &data, int channels, int height, int width,
		int channel, float y, float x)
{
	(void)channels;
	y = rangelim(y, 0.0f, static_cast<float>(height - 1));
	x = rangelim(x, 0.0f, static_cast<float>(width - 1));
	const int y0 = static_cast<int>(std::floor(y));
	const int x0 = static_cast<int>(std::floor(x));
	const int y1 = std::min(y0 + 1, height - 1);
	const int x1 = std::min(x0 + 1, width - 1);
	const float fy = y - y0;
	const float fx = x - x0;
	auto at = [&](int yy, int xx) {
		return data[(static_cast<size_t>(channel) * height + yy) * width + xx];
	};
	return (at(y0, x0) * (1.0f - fx) + at(y0, x1) * fx) * (1.0f - fy) +
		   (at(y1, x0) * (1.0f - fx) + at(y1, x1) * fx) * fy;
}

std::vector<float> resizeBilinear(const std::vector<float> &source, int source_height,
		int source_width, int target_height, int target_width)
{
	std::vector<float> output(static_cast<size_t>(target_height) * target_width);
	const float scale_y = static_cast<float>(source_height) / target_height;
	const float scale_x = static_cast<float>(source_width) / target_width;
	for (int y = 0; y < target_height; ++y)
		for (int x = 0; x < target_width; ++x) {
			const float sy = (y + 0.5f) * scale_y - 0.5f;
			const float sx = (x + 0.5f) * scale_x - 0.5f;
			output[static_cast<size_t>(y) * target_width + x] =
					bilinear(source, 1, source_height, source_width, 0, sy, sx);
		}
	return output;
}

void gaussianBlur(std::vector<float> &values, int height, int width, float sigma)
{
	const int radius = std::max(1, static_cast<int>(sigma));
	std::vector<float> kernel(radius * 2 + 1);
	float kernel_sum = 0.0f;
	for (int i = -radius; i <= radius; ++i) {
		const float value = std::exp(-(i * i) / (2.0f * sigma * sigma));
		kernel[i + radius] = value;
		kernel_sum += value;
	}
	for (float &value : kernel)
		value /= kernel_sum;

	std::vector<float> temporary(values.size());
	for (int y = 0; y < height; ++y)
		for (int x = 0; x < width; ++x) {
			float sum = 0.0f;
			for (int i = -radius; i <= radius; ++i)
				sum += values[static_cast<size_t>(y) * width +
							   rangelim(x + i, 0, width - 1)] *
					   kernel[i + radius];
			temporary[static_cast<size_t>(y) * width + x] = sum;
		}
	for (int y = 0; y < height; ++y)
		for (int x = 0; x < width; ++x) {
			float sum = 0.0f;
			for (int i = -radius; i <= radius; ++i)
				sum += temporary[static_cast<size_t>(rangelim(y + i, 0, height - 1)) *
										 width +
								 x] *
					   kernel[i + radius];
			values[static_cast<size_t>(y) * width + x] = sum;
		}
}

struct ClimateFields
{
	std::vector<float> baseline;
	std::vector<float> beta;
};

ClimateFields calculateClimateFields(const std::vector<float> &coarse_map)
{
	constexpr int radius = 7;
	constexpr int window = radius * 2 + 1;
	constexpr size_t plane = COARSE_SIZE * COARSE_SIZE;
	ClimateFields fields;
	fields.baseline.resize(plane);
	fields.beta.resize(plane);
	auto value = [&](int channel, int z, int x) {
		return coarse_map[static_cast<size_t>(channel) * plane +
						  static_cast<size_t>(z) * COARSE_SIZE + x];
	};
	auto elevation = [&](int z, int x) {
		const float e = std::max(value(0, z, x), 0.0f);
		return e * e;
	};

	for (int z = 0; z < COARSE_SIZE; ++z)
		for (int x = 0; x < COARSE_SIZE; ++x) {
			double sum_t = 0.0;
			double sum_e = 0.0;
			double sum_e2 = 0.0;
			double sum_et = 0.0;
			int land = 0;
			for (int dz = -radius; dz <= radius; ++dz)
				for (int dx = -radius; dx <= radius; ++dx) {
					const int sz = rangelim(z + dz, 0, COARSE_SIZE - 1);
					const int sx = rangelim(x + dx, 0, COARSE_SIZE - 1);
					const float e = elevation(sz, sx);
					if (e <= 0.0f)
						continue;
					const float t = value(2, sz, sx);
					sum_t += t;
					sum_e += e;
					sum_e2 += static_cast<double>(e) * e;
					sum_et += static_cast<double>(e) * t;
					++land;
				}
			float beta = -0.0065f;
			if (land > 0) {
				const double mean_t = sum_t / land;
				const double mean_e = sum_e / land;
				const double variance = sum_e2 / land - mean_e * mean_e;
				const double covariance = sum_et / land - mean_e * mean_t;
				if (variance >= 1.0 &&
						static_cast<float>(land) / (window * window) >= 0.02f)
					beta = rangelim(static_cast<float>(covariance / (variance + 1e-6)),
							-0.012f, 0.0f);
			}
			const size_t index = static_cast<size_t>(z) * COARSE_SIZE + x;
			fields.beta[index] = beta;
			fields.baseline[index] = value(2, z, x) - beta * elevation(z, x);
		}
	return fields;
}

struct TileKey
{
	int x;
	int z;
	bool operator==(const TileKey &other) const { return x == other.x && z == other.z; }
};

struct TileKeyHash
{
	size_t operator()(const TileKey &key) const
	{
		const uint64_t x = static_cast<uint32_t>(key.x);
		const uint64_t z = static_cast<uint32_t>(key.z);
		return std::hash<uint64_t>{}((x << 32U) | z);
	}
};

float linearWeight(int coordinate, int size)
{
	const float middle = (size - 1) * 0.5f;
	return 1.0f - 0.999f * rangelim(std::fabs(coordinate - middle) / middle, 0.0f, 1.0f);
}

#if USE_ONNXRUNTIME
class OnnxStage
{
public:
	bool load(Ort::Env &env, Ort::SessionOptions &options, const std::string &path)
	{
		m_session = std::make_unique<Ort::Session>(env, path.c_str(), options);
		Ort::AllocatorWithDefaultOptions allocator;
		for (size_t i = 0; i < m_session->GetInputCount(); ++i) {
			auto name = m_session->GetInputNameAllocated(i, allocator);
			m_input_names.emplace_back(name.get());
			m_input_shapes.push_back(m_session->GetInputTypeInfo(i)
							.GetTensorTypeAndShapeInfo()
							.GetShape());
		}
		auto output_name = m_session->GetOutputNameAllocated(0, allocator);
		m_output_name = output_name.get();
		return true;
	}

	const std::vector<int64_t> &inputShape(size_t index) const
	{
		return m_input_shapes.at(index);
	}

	size_t inputCount() const { return m_input_names.size(); }

	bool run(const std::vector<std::vector<float>> &input_data,
			const std::vector<std::vector<int64_t>> &input_shapes,
			std::vector<float> &output) const
	{
		if (!m_session || input_data.size() != m_input_names.size() ||
				input_shapes.size() != input_data.size())
			return false;
		Ort::MemoryInfo memory =
				Ort::MemoryInfo::CreateCpu(OrtArenaAllocator, OrtMemTypeDefault);
		std::vector<Ort::Value> tensors;
		std::vector<const char *> names;
		tensors.reserve(input_data.size());
		names.reserve(input_data.size());
		for (size_t i = 0; i < input_data.size(); ++i) {
			names.push_back(m_input_names[i].c_str());
			auto &data = const_cast<std::vector<float> &>(input_data[i]);
			tensors.emplace_back(Ort::Value::CreateTensor<float>(memory, data.data(),
					data.size(), input_shapes[i].data(), input_shapes[i].size()));
		}
		const char *output_name = m_output_name.c_str();
		auto values = m_session->Run(Ort::RunOptions{nullptr}, names.data(),
				tensors.data(), tensors.size(), &output_name, 1);
		if (values.empty() || !values[0].IsTensor())
			return false;
		const size_t count = values[0].GetTensorTypeAndShapeInfo().GetElementCount();
		const float *data = values[0].GetTensorData<float>();
		output.assign(data, data + count);
		return true;
	}

private:
	std::unique_ptr<Ort::Session> m_session;
	std::vector<std::string> m_input_names;
	std::vector<std::vector<int64_t>> m_input_shapes;
	std::string m_output_name;
};

struct SharedModels
{
	std::shared_ptr<Ort::Env> env;
	std::shared_ptr<Ort::SessionOptions> options;
	OnnxStage coarse;
	OnnxStage base;
	OnnxStage decoder;
	std::string provider;
};

bool hasProvider(const std::vector<std::string> &providers, const std::string &provider)
{
	return std::find(providers.begin(), providers.end(), provider) != providers.end();
}

std::shared_ptr<SharedModels> acquireSharedModels(const std::string &model_dir,
		std::string requested_provider, int device_id, int intra_threads)
{
	static std::mutex mutex;
	static std::map<std::string, std::weak_ptr<SharedModels>> bundles;
	std::transform(requested_provider.begin(), requested_provider.end(),
			requested_provider.begin(), [](unsigned char c) { return std::tolower(c); });
	const std::string key = model_dir + "|" + requested_provider + "|" +
							std::to_string(device_id) + "|" +
							std::to_string(intra_threads);
	std::lock_guard<std::mutex> lock(mutex);
	if (auto existing = bundles[key].lock())
		return existing;

	auto models = std::make_shared<SharedModels>();
	models->env = std::make_shared<Ort::Env>(
			ORT_LOGGING_LEVEL_WARNING, "freeminer-terraindiffusion-native");
	models->options = std::make_shared<Ort::SessionOptions>();
	models->options->SetIntraOpNumThreads(std::max(1, intra_threads));
	models->options->SetGraphOptimizationLevel(GraphOptimizationLevel::ORT_ENABLE_ALL);

	const std::vector<std::string> available = Ort::GetAvailableProviders();
	std::string provider = requested_provider;
	if (provider.empty() || provider == "auto") {
		if (hasProvider(available, "CUDAExecutionProvider"))
			provider = "cuda";
		else if (hasProvider(available, "ROCMExecutionProvider"))
			provider = "rocm";
		else
			provider = "cpu";
	}
	if (provider == "cuda") {
		if (!hasProvider(available, "CUDAExecutionProvider"))
			throw std::runtime_error("CUDA execution provider is not available");
		Ort::CUDAProviderOptions cuda_options;
		cuda_options.Update({{"device_id", std::to_string(device_id)}});
		models->options->AppendExecutionProvider_CUDA_V2(*cuda_options);
	} else if (provider == "rocm") {
		if (!hasProvider(available, "ROCMExecutionProvider"))
			throw std::runtime_error("ROCm execution provider is not available");
		OrtROCMProviderOptions rocm_options{};
		rocm_options.device_id = device_id;
		models->options->AppendExecutionProvider_ROCM(rocm_options);
	} else if (provider != "cpu") {
		throw std::runtime_error("unknown execution provider '" + provider + "'");
	}
	models->provider = provider;
	models->coarse.load(
			*models->env, *models->options, model_dir + DIR_DELIM + "coarse_model.onnx");
	models->base.load(
			*models->env, *models->options, model_dir + DIR_DELIM + "base_model.onnx");
	models->decoder.load(
			*models->env, *models->options, model_dir + DIR_DELIM + "decoder_model.onnx");
	bundles[key] = models;
	return models;
}
#endif
}

struct TerrainDiffusionNativePipeline::Impl
{
	Impl(uint64_t seed_, int node_scale_, float height_scale_, float height_offset_,
			float residual_std_, unsigned int cache_tiles_, unsigned int cache_mb_,
			std::string provider_, int device_id_, int intra_threads_,
			std::string conditioning_stats_, bool prefetch_) :
			seed(seed_), node_scale(std::max(1, node_scale_)),
			height_scale(height_scale_), height_offset(height_offset_),
			residual_std(residual_std_), cache_limit(std::max(1U, cache_tiles_)),
			cache_bytes_limit(
					static_cast<size_t>(std::max(16U, cache_mb_)) * 1024 * 1024),
			provider(std::move(provider_)), device_id(device_id_),
			intra_threads(std::max(1, intra_threads_)),
			conditioning_stats(std::move(conditioning_stats_)), prefetch(prefetch_)
	{
		if (prefetch) {
			prefetch_pool = std::make_unique<progschj::ThreadPool>(1);
			prefetch_pool->set_queue_size_limit(16);
		}
	}

	uint64_t seed;
	int node_scale;
	float height_scale;
	float height_offset;
	float residual_std;
	unsigned int cache_limit;
	size_t cache_bytes_limit;
	std::string provider;
	int device_id;
	int intra_threads;
	std::string conditioning_stats;
	bool prefetch;
	struct ConditioningStats
	{
		std::array<std::vector<float>, 5> noise_quantiles;
		std::array<std::vector<float>, 5> data_quantiles;
		float a_temp_std = 0.0f;
		float b_temp_std = 0.0f;
		float temp_std_p1 = 0.0f;
		float temp_std_p99 = 1.0f;
		bool loaded = false;
	} stats;
	std::array<float, 6> coarse_means{
			-37.7000079f, 1.14030653f, 18.1024866f, 332.83426f, 1332.2079f, 52.6600876f};
	std::array<float, 6> coarse_stds{
			39.7419987f, 1.76818442f, 8.92146873f, 321.766022f, 842.929382f, 31.0799847f};
	std::array<float, 5> cond_snr{0.5f, 0.5f, 0.5f, 0.5f, 0.5f};
	std::array<float, 5> frequency_mult{1.0f, 1.0f, 1.0f, 1.0f, 1.0f};
	float drop_water_pct = 0.5f;
	bool is_loaded = false;
	int decoder_size = 0;
	int usable_size = 0;

	struct Tile
	{
		std::vector<TerrainDiffusionSample> samples;
	};
	struct TileCacheEntry
	{
		Tile tile;
		uint64_t last_use = 0;
	};
	struct FloatCacheEntry
	{
		std::vector<float> data;
		uint64_t last_use = 0;
	};
	std::unordered_map<TileKey, TileCacheEntry, TileKeyHash> cache;
	uint64_t cache_clock = 0;
	std::mutex cache_mutex;
	std::unordered_set<TileKey, TileKeyHash> pending_prefetch;

#if USE_ONNXRUNTIME
	std::shared_ptr<SharedModels> models;
	std::unordered_map<TileKey, FloatCacheEntry, TileKeyHash> coarse_cache;
	std::unordered_map<TileKey, FloatCacheEntry, TileKeyHash> base_cache;
#endif
	std::unique_ptr<progschj::ThreadPool> prefetch_pool;

	template <typename Cache, typename SizeFunction>
	void trimCache(Cache &target, size_t byte_limit, const TileKey &protected_key,
			SizeFunction size_function,
			size_t count_limit = std::numeric_limits<size_t>::max())
	{
		auto bytes = [&]() {
			size_t total = 0;
			for (const auto &entry : target)
				total += size_function(entry.second);
			return total;
		};
		while ((target.size() > count_limit || bytes() > byte_limit) &&
				target.size() > 1) {
			auto oldest = target.end();
			for (auto it = target.begin(); it != target.end(); ++it) {
				if (it->first == protected_key)
					continue;
				if (oldest == target.end() ||
						it->second.last_use < oldest->second.last_use)
					oldest = it;
			}
			if (oldest == target.end())
				break;
			target.erase(oldest);
		}
	}

	float worldNoise(int x, int z, float frequency, int offset, int octaves) const
	{
		float value = 0.0f;
		float amplitude = 1.0f;
		float total = 0.0f;
		for (int octave = 0; octave < octaves; ++octave) {
			value += noise2d_value(x * frequency, z * frequency,
							 static_cast<s32>(seed) + offset + octave, true) *
					 amplitude;
			total += amplitude;
			frequency *= 2.0f;
			amplitude *= 0.5f;
		}
		return total > 0.0f ? value / total : 0.0f;
	}

	bool loadConditioningStats()
	{
		if (conditioning_stats.empty())
			return false;
		std::ifstream input(conditioning_stats);
		if (!input.good()) {
			warningstream << "TerrainDiffusion conditioning statistics not found: "
						  << conditioning_stats << std::endl;
			return false;
		}
		Json::CharReaderBuilder builder;
		Json::Value root;
		std::string errors;
		if (!Json::parseFromStream(builder, input, &root, &errors)) {
			warningstream << "TerrainDiffusion failed to parse conditioning statistics "
						  << conditioning_stats << ": " << errors << std::endl;
			return false;
		}
		const Json::Value &noise_tables = root["noise_quantile_tables"];
		const Json::Value &data_tables = root["data_quantile_tables"];
		if (!noise_tables.isArray() || !data_tables.isArray() ||
				noise_tables.size() < 5 || data_tables.size() < 5)
			return false;
		for (int channel = 0; channel < 5; ++channel) {
			const Json::Value &noise = noise_tables[channel];
			const Json::Value &data = data_tables[channel];
			if (!noise.isArray() || noise.size() < 2 || noise.size() != data.size())
				return false;
			stats.noise_quantiles[channel].reserve(noise.size());
			stats.data_quantiles[channel].reserve(data.size());
			for (Json::ArrayIndex i = 0; i < noise.size(); ++i) {
				stats.noise_quantiles[channel].push_back(noise[i].asFloat());
				stats.data_quantiles[channel].push_back(data[i].asFloat());
			}
		}
		stats.a_temp_std = root["a_temp_std"].asFloat();
		stats.b_temp_std = root["b_temp_std"].asFloat();
		stats.temp_std_p1 = root["temp_std_p1"].asFloat();
		stats.temp_std_p99 = root["temp_std_p99"].asFloat();
		stats.loaded = true;
		infostream << "TerrainDiffusion loaded WorldClim conditioning statistics "
				   << conditioning_stats << std::endl;
		return true;
	}

	void loadPipelineConfig(const std::string &model_dir)
	{
		const std::string path = model_dir + DIR_DELIM + "config.json";
		std::ifstream input(path);
		if (!input.good())
			return;
		Json::CharReaderBuilder builder;
		Json::Value root;
		std::string errors;
		if (!Json::parseFromStream(builder, input, &root, &errors)) {
			warningstream << "TerrainDiffusion failed to parse pipeline config " << path
						  << ": " << errors << std::endl;
			return;
		}
		auto read_array = [&](const char *name, auto &destination) {
			const Json::Value &values = root[name];
			if (!values.isArray() || values.size() != destination.size())
				return;
			for (Json::ArrayIndex i = 0; i < values.size(); ++i)
				destination[i] = values[i].asFloat();
		};
		read_array("coarse_means", coarse_means);
		read_array("coarse_stds", coarse_stds);
		read_array("cond_snr", cond_snr);
		read_array("frequency_mult", frequency_mult);
		if (root["drop_water_pct"].isNumeric())
			drop_water_pct = root["drop_water_pct"].asFloat();
	}

	float transformQuantile(int channel, float value) const
	{
		const auto &source = stats.noise_quantiles[channel];
		const auto &target = stats.data_quantiles[channel];
		if (value <= source.front())
			return target.front();
		if (value >= source.back())
			return target.back();
		const auto upper = std::upper_bound(source.begin(), source.end(), value);
		const size_t hi = static_cast<size_t>(upper - source.begin());
		const size_t lo = hi - 1;
		const float span = source[hi] - source[lo];
		const float factor = span > 0.0f ? (value - source[lo]) / span : 0.0f;
		return target[lo] * (1.0f - factor) + target[hi] * factor;
	}

	std::array<float, 5> conditioning(int coarse_x, int coarse_z) const
	{
		if (stats.loaded) {
			static const std::array<int, 5> octaves{4, 2, 4, 4, 4};
			std::array<float, 5> raw{};
			for (int channel = 0; channel < 5; ++channel) {
				const float noise = worldNoise(coarse_x, coarse_z,
						0.05f * frequency_mult[channel], channel + 1, octaves[channel]);
				raw[channel] = transformQuantile(channel, noise);
			}
			const float elevation = raw[0];
			const float precipitation = std::max(raw[3], 0.0f);
			const float lapse =
					rangelim(-6.5f + 0.0015f * precipitation, -9.8f, -4.0f) / 1000.0f;
			float temperature =
					rangelim(raw[1] + lapse * std::max(elevation, 0.0f), -10.0f, 40.0f);
			if (temperature <= 20.0f)
				temperature = (temperature - 20.0f) * 1.25f + 20.0f;
			const float std_range = stats.temp_std_p99 - stats.temp_std_p1;
			const float normalized =
					std_range != 0.0f ? (raw[2] - stats.temp_std_p1) / std_range : 0.0f;
			const float baseline = std::max(stats.temp_std_p1,
					-(stats.a_temp_std * temperature + stats.b_temp_std));
			const float temp_std = std::max(
					20.0f, normalized * (stats.temp_std_p99 - baseline) + baseline +
								   stats.a_temp_std * temperature + stats.b_temp_std);
			const float precip_std =
					raw[4] * std::max(0.0f, (185.0f - 0.04111f * precipitation) / 185.0f);
			return {std::copysign(std::sqrt(std::fabs(elevation)), elevation),
					temperature, temp_std, precipitation, precip_std};
		}

		const float continent = worldNoise(coarse_x, coarse_z, 1.0f / 150.0f, 11, 5);
		const float detail = worldNoise(coarse_x, coarse_z, 1.0f / 28.0f, 23, 4);
		const float elevation = continent * 2200.0f + detail * 500.0f - 250.0f;
		const float elevation_sqrt =
				std::copysign(std::sqrt(std::fabs(elevation)), elevation);
		const float temperature =
				18.0f + worldNoise(coarse_x, coarse_z, 1.0f / 210.0f, 37, 3) * 24.0f -
				std::max(elevation, 0.0f) * 0.0065f;
		const float precipitation = std::max(0.0f,
				1350.0f + worldNoise(coarse_x, coarse_z, 1.0f / 95.0f, 51, 4) * 1100.0f);
		const float temp_seasonality = std::max(20.0f,
				330.0f + worldNoise(coarse_x, coarse_z, 1.0f / 120.0f, 63, 3) * 220.0f);
		const float precip_seasonality = std::max(1.0f,
				52.0f + worldNoise(coarse_x, coarse_z, 1.0f / 75.0f, 79, 3) * 35.0f);
		return {elevation_sqrt, temperature, temp_seasonality, precipitation,
				precip_seasonality};
	}

#if USE_ONNXRUNTIME
	std::vector<float> runCoarseRaw(int origin_x, int origin_z)
	{
		const std::array<float, 5> means{coarse_means[0], coarse_means[2],
				coarse_means[3], coarse_means[4], coarse_means[5]};
		const std::array<float, 5> stds{coarse_stds[0], coarse_stds[2], coarse_stds[3],
				coarse_stds[4], coarse_stds[5]};
		const size_t plane = COARSE_SIZE * COARSE_SIZE;
		std::vector<float> synthetic(5 * plane);
		for (int z = 0; z < COARSE_SIZE; ++z)
			for (int x = 0; x < COARSE_SIZE; ++x) {
				const auto raw = conditioning(origin_x + x, origin_z + z);
				for (int c = 0; c < 5; ++c)
					synthetic[c * plane + z * COARSE_SIZE + x] =
							(raw[c] - means[c]) / stds[c];
			}

		std::vector<float> cond_noise = noisePatch(seed, origin_z, origin_x, COARSE_SIZE,
				COARSE_SIZE, 5, COARSE_SIZE, COARSE_SIZE);
		for (int c = 0; c < 5; ++c) {
			const float t = std::atan(cond_snr[c]);
			for (size_t i = 0; i < plane; ++i)
				synthetic[c * plane + i] = std::cos(t) * synthetic[c * plane + i] +
										   std::sin(t) * cond_noise[c * plane + i];
		}

		std::vector<float> sample = noisePatch(seed + 1, origin_z, origin_x, COARSE_SIZE,
				COARSE_SIZE, 6, COARSE_SIZE, COARSE_SIZE);
		std::array<float, 20> sigmas{};
		constexpr float rho = 7.0f;
		const float max_root = std::pow(SIGMA_MAX, 1.0f / rho);
		const float min_root = std::pow(SIGMA_MIN, 1.0f / rho);
		for (size_t i = 0; i < sigmas.size(); ++i) {
			const float ramp = static_cast<float>(i) / (sigmas.size() - 1);
			sigmas[i] = std::pow(max_root + ramp * (min_root - max_root), rho);
		}
		for (float &value : sample)
			value *= sigmas[0];

		std::vector<float> previous_x0;
		for (size_t step = 0; step < sigmas.size(); ++step) {
			const float sigma = sigmas[step];
			const float next_sigma = step + 1 < sigmas.size() ? sigmas[step + 1] : 0.0f;
			const float input_scale =
					1.0f / std::sqrt(sigma * sigma + SIGMA_DATA * SIGMA_DATA);
			std::vector<float> model_input(11 * plane);
			for (size_t i = 0; i < 6 * plane; ++i)
				model_input[i] = sample[i] * input_scale;
			std::copy(
					synthetic.begin(), synthetic.end(), model_input.begin() + 6 * plane);

			std::vector<std::vector<float>> inputs;
			inputs.push_back(std::move(model_input));
			inputs.push_back({std::atan(sigma / SIGMA_DATA)});
			for (float snr : cond_snr)
				inputs.push_back({std::log(snr / 8.0f)});
			std::vector<std::vector<int64_t>> shapes{
					{1, 11, COARSE_SIZE, COARSE_SIZE}, {1}};
			for (int i = 0; i < 5; ++i)
				shapes.push_back({1});
			std::vector<float> model_output;
			if (!models->coarse.run(inputs, shapes, model_output) ||
					model_output.size() != sample.size())
				throw std::runtime_error("coarse model returned an unexpected tensor");

			const float c_skip =
					SIGMA_DATA * SIGMA_DATA / (sigma * sigma + SIGMA_DATA * SIGMA_DATA);
			const float c_out = sigma * SIGMA_DATA /
								std::sqrt(sigma * sigma + SIGMA_DATA * SIGMA_DATA);
			std::vector<float> x0(sample.size());
			for (size_t i = 0; i < sample.size(); ++i)
				x0[i] = c_skip * sample[i] + c_out * model_output[i];

			if (next_sigma == 0.0f || step == 0 || previous_x0.empty()) {
				if (next_sigma == 0.0f) {
					sample = x0;
				} else {
					const float h = std::log(sigma / next_sigma);
					const float e = std::exp(-h);
					for (size_t i = 0; i < sample.size(); ++i)
						sample[i] = (next_sigma / sigma) * sample[i] - (e - 1.0f) * x0[i];
				}
			} else {
				const float previous_sigma = sigmas[step - 1];
				const float h = std::log(sigma / next_sigma);
				const float h0 = std::log(previous_sigma / sigma);
				const float r0 = h0 / h;
				const float e = std::exp(-h);
				for (size_t i = 0; i < sample.size(); ++i) {
					const float d1 = (x0[i] - previous_x0[i]) / r0;
					sample[i] = (next_sigma / sigma) * sample[i] - (e - 1.0f) * x0[i] -
								0.5f * (e - 1.0f) * d1;
				}
			}
			previous_x0 = std::move(x0);
		}

		for (int c = 0; c < 6; ++c)
			for (size_t i = 0; i < plane; ++i)
				sample[c * plane + i] =
						sample[c * plane + i] / SIGMA_DATA * coarse_stds[c] +
						coarse_means[c];
		for (size_t i = 0; i < plane; ++i)
			sample[plane + i] = sample[i] - sample[plane + i];
		return sample;
	}

	const std::vector<float> &getCoarseTile(const TileKey &key)
	{
		auto found = coarse_cache.find(key);
		if (found != coarse_cache.end()) {
			found->second.last_use = ++cache_clock;
			return found->second.data;
		}
		FloatCacheEntry entry;
		entry.data = runCoarseRaw(key.x * 48, key.z * 48);
		entry.last_use = ++cache_clock;
		auto inserted = coarse_cache.emplace(key, std::move(entry));
		trimCache(coarse_cache, cache_bytes_limit / 5, key,
				[](const FloatCacheEntry &item) {
					return item.data.size() * sizeof(float);
				});
		return inserted.first->second.data;
	}

	std::vector<float> sampleCoarseRegion(
			int origin_x, int origin_z, int width, int height)
	{
		constexpr int channels = 6;
		constexpr int stride = 48;
		std::vector<float> output(static_cast<size_t>(channels) * height * width);
		for (int z = 0; z < height; ++z)
			for (int x = 0; x < width; ++x) {
				const int world_x = origin_x + x;
				const int world_z = origin_z + z;
				const int center_x = floorDiv(world_x, stride);
				const int center_z = floorDiv(world_z, stride);
				std::array<float, channels> sums{};
				float weight_sum = 0.0f;
				for (int tz = center_z - 1; tz <= center_z + 1; ++tz)
					for (int tx = center_x - 1; tx <= center_x + 1; ++tx) {
						const int local_x = world_x - tx * stride;
						const int local_z = world_z - tz * stride;
						if (local_x < 0 || local_z < 0 || local_x >= COARSE_SIZE ||
								local_z >= COARSE_SIZE)
							continue;
						const auto &tile = getCoarseTile({tx, tz});
						const float weight = linearWeight(local_x, COARSE_SIZE) *
											 linearWeight(local_z, COARSE_SIZE);
						weight_sum += weight;
						for (int c = 0; c < channels; ++c)
							sums[c] += tile[(static_cast<size_t>(c) * COARSE_SIZE +
													local_z) *
													   COARSE_SIZE +
											   local_x] *
									   weight;
					}
				if (weight_sum <= 0.0f)
					throw std::runtime_error("coarse overlap has no contributing tile");
				for (int c = 0; c < channels; ++c)
					output[(static_cast<size_t>(c) * height + z) * width + x] =
							sums[c] / weight_sum;
			}
		return output;
	}

	std::vector<float> runBaseRaw(const std::vector<float> &coarse_map,
			int coarse_origin_x, int coarse_origin_z, int base_origin_x,
			int base_origin_z)
	{
		static const std::array<float, 7> means{
				14.99f, 11.65f, 15.87f, 619.26f, 833.12f, 69.40f, 0.66f};
		static const std::array<float, 7> stds{
				21.72f, 21.78f, 10.40f, 452.29f, 738.09f, 34.59f, 0.47f};
		const int coarse_x = floorDiv(base_origin_x, 32) - 1 - coarse_origin_x;
		const int coarse_z = floorDiv(base_origin_z, 32) - 1 - coarse_origin_z;
		std::array<std::vector<float>, 6> groups;
		groups[0].resize(16);
		groups[1].resize(16);
		groups[2].resize(4);
		groups[3].resize(16, (1.0f - means[6]) / stds[6]);
		groups[4].resize(5, 0.0f);
		groups[5].resize(1, -std::sqrt(3.0f));
		for (int z = 0; z < 4; ++z)
			for (int x = 0; x < 4; ++x) {
				const int sx = rangelim(coarse_x + x, 0, COARSE_SIZE - 1);
				const int sz = rangelim(coarse_z + z, 0, COARSE_SIZE - 1);
				const size_t p = sz * COARSE_SIZE + sx;
				groups[0][z * 4 + x] = (coarse_map[p] - means[0]) / stds[0];
				groups[1][z * 4 + x] =
						(coarse_map[COARSE_SIZE * COARSE_SIZE + p] - means[1]) / stds[1];
			}
		for (int c = 0; c < 4; ++c) {
			float value = 0.0f;
			for (int z = 1; z <= 2; ++z)
				for (int x = 1; x <= 2; ++x) {
					const int sx = rangelim(coarse_x + x, 0, COARSE_SIZE - 1);
					const int sz = rangelim(coarse_z + z, 0, COARSE_SIZE - 1);
					value += coarse_map[((c + 2) * COARSE_SIZE + sz) * COARSE_SIZE + sx];
				}
			groups[2][c] = (value * 0.25f - means[c + 2]) / stds[c + 2];
		}

		std::vector<float> condition;
		condition.reserve(58);
		for (const auto &group : groups) {
			const float factor =
					std::sqrt(58.0f / (6.0f * static_cast<float>(group.size())));
			for (float value : group)
				condition.push_back(value * factor);
		}

		auto infer_step = [&](const std::vector<float> *previous, float t,
								  uint64_t noise_seed) {
			std::vector<float> noise = noisePatch(noise_seed, base_origin_z,
					base_origin_x, BASE_SIZE, BASE_SIZE, 5, BASE_SIZE, BASE_SIZE);
			std::vector<float> model_input(noise.size());
			std::vector<float> x_t(noise.size());
			for (size_t i = 0; i < noise.size(); ++i) {
				const float sample = previous ? (*previous)[i] * SIGMA_DATA : 0.0f;
				const float z = noise[i] * SIGMA_DATA;
				x_t[i] = std::cos(t) * sample + std::sin(t) * z;
				model_input[i] = x_t[i] / SIGMA_DATA;
			}
			std::vector<float> output;
			if (!models->base.run({model_input, {t}, condition},
						{{1, 5, BASE_SIZE, BASE_SIZE}, {1}, {1, 58}}, output) ||
					output.size() != noise.size())
				throw std::runtime_error("base model returned an unexpected tensor");
			for (size_t i = 0; i < output.size(); ++i)
				output[i] =
						(std::cos(t) * x_t[i] + std::sin(t) * SIGMA_DATA * output[i]) /
						SIGMA_DATA;
			return output;
		};

		const float initial_t = std::atan(SIGMA_MAX / SIGMA_DATA);
		std::vector<float> output = infer_step(nullptr, initial_t, seed + 5819);
		const float refinement_t = std::atan(0.35f / SIGMA_DATA);
		return infer_step(&output, refinement_t, seed + 5820);
	}

	const std::vector<float> &getBaseTile(const TileKey &key)
	{
		auto found = base_cache.find(key);
		if (found != base_cache.end()) {
			found->second.last_use = ++cache_clock;
			return found->second.data;
		}
		const int base_origin_x = key.x * 32;
		const int base_origin_z = key.z * 32;
		const int coarse_origin_x = floorDiv(base_origin_x, 32) - 25;
		const int coarse_origin_z = floorDiv(base_origin_z, 32) - 25;
		std::vector<float> coarse_map = sampleCoarseRegion(
				coarse_origin_x, coarse_origin_z, COARSE_SIZE, COARSE_SIZE);
		FloatCacheEntry entry;
		entry.data = runBaseRaw(coarse_map, coarse_origin_x, coarse_origin_z,
				base_origin_x, base_origin_z);
		entry.last_use = ++cache_clock;
		auto inserted = base_cache.emplace(key, std::move(entry));
		trimCache(base_cache, cache_bytes_limit * 3 / 10, key,
				[](const FloatCacheEntry &item) {
					return item.data.size() * sizeof(float);
				});
		return inserted.first->second.data;
	}

	std::vector<float> sampleBaseRegion(int origin_x, int origin_z, int width, int height)
	{
		constexpr int channels = 5;
		constexpr int stride = 32;
		std::vector<float> output(static_cast<size_t>(channels) * height * width);
		for (int z = 0; z < height; ++z)
			for (int x = 0; x < width; ++x) {
				const int world_x = origin_x + x;
				const int world_z = origin_z + z;
				const int center_x = floorDiv(world_x, stride);
				const int center_z = floorDiv(world_z, stride);
				std::array<float, channels> sums{};
				float weight_sum = 0.0f;
				for (int tz = center_z - 1; tz <= center_z + 1; ++tz)
					for (int tx = center_x - 1; tx <= center_x + 1; ++tx) {
						const int local_x = world_x - tx * stride;
						const int local_z = world_z - tz * stride;
						if (local_x < 0 || local_z < 0 || local_x >= BASE_SIZE ||
								local_z >= BASE_SIZE)
							continue;
						const auto &tile = getBaseTile({tx, tz});
						const float weight = linearWeight(local_x, BASE_SIZE) *
											 linearWeight(local_z, BASE_SIZE);
						weight_sum += weight;
						for (int c = 0; c < channels; ++c)
							sums[c] +=
									tile[(static_cast<size_t>(c) * BASE_SIZE + local_z) *
													BASE_SIZE +
											local_x] *
									weight;
					}
				if (weight_sum <= 0.0f)
					throw std::runtime_error("base overlap has no contributing tile");
				for (int c = 0; c < channels; ++c)
					output[(static_cast<size_t>(c) * height + z) * width + x] =
							sums[c] / weight_sum;
			}
		return output;
	}

	Tile generateTile(const TileKey &key)
	{
		const int margin = (decoder_size - usable_size) / 2;
		const int decoder_origin_x = key.x * usable_size - margin;
		const int decoder_origin_z = key.z * usable_size - margin;
		const int latent_origin_x = floorDiv(decoder_origin_x, LATENT_COMPRESSION) - 1;
		const int latent_origin_z = floorDiv(decoder_origin_z, LATENT_COMPRESSION) - 1;
		const int latent_end_x =
				floorDiv(decoder_origin_x + decoder_size - 1, LATENT_COMPRESSION) + 2;
		const int latent_end_z =
				floorDiv(decoder_origin_z + decoder_size - 1, LATENT_COMPRESSION) + 2;
		const int latent_width = latent_end_x - latent_origin_x + 1;
		const int latent_height = latent_end_z - latent_origin_z + 1;
		const int coarse_origin_x = floorDiv(decoder_origin_x, 32) - 24;
		const int coarse_origin_z = floorDiv(decoder_origin_z, 32) - 24;

		std::vector<float> coarse_map = sampleCoarseRegion(
				coarse_origin_x, coarse_origin_z, COARSE_SIZE, COARSE_SIZE);
		std::vector<float> latents = sampleBaseRegion(
				latent_origin_x, latent_origin_z, latent_width, latent_height);
		const size_t decoder_plane = static_cast<size_t>(decoder_size) * decoder_size;
		std::vector<float> decoder_input(5 * decoder_plane);
		for (int z = 0; z < decoder_size; ++z)
			for (int x = 0; x < decoder_size; ++x) {
				const int global_lx = floorDiv(decoder_origin_x + x, LATENT_COMPRESSION);
				const int global_lz = floorDiv(decoder_origin_z + z, LATENT_COMPRESSION);
				const int lx = rangelim(global_lx - latent_origin_x, 0, latent_width - 1);
				const int lz =
						rangelim(global_lz - latent_origin_z, 0, latent_height - 1);
				for (int c = 0; c < 4; ++c)
					decoder_input[(static_cast<size_t>(c + 1) * decoder_size + z) *
										  decoder_size +
								  x] =
							latents[(static_cast<size_t>(c) * latent_height + lz) *
											latent_width +
									lx];
			}

		const float t = std::atan(SIGMA_MAX / SIGMA_DATA);
		std::vector<float> residual_noise =
				noisePatch(seed + 5819, decoder_origin_z, decoder_origin_x, decoder_size,
						decoder_size, 1, decoder_size, decoder_size);
		for (size_t i = 0; i < decoder_plane; ++i)
			decoder_input[i] = std::sin(t) * residual_noise[i];
		std::vector<float> residual;
		if (!models->decoder.run({decoder_input, {t}},
					{{1, 5, decoder_size, decoder_size}, {1}}, residual) ||
				residual.size() != decoder_plane)
			throw std::runtime_error("decoder model returned an unexpected tensor");

		for (size_t i = 0; i < residual.size(); ++i) {
			const float x_t = std::sin(t) * residual_noise[i] * SIGMA_DATA;
			residual[i] = (std::cos(t) * x_t + std::sin(t) * SIGMA_DATA * residual[i]) /
						  SIGMA_DATA;
		}

		std::vector<float> lowfreq(decoder_plane);
		std::vector<float> decoded_sqrt(decoder_plane);
		for (int dz = 0; dz < decoder_size; ++dz)
			for (int dx = 0; dx < decoder_size; ++dx) {
				const float latent_x =
						static_cast<float>(decoder_origin_x + dx -
										   latent_origin_x * LATENT_COMPRESSION) /
						LATENT_COMPRESSION;
				const float latent_z =
						static_cast<float>(decoder_origin_z + dz -
										   latent_origin_z * LATENT_COMPRESSION) /
						LATENT_COMPRESSION;
				const size_t index = static_cast<size_t>(dz) * decoder_size + dx;
				lowfreq[index] = bilinear(latents, 5, latent_height, latent_width, 4,
										 latent_z, latent_x) *
										 LOWFREQ_STD +
								 LOWFREQ_MEAN;
				decoded_sqrt[index] = residual[index] * residual_std + lowfreq[index];
			}

		// Match laplacian_denoise(): derive a fresh low-frequency component
		// from the decoded terrain, blur it at latent resolution, then decode.
		const int low_size = decoder_size / LATENT_COMPRESSION;
		std::vector<float> denoised_low = resizeBilinear(
				decoded_sqrt, decoder_size, decoder_size, low_size, low_size);
		gaussianBlur(denoised_low, low_size, low_size, 5.0f);
		denoised_low = resizeBilinear(
				denoised_low, low_size, low_size, decoder_size, decoder_size);
		const ClimateFields climate = calculateClimateFields(coarse_map);

		Tile tile;
		tile.samples.resize(static_cast<size_t>(decoder_size) * decoder_size);
		for (int dz = 0; dz < decoder_size; ++dz)
			for (int dx = 0; dx < decoder_size; ++dx) {
				const size_t index = static_cast<size_t>(dz) * decoder_size + dx;
				const float elevation_sqrt =
						residual[index] * residual_std + denoised_low[index];
				const float raw_height =
						std::copysign(elevation_sqrt * elevation_sqrt, elevation_sqrt);
				TerrainDiffusionSample &sample = tile.samples[dz * decoder_size + dx];
				sample.height = raw_height * height_scale + height_offset;

				const float coarse_world_x =
						static_cast<float>(decoder_origin_x + dx) / 32.0f -
						coarse_origin_x;
				const float coarse_world_z =
						static_cast<float>(decoder_origin_z + dz) / 32.0f -
						coarse_origin_z;
				const float baseline = bilinear(climate.baseline, 1, COARSE_SIZE,
						COARSE_SIZE, 0, coarse_world_z, coarse_world_x);
				const float beta = bilinear(climate.beta, 1, COARSE_SIZE, COARSE_SIZE, 0,
						coarse_world_z, coarse_world_x);
				const float heat = baseline + beta * std::max(raw_height, 0.0f);
				const float precipitation = bilinear(coarse_map, 6, COARSE_SIZE,
						COARSE_SIZE, 4, coarse_world_z, coarse_world_x);
				sample.heat = rangelim(std::lround(heat), -273L, 2000L);
				sample.humidity = MapgenTerrainDiffusion::precipToHumidity(precipitation);
				sample.has_climate = true;
				if (!std::isfinite(sample.height) || !std::isfinite(heat) ||
						!std::isfinite(precipitation))
					throw std::runtime_error(
							"pipeline produced a non-finite terrain sample");
			}
		return tile;
	}
#endif

	const Tile *getTile(const TileKey &key, bool generate_missing = true)
	{
		auto found = cache.find(key);
		if (found != cache.end()) {
			found->second.last_use = ++cache_clock;
			return &found->second.tile;
		}
		if (!generate_missing)
			return nullptr;
#if USE_ONNXRUNTIME
		try {
			TileCacheEntry entry;
			entry.tile = generateTile(key);
			entry.last_use = ++cache_clock;
			auto inserted = cache.emplace(key, std::move(entry));
			trimCache(
					cache, cache_bytes_limit / 2, key,
					[](const TileCacheEntry &item) {
						return item.tile.samples.size() * sizeof(TerrainDiffusionSample);
					},
					cache_limit);
			return &inserted.first->second.tile;
		} catch (const std::exception &e) {
			errorstream << "TerrainDiffusion native inference failed: " << e.what()
						<< std::endl;
			return nullptr;
		}
#else
		return nullptr;
#endif
	}

	bool samplePixel(int pixel_x, int pixel_z, TerrainDiffusionSample &result,
			bool generate_missing = true)
	{
		const int center_x = floorDiv(pixel_x, usable_size);
		const int center_z = floorDiv(pixel_z, usable_size);
		float weight_sum = 0.0f;
		float height_sum = 0.0f;
		float heat_sum = 0.0f;
		float humidity_sum = 0.0f;
		for (int tz = center_z - 1; tz <= center_z + 1; ++tz)
			for (int tx = center_x - 1; tx <= center_x + 1; ++tx) {
				const int origin_x = tx * usable_size - (decoder_size - usable_size) / 2;
				const int origin_z = tz * usable_size - (decoder_size - usable_size) / 2;
				const int local_x = pixel_x - origin_x;
				const int local_z = pixel_z - origin_z;
				if (local_x < 0 || local_z < 0 || local_x >= decoder_size ||
						local_z >= decoder_size)
					continue;
				const Tile *tile = getTile({tx, tz}, generate_missing);
				if (!tile)
					return false;
				const TerrainDiffusionSample &sample =
						tile->samples[static_cast<size_t>(local_z) * decoder_size +
									  local_x];
				const float mid = (decoder_size - 1) * 0.5f;
				const float wx = 1.0f - 0.999f * rangelim(std::fabs(local_x - mid) / mid,
														 0.0f, 1.0f);
				const float wz = 1.0f - 0.999f * rangelim(std::fabs(local_z - mid) / mid,
														 0.0f, 1.0f);
				const float weight = wx * wz;
				weight_sum += weight;
				height_sum += sample.height * weight;
				heat_sum += sample.heat * weight;
				humidity_sum += sample.humidity * weight;
			}
		if (weight_sum <= 0.0f)
			return false;
		result.height = height_sum / weight_sum;
		result.heat = rangelim(std::lround(heat_sum / weight_sum), -273L, 2000L);
		result.humidity = rangelim(std::lround(humidity_sum / weight_sum), 0L, 100L);
		result.has_climate = true;
		return true;
	}

	bool sampleGridLocked(int min_x, int min_z, int max_x, int max_z,
			std::vector<TerrainDiffusionSample> &samples, bool generate_missing)
	{
		const int width = max_x - min_x + 1;
		const int depth = max_z - min_z + 1;
		samples.resize(static_cast<size_t>(width) * depth);
		for (int z = min_z; z <= max_z; ++z)
			for (int x = min_x; x <= max_x; ++x) {
				const float model_x = static_cast<float>(x) / node_scale;
				const float model_z = static_cast<float>(z) / node_scale;
				const int x0 = static_cast<int>(std::floor(model_x));
				const int z0 = static_cast<int>(std::floor(model_z));
				const float fx = model_x - x0;
				const float fz = model_z - z0;
				std::array<TerrainDiffusionSample, 4> corners;
				if (!samplePixel(x0, z0, corners[0], generate_missing) ||
						!samplePixel(x0 + 1, z0, corners[1], generate_missing) ||
						!samplePixel(x0, z0 + 1, corners[2], generate_missing) ||
						!samplePixel(x0 + 1, z0 + 1, corners[3], generate_missing))
					return false;
				TerrainDiffusionSample &sample =
						samples[static_cast<size_t>(z - min_z) * width + x - min_x];
				auto interpolate = [&](auto member) {
					const float top =
							corners[0].*member * (1.0f - fx) + corners[1].*member * fx;
					const float bottom =
							corners[2].*member * (1.0f - fx) + corners[3].*member * fx;
					return top * (1.0f - fz) + bottom * fz;
				};
				sample.height = interpolate(&TerrainDiffusionSample::height);
				sample.heat =
						rangelim(std::lround(interpolate(&TerrainDiffusionSample::heat)),
								-273L, 2000L);
				sample.humidity = rangelim(
						std::lround(interpolate(&TerrainDiffusionSample::humidity)), 0L,
						100L);
				sample.has_climate = true;
			}
		return true;
	}

	void schedulePrefetch(
			int min_pixel_x, int min_pixel_z, int max_pixel_x, int max_pixel_z)
	{
		if (!prefetch_pool)
			return;
		const int min_tx = floorDiv(min_pixel_x, usable_size) - 1;
		const int max_tx = floorDiv(max_pixel_x, usable_size) + 1;
		const int min_tz = floorDiv(min_pixel_z, usable_size) - 1;
		const int max_tz = floorDiv(max_pixel_z, usable_size) + 1;
		for (int tz = min_tz; tz <= max_tz; ++tz)
			for (int tx = min_tx; tx <= max_tx; ++tx) {
				if (tx > min_tx && tx < max_tx && tz > min_tz && tz < max_tz)
					continue;
				const TileKey key{tx, tz};
				{
					std::lock_guard<std::mutex> lock(cache_mutex);
					if (cache.find(key) != cache.end() ||
							pending_prefetch.find(key) != pending_prefetch.end())
						continue;
					pending_prefetch.insert(key);
				}
				try {
					prefetch_pool->enqueue([this, key]() {
						std::lock_guard<std::mutex> lock(cache_mutex);
						getTile(key);
						pending_prefetch.erase(key);
					});
				} catch (const progschj::would_block &) {
					std::lock_guard<std::mutex> lock(cache_mutex);
					pending_prefetch.erase(key);
				}
			}
	}
};

TerrainDiffusionNativePipeline::TerrainDiffusionNativePipeline(uint64_t seed,
		int node_scale, float height_scale, float height_offset, float residual_std,
		unsigned int cache_tiles, unsigned int cache_mb, const std::string &provider,
		int device_id, int intra_threads, const std::string &conditioning_stats,
		bool prefetch) :
		m_impl(std::make_unique<Impl>(seed, node_scale, height_scale, height_offset,
				residual_std, cache_tiles, cache_mb, provider, device_id, intra_threads,
				conditioning_stats, prefetch))
{
}

TerrainDiffusionNativePipeline::~TerrainDiffusionNativePipeline() = default;

bool TerrainDiffusionNativePipeline::runDeterminismSelfTest(std::string &error)
{
	uint64_t state = 1;
	const std::array<uint32_t, 4> expected{
			3114030964U, 3308539156U, 2446277621U, 2609120922U};
	for (uint32_t value : expected) {
		if (pcgNext(state) != value) {
			error = "portable PCG stream differs from the Python reference";
			return false;
		}
	}

	constexpr int width = 13;
	constexpr int height = 17;
	constexpr int split = 7;
	constexpr int channels = 2;
	const auto full = noisePatch(123456789, -37, 19, height, width, channels, 8, 8);
	const auto top = noisePatch(123456789, -37, 19, split, width, channels, 8, 8);
	const auto bottom =
			noisePatch(123456789, -37 + split, 19, height - split, width, channels, 8, 8);
	for (int c = 0; c < channels; ++c)
		for (int y = 0; y < height; ++y)
			for (int x = 0; x < width; ++x) {
				const float expected_value =
						y < split ? top[(static_cast<size_t>(c) * split + y) * width + x]
								  : bottom[(static_cast<size_t>(c) * (height - split) +
												   y - split) *
													width +
											x];
				if (full[(static_cast<size_t>(c) * height + y) * width + x] !=
						expected_value) {
					error = "world-space Gaussian patches do not stitch exactly";
					return false;
				}
			}

	std::vector<float> constant(9, 7.25f);
	const auto resized = resizeBilinear(constant, 3, 3, 11, 7);
	for (float value : resized) {
		if (std::fabs(value - 7.25f) > 1e-6f) {
			error = "bilinear resizing does not preserve a constant field";
			return false;
		}
	}
	gaussianBlur(constant, 3, 3, 5.0f);
	for (float value : constant) {
		if (std::fabs(value - 7.25f) > 1e-5f) {
			error = "Gaussian filtering does not preserve a constant field";
			return false;
		}
	}
	return true;
}

bool TerrainDiffusionNativePipeline::load(const std::string &model_dir)
{
	if (model_dir.empty())
		return false;
#if USE_ONNXRUNTIME
	try {
		m_impl->loadPipelineConfig(model_dir);
		m_impl->loadConditioningStats();
		m_impl->models = acquireSharedModels(
				model_dir, m_impl->provider, m_impl->device_id, m_impl->intra_threads);
		const auto &coarse_shape = m_impl->models->coarse.inputShape(0);
		const auto &base_shape = m_impl->models->base.inputShape(0);
		const auto &shape = m_impl->models->decoder.inputShape(0);
		if (m_impl->models->coarse.inputCount() != 7 || coarse_shape.size() != 4 ||
				coarse_shape[1] != 11 || coarse_shape[2] != COARSE_SIZE ||
				coarse_shape[3] != COARSE_SIZE)
			throw std::runtime_error(
					"coarse model must accept x [N,11,64,64], noise_labels, and five conditions");
		if (m_impl->models->base.inputCount() != 3 || base_shape.size() != 4 ||
				base_shape[1] != 5 || base_shape[2] != BASE_SIZE ||
				base_shape[3] != BASE_SIZE)
			throw std::runtime_error(
					"base model must accept x [N,5,64,64], noise_labels, and cond_0 [N,58]");
		if (m_impl->models->decoder.inputCount() != 2)
			throw std::runtime_error("decoder model must accept x and noise_labels");
		if (shape.size() != 4 || shape[1] != 5 || shape[2] <= 0 || shape[2] != shape[3] ||
				shape[2] > 512 || shape[2] % LATENT_COMPRESSION != 0)
			throw std::runtime_error(
					"decoder x input must be static [N,5,S,S], S <= 512 and divisible by 8");
		m_impl->decoder_size = static_cast<int>(shape[2]);
		m_impl->usable_size = std::max(8, m_impl->decoder_size / 2);
		m_impl->is_loaded = true;
		infostream << "TerrainDiffusion mapgen loaded native three-model pipeline "
				   << model_dir << " decoder_size=" << m_impl->decoder_size
				   << " provider=" << m_impl->models->provider << std::endl;
		return true;
	} catch (const std::exception &e) {
		errorstream << "TerrainDiffusion mapgen failed to load native pipeline "
					<< model_dir << ": " << e.what() << std::endl;
		m_impl->is_loaded = false;
		return false;
	}
#else
	warningstream << "TerrainDiffusion native models configured but Freeminer "
				  << "was built without ONNX Runtime: " << model_dir << std::endl;
	return false;
#endif
}

bool TerrainDiffusionNativePipeline::loaded() const
{
	return m_impl->is_loaded;
}

bool TerrainDiffusionNativePipeline::sampleGrid(int min_x, int min_z, int max_x,
		int max_z, std::vector<TerrainDiffusionSample> &samples)
{
	if (!loaded() || min_x > max_x || min_z > max_z)
		return false;
	std::unique_lock<std::mutex> lock(m_impl->cache_mutex);
	if (!m_impl->sampleGridLocked(min_x, min_z, max_x, max_z, samples, true))
		return false;
	lock.unlock();
	m_impl->schedulePrefetch(floorDiv(min_x, m_impl->node_scale),
			floorDiv(min_z, m_impl->node_scale), floorDiv(max_x, m_impl->node_scale),
			floorDiv(max_z, m_impl->node_scale));
	return true;
}

bool TerrainDiffusionNativePipeline::sampleGridCached(int min_x, int min_z, int max_x,
		int max_z, std::vector<TerrainDiffusionSample> &samples)
{
	if (!loaded() || min_x > max_x || min_z > max_z)
		return false;
	std::unique_lock<std::mutex> lock(m_impl->cache_mutex, std::try_to_lock);
	if (!lock.owns_lock())
		return false;
	return m_impl->sampleGridLocked(min_x, min_z, max_x, max_z, samples, false);
}
