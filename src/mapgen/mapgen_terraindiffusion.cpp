/*
This file is part of Freeminer.

Freeminer is free software: you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation, either version 3 of the License, or
(at your option) any later version.

Freeminer is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with Freeminer. If not, see <http://www.gnu.org/licenses/>.
*/

#include "config.h"
#include "mapgen_terraindiffusion.h"
#include "mapgen_terraindiffusion_native.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <cstring>
#include <memory>
#include <sstream>
#include <stdexcept>
#include <vector>

#if USE_ONNXRUNTIME
#include <onnxruntime_cxx_api.h>
#endif

#include "emerge.h"
#include "httpfetch.h"
#include "log.h"
#include "mapnode.h"
#include "noise.h"
#include "nodedef.h"
#include "serverenvironment.h"
#include "servermap.h"
#include "settings.h"
#include "voxel.h"

namespace
{
constexpr float TD_DEFAULT_HEAT = 20.0f;
constexpr float TD_DEFAULT_HUMIDITY = 50.0f;

u16 readU16LE(const std::string &data, size_t offset)
{
	const auto *bytes = reinterpret_cast<const unsigned char *>(data.data() + offset);
	return (u16)bytes[0] | ((u16)bytes[1] << 8);
}

s16 readS16LE(const std::string &data, size_t offset)
{
	return static_cast<s16>(readU16LE(data, offset));
}

float readFloatLE(const std::string &data, size_t offset)
{
	const auto *bytes = reinterpret_cast<const unsigned char *>(data.data() + offset);
	const u32 bits = (u32)bytes[0] | ((u32)bytes[1] << 8) | ((u32)bytes[2] << 16) |
					 ((u32)bytes[3] << 24);
	float value;
	static_assert(sizeof(value) == sizeof(bits), "float must be 32-bit");
	std::memcpy(&value, &bits, sizeof(value));
	return value;
}

std::string addTerrainApiParams(const std::string &base_url, pos_t min_x, pos_t min_z,
		pos_t max_x, pos_t max_z, s16 scale, s32 seed, bool send_seed)
{
	std::ostringstream os;
	os << base_url << (base_url.find('?') == std::string::npos ? '?' : '&')
	   << "i1=" << min_z << "&j1=" << min_x << "&i2=" << (max_z + 1)
	   << "&j2=" << (max_x + 1) << "&scale=" << scale;
	if (send_seed)
		os << "&seed=" << seed;
	return os.str();
}
}

class TerrainDiffusionOnnxModel
{
public:
	bool load(const std::string &path);
	bool loaded() const { return m_loaded; }
	bool sampleGrid(float node_scale, float height_scale, float height_offset,
			pos_t min_x, pos_t min_z, pos_t max_x, pos_t max_z,
			std::vector<TerrainDiffusionSample> &samples);

private:
	bool m_loaded = false;

#if USE_ONNXRUNTIME
	std::unique_ptr<Ort::Env> m_env;
	std::unique_ptr<Ort::SessionOptions> m_session_options;
	std::unique_ptr<Ort::Session> m_session;
	std::string m_input_name;
	std::vector<std::string> m_output_names;
#endif
};

bool TerrainDiffusionOnnxModel::load(const std::string &path)
{
	if (path.empty())
		return false;

#if USE_ONNXRUNTIME
	try {
		m_env = std::make_unique<Ort::Env>(
				ORT_LOGGING_LEVEL_WARNING, "freeminer-terraindiffusion");
		m_session_options = std::make_unique<Ort::SessionOptions>();
		m_session_options->SetIntraOpNumThreads(1);
		m_session_options->SetGraphOptimizationLevel(
				GraphOptimizationLevel::ORT_ENABLE_BASIC);
		m_session =
				std::make_unique<Ort::Session>(*m_env, path.c_str(), *m_session_options);

		Ort::AllocatorWithDefaultOptions allocator;
		auto input_name = m_session->GetInputNameAllocated(0, allocator);
		m_input_name = input_name.get();

		const size_t output_count = m_session->GetOutputCount();
		m_output_names.clear();
		m_output_names.reserve(output_count);
		for (size_t i = 0; i < output_count; ++i) {
			auto output_name = m_session->GetOutputNameAllocated(i, allocator);
			m_output_names.emplace_back(output_name.get());
		}

		if (m_output_names.empty())
			throw std::runtime_error("ONNX model has no outputs");

		m_loaded = true;
		infostream << "TerrainDiffusion mapgen loaded ONNX model " << path << std::endl;
		return true;
	} catch (const std::exception &e) {
		errorstream << "TerrainDiffusion mapgen failed to load ONNX model " << path
					<< ": " << e.what() << std::endl;
		m_loaded = false;
		return false;
	}
#else
	warningstream << "TerrainDiffusion mapgen ONNX model configured but "
				  << "Freeminer was built without ONNX Runtime: " << path << std::endl;
	return false;
#endif
}

bool TerrainDiffusionOnnxModel::sampleGrid(float node_scale, float height_scale,
		float height_offset, pos_t min_x, pos_t min_z, pos_t max_x, pos_t max_z,
		std::vector<TerrainDiffusionSample> &samples)
{
	if (!m_loaded)
		return false;

#if USE_ONNXRUNTIME
	try {
		const s32 width = max_x - min_x + 1;
		const s32 depth = max_z - min_z + 1;
		const size_t count = (size_t)width * depth;
		if (samples.size() != count)
			samples.resize(count);

		std::vector<float> coords(count * 2);
		size_t i = 0;
		for (pos_t z = min_z; z <= max_z; ++z) {
			for (pos_t x = min_x; x <= max_x; ++x, ++i) {
				coords[i * 2 + 0] = (float)x * node_scale;
				coords[i * 2 + 1] = (float)z * node_scale;
			}
		}

		std::array<int64_t, 2> input_shape{static_cast<int64_t>(count), 2};
		Ort::MemoryInfo memory_info =
				Ort::MemoryInfo::CreateCpu(OrtArenaAllocator, OrtMemTypeDefault);
		Ort::Value input_tensor = Ort::Value::CreateTensor<float>(memory_info,
				coords.data(), coords.size(), input_shape.data(), input_shape.size());

		std::vector<const char *> input_names{m_input_name.c_str()};
		std::vector<const char *> output_names;
		output_names.reserve(m_output_names.size());
		for (const std::string &name : m_output_names)
			output_names.push_back(name.c_str());

		auto outputs = m_session->Run(Ort::RunOptions{nullptr}, input_names.data(),
				&input_tensor, input_names.size(), output_names.data(),
				output_names.size());

		if (outputs.empty())
			return false;

		const float *height_data = outputs[0].GetTensorData<float>();
		const size_t height_count =
				outputs[0].GetTensorTypeAndShapeInfo().GetElementCount();
		if (height_count < count)
			throw std::runtime_error("height output is smaller than coordinate input");

		const float *climate_data = nullptr;
		size_t climate_channels = 0;
		if (outputs.size() > 1) {
			climate_data = outputs[1].GetTensorData<float>();
			const size_t climate_count =
					outputs[1].GetTensorTypeAndShapeInfo().GetElementCount();
			if (climate_count >= count)
				climate_channels = climate_count / count;
		}

		for (size_t n = 0; n < count; ++n) {
			TerrainDiffusionSample &sample = samples[n];
			sample.height = height_data[n] * height_scale + height_offset;
			sample.heat = TD_DEFAULT_HEAT;
			sample.humidity = TD_DEFAULT_HUMIDITY;
			sample.has_climate = false;

			if (climate_data && climate_channels >= 2) {
				const float *climate = climate_data + n * climate_channels;
				sample.heat = rangelim(std::lround(climate[0]), -273L, 2000L);
				sample.humidity =
						climate_channels >= 3
								? MapgenTerrainDiffusion::precipToHumidity(climate[2])
								: rangelim(std::lround(climate[1]), 0L, 100L);
				sample.has_climate = true;
			}
		}

		return true;
	} catch (const std::exception &e) {
		errorstream << "TerrainDiffusion mapgen ONNX inference failed: " << e.what()
					<< std::endl;
		return false;
	}
#else
	return false;
#endif
}

void MapgenTerrainDiffusionParams::readParams(const Settings *settings)
{
	MapgenV7Params::readParams(settings);
	settings->getFlagStrNoEx("mgterraindiffusion_spflags", spflags, flagdesc_mapgen_v7);
	settings->getNoEx("mgterraindiffusion_height_model", height_model);
	settings->getNoEx("mgterraindiffusion_native_model_dir", native_model_dir);
	settings->getS16NoEx("mgterraindiffusion_native_node_scale", native_node_scale);
	settings->getFloatNoEx("mgterraindiffusion_native_height_scale", native_height_scale);
	settings->getFloatNoEx(
			"mgterraindiffusion_native_height_offset", native_height_offset);
	settings->getFloatNoEx("mgterraindiffusion_native_residual_std", native_residual_std);
	settings->getU16NoEx("mgterraindiffusion_native_cache_tiles", native_cache_tiles);
	settings->getU16NoEx("mgterraindiffusion_native_cache_mb", native_cache_mb);
	settings->getNoEx("mgterraindiffusion_native_provider", native_provider);
	settings->getS16NoEx("mgterraindiffusion_native_device_id", native_device_id);
	settings->getS16NoEx("mgterraindiffusion_native_intra_threads", native_intra_threads);
	settings->getNoEx(
			"mgterraindiffusion_native_conditioning_stats", native_conditioning_stats);
	settings->getBoolNoEx("mgterraindiffusion_native_prefetch", native_prefetch);
	settings->getFloatNoEx("mgterraindiffusion_model_node_scale", model_node_scale);
	settings->getFloatNoEx("mgterraindiffusion_model_height_scale", model_height_scale);
	settings->getFloatNoEx("mgterraindiffusion_model_height_offset", model_height_offset);
	settings->getNoEx("mgterraindiffusion_api_url", api_url);
	settings->getS16NoEx("mgterraindiffusion_api_scale", api_scale);
	settings->getFloatNoEx("mgterraindiffusion_api_height_scale", api_height_scale);
	settings->getFloatNoEx("mgterraindiffusion_api_height_offset", api_height_offset);
	settings->getS32NoEx("mgterraindiffusion_api_timeout_ms", api_timeout_ms);
	settings->getBoolNoEx("mgterraindiffusion_api_send_seed", api_send_seed);
	settings->getFloatNoEx(
			"mgterraindiffusion_fallback_height_scale", fallback_height_scale);
	settings->getFloatNoEx(
			"mgterraindiffusion_fallback_detail_scale", fallback_detail_scale);

	model_node_scale = rangelim(model_node_scale, 0.00001f, 1000000.0f);
	native_node_scale = rangelim(native_node_scale, (s16)1, (s16)64);
	native_height_scale = rangelim(native_height_scale, -1000000.0f, 1000000.0f);
	native_residual_std = rangelim(native_residual_std, 0.01f, 100.0f);
	native_cache_tiles = rangelim(native_cache_tiles, (u16)1, (u16)256);
	native_cache_mb = rangelim(native_cache_mb, (u16)16, (u16)65535);
	native_device_id = rangelim(native_device_id, (s16)0, (s16)255);
	native_intra_threads = rangelim(native_intra_threads, (s16)1, (s16)256);
	model_height_scale = rangelim(model_height_scale, -1000000.0f, 1000000.0f);
	api_scale = rangelim(api_scale, (s16)1, (s16)64);
	api_height_scale = rangelim(api_height_scale, -1000000.0f, 1000000.0f);
	api_timeout_ms = rangelim(api_timeout_ms, 1000, 600000);
	fallback_height_scale = rangelim(fallback_height_scale, 1.0f, 4096.0f);
	fallback_detail_scale = rangelim(fallback_detail_scale, 0.0f, 8.0f);
}

void MapgenTerrainDiffusionParams::writeParams(Settings *settings) const
{
	settings->setFlagStr("mgterraindiffusion_spflags", spflags, flagdesc_mapgen_v7);
	settings->set("mgterraindiffusion_height_model", height_model);
	settings->set("mgterraindiffusion_native_model_dir", native_model_dir);
	settings->setS16("mgterraindiffusion_native_node_scale", native_node_scale);
	settings->setFloat("mgterraindiffusion_native_height_scale", native_height_scale);
	settings->setFloat("mgterraindiffusion_native_height_offset", native_height_offset);
	settings->setFloat("mgterraindiffusion_native_residual_std", native_residual_std);
	settings->setU16("mgterraindiffusion_native_cache_tiles", native_cache_tiles);
	settings->setU16("mgterraindiffusion_native_cache_mb", native_cache_mb);
	settings->set("mgterraindiffusion_native_provider", native_provider);
	settings->setS16("mgterraindiffusion_native_device_id", native_device_id);
	settings->setS16("mgterraindiffusion_native_intra_threads", native_intra_threads);
	settings->set(
			"mgterraindiffusion_native_conditioning_stats", native_conditioning_stats);
	settings->setBool("mgterraindiffusion_native_prefetch", native_prefetch);
	settings->setFloat("mgterraindiffusion_model_node_scale", model_node_scale);
	settings->setFloat("mgterraindiffusion_model_height_scale", model_height_scale);
	settings->setFloat("mgterraindiffusion_model_height_offset", model_height_offset);
	settings->set("mgterraindiffusion_api_url", api_url);
	settings->setS16("mgterraindiffusion_api_scale", api_scale);
	settings->setFloat("mgterraindiffusion_api_height_scale", api_height_scale);
	settings->setFloat("mgterraindiffusion_api_height_offset", api_height_offset);
	settings->setS32("mgterraindiffusion_api_timeout_ms", api_timeout_ms);
	settings->setBool("mgterraindiffusion_api_send_seed", api_send_seed);
	settings->setFloat("mgterraindiffusion_fallback_height_scale", fallback_height_scale);
	settings->setFloat("mgterraindiffusion_fallback_detail_scale", fallback_detail_scale);
	MapgenV7Params::writeParams(settings);
}

void MapgenTerrainDiffusionParams::setDefaultSettings(Settings *settings)
{
	settings->setDefault("mgterraindiffusion_spflags", flagdesc_mapgen_v7, MGV7_CAVERNS);
	settings->setDefault("mgterraindiffusion_height_model", "");
	settings->setDefault("mgterraindiffusion_native_model_dir", "");
	settings->setDefault("mgterraindiffusion_native_node_scale", "30");
	settings->setDefault("mgterraindiffusion_native_height_scale", "1.0");
	settings->setDefault("mgterraindiffusion_native_height_offset", "0.0");
	settings->setDefault("mgterraindiffusion_native_residual_std", "0.7");
	settings->setDefault("mgterraindiffusion_native_cache_tiles", "8");
	settings->setDefault("mgterraindiffusion_native_cache_mb", "128");
	settings->setDefault("mgterraindiffusion_native_provider", "auto");
	settings->setDefault("mgterraindiffusion_native_device_id", "0");
	settings->setDefault("mgterraindiffusion_native_intra_threads", "8");
	settings->setDefault("mgterraindiffusion_native_conditioning_stats", "");
	settings->setDefault("mgterraindiffusion_native_prefetch", "false");
	settings->setDefault("mgterraindiffusion_model_node_scale", "1.0");
	settings->setDefault("mgterraindiffusion_model_height_scale", "1.0");
	settings->setDefault("mgterraindiffusion_model_height_offset", "0.0");
	settings->setDefault("mgterraindiffusion_api_url", "");
	settings->setDefault("mgterraindiffusion_api_scale", "1");
	settings->setDefault("mgterraindiffusion_api_height_scale", "1.0");
	settings->setDefault("mgterraindiffusion_api_height_offset", "0.0");
	settings->setDefault("mgterraindiffusion_api_timeout_ms", "30000");
	settings->setDefault("mgterraindiffusion_api_send_seed", "true");
	settings->setDefault("mgterraindiffusion_fallback_height_scale", "160.0");
	settings->setDefault("mgterraindiffusion_fallback_detail_scale", "1.0");
}

MapgenTerrainDiffusion::MapgenTerrainDiffusion(
		MapgenTerrainDiffusionParams *params, EmergeParams *emerge) :
		MapgenV7((MapgenV7Params *)params, emerge), mg_params(params),
		m_model(std::make_unique<TerrainDiffusionOnnxModel>()),
		m_native(std::make_unique<TerrainDiffusionNativePipeline>(seed,
				mg_params->native_node_scale, mg_params->native_height_scale,
				mg_params->native_height_offset, mg_params->native_residual_std,
				mg_params->native_cache_tiles, mg_params->native_cache_mb,
				mg_params->native_provider, mg_params->native_device_id,
				mg_params->native_intra_threads, mg_params->native_conditioning_stats,
				mg_params->native_prefetch))
{
	if (!mg_params->native_model_dir.empty())
		m_native->load(mg_params->native_model_dir);
	if (!mg_params->height_model.empty())
		m_model->load(mg_params->height_model);
}

MapgenTerrainDiffusion::~MapgenTerrainDiffusion() = default;

float MapgenTerrainDiffusion::clamp01(float v)
{
	return rangelim(v, 0.0f, 1.0f);
}

float MapgenTerrainDiffusion::smoothstep(float v)
{
	v = clamp01(v);
	return v * v * (3.0f - 2.0f * v);
}

weather::humidity_t MapgenTerrainDiffusion::precipToHumidity(float precipitation)
{
	return rangelim(std::lround(precipitation / 20.0f), 0L, 100L);
}

float MapgenTerrainDiffusion::sampleFbm(float x, float z, float scale, s32 seed_off,
		int octaves, float persistence, float lacunarity) const
{
	float amplitude = 1.0f;
	float frequency = scale;
	float total = 0.0f;
	float denom = 0.0f;

	for (int i = 0; i < octaves; ++i) {
		total += noise2d_value(x * frequency, z * frequency, seed + seed_off + i, true) *
				 amplitude;
		denom += amplitude;
		amplitude *= persistence;
		frequency *= lacunarity;
	}

	return denom > 0.0f ? total / denom : 0.0f;
}

float MapgenTerrainDiffusion::sampleRidged(float x, float z, float scale, s32 seed_off,
		int octaves, float persistence, float lacunarity) const
{
	float amplitude = 1.0f;
	float frequency = scale;
	float total = 0.0f;
	float denom = 0.0f;

	for (int i = 0; i < octaves; ++i) {
		float n = noise2d_value(x * frequency, z * frequency, seed + seed_off + i, true);
		float ridge = 1.0f - std::fabs(n);
		total += ridge * ridge * amplitude;
		denom += amplitude;
		amplitude *= persistence;
		frequency *= lacunarity;
	}

	return denom > 0.0f ? total / denom : 0.0f;
}

float MapgenTerrainDiffusion::fallbackHeightAtPoint(pos_t x, pos_t z) const
{
	const float detail = std::max(mg_params->fallback_detail_scale, 0.0f);
	const float continent = smoothstep(
			sampleFbm(x, z, 1.0f / 4800.0f, 100, 5, 0.55f, 2.0f) * 0.5f + 0.5f);
	const float land = smoothstep((continent - 0.38f) / 0.38f);
	const float basin = smoothstep((0.56f - continent) / 0.56f);
	const float macro = sampleFbm(x, z, 1.0f / 1300.0f, 200, 4, 0.55f, 2.0f);
	const float hills = sampleFbm(x, z, 1.0f / 360.0f, 300, 4, 0.5f, 2.0f);
	const float ridges = sampleRidged(x, z, 1.0f / 900.0f, 400, 4, 0.55f, 2.0f);

	const float height_scale = mg_params->fallback_height_scale;
	return water_level - height_scale * 0.58f * basin +
		   height_scale * (continent - 0.45f) +
		   land * (macro * height_scale * 0.45f + hills * height_scale * 0.18f * detail +
						  ridges * height_scale * 0.75f);
}

TerrainDiffusionSample MapgenTerrainDiffusion::fallbackSample(pos_t x, pos_t z) const
{
	TerrainDiffusionSample sample;
	sample.height = fallbackHeightAtPoint(x, z);

	const float lat_noise = sampleFbm(x, z, 1.0f / 12000.0f, 500, 3, 0.5f, 2.0f);
	const float precip_noise = sampleFbm(x, z, 1.0f / 2600.0f, 600, 4, 0.52f, 2.0f);
	const float height_cooling = std::max(sample.height - water_level, 0.0f) / 180.0f;

	sample.heat = rangelim(
			std::lround(TD_DEFAULT_HEAT + lat_noise * 28.0f - height_cooling * 8.0f),
			-50L, 60L);
	sample.humidity =
			rangelim(std::lround(TD_DEFAULT_HUMIDITY + precip_noise * 45.0f), 0L, 100L);
	sample.has_climate = true;
	return sample;
}

bool MapgenTerrainDiffusion::sampleOnnxGrid(
		v3pos_t minp, v3pos_t maxp, std::vector<TerrainDiffusionSample> &samples)
{
	if (sampleApiGrid(minp, maxp, samples))
		return true;

	if (m_native && m_native->loaded() &&
			m_native->sampleGrid(minp.X, minp.Z, maxp.X, maxp.Z, samples))
		return true;

	if (m_model && m_model->loaded()) {
		if (m_model->sampleGrid(mg_params->model_node_scale,
					mg_params->model_height_scale, mg_params->model_height_offset, minp.X,
					minp.Z, maxp.X, maxp.Z, samples))
			return true;
	}

	if (!m_warned_no_model) {
		warningstream << "TerrainDiffusion mapgen using procedural fallback; "
					  << "set mgterraindiffusion_native_model_dir for the native "
					  << "three-model pipeline, or mgterraindiffusion_height_model "
					  << "for a compact model." << std::endl;
		m_warned_no_model = true;
	}
	return false;
}

bool MapgenTerrainDiffusion::sampleApiGrid(
		v3pos_t minp, v3pos_t maxp, std::vector<TerrainDiffusionSample> &samples)
{
	if (mg_params->api_url.empty())
		return false;

#if USE_CURL
	const s32 width = maxp.X - minp.X + 1;
	const s32 depth = maxp.Z - minp.Z + 1;
	const size_t count = (size_t)width * depth;
	if (width <= 0 || depth <= 0)
		return false;
	if (samples.size() != count)
		samples.resize(count);

	HTTPFetchRequest request;
	request.url = addTerrainApiParams(mg_params->api_url, minp.X, minp.Z, maxp.X, maxp.Z,
			mg_params->api_scale, seed, mg_params->api_send_seed);
	request.method = HTTP_GET;
	request.timeout = mg_params->api_timeout_ms;
	request.connect_timeout = std::min<long>(mg_params->api_timeout_ms, 10000);
	request.quiet = true;

	HTTPFetchResult result;
	httpfetch_sync(request, result);
	if (!result.succeeded || result.response_code != 200) {
		if (!m_warned_api_failed) {
			warningstream << "TerrainDiffusion API request failed: " << request.url
						  << " response=" << result.response_code
						  << (result.timeout ? " timeout" : "") << std::endl;
			m_warned_api_failed = true;
		}
		return false;
	}

	const size_t elevation_bytes = count * sizeof(s16);
	const size_t climate_bytes = count * 4 * sizeof(float);
	if (result.data.size() < elevation_bytes) {
		if (!m_warned_api_failed) {
			warningstream << "TerrainDiffusion API response too small: got "
						  << result.data.size() << " bytes, need at least "
						  << elevation_bytes << std::endl;
			m_warned_api_failed = true;
		}
		return false;
	}

	const bool has_climate = result.data.size() >= elevation_bytes + climate_bytes;
	for (size_t n = 0; n < count; ++n) {
		TerrainDiffusionSample &sample = samples[n];
		sample.height =
				readS16LE(result.data, n * sizeof(s16)) * mg_params->api_height_scale +
				mg_params->api_height_offset;
		sample.heat = TD_DEFAULT_HEAT;
		sample.humidity = TD_DEFAULT_HUMIDITY;
		sample.has_climate = false;

		if (has_climate) {
			const size_t climate_offset = elevation_bytes + n * 4 * sizeof(float);
			const float heat = readFloatLE(result.data, climate_offset);
			const float precipitation =
					readFloatLE(result.data, climate_offset + 2 * sizeof(float));
			sample.heat = rangelim(std::lround(heat), -273L, 2000L);
			sample.humidity = precipToHumidity(precipitation);
			sample.has_climate = true;
		}
	}

	m_warned_api_failed = false;
	return true;
#else
	if (!m_warned_api_failed) {
		warningstream << "TerrainDiffusion API configured but Freeminer was "
					  << "built without cURL: " << mg_params->api_url << std::endl;
		m_warned_api_failed = true;
	}
	return false;
#endif
}

TerrainDiffusionSample MapgenTerrainDiffusion::samplePoint(pos_t x, pos_t z)
{
	std::vector<TerrainDiffusionSample> samples(1);
	if (sampleApiGrid(v3pos_t(x, 0, z), v3pos_t(x, 0, z), samples))
		return samples[0];

	// Point queries run on the server thread and may share mapgen 0 with an
	// emerge worker. Never wait for or start an expensive native tile here.
	if (m_native && m_native->loaded() && m_native->sampleGridCached(x, z, x, z, samples))
		return samples[0];

	if (m_model && m_model->loaded() &&
			m_model->sampleGrid(mg_params->model_node_scale,
					mg_params->model_height_scale, mg_params->model_height_offset, x, z,
					x, z, samples))
		return samples[0];

	return fallbackSample(x, z);
}

int MapgenTerrainDiffusion::getGroundLevelAtPoint(v2pos_t p)
{
	return myround(samplePoint(p.X, p.Y).height);
}

int MapgenTerrainDiffusion::getSpawnLevelAtPoint(v2pos_t p)
{
	const pos_t y = getGroundLevelAtPoint(p);
	if (y < water_level || y > water_level + 512)
		return MAX_MAP_GENERATION_LIMIT;
	return y + 2;
}

weather::heat_t MapgenTerrainDiffusion::calcBlockHeat(const v3pos_t &p, uint64_t seed,
		float timeofday, float totaltime, bool use_weather)
{
	const TerrainDiffusionSample sample = samplePoint(p.X, p.Z);
	if (sample.has_climate)
		return sample.heat;
	return Mapgen::calcBlockHeat(p, seed, timeofday, totaltime, use_weather);
}

weather::humidity_t MapgenTerrainDiffusion::calcBlockHumidity(const v3pos_t &p,
		uint64_t seed, float timeofday, float totaltime, bool use_weather)
{
	const TerrainDiffusionSample sample = samplePoint(p.X, p.Z);
	if (sample.has_climate)
		return sample.humidity;
	return Mapgen::calcBlockHumidity(p, seed, timeofday, totaltime, use_weather);
}

int MapgenTerrainDiffusion::generateTerrain()
{
	MapNode n_air(CONTENT_AIR);
	MapNode n_stone(c_stone);
	MapNode n_water(c_water_source);
	MapNode n_ice(c_ice);

	const s32 width = node_max.X - node_min.X + 1;
	const s32 depth = node_max.Z - node_min.Z + 1;
	std::vector<TerrainDiffusionSample> samples((size_t)width * depth);

	if (!sampleOnnxGrid(node_min, node_max, samples)) {
		return -1;
		size_t index = 0;
		for (pos_t z = node_min.Z; z <= node_max.Z; ++z)
			for (pos_t x = node_min.X; x <= node_max.X; ++x, ++index)
				samples[index] = fallbackSample(x, z);
	}

	const v3s32 &em = vm->m_area.getExtent();
	pos_t stone_surface_max_y = -MAX_MAP_GENERATION_LIMIT;
	size_t index2d = 0;

	for (pos_t z = node_min.Z; z <= node_max.Z; ++z) {
		for (pos_t x = node_min.X; x <= node_max.X; ++x, ++index2d) {
			const TerrainDiffusionSample &sample = samples[index2d];
			const pos_t surface_y = myround(sample.height);
			stone_surface_max_y = std::max(stone_surface_max_y, surface_y);

			u32 vi = vm->m_area.index(x, node_min.Y - 1, z);
			u32 index3d = (z - node_min.Z) * zstride_1u1d + (x - node_min.X);

			for (pos_t y = node_min.Y - 1; y <= node_max.Y + 1;
					y++, index3d += ystride, VoxelArea::add_y(em, vi, 1)) {
				if (vm->m_data[vi].getContent() != CONTENT_IGNORE)
					continue;

				if (y <= surface_y) {
					if (cave_noise_threshold &&
							noise_cave_indev->result[index3d] > cave_noise_threshold) {
						vm->m_data[vi] = n_air;
					} else {
						MapNode n = layers_get(index3d);
						bool protect = n.getContent() != CONTENT_AIR;
						if (cave_noise_threshold && noise_cave_indev->result[index3d] >
															cave_noise_threshold - 50) {
							vm->m_data[vi] = protect ? n_stone : n;
							protect = true;
						} else {
							vm->m_data[vi] = n;
						}
						if (protect)
							vm->m_flags[vi] |= VOXELFLAG_CHECKED2;
					}
				} else if (y <= water_level) {
					vm->m_data[vi] =
							(sample.heat < 0 && y > sample.heat / 3) ? n_ice : n_water;
					if (liquid_pressure > 0 && y <= 0) {
						const int pressure =
								((ItemGroupList)m_emerge->ndef->get(vm->m_data[vi])
												.groups)["pressure"];
						if (pressure > 0) {
							int add = water_level - y;
							if (add > pressure)
								add = pressure;
							vm->m_data[vi].addLevel(m_emerge->ndef, add, 1);
						}
					}
				} else {
					vm->m_data[vi] = n_air;
				}
			}
		}
	}

	return stone_surface_max_y;
}
