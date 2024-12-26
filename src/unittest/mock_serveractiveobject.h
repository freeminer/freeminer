// Luanti
// SPDX-License-Identifier: LGPL-2.1-or-later
// Copyright (C) 2022 Minetest core developers & community

#include <server/serveractiveobject.h>
#include "irr_v3d.h"

class MockServerActiveObject : public ServerActiveObject
{
public:
	MockServerActiveObject(ServerEnvironment *env = nullptr, v3opos_t p = v3opos_t()) :
		ServerActiveObject(env, p) {}

	virtual ActiveObjectType getType() const { return ACTIVEOBJECT_TYPE_TEST; }
	virtual bool getCollisionBox(aabb3o *toset) const { return false; }
	virtual bool getSelectionBox(aabb3f *toset) const { return false; }
	virtual bool collideWithObjects() const { return false; }
};
