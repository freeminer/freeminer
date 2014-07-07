
#ifndef UTIL_LOCK_HEADER
#define UTIL_LOCK_HEADER

#include <mutex>
#include <atomic>
#include <thread>

#if USE_BOOST // not finished

//#include <ctime>
#include <boost/thread.hpp>
//#include <boost/thread/locks.hpp>
typedef boost::shared_mutex try_shared_mutex;
typedef boost::shared_lock<try_shared_mutex> try_shared_lock;
typedef boost::unique_lock<try_shared_mutex> unique_lock;
#define DEFER_LOCK boost::defer_lock

#elif CMAKE_HAVE_SHARED_MUTEX
//#elif __cplusplus >= 201305L

#include <shared_mutex>
typedef std::shared_timed_mutex try_shared_mutex;
typedef std::shared_lock<try_shared_mutex> try_shared_lock;
typedef std::unique_lock<try_shared_mutex> unique_lock;
#define DEFER_LOCK std::defer_lock

#else

//typedef std::timed_mutex try_shared_mutex;
typedef std::mutex try_shared_mutex;
typedef std::unique_lock<try_shared_mutex> try_shared_lock;
typedef std::unique_lock<try_shared_mutex> unique_lock;
#define DEFER_LOCK std::defer_lock

#endif


// http://stackoverflow.com/questions/4792449/c0x-has-no-semaphores-how-to-synchronize-threads
/* uncomment when need
#include <condition_variable>
class semaphore {
private:
	std::mutex mtx;
	std::condition_variable cv;
	int count;

public:
	semaphore(int count_ = 0):count(count_){;}
	void notify() {
	std::unique_lock<std::mutex> lck(mtx);
		++count;
		cv.notify_one();
	}
	void wait() {
		std::unique_lock<std::mutex> lck(mtx);
		while(count == 0){
			cv.wait(lck);
		}
		count--;
	}
};
*/

template<class T>
class lock_rec {
public:
	T & lock;
	std::atomic_int &r;
	std::thread::id & thread_id;
	lock_rec(T & lock_, std::atomic_int & r_, std::thread::id & thread_id_):
		lock(lock_),
		r(r_),
		thread_id(thread_id_) {
		auto thread_me = std::this_thread::get_id();
		if(!r || thread_me != thread_id) {
			lock.lock();
			thread_id = thread_me;
		}
		++r;
	}
	~lock_rec() {
		if(!--r) {
			//lock.unlock();
		}
	}
};

class locker {
public:
	try_shared_mutex mtx;
	//semaphore sem;
	std::atomic_int r;
	std::thread::id thread_id;

	locker() {
		r = 0;
	}

	unique_lock lock_unique() {
		return unique_lock(mtx);
	}

	try_shared_lock lock_shared() {
		return try_shared_lock(mtx);
	}

	lock_rec<unique_lock> lock_unique_rec() {
		auto lock = unique_lock(mtx, DEFER_LOCK);
		return lock_rec<unique_lock> (lock, r, thread_id);
	}

	lock_rec<try_shared_lock> lock_shared_rec() {
		auto lock = try_shared_lock(mtx, DEFER_LOCK);
		return lock_rec<try_shared_lock> (lock, r, thread_id);
	}
};


#include <map>
template <class Key, class T, class Compare = std::less<Key>,
          class Allocator = std::allocator<std::pair<const Key, T>>>
                  class shared_map: public std::map<Key, T, Compare, Allocator>,
public locker {
public:

	typedef Key                                      key_type;
	typedef T                                        mapped_type;
	typedef Allocator                                allocator_type;
	typedef typename allocator_type::size_type       size_type;
	typedef typename std::map<Key, T, Compare, Allocator> full_type;
	typedef typename full_type::const_iterator const_iterator;
	typedef typename full_type::iterator iterator;

	mapped_type& get(const key_type& k) {
		auto lock = lock_shared();
		return (*this)[k];
	}

	void set(const key_type& k, const mapped_type& v) {
		auto lock = lock_unique();
		full_type::operator[](k) = v;
	}

	bool      empty() {
		auto lock = lock_shared();
		return full_type::empty();
	}

	size_type size() {
		auto lock = lock_shared();
		return full_type::size();
	}

	size_type count(const key_type& k) {
		auto lock = lock_shared();
		return full_type::count(k);
	}

	mapped_type& operator[](const key_type& k) = delete;
	/*
	{ // UNSAFE
		auto lock = lock_unique();
		return full_type::operator[](k);
	}
	*/

	mapped_type& operator[](key_type&& k) = delete;
	/*
	{ // UNSAFE
		auto lock = lock_unique();
		return full_type::operator[](k);
	}
	*/

	typename full_type::iterator  erase(const_iterator position) {
		auto lock = lock_unique();
		return full_type::erase(position);
	}

	size_type erase(const key_type& k) {
		auto lock = lock_unique();
		return full_type::erase(k);
	}

	// iterator  erase(const_iterator first, const_iterator last);

	void clear() {
		auto lock = lock_unique();
		full_type::clear();
	}
};

#endif
