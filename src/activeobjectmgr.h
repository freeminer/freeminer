// Luanti
// SPDX-License-Identifier: LGPL-2.1-or-later
// Copyright (C) 2010-2018 nerzhul, Loic BLOT <loic.blot@unix-experience.fr>

#pragma once

#include <memory>
#include "debug.h"
#include "util/container.h"
#include "irrlichttypes.h"
#include "util/basic_macros.h"
#include "threading/concurrent_unordered_map.h"

class TestClientActiveObjectMgr;
class TestServerActiveObjectMgr;

template <typename T>
class ActiveObjectMgr
{
	friend class ::TestServerActiveObjectMgr;

public:
	using TPtr = std::shared_ptr<T>;

	ActiveObjectMgr() = default;
	DISABLE_CLASS_COPY(ActiveObjectMgr);

	virtual ~ActiveObjectMgr()
	{
/*
		SANITY_CHECK(m_active_objects.empty());
*/		
		// Note: Do not call clear() here. The derived class is already half
		// destructed.
	}

	virtual void step(float dtime, const std::function<void(const TPtr&)> &f) = 0;
	virtual bool registerObject(std::shared_ptr<T> obj) = 0;
	virtual void removeObject(u16 id) = 0;

	void clear()
	{
		// on_destruct could add new objects so this has to be a loop
		do {
			for (auto &it : m_active_objects.iter()) {
/*
				if (!it.second)
					continue;
*/
				m_active_objects.remove(it.first);
			}
		} while (!m_active_objects.empty());
	}

	TPtr getActiveObject(u16 id)
	{
		//const auto lock = m_active_objects.lock_shared_rec(); //prelock
		return m_active_objects.get(id);
	}

protected:
	u16 getFreeId() const
	{
		// try to reuse id's as late as possible
		static thread_local u16 last_used_id = 0;
		u16 startid = last_used_id;
		while (!isFreeId(++last_used_id)) {
			if (last_used_id == startid)
				return 0;
		}

		return last_used_id;
	}

	bool isFreeId(u16 id) const
	{
		//const auto lock = m_active_objects.lock_shared_rec(); // prelock
		return id != 0 && !m_active_objects.get(id);
	}

	// Note that this is ordered to fix #10985
	ModifySafeMap<u16, std::shared_ptr<T>> m_active_objects;

	//concurrent_unordered_map<u16, TPtr> m_active_objects;
	std::vector<TPtr> m_active_objects_deleted;

};
