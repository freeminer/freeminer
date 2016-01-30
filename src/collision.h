/*
collision.h
Copyright (C) 2013 celeron55, Perttu Ahola <celeron55@gmail.com>
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

#ifndef COLLISION_HEADER
#define COLLISION_HEADER

#include "irrlichttypes_bloated.h"
#include <vector>

class Map;
class IGameDef;
class Environment;
class ActiveObject;

enum CollisionType
{
	COLLISION_NODE,
	COLLISION_OBJECT,
};

struct CollisionInfo
{
	enum CollisionType type;
	v3s16 node_p; // COLLISION_NODE
	bool bouncy;
	v3f old_speed;
	v3f new_speed;

	CollisionInfo():
		type(COLLISION_NODE),
		node_p(-32768,-32768,-32768),
		bouncy(false),
		old_speed(0,0,0),
		new_speed(0,0,0)
	{}
};

struct collisionMoveResult
{
	bool touching_ground;
	bool collides;
	bool collides_xz;
	bool standing_on_unloaded;
	bool standing_on_object;
	std::vector<CollisionInfo> collisions;

	collisionMoveResult():
		touching_ground(false),
		collides(false),
		collides_xz(false),
		standing_on_unloaded(false),
		standing_on_object(false)
	{}
};

// Moves using a single iteration; speed should not exceed pos_max_d/dtime
collisionMoveResult collisionMoveSimple(Environment *env,IGameDef *gamedef,
		f32 pos_max_d, const aabb3f &box_0,
		f32 stepheight, f32 dtime,
		v3f *pos_f, v3f *speed_f,
		v3f accel_f, ActiveObject *self=NULL,
		bool collideWithObjects=true);

// Helper function:
// Checks for collision of a moving aabbox with a static aabbox
// Returns -1 if no collision, 0 if X collision, 1 if Y collision, 2 if Z collision
// dtime receives time until first collision, invalid if -1 is returned
int axisAlignedCollision(
		const aabb3f &staticbox, const aabb3f &movingbox,
		const v3f &speed, f32 d, f32 *dtime);

// Helper function:
// Checks if moving the movingbox up by the given distance would hit a ceiling.
bool wouldCollideWithCeiling(
		const std::vector<aabb3f> &staticboxes,
		const aabb3f &movingbox,
		f32 y_increase, f32 d);


#endif

