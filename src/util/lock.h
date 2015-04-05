/*
This file is part of Freeminer.

Freeminer is free software: you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation, either version 3 of the License, or
(at your option) any later version.

Freeminer  is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with Freeminer.  If not, see <http://www.gnu.org/licenses/>.
*/

#ifndef UTIL_LOCK_HEADER
#define UTIL_LOCK_HEADER

#include <mutex>
#include <atomic>
#include <thread>
//#include <chrono>
#include <memory>

#include "config.h"

#ifdef _MSC_VER
#define noexcept
#endif

#if USE_BOOST // not finished

//#include <ctime>
#include <boost/thread.hpp>
//#include <boost/thread/locks.hpp>
typedef boost::shared_mutex try_shared_mutex;
typedef boost::shared_lock<try_shared_mutex> try_shared_lock;
typedef boost::unique_lock<try_shared_mutex> unique_lock;
#define DEFER_LOCK boost::defer_lock
#define TRY_TO_LOCK boost::try_to_lock
#define LOCK_TWO 1

#elif HAVE_SHARED_MUTEX
//#elif __cplusplus >= 201305L

#include <shared_mutex>
typedef std::shared_timed_mutex try_shared_mutex;
typedef std::shared_lock<try_shared_mutex> try_shared_lock;
typedef std::unique_lock<try_shared_mutex> unique_lock;
#define DEFER_LOCK	std::defer_lock
#define TRY_TO_LOCK	std::try_to_lock
#define LOCK_TWO 1

#else

//typedef std::timed_mutex try_shared_mutex;
typedef std::mutex try_shared_mutex;
typedef std::unique_lock<try_shared_mutex> try_shared_lock;
typedef std::unique_lock<try_shared_mutex> unique_lock;
#define DEFER_LOCK std::defer_lock
#define TRY_TO_LOCK	std::try_to_lock
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

template<class guard, class mutex = std::mutex>
class recursive_lock {
public:
	guard * lock;
	std::atomic<std::size_t> & thread_id;
	recursive_lock(mutex & mtx, std::atomic<std::size_t> & thread_id_, bool try_lock = false);
	~recursive_lock();
	bool owns_lock();
	void unlock();
};

template<class mutex = std::mutex, class uniquelock = std::unique_lock<mutex> , class sharedlock = std::unique_lock<mutex> >
class locker {
public:
	typedef recursive_lock<sharedlock, mutex> lock_rec_shared;
	typedef recursive_lock<uniquelock, mutex> lock_rec_unique;

	mutex mtx;
	std::atomic<std::size_t> thread_id;

	locker();
	std::unique_ptr<uniquelock> lock_unique();
	std::unique_ptr<uniquelock> try_lock_unique();
	std::unique_ptr<sharedlock> lock_shared();
	std::unique_ptr<sharedlock> try_lock_shared();
	std::unique_ptr<lock_rec_unique> lock_unique_rec();
	std::unique_ptr<lock_rec_unique> try_lock_unique_rec();
	std::unique_ptr<lock_rec_shared> lock_shared_rec();
	std::unique_ptr<lock_rec_shared> try_lock_shared_rec();
};

class shared_locker : public locker<try_shared_mutex, unique_lock, try_shared_lock> { };

class dummy_lock {
public:
	~dummy_lock() {}; //no unused variable warning
	bool owns_lock() {return true;}
	bool operator!() {return true;}
	dummy_lock * operator->() {return this; }
	void unlock() {};
};

class dummy_locker {
public:
	dummy_lock lock_unique() { return dummy_lock(); };
	dummy_lock try_lock_unique() { return dummy_lock(); };
	dummy_lock lock_shared() { return dummy_lock(); };
	dummy_lock try_lock_shared() { return dummy_lock(); };
	dummy_lock lock_unique_rec() { return dummy_lock(); };
	dummy_lock try_lock_unique_rec() { return dummy_lock(); };
	dummy_lock lock_shared_rec() { return dummy_lock(); };
	dummy_lock try_lock_shared_rec() { return dummy_lock(); };
};


#if ENABLE_THREADS

class maybe_locker : public locker<> { };
class maybe_shared_locker : public shared_locker {};

#else

class maybe_locker : public dummy_locker { };
class maybe_shared_locker : public dummy_locker {};

#endif


/*
Not used, but uncomment if you need
also rename to concurrent_vector.h

#include <vector>


template <class T, class Allocator = std::allocator<T> >
class shared_vector :
	public std::vector<T, Allocator>,
	public locker<>
{
public:
	typedef typename std::vector<T, Allocator>           full_type;
    typedef T                                        value_type;
    typedef Allocator                                allocator_type;
    typedef typename full_type::reference       reference;
    typedef typename full_type::const_reference const_reference;
    typedef typename full_type::size_type       size_type;
    typedef typename full_type::pointer         pointer;
    typedef typename full_type::const_pointer   const_pointer;

	typedef typename full_type::const_iterator                         const_iterator;
	typedef typename full_type::iterator                               iterator;


	bool      empty() {
		auto lock = lock_shared_rec();
		return full_type::empty();
	}

	size_type size() {
		auto lock = lock_shared_rec();
		return full_type::size();
	}

	reference       operator[](size_type n) {
		auto lock = lock_unique_rec();
		return full_type::operator[](n);
	};

	const_reference operator[](size_type n) const {
		auto lock = lock_shared_rec();
		return full_type::operator[](n);
	};

	void resize(size_type sz) {
		auto lock = lock_unique_rec();
		return full_type::resize(sz);
	};

	void clear() {
		auto lock = lock_unique_rec();
		return full_type::clear();
	};

	void push_back(const value_type& x) {
		auto lock = lock_unique_rec();
		return full_type::push_back(x);
	};

	void push_back(value_type&& x) {
		auto lock = lock_unique_rec();
		return full_type::push_back(x);
	};

};


#if ENABLE_THREADS

template <class T, class Allocator = std::allocator<T> >
class maybe_shared_vector : public shared_vector<T, Allocator>
{};

#else

template <class T, class Allocator = std::allocator<T> >
class not_shared_vector :
	public std::vector<T, Allocator>,
	public dummy_locker
{
public:
	typedef typename std::vector<T, Alloc>           full_type;
	typedef T                                        key_type;
	typedef T                                        mapped_type;
    typedef T                                        value_type;

	mapped_type& get(size_type n) {
		return full_type::operator[](k);
	}

	void set(size_type n, const mapped_type& v) {
		full_type::operator[](n) = v;
	}
};

template <class T, class Allocator = std::allocator<T> >
class maybe_shared_vector: public not_shared_vector<Key, Allocator>
{};

#endif

*/



#endif
