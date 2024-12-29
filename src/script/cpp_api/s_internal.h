// Luanti
// SPDX-License-Identifier: LGPL-2.1-or-later
// Copyright (C) 2013 celeron55, Perttu Ahola <celeron55@gmail.com>

/******************************************************************************/
/******************************************************************************/
/* WARNING!!!! do NOT add this header in any include file or any code file    */
/*             not being a modapi file!!!!!!!!                                */
/******************************************************************************/
/******************************************************************************/

#pragma once

#include <thread>
#include "common/c_internal.h"
#include "cpp_api/s_base.h"
/*
#include "config.h"

#if ENABLE_THREADS
#define SCRIPTAPI_LOCK auto _script_lock = std::unique_lock<std::recursive_mutex> (this->m_luastackmutex)
#else
#define SCRIPTAPI_LOCK
#endif
*/
#include "threading/mutex_auto_lock.h"

#ifdef SCRIPTAPI_LOCK_DEBUG
#include <cassert>

class LockChecker {
public:
	LockChecker(int *recursion_counter, std::thread::id *owning_thread)
	{
		m_lock_recursion_counter = recursion_counter;
		m_owning_thread          = owning_thread;
		m_original_level         = *recursion_counter;

		if (*m_lock_recursion_counter > 0) {
			assert(*m_owning_thread == std::this_thread::get_id());
		} else {
			*m_owning_thread = std::this_thread::get_id();
		}

		(*m_lock_recursion_counter)++;
	}

	~LockChecker()
	{
		assert(*m_owning_thread == std::this_thread::get_id());
		assert(*m_lock_recursion_counter > 0);

		(*m_lock_recursion_counter)--;

		assert(*m_lock_recursion_counter == m_original_level);
	}

private:
	int *m_lock_recursion_counter;
	int m_original_level;
	std::thread::id *m_owning_thread;
};

#define SCRIPTAPI_LOCK_CHECK           \
	LockChecker scriptlock_checker(    \
		&this->m_lock_recursion_count, \
		&this->m_owning_thread)

#else
	#define SCRIPTAPI_LOCK_CHECK while(0)
#endif

#define TRY_SCRIPTAPI_PRECHECKHEADER(RET)                                           \
		const std::unique_lock<std::recursive_mutex> scriptlock(this->m_luastackmutex, std::try_to_lock); \
		if (!scriptlock.owns_lock()) { /* DUMP("skiplock", scriptlock); */ return RET; } \
		SCRIPTAPI_LOCK_CHECK;                                                  \
		realityCheck();                                                        \
		lua_State *L = getStack();                                             \
		StackUnroller stack_unroller(L);



#define SCRIPTAPI_PRECHECKHEADER                                               \
		RecursiveMutexAutoLock scriptlock(this->m_luastackmutex);              \
		SCRIPTAPI_LOCK_CHECK;                                                  \
		realityCheck();                                                        \
		lua_State *L = getStack();                                             \
		StackUnroller stack_unroller(L);
