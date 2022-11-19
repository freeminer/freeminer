/*
guiFileSelectMenu.h
Copyright (C) 2013 sapier
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

#include "modalMenu.h"
#include "IGUIFileOpenDialog.h"
#include "guiFormSpecMenu.h" //required because of TextDest only !!!

class GUIFileSelectMenu : public GUIModalMenu
{
public:
	GUIFileSelectMenu(gui::IGUIEnvironment *env, gui::IGUIElement *parent, s32 id,
			IMenuManager *menumgr, const std::string &title,
			const std::string &formid, bool is_file_select);
	~GUIFileSelectMenu();

	/*
	 Remove and re-add (or reposition) stuff
	 */
	void regenerateGui(v2u32 screensize);

	void drawMenu();

	bool OnEvent(const SEvent &event);

	void setTextDest(TextDest *dest) { m_text_dst = dest; }

protected:
	std::wstring getLabelByID(s32 id) { return L""; }
	std::string getNameByID(s32 id) { return ""; }

private:
	void acceptInput();

	std::wstring m_title;
	bool m_accepted = false;

	gui::IGUIFileOpenDialog *m_fileOpenDialog = nullptr;

	TextDest *m_text_dst = nullptr;

	std::string m_formname;
	bool m_file_select_dialog;
};
