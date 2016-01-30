/*
script/cpp_api/s_internal.h
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

/******************************************************************************/
/******************************************************************************/
/* WARNING!!!! do NOT add this header in any include file or any code file    */
/*             not being a modapi file!!!!!!!!                                */
/******************************************************************************/
/******************************************************************************/

#ifndef S_INTERNAL_H_
#define S_INTERNAL_H_

#include "common/c_internal.h"
#include "cpp_api/s_base.h"
#include "config.h"

#if ENABLE_THREADS
#define SCRIPTAPI_LOCK auto _script_lock = std::unique_lock<std::recursive_mutex> (this->m_luastackmutex)
#else
#define SCRIPTAPI_LOCK
#endif

#ifdef SCRIPTAPI_LOCK_DEBUG
#include "debug.h" // assert()

class LockChecker {
public:
	LockChecker(int *recursion_counter, threadid_t *owning_thread)
	{
		m_lock_recursion_counter = recursion_counter;
		m_owning_thread          = owning_thread;
		m_original_level         = *recursion_counter;

		if (*m_lock_recursion_counter > 0)
			assert(thr_is_current_thread(*m_owning_thread));
		else
			*m_owning_thread = thr_get_current_thread_id();

		(*m_lock_recursion_counter)++;
	}

	~LockChecker()
	{
		assert(thr_is_current_thread(*m_owning_thread));
		assert(*m_lock_recursion_counter > 0);

		(*m_lock_recursion_counter)--;

		assert(*m_lock_recursion_counter == m_original_level);
	}

private:
	int *m_lock_recursion_counter;
	int m_original_level;
	threadid_t *m_owning_thread;
};

#define SCRIPTAPI_LOCK_CHECK           \
	LockChecker scriptlock_checker(    \
		&this->m_lock_recursion_count, \
		&this->m_owning_thread)

#else
	#define SCRIPTAPI_LOCK_CHECK while(0)
#endif


#define SCRIPTAPI_PRECHECKHEADER                                               \
		RecursiveMutexAutoLock scriptlock(this->m_luastackmutex);              \
		SCRIPTAPI_LOCK_CHECK;                                                  \
		realityCheck();                                                        \
		lua_State *L = getStack();                                             \
		StackUnroller stack_unroller(L);

#endif /* S_INTERNAL_H_ */

