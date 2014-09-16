/*
util/timetaker.h
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

#ifndef UTIL_TIMETAKER_HEADER
#define UTIL_TIMETAKER_HEADER

#include "../irrlichttypes.h"
#include "../gettime.h"

/*
	TimeTaker
*/
extern unsigned int time_taker_enabled;

class TimeTaker
{
public:
	TimeTaker(const std::string &name, u32 *result=NULL,
		TimePrecision=PRECISION_MILLI);

	~TimeTaker()
	{
		stop();
	}

	u32 stop(bool quiet=false);

	u32 getTimerTime();

private:
	std::string m_name;
	u32 m_time1;
	bool m_running;
	TimePrecision m_precision;
	u32 *m_result;
};

#endif

