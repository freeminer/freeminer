// Luanti
// SPDX-License-Identifier: LGPL-2.1-or-later
// Copyright (C) 2010-2017 celeron55, Perttu Ahola <celeron55@gmail.com>

#include "threading/async.h"
#include "util/serialize.h"
#include "util/pointedthing.h"
#include "client.h"
#include "clientenvironment.h"
#include "clientsimpleobject.h"
#include "clientmap.h"
#include "localplayer.h"
#include "scripting_client.h"
#include "mapblock_mesh.h"
#include "mtevent.h"
#include "collision.h"
#include "nodedef.h"
#include "profiler.h"
#include "raycast.h"
#include "voxelalgorithms.h"
#include "settings.h"
#include "shader.h"
#include "content_cao.h"
#include "porting.h"
#include <algorithm>
#include "client/renderingengine.h"

/*
	ClientEnvironment
*/

ClientEnvironment::ClientEnvironment(irr_ptr<ClientMap> map,
	ITextureSource *texturesource, Client *client):
	Environment(client),
	m_map(std::move(map)),
	m_texturesource(texturesource),
	m_client(client)
{
}

ClientEnvironment::~ClientEnvironment()
{
	m_ao_manager.clear();

	for (auto &simple_object : m_simple_objects) {
		delete simple_object;
	}

	m_map.reset();

	delete m_local_player;
}

Map &ClientEnvironment::getMap()
{
	return *m_map;
}

ClientMap &ClientEnvironment::getClientMap()
{
	return *m_map;
}

void ClientEnvironment::setLocalPlayer(LocalPlayer *player)
{
	/*
		It is a failure if already is a local player
	*/
	FATAL_ERROR_IF(m_local_player != NULL,
		"Local player already allocated");

	m_local_player = player;
}

void ClientEnvironment::step(f32 dtime, double uptime, unsigned int max_cycle_ms)
{

	TimeTaker timer0("ClientEnvironment::step()");

	/* Step time of day */
	stepTimeOfDay(dtime);

	// Get some settings
	bool fly_allowed = m_client->checkLocalPrivilege("fly");
	bool free_move = fly_allowed && g_settings->getBool("free_move");

	// Get local player
	LocalPlayer *lplayer = getLocalPlayer();
	assert(lplayer);
	// collision info queue
	std::vector<CollisionInfo> player_collisions;

	/*
		Get the speed the player is going
	*/
	bool is_climbing = lplayer->is_climbing;

	f32 player_speed = lplayer->getSpeed().getLength();
	v3f pf = lplayer->getPosition();

	/*
		Maximum position increment
	*/
	f32 position_max_increment = 0.1*BS;

	// Maximum time increment (for collision detection etc)
	// time = distance / speed
	f32 dtime_max_increment = 1;
	if(player_speed > 0.001)
		dtime_max_increment = position_max_increment / player_speed;

	// Maximum time increment is 10ms or lower
	if(dtime_max_increment > 0.01)
		dtime_max_increment = 0.01;

	if (dtime_max_increment * m_move_max_loop < dtime)
		dtime_max_increment = dtime / m_move_max_loop;

	static constexpr float DTIME_MAX = 2.0;

	// Don't allow overly huge dtime
	if(dtime > DTIME_MAX)
		dtime = DTIME_MAX;

	if (player_speed <= 0.01 && dtime < 0.1)
		dtime_max_increment = dtime;

	/*
		Stuff that has a maximum time increment
	*/

	u32 breaked = 0;
	const auto lend_ms = porting::getTimeMs() + max_cycle_ms;
	u32 loopcount = 0;

	u32 steps = std::ceil(dtime / dtime_max_increment);
	f32 dtime_part = dtime / steps;
	for (; steps > 0; --steps) {
		/*
			Local player handling
		*/

		// Control local player
		lplayer->applyControl(dtime_part, this);

		// Apply physics
		lplayer->gravity = 0;
		if (!free_move) {
			f32 resistance_factor = 0.3f;
			// Gravity
			if (!is_climbing && !lplayer->in_liquid) {
				// HACK the factor 2 for gravity is arbitrary and should be removed eventually
				lplayer->gravity = 2 * lplayer->movement_gravity * lplayer->physics_override.gravity;

				resistance_factor = 0.97; // todo maybe depend on speed; 0.96 = ~100 nps max
				resistance_factor += (1.0 - resistance_factor) *
									(1 - (MAX_MAP_GENERATION_LIMIT - pf.Y / BS) /
													MAX_MAP_GENERATION_LIMIT);
			}

			// Liquid floating / sinking
			if (!is_climbing && lplayer->in_liquid &&
					!lplayer->swimming_vertical &&
					!lplayer->swimming_pitch)
				// HACK the factor 2 for gravity is arbitrary and should be removed eventually
				lplayer->gravity = 2 * lplayer->movement_liquid_sink * lplayer->physics_override.liquid_sink;

			// Movement resistance
			if (lplayer->move_resistance > 0) {
				v3f speed = lplayer->getSpeed();

				// How much the node's move_resistance blocks movement, ranges
				// between 0 and 1. Should match the scale at which liquid_viscosity
				// increase affects other liquid attributes.
				//static const f32 resistance_factor = 0.3f;
				float fluidity = lplayer->movement_liquid_fluidity;
				fluidity *= MYMAX(1.0f, lplayer->physics_override.liquid_fluidity);
				fluidity = MYMAX(0.001f, fluidity); // prevent division by 0
				float fluidity_smooth = lplayer->movement_liquid_fluidity_smooth;
				fluidity_smooth *= lplayer->physics_override.liquid_fluidity_smooth;
				fluidity_smooth = MYMAX(0.0f, fluidity_smooth);

				v3f d_wanted;
				bool in_liquid_stable = lplayer->in_liquid_stable || lplayer->in_liquid;
				if (in_liquid_stable)
					resistance_factor = 0.3;
				if (in_liquid_stable)
					d_wanted = -speed / fluidity;
				else
					d_wanted = -speed / BS;
				f32 dl = d_wanted.getLength();
				if (in_liquid_stable)
					dl = MYMIN(dl, fluidity_smooth);


				if (lplayer->move_resistance < 1) // rewrite this shit
					dl /= 2;


				dl *= (lplayer->move_resistance * resistance_factor) +
					(1 - resistance_factor);
				v3f d = d_wanted.normalize() * (dl * dtime_part * 100.0f);
				speed += d;

				lplayer->setSpeed(speed);
			}
		}

		/*
			Move the local player.
			This also does collision detection.
		*/

		lplayer->move(dtime_part, this, &player_collisions);

		++loopcount;
		if (porting::getTimeMs() >= lend_ms) {
			breaked = loopcount;
			break;
		}
	}

	if (breaked && m_move_max_loop > loopcount)
		--m_move_max_loop;
	if (!breaked && m_move_max_loop < 5)
		++m_move_max_loop;


	bool player_immortal = false;
	f32 player_fall_factor = 1.0f;
	GenericCAO *playercao = lplayer->getCAO();
	if (playercao) {
		player_immortal = playercao->isImmortal();
		int addp_p = itemgroup_get(playercao->getGroups(),
			"fall_damage_add_percent");
		// convert armor group into an usable fall damage factor
		player_fall_factor = 1.0f + (float)addp_p / 100.0f;
	}

	if (dtime < DTIME_MAX || lplayer->getSpeed().getLength() > PLAYER_FALL_TOLERANCE_SPEED)
	for (const CollisionInfo &info : player_collisions) {
		v3f speed_diff = info.new_speed - info.old_speed;;
		// Handle only fall damage
		// (because otherwise walking against something in fast_move kills you)
		if ((speed_diff.Y < 0 || info.old_speed.Y >= 0)
			&& speed_diff.getLength() <= lplayer->movement_speed_fast * 1.1)
			continue;
/*
		// Get rid of other components
		
		speed_diff.X = 0;
		speed_diff.Z = 0;
*/		
		f32 pre_factor = 1; // 1 hp per node/s
		f32 tolerance = PLAYER_FALL_TOLERANCE_SPEED; // 5 without damage
		if (info.type == COLLISION_NODE) {
			const ContentFeatures &f = m_client->ndef()->
				get(m_map->getNode(info.node_p));
			// Determine fall damage modifier
			int addp_n = itemgroup_get(f.groups, "fall_damage_add_percent");
			// convert node group to an usable fall damage factor
			f32 node_fall_factor = 1.0f + (float)addp_n / 100.0f;
			// combine both player fall damage modifiers
			pre_factor = node_fall_factor * player_fall_factor;
		}
		float speed = pre_factor * speed_diff.getLength();

		if (speed > tolerance && !player_immortal && pre_factor > 0.0f) {
			f32 damage_f = (speed - tolerance) / BS;
			u16 damage = (u16)MYMIN(damage_f + 0.5, U16_MAX);
			if (damage != 0) {
				damageLocalPlayer(damage, true);
				m_client->getEventManager()->put(
					new SimpleTriggerEvent(MtEvent::PLAYER_FALLING_DAMAGE));
			}
		}
	}

	if (m_client->modsLoaded())
		m_script->environment_step(dtime);

	// Update lighting on local player (used for wield item)
	u32 day_night_ratio = getDayNightRatio();
	{
		// Get node at head

		// On InvalidPositionException, use this as default
		// (day: LIGHT_SUN, night: 0)
		MapNode node_at_lplayer(CONTENT_AIR, 0x0f, 0);

		v3s16 p = lplayer->getLightPosition();
		node_at_lplayer = m_map->getNode(p);

		u16 light = getInteriorLight(node_at_lplayer, 0, m_client->ndef());
		lplayer->light_color = encode_light(light, 0); // this transfers light.alpha
		final_color_blend(&lplayer->light_color, light, day_night_ratio);
	}

	/*
		Step active objects and update lighting of them
	*/

	bool update_lighting = m_active_object_light_update_interval.step(dtime, 0.21);
	auto cb_state = [this, dtime, update_lighting, day_night_ratio] (const ClientActiveObjectPtr &cao) {
		// Step object
		cao->step(dtime, this);

		if (update_lighting)
			cao->updateLight(day_night_ratio);
	};
	static thread_local async_step_runner m_ao_manager_async;
	m_ao_manager_async.step([this, dtime = dtime, cb_state=cb_state]{

	m_ao_manager.step(dtime, cb_state);

	});
	/*
		Step and handle simple objects
	*/
	g_profiler->avg("ClientEnv: CSO count [#]", m_simple_objects.size());
	for (auto i = m_simple_objects.begin(); i != m_simple_objects.end();) {
		ClientSimpleObject *simple = *i;

		simple->step(dtime);
		if(simple->m_to_be_removed) {
			delete simple;
			i = m_simple_objects.erase(i);
		}
		else {
			++i;
		}
	}
}

void ClientEnvironment::addSimpleObject(ClientSimpleObject *simple)
{
	m_simple_objects.push_back(simple);
}

GenericCAO* ClientEnvironment::getGenericCAO(u16 id)
{
	ClientActiveObject *obj = getActiveObject(id);
	if (obj && obj->getType() == ACTIVEOBJECT_TYPE_GENERIC)
		return (GenericCAO*) obj;

	return NULL;
}

u16 ClientEnvironment::addActiveObject(std::shared_ptr<ClientActiveObject> object)
{
	auto obj = object.get();
	// Register object. If failed return zero id
	if (!m_ao_manager.registerObject(std::move(object)))
		return 0;

	obj->addToScene(m_texturesource, m_client->getSceneManager());

	// Update lighting immediately
	obj->updateLight(getDayNightRatio());
	return obj->getId();
}

void ClientEnvironment::addActiveObject(u16 id, u8 type,
	const std::string &init_data)
{
	std::unique_ptr<ClientActiveObject> obj =
		ClientActiveObject::create((ActiveObjectType) type, m_client, this);
	if (!obj) {
		infostream<<"ClientEnvironment::addActiveObject(): "
			<<"id="<<id<<" type="<<type<<": Couldn't create object"
			<<std::endl;
		return;
	}

	obj->setId(id);

	try {
		obj->initialize(init_data);
	} catch(SerializationError &e) {
		errorstream<<"ClientEnvironment::addActiveObject():"
			<<" id="<<id<<" type="<<type
			<<": SerializationError in initialize(): "
			<<e.what()
			<<": init_data="<<serializeJsonString(init_data)
			<<std::endl;

			//delete obj;
			return;
	}

	u16 new_id = addActiveObject(std::move(obj));
	// Object initialized:
	if (ClientActiveObject *obj2 = getActiveObject(new_id)) {
		// Final step is to update all children which are already known
		// Data provided by AO_CMD_SPAWN_INFANT
		const auto &children = obj2->getAttachmentChildIds();
		for (auto c_id : children) {
			if (auto o = getActiveObject(c_id))
				o->updateAttachments();
		}
	}
}


void ClientEnvironment::removeActiveObject(u16 id)
{
	// Get current attachment childs to detach them visually
	std::unordered_set<ClientActiveObject::object_t> attachment_childs;
	if (auto obj = getActiveObject(id))
		attachment_childs = obj->getAttachmentChildIds();

	m_ao_manager.removeObject(id);

	// Perform a proper detach in Irrlicht
	for (auto c_id : attachment_childs) {
		if (auto child = getActiveObject(c_id))
			child->updateAttachments();
	}
}

void ClientEnvironment::processActiveObjectMessage(u16 id, const std::string &data)
{
	auto obj = getActiveObject(id);
	if (obj == NULL) {
		infostream << "ClientEnvironment::processActiveObjectMessage():"
			<< " got message for id=" << id << ", which doesn't exist."
			<< std::endl;
		return;
	}

	try {
		obj->processMessage(data);
	} catch (SerializationError &e) {
		errorstream<<"ClientEnvironment::processActiveObjectMessage():"
			<< " id=" << id << " type=" << obj->getType()
			<< " SerializationError in processMessage(): " << e.what()
			<< std::endl;
	}
}

/*
	Callbacks for activeobjects
*/

void ClientEnvironment::damageLocalPlayer(u16 damage, bool handle_hp)
{
	LocalPlayer *lplayer = getLocalPlayer();
	assert(lplayer);

	if (handle_hp) {
		if (lplayer->hp > damage)
			lplayer->hp -= damage;
		else
			lplayer->hp = 0;
	}

	ClientEnvEvent event;
	event.type = CEE_PLAYER_DAMAGE;
	event.player_damage.amount = damage;
	event.player_damage.send_to_server = handle_hp;
	m_client_event_queue.push(event);
}

/*
	Client likes to call these
*/

ClientEnvEvent ClientEnvironment::getClientEnvEvent()
{
	FATAL_ERROR_IF(m_client_event_queue.empty(),
			"ClientEnvironment::getClientEnvEvent(): queue is empty");

	ClientEnvEvent event = m_client_event_queue.front();
	m_client_event_queue.pop();
	return event;
}

void ClientEnvironment::getSelectedActiveObjects(
	const core::line3d<f32> &shootline_on_map,
	std::vector<PointedThing> &objects,
	const std::optional<Pointabilities> &pointabilities)
{
	auto allObjects = m_ao_manager.getActiveSelectableObjects(shootline_on_map);
	const v3f line_vector = shootline_on_map.getVector();

	for (const auto &allObject : allObjects) {
		ClientActiveObject *obj = allObject.obj.get();
		aabb3f selection_box{{0.0f, 0.0f, 0.0f}};
		if (!obj->getSelectionBox(&selection_box))
			continue;

		v3f current_intersection;
		v3f current_normal, current_raw_normal;
		const v3f rel_pos = shootline_on_map.start - obj->getPosition();
		bool collision;
		GenericCAO* gcao = dynamic_cast<GenericCAO*>(obj);
		if (gcao != nullptr && gcao->getProperties().rotate_selectionbox) {
			gcao->getSceneNode()->updateAbsolutePosition();
			const v3f deg = obj->getSceneNode()->getAbsoluteTransformation().getRotationDegrees();
			collision = boxLineCollision(selection_box, deg,
				rel_pos, line_vector, &current_intersection, &current_normal, &current_raw_normal);
		} else {
			collision = boxLineCollision(selection_box, rel_pos, line_vector,
				&current_intersection, &current_normal);
			current_raw_normal = current_normal;
		}
		if (collision) {
			PointabilityType pointable;
			if (pointabilities) {
				if (gcao->isPlayer()) {
					pointable = pointabilities->matchPlayer(gcao->getGroups()).value_or(
							gcao->getProperties().pointable);
				} else {
					pointable = pointabilities->matchObject(gcao->getName(),
							gcao->getGroups()).value_or(gcao->getProperties().pointable);
				}
			} else {
				pointable = gcao->getProperties().pointable;
			}
			if (pointable != PointabilityType::POINTABLE_NOT) {
				current_intersection += obj->getPosition();
				objects.emplace_back(obj->getId(), current_intersection, current_normal, current_raw_normal,
					(current_intersection - shootline_on_map.start).getLengthSQ(), pointable);
			}
		}
	}
}

void ClientEnvironment::updateFrameTime(bool is_paused)
{
	// if paused, m_frame_time_pause_accumulator increases by dtime,
	// otherwise, m_frame_time increases by dtime
	if (is_paused) {
		m_frame_dtime = 0;
		m_frame_time_pause_accumulator = porting::getTimeMs() - m_frame_time;
	}
	else {
		auto new_frame_time = porting::getTimeMs() - m_frame_time_pause_accumulator;
		m_frame_dtime = new_frame_time - MYMAX(m_frame_time, m_frame_time_pause_accumulator);
		m_frame_time = new_frame_time;
	}
}
