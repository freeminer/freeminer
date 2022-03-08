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

enum TimePrecision
{
	PRECISION_SECONDS,
	PRECISION_MILLI,
	PRECISION_MICRO,
	PRECISION_NANO
};

<<<<<<< HEAD
extern u32 getTimeMs();
extern u32 getTime(TimePrecision prec);

/*
	Timestamp stuff
*/

#include <time.h>
#include <string>

extern tm * localtime_safe(time_t * t);

=======
>>>>>>> 5.5.0
inline std::string getTimestamp()
{
	time_t t = time(NULL);
	// This is not really thread-safe but it won't break anything
	// except its own output, so just go with it.
<<<<<<< HEAD
	struct tm *tm = localtime_safe(&t);
	char cs[20]; //YYYY-MM-DD HH:MM:SS + '\0'
=======
	struct tm *tm = localtime(&t);
	char cs[20]; // YYYY-MM-DD HH:MM:SS + '\0'
>>>>>>> 5.5.0
	strftime(cs, 20, "%Y-%m-%d %H:%M:%S", tm);
	return cs;
}
