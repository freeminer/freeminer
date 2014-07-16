#include "lock.h"

template<class T>
lock_rec<T>::lock_rec(T & lock_, std::atomic_int & r_, std::atomic<std::size_t> & thread_id_):
	lock(lock_),
	r(r_),
	thread_id(thread_id_) {
	auto thread_me = std::hash<std::thread::id>()(std::this_thread::get_id());
	if(!r || thread_me != thread_id) {
		lock.lock();
		thread_id = thread_me;
	}
	++r;
}

template<class T>
lock_rec<T>::~lock_rec() {
	if(!--r) {
		//lock.unlock();
	}
}


locker::locker() {
	r = 0;
}

unique_lock locker::lock_unique() {
	return unique_lock(mtx);
}

try_shared_lock locker::lock_shared() {
	return try_shared_lock(mtx);
}

lock_rec<unique_lock> locker::lock_unique_rec() {
	auto lock = unique_lock(mtx, DEFER_LOCK);
	return lock_rec<unique_lock> (lock, r, thread_id);
}

lock_rec<try_shared_lock> locker::lock_shared_rec() {
	auto lock = try_shared_lock(mtx, DEFER_LOCK);
	return lock_rec<try_shared_lock> (lock, r, thread_id);
}


template class lock_rec<unique_lock>;
#if LOCK_TWO
template class lock_rec<try_shared_lock>;
#endif
