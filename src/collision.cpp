/*
collision.cpp
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

#include "collision.h"
#include "mapblock.h"
#include "map.h"
#include "nodedef.h"
#include "gamedef.h"
#include "log.h"
#include "environment.h"
#include "serverobject.h"
#include <vector>
#include <set>
#include "util/timetaker.h"
#include "main.h" // g_profiler
#include "profiler.h"

// float error is 10 - 9.96875 = 0.03125 // default with bug = 0
#define COLL_ZERO 0.032 // broken unit tests
#define COLL_ZEROY 0.032 //0.032 // Y only

// Helper function:
// Checks for collision of a moving aabbox with a static aabbox
// Returns -1 if no collision, 0 if X collision, 1 if Y collision, 2 if Z collision
// The time after which the collision occurs is stored in dtime.
int axisAlignedCollision(
		const aabb3f &staticbox, const aabb3f &movingbox,
		const v3f &speed, f32 d, f32 &dtime)
{
	//TimeTaker tt("axisAlignedCollision");

	f32 xsize = (staticbox.MaxEdge.X - staticbox.MinEdge.X); // - COLL_ZEROY;     // reduce box size for solve collision stuck (flying sand)
	f32 ysize = (staticbox.MaxEdge.Y - staticbox.MinEdge.Y); // - COLL_ZERO; // Y - no sense for falling, but maybe try later
	f32 zsize = (staticbox.MaxEdge.Z - staticbox.MinEdge.Z); // - COLL_ZEROY;

	aabb3f relbox(
			movingbox.MinEdge.X - staticbox.MinEdge.X,
			movingbox.MinEdge.Y - staticbox.MinEdge.Y,
			movingbox.MinEdge.Z - staticbox.MinEdge.Z,
			movingbox.MaxEdge.X - staticbox.MinEdge.X,
			movingbox.MaxEdge.Y - staticbox.MinEdge.Y,
			movingbox.MaxEdge.Z - staticbox.MinEdge.Z
	);

	// These cases can (and should) be rejected immediately
	if( (speed.X >= 0 && relbox.MinEdge.X > xsize) ||
	    (speed.X <= 0 && relbox.MaxEdge.X < 0) ||
	    (speed.Y >= 0 && relbox.MinEdge.Y > ysize) ||
	    (speed.Y <= 0 && relbox.MaxEdge.Y < 0) ||
	    (speed.Z >= 0 && relbox.MinEdge.Z > zsize) ||
	    (speed.Z <= 0 && relbox.MaxEdge.Z < 0))
	    	return -1;

	if(speed.X > 0) // Check for collision with X- plane
	{
		if(relbox.MaxEdge.X <= d)
		{
			dtime = - relbox.MaxEdge.X / speed.X;
			if((relbox.MinEdge.Y + speed.Y * dtime < ysize) &&
					(relbox.MaxEdge.Y + speed.Y * dtime > COLL_ZERO) &&
					(relbox.MinEdge.Z + speed.Z * dtime < zsize) &&
					(relbox.MaxEdge.Z + speed.Z * dtime > COLL_ZERO))
				return 0;
		}
	}
	else if(speed.X < 0) // Check for collision with X+ plane
	{
		if(relbox.MinEdge.X >= xsize - d)
		{
			dtime = (xsize - relbox.MinEdge.X) / speed.X;
			if((relbox.MinEdge.Y + speed.Y * dtime < ysize) &&
					(relbox.MaxEdge.Y + speed.Y * dtime > COLL_ZERO) &&
					(relbox.MinEdge.Z + speed.Z * dtime < zsize) &&
					(relbox.MaxEdge.Z + speed.Z * dtime > COLL_ZERO))
				return 0;
		}
	}

	// NO else if here

	if(speed.Y > 0) // Check for collision with Y- plane
	{
		if(relbox.MaxEdge.Y <= d)
		{
			dtime = - relbox.MaxEdge.Y / speed.Y;
			if((relbox.MinEdge.X + speed.X * dtime < xsize) &&
					(relbox.MaxEdge.X + speed.X * dtime > COLL_ZERO) &&
					(relbox.MinEdge.Z + speed.Z * dtime < zsize) &&
					(relbox.MaxEdge.Z + speed.Z * dtime > COLL_ZERO))
				return 1;
		}
	}
	else if(speed.Y < 0) // Check for collision with Y+ plane
	{
		if(relbox.MinEdge.Y >= ysize - d)
		{
			dtime = (ysize - relbox.MinEdge.Y) / speed.Y;
			if((relbox.MinEdge.X + speed.X * dtime < xsize) &&
					(relbox.MaxEdge.X + speed.X * dtime > COLL_ZEROY) &&
					(relbox.MinEdge.Z + speed.Z * dtime < zsize) &&
					(relbox.MaxEdge.Z + speed.Z * dtime > COLL_ZEROY))
				return 1;
		}
	}

	// NO else if here

	if(speed.Z > 0) // Check for collision with Z- plane
	{
		if(relbox.MaxEdge.Z <= d)
		{
			dtime = - relbox.MaxEdge.Z / speed.Z;
			if((relbox.MinEdge.X + speed.X * dtime < xsize) &&
					(relbox.MaxEdge.X + speed.X * dtime > COLL_ZERO) &&
					(relbox.MinEdge.Y + speed.Y * dtime < ysize) &&
					(relbox.MaxEdge.Y + speed.Y * dtime > COLL_ZERO))
				return 2;
		}
	}
	else if(speed.Z < 0) // Check for collision with Z+ plane
	{
		if(relbox.MinEdge.Z >= zsize - d)
		{
			dtime = (zsize - relbox.MinEdge.Z) / speed.Z;
			if((relbox.MinEdge.X + speed.X * dtime < xsize) &&
					(relbox.MaxEdge.X + speed.X * dtime > COLL_ZERO) &&
					(relbox.MinEdge.Y + speed.Y * dtime < ysize) &&
					(relbox.MaxEdge.Y + speed.Y * dtime > COLL_ZERO))
				return 2;
		}
	}

	return -1;
}

// Helper function:
// Checks if moving the movingbox up by the given distance would hit a ceiling.
bool wouldCollideWithCeiling(
		const std::vector<aabb3f> &staticboxes,
		const aabb3f &movingbox,
		f32 y_increase, f32 d)
{
	//TimeTaker tt("wouldCollideWithCeiling");

	assert(y_increase >= 0);	// pre-condition

	for(std::vector<aabb3f>::const_iterator
			i = staticboxes.begin();
			i != staticboxes.end(); i++)
	{
		const aabb3f& staticbox = *i;
		if((movingbox.MaxEdge.Y - d <= staticbox.MinEdge.Y) &&
				(movingbox.MaxEdge.Y + y_increase > staticbox.MinEdge.Y) &&
				(movingbox.MinEdge.X < staticbox.MaxEdge.X) &&
				(movingbox.MaxEdge.X > staticbox.MinEdge.X) &&
				(movingbox.MinEdge.Z < staticbox.MaxEdge.Z) &&
				(movingbox.MaxEdge.Z > staticbox.MinEdge.Z))
			return true;
	}

	return false;
}


collisionMoveResult collisionMoveSimple(Environment *env, IGameDef *gamedef,
		f32 pos_max_d, const aabb3f &box_0,
		f32 stepheight, f32 dtime,
		v3f &pos_f, v3f &speed_f,
		v3f &accel_f,ActiveObject* self,
		bool collideWithObjects)
{
	Map *map = &env->getMap();
	//TimeTaker tt("collisionMoveSimple");
    //ScopeProfiler sp(g_profiler, "collisionMoveSimple avg", SPT_AVG);

	collisionMoveResult result;

	/*
		Calculate new velocity
	*/
	if( dtime > 1 ) {
/*
		infostream<<"collisionMoveSimple: WARNING: maximum step interval exceeded, lost movement details!"<<std::endl;
*/
		dtime = 1;
	}
	speed_f += accel_f * dtime;

	// If there is no speed, there are no collisions
	if(speed_f.getLength() == 0)
		return result;

	// Limit speed for avoiding hangs
	speed_f.Y=rangelim(speed_f.Y,-1000,1000);
	speed_f.X=rangelim(speed_f.X,-1000,1000);
	speed_f.Z=rangelim(speed_f.Z,-1000,1000);

	/*
		Collect node boxes in movement range
	*/
	std::vector<aabb3f> cboxes;
	std::vector<bool> is_unloaded;
	std::vector<bool> is_step_up;
	std::vector<bool> is_object;
	std::vector<int> bouncy_values;
	std::vector<v3s16> node_positions;
	{
	//TimeTaker tt2("collisionMoveSimple collect boxes");
    //ScopeProfiler sp(g_profiler, "collisionMoveSimple collect boxes avg", SPT_AVG);

	v3s16 oldpos_i = floatToInt(pos_f, BS);
	v3s16 newpos_i = floatToInt(pos_f + speed_f * dtime, BS);
	s16 min_x = MYMIN(oldpos_i.X, newpos_i.X) + (box_0.MinEdge.X / BS) - 1;
	s16 min_y = MYMIN(oldpos_i.Y, newpos_i.Y) + (box_0.MinEdge.Y / BS) - 1;
	s16 min_z = MYMIN(oldpos_i.Z, newpos_i.Z) + (box_0.MinEdge.Z / BS) - 1;
	s16 max_x = MYMAX(oldpos_i.X, newpos_i.X) + (box_0.MaxEdge.X / BS) + 1;
	s16 max_y = MYMAX(oldpos_i.Y, newpos_i.Y) + (box_0.MaxEdge.Y / BS) + 1;
	s16 max_z = MYMAX(oldpos_i.Z, newpos_i.Z) + (box_0.MaxEdge.Z / BS) + 1;

	for(s16 x = min_x; x <= max_x; x++)
	for(s16 y = min_y; y <= max_y; y++)
	for(s16 z = min_z; z <= max_z; z++)
	{
		v3s16 p(x,y,z);

		bool is_position_valid;
		MapNode n = map->getNodeNoEx(p, &is_position_valid);

		if (is_position_valid) {
			// Object collides into walkable nodes

			const ContentFeatures &f = gamedef->getNodeDefManager()->get(n);
			if(f.walkable == false)
				continue;
			int n_bouncy_value = itemgroup_get(f.groups, "bouncy");

			std::vector<aabb3f> nodeboxes = n.getCollisionBoxes(gamedef->ndef());
			for(std::vector<aabb3f>::iterator
					i = nodeboxes.begin();
					i != nodeboxes.end(); i++)
			{
				aabb3f box = *i;
				box.MinEdge += v3f(x, y, z)*BS;
				box.MaxEdge += v3f(x, y, z)*BS;
				cboxes.push_back(box);
				is_unloaded.push_back(false);
				is_step_up.push_back(false);
				bouncy_values.push_back(n_bouncy_value);
				node_positions.push_back(p);
				is_object.push_back(false);
			}
		}
		else {
			// Collide with unloaded nodes
			aabb3f box = getNodeBox(p, BS);
			cboxes.push_back(box);
			is_unloaded.push_back(true);
			is_step_up.push_back(false);
			bouncy_values.push_back(0);
			node_positions.push_back(p);
			is_object.push_back(false);
		}
	}
	} // tt2

	if(collideWithObjects)
	{
		//ScopeProfiler sp(g_profiler, "collisionMoveSimple objects avg", SPT_AVG);
		//TimeTaker tt3("collisionMoveSimple collect object boxes");

		/* add object boxes to cboxes */


		std::vector<ActiveObject*> objects;
#ifndef SERVER
		ClientEnvironment *c_env = dynamic_cast<ClientEnvironment*>(env);
		if (c_env != 0) {
			f32 distance = speed_f.getLength();
			std::vector<DistanceSortedActiveObject> clientobjects;
			c_env->getActiveObjects(pos_f,distance * 1.5,clientobjects);
			for (size_t i=0; i < clientobjects.size(); i++) {
				if ((self == 0) || (self != clientobjects[i].obj)) {
					objects.push_back((ActiveObject*)clientobjects[i].obj);
				}
			}
		}
		else
#endif
		{
			ServerEnvironment *s_env = dynamic_cast<ServerEnvironment*>(env);
			if (s_env != 0) {
				f32 distance = speed_f.getLength();
				std::set<u16> s_objects = s_env->getObjectsInsideRadius(pos_f,distance * 1.5);
				for (std::set<u16>::iterator iter = s_objects.begin(); iter != s_objects.end(); iter++) {
					ServerActiveObject *current = s_env->getActiveObject(*iter);
					if ((self == 0) || (self != current)) {
						objects.push_back((ActiveObject*)current);
					}
				}
			}
		}

		for (std::vector<ActiveObject*>::const_iterator iter = objects.begin();
				iter != objects.end(); ++iter) {
			ActiveObject *object = *iter;

			if (object != NULL) {
				aabb3f object_collisionbox;
				if (object->getCollisionBox(&object_collisionbox) &&
						object->collideWithObjects()) {
					cboxes.push_back(object_collisionbox);
					is_unloaded.push_back(false);
					is_step_up.push_back(false);
					bouncy_values.push_back(0);
					node_positions.push_back(v3s16(0,0,0));
					is_object.push_back(true);
				}
			}
		}
	} //tt3

	assert(cboxes.size() == is_unloaded.size());    // post-condition
	assert(cboxes.size() == is_step_up.size());     // post-condition
	assert(cboxes.size() == bouncy_values.size());  // post-condition
	assert(cboxes.size() == node_positions.size()); // post-condition
	assert(cboxes.size() == is_object.size());      // post-condition

	/*
		Collision detection
	*/

	/*
		Collision uncertainty radius
		Make it a bit larger than the maximum distance of movement
	*/
	f32 d = pos_max_d * 1.1;
	// A fairly large value in here makes moving smoother
	//f32 d = 0.15*BS;

	// This should always apply, otherwise there are glitches
	assert(d > pos_max_d);	// invariant

	int loopcount = 0;

	while(dtime > BS*1e-10)
	{
		//TimeTaker tt3("collisionMoveSimple dtime loop");
        //ScopeProfiler sp(g_profiler, "collisionMoveSimple dtime loop avg", SPT_AVG);

		// Avoid infinite loop
		loopcount++;
		if(loopcount >= 100)
		{
			infostream<<"collisionMoveSimple: WARNING: Loop count exceeded, aborting to avoid infiniite loop"<<std::endl;
			dtime = 0;
			break;
		}

		aabb3f movingbox = box_0;
		movingbox.MinEdge += pos_f;
		movingbox.MaxEdge += pos_f;

		int nearest_collided = -1;
		f32 nearest_dtime = dtime;
		u32 nearest_boxindex = -1;

		/*
			Go through every nodebox, find nearest collision
		*/
		for(u32 boxindex = 0; boxindex < cboxes.size(); boxindex++)
		{
			// Ignore if already stepped up this nodebox.
			if(is_step_up[boxindex])
				continue;

			// Find nearest collision of the two boxes (raytracing-like)
			f32 dtime_tmp;
			int collided = axisAlignedCollision(
					cboxes[boxindex], movingbox, speed_f, d, dtime_tmp);

			if(collided == -1 || dtime_tmp >= nearest_dtime)
				continue;

			nearest_dtime = dtime_tmp;
			nearest_collided = collided;
			nearest_boxindex = boxindex;
		}

		if(nearest_collided == -1)
		{
			// No collision with any collision box.
			pos_f += speed_f * dtime;
			dtime = 0;  // Set to 0 to avoid "infinite" loop due to small FP numbers
		}
		else
		{
			// Otherwise, a collision occurred.

			const aabb3f& cbox = cboxes[nearest_boxindex];

			// Check for stairs.
			bool step_up = (nearest_collided != 1) && // must not be Y direction
					(movingbox.MinEdge.Y < cbox.MaxEdge.Y) &&
					(movingbox.MinEdge.Y + stepheight > cbox.MaxEdge.Y) &&
					(!wouldCollideWithCeiling(cboxes, movingbox,
							cbox.MaxEdge.Y - movingbox.MinEdge.Y,
							d));

			// Get bounce multiplier
			bool bouncy = (bouncy_values[nearest_boxindex] >= 1);
			float bounce = -(float)bouncy_values[nearest_boxindex] / 100.0;

			// Move to the point of collision and reduce dtime by nearest_dtime
			if(nearest_dtime < 0)
			{
				// Handle negative nearest_dtime (can be caused by the d allowance)
				if(!step_up)
				{
					if(nearest_collided == 0)
						pos_f.X += speed_f.X * nearest_dtime;
					if(nearest_collided == 1)
						pos_f.Y += speed_f.Y * nearest_dtime;
					if(nearest_collided == 2)
						pos_f.Z += speed_f.Z * nearest_dtime;
				}
			}
			else
			{
				pos_f += speed_f * nearest_dtime;
				dtime -= nearest_dtime;
			}
			
			bool is_collision = true;
			if(is_unloaded[nearest_boxindex])
				is_collision = false;

			CollisionInfo info;
			if (is_object[nearest_boxindex]) {
				info.type = COLLISION_OBJECT;
			}
			else {
				info.type = COLLISION_NODE;
			}
			info.node_p = node_positions[nearest_boxindex];
			info.bouncy = bouncy;
			info.old_speed = speed_f;

			// Set the speed component that caused the collision to zero
			if(step_up)
			{
				// Special case: Handle stairs
				is_step_up[nearest_boxindex] = true;
				is_collision = false;
			}
			else if(nearest_collided == 0) // X
			{
				if(fabs(speed_f.X) > BS*3)
					speed_f.X *= bounce;
				else
					speed_f.X = 0;
				result.collides = true;
				result.collides_xz = true;
			}
			else if(nearest_collided == 1) // Y
			{
				if(fabs(speed_f.Y) > BS*3)
					speed_f.Y *= bounce;
				else
					speed_f.Y = 0;
				result.collides = true;
			}
			else if(nearest_collided == 2) // Z
			{
				if(fabs(speed_f.Z) > BS*3)
					speed_f.Z *= bounce;
				else
					speed_f.Z = 0;
				result.collides = true;
				result.collides_xz = true;
			}

			info.new_speed = speed_f;
			if(info.new_speed.getDistanceFrom(info.old_speed) < 0.1*BS)
				is_collision = false;

			if(is_collision){
				result.collisions.push_back(info);
			}
		}
	}

	/*
		Final touches: Check if standing on ground, step up stairs.
	*/
	aabb3f box = box_0;
	box.MinEdge += pos_f;
	box.MaxEdge += pos_f;
	for(u32 boxindex = 0; boxindex < cboxes.size(); boxindex++)
	{
		const aabb3f& cbox = cboxes[boxindex];

		/*
			See if the object is touching ground.

			Object touches ground if object's minimum Y is near node's
			maximum Y and object's X-Z-area overlaps with the node's
			X-Z-area.

			Use 0.15*BS so that it is easier to get on a node.
		*/
		if(
				cbox.MaxEdge.X-d > box.MinEdge.X &&
				cbox.MinEdge.X+d < box.MaxEdge.X &&
				cbox.MaxEdge.Z-d > box.MinEdge.Z &&
				cbox.MinEdge.Z+d < box.MaxEdge.Z
		){
			if(is_step_up[boxindex])
			{
				pos_f.Y += (cbox.MaxEdge.Y - box.MinEdge.Y);
				box = box_0;
				box.MinEdge += pos_f;
				box.MaxEdge += pos_f;
			}
			if(fabs(cbox.MaxEdge.Y-box.MinEdge.Y) < 0.15*BS)
			{
				result.touching_ground = true;
				if(is_unloaded[boxindex])
					result.standing_on_unloaded = true;
			}
		}
	}

	return result;
}

#if 0
// This doesn't seem to work and isn't used
collisionMoveResult collisionMovePrecise(Map *map, IGameDef *gamedef,
		f32 pos_max_d, const aabb3f &box_0,
		f32 stepheight, f32 dtime,
		v3f &pos_f, v3f &speed_f, v3f &accel_f)
{
	//TimeTaker tt("collisionMovePrecise");
    ScopeProfiler sp(g_profiler, "collisionMovePrecise avg", SPT_AVG);
	
	collisionMoveResult final_result;

	// If there is no speed, there are no collisions
	if(speed_f.getLength() == 0)
		return final_result;

	// Don't allow overly huge dtime
	if(dtime > 2.0)
		dtime = 2.0;

	f32 dtime_downcount = dtime;

	u32 loopcount = 0;
	do
	{
		loopcount++;

		// Maximum time increment (for collision detection etc)
		// time = distance / speed
		f32 dtime_max_increment = 1.0;
		if(speed_f.getLength() != 0)
			dtime_max_increment = pos_max_d / speed_f.getLength();

		// Maximum time increment is 10ms or lower
		if(dtime_max_increment > 0.01)
			dtime_max_increment = 0.01;

		f32 dtime_part;
		if(dtime_downcount > dtime_max_increment)
		{
			dtime_part = dtime_max_increment;
			dtime_downcount -= dtime_part;
		}
		else
		{
			dtime_part = dtime_downcount;
			/*
				Setting this to 0 (no -=dtime_part) disables an infinite loop
				when dtime_part is so small that dtime_downcount -= dtime_part
				does nothing
			*/
			dtime_downcount = 0;
		}

		collisionMoveResult result = collisionMoveSimple(map, gamedef,
				pos_max_d, box_0, stepheight, dtime_part,
				pos_f, speed_f, accel_f);

		if(result.touching_ground)
			final_result.touching_ground = true;
		if(result.collides)
			final_result.collides = true;
		if(result.collides_xz)
			final_result.collides_xz = true;
		if(result.standing_on_unloaded)
			final_result.standing_on_unloaded = true;
	}
	while(dtime_downcount > 0.001);

	return final_result;
}
#endif
