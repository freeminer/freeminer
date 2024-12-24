#include "getThreadId.h"

#if defined(__ANDROID__)
    #include <sys/types.h>
    #include <unistd.h>
#elif defined(__linux__)
    #include <unistd.h>
    #include <syscall.h>
#elif defined(__FreeBSD__) || defined(__NetBSD__) || defined(__DragonFly__)
    #include <pthread_np.h>
#elif defined(_WIN32)
    #include<processthreadsapi.h>
#elif !defined(__EMSCRIPTEN__)
    #include <pthread.h>
    #include <stdexcept>
#endif


static thread_local uint64_t current_tid = 0;
uint64_t getThreadId()
{
    if (!current_tid)
    {
#if defined(__ANDROID__)
        current_tid = gettid();
#elif defined(__linux__)
        current_tid = syscall(SYS_gettid); /// This call is always successful. - man gettid
#elif defined(__FreeBSD__) || defined(__NetBSD__) || defined(__DragonFly__)
        current_tid = pthread_getthreadid_np();
#elif defined(OS_SUNOS)
        // On Solaris-derived systems, this returns the ID of the LWP, analogous
        // to a thread.
        current_tid = static_cast<uint64_t>(pthread_self());

#elif defined(_WIN32)
        current_tid = GetCurrentThreadId();
#elif !defined(__EMSCRIPTEN__)
        if (0 != pthread_threadid_np(nullptr, &current_tid))
            throw std::logic_error("pthread_threadid_np returned error");
#endif
    }

    return current_tid;
}
