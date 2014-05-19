/*
FMColoredString.cpp
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

#include "FMColoredString.h"
#include "util/string.h"

FMColoredString::FMColoredString()
{}

FMColoredString::FMColoredString(const std::wstring &string, const std::vector<SColor> &colors):
	m_string(string),
	m_colors(colors)
{}

FMColoredString::FMColoredString(const std::wstring &s) {
	m_string = colorizeText(s, m_colors, SColor(255, 255, 255, 255));
}

void FMColoredString::operator=(const wchar_t *str) {
	m_string = colorizeText(str, m_colors, SColor(255, 255, 255, 255));
}

size_t FMColoredString::size() const {
	return m_string.size();
}

FMColoredString FMColoredString::substr(size_t pos, size_t len) const {
	if (pos == m_string.length())
		return FMColoredString();
	if (len == std::string::npos || pos + len > m_string.length()) {
		return FMColoredString(
		           m_string.substr(pos, std::string::npos),
		           std::vector<SColor>(m_colors.begin() + pos, m_colors.end())
		       );
	} else {
		return FMColoredString(
		           m_string.substr(pos, len),
		           std::vector<SColor>(m_colors.begin() + pos, m_colors.begin() + pos + len)
		       );
	}
}

const wchar_t *FMColoredString::c_str() const {
	return m_string.c_str();
}

const std::vector<SColor> &FMColoredString::getColors() const {
	return m_colors;
}

const std::wstring &FMColoredString::getString() const {
	return m_string;
}

/*std::wstring FMColoredString::serialize() const {
	std::wstring out;
	for (size_t i = 0; i < m_string.size(); ++i) {
		if (i == 0 || m_colors[i] != m_colors[i - 1])
			out += L"\vAABBCC";
		out += m_string[i];
	}
	return out;
}*/
