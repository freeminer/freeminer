# Core TNT object effects

`core.tnt_explode(pos, options)` can apply damage and knockback to players and
Lua entities after processing map nodes. Effects are opt-in for direct API callers.
The default game's `tnt.boom(pos, def)` enables them and accepts the same multipliers.

| Option | Core default | Default game's TNT default |
| --- | ---: | ---: |
| `damage_multiplier` | 0 | 1 |
| `knockback_multiplier` | 0 | 1 |
| `object_radius_multiplier` | 3 | 3 |
| `player_damage_multiplier` | 1 | 0.2 |
| `player_knockback_multiplier` | 1 | 0.2 |

Zero disables the corresponding effect; negative and non-finite multipliers are
also treated as zero. Player multipliers multiply the general multipliers.

Let `R = result.radius * object_radius_multiplier`, using the actual shell radius
reached by the map explosion, and `d = max(1, distance_in_nodes)`.
Objects within `R` receive:

- Damage: `20 * R / d * damage_multiplier`, also multiplied by
  `player_damage_multiplier` for players. Fractional damage is truncated.
- Outward velocity: `10 * R / d * knockback_multiplier` nodes/second, also
  multiplied by `player_knockback_multiplier` for players. The impulse is capped
  at 250 nodes/second; entities' resulting velocity is also capped at 250.
  An object exactly at the center has no outward impulse.

Entity damage uses a self-punch with the `fleshy` damage group (capped at 32767),
respecting armor, immortality and `on_punch`. Player damage directly reduces HP
(capped at 65535 damage) through the normal HP-change hooks and immortality checks;
it does not invoke player punch callbacks or apply armor scaling. Knockback is
independent of damage, so immortal objects can still move. There is no random
velocity jitter.

`object_wall_shield` defaults to `true` in both APIs. Surviving walkable nodes
block both damage and knockback, as do unloaded/unknown nodes. The check runs
after map destruction, so walls destroyed by this explosion no longer shield.
Protected walls that remain solid also shield. This is a binary line check,
not material-dependent attenuation: it traces to the object's collision-box
center (or its position when there is no collision box), treating walkable
nodeboxes as full cubes. The origin node is skipped so the source TNT/boom does
not block its own explosion. Non-walkable nodes, including ordinary liquids,
do not shield. Shielded objects do not receive `on_blast_object` callbacks.
Set `object_wall_shield = false` to restore distance-only effects.

An optional `on_blast_object(object, damage)` callback returns
`do_damage, do_knockback`. It runs before either effect and can veto each one.
If it removes the object, both effects are skipped. Callback errors are logged
and skip that object's effects. The default game uses this callback to retain
entity `on_blast` behavior and collect its drops. Node `on_blast` callbacks and
chained explosions returned to Lua are processed after this core object pass.

```lua
core.tnt_explode(pos, {
    radius = 4,
    damage_multiplier = 1,
    knockback_multiplier = 0.5,
    object_radius_multiplier = 3,
    player_damage_multiplier = 0.2,
    player_knockback_multiplier = 0.2,
})
```

## TNT absorption

`blast_tnt_absorb_strength` sets the minimum incoming ray strength required to
absorb TNT into the current explosion. It defaults to `1.0`, independently of
the encountered TNT's explosive power. The existing minimum propagation-strength
check also applies. Weaker hits ignite TNT instead, without adding its energy or
propagating the current wave through it; default burning TNT explodes after four
seconds. Absorbed TNT boosts the current shell's rays. Absorption decisions use
incoming strength before any TNT energy is added for that shell.

Set this option directly in `core.tnt_explode`, through the `def` table passed to
`tnt.boom`, or as `tnt.blast_tnt_absorb_strength` for the game's default. For example,
`blast_tnt_absorb_strength = 2.0` requires a stronger hit before absorption.

When the shared blast energy is exhausted, surviving rays perform one final
ignition-only check in the next shell. Reachable TNT there starts its fuse without
being absorbed; this check does not destroy nodes or extend the blast. Terminal
ignition respects protection unless `ignore_protection` is enabled.

## Energy distribution options

Both `core.tnt_explode(pos, options)` and `tnt.boom(pos, def)` accept:

| Option | Default | Meaning |
| --- | ---: | --- |
| `blast_tnt_ray_fraction` | `0.8` | Fraction of an absorbed explosive's energy added to the local path at its position. The remainder is shared equally among the current shell's active candidates, including that position. |
| `blast_shell_fraction` | `0` | Legacy option; no sharing outside the core. Use `blast_core_shell_fraction` for deliberate core mixing. |

Fractions are clamped to `[0, 1]`; non-finite values use the default. Absorption
adds the explosive's full energy to the budget. `tnt.blast_tnt_ray_fraction` is
also accepted as the game-wide default when the call omits it.

## Cubic core with outward extensions

`blast_core_radius` sets the last shell where full-surface sharing is allowed.
It defaults to the nominal radius derived from the initial blast strength; zero
disables core sharing. `blast_core_shell_fraction` sets the mixing fraction inside
that core. The current source default is `0`; set it explicitly for a solid core:

```lua
tnt.boom(pos, {
    radius = 4,
    blast_strength = 3000,
    blast_core_radius = 3,
    blast_core_shell_fraction = 0.9,
    blast_tnt_ray_fraction = 0.8,
    blast_distance_loss = 0.02,
    blast_resistance_scale = 1,
})
```

Material resistance, protection, callbacks, available energy and the execution
time limit still constrain destruction. Beyond the core there is no shell-wide
redistribution. Blocked or weakened directions retain their shadow.

## Angular footprint propagation

The explosion still visits cubic onion shells. Rays now represent angular patches
on six cube faces instead of one privileged continuation plus newly funded rays.
Each patch carries energy per solid angle. Its exact overlap with next-shell
nodes determines the transferred energy. Splitting a patch conserves its total
energy and does not reduce its intensity. Initial directions cover the whole
sphere uniformly per solid angle, without concentrating energy into 26 rays.

A node may receive several disjoint angular contributions. Their energy combines
for the material hit, then the surviving fraction is applied to every contribution.
The directions remain separate; no strongest-direction winner absorbs the others.
An opaque node removes its patches. Resistant material reduces their energy;
clear neighboring paths cannot refill the damaged angular region. Local absorbed
TNT energy boosts the contributions crossing its node, while the configured TNT
shell fraction remains an explicit new energy source for active candidates.

`blast_distance_loss` and `blast_min_strength` are measured per reference angular
area, `4*pi/26`, preserving their first-shell scale. Actual loss and cutoff scale
with each represented angular area. Consequently subdivision and increasing shell
node counts do not introduce extra attenuation. The air loss is per onion-shell
crossing; material resistance is still charged per impacted node. Energy per voxel
naturally decreases as a fixed angular region spreads over more distant nodes.

Adjacent patches are rejoined only when their energy densities agree to floating
point tolerance. This removes temporary voxel boundaries in clear air without
averaging away material shadows. Detailed material patterns can retain more patches
and cost more processing time; the existing explosion time limit still applies.

`blast_scatter_fraction` and `blast_air_scatter_fraction` are obsolete and ignored:
angular overlap replaces donor counts and fixed child shares. `blast_shell_fraction`
no longer redistributes energy outside the core. Use `blast_core_shell_fraction`
explicitly to control the core. The TNT absorption split remains configurable.
