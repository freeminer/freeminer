#ifndef UTIL_LOCK_HEADER
#define UTIL_LOCK_HEADER

#include <mutex>
#include <atomic>
#include <thread>
#include <memory>
#include <chrono>

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
	int lock_unique() { return 1; };
	dummy_lock try_lock_unique() { return std::unique_ptr<dummy_lock>(new dummy_lock); };
	int lock_shared() { return 1; };
	dummy_lock try_lock_shared() { return std::unique_ptr<dummy_lock>(new dummy_lock); };
	dummy_lock lock_unique_rec() { return std::unique_ptr<dummy_lock>(new dummy_lock); };
	dummy_lock try_lock_unique_rec() { return std::unique_ptr<dummy_lock>(new dummy_lock); };
	dummy_lock lock_shared_rec() { return ; };
	std::unique_ptr<dummy_lock> try_lock_shared_rec() { return std::unique_ptr<dummy_lock>(new dummy_lock); };

*/

#include <map>

//template < class L = locker<class mutex, class uniquelock, class sharedlock>, class Key, class T, class Compare = std::less<Key>,
template < class LOCKER, class Key, class T, class Compare = std::less<Key>,
         class Allocator = std::allocator<std::pair<const Key, T> > >
class concurrent_map_: public std::map<Key, T, Compare, Allocator>,
	//public locker<class mutex, class uniquelock, class sharedlock> {
	public LOCKER {
public:
	typedef typename std::map<Key, T, Compare, Allocator> full_type;
	typedef Key                                           key_type;
	typedef T                                             mapped_type;
	typedef Allocator                                     allocator_type;
	typedef typename allocator_type::size_type            size_type;
	typedef typename full_type::const_iterator            const_iterator;
	typedef typename full_type::iterator                  iterator;
	typedef typename full_type::reverse_iterator          reverse_iterator;
	typedef typename full_type::const_reverse_iterator    const_reverse_iterator;

	mapped_type& get(const key_type& k) {
		auto lock = LOCKER::lock_shared_rec();
		return full_type::operator[](k);
	}

	void set(const key_type& k, const mapped_type& v) {
		auto lock = LOCKER::lock_unique_rec();
		full_type::operator[](k) = v;
	}

	bool set_try(const key_type& k, const mapped_type& v) {
		auto lock = LOCKER::try_lock_unique_rec();
		if (!lock->owns_lock())
			return false;
		full_type::operator[](k) = v;
		return true;
	}

	bool      empty() {
		auto lock = LOCKER::lock_shared_rec();
		return full_type::empty();
	}

	size_type size() {
		auto lock = LOCKER::lock_shared_rec();
		return full_type::size();
	}

	size_type count(const key_type& k) {
		auto lock = LOCKER::lock_shared_rec();
		return full_type::count(k);
	}

	iterator find(const key_type& k) {
		auto lock = LOCKER::lock_shared_rec();
		return full_type::find(k);
	};

	const_iterator find(const key_type& k) const {
		auto lock = LOCKER::lock_shared_rec();
		return full_type::find(k);
	};

	iterator begin() noexcept {
		auto lock = LOCKER::lock_shared_rec();
		return full_type::begin();
	};

	const_iterator begin()   const noexcept {
		auto lock = LOCKER::lock_shared_rec();
		return full_type::begin();
	};

	reverse_iterator rbegin() noexcept {
		auto lock = LOCKER::lock_shared_rec();
		return full_type::rbegin();
	};

	const_reverse_iterator rbegin()   const noexcept {
		auto lock = LOCKER::lock_shared_rec();
		return full_type::rbegin();
	};

	iterator end() noexcept {
		auto lock = LOCKER::lock_shared_rec();
		return full_type::end();
	};

	const_iterator end()   const noexcept {
		auto lock = LOCKER::lock_shared_rec();
		return full_type::end();
	};

	reverse_iterator rend() noexcept {
		auto lock = LOCKER::lock_shared_rec();
		return full_type::rend();
	};

	const_reverse_iterator rend()   const noexcept {
		auto lock = LOCKER::lock_shared_rec();
		return full_type::rend();
	};

	mapped_type& operator[](const key_type& k) = delete;

	mapped_type& operator[](key_type&& k) = delete;

	typename full_type::iterator  erase(const_iterator position) {
		auto lock = LOCKER::lock_unique_rec();
		return full_type::erase(position);
	}

	typename full_type::iterator  erase(iterator position) {
		auto lock = LOCKER::lock_unique_rec();
		return full_type::erase(position);
	}

	size_type erase(const key_type& k) {
		auto lock = LOCKER::lock_unique_rec();
		return full_type::erase(k);
	}

	typename full_type::iterator  erase(const_iterator first, const_iterator last) {
		auto lock = LOCKER::lock_unique_rec();
		return full_type::erase(first, last);
	}

	void clear() {
		auto lock = LOCKER::lock_unique_rec();
		full_type::clear();
	}
};

template <class Key, class T, class Compare = std::less<Key>,
         class Allocator = std::allocator<std::pair<const Key, T> >>
class concurrent_map: public concurrent_map_<locker<>, Key, T, Compare, Allocator>
{ };


#if ENABLE_THREADS

template < class Key, class T, class Compare = std::less<Key>,
         class Allocator = std::allocator<std::pair<const Key, T> >>
class maybe_concurrent_map: public concurrent_map<Key, T, Compare, Allocator>
{ };

#else

template < class Key, class T, class Compare = std::less<Key>,
         class Allocator = std::allocator<std::pair<const Key, T> >>
class not_concurrent_map: public std::map<Key, T, Compare, Allocator>,
	public dummy_locker {
public:
	typedef typename std::map<Key, T, Compare, Allocator> full_type;
	typedef Key                                           key_type;
	typedef T                                             mapped_type;

	mapped_type& get(const key_type& k) {
		return full_type::operator[](k);
	}

	void set(const key_type& k, const mapped_type& v) {
		full_type::operator[](k) = v;
	}

	bool set_try(const key_type& k, const mapped_type& v) {
		full_type::operator[](k) = v;
		return true;
	}
};

template < class Key, class T, class Compare = std::less<Key>,
         class Allocator = std::allocator<std::pair<const Key, T> >>
class maybe_concurrent_map: public not_concurrent_map<Key, T, Compare, Allocator>
{ };

#endif


#include <unordered_map>




template < class LOCKER, class Key, class T, class Hash = std::hash<Key>, class Pred = std::equal_to<Key>,
         class Alloc = std::allocator<std::pair<const Key, T> > >
class concurent_unordered_map_: public std::unordered_map<Key, T, Hash, Pred, Alloc>,
	public LOCKER {
public:
	typedef typename std::unordered_map<Key, T, Hash, Pred, Alloc>     full_type;
	typedef Key                                                        key_type;
	typedef T                                                          mapped_type;
	typedef Hash                                                       hasher;
	typedef Pred                                                       key_equal;
	typedef Alloc                                                      allocator_type;
	typedef std::pair<const key_type, mapped_type>                     value_type;
	typedef value_type&                                                reference;
	typedef const value_type&                                          const_reference;
	typedef typename full_type::pointer                                pointer;
	typedef typename full_type::const_pointer                          const_pointer;
	typedef typename full_type::size_type                              size_type;
	typedef typename full_type::difference_type                        difference_type;

	typedef typename full_type::const_iterator                         const_iterator;
	typedef typename full_type::iterator                               iterator;

	mapped_type& get(const key_type& k) {
		auto lock = LOCKER::lock_shared_rec();
		return full_type::operator[](k);
	}

	void set(const key_type& k, const mapped_type& v) {
		auto lock = LOCKER::lock_unique_rec();
		full_type::operator[](k) = v;
	}

	bool      empty() {
		auto lock = LOCKER::lock_shared_rec();
		return full_type::empty();
	}

	size_type size() {
		auto lock = LOCKER::lock_shared_rec();
		return full_type::size();
	}

	size_type count(const key_type& k) {
		auto lock = LOCKER::lock_shared_rec();
		return full_type::count(k);
	}

	iterator find(const key_type& k) {
		auto lock = LOCKER::lock_shared_rec();
		return full_type::find(k);
	};

	const_iterator find(const key_type& k) const {
		auto lock = LOCKER::lock_shared_rec();
		return full_type::find(k);
	};

	iterator begin() noexcept {
		auto lock = LOCKER::lock_shared_rec();
		return full_type::begin();
	};

	const_iterator begin()   const noexcept {
		auto lock = LOCKER::lock_shared_rec();
		return full_type::begin();
	};

	iterator end() noexcept {
		auto lock = LOCKER::lock_shared_rec();
		return full_type::end();
	};

	const_iterator end()   const noexcept {
		auto lock = LOCKER::lock_shared_rec();
		return full_type::end();
	};

	mapped_type& operator[](const key_type& k) = delete;

	mapped_type& operator[](key_type&& k) = delete;

	typename full_type::iterator  erase(const_iterator position) {
		auto lock = LOCKER::lock_unique_rec();
		return full_type::erase(position);
	}

	typename full_type::iterator  erase(iterator position) {
		auto lock = LOCKER::lock_unique_rec();
		return full_type::erase(position);
	}

	size_type erase(const key_type& k) {
		auto lock = LOCKER::lock_unique_rec();
		return full_type::erase(k);
	}

	typename full_type::iterator  erase(const_iterator first, const_iterator last) {
		auto lock = LOCKER::lock_unique_rec();
		return full_type::erase(first, last);
	}

	void clear() {
		auto lock = LOCKER::lock_unique_rec();
		full_type::clear();
	}

};

template <class Key, class T, class Hash = std::hash<Key>, class Pred = std::equal_to<Key>,
         class Alloc = std::allocator<std::pair<const Key, T> > >
class concurrent_unordered_map: public concurent_unordered_map_<locker<>, Key, T, Hash, Pred, Alloc>
{ };

#if ENABLE_THREADS

template < class Key, class T, class Hash = std::hash<Key>, class Pred = std::equal_to<Key>,
         class Alloc = std::allocator<std::pair<const Key, T> >>
class maybe_concurrent_unordered_map: public concurrent_unordered_map<Key, T, Hash, Pred, Alloc>
{};

#else

template < class Key, class T, class Hash = std::hash<Key>, class Pred = std::equal_to<Key>,
         class Alloc = std::allocator<std::pair<const Key, T> >>
class not_concurrent_unordered_map: public std::unordered_map<Key, T, Hash, Pred, Alloc>,
	public dummy_locker {
public:
	typedef typename std::unordered_map<Key, T, Hash, Pred, Alloc>     full_type;
	typedef Key                                                        key_type;
	typedef T                                                          mapped_type;

	mapped_type& get(const key_type& k) {
		return full_type::operator[](k);
	}

	void set(const key_type& k, const mapped_type& v) {
		full_type::operator[](k) = v;
	}
};

template < class Key, class T, class Hash = std::hash<Key>, class Pred = std::equal_to<Key>,
         class Alloc = std::allocator<std::pair<const Key, T> >>
class maybe_concurrent_unordered_map: public not_concurrent_unordered_map<Key, T, Hash, Pred, Alloc>
{};

#endif



/*
Not used, but uncomment if you need

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
