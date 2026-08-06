// Luanti
// SPDX-License-Identifier: LGPL-2.1-or-later
// Copyright (C) 2010-2018 nerzhul, Loic BLOT <loic.blot@unix-experience.fr>

#pragma once

#include <functional>
#include <vector>
#include "../activeobjectmgr.h"
#include "clientobject.h"
#include "irrlichttypes.h"

namespace client
{
class ActiveObjectMgr final : public ::ActiveObjectMgr<ClientActiveObject>
{
public:
	~ActiveObjectMgr() override;

	void step(float dtime,
			const std::function<void(const ClientActiveObjectPtr &)> &f) override;
	// end_ms is an absolute porting::getTimeMs() deadline; zero disables it.
	void step(float dtime,
			const std::function<void(const ClientActiveObjectPtr &)> &f,
			u64 end_ms);
	bool registerObject(std::shared_ptr<ClientActiveObject> obj) override;
	void removeObject(u16 id) override;

	void getActiveObjects(const v3opos_t &origin, opos_t max_d,
			std::vector<DistanceSortedActiveObject> &dest);

	/// Gets all CAOs whose selection boxes may intersect the @p shootline.
	/// @note CAOs without a selection box are not returned.
	/// @note Distances are along the @p shootline.
	std::vector<DistanceSortedActiveObject> getActiveSelectableObjects(const core::line3d<opos_t> &shootline);

private:
	// Object IDs are ordered, so this remains a stable cursor across removals.
	u16 m_step_resume_after = 0;
};
} // namespace client
