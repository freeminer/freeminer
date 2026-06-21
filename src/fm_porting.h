/*
Copyright (C) 2022 proller <proler@gmail.com>
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

#include "porting.h"

#if defined(linux) || defined(__linux)
	#include <sys/prctl.h>
#elif defined(__FreeBSD__) || defined(__OpenBSD__)
	#include <pthread.h>
	#include <pthread_np.h>
#elif defined(__NetBSD__)
	#include <pthread.h>
#elif defined(__APPLE__)
	#include <pthread.h>
#endif

#if defined(linux) || defined(__linux) || defined(__FreeBSD__) || defined(__NetBSD__) || defined(__OpenBSD__)
	#define PORTING_USE_PTHREAD 1
	#include <pthread.h>
#endif

namespace porting
{

extern std::atomic_bool g_sighup, g_siginfo;


#if defined(linux) || defined(__linux)
	inline void setThreadName(const char *name) {
		/* It would be cleaner to do this with pthread_setname_np,
		 * which was added to glibc in version 2.12, but some major
		 * distributions are still runing 2.11 and previous versions.
		 */
		prctl(PR_SET_NAME, name);
	}
#elif defined(__FreeBSD__) || defined(__OpenBSD__)
	inline void setThreadName(const char *name) {
		pthread_set_name_np(pthread_self(), name);
	}
#elif defined(__NetBSD__)
	inline void setThreadName(const char *name) {
		pthread_setname_np(pthread_self(), name);
	}
#elif defined(_MSC_VER)
	typedef struct tagTHREADNAME_INFO {
		DWORD dwType; // must be 0x1000
		LPCSTR szName; // pointer to name (in user addr space)
		DWORD dwThreadID; // thread ID (-1=caller thread)
		DWORD dwFlags; // reserved for future use, must be zero
	} THREADNAME_INFO;

	inline void setThreadName(const char *name) {
		THREADNAME_INFO info;
		info.dwType = 0x1000;
		info.szName = name;
		info.dwThreadID = -1;
		info.dwFlags = 0;
		__try {
			RaiseException(0x406D1388, 0, sizeof(info) / sizeof(DWORD), (ULONG_PTR *) &info);
		} __except (EXCEPTION_CONTINUE_EXECUTION) {}
	}
#elif defined(__APPLE__)
	inline void setThreadName(const char *name) {
		pthread_setname_np(name);
	}
#elif defined(_WIN32) || defined(__GNU__)
	inline void setThreadName(const char* name) {}
#else
#ifndef __EMSCRIPTEN__
	#warning "Unrecognized platform, thread names will not be available."
#endif

	inline void setThreadName(const char* name) {}
#endif

	inline void setThreadPriority(int priority) {
#if PORTING_USE_PTHREAD
	// http://en.cppreference.com/w/cpp/thread/thread/native_handle
		sched_param sch;
		//int policy;
		//pthread_getschedparam(pthread_self(), &policy, &sch);
		sch.sched_priority = priority;
		if(pthread_setschedparam(pthread_self(), SCHED_FIFO /*SCHED_RR*/, &sch)) {
			//std::cout << "Failed to setschedparam: " << std::strerror(errno) << '\n';
		}
#endif
	}


#if IS_CLIENT_BUILD

//void irr_device_wait_egl (irr::IrrlichtDevice * device = nullptr);

#endif


}
