/*
particles.h
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

#pragma once

#include <string>
#include "irrlichttypes_bloated.h"
#include "tileanimation.h"
#include "mapnode.h"

<<<<<<< HEAD
#include <iostream>
#include "irrlichttypes_extrabloated.h"
#include "client/tile.h"
#include "localplayer.h"
#include "environment.h"
#include <unordered_map>

=======
// This file defines the particle-related structures that both the server and
// client need. The ParticleManager and rendering is in client/particles.h
>>>>>>> 5.5.0

struct CommonParticleParams {
	bool collisiondetection = false;
	bool collision_removal = false;
	bool object_collision = false;
	bool vertical = false;
	std::string texture;
	struct TileAnimationParams animation;
	u8 glow = 0;
	MapNode node;
	u8 node_tile = 0;

	CommonParticleParams() {
		animation.type = TAT_NONE;
		node.setContent(CONTENT_IGNORE);
	}

	/* This helper is useful for copying params from
	 * ParticleSpawnerParameters to ParticleParameters */
	inline void copyCommon(CommonParticleParams &to) const {
		to.collisiondetection = collisiondetection;
		to.collision_removal = collision_removal;
		to.object_collision = object_collision;
		to.vertical = vertical;
		to.texture = texture;
		to.animation = animation;
		to.glow = glow;
		to.node = node;
		to.node_tile = node_tile;
	}
};

struct ParticleParameters : CommonParticleParams {
	v3f pos;
	v3f vel;
	v3f acc;
	f32 expirationtime = 1;
	f32 size = 1;

	void serialize(std::ostream &os, u16 protocol_ver) const;
	void deSerialize(std::istream &is, u16 protocol_ver);
};

struct ParticleSpawnerParameters : CommonParticleParams {
	u16 amount = 1;
	v3f minpos, maxpos, minvel, maxvel, minacc, maxacc;
	f32 time = 1;
	f32 minexptime = 1, maxexptime = 1, minsize = 1, maxsize = 1;

<<<<<<< HEAD
	void step (float dtime);

	void handleParticleEvent(ClientEvent *event,IGameDef *gamedef,
			scene::ISceneManager* smgr, LocalPlayer *player);

	void addDiggingParticles(IGameDef* gamedef, scene::ISceneManager* smgr,
		LocalPlayer *player, v3s16 pos, const TileSpec tiles[]);

	void addPunchingParticles(IGameDef* gamedef, scene::ISceneManager* smgr,
		LocalPlayer *player, v3s16 pos, const TileSpec tiles[]);

	void addNodeParticle(IGameDef* gamedef, scene::ISceneManager* smgr,
		LocalPlayer *player, v3s16 pos, const TileSpec tiles[]);

protected:
	void addParticle(Particle* toadd);

private:

	void stepParticles (float dtime);
	void stepSpawners (float dtime);

	void clearAll ();

	std::vector<Particle*> m_particles;
	std::unordered_map<u32, ParticleSpawner*> m_particle_spawners;

	ClientEnvironment* m_env;
	Mutex m_particle_list_lock;
	Mutex m_spawner_list_lock;
=======
	// For historical reasons no (de-)serialization methods here
>>>>>>> 5.5.0
};
