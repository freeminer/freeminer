# Point gravity (experimental)

Set `fm_gravity_sources` in the server configuration before startup, for example:

```
fm_gravity_sources = 0,0,0,9.81,1000; 5000,0,0,4,500
```

Each entry is `x,y,z,strength,falloff`, in nodes and nodes per second squared.
Alternatively, specify a source's mass in metric tonnes using `mass=`:

```
fm_gravity_sources = 0,0,0,mass=1e9,1; 5000,0,0,4,500
```

Mass and strength sources can be mixed. One node is treated as one meter. For
a mass source the acceleration magnitude at distance `r` is
`6.6743e-8 * mass_tonnes / (r^2 + falloff^2)` nodes/s². Thus a billion tonnes
at 100 nodes with a 1-node falloff produces about 0.00667 nodes/s². The falloff
is a softening distance, not a body's surface radius: at `r = falloff`, the
acceleration is half the unsoftened inverse-square value. At the exact center
the direction is undefined, so the contribution is zero.

Sources are separated by semicolons. Their acceleration vectors add together
with a uniform downward background across the entire world. Set, for example,
`gravity_background = 1.62` for a background of 1.62 nodes/s², or set it to
`0` to use only the point sources. Values from 0 to 10000 are accepted.
The default is 9.81 nodes/s². If the value is invalid, the server logs a warning
and falls back to `movement_gravity`.
For distance `r`, a source has magnitude `strength / (1 + (r / falloff)^2)`.
At the source position its contribution is zero. The field is sampled at the
center of each 16-node mapblock. Empty or invalid source settings contribute no
point gravity. Up to 256 sources are accepted; strengths (including the
strength derived from mass) must have a magnitude at most 10000 nodes/s², and
falloffs must be between 0.000001 and 10^12 nodes. Negative mass repels objects.

For Earth mapgen projections `sphere` and `cube`, the default is a
point at `projection.origin` with Earth's mass (5.9722e21 metric tonnes).
`inverted_sphere` uses the same negative mass, repelling objects outward. The
default softening distance is Earth's physical radius, 6371000 nodes; it keeps
gravity bounded even when the generated world is much smaller than Earth.
For `torus`, eight equal-mass sources are evenly spaced around the center of
the tube. Their mass is chosen to produce 9.81 nodes/s² at the outer surface on
the positive X axis, and their softening distance is 20% of the tube radius.
The resulting field is uneven; on the default torus it is weaker on the inner
surface and tilts toward the central hole near the top and bottom.
Set `fm_gravity_sources` explicitly to replace this default, or to an empty
value to remove it. The uniform background remains active unless configured
separately; set `gravity_background = 0` for purely point-directed gravity.

`core.get_gravity(pos)` returns the server field in nodes per second squared.
Lua entities can call `core.apply_gravity(object, other_acceleration)` during
`on_step` to replace their acceleration with the local field plus an optional
additional acceleration. The function returns false for non-Lua entities or
objects without a position; it returns true after updating a Lua entity. Remove any
separate downward gravity from the entity's existing acceleration.
The configuration is sent to Freeminer clients at login in the Freeminer init
packet, including when `MINETEST_PROTO` is enabled. Restart the server after
changing it. Unmodified Luanti clients receive the uniform background through
the standard movement packet, but cannot apply point gravity to player movement.

This is experimental: player acceleration, digging particles and C++ falling
objects use the field, but collision grounding, jumping, walking, the camera,
and falling-node landing still assume Y is vertical. Lua entities and particles
with their own acceleration are not automatically changed. Mods can query the
field and apply it themselves, taking care not to add their usual gravity twice.
