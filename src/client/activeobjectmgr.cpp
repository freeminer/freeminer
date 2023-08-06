/*
Minetest
Copyright (C) 2010-2018 nerzhul, Loic BLOT <loic.blot@unix-experience.fr>

This program is free software; you can redistribute it and/or modify
it under the terms of the GNU Lesser General Public License as published by
the Free Software Foundation; either version 2.1 of the License, or
(at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU Lesser General Public License for more details.

You should have received a copy of the GNU Lesser General Public License along
with this program; if not, write to the Free Software Foundation, Inc.,
51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
*/

#include <cmath>
#include <log.h>
#include "irr_v3d.h"
#include "irrlichttypes.h"
#include "profiler.h"
#include "activeobjectmgr.h"
#include "util/numeric.h"

namespace client
{

void ActiveObjectMgr::clear()
{
	// delete active objects

	for (auto &active_object : m_active_objects) {
		//delete active_object.second;
		// Object must be marked as gone when children try to detach
		active_object.second = nullptr;
	}

	m_active_objects.clear();
}

void ActiveObjectMgr::step(
		float dtime, const std::function<void(const ClientActiveObjectPtr&)> &f)
{
	g_profiler->avg("ActiveObjectMgr: CAO count [#]", m_active_objects.size());
	for (auto &ao_it : m_active_objects) {
		f(ao_it.second);
	}
}

// clang-format off
bool ActiveObjectMgr::registerObject(ClientActiveObject *obj)
{
	assert(obj); // Pre-condition
	if (obj->getId() == 0) {
		u16 new_id = getFreeId();
		if (new_id == 0) {
			infostream << "Client::ActiveObjectMgr::registerObject(): "
					<< "no free id available" << std::endl;

			delete obj;
			return false;
		}
		obj->setId(new_id);
	}

	if (!isFreeId(obj->getId())) {
		infostream << "Client::ActiveObjectMgr::registerObject(): "
				<< "id is not free (" << obj->getId() << ")" << std::endl;
		delete obj;
		return false;
	}
	infostream << "Client::ActiveObjectMgr::registerObject(): "
			<< "added (id=" << obj->getId() << ")" << std::endl;
	m_active_objects.insert_or_assign(obj->getId(), ClientActiveObjectPtr{obj});
	return true;
}

void ActiveObjectMgr::removeObject(u16 id)
{
	verbosestream << "Client::ActiveObjectMgr::removeObject(): "
			<< "id=" << id << std::endl;
	auto obj = getActiveObject(id);
	if (!obj) {
		infostream << "Client::ActiveObjectMgr::removeObject(): "
				<< "id=" << id << " not found" << std::endl;
		return;
	}

	m_active_objects.erase(id);

	obj->removeFromScene(true);
	//delete obj;
}

// clang-format on
void ActiveObjectMgr::getActiveObjects(const v3opos_t &origin, opos_t max_d,
		std::vector<DistanceSortedActiveObject> &dest)
{
	opos_t max_d2 = max_d * max_d;
	for (auto &ao_it : m_active_objects) {
		const auto obj = ao_it.second;

		opos_t d2 = (obj->getPosition() - origin).getLengthSQ();

		if (d2 > max_d2)
			continue;

		dest.emplace_back(obj.get(), d2);
	}
}

void ActiveObjectMgr::getActiveSelectableObjects(const core::line3d<opos_t> &shootline,
		std::vector<DistanceSortedActiveObject> &dest)
{
	// Imagine a not-axis-aligned cuboid oriented into the direction of the shootline,
	// with the width of the object's selection box radius * 2 and with length of the
	// shootline (+selection box radius forwards and backwards). We check whether
	// the selection box center is inside this cuboid.

	f32 max_d = shootline.getLength();
	v3opos_t dir = shootline.getVector().normalize();
	// arbitrary linearly independent vector and orthogonal dirs
	v3opos_t li2dir = dir + (std::fabs(dir.X) < 0.5f ? v3opos_t(1,0,0) : v3opos_t(0,1,0));
	v3opos_t dir_ortho1 = dir.crossProduct(li2dir).normalize();
	v3opos_t dir_ortho2 = dir.crossProduct(dir_ortho1);

	for (auto &ao_it : m_active_objects) {
		auto obj = ao_it.second;

		aabb3f selection_box;
		if (!obj->getSelectionBox(&selection_box))
			continue;

		// possible optimization: get rid of the sqrt here
		f32 selection_box_radius = selection_box.getRadius();

		v3opos_t pos_diff = obj->getPosition() + v3fToOpos(selection_box.getCenter()) - shootline.start;

		f32 d = dir.dotProduct(pos_diff);

		// backward- and far-plane
		if (d + selection_box_radius < 0.0f || d - selection_box_radius > max_d)
			continue;

		// side-planes
		if (std::fabs(dir_ortho1.dotProduct(pos_diff)) > selection_box_radius
				|| std::fabs(dir_ortho2.dotProduct(pos_diff)) > selection_box_radius)
			continue;

		dest.emplace_back(obj.get(), d);
	}
}

} // namespace client
