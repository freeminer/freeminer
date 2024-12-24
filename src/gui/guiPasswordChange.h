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

#pragma once

#include "irrlichttypes_extrabloated.h"
#include "modalMenu.h"
#include <string>

class Client;
class ISimpleTextureSource;

class GUIPasswordChange : public GUIModalMenu
{
public:
	GUIPasswordChange(gui::IGUIEnvironment *env, gui::IGUIElement *parent, s32 id,
			IMenuManager *menumgr, Client *client,
			ISimpleTextureSource *tsrc);

	/*
		Remove and re-add (or reposition) stuff
	*/
	void regenerateGui(v2u32 screensize);

	void drawMenu();

	void acceptInput();

	bool processInput();

	bool OnEvent(const SEvent &event);
#ifdef __ANDROID__
	void getAndroidUIInput();
#endif

protected:
	std::wstring getLabelByID(s32 id) { return L""; }
	std::string getNameByID(s32 id);

private:
	Client *m_client;
	std::wstring m_oldpass = L"";
	std::wstring m_newpass = L"";
	std::wstring m_newpass_confirm = L"";
	ISimpleTextureSource *m_tsrc;
};
