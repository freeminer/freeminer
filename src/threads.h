/*
threads.h
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

#ifndef THREADS_HEADER
#define THREADS_HEADER

#include "threading/mutex.h"

#if defined(WIN32) || defined(_WIN32) || defined(_WIN32_WCE)
#include <winsock2.h>
#include <windows.h>
typedef DWORD threadid_t;
#else
typedef pthread_t threadid_t;
#endif

inline threadid_t get_current_thread_id()
{
#if defined(WIN32) || defined(_WIN32) || defined(_WIN32_WCE)
	return GetCurrentThreadId();
#else
	return pthread_self();
#endif
}

#endif

