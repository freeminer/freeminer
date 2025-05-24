// Luanti
// SPDX-License-Identifier: LGPL-2.1-or-later
// Copyright (C) 2013 celeron55, Perttu Ahola <celeron55@gmail.com>

#pragma once

#include "irrlichttypes_bloated.h"
#include <vector>

class Map;
class IGameDef;
class Environment;
class ActiveObject;

enum CollisionType : u8
{
	COLLISION_NODE,
	COLLISION_OBJECT,
};

enum CollisionAxis : s8
{
	COLLISION_AXIS_NONE = -1,
	COLLISION_AXIS_X,
	COLLISION_AXIS_Y,
	COLLISION_AXIS_Z,
};

struct CollisionInfo
{
	CollisionInfo() = default;

	CollisionType type = COLLISION_NODE;
	CollisionAxis axis = COLLISION_AXIS_NONE;
	v3pos_t node_p = v3pos_t(-32768,-32768,-32768); // COLLISION_NODE
	ActiveObject *object = nullptr; // COLLISION_OBJECT
	v3opos_t new_pos;
	v3f old_speed;
	v3f new_speed;
};

struct collisionMoveResult
{
	collisionMoveResult() = default;

	bool collides = false;
	bool touching_ground = false;
	bool standing_on_object = false;
	std::vector<CollisionInfo> collisions;
};

/// Status if any problems were ever encountered during collision detection.
/// @warning For unit test use only.
extern bool g_collision_problems_encountered;

/// @param self (optional) ActiveObject to ignore in the collision detection.
collisionMoveResult collisionMoveSimple(Environment *env, IGameDef *gamedef,
		const aabb3f &box_0,
		f32 stepheight, f32 dtime,
		v3opos_t *pos_f, v3f *speed_f,
		v3f accel_f, ActiveObject *self=NULL,
		bool collide_with_objects=true);

/// @brief A simpler version of "collisionMoveSimple" that only checks whether
///        a collision occurs at the given position.
/// @param self (optional) ActiveObject to ignore in the collision detection.
/// @returns `true` when `box_0` truly intersects with a node or object.
///          Touching faces are not counted as intersection.
bool collision_check_intersection(Environment *env, IGameDef *gamedef,
		const aabb3f &box_0, const v3opos_t &pos_f, ActiveObject *self = nullptr,
		bool collide_with_objects = true);

// Helper function:
// Checks for collision of a moving aabbox with a static aabbox
// Returns -1 if no collision, 0 if X collision, 1 if Y collision, 2 if Z collision
// dtime receives time until first collision, invalid if -1 is returned
CollisionAxis axisAlignedCollision(
		const aabb3o &staticbox, const aabb3o &movingbox,
		v3f speed, f32 *dtime);

// Helper function:
// Checks if moving the movingbox up by the given distance would hit a ceiling.
bool wouldCollideWithCeiling(
		const std::vector<aabb3o> &staticboxes,
		const aabb3o &movingbox,
		f32 y_increase, f32 d);
