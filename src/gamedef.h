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

#ifndef GAMEDEF_HEADER
#define GAMEDEF_HEADER

#include <string>
#include "irrlichttypes.h"

class IItemDefManager;
class INodeDefManager;
class ICraftDefManager;
class ITextureSource;
class ISoundManager;
class IShaderSource;
class MtEventManager;
class IRollbackManager;
class EmergeManager;
namespace irr { namespace scene {
	class IAnimatedMesh;
	class ISceneManager;
}}

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
	virtual INodeDefManager* getNodeDefManager()=0;
	virtual ICraftDefManager* getCraftDefManager()=0;

	// This is always thread-safe, but referencing the irrlicht texture
	// pointers in other threads than main thread will make things explode.
	virtual ITextureSource* getTextureSource()=0;

	virtual IShaderSource* getShaderSource()=0;

	// Used for keeping track of names/ids of unknown nodes
	virtual u16 allocateUnknownNodeId(const std::string &name)=0;

	// Only usable on the client
	virtual ISoundManager* getSoundManager()=0;
	virtual MtEventManager* getEventManager()=0;
	virtual scene::IAnimatedMesh* getMesh(const std::string &filename)
	{ return NULL; }
	virtual scene::ISceneManager* getSceneManager()=0;

	// Only usable on the server, and NOT thread-safe. It is usable from the
	// environment thread.
	virtual IRollbackManager* getRollbackManager(){return NULL;}

	// Only usable on the server. Thread safe if not written while running threads.
	virtual EmergeManager *getEmergeManager() { return NULL; }

	// Used on the client
	virtual bool checkLocalPrivilege(const std::string &priv)
	{ return false; }

	// Shorthands
	IItemDefManager  *idef()     { return getItemDefManager(); }
	INodeDefManager  *ndef()     { return getNodeDefManager(); }
	ICraftDefManager *cdef()     { return getCraftDefManager(); }
	ITextureSource   *tsrc()     { return getTextureSource(); }
	ISoundManager    *sound()    { return getSoundManager(); }
	IShaderSource    *shsrc()    { return getShaderSource(); }
	MtEventManager   *event()    { return getEventManager(); }
	IRollbackManager *rollback() { return getRollbackManager();}
	EmergeManager    *emerge()   { return getEmergeManager(); }
};

#endif

