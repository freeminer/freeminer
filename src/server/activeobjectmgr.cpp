// Luanti
// SPDX-License-Identifier: LGPL-2.1-or-later
// Copyright (C) 2010-2018 nerzhul, Loic BLOT <loic.blot@unix-experience.fr>

#include <log.h>
#include "mapblock.h"
#include "profiler.h"
#include "server/serveractiveobject.h"
#include "activeobjectmgr.h"

namespace server
{

void ActiveObjectMgr::deferDelete(const ServerActiveObjectPtr& obj) {
	obj->markForRemoval();
	m_objects_to_delete.emplace_back(obj);
}

ActiveObjectMgr::~ActiveObjectMgr()
{
	if (!m_active_objects.empty()) {
		warningstream << "server::ActiveObjectMgr::~ActiveObjectMgr(): not cleared."
				<< std::endl;
		clear();
	}
}

void ActiveObjectMgr::clearIf(const std::function<bool(const ServerActiveObjectPtr&, u16)> &cb)
{
/* fmtodo:?
	decltype(m_active_objects)::full_type active_objects;

	{
		// bad copy: avoid deadlocks with locks in cb
		const auto lock = m_active_objects.try_lock_shared_rec();
		if (!lock->owns_lock())
			return;
		active_objects = m_active_objects;
	}

	for (auto &[id, it] : active_objects) {
		if (cb(it, id)) {
			// erase by id, `it` can be invalid now
			//removeObject(id);
			objects_to_remove.emplace_back(id);
		}
	}
	if (objects_to_remove.empty())
		return;

   {
	const auto lock = m_active_objects.try_lock_unique_rec();
	if (!lock->owns_lock())
		return;

	// Remove references from m_active_objects
	for (u16 i : objects_to_remove) {
		//m_active_objects.erase(i);
		removeObject(i);
	}
   }
	objects_to_remove.clear();

	return;
// === */

	for (auto &it : m_active_objects.iter()) {
		if (!it.second)
			continue;
		if (cb(it.second, it.first)) {
			// Remove reference from m_active_objects
			m_active_objects.remove(it.first);
		}
	}
}

void ActiveObjectMgr::step(
		float dtime, const std::function<void(const ServerActiveObjectPtr&)> &f)
{
/* fmtodo
	std::swap(m_objects_to_delete, m_objects_to_delete_2);
	m_objects_to_delete.clear();

	std::vector<ServerActiveObjectPtr> active_objects;
	active_objects.reserve(m_active_objects.size());
	{
		const auto lock = m_active_objects.try_lock_unique_rec(); //prelock
		if (!lock->owns_lock())
			return;
		g_profiler->avg("ActiveObjectMgr: SAO count [#]", m_active_objects.size());
		for (const auto &ao_it : m_active_objects) {
			active_objects.emplace_back(ao_it.second);
		}
	}
	// unlocked
	for (const auto &ao_it : active_objects) {
		f(ao_it);
	}
# if 0
*/

    std::vector<ServerActiveObjectPtr> active_objects;

    {
	const auto lock =  m_active_objects.try_lock_shared_rec();
	if (!lock->owns_lock())
		return;

	g_profiler->avg("ActiveObjectMgr: SAO count [#]", m_active_objects.size());
	size_t count = 0;

    active_objects.reserve(m_active_objects.size());

	for (auto &ao_it : m_active_objects.iter()) {
		if (!ao_it.second)
			continue;
		count++;
		active_objects.emplace_back(ao_it.second);
		// f(ao_it.second);
	}
    }

    size_t count = 0;
	for (const auto &ao : active_objects) {
		f(ao);
		++count;
	}

	g_profiler->avg("ActiveObjectMgr: SAO count [#]", count);
}

bool ActiveObjectMgr::registerObject(std::shared_ptr<ServerActiveObject> obj)
{
	if (!obj) return false; // Pre-condition
	if (obj->getId() == 0) {
		u16 new_id = getFreeId();
		if (new_id == 0) {
			errorstream << "Server::ActiveObjectMgr::addActiveObjectRaw(): "
					<< "no free id available" << std::endl;
			return false;
		}
		obj->setId(new_id);
	} else {
		verbosestream << "Server::ActiveObjectMgr::addActiveObjectRaw(): "
				<< "supplied with id " << obj->getId() << std::endl;
	}

	if (!isFreeId(obj->getId())) {
		errorstream << "Server::ActiveObjectMgr::addActiveObjectRaw(): "
				<< "id is not free (" << obj->getId() << ")" << std::endl;
		return false;
	}

	if (objectpos_over_limit(obj->getBasePosition())) {
		v3f p = obj->getBasePosition();
		warningstream << "Server::ActiveObjectMgr::addActiveObjectRaw(): "
				<< "object position (" << p.X << "," << p.Y << "," << p.Z
				<< ") outside maximum range" << std::endl;
		return false;
	}

	auto obj_id = obj->getId();
	m_active_objects.put(obj_id, std::move(obj));

#if !NDEBUG
	auto new_size = m_active_objects.size();
	verbosestream << "Server::ActiveObjectMgr::addActiveObjectRaw(): "
			<< "Added id=" << obj_id << "; there are now ";
	if (new_size == decltype(m_active_objects)::unknown)
		verbosestream << "???";
	else
		verbosestream << new_size;
	verbosestream << " active objects." << std::endl;
#endif
	return true;
}

void ActiveObjectMgr::removeObject(u16 id)
{
	verbosestream << "Server::ActiveObjectMgr::removeObject(): "
			<< "id=" << id << std::endl;

	// this will take the object out of the map and then destruct it
	bool ok = m_active_objects.remove(id);
	if (!ok) {
		infostream << "Server::ActiveObjectMgr::removeObject(): "
				<< "id=" << id << " not found" << std::endl;
	}
}

void ActiveObjectMgr::invalidateActiveObjectObserverCaches()
{
	const auto lock = m_active_objects.try_lock_shared_rec();
	if (!lock->owns_lock()) {
		return;
	}

	for (auto &active_object : m_active_objects.iter()) {
		ServerActiveObject *obj = active_object.second.get();
		if (!obj)
			continue;
		obj->invalidateEffectiveObservers();
	}
}

void ActiveObjectMgr::getObjectsInsideRadius(const v3f &pos, float radius,
		std::vector<ServerActiveObjectPtr> &result,
		std::function<bool(const ServerActiveObjectPtr &obj)> include_obj_cb)
{

#if 0
	std::vector<ServerActiveObjectPtr> active_objects;
	active_objects.reserve(m_active_objects.size());
	{
		/*const auto lock = m_active_objects.try_lock_unique_rec(); //prelock
		if (!lock->owns_lock())
			return;
		*/	
		// bad copy: avoid deadlocks with locks in cb
		for (const auto &ao_it : m_active_objects) {
			active_objects.emplace_back(ao_it.second);
		}
	}
#endif

	const auto lock = m_active_objects.try_lock_unique_rec(); //prelock
	if (!lock->owns_lock())
		return;


	float r2 = radius * radius;
	for (auto &activeObject : m_active_objects.iter()) {
		auto obj = activeObject.second;
		if (!obj)
			continue;
		const v3f &objectpos = obj->getBasePosition();
		if (objectpos.getDistanceFromSQ(pos) > r2)
			continue;

		if (!include_obj_cb || include_obj_cb(obj))
			result.push_back(obj);
	}
}

void ActiveObjectMgr::getObjectsInArea(const aabb3f &box,
		std::vector<ServerActiveObjectPtr> &result,
		std::function<bool(const ServerActiveObjectPtr &obj)> include_obj_cb)
{
/* fmtodo:
	std::vector<ServerActiveObjectPtr> active_objects;
	active_objects.reserve(m_active_objects.size());
	{
		const auto lock = m_active_objects.try_lock_unique_rec(); //prelock
		if (!lock->owns_lock())
			return;
		// bad copy: avoid deadlocks with locks in cb
		for (const auto &ao_it : m_active_objects) {
			active_objects.emplace_back(ao_it.second);
		}

	}

	for (auto &obj : active_objects) {
*/

	for (auto &activeObject : m_active_objects.iter()) {
		auto obj = activeObject.second;
		if (!obj)
			continue;
		const v3f &objectpos = obj->getBasePosition();
		if (!box.isPointInside(objectpos))
			continue;

		if (!include_obj_cb || include_obj_cb(obj))
			result.push_back(obj);
	}
}

void ActiveObjectMgr::getAddedActiveObjectsAroundPos(
		const v3f &player_pos, const std::string &player_name,
		f32 radius, f32 player_radius,
		const std::set<u16> &current_objects,
		std::vector<u16> &added_objects)
{
#if 0
	decltype(m_active_objects)::full_type active_objects;
	{
		// bad copy: avoid deadlocks with locks in cb
		const auto lock = m_active_objects.try_lock_shared_rec();
		if (!lock->owns_lock())
			return;
		active_objects = m_active_objects;
	}
#endif

	int count = 0;
	/*
		Go through the object list,
		- discard removed/deactivated objects,
		- discard objects that are too far away,
		- discard objects that are found in current_objects,
		- discard objects that are not observed by the player.
		- add remaining objects to added_objects
	*/
	for (auto &ao_it : m_active_objects.iter()) {
		u16 id = ao_it.first;

		// Get object
		ServerActiveObject *object = ao_it.second.get();
		if (!object)
			continue;

		if (object->isGone())
			continue;

		f32 distance_f = object->getBasePosition().getDistanceFrom(player_pos);
		if (object->getType() == ACTIVEOBJECT_TYPE_PLAYER) {
			// Discard if too far
			if (distance_f > player_radius && player_radius != 0)
				continue;
		} else if (distance_f > radius)
			continue;

		if (!object->isEffectivelyObservedBy(player_name))
			continue;

		// Discard if already on current_objects
		auto n = current_objects.find(id);
		if (n != current_objects.end())
			continue;
		// Add to added_objects
		added_objects.push_back(id);

		if (++count > 10 && !current_objects.empty())
			break;   
	}
}

} // namespace server
