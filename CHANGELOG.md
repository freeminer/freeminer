# Engine

### 0.4.10.4 (dev)

  - license switched to GPLv3
  - start using c++11 features
  - server: lot of speed optimizations
  - real liquids
  - real liquid optimized: processing up to 150000 nodes per second
  - weather (now fully adjustable): melting, freezing, seasons change, [ugly] snow and rain
  - rewrited pathfinder (Selat)
  - api for key-value storage (Selat)
  - circuit (Selat)
  - auto reconnect if connection lost
  - do not save not changed generated blocks (reduce base size 5-20x) [save_generated_block = 0]
  - farmesh - lost detail on far blocks, allow to view landscape with 500-1000-+ range (very dev) [farmesh = 2]
  - Save user/pass for every server [password_save = 1]
  - Unicode support in chat, inputs, player names
  - server: enable_any_name=0 # allow any player names
  - optimized block sending - farther range
  - Various death messages
  - Mandelbulber fractal generator included for math mapgen
  - lot of stability fixes
  - DRUN_IN_PLACE=1 now by default ALL PACKAGE MAINTAINERS NEED TO ADD -DRUN_IN_PLACE=0 to build scripts
  - Added map thread (liquid, lighting, map save) - server step closer to 100 ms
  - client: Removed vmanip from mesh making - adds some fps
  - Auto repair broken leveldb db
  - Math and indev mapgens
  - v5 mapgen
  - minimap (gsmapper) (dev)
  - drowning in sand/dirt/gravel
  - initial shadows from objects [shadows = 1]
  - Using more threads for server (server, env, liquid, map, sendblock), removed envlock. (but lua still singlethreaded) can disable in config: [more_threads = 0] or cmake: -DDISABLE_THREADS=1

#### incompatible with minetest changes:
  - players data files saved to leveldb storage
  - new leveldb map key format: "a10,-11,12" instead of 64bit number (freeminer can read minetest maps, but writes to new format)

### 0.4.9.3

  - Wieldlight. Grab a torch and see the magic. (Zeg9)
  - Texturable sun and moon. (RealBadAngel)
  - /die command
  - Directional fog + horizon colors, based on sun & moon positions at sunrise / sunset (MirceaKitsune)
  - Support for colors in chat; API is available for mods
  - LevelDB is now used by default as map storage backend whenever available. Official Windows build will ship with LevelDB support from now on.
  - Multiple columns in player list (TAB key).
  - Third person view, press F7 to cycle through available modes. (BlockMen)
  - A lot of boring bugfixes, performance improvements and other stuff.

### 0.4.8.2

  - Greater FOV (field of view) when running. (Jeija)
  - Zoom a-la optifine (z key) (Exio4)
  - Force loading world: entities has new force_load attribute, when set the engine won't unload blocks near them. (Novatux)
  - New API method freeminer.swap_node which preserves node metadata while replacing it. (Novatux)
  - New main menu. (xyz)
  - Renamed minetest.conf to freeminer.conf
  - Renamed default minetest_game to default
-- 0.4.8.0
  - 99% lag-free; optimized server can handle 50-100-... players; no problems with laggy mods even on slow hardware
  - Much faster on client (VBO, can eat memory), increased view and send range (PilzAdam)
  - Directional fog + horizon colors based on sun & moon positions (MirceaKitsune #799 #772)
  - Adjustable dynamic weather and liquids (it's possible to define number of liquid levels in node)
  - Weather defined water or ice on map generation. (freezed oceans if low temperature) and dirt_with_snow instead of dirt_with_grass when too cold.
  - Cave trees in huge caves (indev mapgen)
  - Optimized falling (much less mid-air stuck, limited max falling speed, more air control at high-speed falling)
  - Fixed and optimized headless client (you can run 30-50 bots on one PC)
  - Hell (very hot at -30500), everything melting and burning
  - Slippery (Zeg9 #817)
  - Diagonal Rail (khonkhortisan #528)
  - Improved (re)spawn (don't spawn in stone) (sweetbomber #744)
  - Player list, viewable by holding TAB key (sfan5 #958)
  - Improved math mapgen: more (10+)generators from http://mandelbulber.com/, all params are adjustable
  - And some small bugfixes and improvements.

***************
## API changes

### 0.4.10.4 (dev)

  - minetest.set_node(pos, node, fast) - new fast param. may caue light bugs, but fast
  - minetest.remove_node(pos, fast) - fast remove param
### 0.4.8.0

  - New node groups: freeze, melt, hot, cold (define temperatures); learn how to use them here
  - New node group: slippery
  - minetest.register_abm({…, action(…, neighbor)})
      the action function has a new parameter called neighbor which will contain a node that was matched as a neighbor
  - minetest.register_abm({…, neighbors_range = 4})
      maximum radius to search for a neighbor in (for example, melting snow in 4×4 area around torch)
  - minetest.get_surface (sapier #640)

****************
## Game changes

### 0.4.10.4 (dev)

  - TNT mod
  - Bucket fixes
  - Drop torches, plants from liquid

### 0.4.8.0

  - Weather support (liquids freeze,melt; plants,trees growing; dirt transforms, ...)
  - Rain, snow
  - Melting stone at high temp (at -30600)
  - Slippery (ice, snow)
  - Biomes for mapgen v7 and math
  - sponge:sponge sponge:iron
  - Weather defined tree and flowers growing
  - Dirt, sand and gravel will fall off the edges (try to build something ;)
  - Furnaces burn items even when they are unloaded
  - Moonflower (MirceaKitsune #175)
  - Small fixes