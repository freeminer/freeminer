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
	// In certain environments, some standard headers like <iomanip>
	// and <locale> include libintl.h. If libintl.h is included after
	// we define our gettext macro below, this causes a syntax error
	// at the declaration of the gettext function in libintl.h.
	// Fix this by including such a header before defining the macro.
	// See issue #4446.
	// Note that we can't include libintl.h directly since we're in
	// the USE_GETTEXT=0 case and can't assume that gettext is installed.
	#include <locale>

	#define gettext(String) String
	#define mygettext(String) String
#endif

#define _(String) mygettext(String)
#define gettext_noop(String) (String)
#define N_(String) gettext_noop((String))

void init_gettext(const char *path, const std::string &configured_language,
	int argc, char *argv[]);

extern wchar_t *utf8_to_wide_c(const char *str);

// You must free the returned string!
// The returned string is allocated using new
inline const wchar_t *wgettext(const char *str)
{
	return utf8_to_wide_c(mygettext(str));
}

inline std::wstring wstrgettext(const std::string &text)
{
	//return narrow_to_wide(mygettext(text.c_str()));
	const wchar_t *tmp = wgettext(text.c_str());
	std::wstring retval = (std::wstring)tmp;
	delete[] tmp;
	return retval;
}

inline std::string strgettext(const std::string &text)
{
	return mygettext(text.c_str());
}

#endif
