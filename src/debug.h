// Luanti
// SPDX-License-Identifier: LGPL-2.1-or-later
// Copyright (C) 2013 celeron55, Perttu Ahola <celeron55@gmail.com>

#pragma once

#include <exception>
#include <cassert>
#include "gettime.h"
#include "debug/stacktrace.h"
#include "log.h"
#include "config.h"

#ifdef _MSC_VER
	#define FUNCTION_NAME __FUNCTION__
#else
	#define FUNCTION_NAME __PRETTY_FUNCTION__
#endif

// Whether to catch all std::exceptions.
// When "catching", the program will abort with an error message.
// In debug mode, leave these for the debugger and don't catch them.
#ifdef NDEBUG
	#define CATCH_UNHANDLED_EXCEPTIONS 1
#else
	#define CATCH_UNHANDLED_EXCEPTIONS 0
#endif

/* Abort program execution immediately
 */
[[noreturn]] extern void fatal_error_fn(
		const char *msg, const char *file,
		unsigned int line, const char *function);

#define FATAL_ERROR(msg) \
	fatal_error_fn((msg), __FILE__, __LINE__, FUNCTION_NAME)

#define FATAL_ERROR_IF(expr, msg) \
	((expr) \
	? fatal_error_fn((msg), __FILE__, __LINE__, FUNCTION_NAME) \
	: (void)(0))

/*
	sanity_check()
	Equivalent to assert() but persists in Release builds (i.e. when NDEBUG is
	defined)
*/

[[noreturn]] extern void sanity_check_fn(
		const char *assertion, const char *file,
		unsigned int line, const char *function);

#define SANITY_CHECK(expr) \
	((expr) \
	? (void)(0) \
	: sanity_check_fn(#expr, __FILE__, __LINE__, FUNCTION_NAME))

#define sanity_check(expr) SANITY_CHECK(expr)

std::string debug_describe_exc(const std::exception &e);

void debug_set_exception_handler();

/*
	These should be put into every thread
*/

#if CATCH_UNHANDLED_EXCEPTIONS == 1
	#define BEGIN_DEBUG_EXCEPTION_HANDLER try {
	#define END_DEBUG_EXCEPTION_HANDLER                        \
		} catch (const std::exception &e) {                          \
			std::string e_descr = debug_describe_exc(e);       \
			errorstream << "An unhandled exception occurred: " \
				<< e_descr << std::endl << stacktrace() << std::endl;                       \
			FATAL_ERROR(e_descr.c_str());                      \
		} catch (...) {                    \
			errorstream << "An unknown unhandled exception occurred at " \
				<< __PRETTY_FUNCTION__ << ":" << __LINE__ << std::endl << stacktrace() << std::endl; \
			FATAL_ERROR("unknown");                             \
		}
#else
	// Dummy ones
	#define BEGIN_DEBUG_EXCEPTION_HANDLER
	#define END_DEBUG_EXCEPTION_HANDLER
#endif

//#define EXCEPTION_DEBUG 1 // Disable almost all catch() to get good system stacktraces

#if EXCEPTION_DEBUG
	#define EXCEPTION_HANDLER_BEGIN
	#define EXCEPTION_HANDLER_END
#else
	#define EXCEPTION_HANDLER_BEGIN try {
	#define EXCEPTION_HANDLER_END } catch (const std::exception &e) { errorstream << m_name << ": An unhandled exception occurred: " << e.what() << std::endl << stacktrace() << std::endl; } \
									catch (...)               { errorstream << m_name << ": Unknown unhandled exception at " << __PRETTY_FUNCTION__ << ":" << __LINE__ << std::endl << stacktrace() << std::endl; }
#endif
