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

//
// Determine which threading API we will use
//
#if __cplusplus >= 201103L
	#define USE_CPP11_THREADS 1
#elif defined(_WIN32) || defined(WIN32)
	#define USE_WIN_THREADS 1
#else
	#define USE_POSIX_THREADS 1
#endif

///////////////


#if USE_CPP11_THREADS
	#include <thread>
#endif

#include "threading/mutex.h"

//
// threadid_t, threadhandle_t
//
#if USE_CPP11_THREADS
	//typedef std::thread::id threadid_t;
	typedef std::size_t threadid_t;
	typedef std::thread::native_handle_type threadhandle_t;
#elif USE_WIN_THREADS
	typedef DWORD threadid_t;
	typedef HANDLE threadhandle_t;
#elif USE_POSIX_THREADS
	typedef pthread_t threadid_t;
	typedef pthread_t threadhandle_t;
#endif

//
// ThreadStartFunc
//
#if USE_CPP11_THREADS || USE_POSIX_THREADS
	typedef void *ThreadStartFunc(void *param);
#elif defined(_WIN32_WCE)
	typedef DWORD ThreadStartFunc(LPVOID param);
#elif defined(_WIN32) || defined(WIN32)
	typedef DWORD WINAPI ThreadStartFunc(LPVOID param);
#endif


inline threadid_t thr_get_current_thread_id()
{
#if USE_CPP11_THREADS
	return std::hash<std::thread::id>()(std::this_thread::get_id());
/*
	return std::this_thread::get_id();
*/
#elif USE_WIN_THREADS
	return GetCurrentThreadId();
#elif USE_POSIX_THREADS
	return pthread_self();
#endif
}


inline bool thr_compare_thread_id(threadid_t thr1, threadid_t thr2)
{
#if USE_POSIX_THREADS
	return pthread_equal(thr1, thr2);
#else
	return thr1 == thr2;
#endif
}

inline bool thr_is_current_thread(threadid_t thr)
{
	return thr_compare_thread_id(thr_get_current_thread_id(), thr);
}

#endif
