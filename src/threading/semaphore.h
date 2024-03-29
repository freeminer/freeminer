/*
This file is part of Freeminer.
Copyright (C) 2013 sapier <sapier AT gmx DOT net>

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

#if defined(_WIN32)
#include <windows.h>
#elif defined(__MACH__) && defined(__APPLE__)
#include <mach/semaphore.h>
#else
#include <semaphore.h>
#endif

#include "util/basic_macros.h"

class Semaphore
{
public:
	Semaphore(int val = 0);
	~Semaphore();

	DISABLE_CLASS_COPY(Semaphore);

	void post(unsigned int num = 1);
	void wait();
	bool wait(unsigned int time_ms);

private:
#if defined(WIN32) || defined(_WIN32)
	HANDLE semaphore;
#elif defined(__MACH__) && defined(__APPLE__)
	semaphore_t semaphore;
#else
	sem_t semaphore;
#endif
};
