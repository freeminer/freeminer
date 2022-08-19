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

#pragma once

#include "irrlichttypes.h"
#include "gettime.h"

/*
	TimeTaker
*/
extern unsigned int g_time_taker_enabled;

class TimeTaker
{
public:
	// in freeminer timetaker by default disabled for release builds.
	// to count time should call this: 
	void start();


	TimeTaker(const std::string &name, u64 *result=nullptr,
		TimePrecision prec=PRECISION_MILLI);

	~TimeTaker()
	{
		stop();
	}

	u64 stop(bool quiet=false);

	u64 getTimerTime();

private:
	std::string m_name;
	u64 m_time1 = 0;
	bool m_running = true;
	TimePrecision m_precision;
	u64 *m_result = nullptr;
};
