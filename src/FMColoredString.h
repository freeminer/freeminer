/*
FMColoredString.h
Copyright (C) 2013 xyz, Ilya Zhuravlev <whatever@xyz.is>
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

#ifndef FMCOLOREDSTRING_HEADER
#define FMCOLOREDSTRING_HEADER

#include <string>
#include <vector>
#include <SColor.h>

using namespace irr::video;

class FMColoredString {
public:
	FMColoredString();
	FMColoredString(const std::wstring &s);
	FMColoredString(const std::wstring &string, const std::vector<SColor> &colors);
	void operator=(const wchar_t *str);
	size_t size() const;
	FMColoredString substr(size_t pos = 0, size_t len = std::string::npos) const;
	const wchar_t *c_str() const;
	const std::vector<SColor> &getColors() const;
	const std::wstring &getString() const;
private:
	std::wstring m_string;
	std::vector<SColor> m_colors;
};

#endif
