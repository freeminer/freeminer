/*
guiChatConsole.cpp
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

#include "guiChatConsole.h"
#include "chat.h"
#include "client.h"
#include "debug.h"
#include "gettime.h"
#include "keycode.h"
#include "settings.h"
#include "porting.h"
#include "client/tile.h"
#include "fontengine.h"
#include "log_types.h"
#include "gettext.h"
#include <string>

#include "xCGUITTFont.h"

inline u32 clamp_u8(s32 value)
{
	return (u32) MYMIN(MYMAX(value, 0), 255);
}


GUIChatConsole::GUIChatConsole(
		gui::IGUIEnvironment* env,
		gui::IGUIElement* parent,
		s32 id,
		ChatBackend* backend,
		Client* client
):
	IGUIElement(gui::EGUIET_ELEMENT, env, parent, id,
			core::rect<s32>(0,0,100,100)),
	m_chat_backend(backend),
	m_client(client),
	m_screensize(v2u32(0,0)),
	m_animate_time_old(0),
	m_open(false),
	m_close_on_return(false),
	m_height(0),
	m_desired_height(0),
	m_desired_height_fraction(0.0),
	m_height_speed(5.0),
	m_open_inhibited(0),
	m_cursor_blink(0.0),
	m_cursor_blink_speed(0.0),
	m_cursor_height(0.0),
	m_background(NULL),
	m_background_color(255, 0, 0, 0),
	m_font(NULL),
	m_fontsize(0, 0)
{
	m_animate_time_old = getTimeMs();

	// load background settings
	s32 console_alpha = g_settings->getS32("console_alpha");
	m_background_color.setAlpha(clamp_u8(console_alpha));

	// load the background texture depending on settings
	ITextureSource *tsrc = client->getTextureSource();
	if (tsrc->isKnownSourceImage("background_chat.jpg")) {
		m_background = tsrc->getTexture("background_chat.jpg");
		m_background_color.setRed(255);
		m_background_color.setGreen(255);
		m_background_color.setBlue(255);
	} else {
		v3f console_color = g_settings->getV3F("console_color");
		m_background_color.setRed(clamp_u8(myround(console_color.X)));
		m_background_color.setGreen(clamp_u8(myround(console_color.Y)));
		m_background_color.setBlue(clamp_u8(myround(console_color.Z)));
	}

	m_font = g_fontengine->getFont(FONT_SIZE_UNSPECIFIED, FM_Mono);

	if (m_font == NULL)
	{
		errorstream << "GUIChatConsole: Unable to load mono font ";
	}
	else
	{
		core::dimension2d<u32> dim = m_font->getDimension(L"M");
		m_fontsize = v2u32(dim.Width, dim.Height);
		m_font->grab();
	}
	m_fontsize.X = MYMAX(m_fontsize.X, 1);
	m_fontsize.Y = MYMAX(m_fontsize.Y, 1);

	// set default cursor options
	setCursor(true, true, 2.0, 0.1);
}

GUIChatConsole::~GUIChatConsole()
{
	if (m_font)
		m_font->drop();
}

void GUIChatConsole::openConsole(float height, bool close_on_return)
{
	m_open = true;
	m_close_on_return = close_on_return;
	m_desired_height_fraction = height;
	m_desired_height = height * m_screensize.Y;
	reformatConsole();
}

bool GUIChatConsole::isOpen() const
{
	return m_open;
}

bool GUIChatConsole::isOpenInhibited() const
{
	return m_open_inhibited > 0;
}

void GUIChatConsole::closeConsole()
{
	m_open = false;
}

void GUIChatConsole::closeConsoleAtOnce()
{
	m_open = false;
	m_height = 0;
	recalculateConsolePosition();
}

f32 GUIChatConsole::getDesiredHeight() const
{
	return m_desired_height_fraction;
}

void GUIChatConsole::setCursor(
	bool visible, bool blinking, f32 blink_speed, f32 relative_height)
{
	if (visible)
	{
		if (blinking)
		{
			// leave m_cursor_blink unchanged
			m_cursor_blink_speed = blink_speed;
		}
		else
		{
			m_cursor_blink = 0x8000;  // on
			m_cursor_blink_speed = 0.0;
		}
	}
	else
	{
		m_cursor_blink = 0;  // off
		m_cursor_blink_speed = 0.0;
	}
	m_cursor_height = relative_height;
}

void GUIChatConsole::draw()
{
	if(!IsVisible)
		return;

	video::IVideoDriver* driver = Environment->getVideoDriver();

	// Check screen size
	v2u32 screensize = driver->getScreenSize();
	if (screensize != m_screensize)
	{
		// screen size has changed
		// scale current console height to new window size
		if (m_screensize.Y != 0)
			m_height = m_height * screensize.Y / m_screensize.Y;
		m_desired_height = m_desired_height_fraction * m_screensize.Y;
		m_screensize = screensize;
		reformatConsole();
	}

	// Animation
	u32 now = getTimeMs();
	animate(now - m_animate_time_old);
	m_animate_time_old = now;

	// Draw console elements if visible
	if (m_height > 0)
	{
		drawBackground();
		drawText();
		drawPrompt();
	}

	gui::IGUIElement::draw();
}

void GUIChatConsole::reformatConsole()
{
	s32 cols = m_screensize.X / m_fontsize.X - 2; // make room for a margin (looks better)
	s32 rows = m_desired_height / m_fontsize.Y - 1; // make room for the input prompt
	if (cols <= 0 || rows <= 0)
		cols = rows = 0;
	m_chat_backend->reformat(cols, rows);
}

void GUIChatConsole::recalculateConsolePosition()
{
	core::rect<s32> rect(0, 0, m_screensize.X, m_height);
	DesiredRect = rect;
	recalculateAbsolutePosition(false);
}

void GUIChatConsole::animate(u32 msec)
{
	// animate the console height
	s32 goal = m_open ? m_desired_height : 0;
	if (m_height != goal)
	{
		s32 max_change = msec * m_screensize.Y * (m_height_speed / 1000.0);
		if (max_change == 0)
			max_change = 1;

		if (m_height < goal)
		{
			// increase height
			if (m_height + max_change < goal)
				m_height += max_change;
			else
				m_height = goal;
		}
		else
		{
			// decrease height
			if (m_height > goal + max_change)
				m_height -= max_change;
			else
				m_height = goal;
		}

		recalculateConsolePosition();
	}

	// blink the cursor
	if (m_cursor_blink_speed != 0.0)
	{
		u32 blink_increase = 0x10000 * msec * (m_cursor_blink_speed / 1000.0);
		if (blink_increase == 0)
			blink_increase = 1;
		m_cursor_blink = ((m_cursor_blink + blink_increase) & 0xffff);
	}

	// decrease open inhibit counter
	if (m_open_inhibited > msec)
		m_open_inhibited -= msec;
	else
		m_open_inhibited = 0;
}

void GUIChatConsole::drawBackground()
{
	video::IVideoDriver* driver = Environment->getVideoDriver();
	if (m_background != NULL)
	{
		core::rect<s32> sourcerect(0, -m_height, m_screensize.X, 0);
		driver->draw2DImage(
			m_background,
			v2s32(0, 0),
			sourcerect,
			&AbsoluteClippingRect,
			m_background_color,
			false);
	}
	else
	{
		driver->draw2DRectangle(
			m_background_color,
			core::rect<s32>(0, 0, m_screensize.X, m_height),
			&AbsoluteClippingRect);
	}
}

void GUIChatConsole::drawText()
{
	if (m_font == NULL)
		return;

	irr::gui::CGUITTFont *tmp = static_cast<irr::gui::CGUITTFont*>(m_font);

	ChatBuffer& buf = m_chat_backend->getConsoleBuffer();
	for (u32 row = 0; row < buf.getRows(); ++row)
	{
		const ChatFormattedLine& line = buf.getFormattedLine(row);
		if (line.fragments.empty())
			continue;

		s32 line_height = m_fontsize.Y;
		s32 y = row * line_height + m_height - m_desired_height;
		if (y + line_height < 0)
			continue;

		for (u32 i = 0; i < line.fragments.size(); ++i)
		{
			const ChatFormattedFragment& fragment = line.fragments[i];
			s32 x = (fragment.column + 1) * m_fontsize.X;
			core::rect<s32> destrect(
				x, y, x + m_fontsize.X * fragment.text.size(), y + m_fontsize.Y);
			tmp->draw(
				fragment.text.c_str(),
				destrect,
				fragment.text.getColors(),
				false,
				false,
				&AbsoluteClippingRect);
		}
	}
}

void GUIChatConsole::drawPrompt()
{
	if (m_font == NULL)
		return;

	u32 row = m_chat_backend->getConsoleBuffer().getRows();
	s32 line_height = m_fontsize.Y;
	s32 y = row * line_height + m_height - m_desired_height;

	ChatPrompt& prompt = m_chat_backend->getPrompt();
	std::wstring prompt_text = prompt.getVisiblePortion();

	// FIXME Draw string at once, not character by character
	// That will only work with the cursor once we have a monospace font
	for (u32 i = 0; i < prompt_text.size(); ++i)
	{
		wchar_t ws[2] = {prompt_text[i], 0};
		s32 x = (1 + i) * m_fontsize.X;
		core::rect<s32> destrect(
			x, y, x + m_fontsize.X, y + m_fontsize.Y);
		m_font->draw(
			ws,
			destrect,
			video::SColor(255, 255, 255, 255),
			false,
			false,
			&AbsoluteClippingRect);
	}

	// Draw the cursor during on periods
	if ((m_cursor_blink & 0x8000) != 0)
	{
		s32 cursor_pos = prompt.getVisibleCursorPosition();
		if (cursor_pos >= 0)
		{
			video::IVideoDriver* driver = Environment->getVideoDriver();
			s32 x = (1 + cursor_pos) * m_fontsize.X;
			core::rect<s32> destrect(
				x,
				y + (1.0-m_cursor_height) * m_fontsize.Y,
				x + m_fontsize.X,
				y + m_fontsize.Y);
			video::SColor cursor_color(255,255,255,255);
			driver->draw2DRectangle(
				cursor_color,
				destrect,
				&AbsoluteClippingRect);
		}
	}

}

void GUIChatConsole::setPrompt(const std::wstring& input) {
	m_chat_backend->getPrompt().cursorOperation(
			ChatPrompt::CURSOROP_DELETE,
			ChatPrompt::CURSOROP_DIR_LEFT,
			ChatPrompt::CURSOROP_SCOPE_LINE);
	for (unsigned int i = 0; i < input.size(); i++) {
		m_chat_backend->getPrompt().input(input[i]);
	}
}

bool GUIChatConsole::getAndroidUIInput() {
#ifdef __ANDROID__
	if (porting::getInputDialogState() == 0) {
		std::string text = porting::getInputDialogValue();
		std::wstring wtext = narrow_to_wide(text);
		//errorstream<<"GUIChatConsole::getAndroidUIInput() text=["<<text<<"] "<<std::endl;
		m_chat_backend->getPrompt().input(wtext);
		std::wstring wrtext = m_chat_backend->getPrompt().submit();
		m_client->typeChatMessage(wide_to_narrow(wrtext));

		if (m_close_on_return) {
			closeConsole();
			Environment->removeFocus(this);
		}

		return true;
	}
#endif
	return false;
}

bool GUIChatConsole::OnEvent(const SEvent& event)
{
	if(event.EventType == EET_KEY_INPUT_EVENT && event.KeyInput.PressedDown)
	{
		KeyPress kp(event.KeyInput);
		// Key input
		if(KeyPress(event.KeyInput) == getKeySetting("keymap_console"))
		{
			closeConsole();
			Environment->removeFocus(this);

			// inhibit open so the_game doesn't reopen immediately
			m_open_inhibited = 50;
			return true;
		}
		else if (kp == EscapeKey || kp == CancelKey)
		{
			closeConsoleAtOnce();
			Environment->removeFocus(this);
			// the_game will open the pause menu
			return true;
		}
		else if(event.KeyInput.Key == KEY_PRIOR && event.KeyInput.Char == 0)
		{
			m_chat_backend->scrollPageUp();
			return true;
		}
		else if(event.KeyInput.Key == KEY_NEXT && event.KeyInput.Char == 0)
		{
			m_chat_backend->scrollPageDown();
			return true;
		}
		else if(event.KeyInput.Key == KEY_RETURN)
		{
			std::string text = wide_to_narrow(m_chat_backend->getPrompt().submit());
			m_client->typeChatMessage(text);

			if (m_close_on_return) {
				closeConsole();
				Environment->removeFocus(this);
			}
			return true;
		}
		else if(event.KeyInput.Key == KEY_UP && event.KeyInput.Char == 0)
		{
			// Up pressed
			// Move back in history
			m_chat_backend->getPrompt().historyPrev();
			return true;
		}
		else if(event.KeyInput.Key == KEY_DOWN && event.KeyInput.Char == 0)
		{
			// Down pressed
			// Move forward in history
			m_chat_backend->getPrompt().historyNext();
			return true;
		}
		else if(event.KeyInput.Key == KEY_LEFT && event.KeyInput.Char == 0)
		{
			// Left or Ctrl-Left pressed
			// move character / word to the left
			ChatPrompt::CursorOpScope scope =
				event.KeyInput.Control ?
				ChatPrompt::CURSOROP_SCOPE_WORD :
				ChatPrompt::CURSOROP_SCOPE_CHARACTER;
			m_chat_backend->getPrompt().cursorOperation(
				ChatPrompt::CURSOROP_MOVE,
				ChatPrompt::CURSOROP_DIR_LEFT,
				scope);
			return true;
		}
		else if(event.KeyInput.Key == KEY_RIGHT && event.KeyInput.Char == 0)
		{
			// Right or Ctrl-Right pressed
			// move character / word to the right
			ChatPrompt::CursorOpScope scope =
				event.KeyInput.Control ?
				ChatPrompt::CURSOROP_SCOPE_WORD :
				ChatPrompt::CURSOROP_SCOPE_CHARACTER;
			m_chat_backend->getPrompt().cursorOperation(
				ChatPrompt::CURSOROP_MOVE,
				ChatPrompt::CURSOROP_DIR_RIGHT,
				scope);
			return true;
		}
		else if(event.KeyInput.Key == KEY_HOME && event.KeyInput.Char == 0)
		{
			// Home pressed
			// move to beginning of line
			m_chat_backend->getPrompt().cursorOperation(
				ChatPrompt::CURSOROP_MOVE,
				ChatPrompt::CURSOROP_DIR_LEFT,
				ChatPrompt::CURSOROP_SCOPE_LINE);
			return true;
		}
		else if(event.KeyInput.Key == KEY_END && event.KeyInput.Char == 0)
		{
			// End pressed
			// move to end of line
			m_chat_backend->getPrompt().cursorOperation(
				ChatPrompt::CURSOROP_MOVE,
				ChatPrompt::CURSOROP_DIR_RIGHT,
				ChatPrompt::CURSOROP_SCOPE_LINE);
			return true;
		}
		else if(event.KeyInput.Key == KEY_BACK)
		{
			// Backspace or Ctrl-Backspace pressed
			// delete character / word to the left
			ChatPrompt::CursorOpScope scope =
				event.KeyInput.Control ?
				ChatPrompt::CURSOROP_SCOPE_WORD :
				ChatPrompt::CURSOROP_SCOPE_CHARACTER;
			m_chat_backend->getPrompt().cursorOperation(
				ChatPrompt::CURSOROP_DELETE,
				ChatPrompt::CURSOROP_DIR_LEFT,
				scope);
			return true;
		}
		else if(event.KeyInput.Key == KEY_DELETE && (event.KeyInput.Char == 0 || event.KeyInput.Char == 127))
		{
			// Delete or Ctrl-Delete pressed
			// delete character / word to the right
			ChatPrompt::CursorOpScope scope =
				event.KeyInput.Control ?
				ChatPrompt::CURSOROP_SCOPE_WORD :
				ChatPrompt::CURSOROP_SCOPE_CHARACTER;
			m_chat_backend->getPrompt().cursorOperation(
				ChatPrompt::CURSOROP_DELETE,
				ChatPrompt::CURSOROP_DIR_RIGHT,
				scope);
			return true;
		}
		else if(event.KeyInput.Key == KEY_KEY_V && event.KeyInput.Control)
		{
			// Ctrl-V pressed
			// paste text from clipboard
			IOSOperator *os_operator = Environment->getOSOperator();
			const c8 *text = os_operator->getTextFromClipboard();
			if (text)
			{
				std::wstring wtext = narrow_to_wide(text);
				m_chat_backend->getPrompt().input(wtext);
			}
			return true;
		}
		else if(event.KeyInput.Key == KEY_KEY_U && event.KeyInput.Control)
		{
			// Ctrl-U pressed
			// kill line to left end
			m_chat_backend->getPrompt().cursorOperation(
				ChatPrompt::CURSOROP_DELETE,
				ChatPrompt::CURSOROP_DIR_LEFT,
				ChatPrompt::CURSOROP_SCOPE_LINE);
			return true;
		}
		else if(event.KeyInput.Key == KEY_KEY_K && event.KeyInput.Control)
		{
			// Ctrl-K pressed
			// kill line to right end
			m_chat_backend->getPrompt().cursorOperation(
				ChatPrompt::CURSOROP_DELETE,
				ChatPrompt::CURSOROP_DIR_RIGHT,
				ChatPrompt::CURSOROP_SCOPE_LINE);
			return true;
		}
		else if(event.KeyInput.Key == KEY_TAB)
		{
			// Tab or Shift-Tab pressed
			// Nick completion
			std::list<std::string> names = m_client->getConnectedPlayerNames();
			bool backwards = event.KeyInput.Shift;
			m_chat_backend->getPrompt().nickCompletion(names, backwards);
			return true;
		}
		else if(event.KeyInput.Char != 0 && !event.KeyInput.Control)
		{
			#if (defined(linux) || defined(__linux) || defined(__FreeBSD__)) and IRRLICHT_VERSION_10000 < 10900
				wchar_t wc = L'_';
				mbtowc( &wc, (char *) &event.KeyInput.Char, sizeof(event.KeyInput.Char) );
				m_chat_backend->getPrompt().input(wc);
			#else
				m_chat_backend->getPrompt().input(event.KeyInput.Char);
			#endif
			return true;
		}
	}
	else if(event.EventType == EET_MOUSE_INPUT_EVENT)
	{
		if(event.MouseInput.Event == EMIE_MOUSE_WHEEL)
		{
			s32 rows = myround(-3.0 * event.MouseInput.Wheel);
			m_chat_backend->scroll(rows);
		}
	}

	return Parent ? Parent->OnEvent(event) : false;
}

