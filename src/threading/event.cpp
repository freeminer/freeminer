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

#include "threading/event.h"
#include "threading/mutex_auto_lock.h"

#if __cplusplus < 201103L
Event::Event()
{
#ifdef _WIN32
	event = CreateEvent(NULL, false, false, NULL);
#else
	pthread_cond_init(&cv, NULL);
	pthread_mutex_init(&mutex, NULL);
#endif
}

Event::~Event()
{
#ifdef _WIN32
	CloseHandle(event);
#else
	pthread_cond_destroy(&cv);
	pthread_mutex_destroy(&mutex);
#endif
}
#endif


void Event::wait()
{
#if __cplusplus >= 201103L
	MutexAutoLock lock(mutex);
	while (!notified) {
		cv.wait(lock);
	}
	notified = false;
#elif defined(_WIN32)
	WaitForSingleObject(event, INFINITE);
#else
	pthread_mutex_lock(&mutex);
	while (!notified) {
		pthread_cond_wait(&cv, &mutex);
	}
	notified = false;
	pthread_mutex_unlock(&mutex);
#endif
}


void Event::signal()
{
#if __cplusplus >= 201103L
	MutexAutoLock lock(mutex);
	notified = true;
	cv.notify_one();
#elif defined(_WIN32)
	SetEvent(event);
#else
	pthread_mutex_lock(&mutex);
	notified = true;
	pthread_cond_signal(&cv);
	pthread_mutex_unlock(&mutex);
#endif
}
