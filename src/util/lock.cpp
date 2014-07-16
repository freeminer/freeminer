#include "lock.h"

#include "../log.h"

template<class T>
lock_rec<T>::lock_rec(T * lock_, std::atomic_int & r_, std::atomic<std::size_t> & thread_id_):
	lock(lock_),
	r(r_),
	thread_id(thread_id_) {
	auto thread_me = std::hash<std::thread::id>()(std::this_thread::get_id());
	//infostream<<" lock="<<lock->mutex()<<" thread_me="<<thread_me<<" thread_id="<<thread_id<<" r="<<r<<std::endl;
	if(!r++ || thread_me != thread_id) {
		lock->lock();
		thread_id = thread_me;
	}
}

template<class T>
lock_rec<T>::~lock_rec() {
	//auto thread_me = std::hash<std::thread::id>()(std::this_thread::get_id());
	//infostream<<" unlock="<<lock->mutex()<<" thread_me="<<thread_me<<" thread_id="<<thread_id<<" r="<<r<<std::endl;
	if(!--r /*&& thread_me == thread_id*/) {
		//lock->unlock();
	}
	delete lock;
}


locker::locker() {
	r = 0;
	//infostream<<" locker() r="<<r<<" mtx="<<&mtx<<" thread_id="<<thread_id<<std::endl;
}

unique_lock locker::lock_unique() {
	//infostream<<" locker::lock_unique() r="<<r<<" mtx="<<&mtx<<" thread_id="<<thread_id<<std::endl;
	return unique_lock(mtx);
}

try_shared_lock locker::lock_shared() {
	//infostream<<" locker::lock_shared() r="<<r<<" mtx="<<&mtx<<" thread_id="<<thread_id<<std::endl;
	return try_shared_lock(mtx);
}

lock_rec<unique_lock> locker::lock_unique_rec() {
	//infostream<<" locker::lock_unique_rec() r="<<r<<" mtx="<<&mtx<<" thread_id="<<thread_id<<std::endl;
	return lock_rec<unique_lock> (new unique_lock(mtx, DEFER_LOCK), r, thread_id);
}

lock_rec<try_shared_lock> locker::lock_shared_rec() {
	//infostream<<" locker::lock_shared_rec() r="<<r<<" mtx="<<&mtx<<" thread_id="<<thread_id<<std::endl;
	return lock_rec<try_shared_lock> (new try_shared_lock(mtx, DEFER_LOCK), r, thread_id);
}


template class lock_rec<unique_lock>;
#if LOCK_TWO
template class lock_rec<try_shared_lock>;
#endif
