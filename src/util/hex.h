/*
hex.h
Copyright (C) 2013 Jonathan Neuschäfer <j.neuschaefer@gmx.net>
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

#ifndef HEX_HEADER
#define HEX_HEADER

#include <string>

static const char hex_chars[] = "0123456789abcdef";

static inline std::string hex_encode(const char *data, unsigned int data_size)
{
	std::string ret;
	char buf2[3];
	buf2[2] = '\0';

	for(unsigned int i = 0; i < data_size; i++)
	{
		unsigned char c = (unsigned char) data[i];
		buf2[0] = hex_chars[(c & 0xf0) >> 4];
		buf2[1] = hex_chars[c & 0x0f];
		ret.append(buf2);
	}

	return ret;
}

static inline std::string hex_encode(const std::string &data)
{
    return hex_encode(data.c_str(), data.size());
}

static inline bool hex_digit_decode(char hexdigit, unsigned char &value)
{
	if(hexdigit >= '0' && hexdigit <= '9')
		value = hexdigit - '0';
	else if(hexdigit >= 'A' && hexdigit <= 'F')
		value = hexdigit - 'A' + 10;
	else if(hexdigit >= 'a' && hexdigit <= 'f')
		value = hexdigit - 'a' + 10;
	else
		return false;
	return true;
}

#endif
