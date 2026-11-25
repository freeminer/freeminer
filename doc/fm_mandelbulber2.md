# Mandelbulber2 map generation

CMake discovers the CPU formula implementations in
`src/external/mandelbulber2/mandelbulber2` when that source tree is present.
Use `-DENABLE_MANDELBULBER2=OFF` to disable Mandelbulber2 formulas. When enabled,
if the source is absent and `FETCH_DEPS` is on, CMake downloads the pinned
Mandelbulber2 revision into the configured dependency directory. An existing repository can
also be supplied with `-DFETCHCONTENT_SOURCE_DIR_MANDELBULBER2=/path/to/mandelbulber2`.
With `mg_name = math`, select a formula by its Mandelbulber2 internal name:

    mg_math = {"generator":"mb2:mandelbulb_iq_pow8","N":15,"scale":0.001}

Use comma-separated names to cycle through a sequence, applying one formula
per iteration. The last formula that provides a distance estimator determines
the distance mode. Transforms without a distance estimator require a sequence
with another formula, for example:

    mg_math = {"generator":"mb2:transf_scale,mandelbulb","N":20,"scale":0.001}

The adapter uses Mandelbulber2's shared CPU defaults. Freeminer settings
`power`, `fractal_constant_factor`, `julia_mode`, `julia_a`, `julia_b`,
and `julia_c` apply where relevant. Cyclic sequences use shared parameters
and do not import Mandelbulber2 scene or hybrid presets. Custom OpenCL
formulas require Mandelbulber2's OpenCL runtime.

The built-in `sphere`, `rooms`, `mandelbox`, and `mengersponge` generators
remain available. Other unprefixed names select Mandelbulber2 formulas, with
aliases for several legacy names. Mandelbulber2 does not reproduce every legacy
formula or parameter, so existing math worlds may generate different terrain.

The create-world dialog accepts the complete `mg_math` JSON object for the
math mapgen. A nonempty field overrides the selected preset. Put Mandelbulber2
formula parameters in the `mandelbulber` object using the names from
Mandelbulber2's fractal parameter defaults. For example:

    {"generator":"mb2:mandelbox", "scale":0.001, "mandelbulber":{"mandelbox_scale":-1.5,"mandelbox_folding_min_radius":0.5,"transf_offset_000":[0,0,0]}}

Numbers, booleans, strings and three- or four-component vectors are accepted
according to the upstream parameter type. Vectors can be arrays or objects
with `x`, `y`, `z`, and, for four-dimensional vectors, `w` members. Indexed
parameters use the key followed by an underscore and the upstream index, for
example `IFS_enabled_0`. Unknown parameter names and invalid types cause an
error. The `mandelbulber` object applies to the shared formula parameters of a
sequence. Rendering, material and per-formula hybrid settings are outside this
parameter object.

The math mapgen field in the create-world dialog also accepts a pasted
Mandelbulber settings file. The importer reads `formula_1` from
`[main_parameters]` and numeric, boolean, and vector values from `[fractal_1]`.
It also imports the main iteration count `N` and Julia mode and constant when
present. The imported world uses `scale` 0.001, which can be adjusted later in
its `mg_math` JSON. Camera, lighting, and material settings affect Mandelbulber
rendering and are ignored by the map generator. Multiple fractal sections and
multiple active formulas are rejected. Formula IDs must identify a CPU formula
available in the bundled Mandelbulber2 version.
