
#ifndef UTIL_LOCK_HEADER
#define UTIL_LOCK_HEADER

#include <mutex>
#include <atomic>

#if USE_BOOST
//#include <boost/thread/locks.hpp>
#include <ctime>
#include <boost/thread.hpp>
typedef boost::shared_mutex try_shared_mutex;
typedef boost::shared_lock<try_shared_mutex> try_shared_lock;
typedef boost::unique_lock<try_shared_mutex> unique_lock;
#elif 0 and __cplusplus >= 201305L
#include <shared_mutex>
typedef std::shared_timed_mutex try_shared_mutex;
typedef std::shared_lock try_shared_lock<try_shared_mutex>;
typedef std::unique_lock<try_shared_mutex> unique_lock;
#else
typedef std::timed_mutex try_shared_mutex;
typedef std::unique_lock<try_shared_mutex> try_shared_lock;
typedef std::unique_lock<try_shared_mutex> unique_lock;
#endif



// http://stackoverflow.com/questions/4792449/c0x-has-no-semaphores-how-to-synchronize-threads
#include <condition_variable>
class semaphore{
private:
    std::mutex mtx;
    std::condition_variable cv;
    int count;

public:
    semaphore(int count_ = 0):count(count_){;}
    void notify()
    {
        std::unique_lock<std::mutex> lck(mtx);
        ++count;
        cv.notify_one();
    }
    void wait()
    {
        std::unique_lock<std::mutex> lck(mtx);

        while(count == 0){
            cv.wait(lck);
        }
        count--;
    }
};

class locker {
public:
	try_shared_mutex mtx;
	semaphore sem;
	bool lock_ext;

	locker() {
		lock_ext = 0;
	}

	unique_lock lock_unique() {
		lock_ext = 1;
		return unique_lock(mtx);
	}

	try_shared_lock lock_shared() {
		lock_ext = 1;
		return try_shared_lock(mtx);
	}

	unique_lock lock_unique_int() {
		auto lock = unique_lock(mtx, std::defer_lock);
		if (!lock_ext)
			lock.lock();
		return lock;
	}

	try_shared_lock lock_shared_int() {
		auto lock = try_shared_lock(mtx, std::defer_lock);
		if (!lock_ext)
			lock.lock();
		return lock;
	}

	void unlock_ext() {
		lock_ext = 0;
	}

};


#include <map>
template <class Key, class T, class Compare = std::less<Key>,
          class Allocator = std::allocator<std::pair<const Key, T>>>
class shared_map: public std::map<Key, T, Compare, Allocator>,
public locker
{

    typedef Key                                      key_type;
    typedef T                                        mapped_type;
	typedef Allocator                                allocator_type;
	typedef typename allocator_type::size_type       size_type;

    //typedef implementation-defined                   iterator;
    //typedef implementation-defined                   const_iterator;

	typedef typename std::map<Key, T, Compare, Allocator> full_type;

//	mapped_type& operator[](const key_type& k) { }
public:

/*
	try_shared_mutex mtx;
	semaphore sem;

	unique_lock lock_unique() {
		return unique_lock(mtx);
	}

	try_shared_lock lock_shared() {
		return try_shared_lock(mtx);
	}
*/

	mapped_type& get(const key_type& k) {
		auto lock = lock_shared_int();
		return (*this)[k];
	}

	void set(const key_type& k, const mapped_type& v) {
		auto lock = lock_unique_int();
		(*this)[k] = v;
	}

	bool      empty()    noexcept {
		auto lock = lock_shared_int();
		return full_type::empty();
	}
	size_type size()     noexcept {
		auto lock = lock_shared_int();
		return full_type::size();
	}

	size_type count(const key_type& k) {
		auto lock = lock_shared_int();
		return full_type::count(k);
	}
/*
	mapped_type& operator[](key_type&& k) {
		//unique_lock lock(mtx);
		auto lock = lock_unique();
		return full_type::operator[](k);
	}
*/
	typename full_type::iterator  erase(typename full_type::const_iterator position) {
		auto lock = lock_unique_int();
		return full_type::erase(position);
	}
//    size_type erase(const key_type& k);
//    iterator  erase(const_iterator first, const_iterator last);
    void clear() noexcept {
		auto lock = lock_unique_int();
		full_type::clear();
	}



};


#endif
