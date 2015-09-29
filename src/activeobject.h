/*
activeobject.h
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

#ifndef ACTIVEOBJECT_HEADER
#define ACTIVEOBJECT_HEADER

#include "debug.h"
#include "irr_aabb3d.h"
#include <string>
#include <map>

enum ActiveObjectType {
	ACTIVEOBJECT_TYPE_INVALID = 0,
	ACTIVEOBJECT_TYPE_TEST = 1,
// Deprecated stuff
	ACTIVEOBJECT_TYPE_ITEM = 2,
	ACTIVEOBJECT_TYPE_RAT = 3,
	ACTIVEOBJECT_TYPE_OERKKI1 = 4,
	ACTIVEOBJECT_TYPE_FIREFLY = 5,
	ACTIVEOBJECT_TYPE_MOBV2 = 6,
// End deprecated stuff
	ACTIVEOBJECT_TYPE_LUAENTITY = 7,
// Special type, not stored as a static object
	ACTIVEOBJECT_TYPE_PLAYER = 100,
// Special type, only exists as CAO
	ACTIVEOBJECT_TYPE_GENERIC = 101,
};
// Other types are defined in content_object.h

struct ActiveObjectMessage
{
	ActiveObjectMessage(u16 id_, bool reliable_=true, std::string data_=""):
		id(id_),
		reliable(reliable_),
		datastring(data_)
	{}

	u16 id;
	bool reliable;
	std::string datastring;
};

/*
	Parent class for ServerActiveObject and ClientActiveObject
*/
class ActiveObject
{
public:
	ActiveObject(u16 id): m_id(id)
	{}

	u16 getId()
	{
		return m_id;
	}

	void setId(u16 id)
	{
		m_id = id;
	}

	virtual ActiveObjectType getType() const = 0;

	virtual bool getCollisionBox(aabb3f *toset) = 0;
	virtual bool collideWithObjects() = 0;
protected:
	u16 m_id; // 0 is invalid, "no id"
};

/* active objects that have a type (i.e. all of them), will create a special activeobject intermediary
   to manage them always having the integer for that type. Just inherit from SomeActiveObject, TypedActiveObject<SOMETYPE>
   should be efficient and consistent...

   We have to have this template do the inheriting from ActiveObject, since C++ sucks at mixins.
   `class C: A, B` won't use A's virtual methods to fill in for pure virtual methods in B, or vice versa.
   http://stackoverflow.com/a/3092538/3833643

template <ActiveObjectType type, typename HacktiveObject>
class TypedActiveObject : public HacktiveObject {
public:
	template <typename A, typename B, typename C, typename CPLUSPLUSSUCKS>
	TypedActiveObject(A a, B b, C c, CPLUSPLUSSUCKS d) {
		HacktiveObject(a,b,c,d);
	}
	virtual ActiveObjectType getType() const {
		return type;
	}
	static ActiveObjectType getTypeStatic() {
		return type;
	}
};

Can't have an intermediary template class, because C++ sucks and won't skip it for constructors.
Can't have a mixin template class, because C++ sucks, and doesn't let you mix in virtual methods.
Stupid to have every typed object constructor have to go TypedBLahBLoh<SOME_TYPE_NUMBER,MyType>(...) instead of ServerActiveObject(..)

So we'll just use a C macro. ActiveObjectRegistry doesn't care, as long as getTypeStatic is implemented!
*/

#define HAVE_TYPE(type)							\
	public:										\
	virtual ActiveObjectType getType() const {	\
		return type;							\
	}											\
	/* did we mention that C++ sucks */			\
	static ActiveObjectType getTypeStatic() {	\
		return type;							\
	}


/* A registry for active objects... there should be two, one for the client one for the server.
   This lets the server tell the client or vice versa which object should be created, by sending
   an integer over the wire. The registry will create the correct class according to what that
   integer is associated with. Since server objects and client objects are initialized differently,
   and also can have the same integer for different objects (42 from server might mean differently
   than 42 from client), they need two registries. But they both basically do the same thing, so
   here's a template to create each of them simply and consistently.

   note: since the client (may have) an embedded server, it needs both a client and server registry.
*/

template<typename T>
class ActiveObjectRegistry {
	// The signature for various classes static "create" method.
	// class should define an inner Parameters class, for how client/server take different arguments
	// and also a static create function with this signature, that creates the class and returns its pointer.
	typedef T* (*Factory)(typename T::Parameters params);
public:
	// is it good for this type to not have a factory? default: false (see ServerRegistry::check)
	bool check(ActiveObjectType type) {
		return false;
	}

	T* create(ActiveObjectType type,
			  typename T::Parameters params) {
		//IGameDef *gamedef, TEnvironment *env)
		// Find factory function
		typename std::map<ActiveObjectType, Factory>::iterator n;
		n = m_types.find(type);
		if(n == m_types.end()) {
			if(check(type))
				return NULL;
			// If factory is not found, just return.
			dstream<<"WARNING: ActiveObjectRegistry: No factory for type="
				   <<(int)type<<std::endl;
			return NULL;
		}

		Factory f = n->second;
		T *object = (*f)(params);
		return object;
	}
	// register all types inside this function, NOT globally
	// then call ...Registry.setup() in main.
	void setup();

private:
	// add class to the registry. This is private because it should be
	// called ONLY in setup and NEVER in a global context, despite
	// the registry being a global object. Remember, global objects could
	// cause segfaults, if you use them before main() starts.
	template <typename Impl>
	void add() {
		typename std::map<ActiveObjectType, Factory>::iterator n;
		ActiveObjectType type = Impl::getTypeStatic();
		n = m_types.find(type);
		if(n != m_types.end())
			return;
		// Impl::create should have signature Factory
		m_types[type] = Impl::create;
	}

	// Used for creating objects based on type
	std::map<ActiveObjectType, Factory> m_types;
};



#endif
