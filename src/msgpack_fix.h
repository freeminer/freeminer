
#if defined(__APPLE__)
	#undef nil
#endif

#if (defined(WIN32) || defined(_WIN32) || defined(_WIN32_WCE))
	#define WIN32_LEAN_AND_MEAN
	#ifndef _WIN32_WINNT
		#define _WIN32_WINNT 0x0501
	#endif
#endif


#include <msgpack.hpp>
