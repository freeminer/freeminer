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

#ifndef THREADING_MUTEX_AUTO_LOCK_H
#define THREADING_MUTEX_AUTO_LOCK_H

#if __cplusplus >= 201103L
	#include <mutex>
	using MutexAutoLock = std::unique_lock<std::mutex>;
	using RecursiveMutexAutoLock = std::unique_lock<std::recursive_mutex>;
#else

#include "threading/mutex.h"


class MutexAutoLock
{
public:
	MutexAutoLock(Mutex &m) : mutex(m) { mutex.lock(); }
	~MutexAutoLock() { mutex.unlock(); }

private:
	Mutex &mutex;
};

class RecursiveMutexAutoLock
{
public:
	RecursiveMutexAutoLock(RecursiveMutex &m) : mutex(m) { mutex.lock(); }
	~RecursiveMutexAutoLock() { mutex.unlock(); }

private:
	RecursiveMutex &mutex;
};
#endif

#endif

