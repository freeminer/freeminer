local nodes = {}
local function key(p) return p.x .. ',' .. p.y .. ',' .. p.z end
vector = {}
function vector.new(x,y,z) return {x=x,y=y,z=z} end
function vector.zero() return vector.new(0,0,0) end
function vector.offset(p,x,y,z) return vector.new(p.x+x,p.y+y,p.z+z) end
function vector.add(a,b) return vector.offset(a,b.x,b.y,b.z) end
function vector.round(p) return vector.new(math.floor(p.x+0.5),math.floor(p.y+0.5),math.floor(p.z+0.5)) end
function vector.copy(p) return vector.new(p.x,p.y,p.z) end
local spawned, removed = 0, 0
core = { settings = {get=function() return nil end}, registered_nodes = {
 air={walkable=false,buildable_to=true,liquidtype='none'},
 stone={walkable=true,buildable_to=false,liquidtype='none'},
 sand={walkable=true,buildable_to=false,liquidtype='none',groups={falling_node=1}},
 torch={walkable=false,buildable_to=false,liquidtype='none',groups={attached_node=1}},
}, registered_items = {}, registered_on_dignodes = {} }
minetest=core
function core.get_node(p) return nodes[key(p)] or {name='air',param2=0} end
core.get_node_or_nil=core.get_node
function core.get_item_group(name,group) local d=core.registered_nodes[name]; return d and d.groups and d.groups[group] or 0 end
function core.spawn_falling_node(p) spawned=spawned+1; nodes[key(p)]={name='air'}; return true, {} end
function core.remove_node(p) removed=removed+1; nodes[key(p)]={name='air'} end
function core.get_node_drops() return {} end
function core.register_on_placenode() end
function core.register_on_dignode() end
function core.register_on_punchnode() end
assert(loadfile('builtin/game/falling.lua'))({})
nodes['0,1,0']={name='sand',param2=0}
assert(core.check_single_for_falling({x=0,y=1,z=0}) == true and spawned == 1)
nodes['1,1,0']={name='sand',param2=0}
assert(core.check_single_for_falling({x=0,y=0,z=0}) == false and spawned == 1)
core.check_for_falling({x=1,y=0,z=0})
assert(spawned == 2)
nodes['4,1,0']={name='torch',param2=0}
assert(core.check_single_for_falling({x=4,y=1,z=0}) == true and removed == 1)
core.registered_nodes.snow = {walkable=true,buildable_to=false,liquidtype='none',paramtype2='leveled',groups={falling_node=1}}
nodes['8,0,0']={name='snow',param2=1}
nodes['8,1,0']={name='snow',param2=1}
function core.get_node_level() return 1 end
function core.get_node_max_level() return 8 end
local timer_started = false
function core.get_node_timer() return {is_started=function() return timer_started end} end
local node_meta = {fields={owner='alice'},inventory={}}
function core.get_meta() return {to_table=function() return node_meta end} end
assert(core.check_single_for_falling({x=8,y=1,z=0}) == false and spawned == 2)
node_meta = {fields={},inventory={main={'item'}}}
assert(core.check_single_for_falling({x=8,y=1,z=0}) == false and spawned == 2)
node_meta = {fields={},inventory={}}
timer_started = true
assert(core.check_single_for_falling({x=8,y=1,z=0}) == false and spawned == 2)
timer_started = false
assert(core.check_single_for_falling({x=8,y=1,z=0}) == true and spawned == 3)
print('falling Lua checks passed')
