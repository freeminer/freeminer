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

Let `R = result.effect_radius * object_radius_multiplier` and
`d = max(1, distance_in_nodes)`. `effect_radius` is the nominal radius derived from
total explosive power (including absorbed TNT), capped by the shell reached.
`result.radius` separately reports the furthest shell visited. Long air rays do not
enlarge the drop/particle area or object effects.
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

When a projected hit falls below the propagation cutoff, TNT at that position
is ignited without absorption or further propagation. Terminal ignition respects
protection unless `ignore_protection` is enabled. Chained explosions returned after
a timeout are scheduled on later server steps, avoiding recursive budget resets.

## Energy distribution options

Both `core.tnt_explode(pos, options)` and `tnt.boom(pos, def)` accept:

| Option | Default | Meaning |
| --- | ---: | --- |
| `blast_tnt_ray_fraction` | `0.8` | Fraction of an absorbed explosive's energy added to the local path at its position. The remainder is shared equally among the current shell's active candidates, including that position. |

Fractions are clamped to `[0, 1]`; non-finite values use the default. Absorption
adds the explosive's full energy to live hits. `tnt.blast_tnt_ray_fraction` is
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
    blast_distance_loss = 0.1,
    blast_resistance_scale = 1,
})
```

Material resistance, protection, callbacks, available energy and the execution
time limit still constrain destruction. A partial mix retains every original angular
density scaled by `1 - blast_core_shell_fraction` and adds only the pooled fraction
as background over the full surface. Beyond the core there is no shell-wide
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

`blast_distance_loss` is a fixed energy cost per node hit, defaulting to `0.1`.
An air node with incoming energy `E` leaves `max(0, E - 0.1)`, regardless of
shell distance or angular coverage. Each node pays once, even when several patches
reach it; their relative contributions are preserved. Solid nodes additionally
consume their material resistance. More reached nodes therefore consume more
energy, and energy per voxel also decreases as the wave spreads.

Set `blast_distance_loss` per explosion or use the default game's
`tnt_blast_distance_loss` setting. Existing explicit values remain in effect;
`0.1` is an absolute energy amount, not a 10% reduction.

`blast_min_strength` (default `0.15`) is a minimum **total energy per node hit**,
independent of angular area. Hits at or below it stop after the terminal TNT
ignition check. This discards distant, powerless fragments instead of following
them for hundreds of shells. Concentrated strong rays can still travel further.
Reducing this option increases air range and processing cost; setting it to zero
removes this energy floor.

Adjacent patches are rejoined only when their energy densities agree to floating
point tolerance. This removes temporary voxel boundaries in clear air without
averaging away material shadows. Detailed material patterns can retain more patches
and cost more processing time; the existing explosion time limit still applies.

`blast_scatter_fraction` and `blast_air_scatter_fraction` are obsolete and ignored:
angular overlap replaces donor counts and fixed child shares. `blast_shell_fraction`
is also ignored. Use `blast_core_shell_fraction`
explicitly to control the core. The TNT absorption split remains configurable.

## Propagation outcomes and returned energy

Solid nodes (walkable nodes and liquids) block the current wave if they are
protected, cannot be removed or transformed, or have less than one unit of energy
left after resistance. A queued solid-node `on_blast` callback also blocks: it runs
in Lua after this wave and might leave the node intact. Successfully removed or
transformed nodes transmit their remaining energy. Non-solid decoration can transmit
without being removed; protection still prevents modification.

`strength_left` sums the surviving energy in the active hit records, including
local TNT boosts and excluding distance loss, resistance, blocked paths, cutoffs,
and unloaded space. At a timeout it includes hits still awaiting processing.
There is no separate shared energy budget that can keep blocked paths alive.

## Drop aggregation

The default game groups drops by the full ItemStack identity (name, wear and
metadata). Aggregate counts are stored separately from ItemStack's 16-bit count.
Ejection creates stacks bounded by the item's stack limit and 65,535, preserving
large totals and keeping incompatible items separate. Entity and node callback
drops use the same accumulator.

## Tuning presets

These are explicit call options; the engine default has no core mixing.

| Preset | `blast_core_radius` | `blast_core_shell_fraction` | `blast_tnt_ray_fraction` |
| --- | ---: | ---: | ---: |
| Preserve material shadows | `0` | `0` | `0.8` |
| Strong cubic core, directional outer blast | `3` | `0.9` | `0.8` |
| Spread absorbed TNT across active paths | `3` | `0.9` | `0.2` |

Core destruction still needs enough `blast_strength` to overcome the material.
