/*
jthread/jsemaphore.h
Copyright (C) 2013 sapier, < sapier AT gmx DOT net >
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

#ifndef JSEMAPHORE_H_
#define JSEMAPHORE_H_

#if defined(WIN32) || defined(_WIN32)
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <assert.h>
#define MAX_SEMAPHORE_COUNT 1024
#elif __MACH__
#include <pthread.h>
#include <mach/mach.h>
#include <mach/task.h>
#include <mach/semaphore.h>
#include <sys/semaphore.h>
#include <errno.h>
#include <time.h>
#else
#include <pthread.h>
#include <semaphore.h>
#endif

class JSemaphore {
public:
	JSemaphore();
	~JSemaphore();
	JSemaphore(int initval);

	void Post();
	void Wait();
	bool Wait(unsigned int time_ms);

	int GetValue();

private:
#if defined(WIN32) || defined(_WIN32)
	HANDLE m_hSemaphore;
#elif __MACH__
	semaphore_t m_semaphore;
	int semcount;
#else
	sem_t m_semaphore;
#endif
};


#endif /* JSEMAPHORE_H_ */
