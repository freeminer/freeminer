/*
util/timetaker.cpp
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

#include "timetaker.h"

#include "porting.h"
#include "log.h"
#include <ostream>

unsigned int g_time_taker_enabled = 0;

void TimeTaker::start()
{
	if (!m_time1)
	m_time1 = porting::getTime(m_precision);
}

u64 TimeTaker::stop(bool quiet)
{
	if (m_running) {
		u64 dtime = porting::getTime(m_precision) - m_time1;
		if (m_result != nullptr) {
			(*m_result) += dtime;
		} else {
			if (!quiet && dtime >= g_time_taker_enabled) {
				verbosestream << m_name << " took "
					<< dtime << TimePrecision_units[m_precision] << std::endl;
			}
		}
		m_running = false;
		return dtime;
	}
	return 0;
}

u64 TimeTaker::getTimerTime()
{
	return porting::getTime(m_precision) - m_time1;
}

