/*
gettime.h
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

#pragma once

#include <ctime>
#include <string>
#include <mutex>

enum TimePrecision
{
	PRECISION_SECONDS,
	PRECISION_MILLI,
	PRECISION_MICRO,
	PRECISION_NANO
};

inline struct tm mt_localtime()
{
	// initialize the time zone on first invocation
	static std::once_flag tz_init;
	std::call_once(tz_init, [] {
#ifdef _WIN32
		_tzset();
#else
		tzset();
#endif
		});

	struct tm ret;
	time_t t = time(NULL);
	// TODO we should check if the function returns NULL, which would mean error
#ifdef _WIN32
	localtime_s(&ret, &t);
#else
	localtime_r(&t, &ret);
#endif
	return ret;
}


inline std::string getTimestampMt()
{
	const struct tm tm = mt_localtime();
	char cs[20]; // YYYY-MM-DD HH:MM:SS + '\0'
	strftime(cs, 20, "%Y-%m-%d %H:%M:%S", &tm);
	return cs;
}

extern tm * localtime_safe(time_t * t);
inline std::string getTimestamp()
{
	time_t t = time(NULL);
	struct tm *tm = localtime_safe(&t);
	char cs[20]; //YYYY-MM-DD HH:MM:SS + '\0'
	strftime(cs, 20, "%Y-%m-%d %H:%M:%S", tm);
	return cs;
}

