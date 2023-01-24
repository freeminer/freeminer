/*
clientobject.h
Copyright (C) 2010-2013 celeron55, Perttu Ahola <celeron55@gmail.com>
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

#include <memory>

#include "../irrlichttypes_extrabloated.h"
#include "activeobject.h"
#include <unordered_map>
#include <unordered_set>


class ClientEnvironment;
class ITextureSource;
class Client;
class IGameDef;
class LocalPlayer;
struct ItemStack;
class WieldMeshSceneNode;

class ClientActiveObject : public ActiveObject
{
public:
	ClientActiveObject(u16 id, Client *client, ClientEnvironment *env);
	virtual ~ClientActiveObject();

	virtual void addToScene(ITextureSource *tsrc, scene::ISceneManager *smgr) = 0;
	virtual void removeFromScene(bool permanent) {}

	virtual void updateLight(u32 day_night_ratio) {}

	virtual bool getCollisionBox(aabb3o *toset) const { return false; }
	virtual bool getSelectionBox(aabb3f *toset) const { return false; }
	virtual bool collideWithObjects() const { return false; }
	virtual const v3opos_t getPosition() const { return v3opos_t(0.0f); }
	virtual scene::ISceneNode *getSceneNode() const
	{ return NULL; }
	virtual scene::IAnimatedMeshSceneNode *getAnimatedMeshSceneNode() const
	{ return NULL; }
	virtual bool isLocalPlayer() const { return false; }

	virtual ClientActiveObject *getParent() const { return nullptr; };
	virtual const std::unordered_set<int> &getAttachmentChildIds() const
	{ static std::unordered_set<int> rv; return rv; }
	virtual void updateAttachments() {};

	virtual bool doShowSelectionBox() { return true; }

	// Step object in time
	virtual void step(float dtime, ClientEnvironment *env) {}

	// Process a message sent by the server side object
	virtual void processMessage(const std::string &data) {}

	virtual std::string infoText() { return ""; }
	virtual std::string debugInfoText() { return ""; }

	/*
		This takes the return value of
		ServerActiveObject::getClientInitializationData
	*/
	virtual void initialize(const std::string &data) {}

	// Create a certain type of ClientActiveObject
	static ClientActiveObject *create(ActiveObjectType type, Client *client,
		ClientEnvironment *env);

	// If returns true, punch will not be sent to the server
	virtual bool directReportPunch(v3f dir, const ItemStack *punchitem = nullptr,
		float time_from_last_punch = 1000000) { return false; }

protected:
	// Used for creating objects based on type
	typedef ClientActiveObject *(*Factory)(Client *client, ClientEnvironment *env);
	static void registerType(u16 type, Factory f);
	Client *m_client;
	ClientEnvironment *m_env;
private:
	// Used for creating objects based on type
	static std::unordered_map<u16, Factory> m_types;
};

class DistanceSortedActiveObject
{
public:
	ClientActiveObject *obj;

	DistanceSortedActiveObject(ClientActiveObject *a_obj, f32 a_d)
	{
		obj = a_obj;
		d = a_d;
	}

	bool operator < (const DistanceSortedActiveObject &other) const
	{
		return d < other.d;
	}

private:
	f32 d;
};

using ClientActiveObjectPtr = std::shared_ptr<ClientActiveObject>;
