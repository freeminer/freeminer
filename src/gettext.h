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
#include "log.h"

#if USE_GETTEXT
#include <libintl.h>
#define mygettext(String) gettext(String)
#else
#define mygettext(String) String
#endif

#define _(String) mygettext(String)
#define gettext_noop(String) String
#define N_(String) gettext_noop (String)

#if defined(_WIN32)
#define WIN32_LEAN_AND_MEAN
#ifndef _WIN32_WINNT
	#define _WIN32_WINNT 0x0501
#endif
#define NOMINMAX
#include <windows.h>

#endif // #if defined(_WIN32)

#ifdef _MSC_VER
void init_gettext(const char *path,std::string configured_language,int argc, char** argv);
#else
void init_gettext(const char *path,std::string configured_language);
#endif

extern std::wstring narrow_to_wide(const std::string& mbs);
#include "util/numeric.h"


/******************************************************************************/
inline wchar_t* chartowchar_t(const char *str)
{
	wchar_t* nstr = 0;
#if defined(_WIN32)
	int nResult = MultiByteToWideChar( CP_UTF8, 0, (LPCSTR) str, -1, 0, 0 );
	if( nResult == 0 )
	{
		errorstream<<"gettext: MultiByteToWideChar returned null"<<std::endl;
	}
	else
	{
		nstr = new wchar_t[nResult];
		MultiByteToWideChar( CP_UTF8, 0, (LPCSTR) str, -1, (WCHAR *) nstr, nResult );
	}
#else
	size_t l = strlen(str);
	nstr = new wchar_t[l+1];

	std::wstring intermediate = narrow_to_wide(str);
	memset(nstr, 0, (l+1)*sizeof(wchar_t));
	memcpy(nstr, intermediate.c_str(), l*sizeof(wchar_t));
#endif

	return nstr;
}

/******************************************************************************/
inline wchar_t* wgettext(const char *str)
{
	return chartowchar_t(mygettext(str));
}

/******************************************************************************/
inline std::wstring wstrgettext(std::string text) {
	wchar_t* wlabel = wgettext(text.c_str());
	std::wstring out = (std::wstring)wlabel;
	delete[] wlabel;
	return out;
}

#endif
