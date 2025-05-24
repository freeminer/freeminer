// Luanti
// SPDX-License-Identifier: LGPL-2.1-or-later
// Copyright (C) 2013 celeron55, Perttu Ahola <celeron55@gmail.com>

#include "collision.h"
#include <cmath>
#include "mapblock.h"
#include "map.h"
#include "nodedef.h"
#include "gamedef.h"
#include "util/numeric.h"
#if CHECK_CLIENT_BUILD()
#include "client/clientenvironment.h"
#include "client/localplayer.h"
#endif
#include "serverenvironment.h"
#include "server/serveractiveobject.h"
#include "util/timetaker.h"
#include "profiler.h"

#ifdef __FAST_MATH__
#warning "-ffast-math is known to cause bugs in collision code, do not use!"
#endif

bool g_collision_problems_encountered = false;

namespace {

struct NearbyCollisionInfo {
	// node
	NearbyCollisionInfo(bool is_ul, int bouncy, v3pos_t pos, const aabb3o &box) :
		obj(nullptr),
		box(box),
		position(pos),
		bouncy(bouncy),
		is_unloaded(is_ul),
		is_step_up(false)
	{}

	// object
	NearbyCollisionInfo(ActiveObject *obj, int bouncy, const aabb3o &box) :
		obj(obj),
		box(box),
		bouncy(bouncy),
		is_unloaded(false),
		is_step_up(false)
	{}

	inline bool isObject() const { return obj != nullptr; }

	ActiveObject *obj;
	aabb3o box;
	v3pos_t position;
	u8 bouncy;
	// bitfield to save space
	bool is_unloaded:1, is_step_up:1;
};

// Helper functions:
// Truncate floating point numbers to specified number of decimal places
// in order to move all the floating point error to one side of the correct value
inline f32 truncate(const f32 val, const f32 factor)
{
	return truncf(val * factor) / factor;
}

inline v3f truncate(const v3f vec, const f32 factor)
{
	return v3f(
		truncate(vec.X, factor),
		truncate(vec.Y, factor),
		truncate(vec.Z, factor)
	);
}

}

// Helper function:
// Checks for collision of a moving aabbox with a static aabbox
// Returns -1 if no collision, 0 if X collision, 1 if Y collision, 2 if Z collision
// The time after which the collision occurs is stored in dtime.
CollisionAxis axisAlignedCollision(
		const aabb3o &staticbox, const aabb3o &movingbox,
		const v3f speed, f32 *dtime)
{
	//TimeTaker tt("axisAlignedCollision");

	aabb3o relbox(
			(movingbox.MaxEdge.X - movingbox.MinEdge.X) + (staticbox.MaxEdge.X - staticbox.MinEdge.X),						// sum of the widths
			(movingbox.MaxEdge.Y - movingbox.MinEdge.Y) + (staticbox.MaxEdge.Y - staticbox.MinEdge.Y),
			(movingbox.MaxEdge.Z - movingbox.MinEdge.Z) + (staticbox.MaxEdge.Z - staticbox.MinEdge.Z),
			std::max(movingbox.MaxEdge.X, staticbox.MaxEdge.X) - std::min(movingbox.MinEdge.X, staticbox.MinEdge.X),	//outer bounding 'box' dimensions
			std::max(movingbox.MaxEdge.Y, staticbox.MaxEdge.Y) - std::min(movingbox.MinEdge.Y, staticbox.MinEdge.Y),
			std::max(movingbox.MaxEdge.Z, staticbox.MaxEdge.Z) - std::min(movingbox.MinEdge.Z, staticbox.MinEdge.Z)
	);

	const f32 dtime_max = *dtime;
	f32 inner_margin;		// the distance of clipping recovery
	f32 distance;
	f32 time;


	if (speed.Y) {
		distance = relbox.MaxEdge.Y - relbox.MinEdge.Y;
		*dtime = distance / std::abs(speed.Y);
		time = std::max(*dtime, 0.0f);

		if (*dtime <= dtime_max) {
			inner_margin = std::max<opos_t>(-0.5f * (staticbox.MaxEdge.Y - staticbox.MinEdge.Y), -2.0f);

			if ((speed.Y > 0 && staticbox.MinEdge.Y - movingbox.MaxEdge.Y > inner_margin) ||
				(speed.Y < 0 && movingbox.MinEdge.Y - staticbox.MaxEdge.Y > inner_margin)) {
				if (
					(std::max(movingbox.MaxEdge.X + speed.X * time, staticbox.MaxEdge.X)
						- std::min(movingbox.MinEdge.X + speed.X * time, staticbox.MinEdge.X)
						- relbox.MinEdge.X < 0) &&
						(std::max(movingbox.MaxEdge.Z + speed.Z * time, staticbox.MaxEdge.Z)
							- std::min(movingbox.MinEdge.Z + speed.Z * time, staticbox.MinEdge.Z)
							- relbox.MinEdge.Z < 0)
					)
					return COLLISION_AXIS_Y;
			}
		}
		else {
			return COLLISION_AXIS_NONE;
		}
	}

	// NO else if here

	if (speed.X) {
		distance = relbox.MaxEdge.X - relbox.MinEdge.X;
		*dtime = distance / std::abs(speed.X);
		time = std::max(*dtime, 0.0f);

		if (*dtime <= dtime_max) {
			inner_margin = std::max<opos_t>(-0.5f * (staticbox.MaxEdge.X - staticbox.MinEdge.X), -2.0f);

			if ((speed.X > 0 && staticbox.MinEdge.X - movingbox.MaxEdge.X > inner_margin) ||
				(speed.X < 0 && movingbox.MinEdge.X - staticbox.MaxEdge.X > inner_margin)) {
				if (
					(std::max(movingbox.MaxEdge.Y + speed.Y * time, staticbox.MaxEdge.Y)
						- std::min(movingbox.MinEdge.Y + speed.Y * time, staticbox.MinEdge.Y)
						- relbox.MinEdge.Y < 0) &&
						(std::max(movingbox.MaxEdge.Z + speed.Z * time, staticbox.MaxEdge.Z)
							- std::min(movingbox.MinEdge.Z + speed.Z * time, staticbox.MinEdge.Z)
							- relbox.MinEdge.Z < 0)
					)
					return COLLISION_AXIS_X;
			}
		} else {
			return COLLISION_AXIS_NONE;
		}
	}

	// NO else if here

	if (speed.Z) {
		distance = relbox.MaxEdge.Z - relbox.MinEdge.Z;
		*dtime = distance / std::abs(speed.Z);
		time = std::max(*dtime, 0.0f);

		if (*dtime <= dtime_max) {
			inner_margin = std::max<opos_t>(-0.5f * (staticbox.MaxEdge.Z - staticbox.MinEdge.Z), -2.0f);

			if ((speed.Z > 0 && staticbox.MinEdge.Z - movingbox.MaxEdge.Z > inner_margin) ||
				(speed.Z < 0 && movingbox.MinEdge.Z - staticbox.MaxEdge.Z > inner_margin)) {
				if (
					(std::max(movingbox.MaxEdge.X + speed.X * time, staticbox.MaxEdge.X)
						- std::min(movingbox.MinEdge.X + speed.X * time, staticbox.MinEdge.X)
						- relbox.MinEdge.X < 0) &&
						(std::max(movingbox.MaxEdge.Y + speed.Y * time, staticbox.MaxEdge.Y)
							- std::min(movingbox.MinEdge.Y + speed.Y * time, staticbox.MinEdge.Y)
							- relbox.MinEdge.Y < 0)
					)
					return COLLISION_AXIS_Z;
			}
		}
	}

	return COLLISION_AXIS_NONE;
}

// Helper function:
// Checks if moving the movingbox up by the given distance would hit a ceiling.
bool wouldCollideWithCeiling(
		const std::vector<NearbyCollisionInfo> &cinfo,
		const aabb3o &movingbox,
		f32 y_increase, f32 d)
{
	//TimeTaker tt("wouldCollideWithCeiling");

	assert(y_increase >= 0);	// pre-condition

	for (const auto &it : cinfo) {
		const aabb3o &staticbox = it.box;
		if ((movingbox.MaxEdge.Y - d <= staticbox.MinEdge.Y) &&
				(movingbox.MaxEdge.Y + y_increase > staticbox.MinEdge.Y) &&
				(movingbox.MinEdge.X < staticbox.MaxEdge.X) &&
				(movingbox.MaxEdge.X > staticbox.MinEdge.X) &&
				(movingbox.MinEdge.Z < staticbox.MaxEdge.Z) &&
				(movingbox.MaxEdge.Z > staticbox.MinEdge.Z))
			return true;
	}

	return false;
}

static bool add_area_node_boxes(const v3pos_t min, const v3pos_t max, IGameDef *gamedef,
		Environment *env, std::vector<NearbyCollisionInfo> &cinfo)
{
	const auto *nodedef = gamedef->getNodeDefManager();
	bool any_position_valid = false;

	thread_local std::vector<aabb3f> nodeboxes;
	Map *map = &env->getMap();

	v3pos_t p;
	for (p.Z = min.Z; p.Z <= max.Z; p.Z++)
	for (p.Y = min.Y; p.Y <= max.Y; p.Y++)
	for (p.X = min.X; p.X <= max.X; p.X++) {
		bool is_position_valid;
		MapNode n = map->getNode(p, &is_position_valid);

		if (is_position_valid && n.getContent() != CONTENT_IGNORE) {
			// Object collides into walkable nodes

			any_position_valid = true;
			const ContentFeatures &f = nodedef->get(n);

			if (!f.walkable)
				continue;

			// Negative bouncy may have a meaning, but we need +value here.
			int n_bouncy_value = abs(itemgroup_get(f.groups, "bouncy"));

			u8 neighbors = n.getNeighbors(p, map);

			nodeboxes.clear();
			n.getCollisionBoxes(nodedef, &nodeboxes, neighbors);

			// Calculate float position only once
			auto posf = intToFloat(p, BS);
			for (auto boxf : nodeboxes) {
				aabb3o box = ToOpos(boxf);
				box.MinEdge += posf;
				box.MaxEdge += posf;
				cinfo.emplace_back(false, n_bouncy_value, p, box);
			}
		} else {
			// Collide with unloaded nodes (position invalid) and loaded
			// CONTENT_IGNORE nodes (position valid)
			auto box = getNodeBox(p, (opos_t)BS);
			cinfo.emplace_back(true, 0, p, box);
		}
	}
	return any_position_valid;
}

static void add_object_boxes(Environment *env,
		const aabb3f &box_0, f32 dtime,
		const v3opos_t pos_f, const v3f speed_f, ActiveObject *self,
		std::vector<NearbyCollisionInfo> &cinfo)
{
	auto process_object = [&cinfo] (ActiveObject *object) {
		if (object && object->collideWithObjects()) {
			aabb3o box{{0.0f, 0.0f, 0.0f}};
			if (object->getCollisionBox(&box))
				cinfo.emplace_back(object, 0, box);
		}
	};

	// Calculate distance by speed, add own extent and 1.5m of tolerance
	const f32 distance = speed_f.getLength() * dtime +
		box_0.getExtent().getLength() + 1.5f * BS;

#if CHECK_CLIENT_BUILD()
	ClientEnvironment *c_env = dynamic_cast<ClientEnvironment*>(env);
	if (c_env) {
		std::vector<DistanceSortedActiveObject> clientobjects;
		c_env->getActiveObjects(pos_f, distance, clientobjects);

		for (auto &clientobject : clientobjects) {
			// Do collide with everything but itself and children
			if (!self || (self != clientobject.obj &&
					self != clientobject.obj->getParent())) {
				process_object(clientobject.obj);
			}
		}

		// add collision with local player
		LocalPlayer *lplayer = c_env->getLocalPlayer();
		auto *obj = (ClientActiveObject*) lplayer->getCAO();
		if (!self || (self != obj && self != obj->getParent())) {
			auto lplayer_collisionbox = ToOpos(lplayer->getCollisionbox());
			auto lplayer_pos = lplayer->getPosition();
			lplayer_collisionbox.MinEdge += lplayer_pos;
			lplayer_collisionbox.MaxEdge += lplayer_pos;
			cinfo.emplace_back(obj, 0, lplayer_collisionbox);
		}
	}
	else
#endif
	{
		ServerEnvironment *s_env = dynamic_cast<ServerEnvironment*>(env);
		if (s_env) {
			// search for objects which are not us and not our children.
			// we directly process the object in this callback to avoid useless
			// looping afterwards.
			auto include_obj_cb = [self, &process_object] (ServerActiveObject *obj) {
				if (!obj->isGone() &&
					(!self || (self != obj && self != obj->getParent()))) {
					process_object(obj);
				}
				return false;
			};

			// nothing is put into this vector
			std::vector<ServerActiveObject*> s_objects;
			s_env->getObjectsInsideRadius(s_objects, pos_f, distance, include_obj_cb);
		}
	}
}

#define PROFILER_NAME(text) (dynamic_cast<ServerEnvironment*>(env) ? ("Server: " text) : ("Client: " text))

collisionMoveResult collisionMoveSimple(Environment *env, IGameDef *gamedef,
		const aabb3f &box_0,
		f32 stepheight, f32 dtime,
		v3opos_t *pos_f, v3f *speed_f,
		v3f accel_f, ActiveObject *self,
		bool collide_with_objects)
{
	static bool time_notification_done = false;

	ScopeProfiler sp(g_profiler, PROFILER_NAME("collisionMoveSimple()"), SPT_AVG, PRECISION_MICRO);

	collisionMoveResult result;

	/*
		Calculate new velocity
	*/
	if (dtime > DTIME_LIMIT) {
		if (!time_notification_done) {
			time_notification_done = true;
			warningstream << "collisionMoveSimple: maximum step interval exceeded,"
					" lost movement details!"<<std::endl;
		}
		g_collision_problems_encountered = true;
		dtime = DTIME_LIMIT;
	} else {
		time_notification_done = false;
	}

	v3opos_t dpos_f = (v3fToOpos(*speed_f + accel_f) * 0.5f * dtime) * dtime;
	v3opos_t newpos_f = *pos_f + dpos_f;
	*speed_f += accel_f * dtime;

	// If the object is static, there are no collisions
	if (dpos_f == v3opos_t())
		return result;

	// Limit speed for avoiding hangs
	speed_f->Y = rangelim(speed_f->Y, -5000, 5000);
	speed_f->X = rangelim(speed_f->X, -5000, 5000);
	speed_f->Z = rangelim(speed_f->Z, -5000, 5000);

	*speed_f = truncate(*speed_f, 10000.0f);

	/*
		Collect node boxes in movement range
	*/

	// cached allocation
	thread_local std::vector<NearbyCollisionInfo> cinfo;
	cinfo.clear();

	{
		v3opos_t minpos_f(
			MYMIN(pos_f->X, newpos_f.X),
			MYMIN(pos_f->Y, newpos_f.Y) + 0.01f * BS, // bias rounding, player often at +/-n.5
			MYMIN(pos_f->Z, newpos_f.Z)
		);
		v3opos_t maxpos_f(
			MYMAX(pos_f->X, newpos_f.X),
			MYMAX(pos_f->Y, newpos_f.Y),
			MYMAX(pos_f->Z, newpos_f.Z)
		);
		v3pos_t min = floatToInt(minpos_f + v3fToOpos(box_0.MinEdge), BS) - v3pos_t(1, 1, 1);
		v3pos_t max = floatToInt(maxpos_f + v3fToOpos(box_0.MaxEdge), BS) + v3pos_t(1, 1, 1);

		bool any_position_valid = add_area_node_boxes(min, max, gamedef, env, cinfo);

		// Do not move if world has not loaded yet, since custom node boxes
		// are not available for collision detection.
		// This also intentionally occurs in the case of the object being positioned
		// solely on loaded CONTENT_IGNORE nodes, no matter where they come from.
		if (!any_position_valid) {
			*speed_f = v3f(0, 0, 0);
			return result;
		}
	}

	/*
		Collect object boxes in movement range
	*/
	if (collide_with_objects) {
		add_object_boxes(env, box_0, dtime, *pos_f, *speed_f, self, cinfo);
	}

	/*
		Collision detection
	*/

	f32 d = 0.0f;

	int loopcount = 0;

	while(dtime > BS * 1e-10f) {
		// Avoid infinite loop
		loopcount++;
		if (loopcount >= 100) {
			warningstream << "collisionMoveSimple: Loop count exceeded, aborting to avoid infinite loop" << std::endl;
			g_collision_problems_encountered = true;
			break;
		}

		aabb3o movingbox(ToOpos(box_0));
		movingbox.MinEdge += *pos_f;
		movingbox.MaxEdge += *pos_f;

		CollisionAxis nearest_collided = COLLISION_AXIS_NONE;
		f32 nearest_dtime = dtime;
		int nearest_boxindex = -1;

		/*
			Go through every nodebox, find nearest collision
		*/
		for (u32 boxindex = 0; boxindex < cinfo.size(); boxindex++) {
			const NearbyCollisionInfo &box_info = cinfo[boxindex];
			// Ignore if already stepped up this nodebox.
			if (box_info.is_step_up)
				continue;

			// Find nearest collision of the two boxes (raytracing-like)
			f32 dtime_tmp = nearest_dtime;
			CollisionAxis collided = axisAlignedCollision(box_info.box,
					movingbox, *speed_f, &dtime_tmp);

			if (collided == -1 || dtime_tmp >= nearest_dtime)
				continue;

			nearest_dtime = dtime_tmp;
			nearest_collided = collided;
			nearest_boxindex = boxindex;
		}

		if (nearest_collided == COLLISION_AXIS_NONE) {
			// No collision with any collision box.
			*pos_f += v3fToOpos(truncate(*speed_f * dtime, 100.0f));
			dtime = 0;  // Set to 0 to avoid "infinite" loop due to small FP numbers
		} else {
			// Otherwise, a collision occurred.
			NearbyCollisionInfo &nearest_info = cinfo[nearest_boxindex];
			const aabb3o& cbox = nearest_info.box;

			//movingbox except moved to the horizontal position it would be after step up
			aabb3o stepbox = movingbox;
			stepbox.MinEdge.X += speed_f->X * dtime;
			stepbox.MinEdge.Z += speed_f->Z * dtime;
			stepbox.MaxEdge.X += speed_f->X * dtime;
			stepbox.MaxEdge.Z += speed_f->Z * dtime;
			// Check for stairs.
			bool step_up = (nearest_collided != COLLISION_AXIS_Y) && // must not be Y direction
					(movingbox.MinEdge.Y < cbox.MaxEdge.Y) &&
					(movingbox.MinEdge.Y + stepheight > cbox.MaxEdge.Y) &&
					(!wouldCollideWithCeiling(cinfo, stepbox,
							cbox.MaxEdge.Y - movingbox.MinEdge.Y,
							d));

			// Get bounce multiplier
			float bounce = -(float)nearest_info.bouncy / 100.0f;

			// Move to the point of collision and reduce dtime by nearest_dtime
			if (nearest_dtime < 0) {
				// Handle negative nearest_dtime
				if (!step_up) {
					if (nearest_collided == COLLISION_AXIS_X)
						pos_f->X += speed_f->X * nearest_dtime;
					if (nearest_collided == COLLISION_AXIS_Y)
						pos_f->Y += speed_f->Y * nearest_dtime;
					if (nearest_collided == COLLISION_AXIS_Z)
						pos_f->Z += speed_f->Z * nearest_dtime;
				}
			} else {
				*pos_f += v3fToOpos(truncate(*speed_f * nearest_dtime, 100.0f));
				dtime -= nearest_dtime;
			}

			bool is_collision = true;
			if (nearest_info.is_unloaded)
				is_collision = false;

			CollisionInfo info;
			if (nearest_info.isObject())
				info.type = COLLISION_OBJECT;
			else
				info.type = COLLISION_NODE;

			info.node_p = nearest_info.position;
			info.object = nearest_info.obj;
			info.new_pos = *pos_f;
			info.old_speed = *speed_f;

			// Set the speed component that caused the collision to zero
			if (step_up) {
				// Special case: Handle stairs
				nearest_info.is_step_up = true;
				is_collision = false;
			} else if (nearest_collided == COLLISION_AXIS_X) {
				if (fabs(speed_f->X) > BS * 3)
					speed_f->X *= bounce;
				else
					speed_f->X = 0;
				result.collides = true;
			} else if (nearest_collided == COLLISION_AXIS_Y) {
				if(fabs(speed_f->Y) > BS * 3)
					speed_f->Y *= bounce;
				else
					speed_f->Y = 0;
				result.collides = true;
			} else if (nearest_collided == COLLISION_AXIS_Z) {
				if (fabs(speed_f->Z) > BS * 3)
					speed_f->Z *= bounce;
				else
					speed_f->Z = 0;
				result.collides = true;
			}

			info.new_speed = *speed_f;
			if (info.new_speed.getDistanceFrom(info.old_speed) < 0.1f * BS)
				is_collision = false;

			if (is_collision) {
				info.axis = nearest_collided;
				result.collisions.push_back(std::move(info));
			}
		}
	}

	/*
		Final touches: Check if standing on ground, step up stairs.
	*/
	aabb3o box(ToOpos(box_0));
	box.MinEdge += *pos_f;
	box.MaxEdge += *pos_f;
	for (const auto &box_info : cinfo) {
		const aabb3o &cbox = box_info.box;

		/*
			See if the object is touching ground.

			Object touches ground if object's minimum Y is near node's
			maximum Y and object's X-Z-area overlaps with the node's
			X-Z-area.
		*/

		if (cbox.MaxEdge.X - d > box.MinEdge.X && cbox.MinEdge.X + d < box.MaxEdge.X &&
				cbox.MaxEdge.Z - d > box.MinEdge.Z &&
				cbox.MinEdge.Z + d < box.MaxEdge.Z) {
			if (box_info.is_step_up) {
				pos_f->Y += cbox.MaxEdge.Y - box.MinEdge.Y;
				box = ToOpos(box_0);
				box.MinEdge += *pos_f;
				box.MaxEdge += *pos_f;
			}
			if (std::fabs(cbox.MaxEdge.Y - box.MinEdge.Y) < 0.05f) {
				result.touching_ground = true;

				if (box_info.isObject())
					result.standing_on_object = true;
			}
		}
	}

	return result;
}

bool collision_check_intersection(Environment *env, IGameDef *gamedef,
		const aabb3f &box_0, const v3opos_t &pos_f, ActiveObject *self,
		bool collide_with_objects)
{
	ScopeProfiler sp(g_profiler, PROFILER_NAME("collision_check_intersection()"), SPT_AVG, PRECISION_MICRO);

	std::vector<NearbyCollisionInfo> cinfo;
	{
		auto min = floatToInt(pos_f + v3fToOpos(box_0.MinEdge), BS) - v3pos_t(1, 1, 1);
		auto max = floatToInt(pos_f + v3fToOpos(box_0.MaxEdge), BS) + v3pos_t(1, 1, 1);

		bool any_position_valid = add_area_node_boxes(min, max, gamedef, env, cinfo);

		if (!any_position_valid) {
			return true;
		}
	}

	if (collide_with_objects) {
		v3f speed;
		add_object_boxes(env, box_0, 0, pos_f, speed, self, cinfo);
	}

	/*
		Collision detection
	*/
        aabb3o checkbox = {v3fToOpos(box_0.MinEdge), v3fToOpos(box_0.MaxEdge)};
        // aabbox3d::intersectsWithBox(box) returns true when the faces are touching perfectly.
	// However, we do not want want a true-ish return value in that case. Add some tolerance.
	checkbox.MinEdge += pos_f + (0.1f * BS);
	checkbox.MaxEdge += pos_f - (0.1f * BS);

	/*
		Go through every node and object box
	*/
	for (const NearbyCollisionInfo &box_info : cinfo) {
		if (box_info.box.intersectsWithBox(checkbox))
			return true;
	}

	return false;
}
