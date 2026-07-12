# Terrain Diffusion Mapgen

`mapgen_terraindiffusion.cpp` adds a separate Freeminer mapgen named
`terraindiffusion`.

The mapgen has four terrain sources, tried in this order:

1. Terrain Diffusion API server output from the upstream Python pipeline.
2. The native three-model ONNX pipeline.
3. A compact coordinate-to-height ONNX model.
4. Procedural fallback terrain.

If no external source is configured or reachable, it still generates terrain
with the procedural fallback.

## Build

ONNX Runtime is optional.

The CMake option is enabled by default:

```cmake
ENABLE_ONNXRUNTIME=1
```

The default ONNX Runtime root is:

```text
src/external/onnxruntime
```

The detector looks for headers like:

```text
include/onnxruntime/core/session/onnxruntime_cxx_api.h
```

and a library like:

```text
build/Linux/Release/libonnxruntime.so
```

If detection succeeds, CMake prints `Using ONNX Runtime`. If not, the mapgen is
still available, but uses fallback terrain.

## Basic Configuration

Use the mapgen with:

```conf
mg_name = terraindiffusion
```

Use an ONNX model with:

```conf
mgterraindiffusion_height_model = /absolute/path/to/model.onnx
```

Use the complete native pipeline with an exported model directory:

```conf
mgterraindiffusion_native_model_dir = /absolute/path/to/onnx_export/terrain-diffusion-30m
mgterraindiffusion_native_node_scale = 30
```

Use the full upstream Terrain Diffusion pipeline through its API server with:

```conf
mgterraindiffusion_api_url = http://127.0.0.1:8000/terrain
mgterraindiffusion_api_scale = 1
mgterraindiffusion_api_send_seed = true
```

Optional model scaling:

```conf
mgterraindiffusion_model_node_scale = 1.0
mgterraindiffusion_model_height_scale = 1.0
mgterraindiffusion_model_height_offset = 0.0
```

Optional native pipeline controls:

```conf
mgterraindiffusion_native_height_scale = 1.0
mgterraindiffusion_native_height_offset = 0.0
mgterraindiffusion_native_residual_std = 0.7
mgterraindiffusion_native_cache_tiles = 8
mgterraindiffusion_native_cache_mb = 128
mgterraindiffusion_native_provider = auto
mgterraindiffusion_native_device_id = 0
mgterraindiffusion_native_intra_threads = 8
mgterraindiffusion_native_prefetch = false
```

`native_provider = auto` selects CUDA, then ROCm, then CPU according to the
providers compiled into ONNX Runtime. Model sessions are shared between mapgen
workers, while generated terrain caches remain local to each worker.

The defaults are tuned for the `terrain-diffusion-30m` checkpoint and a modern
multicore CPU:

- `native_node_scale = 30` maps one model pixel (30 m) to 30 one-meter nodes.
- `native_height_scale = 1.0` preserves the model's meter-based elevations.
- `native_residual_std = 0.7` comes from the checkpoint pipeline config.
- `native_intra_threads = 8` gives the large base graph useful parallelism
  without occupying every logical CPU during map generation.
- eight tiles and 128 MB retain a large world area at 30-node scale while
  bounding each mapgen worker's generated-data memory.
- prefetch is disabled because generating an unused 512-pixel neighboring tile
  is expensive. Enable it only when generation follows long, predictable paths.

The exporter writes the checkpoint's pipeline `config.json` beside the ONNX
graphs. Keep that file with the three models; native inference uses it for
normalization and conditioning values and falls back to the 30 m defaults when
it is absent.

Optional API scaling:

```conf
mgterraindiffusion_api_height_scale = 1.0
mgterraindiffusion_api_height_offset = 0.0
mgterraindiffusion_api_timeout_ms = 30000
```

Fallback terrain controls:

```conf
mgterraindiffusion_fallback_height_scale = 160.0
mgterraindiffusion_fallback_detail_scale = 1.0
```

The mapgen derives from V7, so normal V7 caves, caverns, dungeons,
decorations, ores, liquids, lighting, and layer handling still apply after the
terrain height is sampled.

## Full Terrain Diffusion Pipeline Through API

This is the reference-compatible path when exact upstream Python conditioning
and postprocessing are required. The native path below avoids the Python
service and is intended for normal in-engine generation.

Start the upstream API server:

```bash
cd src/external/terrain-diffusion

python -m terrain_diffusion.inference.api \
  xandergos/terrain-diffusion-30m \
  --host 127.0.0.1 \
  --port 8000 \
  --device cuda
```

For CPU-only testing:

```bash
python -m terrain_diffusion.inference.api \
  xandergos/terrain-diffusion-30m \
  --host 127.0.0.1 \
  --port 8000 \
  --device cpu \
  --no-compile
```

Then configure Freeminer:

```conf
mg_name = terraindiffusion
mgterraindiffusion_api_url = http://127.0.0.1:8000/terrain
```

The API returns:

```text
elevation: int16 little-endian [H, W]
climate:   float32 little-endian [H, W, 4]
```

Climate channels are:

```text
temp, temperature seasonality, precipitation, precipitation seasonality
```

Freeminer uses `temp` as heat and converts `precipitation` to humidity.

## Native Three-Model Pipeline

Install the exporter dependencies in a virtual environment from the Freeminer
repository root:

```bash
python -m venv .venv-terraindiffusion
source .venv-terraindiffusion/bin/activate
python -m pip install -r src/external/terrain-diffusion/requirements.txt
python -m pip install onnx onnxruntime
```

Export all three upstream models into one directory:

```bash
PYTHONPATH=src/external/terrain-diffusion \
python -m terrain_diffusion.onnx.export \
  checkpoints/models/terrain-diffusion-30m \
  --output onnx_export/terrain-diffusion-30m \
  --verify
```

The directory must contain:

```text
coarse_model.onnx
base_model.onnx
decoder_model.onnx
```

For conditioning distributions derived from the same WorldClim data as the
upstream pipeline, generate its quantile cache once:

```bash
source .venv-terraindiffusion/bin/activate
python util/terrain_diffusion_stats.py
```

The upstream utility may offer to download missing WorldClim files. Configure
the generated cache with:

```conf
mgterraindiffusion_native_conditioning_stats = /absolute/path/to/src/external/terrain-diffusion/data/global/synthetic_map_stats.json
```

The C++ pipeline reproduces the upstream model and scheduler stages: a
deterministic world-space synthetic climate/elevation condition feeds the
20-step coarse DPM solver, the coarse result conditions the base latent model,
and the decoder combines high-frequency residuals with the low-frequency
elevation latent. Temperature and precipitation from the coarse output are
also passed to Freeminer's biome climate calculations.

The C++ conditioning field uses Freeminer's deterministic world noise instead
of Python's WorldClim-derived quantile tables. Consequently it uses the actual
three neural models but is not expected to produce byte-identical terrain to
the Python API for the same seed.

Coarse, latent, and decoded stages use globally aligned overlap blending.
Generated tiles are retained in memory-bounded LRU caches. Increasing
`mgterraindiffusion_native_cache_tiles` or
`mgterraindiffusion_native_cache_mb` avoids repeated inference when generation
revisits nearby positions, at the cost of memory. Optional prefetch can prepare
neighboring decoded tiles on a background worker.

## Compact ONNX Interface

The optional compact path expects one coordinate-to-height inference model.

Input:

```text
coords: float32 [N, 2]
```

Each row contains:

```text
x * mgterraindiffusion_model_node_scale
z * mgterraindiffusion_model_node_scale
```

Output 0:

```text
height: float32 [N]
```

The final surface level is:

```text
height * mgterraindiffusion_model_height_scale +
mgterraindiffusion_model_height_offset
```

Optional output 1:

```text
climate: float32 [N, C]
```

Supported climate layouts:

```text
C = 2: heat, humidity
C >= 3: heat, unused, precipitation
```

For `C >= 3`, precipitation is converted to humidity by:

```text
humidity = clamp(round(precipitation / 20), 0, 100)
```

Heat is clamped to `[-273, 2000]`; humidity is clamped to `[0, 100]`.

## Upstream Terrain Diffusion Models

The bundled upstream project is in:

```text
src/external/terrain-diffusion
```

Recommended upstream model for playable terrain:

```text
xandergos/terrain-diffusion-30m
```

Larger-scale model:

```text
xandergos/terrain-diffusion-90m
```

The upstream README describes downloading and exporting models from Hugging
Face. Its ONNX exporter produces multiple pipeline models:

```text
coarse_model.onnx
base_model.onnx
decoder_model.onnx
```

Set the directory containing these files as
`mgterraindiffusion_native_model_dir`. Do not pass one of them to
`mgterraindiffusion_height_model`.

Example upstream export command:

```bash
hf download xandergos/terrain-diffusion-30m \
  --local-dir checkpoints/models/terrain-diffusion-30m

PYTHONPATH=src/external/terrain-diffusion \
python -m terrain_diffusion.onnx.export \
  checkpoints/models/terrain-diffusion-30m \
  --output onnx_export/terrain-diffusion-30m \
  --verify
```

The resulting directory can be used directly by the native pipeline.

## Runtime Behavior

When a compatible ONNX model loads successfully, the log contains:

```text
TerrainDiffusion mapgen loaded ONNX model ...
```

When no compatible model is available, the mapgen logs once:

```text
TerrainDiffusion mapgen using procedural fallback
```

The fallback is intentionally deterministic and terrain-like, so worlds remain
usable even without ONNX Runtime or a model.

## Current Limitation

The real Terrain Diffusion pipeline is supported through the upstream Python API
server. The native ONNX path is still only a compact `coords -> height` model
host and is not yet a complete native port of Terrain Diffusion's multi-stage
inference pipeline.
