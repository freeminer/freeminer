#include "lock.h"
#include "../log.h"

template<class T>
lock_rec<T>::lock_rec(T * lock_, std::atomic<std::size_t> & thread_id_, bool try_lock):
	thread_id(thread_id_) {
	auto thread_me = std::hash<std::thread::id>()(std::this_thread::get_id());
	if(thread_me != thread_id) {
		if (try_lock) {
			if (lock_->try_lock()) {
				lock = lock_;
				thread_id = thread_me;
				return;
			} else {
				//infostream<<"not locked in "<<ms.count()<<" thread="<<thread_id<<" lock="<<lock<<std::endl;
			}
		} else {
			lock_->lock();
			lock = lock_;
			thread_id = thread_me;
			return;
		}
	}
	delete lock_;
	lock = nullptr;
}

template<class T>
lock_rec<T>::~lock_rec() {
	if(owns_lock()) {
		thread_id = 0;
		lock->unlock();
		delete lock;
		lock = nullptr;
	}
}

template<class T>
bool lock_rec<T>::owns_lock() {
	return lock;
}

locker::locker() {
	thread_id = 0;
}

std::unique_ptr<unique_lock> locker::lock_unique() {
	return std::unique_ptr<unique_lock>(new unique_lock(mtx));
}

std::unique_ptr<unique_lock> locker::try_lock_unique() {
	return std::unique_ptr<unique_lock>(new unique_lock(mtx, std::try_to_lock));
}

std::unique_ptr<try_shared_lock> locker::lock_shared() {
	return std::unique_ptr<try_shared_lock>(new try_shared_lock(mtx));
}

std::unique_ptr<try_shared_lock> locker::try_lock_shared() {
	return std::unique_ptr<try_shared_lock>(new try_shared_lock(mtx, std::try_to_lock));
}

std::unique_ptr<lock_rec<unique_lock>> locker::lock_unique_rec() {
	return std::unique_ptr<lock_rec<unique_lock>>(new lock_rec<unique_lock> (new unique_lock(mtx, DEFER_LOCK), thread_id));
}

std::unique_ptr<lock_rec<unique_lock>> locker::try_lock_unique_rec() {
	return std::unique_ptr<lock_rec<unique_lock>>(new lock_rec<unique_lock> (new unique_lock(mtx, DEFER_LOCK), thread_id, true));
}

std::unique_ptr<lock_rec<try_shared_lock>> locker::lock_shared_rec() {
	return std::unique_ptr<lock_rec<try_shared_lock>>(new lock_rec<try_shared_lock> (new try_shared_lock(mtx, DEFER_LOCK), thread_id));
}

std::unique_ptr<lock_rec<try_shared_lock>> locker::try_lock_shared_rec() {
	return std::unique_ptr<lock_rec<try_shared_lock>>(new lock_rec<try_shared_lock> (new try_shared_lock(mtx, DEFER_LOCK), thread_id, true));
}


template class lock_rec<unique_lock>;
#if LOCK_TWO
template class lock_rec<try_shared_lock>;
#endif
     //