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
