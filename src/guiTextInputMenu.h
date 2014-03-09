/*
guiTextInputMenu.h
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

#ifndef GUITEXTINPUTMENU_HEADER
#define GUITEXTINPUTMENU_HEADER

#include "irrlichttypes_extrabloated.h"
#include "modalMenu.h"
#include "guiFormSpecMenu.h"
#include <string>

class GUITextInputMenu : public GUIModalMenu
{
public:
	GUITextInputMenu(gui::IGUIEnvironment* env,
			gui::IGUIElement* parent, s32 id,
			IMenuManager *menumgr,
			TextDest *dest,
			std::wstring initial_text);
	~GUITextInputMenu();
	
	void removeChildren();
	/*
		Remove and re-add (or reposition) stuff
	*/
	void regenerateGui(v2u32 screensize);

	void drawMenu();

	void acceptInput();

	bool OnEvent(const SEvent& event);
	
private:
	TextDest *m_dest;
	std::wstring m_initial_text;
};

#endif

