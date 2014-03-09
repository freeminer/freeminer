/*
clientsimpleobject.h
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

#ifndef CLIENTSIMPLEOBJECT_HEADER
#define CLIENTSIMPLEOBJECT_HEADER

#include "irrlichttypes_bloated.h"
class ClientEnvironment;

class ClientSimpleObject
{
protected:
public:
	bool m_to_be_removed;

	ClientSimpleObject(): m_to_be_removed(false) {}
	virtual ~ClientSimpleObject(){}
	virtual void step(float dtime){}
};

#endif

