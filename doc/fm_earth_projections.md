# Earth world geometry

`mg_earth.projection` selects actual voxel geometry, not a flat map projection.
The default is `flat`, preserving the existing column-based Earth generator.
Use a new world when changing geometry; existing chunks are not reprojected.

Example `minetest.conf` / world mapgen configuration:

```conf
mg_name = earth
mg_earth = {"scale":{"x":1,"y":100,"z":1},"projection":{"type":"sphere","radius":1024,"origin":{"x":0,"y":0,"z":0}}}
```

For a 1:1 metre scale (one node represents one metre in every direction), use
`1` for all three scale components. The corresponding configurations for each
projection are:

```conf
# Flat Earth
mg_earth = {"scale":{"x":1,"y":1,"z":1},"projection":"flat"}

# Spherical Earth
mg_earth = {"scale":{"x":1,"y":1,"z":1},"projection":{"type":"sphere","radius":6378137,"origin":{"x":0,"y":0,"z":0}}}

# Cubic Earth
mg_earth = {"scale":{"x":1,"y":1,"z":1},"projection":{"type":"cube","radius":6378137,"origin":{"x":0,"y":0,"z":0}}}

# Torus Earth
mg_earth = {"scale":{"x":1,"y":1,"z":1},"projection":{"type":"torus","radius":6378137,"major_radius":12756274,"origin":{"x":0,"y":0,"z":0}}}
```

The radii above are deliberately small so that test worlds generate quickly.
At true 1:1 Earth scale, use an Earth radius of about 6,378,137 nodes:

```conf
mg_earth = {"scale":{"x":1,"y":1,"z":1},"projection":{"type":"sphere","radius":6378137,"origin":{"x":0,"y":0,"z":0}}}
```

The sphere and cube `radius` values are measured in nodes. At 1:1 scale, one
node is one metre, so `radius = 6378137` represents the Earth's equatorial
radius in metres. With another scale, divide the desired radius in metres by
the horizontal node size.

For a cube, change `type` to `cube`. A 1:1 torus uses the same Earth-sized
tube radius and a major radius of twice that value:

```conf
mg_earth = {"scale":{"x":1,"y":1,"z":1},"projection":{"type":"torus","radius":6378137,"major_radius":12756274,"origin":{"x":0,"y":0,"z":0}}}
```

An omitted projection, `"projection":"flat"`, or
`"projection":{"type":"flat"}` selects the existing flat world. The names
`sphere` and `spherical` both select the spherical implementation.

| Parameter | Meaning |
| --- | --- |
| `projection.radius` | Sphere radius, cube half-side, or torus tube radius, in nodes; default 10000 |
| `projection.major_radius` | Torus ring radius, in nodes; default 20000; must exceed tube radius |
| `projection.origin` | World-space center in nodes; default `(0,0,0)` |
| `scale.y` | Elevation metres per node, as in flat mode |
| `center.y` | Subtracted from scaled Earth elevation, as in flat mode |
| `water_level` | Mapgen water level, interpreted as surface-relative altitude in curved mode |

For curved worlds, radius replaces horizontal scaling. `scale.x`, `scale.z`,
`center.x`, and `center.z` only affect flat mode. Scales must be finite and
positive for curved worlds. Choose radius and elevation scale so relief is
small relative to the radius; for a torus keep tube radius plus relief below
its major radius to preserve the hole. Sea level should satisfy the same limits.

The sphere and cube use Y as the polar axis and longitude zero along +X;
longitude +90 points along +Z. The cube maps each Earth direction radially to
its enclosing cube face. Its altitude is `max(abs(x),abs(y),abs(z)) - radius`.
The cube face normal is axis-aligned, with deterministic X/Y/Z precedence at
edges. Geographic elevation on a cube consequently expands or contracts faces
radially; local normal and constant-geographic-coordinate displacement differ.

On a torus, longitude goes around the ring in the XZ plane. Twice the latitude
is the angle around the tube, starting on its outer equator. Earth's two poles
meet at the tube's inner equator; this is an intentional geographic seam.
Unlike a sphere, pole samples there retain longitude-dependent data.

## Adapter and generation

`fm_earth_projection.h` provides concrete `Flat`, `Sphere`, `Cube`, and `Torus`
types and a runtime `Adapter`. The mapgen binds a concrete generation function
at initialization. Its voxel loop directly calls that projection's methods;
there is no projection switch or indirect projection call inside the loop.
Standalone adapter queries use bound function pointers.

- `sample(world_position)` returns degrees and surface-relative altitude.
- `place(latitude, longitude, altitude)` returns a continuous world position in
  node units. Convert to engine object units separately if needed.
- `altitude(world_position)` avoids geographic angle calculations.
- `up(world_position)` returns a unit outward normal for future gravity use.
- `coverage(min, max)` returns conservative geographic rectangles, split at
  seams; a region containing an axis/singularity can cover all longitudes.
- `top(x, z, altitude)` returns the upper Y intersection with a constant-altitude
  surface, or no intersection.

Inverse placement is meaningful outside the collapsed core: sphere/cube
`radius + altitude > 0`, and torus `0 < radius + altitude < major_radius`.
Singular samples use deterministic finite coordinates/normals.

Curved terrain compares each voxel's altitude with scaled HGT elevation and
fills seas by altitude. Far visibility, geographic climate sampling and climate
altitude adjustments use this geometry too. Deep core and empty outer space
skip HGT queries using the signed 16-bit raster elevation envelope. The flat
terrain loop remains unchanged. Curved sampling is more expensive; eliminating
dispatch does not eliminate trigonometry or additional raster samples.

Legacy single-Y ground queries scan the vertical ray for its uppermost solid
voxel and return the mapgen's unsuitable-position sentinel for holes. This is
more expensive than flat height lookup. Spawn checks use that upper surface;
for torus worlds, set a static spawn near the ring rather than its central hole.
Use the full 3D conversion API for curved worlds; legacy two-coordinate
`pos_to_ll(x,z)` and `ll_to_pos(ll)` reject curved mode.

## Current integration limits

Gravity, player/camera orientation, wind orientation, directional sunlight and
the liquid solver still use the engine's existing world axes. Curved generation
does not enqueue initial liquid flow; later gameplay-triggered liquid updates
still follow the existing solver. Terrain is ready for a subsequent gravity
patch, but these worlds are not yet fully playable planets.

OSM/Arnis buildings, roads, and other authored features are disabled for curved
modes because their placement pipeline assumes vertical columns. Flat authored
features remain enabled. The adapter exposes bounds and inverse placement for
that future integration. Curved generation also bypasses the flat heightmap,
Y-based chunk skipping, and flat biome/decorations/cave passes. It uses Earth
layer materials with altitude-based chunk bounds; it does not rotate layer
fold patterns or node orientations.
