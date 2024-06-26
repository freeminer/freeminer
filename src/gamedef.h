/*
gamedef.h
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
#include <vector>
#include "irrlichttypes.h"

class IItemDefManager;
class NodeDefManager;
class ICraftDefManager;
class ITextureSource;
class IShaderSource;
class IRollbackManager;
class EmergeManager;
class Camera;
class ModChannel;
class ModStorage;
class ModStorageDatabase;

namespace irr::scene {
	class IAnimatedMesh;
	class ISceneManager;
}

struct SubgameSpec;
struct ModSpec;
/*
	An interface for fetching game-global definitions like tool and
	mapnode properties
*/

class IGameDef
{
public:
	// These are thread-safe IF they are not edited while running threads.
	// Thus, first they are set up and then they are only read.
	virtual IItemDefManager* getItemDefManager()=0;
	virtual const NodeDefManager* getNodeDefManager()=0;
	virtual ICraftDefManager* getCraftDefManager()=0;

	// Used for keeping track of names/ids of unknown nodes
	virtual u16 allocateUnknownNodeId(const std::string &name)=0;

	// Only usable on the server, and NOT thread-safe. It is usable from the
	// environment thread.
	virtual IRollbackManager* getRollbackManager() { return NULL; }

	// Shorthands
	// TODO: these should be made const-safe so that a const IGameDef* is
	//       actually usable
	IItemDefManager  *idef()     { return getItemDefManager(); }
	const NodeDefManager  *ndef() { return getNodeDefManager(); }
	ICraftDefManager *cdef()     { return getCraftDefManager(); }
	IRollbackManager *rollback() { return getRollbackManager(); }

	virtual const std::vector<ModSpec> &getMods() const = 0;
	virtual const ModSpec* getModSpec(const std::string &modname) const = 0;
	virtual const SubgameSpec* getGameSpec() const { return nullptr; }
	virtual std::string getWorldPath() const { return ""; }
	virtual ModStorageDatabase *getModStorageDatabase() = 0;

	virtual bool joinModChannel(const std::string &channel) = 0;
	virtual bool leaveModChannel(const std::string &channel) = 0;
	virtual bool sendModChannelMessage(const std::string &channel,
		const std::string &message) = 0;
	virtual ModChannel *getModChannel(const std::string &channel) = 0;
};
