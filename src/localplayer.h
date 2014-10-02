/*
localplayer.h
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

#ifndef LOCALPLAYER_HEADER
#define LOCALPLAYER_HEADER

#include "player.h"
#include <list>

class Environment;
class ClientEnvironment;
struct MapNode;
class GenericCAO;
class ClientActiveObject;

enum LocalPlayerAnimations {NO_ANIM, WALK_ANIM, DIG_ANIM, WD_ANIM};  // no local animation, walking, digging, both

class LocalPlayer : public Player
{
public:
	LocalPlayer(IGameDef *gamedef, const char *name);
	virtual ~LocalPlayer();

	bool isLocal() const
	{
		return true;
	}

	ClientActiveObject *parent;

	bool isAttached;

	v3f overridePosition;
	
	void move(f32 dtime, Environment *env, f32 pos_max_d);
	void move(f32 dtime, Environment *env, f32 pos_max_d,
			std::list<CollisionInfo> *collision_info);
	bool canPlaceNode(const v3s16& p, const MapNode& node);

	void applyControl(float dtime, ClientEnvironment *env);

	v3s16 getStandingNodePos();

	// Used to check if anything changed and prevent sending packets if not
	v3f last_position;
	v3f last_speed;
	float last_pitch;
	float last_yaw;
	unsigned int last_keyPressed;

	float camera_impact;
	v3f eye_offset_first;
	v3f eye_offset_third;

	int last_animation;
	float last_animation_speed;

	std::string hotbar_image;
	std::string hotbar_selected_image;

	GenericCAO* getCAO() const {
		return m_cao;
	}

	void setCAO(GenericCAO* toset) {
		assert( m_cao == NULL );
		m_cao = toset;
	}

private:
	// This is used for determining the sneaking range
	v3s16 m_sneak_node;
	// Whether the player is allowed to sneak
	bool m_sneak_node_exists;
	// Node below player, used to determine whether it has been removed,
	// and its old type
	v3s16 m_old_node_below;
	std::string m_old_node_below_type;
	// Whether recalculation of the sneak node is needed
	bool m_need_to_get_new_sneak_node;
	bool m_can_jump;

	GenericCAO* m_cao;
};

#endif

