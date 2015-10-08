/*
util/string.h
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

#ifndef UTIL_STRING_HEADER
#define UTIL_STRING_HEADER

#include "irrlichttypes_bloated.h"
#include <stdlib.h>
#include <string>
#include <cstring>
#include <vector>
#include <map>
#include <sstream>
#include "SColor.h"
#include <cctype>

#define STRINGIFY(x) #x
#define TOSTRING(x) STRINGIFY(x)

// Checks whether a byte is an inner byte for an utf-8 multibyte sequence
#define IS_UTF8_MULTB_INNER(x) (((unsigned char)x >= 0x80) && ((unsigned char)x < 0xc0))

typedef std::map<std::string, std::string> StringMap;

struct FlagDesc {
	const char *name;
	u32 flag;
};

// try not to convert between wide/utf8 encodings; this can result in data loss
// try to only convert between them when you need to input/output stuff via Irrlicht
std::wstring utf8_to_wide(const std::string &input);
std::string wide_to_utf8(const std::wstring &input);

wchar_t *utf8_to_wide_c(const char *str);

// NEVER use those two functions unless you have a VERY GOOD reason to
// they just convert between wide and multibyte encoding
// multibyte encoding depends on current locale, this is no good, especially on Windows

// You must free the returned string!
// The returned string is allocated using new
wchar_t *narrow_to_wide_c(const char *str);

// NEVER use those two functions unless you have a VERY GOOD reason to
// they just convert between wide and multibyte encoding
// multibyte encoding depends on current locale, this is no good, especially on Windows
std::wstring narrow_to_wide_real(const std::string& mbs);
std::string wide_to_narrow_real(const std::wstring& wcs);

// try not to convert between wide/utf8 encodings; this can result in data loss
// try to only convert between them when you need to input/output stuff via Irrlicht
// TODO: must be named utf8_to_wide and wide_to_utf8
std::wstring narrow_to_wide(const std::string &mbs);
std::string wide_to_narrow(const std::wstring &wcs);

std::string urlencode(std::string str);
std::string urldecode(std::string str);
u32 readFlagString(std::string str, const FlagDesc *flagdesc, u32 *flagmask);
std::string writeFlagString(u32 flags, const FlagDesc *flagdesc, u32 flagmask);
size_t mystrlcpy(char *dst, const char *src, size_t size);
char *mystrtok_r(char *s, const char *sep, char **lasts);
u64 read_seed(const char *str);
bool parseColorString(const std::string &value, video::SColor &color, bool quiet = true);


/**
 * Returns a copy of \p str with spaces inserted at the right hand side to ensure
 * that the string is \p len characters in length. If \p str is <= \p len then the
 * returned string will be identical to str.
 */
inline std::string padStringRight(std::string str, size_t len)
{
	if (len > str.size())
		str.insert(str.end(), len - str.size(), ' ');

	return str;
}

/**
 * Returns a version of \p str with the first occurrence of a string
 * contained within ends[] removed from the end of the string.
 *
 * @param str
 * @param ends A NULL- or ""- terminated array of strings to remove from s in
 *	the copy produced.  Note that once one of these strings is removed
 *	that no further postfixes contained within this array are removed.
 *
 * @return If no end could be removed then "" is returned.
 */
inline std::string removeStringEnd(const std::string &str,
		const char *ends[])
{
	const char **p = ends;

	for (; *p && (*p)[0] != '\0'; p++) {
		std::string end = *p;
		if (str.size() < end.size())
			continue;
		if (str.compare(str.size() - end.size(), end.size(), end) == 0)
			return str.substr(0, str.size() - end.size());
	}

	return "";
}


/**
 * Check two strings for equivalence.  If \p case_insensitive is true
 * then the case of the strings is ignored (default is false).
 *
 * @param s1
 * @param s2
 * @param case_insensitive
 * @return true if the strings match
 */
template <typename T>
inline bool str_equal(const std::basic_string<T> &s1,
		const std::basic_string<T> &s2,
		bool case_insensitive = false)
{
	if (!case_insensitive)
		return s1 == s2;

	if (s1.size() != s2.size())
		return false;

	for (size_t i = 0; i < s1.size(); ++i)
		if(tolower(s1[i]) != tolower(s2[i]))
			return false;

	return true;
}


/**
 * Check whether \p str begins with the string prefix. If \p case_insensitive
 * is true then the check is case insensitve (default is false; i.e. case is
 * significant).
 *
 * @param str
 * @param prefix
 * @param case_insensitive
 * @return true if the str begins with prefix
 */
template <typename T>
inline bool str_starts_with(const std::basic_string<T> &str,
		const std::basic_string<T> &prefix,
		bool case_insensitive = false)
{
	if (str.size() < prefix.size())
		return false;

	if (!case_insensitive)
		return str.compare(0, prefix.size(), prefix) == 0;

	for (size_t i = 0; i < prefix.size(); ++i)
		if (tolower(str[i]) != tolower(prefix[i]))
			return false;
	return true;
}

/**
 * Check whether \p str begins with the string prefix. If \p case_insensitive
 * is true then the check is case insensitve (default is false; i.e. case is
 * significant).
 *
 * @param str
 * @param prefix
 * @param case_insensitive
 * @return true if the str begins with prefix
 */
template <typename T>
inline bool str_starts_with(const std::basic_string<T> &str,
		const T *prefix,
		bool case_insensitive = false)
{
	return str_starts_with(str, std::basic_string<T>(prefix),
			case_insensitive);
}

/**
 * Splits a string into its component parts separated by the character
 * \p delimiter.
 *
 * @return An std::vector<std::basic_string<T> > of the component parts
 */
template <typename T>
inline std::vector<std::basic_string<T> > str_split(
		const std::basic_string<T> &str,
		T delimiter)
{
	std::vector<std::basic_string<T> > parts;
	std::basic_stringstream<T> sstr(str);
	std::basic_string<T> part;

	while (std::getline(sstr, part, delimiter))
		parts.push_back(part);

	return parts;
}


/**
 * @param str
 * @return A copy of \p str converted to all lowercase characters.
 */
inline std::string lowercase(const std::string &str)
{
	std::string s2;

	s2.reserve(str.size());

	for (size_t i = 0; i < str.size(); i++)
		s2 += tolower(str[i]);

	return s2;
}


/**
 * @param str
 * @return A copy of \p str with leading and trailing whitespace removed.
 */
inline std::string trim(const std::string &str)
{
	size_t front = 0;

	while (std::isspace(str[front]))
		++front;

	size_t back = str.size();
	while (back > front && std::isspace(str[back - 1]))
		--back;

	return str.substr(front, back - front);
}


/**
 * Returns whether \p str should be regarded as (bool) true.  Case and leading
 * and trailing whitespace are ignored.  Values that will return
 * true are "y", "yes", "true" and any number that is not 0.
 * @param str
 */
inline bool is_yes(const std::string &str)
{
	std::string s2 = lowercase(trim(str));

	return s2 == "y" || s2 == "yes" || s2 == "true" || atoi(s2.c_str()) != 0;
}


/**
 * Converts the string \p str to a signed 32-bit integer. The converted value
 * is constrained so that min <= value <= max.
 *
 * @see atoi(3) for limitations
 *
 * @param str
 * @param min Range minimum
 * @param max Range maximum
 * @return The value converted to a signed 32-bit integer and constrained
 *	within the range defined by min and max (inclusive)
 */
inline s32 mystoi(const std::string &str, s32 min, s32 max)
{
	s32 i = atoi(str.c_str());

	if (i < min)
		i = min;
	if (i > max)
		i = max;

	return i;
}


/// Returns a 64-bit value represented by the string \p str (decimal).
inline s64 stoi64(const std::string &str)
{
	std::stringstream tmp(str);
	s64 t;
	tmp >> t;
	return t;
}

// MSVC2010 includes it's own versions of these
//#if !defined(_MSC_VER) || _MSC_VER < 1600


/**
 * Returns a 32-bit value reprensented by the string \p str (decimal).
 * @see atoi(3) for further limitations
 */
inline s32 mystoi(const std::string &str)
{
	return atoi(str.c_str());
}


/**
 * Returns s 32-bit value represented by the wide string \p str (decimal).
 * @see atoi(3) for further limitations
 */
inline s32 mystoi(const std::wstring &str)
{
	return atoi(wide_to_narrow(str).c_str());
}


/**
 * Returns a float reprensented by the string \p str (decimal).
 * @see atof(3)
 */
inline float mystof(const std::string &str)
{
	return atof(str.c_str());
}

//#endif

#define stoi mystoi
#define stof mystof

// TODO: Replace with C++11 std::to_string.

/// Returns A string representing the value \p val.
template <typename T>
inline std::string to_string(T val)
{
	std::ostringstream oss;
	oss << val;
	return oss.str();
}

/// Returns a string representing the decimal value of the 32-bit value \p i.
inline std::string itos(s32 i) { return to_string(i); }
/// Returns a string representing the decimal value of the 64-bit value \p i.
inline std::string i64tos(s64 i) { return to_string(i); }
/// Returns a string representing the decimal value of the float value \p f.
inline std::string ftos(float f) { return to_string(f); }


/**
 * Replace all occurrences of \p pattern in \p str with \p replacement.
 *
 * @param str String to replace pattern with replacement within.
 * @param pattern The pattern to replace.
 * @param replacement What to replace the pattern with.
 */
inline void str_replace(std::string &str, const std::string &pattern,
		const std::string &replacement)
{
	std::string::size_type start = str.find(pattern, 0);
	while (start != str.npos) {
		str.replace(start, pattern.size(), replacement);
		start = str.find(pattern, start + replacement.size());
	}
}


/**
 * Replace all occurrences of the character \p from in \p str with \p to.
 *
 * @param str The string to (potentially) modify.
 * @param from The character in str to replace.
 * @param to The replacement character.
 */
void str_replace(std::string &str, char from, char to);


/**
 * Check that a string only contains whitelisted characters. This is the
 * opposite of string_allowed_blacklist().
 *
 * @param str The string to be checked.
 * @param allowed_chars A string containing permitted characters.
 * @return true if the string is allowed, otherwise false.
 *
 * @see string_allowed_blacklist()
 */
inline bool string_allowed(const std::string &str, const std::string &allowed_chars)
{
	return str.find_first_not_of(allowed_chars) == str.npos;
}


/**
 * Check that a string contains no blacklisted characters. This is the
 * opposite of string_allowed().
 *
 * @param str The string to be checked.
 * @param blacklisted_chars A string containing prohibited characters.
 * @return true if the string is allowed, otherwise false.

 * @see string_allowed()
 */
inline bool string_allowed_blacklist(const std::string &str,
		const std::string &blacklisted_chars)
{
	return str.find_first_of(blacklisted_chars) == str.npos;
}


/**
 * Create a string based on \p from where a newline is forcefully inserted
 * every \p row_len characters.
 *
 * @note This function does not honour word wraps and blindy inserts a newline
 *	every \p row_len characters whether it breaks a word or not.  It is
 *	intended to be used for, for example, showing paths in the GUI.
 *
 * @note This function doesn't wrap inside utf-8 multibyte sequences and also
 * 	counts multibyte sequences correcly as single characters.
 *
 * @param from The (utf-8) string to be wrapped into rows.
 * @param row_len The row length (in characters).
 * @return A new string with the wrapping applied.
 */
inline std::string wrap_rows(const std::string &from,
		unsigned row_len)
{
	std::string to;

	size_t character_idx = 0;
	for (size_t i = 0; i < from.size(); i++) {
		if (!IS_UTF8_MULTB_INNER(from[i])) {
			// Wrap string after last inner byte of char
			if (character_idx > 0 && character_idx % row_len == 0)
				to += '\n';
			character_idx++;
		}
		to += from[i];
	}

	return to;
}


/**
 * Removes backslashes from an escaped string (FormSpec strings)
 */
template <typename T>
inline std::basic_string<T> unescape_string(std::basic_string<T> &s)
{
	std::basic_string<T> res;

	for (size_t i = 0; i < s.length(); i++) {
		if (s[i] == '\\') {
			i++;
			if (i >= s.length())
				break;
		}
		res += s[i];
	}

	return res;
}


/**
 * Checks that all characters in \p to_check are a decimal digits.
 *
 * @param to_check
 * @return true if to_check is not empty and all characters in to_check are
 *	decimal digits, otherwise false
 */
inline bool is_number(const std::string &to_check)
{
	for (size_t i = 0; i < to_check.size(); i++)
		if (!std::isdigit(to_check[i]))
			return false;

	return !to_check.empty();
}

/**
 * Returns a C-string, either "true" or "false", corresponding to \p val.
 *
 * @return If \p val is true, then "true" is returned, otherwise "false".
 */
inline const char *bool_to_cstr(bool val)
{
	return val ? "true" : "false";
}

std::wstring colorizeText(const std::wstring &s, std::vector<video::SColor> &colors, const video::SColor &initial_color);
std::wstring sanitizeChatString(const std::wstring &s);
bool char_icompare(char c1, char c2);
bool string_icompare(const std::string& a, const std::string& b);
#endif
