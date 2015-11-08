-- Minetest: builtin/constants.lua

--
-- Constants values for use with the Lua API
--

-- Built-in Content IDs (for use with VoxelManip API)
core.CONTENT_UNKNOWN = 125
core.CONTENT_AIR     = 126
core.CONTENT_IGNORE  = 127

-- Block emerge status constants (for use with core.emerge_area)
core.EMERGE_CANCELLED   = 0
core.EMERGE_ERRORED     = 1
core.EMERGE_FROM_MEMORY = 2
core.EMERGE_FROM_DISK   = 3
core.EMERGE_GENERATED   = 4
