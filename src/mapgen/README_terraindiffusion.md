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

## Complete Native Setup Guide

The commands below are intended to be run from the Freeminer repository root.
They build the CPU execution provider, which works without an NVIDIA or AMD
GPU. ONNX Runtime is optional for Freeminer generally, but is required for the
native and compact ONNX terrain sources.

Expect the complete source, build, checkpoint, Python environment, and ONNX
export to require several gigabytes of disk space. The 30 m ONNX export alone
is approximately 2.2 GB.

### 1. Install build prerequisites

On Ubuntu or Debian:

```bash
sudo apt update
sudo apt install build-essential cmake ninja-build git \
  python3 python3-dev python3-pip python3-venv
```

Freeminer has additional normal build dependencies outside the scope of this
mapgen guide. Install those as required by the main project build.

### 2. Clone ONNX Runtime

Clone recursively because ONNX Runtime uses Git submodules:

```bash
git clone --recursive https://github.com/microsoft/onnxruntime.git \
  src/external/onnxruntime
```

The revision tested with this integration is:

```text
c4f19617bca2567ebd8a35a750c14158f3fa4a40
```

To reproduce that revision exactly:

```bash
git -C src/external/onnxruntime checkout \
  c4f19617bca2567ebd8a35a750c14158f3fa4a40
git -C src/external/onnxruntime submodule update --init --recursive
```

Build a shared CPU library:

```bash
src/external/onnxruntime/build.sh \
  --config Release \
  --build_shared_lib \
  --parallel \
  --skip_tests
```

The expected outputs are:

```text
src/external/onnxruntime/include/onnxruntime/core/session/onnxruntime_cxx_api.h
src/external/onnxruntime/build/Linux/Release/libonnxruntime.so
```

For an NVIDIA build, install a compatible CUDA Toolkit and cuDNN first, then
add the ONNX Runtime CUDA options:

```bash
src/external/onnxruntime/build.sh \
  --config Release \
  --build_shared_lib \
  --parallel \
  --skip_tests \
  --use_cuda \
  --cuda_home /usr/local/cuda \
  --cudnn_home /usr/local/cuda
```

The native setting `mgterraindiffusion_native_provider = auto` selects CUDA
only when the built ONNX Runtime reports that provider; otherwise it uses CPU.

### 3. Clone Terrain Diffusion

```bash
git clone https://github.com/xandergos/terrain-diffusion.git \
  src/external/terrain-diffusion
```

The tested upstream revision is:

```text
82a0431281f21a6ec3d691a12ee61525de5b0790
```

The Freeminer checkout adds a small change to
`terrain_diffusion/onnx/export.py` so the pipeline `config.json` is exported
beside the graphs. When using an unmodified upstream checkout, copy that file
manually in step 6.

### 4. Create the Python export environment

```bash
python3 -m venv .venv-terraindiffusion
source .venv-terraindiffusion/bin/activate
python -m pip install --upgrade pip
python -m pip install \
  -r src/external/terrain-diffusion/requirements.txt
python -m pip install onnx onnxruntime "huggingface_hub[cli]"
```

For CUDA export or verification, install the PyTorch and ONNX Runtime GPU
packages matching the installed CUDA version instead of the CPU packages.
Exporting on CPU works, but the large base model can take considerable time.

### 5. Download the 30 m checkpoint

Use the current `hf` command, not the deprecated `huggingface-cli` command:

```bash
hf download xandergos/terrain-diffusion-30m \
  --local-dir checkpoints/models/terrain-diffusion-30m
```

The model is public. If Hugging Face requests authentication, run
`hf auth login` and repeat the download.

### 6. Export all three ONNX graphs

Keep the virtual environment active and expose the cloned Python package with
`PYTHONPATH`:

```bash
PYTHONPATH=src/external/terrain-diffusion \
python -m terrain_diffusion.onnx.export \
  checkpoints/models/terrain-diffusion-30m \
  --output onnx_export/terrain-diffusion-30m \
  --verify
```

If the upstream exporter does not create `config.json`, copy it explicitly:

```bash
cp checkpoints/models/terrain-diffusion-30m/config.json \
  onnx_export/terrain-diffusion-30m/config.json
```

The resulting directory must contain:

```text
onnx_export/terrain-diffusion-30m/coarse_model.onnx
onnx_export/terrain-diffusion-30m/base_model.onnx
onnx_export/terrain-diffusion-30m/decoder_model.onnx
onnx_export/terrain-diffusion-30m/config.json
```

Do not set any of these three graph paths as
`mgterraindiffusion_height_model`; that setting is only for the optional compact
coordinate-to-height model.

### 7. Configure and build Freeminer

The ONNX Runtime detector is enabled by default and expects the source layout
used above. Configure explicitly so CMake output is easy to verify:

```bash
cmake -S . -B build -G Ninja \
  -DCMAKE_BUILD_TYPE=RelWithDebInfo \
  -DBUILD_UNITTESTS=TRUE \
  -DENABLE_ONNXRUNTIME=ON \
  -DONNXRUNTIME_ROOT="$PWD/src/external/onnxruntime"
cmake --build build --target freeminer -j"$(nproc)"
```

Configuration must print a line similar to:

```text
Using ONNX Runtime: .../include/onnxruntime/core/session .../libonnxruntime.so
```

If CMake cached a previous failed search, configure once with the exact paths:

```bash
cmake -S . -B build \
  -DENABLE_ONNXRUNTIME=ON \
  -DONNXRUNTIME_INCLUDE_DIR="$PWD/src/external/onnxruntime/include/onnxruntime/core/session" \
  -DONNXRUNTIME_LIBRARY="$PWD/src/external/onnxruntime/build/Linux/Release/libonnxruntime.so"
```

Check that the executable resolves the shared library:

```bash
ldd build/freeminer | grep onnxruntime
```

When an installed or relocated executable reports `libonnxruntime.so => not
found`, install the library in a system path or set its directory explicitly:

```bash
export LD_LIBRARY_PATH="$PWD/src/external/onnxruntime/build/Linux/Release:$LD_LIBRARY_PATH"
```

### 8. Verify the real model set

With unit tests enabled, this loads and validates all three graph signatures
without generating a full terrain tile:

```bash
FREEMINER_TD_MODEL_DIR="$PWD/onnx_export/terrain-diffusion-30m" \
build/freeminer --run-unittests --test-module TestTerrainDiffusion
```

Both `testDeterministicHelpers` and `testModelLoad` should pass.

### 9. Configure a world

Use absolute paths in the world mapgen configuration:

```conf
mg_name = terraindiffusion
mgterraindiffusion_native_model_dir = /absolute/path/to/onnx_export/terrain-diffusion-30m
mgterraindiffusion_native_node_scale = 30
mgterraindiffusion_native_provider = auto
```

At startup, successful native initialization logs:

```text
TerrainDiffusion mapgen loaded native three-model pipeline ...
```

The first generated native tile is intentionally expensive, especially on
CPU. Later generation in the same area benefits from the coarse, latent, and
decoded tile caches.

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

Follow the complete setup guide above to clone both projects, build ONNX
Runtime, download a checkpoint, and export the three graphs. This section
describes optional conditioning and implementation details.

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

The setup guide uses the recommended 30 m model. The upstream ONNX exporter
produces multiple pipeline models:

```text
coarse_model.onnx
base_model.onnx
decoder_model.onnx
```

Set the directory containing these files as
`mgterraindiffusion_native_model_dir`. Do not pass one of them to
`mgterraindiffusion_height_model`.

The resulting directory, including its pipeline `config.json`, can be used
directly by the native pipeline.

## Runtime Behavior

When the native model set loads successfully, the log contains:

```text
TerrainDiffusion mapgen loaded native three-model pipeline ...
```

The compact model path instead logs `TerrainDiffusion mapgen loaded ONNX
model ...`.

When no compatible model is available, the mapgen logs once:

```text
TerrainDiffusion mapgen using procedural fallback
```

The fallback is intentionally deterministic and terrain-like, so worlds remain
usable even without ONNX Runtime or a model.

## Current Limitations

- Native inference runs the complete three-model pipeline, but its deterministic
  synthetic conditioning is not byte-identical to the upstream Python
  WorldClim pipeline. Generate and configure `synthetic_map_stats.json` for a
  closer statistical match.
- Loading the approximately 2 GB model set and generating the first decoded
  tile can be slow. CPU generation is functional but substantially slower than
  a supported GPU execution provider.
- Generated caches belong to individual mapgen workers, while ONNX sessions are
  shared. Increase cache settings carefully because their memory budgets apply
  per worker.
- Server spawn and climate point queries consume native results only when the
  required decoded tiles are already cached. They fall back immediately when
  the cache is busy or missing so an emerge worker cannot hold the server thread
  behind a long inference lock.
