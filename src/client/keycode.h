/*
keycode.h
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

#pragma once

#include "irrlichttypes.h"
#include "Keycodes.h"
#include <IEventReceiver.h>
#include <string>

/* A key press, consisting of either an Irrlicht keycode
   or an actual char */

class KeyPress
{
public:
	KeyPress() = default;

	KeyPress(const char *name);

	KeyPress(const irr::SEvent::SKeyInput &in, bool prefer_character = false);

	bool operator==(const KeyPress &o) const
	{
		return (Char > 0 && Char == o.Char) || (valid_kcode(Key) && Key == o.Key);
	}

	const char *sym() const;
	const char *name() const;

protected:
	static bool valid_kcode(irr::EKEY_CODE k)
	{
		return k > 0 && k < irr::KEY_KEY_CODES_COUNT;
	}

	irr::EKEY_CODE Key = irr::KEY_KEY_CODES_COUNT;
	wchar_t Char = L'\0';
	std::string m_name = "";
};

extern const KeyPress LControlKey;
extern const KeyPress RControlKey;
extern const KeyPress LShiftKey;
extern const KeyPress RShiftKey;

extern const KeyPress EscapeKey;
extern const KeyPress CancelKey;

// Key configuration getter
KeyPress getKeySetting(const char *settingname);

// Clear fast lookup cache
void clearKeyCache();

irr::EKEY_CODE keyname_to_keycode(const char *name);
