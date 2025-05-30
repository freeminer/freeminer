// Luanti
// SPDX-License-Identifier: LGPL-2.1-or-later
// Copyright (C) 2010-2018 nerzhul, Loic BLOT <loic.blot@unix-experience.fr>

#pragma once

#include <functional>
#include <set>
#include <shared_mutex>
#include <vector>
#include "../activeobjectmgr.h"
#include "serveractiveobject.h"
#include "util/k_d_tree.h"

namespace server
{
class ActiveObjectMgr final : public ::ActiveObjectMgr<ServerActiveObject>
{
//fm:
public:
	void deferDelete(const ServerActiveObjectPtr& obj);
private:
    std::vector<ServerActiveObjectPtr> m_objects_to_delete, m_objects_to_delete_2;
	std::vector<u16> objects_to_remove;
public:
	~ActiveObjectMgr() override;

	// If cb returns true, the obj will be deleted
	void clearIf(const std::function<bool(const ServerActiveObjectPtr&, u16)> &cb);
	void step(float dtime,
			const std::function<void(const ServerActiveObjectPtr&)> &f) override;
	bool registerObject(std::shared_ptr<ServerActiveObject> obj) override;
	void removeObject(u16 id) override;

	void invalidateActiveObjectObserverCaches();

	void updateObjectPos(u16 id, v3f pos);

	void getObjectsInsideRadius(v3f pos, float radius,
			std::vector<ServerActiveObjectPtr> &result,
			std::function<bool(const ServerActiveObjectPtr &obj)> include_obj_cb);
	void getObjectsInArea(const aabb3f &box,
			std::vector<ServerActiveObjectPtr> &result,
			std::function<bool(const ServerActiveObjectPtr &obj)> include_obj_cb);
	void getAddedActiveObjectsAroundPos(
			v3f player_pos, const std::string &player_name,
			f32 radius, f32 player_radius,
			const std::set<u16> &current_objects,
			std::vector<u16> &added_objects);

private:
	// k_d_tree::DynamicKdTrees<3, f32, u16> m_spatial_index;

	// tmp:
	template <uint8_t Dim, class Component, class Id>
	class FakeDynamicKdTrees
	{
		using Tree = k_d_tree::KdTree<Dim, Component, Id>;

	public:
		using Point = typename Tree::Point;

		void insert(const std::array<Component, Dim> &point, Id id) {}

		void remove(Id id) {}

		void update(const Point &newPos, Id id) {}

		template <typename F>
		void rangeQuery(const Point &min, const Point &max, const F &cb) const
		{
		}

		size_t size() const { return 0; }
	};

	FakeDynamicKdTrees<3, opos_t, u16> m_spatial_index;
	// ===
};
} // namespace server
