#include "lock.h"
#include "../log.h"
#include "../profiler.h"

#if !defined(NDEBUG) && !defined(LOCK_PROFILE)
//#define LOCK_PROFILE 1
#endif

#if LOCK_PROFILE
#define SCOPE_PROFILE(a) ScopeProfiler scp___(g_profiler, "Lock: " a);
#else
#define SCOPE_PROFILE(a)
#endif

template<class GUARD>
lock_rec<GUARD>::lock_rec(try_shared_mutex & mtx, std::atomic<std::size_t> & thread_id_, bool try_lock):
	thread_id(thread_id_) {
	auto thread_me = std::hash<std::thread::id>()(std::this_thread::get_id());
	if(thread_me != thread_id) {
		if (try_lock) {
			SCOPE_PROFILE("try_lock");
			lock = new GUARD(mtx, TRY_TO_LOCK);
			if (lock->owns_lock()) {
				thread_id = thread_me;
				return;
			} else {
#if LOCK_PROFILE
				g_profiler->add("Lock: try_lock fail", 1);
#endif
				//infostream<<"not locked "<<" thread="<<thread_id<<" lock="<<lock<<std::endl;
			}
			delete lock;
		} else {
			SCOPE_PROFILE("lock");
			lock = new GUARD(mtx);
			thread_id = thread_me;
			return;
		}
	} else {
#if LOCK_PROFILE
		g_profiler->add("Lock: recursive", 1);
#endif
	}
	lock = nullptr;
}

template<class GUARD>
lock_rec<GUARD>::~lock_rec() {
	unlock();
}

template<class GUARD>
bool lock_rec<GUARD>::owns_lock() {
	if (lock)
		return lock;
	auto thread_me = std::hash<std::thread::id>()(std::this_thread::get_id());
	return thread_id == thread_me;
}

template<class GUARD>
void lock_rec<GUARD>::unlock() {
	if(lock) {
		thread_id = 0;
		lock->unlock();
		delete lock;
		lock = nullptr;
	}
}

locker::locker() {
	thread_id = 0;
}

std::unique_ptr<unique_lock> locker::lock_unique() {
	return std::unique_ptr<unique_lock>(new unique_lock(mtx));
}

std::unique_ptr<unique_lock> locker::try_lock_unique() {
	SCOPE_PROFILE("locker::try_lock_unique");
	return std::unique_ptr<unique_lock>(new unique_lock(mtx, std::try_to_lock));
}

std::unique_ptr<try_shared_lock> locker::lock_shared() {
	SCOPE_PROFILE("locker::lock_shared");
	return std::unique_ptr<try_shared_lock>(new try_shared_lock(mtx));
}

std::unique_ptr<try_shared_lock> locker::try_lock_shared() {
	SCOPE_PROFILE("locker::try_lock_shared");
	return std::unique_ptr<try_shared_lock>(new try_shared_lock(mtx, std::try_to_lock));
}

std::unique_ptr<lock_rec<unique_lock>> locker::lock_unique_rec() {
	SCOPE_PROFILE("locker::lock_unique_rec");
	return std::unique_ptr<lock_rec<unique_lock>>(new lock_rec<unique_lock> (mtx, thread_id));
}

std::unique_ptr<lock_rec<unique_lock>> locker::try_lock_unique_rec() {
	SCOPE_PROFILE("locker::try_lock_unique_rec");
	return std::unique_ptr<lock_rec<unique_lock>>(new lock_rec<unique_lock> (mtx, thread_id, true));
}

std::unique_ptr<lock_rec<try_shared_lock>> locker::lock_shared_rec() {
	SCOPE_PROFILE("locker::lock_shared_rec");
	return std::unique_ptr<lock_rec<try_shared_lock>>(new lock_rec<try_shared_lock> (mtx, thread_id));
}

std::unique_ptr<lock_rec<try_shared_lock>> locker::try_lock_shared_rec() {
	SCOPE_PROFILE("locker::try_lock_shared_rec");
	return std::unique_ptr<lock_rec<try_shared_lock>>(new lock_rec<try_shared_lock> (mtx, thread_id, true));
}


template class lock_rec<unique_lock>;
#if LOCK_TWO
template class lock_rec<try_shared_lock>;
#endif
