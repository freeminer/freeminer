core.register_tool("testtools:particle_spawner", {
	description = "Particle Spawner".."\n"..
		"Punch: Spawn random test particle",
	inventory_image = "testtools_particle_spawner.png",
	groups = { testtool = 1, disable_repair = 1 },
	on_use = function(itemstack, user, pointed_thing)
		local pos = core.get_pointed_thing_position(pointed_thing, true)
		if pos == nil then
			if user then
				pos = user:get_pos()
			end
		end
		pos = vector.add(pos, {x=0, y=0.5, z=0})
		local tex, anim
		if math.random(0, 1) == 0 then
			tex = "testtools_particle_sheet.png"
			anim = {type="sheet_2d", frames_w=3, frames_h=2, frame_length=0.5}
		else
			tex = "testtools_particle_vertical.png"
			anim = {type="vertical_frames", aspect_w=16, aspect_h=16, length=3.3}
		end

		core.add_particle({
			pos = pos,
			velocity = {x=0, y=0, z=0},
			acceleration = {x=0, y=0.04, z=0},
			expirationtime = 6,
			collisiondetection = true,
			texture = tex,
			animation = anim,
			size = 4,
			glow = math.random(0, 5),
		})
	end,
})

