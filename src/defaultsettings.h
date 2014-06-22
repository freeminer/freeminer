/*
defaultsettings.h
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

#ifndef DEFAULTSETTINGS_HEADER
#define DEFAULTSETTINGS_HEADER

class Settings;

/**
 * initialize basic default settings
 * @param settings pointer to settings
 */
void set_default_settings(Settings *settings);

/**
 * initialize default values which require knowledge about gui
 * @param settings pointer to settings
 */
void late_init_default_settings(Settings* settings);

/**
 * override a default settings by settings from another settings element
 * @param settings target settings pointer
 * @param from source settings pointer
 */
void override_default_settings(Settings *settings, Settings *from);

#endif

