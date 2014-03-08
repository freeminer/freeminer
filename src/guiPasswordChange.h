/*
guiPasswordChange.cpp
Copyright (C) 2010-2013 celeron55, Perttu Ahola <celeron55@gmail.com>
Copyright (C) 2013 Ciaran Gultnieks <ciaran@ciarang.com>
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

#ifndef GUIPASSWORDCHANGE_HEADER
#define GUIPASSWORDCHANGE_HEADER

#include "irrlichttypes_extrabloated.h"
#include "modalMenu.h"
#include "client.h"
#include <string>

class GUIPasswordChange : public GUIModalMenu
{
public:
	GUIPasswordChange(gui::IGUIEnvironment* env,
			gui::IGUIElement* parent, s32 id,
			IMenuManager *menumgr,
			Client* client);
	~GUIPasswordChange();
	
	void removeChildren();
	/*
		Remove and re-add (or reposition) stuff
	*/
	void regenerateGui(v2u32 screensize);

	void drawMenu();

	bool acceptInput();

	bool OnEvent(const SEvent& event);
	
private:
	Client* m_client;

};

#endif

