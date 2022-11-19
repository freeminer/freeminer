/*
guiVolumeChange.h
Copyright (C) 2010-2013 celeron55, Perttu Ahola <celeron55@gmail.com>
Copyright (C) 2013 Ciaran Gultnieks <ciaran@ciarang.com>
Copyright (C) 2013 RealBadAngel, Maciej Kasatkin <mk@realbadangel.pl>
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

#include "irrlichttypes_extrabloated.h"
#include "modalMenu.h"
#include <string>

class ISimpleTextureSource;

class GUIVolumeChange : public GUIModalMenu
{
public:
	GUIVolumeChange(gui::IGUIEnvironment* env,
			gui::IGUIElement* parent, s32 id,
			IMenuManager *menumgr, ISimpleTextureSource *tsrc);
	/*
		Remove and re-add (or reposition) stuff
	*/
	void regenerateGui(v2u32 screensize);

	void drawMenu();

	bool OnEvent(const SEvent& event);

	bool pausesGame() { return true; }

protected:
	std::wstring getLabelByID(s32 id) { return L""; }
	std::string getNameByID(s32 id) { return ""; }

private:
	ISimpleTextureSource *m_tsrc;
};
