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
			const std::function<void(ClientActiveObject *)> &f) override;
	bool registerObject(std::unique_ptr<ClientActiveObject> obj) override;
	void removeObject(u16 id) override;

	void getActiveObjects(const v3opos_t &origin, opos_t max_d,
			std::vector<DistanceSortedActiveObject> &dest);

	/// Gets all CAOs whose selection boxes may intersect the @p shootline.
	/// @note CAOs without a selection box are not returned.
	/// @note Distances are along the @p shootline.
	std::vector<DistanceSortedActiveObject> getActiveSelectableObjects(const core::line3d<opos_t> &shootline);
};
} // namespace client
