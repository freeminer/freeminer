/*
guiTextInputMenu.cpp
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

#include "guiTextInputMenu.h"
#include "debug.h"
#include "serialization.h"
#include "main.h" // for g_settings
#include "settings.h"
#include <string>
#include <IGUICheckBox.h>
#include <IGUIEditBox.h>
#include <IGUIButton.h>
#include <IGUIStaticText.h>
#include <IGUIFont.h>

#include "gettext.h"
#include "intlGUIEditBox.h"

GUITextInputMenu::GUITextInputMenu(gui::IGUIEnvironment* env,
		gui::IGUIElement* parent, s32 id,
		IMenuManager *menumgr,
		TextDest *dest,
		std::wstring initial_text
):
	GUIModalMenu(env, parent, id, menumgr),
	m_dest(dest),
	m_initial_text(initial_text)
{
}

GUITextInputMenu::~GUITextInputMenu()
{
	removeChildren();
	if(m_dest)
		delete m_dest;
}

void GUITextInputMenu::removeChildren()
{
	{
		gui::IGUIElement *e = getElementFromId(256);
		if(e != NULL)
			e->remove();
	}
	{
		gui::IGUIElement *e = getElementFromId(257);
		if(e != NULL)
			e->remove();
	}
}

void GUITextInputMenu::regenerateGui(v2u32 screensize)
{
	std::wstring text;

	{
		gui::IGUIElement *e = getElementFromId(256);
		if(e != NULL)
		{
			text = e->getText();
		}
		else
		{
			text = m_initial_text;
			m_initial_text = L"";
		}
	}

	/*
		Remove stuff
	*/
	removeChildren();
	
	/*
		Calculate new sizes and positions
	*/
	core::rect<s32> rect(
			screensize.X/2 - 580/2,
			screensize.Y/2 - 300/2,
			screensize.X/2 + 580/2,
			screensize.Y/2 + 300/2
	);
	
	DesiredRect = rect;
	recalculateAbsolutePosition(false);

	v2s32 size = rect.getSize();

	/*
		Add stuff
	*/
	{
		core::rect<s32> rect(0, 0, 300, 30);
		rect = rect + v2s32(size.X/2-300/2, size.Y/2-30/2-25);
		gui::IGUIElement *e;
		e = (gui::IGUIElement *) new gui::intlGUIEditBox(text.c_str(), true, Environment, this, 256, rect);
		// e->drop(); TODO: figure out what actually happens here.
		Environment->setFocus(e);

		irr::SEvent evt;
		evt.EventType = EET_KEY_INPUT_EVENT;
		evt.KeyInput.Key = KEY_END;
		evt.KeyInput.PressedDown = true;
		evt.KeyInput.Char = 0;
		evt.KeyInput.Control = 0;
		evt.KeyInput.Shift = 0;
		e->OnEvent(evt);
	}
	{
		core::rect<s32> rect(0, 0, 140, 30);
		rect = rect + v2s32(size.X/2-140/2, size.Y/2-30/2+25);
		const wchar_t* text = wgettext("Proceed");
		Environment->addButton(rect, this, 257,
			text);
		delete[] text;
	}
}

void GUITextInputMenu::drawMenu()
{
	gui::IGUISkin* skin = Environment->getSkin();
	if (!skin)
		return;
	video::IVideoDriver* driver = Environment->getVideoDriver();
	
	video::SColor bgcolor(140,0,0,0);
	driver->draw2DRectangle(bgcolor, AbsoluteRect, &AbsoluteClippingRect);

	gui::IGUIElement::draw();
}

void GUITextInputMenu::acceptInput()
{
	if(m_dest)
	{
		gui::IGUIElement *e = getElementFromId(256);
		if(e != NULL)
		{
			m_dest->gotText(e->getText());
		}
		delete m_dest;
		m_dest = NULL;
	}
}

bool GUITextInputMenu::OnEvent(const SEvent& event)
{
	if(event.EventType==EET_KEY_INPUT_EVENT)
	{
		if(event.KeyInput.Key==KEY_ESCAPE && event.KeyInput.PressedDown)
		{
			quitMenu();
			return true;
		}
		if(event.KeyInput.Key==KEY_RETURN && event.KeyInput.PressedDown)
		{
			acceptInput();
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
				dstream<<"GUITextInputMenu: Not allowing focus change."
						<<std::endl;
				// Returning true disables focus change
				return true;
			}
		}
		if(event.GUIEvent.EventType==gui::EGET_BUTTON_CLICKED)
		{
			switch(event.GUIEvent.Caller->getID())
			{
			case 257:
				acceptInput();
				quitMenu();
				// quitMenu deallocates menu
				return true;
			}
		}
		if(event.GUIEvent.EventType==gui::EGET_EDITBOX_ENTER)
		{
			switch(event.GUIEvent.Caller->getID())
			{
			case 256:
				acceptInput();
				quitMenu();
				// quitMenu deallocates menu
				return true;
			}
		}
	}

	return Parent ? Parent->OnEvent(event) : false;
}

