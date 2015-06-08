Engine
======

### 0.4.12.7 (dev)
  * Stability and speed fixes
  * liquid_pressure=1 (dev)
  * thread for abm apply
  * optional support old minetest protocol (to host servers for minetest players) to enable compile with -DMINETEST_PROTO=1
  * use bundled msgpack 1.1.0 (incompatible with 0.5.x in some packets where vector<string> used)
  * timelapse=seconds option for automated screenshots
  * set any settings from command line: -key[=[value]]
    ./freeminer -name=herob
    ./freeminer "-name=hero brine"    # value with spaces
    ./freeminer -autojump             # set autojump to 1
    ./freeminer -enable_fog=          # set enable_fog to ""
  * android: gettext and luajit enabled


### 0.4.12.6 (Feb 18, 2015)
  * Stability and speed fixes


### 0.4.11.5 (Dec 25, 2014)
  * Lot of stability and speed fixes
  * use utf8 everywhere

#### Protocol (Incompatible with minetest now)
  * enet based networking (30x+  faster than before, more reliable)
  * msgpack based protocol (easy extendable, better backward compatibility)

#### Settings
  * Storing config to .json
    map_meta.txt will be auto converted to json
    optional for main config (if you want to start use:  `echo {} > freeminer.json`  and start freeminer)

#### Server
  * Added stat viewer tool (shown by issuing `/stat` command)

#### Client
  * Celestial object movement and angle tuned according to position in the world (0,0 is the equator)

#### API
  * Save and load lua tables as json to settings
    core.setting_setjson("tttt", {a1=2, b3=4, c5={e6=7,ff={1,2,3,4}}})
    print("json readed: tttt.b3=" .. core.setting_getjson("tttt")["b3"] .. "   full saved=" .. core.write_json( core.setting_getjson("tttt")))


### 0.4.10.4 (Nov 24, 2014)

  * License switched to GPLv3
  * Start using C++11 features
  * `RUN_IN_PLACE` is now true by default.
  ALL PACKAGE MAINTAINERS NEED TO ADD `-DRUN_IN_PLACE=0` to build scripts
  * Lot of stability fixes

#### Server
  * Basic mod debugging (reports broken recipes) `mod_debugging = 1`
  * Using more threads for server (server (processing packets), env(ABMs, steps, timers), liquid, map (lighting, map save), sendblock),
    removed envlock (lua is still singlethreaded).
    (Server step now 30-50 ms)
    Can be disabled by `more_threads = 0` or cmake: `-DENABLE_THREADS=0` (also disables locking)
  * Lots of speed optimizations
  * Rewritten pathfinder (Selat)
  * Circuit API (Selat)
  * Allow any player names `enable_any_name = 1`
  * Optimized block sending for farther range
  * Various death messages
  * Statistic collector (per player, per server, timed (daily, weekly, monthly))

#### Mapgens
  * Layers added to mapgen v5
  * Indev mapgen - enchanced v6 mapgen, features:
    Farlands -  when moving to edges of map - higher mountains, larger biomes
    Floating islands (starts at ~ 500 , higher - bigger)
    Huge caves
    Water caves
    Underground layers (not only boring stone) config: games/default/mods/default/mapgen.lua : mg_indev = { ...
  * Math mapgen
    Generate various fractal worlds via Mandelbulber ( http://www.mandelbulber.com/ )
    menger_sponge, hypercomplex, xenodreambuie, mandelbox, ...
    examples: http://forum.freeminer.org/threads/math-mapgen.34/
  * v5 mapgen

#### Core support for game features
    Its enabled in freeminer's default game, but other games need to specify appropriate params
  * Real liquids
    Liquids act as liquid: flow down, keep constant volume, can fill holes and caves
    processing up to 50000 nodes per second
  * Weather (now fully adjustable)
    melting, freezing, seasons change, [ugly] snow and rain
  * Hot-cold nodes - can freeze-melt neighbour nodes (eg melt ice around torch/lava/fire/... )
  * sand/dirt/gravel now flows as liquid with one level
  * Drowning in sand/dirt/gravel
  * Liquids drop torches and plants

#### Client
  * Fixed some input issues: not working numpad and some keys in azerty layout
  * Chat forms were replaced with console (opens up to 10% height of the screen)
  * Default keymap for console changed to tilde (~)
  * Farmesh - lose details on far meshes which allows
  rendering up to 1000 blocks (very dev) `farmesh = 2`
  * Unicode support in chat, inputs, player names
  * Minimap (gsmapper) (dev) `hud_map = 1`
  * Save user/pass for every server `password_save = 1`
  * Auto reconnect if connection lost
  * Initial shadows from objects `shadows = 1`

#### Storage
  * API for key-value storage (Selat)
  * Do not save not changed generated blocks (reduce base size 5-20x)
  `save_generated_block = 0`
  * Auto repair broken LevelDB
  * Initial support for LevelDB maintenance.
    Temporarily closes db for backups or mapper tools.
    `killall -HUP freeminerserver; do something; killall -HUP freeminerserver`

#### API
  * new param `fast` improving performance:
    * `core.set_node(pos, node, fast)` (might cause lighting bugs)
    * `core.remove_node(pos, fast)`

#### Game
  * TNT mod
  * Bucket fixes

#### Incompatible with minetest:

  * Players data files saved to LevelDB storage
  * New LevelDB map key format: "a10,-11,12" instead of 64bit number
  (freeminer can read minetest maps, but writes to new format)


### 0.4.9.3 (Jan 22, 2014)

  * Wieldlight. Grab a torch and see the magic. (Zeg9)
  * Texturable sun and moon. (RealBadAngel)
  * /die command
  * Directional fog + horizon colors, based on sun & moon positions
  at sunrise / sunset (MirceaKitsune)
  * Support for colors in chat; API is available for mods
  * LevelDB is now used as default backend whenever available.
  Official Windows build will ship with LevelDB support from now on.
  * Multiple columns in player list (TAB key).
  * Third person view, press F7 to cycle through available modes. (BlockMen)
  * A lot of boring bugfixes, performance improvements and other stuff.


### 0.4.8.2 (Dec 6, 2013)

  * Greater FOV (field of view) when running. (Jeija)
  * Zoom a-la optifine (z key) (Exio4)
  * Force loading world: entities has new force_load attribute,
  when set the engine won't unload blocks near them. (Novatux)
  * New API method `freeminer.swap_node` which preserves node metadata
  while replacing it. (Novatux)
  * New main menu. (xyz)
  * Renamed `minetest.conf` to `freeminer.conf`
  * Renamed default `minetest_game` to `default`


### 0.4.8.0 (Nov 28, 2013)
  * 99% lag-free; optimized server can handle 50-100-... players;
  no problems with laggy mods even on slow hardware
  * Much faster on client (VBO, can eat memory),
  increased view and send range (PilzAdam)
  * Directional fog + horizon colors based on sun & moon positions
  (MirceaKitsune #799 #772)
  * Adjustable dynamic weather and liquids
  (it's possible to define number of liquid levels in node)
  * Weather defined water or ice on map generation.
  (dirt_with_snow instead of dirt_with_grass and frozen oceans when too cold.)
  * Cave trees in huge caves (indev mapgen)
  * Optimized falling (much less mid-air stuck,
  limited max falling speed, more air control at high-speed falling)
  * Optimized headless client (you can run 30-50 bots on one PC)
  * Hell (very hot at -30500), everything melting and burning
  * Slippery (Zeg9 #817)
  * Diagonal Rail (khonkhortisan #528)
  * Improved (re)spawn (don't spawn in stone) (sweetbomber #744)
  * Player list, viewable by holding TAB key (sfan5 #958)
  * Improved math mapgen: more (10+) generators
  from <http://mandelbulber.com>, all params are adjustable
  * And some small bugfixes and improvements.

#### API
  * New node groups: freeze, melt, hot, cold (define temperatures);
  learn how to use them here
  * New node group: slippery
  * `core.register_abm({…, action(…, neighbor)})`
      the action function has a new parameter called neighbor
      which will contain a node that was matched as a neighbor
  * `core.register_abm({…, neighbors_range = 4})`
      maximum radius to search for a neighbor in
      (for example, melting snow in 4×4 area around torch)
  * `core.get_surface` (sapier #640)

#### Game
  * Weather support
  (liquids freeze, melt; plants, trees growing; dirt transforms, ...)
  * Rain, snow
  * Melting stone at high temp (at -30600)
  * Slippery (ice, snow)
  * Biomes for mapgen v7 and math
  * Added sponge:sponge and sponge:iron
  * Weather defined tree and flowers growing
  * Dirt, sand and gravel will fall off the edges
  (try to build something ;)
  * Furnaces burn items even when they are unloaded
  * Moonflower (MirceaKitsune #175)
  * Small fixes
