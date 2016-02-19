/*
guiPasswordChange.cpp
Copyright (C) 2013 celeron55, Perttu Ahola <celeron55@gmail.com>
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

#include "guiPasswordChange.h"
#include "debug.h"
#include "serialization.h"
#include <string>
#include <IGUICheckBox.h>
#include <IGUIEditBox.h>
#include <IGUIButton.h>
#include <IGUIStaticText.h>
#include <IGUIFont.h>

#include "gettext.h"
#include "util/string.h"

const int ID_oldPassword = 256;
const int ID_newPassword1 = 257;
const int ID_newPassword2 = 258;
const int ID_change = 259;
const int ID_message = 260;

GUIPasswordChange::GUIPasswordChange(gui::IGUIEnvironment* env,
		gui::IGUIElement* parent, s32 id,
		IMenuManager *menumgr,
		Client* client
):
	GUIModalMenu(env, parent, id, menumgr),
	m_client(client)
{
}

GUIPasswordChange::~GUIPasswordChange()
{
	removeChildren();
}

void GUIPasswordChange::removeChildren()
{
	{
		gui::IGUIElement *e = getElementFromId(ID_oldPassword);
		if(e != NULL)
			e->remove();
	}
	{
		gui::IGUIElement *e = getElementFromId(ID_newPassword1);
		if(e != NULL)
			e->remove();
	}
	{
		gui::IGUIElement *e = getElementFromId(ID_newPassword2);
		if(e != NULL)
			e->remove();
	}
	{
		gui::IGUIElement *e = getElementFromId(ID_change);
		if(e != NULL)
			e->remove();
	}
}

void GUIPasswordChange::regenerateGui(v2u32 screensize)
{
	/*
		Remove stuff
	*/
	removeChildren();

	const static double gui_scaling = g_settings->getFloat("hud_scaling"); // gui_scaling here or get from pixel ratio
	/*
		Calculate new sizes and positions
	*/
	core::rect<s32> rect(
			screensize.X/2 - 580/2/gui_scaling,
			screensize.Y/2 - 300/2/gui_scaling,
			screensize.X/2 + 580/2/gui_scaling,
			screensize.Y/2 + 300/2/gui_scaling
	);

	DesiredRect = rect;
	recalculateAbsolutePosition(false);

	v2s32 size = rect.getSize();
	v2s32 topleft_client(40/gui_scaling, 0);

	const wchar_t *text;

	/*
		Add stuff
	*/
	s32 ypos = 50/gui_scaling;
	{
		core::rect<s32> rect(0, 0, 150/gui_scaling, 20/gui_scaling);
		rect += topleft_client + v2s32(25, ypos+6);
		text = wgettext("Old Password");
		Environment->addStaticText(text, rect, false, true, this, -1);
		delete[] text;
	}
	{
		core::rect<s32> rect(0, 0, 230/gui_scaling, 30/gui_scaling);
		rect += topleft_client + v2s32(160, ypos);
		gui::IGUIEditBox *e =
		Environment->addEditBox(L"", rect, true, this, ID_oldPassword);
		Environment->setFocus(e);
		e->setPasswordBox(true);
	}
	ypos += 50/gui_scaling;
	{
		core::rect<s32> rect(0, 0, 150/gui_scaling, 20/gui_scaling);
		rect += topleft_client + v2s32(25, ypos+6);
		text = wgettext("New Password");
		Environment->addStaticText(text, rect, false, true, this, -1);
		delete[] text;
	}
	{
		core::rect<s32> rect(0, 0, 230/gui_scaling, 30/gui_scaling);
		rect += topleft_client + v2s32(160, ypos);
		gui::IGUIEditBox *e =
		Environment->addEditBox(L"", rect, true, this, ID_newPassword1);
		e->setPasswordBox(true);
	}
	ypos += 50/gui_scaling;
	{
		core::rect<s32> rect(0, 0, 150/gui_scaling, 20/gui_scaling);
		rect += topleft_client + v2s32(25, ypos+6);
		text = wgettext("Confirm Password");
		Environment->addStaticText(text, rect, false, true, this, -1);
		delete[] text;
	}
	{
		core::rect<s32> rect(0, 0, 230/gui_scaling, 30/gui_scaling);
		rect += topleft_client + v2s32(160, ypos);
		gui::IGUIEditBox *e =
		Environment->addEditBox(L"", rect, true, this, ID_newPassword2);
		e->setPasswordBox(true);
	}

	ypos += 50/gui_scaling;
	{
		core::rect<s32> rect(0, 0, 140/gui_scaling, 30/gui_scaling);
		rect = rect + v2s32(size.X/2-140/2, ypos);
		text = wgettext("Change");
		Environment->addButton(rect, this, ID_change, text);
		delete[] text;
	}

	ypos += 50/gui_scaling;
	{
		core::rect<s32> rect(0, 0, 300/gui_scaling, 20/gui_scaling);
		rect += topleft_client + v2s32(35, ypos);
		text = wgettext("Passwords do not match!");
		IGUIElement *e =
		Environment->addStaticText(
			text,
			rect, false, true, this, ID_message);
		e->setVisible(false);
		delete[] text;
	}
}

void GUIPasswordChange::drawMenu()
{
	gui::IGUISkin* skin = Environment->getSkin();
	if (!skin)
		return;
	video::IVideoDriver* driver = Environment->getVideoDriver();

	video::SColor bgcolor(140,0,0,0);
	driver->draw2DRectangle(bgcolor, AbsoluteRect, &AbsoluteClippingRect);

	gui::IGUIElement::draw();
}

bool GUIPasswordChange::acceptInput()
{
		std::wstring oldpass;
		std::wstring newpass;
		gui::IGUIElement *e;
		e = getElementFromId(ID_oldPassword);
		if(e != NULL)
			oldpass = e->getText();
		e = getElementFromId(ID_newPassword1);
		if(e != NULL)
			newpass = e->getText();
		e = getElementFromId(ID_newPassword2);
		if(e != NULL && newpass != e->getText())
		{
			e = getElementFromId(ID_message);
			if(e != NULL)
				e->setVisible(true);
			return false;
		}
		m_client->sendChangePassword(wide_to_utf8(oldpass),
			wide_to_utf8(newpass));
		return true;
}

bool GUIPasswordChange::OnEvent(const SEvent& event)
{
	if (GUIModalMenu::OnEvent(event))
		return true;

	if(event.EventType==EET_KEY_INPUT_EVENT)
	{
		if(event.KeyInput.Key==KEY_ESCAPE && event.KeyInput.PressedDown)
		{
			quitMenu();
			return true;
		}
		if(event.KeyInput.Key==KEY_RETURN && event.KeyInput.PressedDown)
		{
			if(acceptInput())
				quitMenu();
			return true;
		}
	}
	if(event.EventType==EET_GUI_EVENT)
	{
		if(event.GUIEvent.EventType==gui::EGET_ELEMENT_FOCUS_LOST
				&& isVisible())
		{
			if(!canTakeFocus(event.GUIEvent.Element))
			{
				dstream<<"GUIPasswordChange: Not allowing focus change."
						<<std::endl;
				// Returning true disables focus change
				return true;
			}
		}
		if(event.GUIEvent.EventType==gui::EGET_BUTTON_CLICKED)
		{
			switch(event.GUIEvent.Caller->getID())
			{
			case ID_change:
				if(acceptInput())
					quitMenu();
				return true;
			}
		}
		if(event.GUIEvent.EventType==gui::EGET_EDITBOX_ENTER)
		{
			switch(event.GUIEvent.Caller->getID())
			{
			case ID_oldPassword:
			case ID_newPassword1:
			case ID_newPassword2:
				if(acceptInput())
					quitMenu();
				return true;
			}
		}
	}

	return Parent ? Parent->OnEvent(event) : false;
}

