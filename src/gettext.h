/*
gettext.h
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

#ifndef GETTEXT_HEADER
#define GETTEXT_HEADER

#include "config.h" // for USE_GETTEXT

#if USE_GETTEXT
#include <libintl.h>
#define mygettext(String) gettext(String)
#else
#define mygettext(String) String
#endif

#define _(String) mygettext(String)
#define gettext_noop(String) String
#define N_(String) gettext_noop (String)

#ifdef _MSC_VER
void init_gettext(const char *path, const std::string &configured_language, int argc, char** argv);
#else
void init_gettext(const char *path, const std::string &configured_language);
#endif

extern const wchar_t *narrow_to_wide_c(const char *mbs);
extern std::wstring narrow_to_wide(const std::string &mbs);


// You must free the returned string!
inline const wchar_t *wgettext(const char *str)
{
	return narrow_to_wide_c(mygettext(str));
}

inline std::wstring wstrgettext(const std::string &text)
{
	return narrow_to_wide(mygettext(text.c_str()));
}

inline std::string strgettext(const std::string &text)
{
	return mygettext(text.c_str());
}

#endif
