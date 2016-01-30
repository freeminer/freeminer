/*
This file is a part of the JThread package, which contains some object-
oriented thread wrappers for different thread implementations.

Copyright (c) 2000-2006  Jori Liesenborgs (jori.liesenborgs@gmail.com)

Permission is hereby granted, free of charge, to any person obtaining a
copy of this software and associated documentation files (the "Software"),
to deal in the Software without restriction, including without limitation
the rights to use, copy, modify, merge, publish, distribute, sublicense,
and/or sell copies of the Software, and to permit persons to whom the
Software is furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in
all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.  IN NO EVENT SHALL
THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
DEALINGS IN THE SOFTWARE.
*/

#ifndef THREADING_EVENT_H
#define THREADING_EVENT_H

#if __cplusplus >= 201103L
	#include <condition_variable>
	#include "threading/mutex.h"
	#include "threading/mutex_auto_lock.h"
#elif defined(_WIN32)
	#ifndef WIN32_LEAN_AND_MEAN
		#define WIN32_LEAN_AND_MEAN
	#endif
	#include <windows.h>
#else
	#include <pthread.h>
#endif


/** A syncronization primitive that will wake up one waiting thread when signaled.
 * Calling @c signal() multiple times before a waiting thread has had a chance
 * to notice the signal will wake only one thread.  Additionally, if no threads
 * are waiting on the event when it is signaled, the next call to @c wait()
 * will return (almost) immediately.
 */
class Event {
public:
#if __cplusplus < 201103L
	Event();
	~Event();
#endif
	void wait();
	void signal();

private:
#if __cplusplus >= 201103L
	std::condition_variable cv;
	Mutex mutex;
	bool notified;
#elif defined(_WIN32)
	HANDLE event;
#else
	pthread_cond_t cv;
	pthread_mutex_t mutex;
	bool notified;
#endif
};

#endif
