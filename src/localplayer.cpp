/*
localplayer.cpp
Copyright (C) 2010-2013 celeron55, Perttu Ahola <celeron55@gmail.com>
*/

/*
This file is part of Freeminer.

Freeminer is free software: you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation, either version 3 of the License, or
(at your option) any later version.

Freeminer  is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with Freeminer.  If not, see <http://www.gnu.org/licenses/>.
*/

#include "localplayer.h"

#include "event.h"
#include "collision.h"
#include "gamedef.h"
#include "nodedef.h"
#include "settings.h"
#include "environment.h"
#include "map.h"
#include "util/numeric.h"

#include "log_types.h"

/*
	LocalPlayer
*/

LocalPlayer::LocalPlayer(IGameDef *gamedef, const char *name):
	Player(gamedef, name),
	parent(0),
	isAttached(false),
	overridePosition(v3f(0,0,0)),
	last_position(v3f(0,0,0)),
	last_speed(v3f(0,0,0)),
	last_pitch(0),
	last_yaw(0),
	last_keyPressed(0),
	eye_offset_first(v3f(0,0,0)),
	eye_offset_third(v3f(0,0,0)),
	last_animation(NO_ANIM),
	hotbar_image(""),
	hotbar_selected_image(""),
	light_color(255,255,255,255),
	m_sneak_node(32767,32767,32767),
	m_sneak_node_exists(false),
	m_old_node_below(32767,32767,32767),
	m_old_node_below_type("air"),
	m_need_to_get_new_sneak_node(true),
	m_can_jump(false),
	m_cao(NULL)
{
	// Initialize hp to 0, so that no hearts will be shown if server
	// doesn't support health points
	hp = 0;

}

LocalPlayer::~LocalPlayer()
{
}

void LocalPlayer::move(f32 dtime, Environment *env, f32 pos_max_d,
		std::vector<CollisionInfo> *collision_info)
{
	Map *map = &env->getMap();
	INodeDefManager *nodemgr = m_gamedef->ndef();

	v3f position = getPosition();

	//v3f old_speed = m_speed;

	// Copy parent position if local player is attached
	if(isAttached)
	{
		setPosition(overridePosition);
		m_sneak_node_exists = false;
		return;
	}

	// Skip collision detection if noclip mode is used
	bool fly_allowed = m_gamedef->checkLocalPrivilege("fly");
	bool noclip = m_gamedef->checkLocalPrivilege("noclip") &&
		g_settings->getBool("noclip");
	bool free_move = noclip && fly_allowed && g_settings->getBool("free_move");
	if(free_move)
	{
        position += m_speed * dtime;
		setPosition(position);
		m_sneak_node_exists = false;
		return;
	}

	/*
		Collision detection
	*/

	bool is_valid_position;
	MapNode node;
	v3s16 pp;

	/*
		Check if player is in liquid (the oscillating value)
	*/

	// If in liquid, the threshold of coming out is at higher y
	if (in_liquid)
	{
		// If not in liquid, the threshold of going in is at lower y
		pp = floatToInt(position + v3f(0,BS*(in_liquid ? 0.1 : 0.5),0), BS);
		node = map->getNodeNoEx(pp, &is_valid_position);
		if (is_valid_position) {
			auto f = nodemgr->get(node.getContent());
			in_liquid = f.isLiquid();
			liquid_viscosity = f.liquid_viscosity;
			if (f.param_type_2 == CPT2_LEVELED) {
				float level = node.getLevel(nodemgr);
				float maxlevel = node.getMaxLevel(nodemgr);
				if (level && maxlevel && level < maxlevel)
					liquid_viscosity /= maxlevel / level;
			}
		} else {
			in_liquid = false;
		}

	}
	// If not in liquid, the threshold of going in is at lower y
	else
	{
		pp = floatToInt(position + v3f(0,BS*0.5,0), BS);
		node = map->getNodeNoEx(pp, &is_valid_position);
		if (is_valid_position) {
			in_liquid = nodemgr->get(node.getContent()).isLiquid();
			liquid_viscosity = nodemgr->get(node.getContent()).liquid_viscosity;
		} else {
			in_liquid = false;
		}
	}


	/*
		Check if player is in liquid (the stable value)
	*/
	pp = floatToInt(position + v3f(0,0,0), BS);
	node = map->getNodeNoEx(pp, &is_valid_position);
	if (is_valid_position) {
		in_liquid_stable = nodemgr->get(node.getContent()).isLiquid();
	} else {
		in_liquid_stable = false;
	}

	/*
	        Check if player is climbing
	*/


	pp = floatToInt(position + v3f(0,0.5*BS,0), BS);
	v3s16 pp2 = floatToInt(position + v3f(0,-0.2*BS,0), BS);
	node = map->getNodeNoEx(pp, &is_valid_position);
	bool is_valid_position2;
	MapNode node2 = map->getNodeNoEx(pp2, &is_valid_position2);

	if (!(is_valid_position && is_valid_position2)) {
		is_climbing = false;
	} else {
		bool can_climbing = (nodemgr->get(node.getContent()).climbable
				|| nodemgr->get(node2.getContent()).climbable) && !free_move;
		if (m_speed.Y >= -PLAYER_FALL_TOLERANCE_SPEED)
			is_climbing = can_climbing;
		else if (can_climbing)
			m_speed.Y += 0.3*BS;
	}


	/*
		Collision uncertainty radius
		Make it a bit larger than the maximum distance of movement
	*/
	//f32 d = pos_max_d * 1.1;
	// A fairly large value in here makes moving smoother
	//f32 d = 0.15*BS;

	// This should always apply, otherwise there are glitches
	//sanity_check(d > pos_max_d);

	// Maximum distance over border for sneaking
	f32 sneak_max = BS*0.4;

	/*
		If sneaking, keep in range from the last walked node and don't
		fall off from it
	*/
	if(control.sneak && m_sneak_node_exists &&
			!(fly_allowed && g_settings->getBool("free_move")) && !in_liquid &&
			physics_override_sneak)
	{
		f32 maxd = 0.5*BS + sneak_max;
		v3f lwn_f = intToFloat(m_sneak_node, BS);
		auto old_pos = position;
		position.X = rangelim(position.X, lwn_f.X-maxd, lwn_f.X+maxd);
		position.Z = rangelim(position.Z, lwn_f.Z-maxd, lwn_f.Z+maxd);

		if (old_pos != position) {
			m_speed.X = rangelim(m_speed.X, -movement_speed_climb, movement_speed_climb);
			m_speed.Z = rangelim(m_speed.Z, -movement_speed_climb, movement_speed_climb);
		}

		if(!is_climbing)
		{
			f32 min_y = lwn_f.Y + 0.5*BS;
			if(position.Y < min_y && m_speed.Y >= -PLAYER_FALL_TOLERANCE_SPEED)
			{
				position.Y = min_y;

				if(m_speed.Y < 0)
					m_speed.Y = 0;
			}

			if (m_speed.Y < -PLAYER_FALL_TOLERANCE_SPEED) {
				m_speed.Y += 0.3*BS;
			}

		}
	}

	// this shouldn't be hardcoded but transmitted from server
	float player_stepheight = touching_ground ? (BS*0.6) : (BS*0.2);

	if (control.aux1 || g_settings->getBool("autojump")) {
		player_stepheight += (0.5 * BS);
	}

	v3f accel_f = v3f(0,0,0);

	collisionMoveResult result = collisionMoveSimple(env, m_gamedef,
			pos_max_d, m_collisionbox, player_stepheight, dtime,
			position, m_speed, accel_f);

	/*
		If the player's feet touch the topside of any node, this is
		set to true.

		Player is allowed to jump when this is true.
	*/
	bool touching_ground_was = touching_ground;
	touching_ground = result.touching_ground;

    //bool standing_on_unloaded = result.standing_on_unloaded;

	/*
		Check the nodes under the player to see from which node the
		player is sneaking from, if any.  If the node from under
		the player has been removed, the player falls.
	*/
	v3s16 current_node = floatToInt(position - v3f(0,BS/2,0), BS);
	if(m_sneak_node_exists &&
	   nodemgr->get(map->getNodeNoEx(m_old_node_below)).name == "air" &&
	   m_old_node_below_type != "air")
	{
		// Old node appears to have been removed; that is,
		// it wasn't air before but now it is
		m_need_to_get_new_sneak_node = false;
		m_sneak_node_exists = false;
	}
	else if(nodemgr->get(map->getNodeNoEx(current_node)).name != "air")
	{
		// We are on something, so make sure to recalculate the sneak
		// node.
		m_need_to_get_new_sneak_node = true;
	}
	if(m_need_to_get_new_sneak_node && physics_override_sneak)
	{
		v3s16 pos_i_bottom = floatToInt(position - v3f(0,BS/2,0), BS);
		v2f player_p2df(position.X, position.Z);
		f32 min_distance_f = 100000.0*BS;
		// If already seeking from some node, compare to it.
		/*if(m_sneak_node_exists)
		{
			v3f sneaknode_pf = intToFloat(m_sneak_node, BS);
			v2f sneaknode_p2df(sneaknode_pf.X, sneaknode_pf.Z);
			f32 d_horiz_f = player_p2df.getDistanceFrom(sneaknode_p2df);
			f32 d_vert_f = fabs(sneaknode_pf.Y + BS*0.5 - position.Y);
			// Ignore if player is not on the same level (likely dropped)
			if(d_vert_f < 0.15*BS)
				min_distance_f = d_horiz_f;
		}*/
		v3s16 new_sneak_node = m_sneak_node;
		for(s16 x=-1; x<=1; x++)
		for(s16 z=-1; z<=1; z++)
		{
			v3s16 p = pos_i_bottom + v3s16(x,0,z);
			v3f pf = intToFloat(p, BS);
			v2f node_p2df(pf.X, pf.Z);
			f32 distance_f = player_p2df.getDistanceFrom(node_p2df);
			f32 max_axis_distance_f = MYMAX(
					fabs(player_p2df.X-node_p2df.X),
					fabs(player_p2df.Y-node_p2df.Y));

			if(distance_f > min_distance_f ||
					max_axis_distance_f > 0.5*BS + sneak_max + 0.1*BS)
				continue;


			// The node to be sneaked on has to be walkable
			node = map->getNodeNoEx(p, &is_valid_position);
			if (!is_valid_position || nodemgr->get(node).walkable == false)
				continue;
			// And the node above it has to be nonwalkable
			node = map->getNodeNoEx(p + v3s16(0,1,0), &is_valid_position);
			if (!is_valid_position || nodemgr->get(node).walkable) {
				continue;
			}
			if (!physics_override_sneak_glitch) {
				node =map->getNodeNoEx(p + v3s16(0,2,0), &is_valid_position);
				if (!is_valid_position || nodemgr->get(node).walkable)
					continue;
			}

			min_distance_f = distance_f;
			new_sneak_node = p;
		}

		bool sneak_node_found = (min_distance_f < 100000.0*BS*0.9);

		m_sneak_node = new_sneak_node;
		m_sneak_node_exists = sneak_node_found;

		/*
			If sneaking, the player's collision box can be in air, so
			this has to be set explicitly
		*/
		if(sneak_node_found && control.sneak)
			touching_ground = true;
	}

	/*
		Set new position
	*/
	setPosition(position);

	/*
		Report collisions
	*/
	bool bouncy_jump = false;
	// Dont report if flying
	if(collision_info && !(g_settings->getBool("free_move") && fly_allowed)) {
		for(size_t i=0; i<result.collisions.size(); i++) {
			const CollisionInfo &info = result.collisions[i];
			collision_info->push_back(info);
			if(info.new_speed.Y - info.old_speed.Y > 0.1*BS &&
					info.bouncy)
				bouncy_jump = true;
		}
	}

	if(bouncy_jump && control.jump){
		m_speed.Y += movement_speed_jump*BS;
		touching_ground = false;
		MtEvent *e = new SimpleTriggerEvent("PlayerJump");
		m_gamedef->event()->put(e);
	}

	if(!touching_ground_was && touching_ground){
		MtEvent *e = new SimpleTriggerEvent("PlayerRegainGround");
		m_gamedef->event()->put(e);

		// Set camera impact value to be used for view bobbing
		camera_impact = getSpeed().Y * -1;
	}

	{
		camera_barely_in_ceiling = false;
		v3s16 camera_np = floatToInt(getEyePosition(), BS);
		MapNode n = map->getNodeNoEx(camera_np);
		if(n.getContent() != CONTENT_IGNORE){
			if(nodemgr->get(n).walkable && nodemgr->get(n).solidness == 2){
				camera_barely_in_ceiling = true;
			}
		}
	}

	/*
		Update the node last under the player
	*/
	m_old_node_below = floatToInt(position - v3f(0,BS/2,0), BS);
	m_old_node_below_type = nodemgr->get(map->getNodeNoEx(m_old_node_below)).name;

	/*
		Check properties of the node on which the player is standing
	*/
	const ContentFeatures &f = nodemgr->get(map->getNodeNoEx(getStandingNodePos()));
	// Determine if jumping is possible
	m_can_jump = touching_ground && !in_liquid;
	if(itemgroup_get(f.groups, "disable_jump"))
		m_can_jump = false;
}

void LocalPlayer::move(f32 dtime, Environment *env, f32 pos_max_d)
{
	move(dtime, env, pos_max_d, NULL);
}

bool LocalPlayer::canPlaceNode(const v3s16& p, const MapNode& n)
{
	bool noclip = m_gamedef->checkLocalPrivilege("noclip") &&
		g_settings->getBool("noclip");
	// Dont place node when player would be inside new node
	// NOTE: This is to be eventually implemented by a mod as client-side Lua

	if (m_gamedef->ndef()->get(n).walkable && !noclip && !g_settings->getBool("enable_build_where_you_stand")) {
		auto nodeboxes = n.getNodeBoxes(m_gamedef->ndef());
		aabb3f player_box = m_collisionbox;
		v3f position(getPosition());
		v3f node_pos(p.X, p.Y, p.Z);
		v3f center = player_box.getCenter();
		v3f min_edge = (player_box.MinEdge - center) * 0.999f;
		v3f max_edge = (player_box.MaxEdge - center) * 0.999f;
		player_box.MinEdge = center + min_edge;
		player_box.MaxEdge = center + max_edge;
		player_box.MinEdge += position;
		player_box.MaxEdge += position;
		for(auto box : nodeboxes) {
			box.MinEdge += node_pos * BS;
			box.MaxEdge += node_pos * BS;
			if(box.intersectsWithBox(player_box)) {
				return false;
			}
		}
	}
	return true;
}

void LocalPlayer::applyControl(float dtime, ClientEnvironment *env)
{
	// Clear stuff
	swimming_vertical = false;

	setPitch(control.pitch);
	setYaw(control.yaw);

	// Nullify speed and don't run positioning code if the player is attached
	if(isAttached)
	{
		setSpeed(v3f(0,0,0));
		return;
	}

	v3f move_direction = v3f(0,0,1);
	move_direction.rotateXZBy(getYaw());

	v3f speedH = v3f(0,0,0); // Horizontal (X, Z)
	v3f speedV = v3f(0,0,0); // Vertical (Y)

	bool fly_allowed = m_gamedef->checkLocalPrivilege("fly");
	bool fast_allowed = m_gamedef->checkLocalPrivilege("fast");

	free_move = fly_allowed && g_settings->getBool("free_move");
	bool fast_move = fast_allowed && g_settings->getBool("fast_move");
	// When aux1_descends is enabled the fast key is used to go down, so fast isn't possible
	bool fast_climb = fast_move && control.aux1 && !g_settings->getBool("aux1_descends");
	bool continuous_forward = g_settings->getBool("continuous_forward");
	bool fast_pressed = false;
	// Whether superspeed mode is used or not
	superspeed = false;

	if(g_settings->getBool("always_fly_fast") && free_move && fast_move)
		superspeed = true;

	// Old descend control
	if(g_settings->getBool("aux1_descends"))
	{
		// If free movement and fast movement, always move fast
		if(free_move && fast_move)
			superspeed = true;

		// Auxiliary button 1 (E)
		if(control.aux1)
		{
			if(free_move)
			{
				// In free movement mode, aux1 descends
				if(fast_move)
					speedV.Y = -movement_speed_fast;
				else
					speedV.Y = -movement_speed_walk;
			}
			else if(in_liquid || in_liquid_stable)
			{
				speedV.Y = -movement_speed_walk;
				swimming_vertical = true;
			}
			else if(is_climbing)
			{
				speedV.Y = -movement_speed_climb;
			}
			else
			{
				// If not free movement but fast is allowed, aux1 is
				// "Turbo button"
				if(fast_allowed)
					superspeed = true;
			}
		}
	}
	// New minecraft-like descend control
	else
	{
		// Auxiliary button 1 (E)
		if(control.aux1)
		{
			if(!is_climbing)
			{
				// aux1 is "Turbo button"
				if(fast_allowed)
					superspeed = true;
			}
			if(fast_allowed)
				fast_pressed = true;
		}

		if(control.sneak)
		{
			if(free_move)
			{
				// In free movement mode, sneak descends
				if(fast_move && (control.aux1 || g_settings->getBool("always_fly_fast")))
					speedV.Y = -movement_speed_fast;
				else
					speedV.Y = -movement_speed_walk;
			}
			else if(in_liquid || in_liquid_stable)
			{
				if(fast_climb)
					speedV.Y = -movement_speed_fast;
				else
					speedV.Y = -movement_speed_walk;
				swimming_vertical = true;
			}
			else if(is_climbing)
			{
				if(fast_climb)
					speedV.Y = -movement_speed_fast;
				else
					speedV.Y = -movement_speed_climb;
			}
		}
	}

	if(continuous_forward)
		speedH += move_direction;

	if(control.up)
	{
		if(continuous_forward)
			superspeed = true;
		else
			speedH += move_direction;
	}
	if(control.down)
	{
		speedH -= move_direction;
	}
	if(control.left)
	{
		speedH += move_direction.crossProduct(v3f(0,1,0));
	}
	if(control.right)
	{
		speedH += move_direction.crossProduct(v3f(0,-1,0));
	}
	if(control.jump)
	{
		if(free_move)
		{
			if(g_settings->getBool("aux1_descends") || g_settings->getBool("always_fly_fast"))
			{
				if(fast_move)
					speedV.Y = movement_speed_fast;
				else
					speedV.Y = movement_speed_walk;
			} else {
				if(fast_move && control.aux1)
					speedV.Y = movement_speed_fast;
				else
					speedV.Y = movement_speed_walk;
			}
		}
		else if(m_can_jump)
		{
			/*
				NOTE: The d value in move() affects jump height by
				raising the height at which the jump speed is kept
				at its starting value
			*/
			v3f speedJ = getSpeed();
			if(speedJ.Y >= -0.5 * BS)
			{
				speedJ.Y = movement_speed_jump * physics_override_jump;
				setSpeed(speedJ);

				MtEvent *e = new SimpleTriggerEvent("PlayerJump");
				m_gamedef->event()->put(e);
			}
		}
		else if(in_liquid)
		{
			if(fast_climb)
				speedV.Y = movement_speed_fast;
			else
				speedV.Y = movement_speed_walk;
			swimming_vertical = true;
		}
		else if(is_climbing)
		{
			if(fast_climb)
				speedV.Y = movement_speed_fast;
			else
				speedV.Y = movement_speed_climb;
		}
	}

	// The speed of the player (Y is ignored)
	if(superspeed || (is_climbing && fast_climb) || ((in_liquid || in_liquid_stable) && fast_climb) || fast_pressed)
		speedH = speedH.normalize() * movement_speed_fast;
	else if(control.sneak && !free_move && !in_liquid && !in_liquid_stable)
		speedH = speedH.normalize() * movement_speed_crouch;
	else
		speedH = speedH.normalize() * movement_speed_walk;

	// Acceleration increase
	f32 incH = 0; // Horizontal (X, Z)
	f32 incV = 0; // Vertical (Y)
	if((!touching_ground && !free_move && !is_climbing && !in_liquid) || (!free_move && m_can_jump && control.jump))
	{
		// Jumping and falling
		if(superspeed || (fast_move && control.aux1))
			incH = movement_acceleration_fast * BS * dtime;
		else
			incH = movement_acceleration_air * BS * dtime;
		incV = 0; // No vertical acceleration in air

		// better air control when falling fast
		float speed = m_speed.getLength();
		if (!superspeed && speed > movement_speed_fast && (control.down || control.up || control.left || control.right)) {
			v3f rotate = move_direction * (speed / (BS * 110)); // 80 - good wingsuit
			if(control.up)		rotate = rotate.crossProduct(v3f(0,1,0));
			if(control.down)	rotate = rotate.crossProduct(v3f(0,-1,0));
			if(control.left)	rotate *=-1;
			m_speed.rotateYZBy(rotate.X);
			m_speed.rotateXZBy(rotate.Y);
			m_speed.rotateXYBy(rotate.Z);
			m_speed = m_speed.normalize() * speed * (1-speed*0.00001); // 0.998
			if (m_speed.Y)
				return;
		}
	}
	else if (superspeed || (is_climbing && fast_climb) || ((in_liquid || in_liquid_stable) && fast_climb))
		incH = incV = movement_acceleration_fast * BS * dtime;
	else
		incH = incV = movement_acceleration_default * BS * dtime;

	// Accelerate to target speed with maximum increment
	INodeDefManager *nodemgr = m_gamedef->ndef();
	Map *map = &env->getMap();
	v3s16 p = floatToInt(getPosition() - v3f(0,BS/2,0), BS);
	float slippery = 0;
	try {
		slippery = itemgroup_get(nodemgr->get(map->getNode(p)).groups, "slippery");
	}
	catch (...) {}
	accelerateHorizontal(speedH * physics_override_speed, incH * physics_override_speed, slippery);
	accelerateVertical(speedV * physics_override_speed, incV * physics_override_speed);
}

v3s16 LocalPlayer::getStandingNodePos()
{
	if(m_sneak_node_exists)
		return m_sneak_node;
	return floatToInt(getPosition() - v3f(0, BS, 0), BS);
}
